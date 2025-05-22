#include <Arduino.h>
#include "SimpleSIPClient.h"   // wrapper around ArduinoSIP :contentReference[oaicite:1]{index=1}
#include "RTPInput.h"
#include "RTPOutput.h"

// -- Wi-Fi credentials (used inside SimpleSIPClient::begin) --
const char* WIFI_SSID     = "Good's Wifi 2.4";
const char* WIFI_PASSWORD = "Class#1956";

// -- SIP credentials and server --
const char* SIP_USER      = "1008";
const char* SIP_PASS      = "1008esp32";
const char* SIP_SERVER    = "10.0.0.33";
const uint16_t SIP_PORT        = 5060;
const uint16_t CONFERENCE_EXT  = 7001;

// -- RTP media ports and I2S pins --
const uint16_t RTP_SEND_PORT   = 5004;  // for conference audio transmit
const uint16_t RTP_RECV_PORT   = 5006;  // for conference audio receive
const int PIN_WS_OUT   = 33;
const int PIN_BCK_OUT  = 12;
const int PIN_DATA_OUT = 22;
const int PIN_WS_IN    = 15;
const int PIN_BCK_IN   = 14;
const int PIN_DATA_IN  = 32;

// -- Tracking for SIP
unsigned long lastSipTick   = 0;
const unsigned long SIP_MS  = 100;
bool callLaunched           = false;
bool rtpStarted             = false;

SimpleSIPClient sipClient(WIFI_SSID, WIFI_PASSWORD, SIP_USER, SIP_PASS, SIP_SERVER, SIP_PORT, RTP_RECV_PORT);
RTPOutput       rtpOut(WIFI_SSID, WIFI_PASSWORD);
RTPInput        rtpIn(WIFI_SSID, WIFI_PASSWORD);

void setup() {
  Serial.begin(115200);
  delay(500);

  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // Initialize and register SIP (also brings up Wi-Fi)
  Serial.println("Starting SIP client...");
  if (!sipClient.begin()) {
    Serial.println("SIP client init failed");
    while (true) delay(1000);
  }
  Serial.println("SIP client init OK");
  Serial.println("→ Sending conference INVITE");
  sipClient.callConference(CONFERENCE_EXT, RTP_SEND_PORT);
  callLaunched = true;
}

void loop() {
  unsigned long now = millis();

  // Throttle SIP processing so we don't flood
  if (now - lastSipTick >= SIP_MS) {
    sipClient.loop();
    lastSipTick = now;
  }

  // Once the 200 OK has arrived (isInCall), start RTP exactly once
  if (callLaunched && !rtpStarted && sipClient.isInCall()) {
    uint16_t mediaPort = sipClient.getRtpPort();
    Serial.printf("Starting RTP to port %u\n", mediaPort);

    // Transmitt pipeline
    if (!rtpIn.begin(SIP_SERVER, mediaPort, PIN_WS_IN, PIN_BCK_IN, PIN_DATA_IN)) {
      Serial.println("RTPInput init failed");
      while (true) delay(100);
    }
    Serial.println("RTPInput ready");

        // Recive pipeline
    //if (!rtpOut.begin(RTP_RECV_PORT, PIN_WS_OUT, PIN_BCK_OUT, PIN_DATA_OUT, 0.8f)) {
    //  Serial.println("RTPOutput init failed");
    //  while (true) delay(100);
    //}
    Serial.println("RTPOutput ready");

    rtpStarted = true;
  }

  // 3) Once RTP is started, keep pumping audio at full speed
  if (rtpStarted) {
    //rtpOut.update();
    rtpIn.update();
  }
}