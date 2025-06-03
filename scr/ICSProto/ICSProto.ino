#include <Arduino.h>
#include "SimpleSIPClient.h" 
#include "RTPInput.h"
#include "RTPOutput.h"
#include "UserInput.h"

// Wi-Fi credentials (used inside SimpleSIPClient::begin)
const char* WIFI_SSID     = "Good's Wifi 2.4";
const char* WIFI_PASSWORD = "Class#1956";

// SIP credentials and server
const char* SIP_USER      = "1009";        //1009 is the smaller battery. 1008 is the larger
const char* SIP_PASS      = "1009esp32";
const char* SIP_SERVER    = "10.0.0.33";
const uint16_t SIP_PORT        = 5060;
const uint16_t LOCAL_SIP_PORT        = 5060;

// RTP media ports and I2S pins
const uint16_t RTP_RECV_PORT   = 5004;  // for conference audio receive
const int PIN_WS_OUT   = 33;
const int PIN_BCK_OUT  = 12;
const int PIN_DATA_OUT = 22;
const int PIN_WS_IN    = 15;
const int PIN_BCK_IN   = 14;
const int PIN_DATA_IN  = 32;
const int PIN_VOL_UP   = 26;
const int PIN_VOL_DOWN = 25;
const int PIN_MUTE     = 27;
const int PIN_GROUP    = 34;  //A2

// Tracking for SIP
unsigned long lastSipTick   = 0;
const unsigned long SIP_MS  = 100;
bool callLaunched           = false;
bool rtpStarted             = false;
uint16_t baseExt            = 7000;
uint8_t groups              = 2;

SimpleSIPClient sipClient(WIFI_SSID, WIFI_PASSWORD, SIP_USER, SIP_PASS, SIP_SERVER, SIP_PORT, LOCAL_SIP_PORT);
RTPOutput       rtpOut(WIFI_SSID, WIFI_PASSWORD);
RTPInput        rtpIn(WIFI_SSID, WIFI_PASSWORD);
UserInput       userInput(PIN_VOL_UP, PIN_VOL_DOWN, PIN_MUTE, PIN_GROUP);
float lastAmpGain = 0.0f;

void setup() {
  Serial.begin(115200);
  delay(500);

  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  userInput.begin();
  Serial.println("User Input initalized");
  
  // Initialize and register SIP (also brings up Wi-Fi)
  Serial.println("Starting SIP client...");
  if (!sipClient.begin()) {
    Serial.println("SIP client init failed");
    while (true) delay(1000);
  }
  Serial.println("SIP client init OK");
  Serial.println("→ Sending conference INVITE");
  sipClient.callConference(userInput.readGroup(baseExt, groups), RTP_RECV_PORT);
  Serial.printf("Joining group %u\n", userInput.readGroup(baseExt, groups));
  callLaunched = true;
}

void loop() {
  userInput.update();
  // Throttle SIP processing so we don't flood
  if (millis() - lastSipTick >= SIP_MS) {
    sipClient.update();
    lastSipTick = millis();
  }

  // Call logic
  if (callLaunched && !rtpStarted && sipClient.isInCall()) {
    userInput.setMuted(true);
    uint16_t mediaPort = sipClient.getRtpPort();
    //Serial.printf("Starting RTP to port %u\n", mediaPort);

    // Transmitt pipeline
    if (!rtpIn.begin(SIP_SERVER, mediaPort, PIN_WS_IN, PIN_BCK_IN, PIN_DATA_IN)) {
      Serial.println("RTPInput init failed");
      while (true) delay(100);
    }
    Serial.println("RTPInput ready");

      // Recive pipeline
    if (!rtpOut.begin(RTP_RECV_PORT, PIN_WS_OUT, PIN_BCK_OUT, PIN_DATA_OUT, 1.0f)) {
      Serial.println("RTPOutput init failed");
      while (true) delay(100);
    }
    Serial.println("RTPOutput ready");
    rtpStarted = true;
    lastAmpGain = userInput.getVolume();
  }

  // Stream Logic based on user input

  if (rtpStarted) { float newGain = userInput.getVolume();
    if (newGain != lastAmpGain) {
      rtpOut.setAmpGain(newGain);
      lastAmpGain = newGain;
    }
    
    if(userInput.isMuted()){
      rtpOut.update();  // Drives RTP to Amp Output
    }
    if(!userInput.isMuted()){
      rtpIn.update();   // Drives Mic Input to RTP
    }
  }
}
