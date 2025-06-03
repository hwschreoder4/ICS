#pragma once
#include <Arduino.h>

class UserInput {
public:
  UserInput(uint8_t pinUp, uint8_t pinDown, uint8_t pinMute, uint8_t pinGroup,
            unsigned long debounceMs = 100, float volumeStep = 0.2f,
            float minVol = 0.0f, float maxVol = 1.0f);

  void begin();
  void update();
  float getVolume() const;
  bool  isMuted()  const;
  void setMuted(bool m) {_muted =m;}
  uint16_t readGroup (uint16_t baseExt, uint8_t groups);

private:
  // pins, timings, volume state...
  uint8_t _pinUp, _pinDown, _pinMute, _pinGroup;
  unsigned long _lastUpTime, _lastDownTime, _lastMuteTime;
  bool          _muted;
  float         _volume, _volumeStep, _minVol, _maxVol;
  unsigned long _debounceMs;

  // ISR flags (only declarations here)
  static volatile bool _rawUpFlag;
  static volatile bool _rawDownFlag;
  static volatile bool _rawMuteFlag;

  // ISR handlers (declarations only)
  static void IRAM_ATTR handleUpISR();
  static void IRAM_ATTR handleDownISR();
  static void IRAM_ATTR handleMuteISR();
};
