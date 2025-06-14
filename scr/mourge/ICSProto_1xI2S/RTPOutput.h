/*
 * RTPOutput.h
 * Based on work by Phil Schatzmann (https://github.com/pschatzmann/arduino-audio-tools)
 * Licensed under the GNU General Public License v3.0
 * (c) 2025 Hugo Schroeder

 * This serves to handle all the elements needed to recive an RTP stream and play it back
 * Future clean up to include the ablity to disable the serial logging of underflow easily

 */

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "AudioTools.h"
#include "AudioTools/Communication/UDPStream.h"
#include "RTPOverUDP.h"
#include "AudioTools/AudioCodecs/CodecG7xx.h"
#include "AudioTools/CoreAudio/Buffers.h"

using namespace audio_tools;

class RTPOutput {
public:
  RTPOutput(I2SStream* i2s, const char* ssid, const char* password)
    : _i2s(i2s)
    , _ssid(ssid)
    , _password(password)
    , _udpStream(nullptr)
    , _rtp(nullptr)
    , _volume(nullptr)
    , _jitterBuffer(nullptr)
    , _decoder(nullptr)
    , _bufCopy(nullptr)
    , __toMono(nullptr)
    , _player(nullptr)
    , _underflows(0)
    , _lastLog(0) {}

bool begin(uint16_t localPort, /*bool toneMode = false,*/  float volumeLevel = 0.5f) {
    // Instantiate pipeline components
    _udpStream    = new UDPStream(_ssid, _password);
    _rtp          = new RTPOverUDP(*_udpStream);
    _volume       = new VolumeStream(*_i2s);
    _jitterBuffer = new RingBufferStream(JITTER_BUF_SIZE);
    _decoder      = new EncodedAudioStream(_rtp, new G711_ULAWDecoder());
    _bufCopy      = new StreamCopy(*_jitterBuffer, *_decoder);
    __toMono      = new FormatConverterStream(*_jitterBuffer);
    _player       = new StreamCopy(*_volume, *__toMono);

    if (!_udpStream->begin(localPort)) {
      Serial.println("[RTPOutput] Error: UDPStream begin failed");
      return false;
    }
    if (!_decoder->begin(_pcmNet)) {
      Serial.println("[RTPOutput] Error: Decoder begin failed");
      return false;
    }

    auto vcfg = _volume->defaultConfig();
    vcfg.copyFrom(_pcmMono);
    _volume->begin(vcfg);
    _volume->setVolume(volumeLevel);

    //Format converter
    if (!__toMono->begin(_pcmNet, _pcmMono)) {
      Serial.println("[RTPOutput] Error: Converter begin failed");
      return false;
    }
    // Pre-fill jitter buffer
    while (_jitterBuffer->available() < MONO_FRAME_BYTES *12) {
      _bufCopy->copy(); // pump in 12×20 ms = 240 ms of audio
      delay(1);
    }
    Serial.println("[RTPOutput]Buffer warmed up—starting playback");
  
    _player->begin();
    _lastLog = millis();
    return true;
  }

  void update() {
    // Refill jitter
    while (_jitterBuffer->availableForWrite() >= MONO_FRAME_BYTES && _jitterBuffer->available() < REFILL_THRESHOLD) {
      _bufCopy->copy();
    }
    if (_jitterBuffer->available() < MONO_FRAME_BYTES) {
      _underflows++;
    }
    _player->copy();

    // after _player->copy();
    unsigned long now = millis();
    if (now - _lastLog > 5000) {
      Serial.printf("Underflows: %d, Buffer: %u bytes\n",
                    _underflows, _jitterBuffer->available());
      _underflows = 0;
      _lastLog = now;
    }
  }

private:
  I2SStream*       _i2s;
  const char*      _ssid;
  const char*      _password;
  UDPStream*       _udpStream;
  RTPOverUDP*      _rtp;
  VolumeStream*    _volume;
  RingBufferStream* _jitterBuffer;
  EncodedAudioStream* _decoder;
  StreamCopy*      _bufCopy;
  FormatConverterStream* __toMono;
  StreamCopy*      _player;

  int              _underflows;
  unsigned long    _lastLog;

  static const size_t MONO_FRAME_BYTES   = 160 * 2;
  static const size_t STEREO_FRAME_BYTES = 160 * 2 * 2;
  static const size_t REFILL_THRESHOLD   = MONO_FRAME_BYTES * 2;
  static const size_t JITTER_BUF_SIZE    = 160 * 2 * 2 * 30;

  AudioInfo _pcmNet   {8000, 1, 16};
  AudioInfo _pcmMono {16000, 1, 32};
};