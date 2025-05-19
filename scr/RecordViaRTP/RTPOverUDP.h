/*
 * RTPOverUDP.h
 * Based on work by Phil Schatzmann (https://github.com/pschatzmann/arduino-audio-tools)
 * Licensed under the GNU General Public License v3.0
 * (c) 2025 Hugo Schroeder

 * This class serves as a bare bones RTP over UDP class, as it just strips the header file and returns the payload.
 * For sending packets, it will construct RTP packets, but does not handle the UDP stream

 */
#pragma once
#include <Arduino.h>
#include "AudioTools/Communication/UDPStream.h"
#include "AudioTools/CoreAudio/BaseStream.h"

using namespace audio_tools;

static const size_t RTP_HEADER_SIZE = 12;

class RTPOverUDP : public BaseStream {
public:
  explicit RTPOverUDP(UDPStream& udpStream)
    : _udp(udpStream)
    , _seq(0)
    , _timestamp(0)
    , _ssrc(esp_random())
    , _payloadType(0)
    , _sampleRate(8000) {}

  void setSSRC(uint32_t s)        { _ssrc = s; }
  void setPayloadType(uint8_t pt) { _payloadType = pt; }
  void setSampleRate(uint32_t sr) { _sampleRate = sr; }

  size_t write(const uint8_t* payload, size_t len) override {
    // build RTP header
    uint8_t header[12];                     // V=2, P=0, X=0, CC=0
    header[0] = 0x80;
    header[1] = _payloadType;                // M=0 - Payload type
    header[2] = (_seq >> 8) & 0xFF;          // Sequence Numebr
    header[3] = (_seq     ) & 0xFF;          // timestamp
    header[4] = (_timestamp >> 24) & 0xFF;
    header[5] = (_timestamp >> 16) & 0xFF;
    header[6] = (_timestamp >>  8) & 0xFF;
    header[7] = (_timestamp      ) & 0xFF;    // SSRC
    header[8]  = (_ssrc >> 24) & 0xFF;
    header[9]  = (_ssrc >> 16) & 0xFF;
    header[10] = (_ssrc >> 8 ) & 0xFF;
    header[11] = (_ssrc      ) & 0xFF;

    //_udp.write(header, RTP_HEADER_SIZE);
    //_udp.write(payload, len);
    size_t total = RTP_HEADER_SIZE + len;
    auto* pkt = (uint8_t*)malloc(total);
    memcpy(pkt, header, RTP_HEADER_SIZE);
    memcpy(pkt+RTP_HEADER_SIZE, payload, len);
    size_t sent = _udp.write(pkt, total);
    free(pkt);

    _seq++;
    _timestamp += (len * 8000) / (_sampleRate * sizeof(int16_t));
    return RTP_HEADER_SIZE + len;
  }

  int available() override {
    int total = _udp.available();
    if (total <= RTP_HEADER_SIZE) {
      if (total > 0) {
        uint8_t discard[total];
        _udp.readBytes(discard, total);
      }
      return 0;
    }
    return total - RTP_HEADER_SIZE;
  }

  // Read up to 'len' payload bytes into buffer
  size_t readBytes(uint8_t* buffer, size_t len) override {
    int total = _udp.available();
    if (total <= 12) {
      if (total > 0) {
        uint8_t discardBuf[total];
        _udp.readBytes(discardBuf, total);
      }
      return 0;
    }
    // Skip the 12-byte RTP header
    uint8_t headerBuf[12];
    _udp.readBytes(headerBuf, 12);
    size_t toCopy = total - 12;
    if (toCopy > len) toCopy = len;
    return _udp.readBytes(buffer, toCopy);
  }

private:
  UDPStream& _udp;
  uint16_t _seq;
  uint32_t _timestamp;
  uint32_t _ssrc;
  uint8_t  _payloadType;
  uint32_t _sampleRate;

  static constexpr size_t RTP_HEADER_SIZE = 12;
};
