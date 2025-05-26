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
//#include "AudioTools/CoreAudio/AudioFilter/MedianFilter.h"
//#include "AudioTools/CoreAudio/AudioFilter/Filter.h"

using namespace audio_tools;

class RTPInput {
public:
  RTPInput(const char* ssid, const char* password)
    : _ssid(ssid)
    , _password(password)
    , _udpStream(nullptr)
    , _rtp(nullptr)
    , _i2sIn(nullptr)
    , _offsetFilter(nullptr)
    //, _filterChain(nullptr)
    //, _medianFilter(nullptr)
    , _dcCorrect(nullptr)
    , _toNet(nullptr)
    , _encoder(nullptr)
    , _sender(nullptr) {}

  bool begin(const IPAddress& dest, uint16_t port, int pin_ws, int pin_bck, int pin_data) {
    // Instantiate pipeline components
    _udpStream    = new UDPStream(_ssid, _password);
    _rtp          = new RTPOverUDP(*_udpStream);
    _i2sIn        = new I2SStream();
    _offsetFilter = new OffsetFilter();
    //_medianFilter = new MedianFilter<int32_t>(4);
    _dcCorrect    = new FilteredStream<int32_t, int32_t>(*_i2sIn, 1);
    _toNet        = new FormatConverterStream(*_dcCorrect);
    _encoder      = new EncodedAudioStream(_rtp, new G711_ULAWEncoder());
    _sender       = new StreamCopy(*_encoder, *_toNet);
    _dest         = dest;
    _port         = port;

    // Configure I2S input
    Serial.println("[RTPInput] Configuring I2S input...");
    auto cfg = _i2sIn->defaultConfig(RX_MODE);
    cfg.copyFrom(_pcmIn);
    cfg.i2s_format = I2S_STD_FORMAT;
    cfg.pin_ws     = pin_ws;
    cfg.pin_bck    = pin_bck;
    cfg.pin_data   = pin_data;
    cfg.is_master  = true;
    cfg.port_no  = I2S_NUM_0;
    if (!_i2sIn->begin(cfg)) {
      Serial.println("[RTPInput] Error: I2S input begin failed");
      return false;
    }

    // Build filters and converters. Median Filter added to chain to reduce popping
    //Filter<int32_t>* filters[2] = {_offsetFilter, _medianFilter};
    //_filterChain = new FilterChain<int32_t,2>(std::move(filters));
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
    if (!_udpStream->begin(dest, port)) {
      Serial.println("[RTPInput] Error: UDPStream.begin() failed");
      return false;
    }
    Serial.printf("[RTPInput] UDPStream.begin() succeeded to %s:%u\n", dest.toString().c_str(), port);
    
    // Send three HELLO datagrams
    for (int i = 0; i < 3; ++i) {
      const char* hello = "HELLO";
      size_t n = _udpStream->write(reinterpret_cast<const uint8_t*>(hello), strlen(hello));
    Serial.printf(
      "[RTPInput] HELLO sent (%u bytes) to %s:%u\n", n, dest.toString().c_str(),port);
    delay(100);
  }

    return true;
  }

  void update() {
    size_t sent = _sender->copy();
    //Serial.printf("[RTPInput] Sent %u RTP bytes\n", sent);
  }

private:
  const char*                       _ssid;
  const char*                       _password;
  uint16_t                          _port;
  IPAddress                         _dest;
  UDPStream*                        _udpStream;
  RTPOverUDP*                       _rtp;
  I2SStream*                        _i2sIn;
  OffsetFilter*                     _offsetFilter;
  //MedianFilter<int32_t>*            _medianFilter;
  FilteredStream<int32_t, int32_t>* _dcCorrect;
  //FilterChain<int32_t,2>*            _filterChain;
  FormatConverterStream*            _toNet;
  EncodedAudioStream*               _encoder;
  StreamCopy*                       _sender;


  AudioInfo _pcmIn{16000, 1, 16};
  AudioInfo _pcmNet{8000, 1, 16};
};
