# Teensy-3-VBAN
Using Teensy 3.2 on ethernet with VBAN Audio

# Info

VBAN Ethernet on Teensy 3.2
We need some modifications on the internal library's
in ethernet.h
uncomment #define ETHERNET_LARGE_BUFFERS 
change #define MAX_SOCK_NUM 8 to #define MAX_SOCK_NUM 2 
 
in w5100.h
change the SPI speed to fastest 
e.a. #define SPI_ETHERNET_SETTINGS SPISettings(30000000, MSBFIRST, SPI_MODE0)

Select Audio in USB > Type or Audio+Serial+Midi~, I'm using CPU on 144mhz but should work aslow as stock mhz.

I'm'not using the audioshield however it could work anyway with it. but you need to integrate the SGTL code stuff.
Curently Im using a PCM5102A

Second, when connected through USB to a PC, it can also be used as a soundcard, but only on 44100hz! 
In VBAN it's possible to change samplerate up to 96khz (some are not listed) doing this will break USB sound, resulting high/low crackling pitch playback.


Pinouts
|Teensy pin | DAC pin
|-----------|---------|
|23 | LRCK |
|22 | DIN |
|9  | BCK |
|11 | SCK |

Ethernet shield W5200/W5500 (W5100 is too slow! will lose packages)
|teensy pin|eth|
|--------|--------|
|12 | MISO |
|14 | SCLK |
| 7 | MOSI |
| 6 | SCS |
| 5 | RST |

 There is some glitches aka, a 40ms silence randomly. I don't know where this come from. but less then using Wifi on the ESP modules.
 
 Inspired from https://github.com/flyingKenny/VBAN-Receptor-ESP8266-I2S
 
 Code edited by Dnstje
