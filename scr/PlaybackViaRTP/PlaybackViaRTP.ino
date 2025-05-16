#include <Arduino.h>
#include "RTPOutput.h"

const char* WIFI_SSID     = "Good's Wifi 2.4";  // These will need changed for your Wifi
const char* WIFI_PASSWORD = "Class#1956";

RTPOutput RTPOut(WIFI_SSID, WIFI_PASSWORD);

void setup() {
  Serial.begin(115200);
  delay(500);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  if (!RTPOut.begin(5006, /*WS*/33, /*BCK*/12, /*DATA*/22, 0.5f)) {
    Serial.println("RTPOutput init failed");
    while (true) delay(100);
  }
}

void loop() {
  RTPOut.update();
}
