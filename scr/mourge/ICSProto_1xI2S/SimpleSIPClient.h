#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoSIP.h>

class SimpleSIPClient {
public:
  SimpleSIPClient(const char* ssid,
                  const char* wifiPass,
                  const char* user,
                  const char* pass,
                  const char* server,
                  uint16_t port,
                  uint16_t localPort = 5060)
    : _ssid(ssid)
    , _wifiPass(wifiPass)
    , _user(user)
    , _pass(pass)
    , _server(server)
    , _port(port)
    , _localPort(localPort)
    , _sip(outBuf, sizeof(outBuf))
  {}

  bool begin() {
    // 1) bring up Wi-Fi
    WiFi.begin(_ssid, _wifiPass);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
      if (millis() - start > 10000) {
        Serial.println("Wi-Fi connect failed");
        return false;
      }
      delay(500);
    }
    Serial.print("Wi-Fi up, IP = ");
    Serial.println(WiFi.localIP());

    // 2) start listening for SIP on our local UDP port
    _udp.begin(_localPort);

    // 3) initialize our Sip instance
    String lip = WiFi.localIP().toString();
    strncpy(_localIp, lip.c_str(), sizeof(_localIp));
    _localIp[sizeof(_localIp)-1] = '\0';
    _sip.Init(
      _server,
      _port,
      _localIp,
      _localPort,
      _user,
      _pass
    );
    return true;
  }

  void loop() {
    // pump incoming SIP packets through the SIP state machine
    _sip.Processing(inBuf, sizeof(inBuf));
  }

  bool callConference(uint16_t conferenceExt, uint16_t localRTPPort) {
    snprintf(_extBuf, sizeof(_extBuf), "%u", (unsigned)conferenceExt);
    _pendingSdp =
      String(F("v=0\r\n")) +
      "o=- 0 0 IN IP4 " + WiFi.localIP().toString() + "\r\n" +
      "s=ESP32 SIP Call\r\n" +
      "c=IN IP4 " + WiFi.localIP().toString() + "\r\n" +
      "t=0 0\r\n" +
      "m=audio " + String(localRTPPort) + " RTP/AVP 0\r\n" +
      "a=rtpmap:0 PCMU/8000\r\n";
      "a=ptime:20\r\n";

    // this will drive the 401/ack/invite dance under the covers
  return _sip.Dial(
    _extBuf,
    "ESP32 Call",  
    _pendingSdp.c_str(),
    _pendingSdp.length()
    );
  }
  bool isInCall() const {
    bool reg = _sip.IsInCall();
    if(reg){Serial.println("SIP In Call Complete");}
    return reg;
  }

  uint16_t getRtpPort() const {return _sip.GetRemoteRtpPort();}


private:
  const char* _ssid;
  const char* _wifiPass;
  const char* _user;
  const char* _pass;
  const char* _server;
  uint16_t    _port;
  uint16_t    _localPort;
  WiFiUDP     _udp;
  char        inBuf[1024];
  char        outBuf[1024];
  Sip         _sip;
  char        _extBuf[8];
  char        _localIp[16]; 
  String      _pendingSdp;
};
