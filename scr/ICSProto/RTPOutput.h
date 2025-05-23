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
  RTPOutput(const char* ssid, const char* password)
    : _ssid(ssid)
    , _password(password)
    , _udpStream(nullptr)
    , _rtp(nullptr)
    , _i2sOut(nullptr)
    , _volume(nullptr)
    , _jitterBuffer(nullptr)
    , _decoder(nullptr)
    , _bufCopy(nullptr)
    , _toStereo(nullptr)
    , _player(nullptr)
    , _underflows(0)
    , _lastLog(0) {}

bool begin(uint16_t port,
             int pin_ws, int pin_bck, int pin_data,
             float volumeLevel = 0.5f) {
    Serial.printf("[RTPOutput] begin(): port=%u ws=%d bck=%d data=%d vol=%.2f\n", port, pin_ws, pin_bck, pin_data, volumeLevel);
    // Instantiate pipeline components
    _udpStream    = new UDPStream(_ssid, _password);
    _rtp          = new RTPOverUDP(*_udpStream);
    _i2sOut       = new I2SStream();
    _volume       = new VolumeStream(*_i2sOut);
    _jitterBuffer = new RingBufferStream(JITTER_BUF_SIZE);
    _decoder      = new EncodedAudioStream(_rtp, new G711_ULAWDecoder());
    _bufCopy      = new StreamCopy(*_jitterBuffer, *_decoder);
    _toStereo     = new FormatConverterStream(*_jitterBuffer);
    _player       = new StreamCopy(*_volume, *_toStereo);

    // Begin UDP stream
    if (!_udpStream->begin(port)) {
      Serial.printf("[RTPOutput]Error: UDPStream bind failed on port %u\n", port);
      return false;
    }

    // Configure I2S output
    Serial.println("[RTPOutput]Configuring I2S output...");
    auto cfg = _i2sOut->defaultConfig(TX_MODE);
    cfg.copyFrom(_pcmStereo);
    cfg.pin_ws   = pin_ws;
    cfg.pin_bck  = pin_bck;
    cfg.pin_data = pin_data;
    cfg.i2s_format = I2S_STD_FORMAT;
    cfg.port_no  = I2S_NUM_1;
    cfg.buffer_size  = STEREO_FRAME_BYTES;
    cfg.buffer_count = 40;
    if (!_i2sOut->begin(cfg)) {
      Serial.println("[RTPOutput]Error: I2SStream begin failed");
      return false;
    }
    //Configure  Decoder
    if (!_decoder->begin(_pcmMono)) {
      Serial.println("[RTPOutput]Error: Decoder begin failed");
      return false;
    }

    // Configure volume control
    auto vcfg = _volume->defaultConfig();
    vcfg.copyFrom(_pcmStereo);
    _volume->begin(vcfg);
    _volume->setVolume(volumeLevel);

    // Set up format conversion
    if (!_toStereo->begin(_pcmMono, _pcmStereo)){
      Serial.println("[RTPOutput]Error: toStero Convertor begin failed");
      return false;
    }
    Serial.println("[RTPOutput] FormatConverter begin OK");
    // Pre-fill jitter buffer
    while (_jitterBuffer->available() < MONO_FRAME_BYTES *12) {
      _bufCopy->copy(); // pump in 12×20 ms = 240 ms of audio
      delay(1);
    }
    Serial.println("[RTPOutput]Buffer warmed up—starting playback");

    // Begin playback
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

    //unsigned long now = millis();
    //if (now - _lastLog > 5000) {
    //  Serial.printf("Underflows: %d, Buffer: %u bytes\n",
    //                _underflows, _jitterBuffer->available());
    //  _underflows = 0;
    //  _lastLog = now;
    //}
  }

private:
  const char*      _ssid;
  const char*      _password;
  UDPStream*       _udpStream;
  RTPOverUDP*      _rtp;
  I2SStream*       _i2sOut;
  VolumeStream*    _volume;
  RingBufferStream* _jitterBuffer;
  EncodedAudioStream* _decoder;
  StreamCopy*      _bufCopy;
  FormatConverterStream* _toStereo;
  StreamCopy*      _player;

  int              _underflows;
  unsigned long    _lastLog;

  static const size_t MONO_FRAME_BYTES   = 160 * 2;
  static const size_t STEREO_FRAME_BYTES = 160 * 2 * 2;
  static const size_t REFILL_THRESHOLD   = MONO_FRAME_BYTES * 2;
  static const size_t JITTER_BUF_SIZE    = 160 * 2 * 2 * 30;

  AudioInfo _pcmMono   {8000, 1, 16};
  AudioInfo _pcmStereo {8000, 2, 16};
};