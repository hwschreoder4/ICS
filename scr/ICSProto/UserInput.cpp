#include "UserInput.h"

// define your static flags in the .cpp
volatile bool UserInput::_rawUpFlag   = false;
volatile bool UserInput::_rawDownFlag = false;
volatile bool UserInput::_rawMuteFlag = false;

UserInput::UserInput(uint8_t pinUp, uint8_t pinDown, uint8_t pinMute, uint8_t pinGroup,
                     unsigned long debounceMs, float volumeStep,
                     float minVol, float maxVol)
  : _pinUp(pinUp), _pinDown(pinDown), _pinMute(pinMute), _pinGroup(pinGroup),
    _debounceMs(debounceMs), _volumeStep(volumeStep),
    _minVol(minVol), _maxVol(maxVol),
    _volume(maxVol), _lastUpTime(0),
    _lastDownTime(0), _lastMuteTime(0),
    _muted(false) {}

void UserInput::begin() {
  pinMode(_pinUp, INPUT_PULLUP);
  pinMode(_pinDown, INPUT_PULLUP);
  pinMode(_pinMute, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(_pinUp), handleUpISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(_pinDown), handleDownISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(_pinMute), handleMuteISR, FALLING);

  pinMode(_pinGroup, INPUT);

  Serial.println("UserInput initialized.");
}

void UserInput::update() {
  unsigned long now = millis();
  if (_rawUpFlag) {
    _rawUpFlag = false;
    if (now - _lastUpTime >= _debounceMs) {
      _lastUpTime = now;
      _volume = min(_volume + _volumeStep, _maxVol);
      //Serial.printf("[UserInput] Volume Up → %.2f\n", _volume);
    }
  }
    if (_rawDownFlag) {
    _rawDownFlag = false;
    if (now - _lastDownTime >= _debounceMs) {
      _lastDownTime = now;
      _volume = max(_volume - _volumeStep, _minVol);
      //Serial.printf("[UserInput] Volume Down → %.2f\n", _volume);
    }
  }
    if (_rawMuteFlag) {
    _rawMuteFlag = false;
    if (now - _lastMuteTime >= _debounceMs) {
      _lastMuteTime = now;
      _muted = !_muted;
      //Serial.printf("[UserInput] Mute toggled → %s\n", _muted ? "ON" : "OFF");
    }
  }
  
}

float UserInput::getVolume() const { return _volume; }
bool  UserInput::isMuted()  const { return _muted; }

/** Map the ADC reading into your conference extension. */
uint16_t UserInput::readGroup(uint16_t baseExt = 7000, uint8_t groups = 2) {
  int raw = analogRead(_pinGroup);                // 0..4095
  uint8_t band = constrain(raw / (4096 / groups) + 1, 1, groups);
  return baseExt + band;
}

// Interrup handling for the volume and mute
void IRAM_ATTR UserInput::handleUpISR()   { _rawUpFlag = true; }
void IRAM_ATTR UserInput::handleDownISR() { _rawDownFlag = true; }
void IRAM_ATTR UserInput::handleMuteISR() { _rawMuteFlag = true; }
