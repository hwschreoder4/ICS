#include <Arduino.h>
#include "SimpleSIPClient.h"   // wrapper around ArduinoSIP :contentReference[oaicite:1]{index=1}
#include "RTPInput.h"
#include "RTPOutput.h"

// Wi-Fi credentials (used inside SimpleSIPClient::begin)
const char* WIFI_SSID     = "Good's Wifi 2.4";
const char* WIFI_PASSWORD = "Class#1956";

// SIP credentials and server
const char* SIP_USER      = "1008";
const char* SIP_PASS      = "1008esp32";
const char* SIP_SERVER    = "10.0.0.33";
const uint16_t SIP_PORT        = 5060;
const uint16_t LOCAL_SIP_PORT        = 5060;
const uint16_t CONFERENCE_EXT  = 7001;

// RTP media ports and I2S pins
const uint16_t RTP_RECV_PORT   = 5004;  // for conference audio receive
const int PIN_DATA_AMP = 22;
const int PIN_WS       = 15;
const int PIN_BCK      = 14;
const int PIN_DATA_MIC = 32;

// Tracking for SIP
unsigned long lastSipTick   = 0;
const unsigned long SIP_MS  = 100;
bool callLaunched           = false;
bool rtpStarted             = false;

SimpleSIPClient sipClient(WIFI_SSID, WIFI_PASSWORD, SIP_USER, SIP_PASS, SIP_SERVER, SIP_PORT, LOCAL_SIP_PORT);
I2SStream*      fullDuplexI2S;
RTPInput*       rtpIn;
RTPOutput*      rtpOut;

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
  sipClient.callConference(CONFERENCE_EXT, RTP_RECV_PORT);
  callLaunched = true;
  Serial.println("→ Waiting for call to be established…");
  while (!sipClient.isInCall()) {
    sipClient.loop();
    delay(SIP_MS);           
  }

  uint16_t mediaPort = sipClient.getRtpPort();
  Serial.printf("Starting RTP to port %u\n", mediaPort);
  
  // Full-duplex I²S setup, once at boot
  fullDuplexI2S = new I2SStream();
  AudioInfo fmt{16000,1,32};
  auto cfg = fullDuplexI2S->defaultConfig(RXTX_MODE);
  cfg.copyFrom(fmt);
  cfg.pin_ws      = PIN_WS;
  cfg.pin_bck     = PIN_BCK;
  cfg.pin_data    = PIN_DATA_AMP;  // amp DIN
  cfg.pin_data_rx = PIN_DATA_MIC;  // mic SD
  cfg.is_master   = true;
  cfg.buffer_size = 2048;
  cfg.buffer_count= 24;
  fullDuplexI2S->begin(cfg);

  // Instantiate audio pipelines (no network yet)
  rtpIn  = new RTPInput (fullDuplexI2S, WIFI_SSID, WIFI_PASSWORD );
  rtpOut = new RTPOutput(fullDuplexI2S, WIFI_SSID, WIFI_PASSWORD);
  rtpOut->begin(RTP_RECV_PORT, 1.0f /*Volume*/);
  rtpIn ->begin(LOCAL_SIP_PORT, SIP_SERVER, mediaPort);
  rtpStarted = true;
}


void loop() {
  unsigned long now = millis();

  // Throttle SIP processing so we don't flood
  if (now - lastSipTick >= SIP_MS) {
    sipClient.loop();
    lastSipTick = now;
  }

  // Once RTP is started, keep pumping audio at full speed
  if (rtpStarted) {
    rtpOut->update();  // Drives RTP to Amp Output
    rtpIn->update();   // Drives Mic Input to RTP
  }
}