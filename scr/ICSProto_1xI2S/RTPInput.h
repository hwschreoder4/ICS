/*
 * RTPInput.h
 * Based on work by Phil Schatzmann (https://github.com/pschatzmann/arduino-audio-tools)
 * Licensed under the GNU General Public License v3.0
 * (c) 2025 Hugo Schroeder

 * This serves to handle all the elements needed to send an RTP stream and play it back
 */
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "AudioTools.h"
#include "AudioTools/Communication/UDPStream.h"
#include "RTPOverUDP.h"
#include "AudioTools/AudioCodecs/CodecG7xx.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "OffsetFilter.h"

using namespace audio_tools;

class RTPInput {
public:
  RTPInput(I2SStream* i2s, const char* ssid, const char* password)
    : _i2s(i2s)
    , _ssid(ssid)
    , _password(password)
    , _udpStream(nullptr)
    , _rtp(nullptr)
    , _offsetFilter(nullptr)
    , _dcCorrect(nullptr)
    , _toNet(nullptr)
    , _encoder(nullptr)
    , _sender(nullptr) {}

  bool begin(uint16_t localPort, const char* destIP, uint16_t destPort) {
    _destIP     = IPAddress(destIP);
    _destPort   = destPort;
    // Instantiate pipeline components
    _udpStream    = new UDPStream(_ssid, _password);
    _rtp          = new RTPOverUDP(*_udpStream);    
    _offsetFilter = new OffsetFilter();
    _dcCorrect    = new FilteredStream<int32_t, int32_t>(*_i2s, 1);
    _toNet        = new FormatConverterStream(*_dcCorrect);
    _encoder      = new EncodedAudioStream(_rtp, new G711_ULAWEncoder());
    _sender       = new StreamCopy(*_encoder, *_toNet);
        
    // Build filters and converters
    _dcCorrect->setFilter(0, _offsetFilter);
    if (!_dcCorrect->begin(_pcmIn)) {
      Serial.println("[RTPInput] Error: dcCorrect begin failed");
      return false;
    }
    if (!_toNet->begin(_pcmIn, _pcmNet)) {
      Serial.println("[RTPInput] Error: toNet begin failed");
      return false;
    }
    if (!_encoder->begin(_pcmNet)) {
      Serial.println("[RTPInput] Error: Encoder begin failed");
      return false;
    }
    if (!_udpStream->begin(_destIP, _destPort)){
      Serial.println("[RTPInput] Error: UDPStream begin failed");
      return false;
    }
    return true;
  }

  void update() {
    size_t sent = _sender->copy();
    //Serial.printf("[RTPInput] Sent %u RTP bytes\n", sent);
  }

private:
  I2SStream*                        _i2s;
  const char*                       _ssid;
  const char*                       _password;
  uint16_t                          _destPort;
  IPAddress                         _destIP;
  UDPStream*                        _udpStream;
  RTPOverUDP*                       _rtp;
  OffsetFilter*                     _offsetFilter;
  FilteredStream<int32_t, int32_t>* _dcCorrect;
  FormatConverterStream*            _toNet;
  EncodedAudioStream*               _encoder;
  StreamCopy*                       _sender;


  AudioInfo _pcmIn{16000, 1, 32};
  AudioInfo _pcmNet{8000, 1, 16};
};
