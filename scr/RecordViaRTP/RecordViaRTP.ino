#include <Arduino.h>
#include "RTPInput.h"

const char* WIFI_SSID     = "Good's Wifi 2.4";  // These will need changed for your Wifi
const char* WIFI_PASSWORD = "Class#1956";

RTPInput RTPIn(WIFI_SSID, WIFI_PASSWORD);

void setup() {
  Serial.begin(115200);
  delay(500);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  if (!RTPIn.begin(IPAddress(10,0,0,95), 5004, /*WS*/15, /*BCK*/14, /*DATA*/32)) {
    Serial.println("RTPInput init failed");
    while (true) delay(100);
  }
}

void loop() {
  RTPIn.update();
}