/*
 *  This file is part of vban.
 *  Copyright (c) 2017 by Benoît Quiniou <quiniouben@yahoo.fr>
 *
 *  vban is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  vban is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with vban.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PACKET_H__
#define __PACKET_H__

#include <stddef.h>
#include "vban.h"
//#include "stream.h"

/**
 * Check packet content and only return valid return value if this is an audio pcm packet
 * @param streamname string pointer holding streamname
 * @param buffer pointer to data to check
 * @param size of the data in buffer;
 * @return 0 if packet is valid, negative value otherwise
 */
int packet_check(char const* streamname, char const* buffer, size_t size);

/** Return VBanHeader pointer from buffer */
#define PACKET_HEADER_PTR(_buffer) ((struct VBanHeader*)_buffer)

/** Return payload pointer of a Vban packet from buffer pointer */
#define PACKET_PAYLOAD_PTR(_buffer) ((char*)(PACKET_HEADER_PTR(_buffer)+1))

/** Return paylod size from total packet size */
#define PACKET_PAYLOAD_SIZE(_size) (_size - sizeof(struct VBanHeader))

/**
 * Fill @p stream_config with the values corresponding to the data ini the packet
 * @param buffer pointer to data
 * @param stream_config pointer
 * @return 0 upon success, negative value otherwise
 */
int packet_get_stream_config(char const* buffer, struct stream_config_t* stream_config);

/**
 * Get max payload_size from packet header
 * @param buffer pointer to packet
 * @return size upon success, negative value otherwise
 */
int packet_get_max_payload_size(char const* buffer);

/**
 * Init header content.
 * @param buffer pointer to data
 * @param stream_config pointer
 * @return 0 upon success, negative value otherwise
 */
int packet_init_header(char* buffer, struct stream_config_t const* stream_config, char const* streamname);

/**
 * Fill the packet withe values corresponding to stream_config
 * @param buffer pointer to data
 * @param payload_size size of the payload
 * @return 0 upon success, negative value otherwise
 */
int packet_set_new_content(char* buffer, size_t payload_size);

#endif /*__PACKET_H__*/


static int packet_pcm_check(char const* buffer, size_t size)
{
  /** the packet is already a valid vban packet and buffer already checked before */

  struct VBanHeader const* const hdr = PACKET_HEADER_PTR(buffer);
  enum VBanBitResolution const bit_resolution = (VBanBitResolution)(hdr->format_bit & VBAN_BIT_RESOLUTION_MASK);
  int const sample_rate   = hdr->format_SR & VBAN_SR_MASK;
  int const nb_samples    = hdr->format_nbs + 1;
  int const nb_channels   = hdr->format_nbc + 1;
  size_t sample_size      = 0;
  size_t payload_size     = 0;

  //logger_log(LOG_DEBUG, "%s: packet is vban: %u, sr: %d, nbs: %d, nbc: %d, bit: %d, name: %s, nu: %u",
  //    __func__, hdr->vban, hdr->format_SR, hdr->format_nbs, hdr->format_nbc, hdr->format_bit, hdr->streamname, hdr->nuFrame);

  if (bit_resolution >= VBAN_BIT_RESOLUTION_MAX)
  {
    Serial.println("invalid bit resolution");
    return;// -EINVAL;
  }

  if (sample_rate >= VBAN_SR_MAXNUMBER)
  {
    Serial.println("invalid sample rate");
    return;// -EINVAL;
  }

  sample_size = VBanBitResolutionSize[bit_resolution];
  payload_size = nb_samples * sample_size * nb_channels;

  if (payload_size != (size - VBAN_HEADER_SIZE))
  {
    //    logger_log(LOG_WARNING, "%s: invalid payload size, expected %d, got %d", __func__, payload_size, (size - VBAN_HEADER_SIZE));
    Serial.println("invalid payload size");
    return;// -EINVAL;
  }

  return 0;
}

int vban_packet_check(char const* streamname, char const* buffer, size_t size)
{
  struct VBanHeader const* const hdr = PACKET_HEADER_PTR(buffer);
  enum VBanProtocol protocol = VBAN_PROTOCOL_UNDEFINED_4;
  enum VBanCodec codec = (VBanCodec)VBAN_BIT_RESOLUTION_MAX;

  if ((streamname == 0) || (buffer == 0))
  {
    Serial.println("null pointer argument");
    return;// -EINVAL;
  }

  if (size <= VBAN_HEADER_SIZE)
  {
    Serial.println("packet too small");
    return;// -EINVAL;
  }

  if (hdr->vban != VBAN_HEADER_FOURC)
  {
    Serial.println("invalid vban magic fourc");
    return;// -EINVAL;
  }

  if (strncmp(streamname, hdr->streamname, VBAN_STREAM_NAME_SIZE))
  {
    Serial.println("different streamname");
    return;// -EINVAL;
  }

  /** check the reserved bit : it must be 0 */
  if (hdr->format_bit & VBAN_RESERVED_MASK)
  {
    Serial.println("reserved format bit invalid value");
    return;// -EINVAL;
  }

  /** check protocol and codec */
  protocol        = (VBanProtocol)(hdr->format_SR & VBAN_PROTOCOL_MASK);
  codec           = (VBanCodec)(hdr->format_bit & VBAN_CODEC_MASK);

  switch (protocol)
  {
    case VBAN_PROTOCOL_AUDIO:
      return (codec == VBAN_CODEC_PCM) ? packet_pcm_check(buffer, size) : 0;// : -EINVAL;
      /*Serial.print("OK: VBAN_PROTOCOL_AUDIO");
        Serial.print(": nuFrame ");
        Serial.print(hdr->nuFrame);
        Serial.print(": format_SR ");
        Serial.print(hdr->format_SR);
        Serial.print(": format_nbs ");
        Serial.print(hdr->format_nbs);
        Serial.print(": format_nbc ");
        Serial.println(hdr->format_nbc);
      */
      return 0;
    case VBAN_PROTOCOL_SERIAL:
    case VBAN_PROTOCOL_TXT:
    case VBAN_PROTOCOL_UNDEFINED_1:
    case VBAN_PROTOCOL_UNDEFINED_2:
    case VBAN_PROTOCOL_UNDEFINED_3:
    case VBAN_PROTOCOL_UNDEFINED_4:
      /** not supported yet */
      return;// -EINVAL;

    default:
      Serial.println("packet with unknown protocol");
      return;// -EINVAL;
  }
}
