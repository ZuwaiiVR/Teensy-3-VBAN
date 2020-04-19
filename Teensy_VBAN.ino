// VBAN Ethernet on Teensy 3.2
// We need some modifications on the internal library's
// in ethernet.h
// uncomment #define ETHERNET_LARGE_BUFFERS 
// change #define MAX_SOCK_NUM 8 to #define MAX_SOCK_NUM 2 
// 
// in w5100.h
// change the SPI speed to fastest 
// e.a. #define SPI_ETHERNET_SETTINGS SPISettings(30000000, MSBFIRST, SPI_MODE0)
//
// Select Audio in USB > Type or Audio+Serial+Midi~, I'm using CPU on 144mhz but should work aslow as stock mhz.
//
// I'm'not using the audioshield however it could work anyway with it. but you need to integrate the SGTL code stuff.
// Curently Im using a PCM5102A
//
// Second, when connected through USB to a PC, it can also be used as a soundcard, but only on 44100hz! 
// In VBAN it's possible to change samplerate up to 96khz (some are not listed) doing this will break USB sound, resulting high/low crackling pitch playback.
//

// Pinouts
// Teensy pin > DAC pin
// 23 > LRCK 
// 22 > DIN
// 9  > BCK
// 11 > SCK

//Ethernet shield W5200/W5500 (W5100 is too slow! will lose packages)
// 12 > MISO
// 14 > SCLK
// 7 > MOSI
// 6 > SCS
// 5 > RST
// There is some glitches aka, a 40ms silence randomly. I don't know where this come from. but less then using Wifi on the ESP modules.
// Inspired from https://github.com/flyingKenny/VBAN-Receptor-ESP8266-I2S
// Code edited by Dnstje

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Audio.h>
#include <Wire.h>
#include <SD.h>
#include <SerialFlash.h>
#include "vban.h"
#include "packet.h"


// GUItool: begin automatically generated code
AudioInputUSB            usb1;           //xy=177,150
AudioPlayQueue           queue1;         //xy=201,237
AudioPlayQueue           queue2;         //xy=203,285
AudioMixer4              mixer1;         //xy=414,362
AudioMixer4              mixer2;         //xy=466,211
AudioOutputI2S           i2s2;           //xy=579,264
AudioConnection          patchCord1(usb1, 0, mixer2, 1);
AudioConnection          patchCord2(usb1, 1, mixer1, 1);
AudioConnection          patchCord3(queue1, 0, mixer2, 0);
AudioConnection          patchCord4(queue2, 0, mixer1, 0);
AudioConnection          patchCord5(mixer1, 0, i2s2, 1);
AudioConnection          patchCord6(mixer2, 0, i2s2, 0);

EthernetUDP Udp;
// Ethernet & Vban stuff
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 124); // VBAN IP
unsigned int localPort = 6980; // VBAN Port
char streamname[] = "Stream1";  //VBAN Stream name

// Internal buffer & sound stuff
int VBANBuffer[VBAN_PROTOCOL_MAX_SIZE];
int* VBANData;
int16_t* VBANData1Ch;
byte cirbuf = 0;
int16_t actualL[128];
int16_t actualR[128];

//Debug stuff
bool debug = true; //enable some debug information
elapsedMillis e;
int counter;
int tmppkg;
int prevframe;
int sformat_nbs;
int pksize_;
int sformat_SR;
int sformat_nbc;
int fps;
int lastfps;

void setup() {
  pinMode(5, OUTPUT);
  digitalWrite(5, LOW);
  delay(250);
  digitalWrite(5, HIGH);
  Ethernet.init(6);
  SPI.setMOSI(7);
  SPI.setSCK(14);
  AudioMemory(20);
  Serial.begin(115200);
  Serial.println("Initialize Ethernet:");
  delay(1000);
  Ethernet.begin(mac, ip);
  Udp.begin(localPort);
}

void loop() {
  int packetSize = Udp.parsePacket();
  if (e > 1000 && debug) {
    Serial.print(lastfps);

    Serial.print("    udp_pcksize ");
    Serial.print(pksize_);
    Serial.print(" | samples ");
    Serial.print(sformat_nbs);
    Serial.print(" | pps ");
    Serial.print(counter);

    Serial.print(" | samplrate ");
    Serial.print(VBanSRList[sformat_SR]);
    Serial.print(" | ch ");
    Serial.print(sformat_nbc);

    Serial.print("  all=");
    Serial.print(AudioProcessorUsage());
    Serial.print(",");
    Serial.print(AudioProcessorUsageMax());
    Serial.print("    ");
    Serial.print("Memory: ");
    Serial.print(AudioMemoryUsage());
    Serial.print(",");
    Serial.print(AudioMemoryUsageMax());
    Serial.println(" ");
    counter = 0;
    e = 0;
  }
  if (packetSize) {
    pksize_ = packetSize;
    counter++;
    Udp.read((char*)VBANBuffer, VBAN_PROTOCOL_MAX_SIZE);

    if (vban_packet_check(streamname, (char*)VBANBuffer, packetSize) == 0)
    {
      struct VBanHeader const* const hdr = PACKET_HEADER_PTR((char*)VBANBuffer);
      setI2SFreq(VBanSRList[hdr->format_SR]);
      if (hdr->nuFrame != prevframe + 1) {
        Serial.println("pkg drop.");
      }
      prevframe = hdr->nuFrame;
      sformat_nbs = hdr->format_nbs;
      sformat_nbc = hdr->format_nbc;
      sformat_SR = hdr->format_SR;

      VBANData = (int*)(((char*)VBANBuffer) + VBAN_HEADER_SIZE);
      for (int VBANSample = 0; VBANSample < hdr->format_nbs; VBANSample++)
      {
        switch (hdr->format_nbc)
        {
          case 0:   //one channel
            VBANData1Ch = (int16_t*)VBANData;
            tosound((int)VBANData1Ch[VBANSample] << 16 | VBANData1Ch[VBANSample], (int)VBANData1Ch[VBANSample] << 16 | VBANData1Ch[VBANSample]);
            break;
          case 1:    //two channel
            tosound((((VBANData[VBANSample] << 16) >> 16) ) , ((VBANData[VBANSample] >> 16) ) );
            break;
        }
      }
    }
    else
    {
      Serial.println("VBAN Error");
    }
  }
}

