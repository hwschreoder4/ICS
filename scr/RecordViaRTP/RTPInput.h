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
#include "AudioTools/CoreAudio/AudioFilter/Filter.h"

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
    , _dcCorrect(nullptr)
    , _highPass(nullptr)
    , _filteredHP(nullptr)
    , _lowPass(nullptr)
    , _filteredLP(nullptr)
   //, _toNet(nullptr)
    , _encoder(nullptr)
    , _sender(nullptr) {}

  bool begin(const IPAddress& dest, uint16_t port, int pin_ws, int pin_bck, int pin_data) {
    // Instantiate pipeline components
    _udpStream    = new UDPStream(_ssid, _password);
    _rtp          = new RTPOverUDP(*_udpStream);
    _i2sIn        = new I2SStream();
    _offsetFilter = new OffsetFilter();
    _dcCorrect    = new FilteredStream<int32_t, int32_t>(*_i2sIn, 1);
    auto sampleRate = _pcmIn.sample_rate;  
    _highPass     = new HighPassFilter<long>(300.0f, sampleRate, 0.7f);
    _lowPass      = new LowPassFilter<long>(3400.0f, sampleRate, 0.7f);
    //_toNet        = new FormatConverterStream(*_dcCorrect);
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
    if (!_i2sIn->begin(cfg)) {
      Serial.println("[RTPInput] Error: I2S input begin failed");
      return false;
    }

    // Build filters and converters
    _dcCorrect->setFilter(0, _offsetFilter);
    if (!_dcCorrect->begin(_pcmIn)) {
      Serial.println("[RTPInput] Error: dcCorrect begin failed");
      return false;
    }

    // High-pass filter stream
    _filteredHP = new FilteredStream<int32_t,int32_t>(* _dcCorrect, 1);
    _filteredHP->setFilter(0, _highPass);
    if (!_filteredHP->begin(_pcmIn)) {
      Serial.println("[RTPInput] Error: filteredHP begin failed");
      return false;
    }

    // Low-pass filter stream
    _filteredLP = new FilteredStream<int32_t,int32_t>(* _filteredHP, 1);
    _filteredLP->setFilter(0, _lowPass);
    if (!_filteredLP->begin(_pcmIn)) {
      Serial.println("[RTPInput] Error: filteredLP begin failed");
      return false;
    }

    //if (!_toNet->begin(_pcmIn, _pcmNet)) {
    //  Serial.println("[RTPInput] Error: toNet begin failed");
    //  return false;
    //}
    if (!_encoder->begin(_pcmNet)) {
      Serial.println("[RTPInput] Error: Encoder begin failed");
      return false;
    }
    if (!_udpStream->begin(dest, port)) {
      Serial.println("[RTPInput] Error: UDPStream.begin() failed");
      return false;
    }
    // 2) (optional) send three HELLO datagrams
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
    Serial.printf("[RTPInput] Sent %u RTP bytes\n", sent);
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
  FilteredStream<int32_t, int32_t>* _dcCorrect;
  HighPassFilter<long>*                   _highPass;
  FilteredStream<int32_t,int32_t>*  _filteredHP;
  LowPassFilter<long>*                    _lowPass;
  FilteredStream<int32_t,int32_t>*  _filteredLP;
  //FormatConverterStream*            _toNet;
  EncodedAudioStream*               _encoder;
  StreamCopy*                       _sender;


  AudioInfo _pcmIn{8000, 1, 16};
  AudioInfo _pcmNet{8000, 1, 16};
};
