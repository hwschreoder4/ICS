/*
 * PlaybackViaSD.ino
 * Based on work by Phil Schatzmann (https://github.com/pschatzmann/arduino-audio-tools)
 * Licensed under the GNU General Public License v3.0
 * (c) 2025 Hugo Schroeder

 * This reads a 8kHz, single channel WAV file from the SD card and outputs it to the amp

 */

#pragma once
#include <AudioLogger.h>
#include <AudioTools.h>
#include <AudioToolsConfig.h>
#include <SPI.h>
#include <SD.h>
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

const int chipSelect=4;
I2SStream i2s; // final output of decoded stream
VolumeStream volume(i2s);
EncodedAudioStream decoder(&volume, new MP3DecoderHelix()); // Decoding stream
StreamCopy copier; 
File audioFile;

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Error);  

  // setup file
  SD.begin(chipSelect);
  audioFile = SD.open("/Swing Swing.mp3");

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  config.pin_ws = 33;
  config.pin_bck = 12;
  config.pin_data = 22;
  config.buffer_size = 640;
  config.buffer_count = 40;
  i2s.begin(config);

  // setup I2S based on sampling rate provided by decoder
  decoder.begin();

  // set up volume control

  auto vcfg = volume.defaultConfig();
  vcfg.copyFrom(config);
  volume.begin(vcfg);
  volume.setVolume(0.5);

  // begin copy
  copier.begin(decoder, audioFile);

}

void loop(){

  if (!copier.copy()) {
    stop();}
}