void tosound(int16_t LEFTdata, int16_t RIGHTdata) {
  actualL[cirbuf] = LEFTdata;
  actualR[cirbuf] = RIGHTdata;
  if (cirbuf >= 127) {
    fps = micros();
    cirbuf = 0;
    if (queue1.available() && queue2.available()) {
      int16_t *p = queue1.getBuffer();
      int16_t *o = queue2.getBuffer();
      memcpy(p, actualL, sizeof(actualL));
      memcpy(o, actualR, sizeof(actualR));
      queue1.playBuffer();
      queue2.playBuffer();
      lastfps = micros() - fps;
    } else {
      Serial.println("no mem");
    }
  } else {
    cirbuf++;
  }
}

void setI2SFreq(int freq) { // Adopted from FrankB https://forum.pjrc.com/threads/38753-Discussion-about-a-simple-way-to-change-the-sample-rate
  typedef struct {
    uint8_t mult;
    uint16_t div;
  } tmclk;

  const int numfreqs = 14;
  const int samplefreqs[numfreqs] = { 8000, 11025, 16000, 22050, 32000, 44100, 44117.64706 , 48000, 88200, 44117.64706 * 2, 96000, 176400, 44117.64706 * 4, 192000};

#if (F_PLL==16000000)
  const tmclk clkArr[numfreqs] = {{16, 125}, {148, 839}, {32, 125}, {145, 411}, {64, 125}, {151, 214}, {12, 17}, {96, 125}, {151, 107}, {24, 17}, {192, 125}, {127, 45}, {48, 17}, {255, 83} };
#elif (F_PLL==72000000)
  const tmclk clkArr[numfreqs] = {{32, 1125}, {49, 1250}, {64, 1125}, {49, 625}, {128, 1125}, {98, 625}, {8, 51}, {64, 375}, {196, 625}, {16, 51}, {128, 375}, {249, 397}, {32, 51}, {185, 271} };
#elif (F_PLL==96000000)
  const tmclk clkArr[numfreqs] = {{8, 375}, {73, 2483}, {16, 375}, {147, 2500}, {32, 375}, {147, 1250}, {2, 17}, {16, 125}, {147, 625}, {4, 17}, {32, 125}, {151, 321}, {8, 17}, {64, 125} };
#elif (F_PLL==120000000)
  const tmclk clkArr[numfreqs] = {{32, 1875}, {89, 3784}, {64, 1875}, {147, 3125}, {128, 1875}, {205, 2179}, {8, 85}, {64, 625}, {89, 473}, {16, 85}, {128, 625}, {178, 473}, {32, 85}, {145, 354} };
#elif (F_PLL==144000000)
  const tmclk clkArr[numfreqs] = {{16, 1125}, {49, 2500}, {32, 1125}, {49, 1250}, {64, 1125}, {49, 625}, {4, 51}, {32, 375}, {98, 625}, {8, 51}, {64, 375}, {196, 625}, {16, 51}, {128, 375} };
#elif (F_PLL==180000000)
  const tmclk clkArr[numfreqs] = {{46, 4043}, {49, 3125}, {73, 3208}, {98, 3125}, {183, 4021}, {196, 3125}, {16, 255}, {128, 1875}, {107, 853}, {32, 255}, {219, 1604}, {214, 853}, {64, 255}, {219, 802} };
#elif (F_PLL==192000000)
  const tmclk clkArr[numfreqs] = {{4, 375}, {37, 2517}, {8, 375}, {73, 2483}, {16, 375}, {147, 2500}, {1, 17}, {8, 125}, {147, 1250}, {2, 17}, {16, 125}, {147, 625}, {4, 17}, {32, 125} };
#elif (F_PLL==216000000)
  const tmclk clkArr[numfreqs] = {{32, 3375}, {49, 3750}, {64, 3375}, {49, 1875}, {128, 3375}, {98, 1875}, {8, 153}, {64, 1125}, {196, 1875}, {16, 153}, {128, 1125}, {226, 1081}, {32, 153}, {147, 646} };
#elif (F_PLL==240000000)
  const tmclk clkArr[numfreqs] = {{16, 1875}, {29, 2466}, {32, 1875}, {89, 3784}, {64, 1875}, {147, 3125}, {4, 85}, {32, 625}, {205, 2179}, {8, 85}, {64, 625}, {89, 473}, {16, 85}, {128, 625} };
#endif

  for (int f = 0; f < numfreqs; f++) {
    if ( freq == samplefreqs[f] ) {
      while (I2S0_MCR & I2S_MCR_DUF) ;
      I2S0_MDR = I2S_MDR_FRACT((clkArr[f].mult - 1)) | I2S_MDR_DIVIDE((clkArr[f].div - 1));
      return;
    }
  }
}
