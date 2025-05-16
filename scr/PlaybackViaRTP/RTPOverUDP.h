/*
 * RTPOverUDP.h
 * Based on work by Phil Schatzmann (https://github.com/pschatzmann/arduino-audio-tools)
 * Licensed under the GNU General Public License v3.0
 * (c) 2025 Hugo Schroeder

 * This class serves as a bare bones RTP over UDP class, as it just strips the header file and returns the payload

 */

#pragma once
#include "AudioTools/Communication/UDPStream.h"
#include "AudioTools/CoreAudio/BaseStream.h"

namespace audio_tools {

class RTPOverUDP : public BaseStream {
public:
  explicit RTPOverUDP(UDPStream& udpStream)
    : _udp(udpStream) {}

  size_t write(const uint8_t* /*data*/, size_t /*len*/) override {  //write function will need expaned for the Recording
    return 0;
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
  static constexpr size_t RTP_HEADER_SIZE = 12;
};

} 