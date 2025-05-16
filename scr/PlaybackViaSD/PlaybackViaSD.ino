
/*
 * PlaybackViaSD.ino
 * Based on work by Phil Schatzmann (https://github.com/pschatzmann/arduino-audio-tools)
 * Licensed under the GNU General Public License v3.0
 * (c) 2025 Hugo Schroeder

 * This reads a 8kHz, single channel WAV file from the SD card and outputs it to the amp

 */


#pragma once
#include <SPI.h>
#include <SD.h>
#include <AudioLogger.h>
#include <AudioToolsConfig.h>
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"

using namespace audio_tools;

#define SD_CS_PIN 4    //A5 is GPIO 4...
#define SD_SCK_PIN 5   // Serial Clock
#define SD_MOSI_PIN 19 // Master Out, Slave In
#define SD_MISO_PIN 21 // Master In, Slave Out
#define WAV_FILENAME "/swing.wav"

// SPI instance for SD (HSPI)
SPIClass sdSpi(HSPI);

// Audio formats
const AudioInfo pcmMono(8000, 1, 16);    // WAV is 8kHz, mono, 16-bit
const AudioInfo pcmStereo(8000, 2, 16);  // convert to stereo for I2S

// I2S output and volume
I2SStream i2sOut;
VolumeStream volume(i2sOut);

// WAV reader and converter
EncodedAudioStream* wavStream;
FormatConverterStream* monoToStereo;
StreamCopy* playCopy;

void setup() {
  Serial.begin(115200);

  // SD Init
  sdSpi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, sdSpi)) {
    Serial.println("[SD] initialization failed!");
    while (true) delay(10);
  }
  if (!SD.exists(WAV_FILENAME)) {
    Serial.print("[SD] File not found: ");
    Serial.println(WAV_FILENAME);
    while (true) delay(10);
  }

  // Open and Decode WAV
  File wavFile = SD.open(WAV_FILENAME, FILE_READ);
  wavStream = new EncodedAudioStream(&wavFile, new WAVDecoder());

  // Mono ->Stero convertor
  monoToStereo = new FormatConverterStream(*wavStream);
  if (!monoToStereo->begin(pcmMono, pcmStereo)) {
    Serial.println("[Conv] begin failed");
    while (true) delay(10);
  }

  // I2S config 
  auto cfg = i2sOut.defaultConfig(TX_MODE);
  cfg.copyFrom(pcmStereo);
  cfg.pin_ws   = 33;
  cfg.pin_bck  = 12;
  cfg.pin_data = 22;
  if (!i2sOut.begin(cfg)) {
    Serial.println("[I2S] begin failed");
    while (true) delay(10);
  }

  // Volume Init
  auto vcfg = volume.defaultConfig();
  vcfg.copyFrom(pcmStereo);
  volume.begin(vcfg);
  volume.setVolume(0.5);

  playCopy = new StreamCopy(volume, *monoToStereo);
  playCopy->begin();

  Serial.println("[SD] Starting WAV playback...");
}

void loop() {
  if (monoToStereo->available() > 0) {
    playCopy->copy();  // pushes STEREO_FRAME_BYTES to I2S
  }
  // once EOF, available() will be 0: playback stops
}
