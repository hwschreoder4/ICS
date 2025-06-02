/* ====================================================================

   Copyright (c) 2018 Juergen Liegner  All rights reserved.
   (https://www.mikrocontroller.net/topic/444994)
   
   Copyright (c) 2019 Thorsten Godau (dl9sec)
   (Created an Arduino library from the original code and did some beautification)
   
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.

   3. Neither the name of the author(s) nor the names of any contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.

   ====================================================================*/
#include <MD5Builder.h>
#include <WiFiUdp.h>
#include "ArduinoSIP.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware and API independent Sip class
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

Sip::Sip(char *pBuf, size_t lBuf) {

  pbuf = pBuf;
  lbuf = lBuf;
  pDialNr = "";
  pDialDesc = "";

}


Sip::~Sip() {
  
}


void Sip::Init(const char *SipIp, int SipPort, const char *MyIp, int MyPort, const char *SipUser, const char *SipPassWd, int MaxDialSec) {
  
  Udp.begin(MyPort);
  
  caRead[0] = 0;
  pbuf[0] = 0;
  pSipIp = SipIp;
  iSipPort = SipPort;
  pSipUser = SipUser;
  pSipPassWd = SipPassWd;
  pMyIp = MyIp;
  iMyPort = MyPort;
  iAuthCnt = 0;
  iRingTime = 0;
  iMaxTime = MaxDialSec * 1000;
}

bool Sip::Register(const char* challenge) {
    // If authentication in progress, iAuthCnt > 0, a "401 Unauthorized" challenge first.
    // Otherwise, this is the first REGISTER attempt.
    char realm[128] = { 0 }, nonce[128] = { 0 }, opaque[128] = { 0 }, qop[32] = { 0 };
    uint16_t cseq = (iAuthCnt == 0) ? 1 : (1 + iAuthCnt);

    // Build the REGISTER request
    pbuf[0] = '\0';
    AddSipLine("REGISTER sip:%s SIP/2.0", pSipIp);                   // Request‐URI = server
    AddSipLine("Via: SIP/2.0/UDP %s:%u;branch=%010u;rport=%u",      // Via: our IP:port
        pMyIp, iMyPort, Random(), iMyPort);
    AddSipLine("Max-Forwards: 70");
    AddSipLine("From: <sip:%s@%s>;tag=%010u",                        // From: our user
        pSipUser, pSipIp, Random());
    AddSipLine("To: <sip:%s@%s>", pSipUser, pSipIp);                  // To: same as From
    if (iAuthCnt == 0) { regid = Random(); }                          // Pick a random 32-bit regId; server to challenge
    AddSipLine("Call-ID: %010u@%s", regid, pMyIp);
    AddSipLine("CSeq: %u REGISTER", cseq);
    AddSipLine("Contact: <sip:%s@%s:%u;transport=udp>",
        pSipUser, pMyIp, iMyPort);
    AddSipLine("User-Agent: arduino-sip/0.1");

    // If iAuthCnt > 0, process challenge from 401
    if (challenge != nullptr && iAuthCnt > 0) {
        // Parse out realm, nonce, opaque, qop from the challenge text
        bool ok = ParseParameter(realm, sizeof(realm), "realm=\"", challenge, '"') &&
            ParseParameter(nonce, sizeof(nonce), "nonce=\"", challenge, '"') &&
            ParseParameter(opaque, sizeof(opaque), "opaque=\"", challenge, '"') &&
            ParseParameter(qop, sizeof(qop), "qop=\"", challenge, '"');
        if (!ok) {
            // Malformed challenge: give up
            return false;
        }
        char nc[9], cnonce[17];                                     //Build nc and cnonce
        snprintf(nc, sizeof(nc), "%08X", iAuthCnt + 1);
        snprintf(cnonce, sizeof(cnonce), "%08X", (uint32_t)millis());

        MD5Builder md5;                                             // HA1 = MD5(user:realm:pass)
        md5.begin(); md5.add(pSipUser); md5.add(":"); md5.add(realm);
        md5.add(":"); md5.add(pSipPassWd); md5.calculate();
        char ha1[33]; strcpy(ha1, md5.toString().c_str());

        // HA2 = MD5("REGISTER:sip:user@host")
        md5.begin(); md5.add("REGISTER:sip:"); md5.add(pSipUser);
        md5.add("@"); md5.add(pSipIp); md5.calculate();
        char ha2[33]; strcpy(ha2, md5.toString().c_str());

        // response = MD5(HA1:nonce:nc:cnonce:qop:HA2)
        md5.begin();
        md5.add(ha1); md5.add(":"); md5.add(nonce);
        md5.add(":"); md5.add(nc);
        md5.add(":"); md5.add(cnonce);
        md5.add(":"); md5.add(qop);
        md5.add(":"); md5.add(ha2);
        md5.calculate();
        char resp[33]; strcpy(resp, md5.toString().c_str());

        // Add Authorization header
        AddSipLine(
            "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"sip:%s@%s\","
            "response=\"%s\", algorithm=MD5, opaque=\"%s\", qop=\"%s\", nc=%s, cnonce=\"%s\"",
            pSipUser, realm, nonce,
            pSipUser, pSipIp,
            resp, opaque, qop,
            nc, cnonce
        );
        iAuthCnt++;
    }
    // End of headers. Content-Length = 0
    AddSipLine("Expires: 3600");         // optional: tell server to keep registration 1 hour
    AddSipLine("Content-Length: 0");
    AddSipLine("");                      // blank line
    SendUdp();

    // Clear any previous challenge‐data so that new 401 will re‐populate it
    caRead[0] = '\0';
    return true;
}

bool Sip::Dial(const char *DialNr, const char *DialDesc, const char* sdpPtr, size_t sdpLength) {
  
  if ( iRingTime )
    return false;

  pDialNr = DialNr;
  pDialDesc = DialDesc;
  sdpBody = sdpPtr;
  sdpLen = sdpLength;
  iDialRetries = 0;
  Invite();
  iDialRetries++;
  iRingTime = Millis();

  return true;
}


void Sip::Processing(char* readBuf, size_t bufLen) {
    int packetSize = Udp.parsePacket();
    if (packetSize > 0) {
        // read into buffer and null-terminate
        int len = Udp.read(readBuf, bufLen - 1);
        if (len > 0) {
          readBuf[len] = '\0';
         //Serial.printf("[SIP Rx %d bytes]\n", len);            // Lines added for debug
         //Serial.println(readBuf);                              // Lines added for debug
          HandleUdpPacket(readBuf);
        }
    }
    // handle initial retransmits (no auth yet)
    if (!caRead[0] && iAuthCnt == 0 && iDialRetries < 5) {
        unsigned long elapsed = millis() - iRingTime;
        if (elapsed > (iDialRetries * 200)) {
            iDialRetries++;
            delay(30);
            Invite();
        }
    }
    return;
}


void Sip::HandleUdpPacket(const char *p) {
  
    if (strstr(p, "OPTIONS sip:1009@")) {     // This reply to the keep alive needs to be at the top of the stack
        ParseReturnParams(p);
        // build minimal 200 response
        pbuf[0] = '\0';
        AddSipLine("SIP/2.0 200 OK");
        AddCopySipLine(p, "Via: ");
        AddCopySipLine(p, "To: ");
        AddCopySipLine(p, "From: ");
        AddCopySipLine(p, "Call-ID: ");
        AddCopySipLine(p, "CSeq: ");
        AddSipLine("Content-Length: 0");
        AddSipLine("");
        SendUdp();
    }
    
    uint32_t iWorkTime = iRingTime ? (Millis() - iRingTime) : 0;
  
  if ( iRingTime && iWorkTime > iMaxTime )
  {
    // Cancel(3);
    //Bye(3);
    iRingTime = 0;
  }

  if ( !p )
  {
    // max 5 dial retry when loos first invite packet
    if ( iAuthCnt == 0 && iDialRetries < 5 && iWorkTime > (iDialRetries * 200) )
    {
      iDialRetries++;
      delay(30);
      Invite();
    }
	
    return;
  }


  
  if (strstr(p, "INVITE sip:100")) {       //Auto accept INVITE and move to that call
      AnswerInvite(p);
      /*if (isInCall) {
          Bye(iLastCSeq);
          isInCall = false;
          iRingTime = 0;
      } */
      iLastCSeq = GrepInteger(p, "\nCSeq: ");
      return;
  }

  if ( strstr(p, "SIP/2.0 401 Unauthorized"))
  {
      if (strstr(p, "CSeq:") && strstr(p, "REGISTER")) { Register(p); return; }
     //Serial.println(">>> Got 401 Unauthorized!");               // Serial Print Debug lines
     //Serial.println(">>> Challenge before Ack():");
     //Serial.println(p);
      
     Ack(p);
     //Serial.println(">>> Challenge after Ack():");
     //Serial.println(p);
     // call Invite with response data (p) to build auth md5 hashes
     Invite(p);
     return;
  }

  else if ( strstr(p, "SIP/2.0 200 OK") && strstr(p, "CSeq:"))		// OK
  {
    isInCall = true;
    Serial.println(">>> Got 200 OK for our INVITE — sending ACK");
    ParseReturnParams(p);
    remoteRtpPort = parseRemoteRtpPort(p);
    Serial.printf(">>> remoteRtpPort = %u\n", remoteRtpPort);
    Ack(p);
    return;
  }
  else if (    strstr(p, "SIP/2.0 183 ") 	// Session Progress
            || strstr(p, "SIP/2.0 100 ")	// Trying
            || strstr(p, "SIP/2.0 180 "))	// Ringing
  {
    ParseReturnParams(p);
  }

  else if (    strstr(p, "SIP/2.0 486 ")	// Busy Here
            || strstr(p, "SIP/2.0 603 ") 	// Decline
            || strstr(p, "SIP/2.0 487 ")) 	// Request Terminatet
  {
    Ack(p);
    iRingTime = 0;
  }
  else if (strstr(p, "INFO"))
  {
    iLastCSeq = GrepInteger(p, "\nCSeq: ");
    Ok(p);
  }

  else if (strstr(p, "BYE"))
  {
      Ack(p);
      isInCall = false;
      iRingTime = 0;
  }

}


void Sip::AddSipLine(const char* constFormat , ... ) {
  
  va_list arglist;
  va_start(arglist, constFormat);
  uint16_t l = (uint16_t)strlen(pbuf);
  char *p = pbuf + l;
  vsnprintf(p, lbuf - l, constFormat, arglist );
  va_end(arglist);
  l = (uint16_t)strlen(pbuf);
  if ( l < (lbuf - 2) )
  {
    pbuf[l] = '\r';
    pbuf[l + 1] = '\n';
    pbuf[l + 2] = 0;
  }
}


// Search a line in response date (p) and append on pbuf
bool Sip::AddCopySipLine(const char *p, const char *psearch) {

  char *pa = strstr((char*)p, psearch);

  if ( pa )
  {
    char *pe = strstr(pa, "\r");

    if ( pe == 0 )
      pe = strstr(pa, "\n");

    if ( pe > pa )
    {
      char c = *pe;
      *pe = 0;
      AddSipLine("%s", pa);
      *pe = c;
	  
      return true;
    }
  }
  
  return false;
}


// Parse parameter value from http formated string
bool Sip::ParseParameter(char *dest, int destlen, const char *name, const char *line, char cq) {

  const char *qp;
  const char *r;

  if ( ( r = strstr(line, name) ) != NULL )
  {
    r = r + strlen(name);
    qp = strchr(r, cq);
    int l = qp - r;
    if ( l < destlen )
    {
      strncpy(dest, r, l);
      dest[l] = 0;
	  
      return true;
    }
  }
  
  return false;
}


// Copy Call-ID, From, Via and To from response to caRead using later for BYE or CANCEL the call
bool Sip::ParseReturnParams(const char *p) {
  
  pbuf[0] = 0;
  
  AddCopySipLine(p, "Call-ID: ");
  AddCopySipLine(p, "From: ");
  AddCopySipLine(p, "Via: ");
  AddCopySipLine(p, "To: ");
  
  if ( strlen(pbuf) >= 2 )
  {
    strcpy(caRead, pbuf);
    caRead[strlen(caRead) - 2] = 0;
  }
  
  return true;
}


int Sip::GrepInteger(const char *p, const char *psearch) {

  int param = -1;
  const char *pc = strstr(p, psearch);

  if ( pc )
  {
    param = atoi(pc + strlen(psearch));
  }
  
  return param;
}

// Extract the port number from the first "m=audio <port>" line
uint16_t Sip::parseRemoteRtpPort(const char* p) {
    Serial.println(">>> Full SDP payload:");
    Serial.println(p);

    // extract the port
    uint16_t port = 0;
    const char* m = strstr(p, "m=audio ");
    if (m) {
        m += strlen("m=audio ");
        port = atoi(m);
    }
    Serial.printf(">>> parsed remoteRtpPort = %u\n", port);
    return port;
}


void Sip::Ack(const char* p) {
    //Serial.println(">>> Sip::Ack(): has been reached");
    //Serial.println(">>> Ack() got this packet:");
    //Serial.println(p);   // ← dump the full SIP text
    if (strstr(p, "SIP/2.0 200 OK") && strstr(p, "INVITE")) {
        isInCall = true;
    }
    char uri[64] = { 0 };
    if (!ParseParameter(uri, sizeof(uri), "To: <", p, '>'))
        return;

    char tag[64] = { 0 };
    ParseParameter(tag, sizeof(tag), "tag=", p, '\r');
    pbuf[0] = 0;

    AddSipLine("ACK %s SIP/2.0", uri);         // request‐line
    AddCopySipLine(p, "Call-ID: ");
    int cseq = GrepInteger(p, "\nCSeq: ");
    AddSipLine("CSeq: %i ACK", cseq);
    AddCopySipLine(p, "From: ");
    AddCopySipLine(p, "Via: ");

    if (tag[0]) { AddSipLine("To: <%s>;tag=%s", uri, tag); }
    else {
        // fallback if no tag was found
        AddCopySipLine(p, "To: ");}
    AddSipLine("Content-Length: 0");
    AddSipLine("");            // blank line
    SendUdp();                 // kick it onto the wire
}



void Sip::Cancel(int cseq) {
  
  if ( caRead[0] == 0 )
    return;

  pbuf[0] = 0;
  AddSipLine("%s sip:%s@%s SIP/2.0",  "CANCEL", pDialNr, pSipIp);
  AddSipLine("%s",  caRead);
  AddSipLine("CSeq: %i %s",  cseq, "CANCEL");
  AddSipLine("Max-Forwards: 70");
  AddSipLine("User-Agent: sip-client/0.0.1");
  AddSipLine("Content-Length: 0");
  AddSipLine("");
  SendUdp();
}


void Sip::Bye(int cseq) {
  
  if ( caRead[0] == 0 )
    return;

  pbuf[0] = 0;
  AddSipLine("%s sip:%s@%s SIP/2.0",  "BYE", pDialNr, pSipIp);
  AddSipLine("%s",  caRead);
  AddSipLine("CSeq: %i %s", cseq, "BYE");
  AddSipLine("Max-Forwards: 70");
  AddSipLine("User-Agent: sip-client/0.0.1");
  AddSipLine("Content-Length: 0");
  AddSipLine("");
  SendUdp();
}


void Sip::Ok(const char *p) {
  
  pbuf[0] = 0;
  AddSipLine("SIP/2.0 200 OK");
  AddCopySipLine(p, "Call-ID: ");
  AddCopySipLine(p, "CSeq: ");
  AddCopySipLine(p, "From: ");
  AddCopySipLine(p, "Via: ");
  AddCopySipLine(p, "To: ");
  AddSipLine("Content-Length: 0");
  AddSipLine("");
  SendUdp();
}


// Call invite without or with the response from peer
void Sip::Invite(const char* challenge) {
    //Serial.println(">>> Challenge at top of Invite():");
    //Serial.println(challenge);
    // limit auth retries
    if (challenge && iAuthCnt > 3) return;

    // parse challenge parameters if present
    char realm[128] = { 0 }, nonce[128] = { 0 };
    char opaque[128] = { 0 }, qop[32] = { 0 };
    uint16_t cseq = challenge ? 2 : 1;

    if (challenge) {
        //Serial.println("Raw 401 payload:");
        //Serial.println(challenge);
        // pull out realm, nonce, opaque, qop
        bool ok =
            ParseParameter(realm, sizeof(realm), "realm=\"", challenge) &&
            ParseParameter(nonce, sizeof(nonce), "nonce=\"", challenge) &&
            ParseParameter(opaque, sizeof(opaque), "opaque=\"", challenge) &&
            ParseParameter(qop, sizeof(qop), "qop=\"", challenge);
        //Serial.printf("parse ok=%d → realm=\"%s\", nonce=\"%s\", opaque=\"%s\", qop=\"%s\"\n", ok, realm, nonce, opaque, qop);
        if (!ok) {
            //Serial.println("Malformed challenge, giving up.");
            return;
        }
    }
    else {
        // first INVITE, generate new call IDs
        if (iDialRetries == 0) {
            callid = Random();
            tagid = Random();
            branchid = Random();
        }
        iAuthCnt = 0;
    }

    // start with empty buffer
    pbuf[0] = '\0';

    // standard INVITE headers
    AddSipLine("INVITE sip:%s@%s SIP/2.0", pDialNr, pSipIp);
    AddSipLine("Call-ID: %010u@%s", callid, pMyIp);
    AddSipLine("CSeq: %u INVITE", cseq);
    AddSipLine("Max-Forwards: 70");
    AddSipLine("From: \"%s\" <sip:%s@%s>;tag=%010u", pDialDesc, pSipUser, pSipIp, tagid);
    AddSipLine("Via: SIP/2.0/UDP %s:%u;branch=%010u;rport=%u", pMyIp, iMyPort, branchid, iMyPort);
    AddSipLine("To: <sip:%s@%s>", pDialNr, pSipIp);
    AddSipLine("Contact: \"%s\" <sip:%s@%s:%u;transport=udp>", pSipUser, pSipUser, pMyIp, iMyPort);

    // add Authorization if required
    if (challenge) {
        // build nc and cnonce
        char nc[9], cnonce[17];
        snprintf(nc, sizeof(nc), "%08X", iAuthCnt + 1);
        snprintf(cnonce, sizeof(cnonce), "%08X", (uint32_t)millis());

        MD5Builder md5;
        // HA1 = MD5(user:realm:pass)
        md5.begin(); md5.add(pSipUser); md5.add(":"); md5.add(realm);
        md5.add(":"); md5.add(pSipPassWd); md5.calculate();
        char ha1[33]; strcpy(ha1, md5.toString().c_str());

        // HA2 = MD5("INVITE:sip:user@host")
        md5.begin(); md5.add("INVITE:"); md5.add("sip:"); md5.add(pDialNr);
        md5.add("@");     md5.add(pSipIp); md5.calculate();
        char ha2[33]; strcpy(ha2, md5.toString().c_str());

        // response = MD5(HA1:nonce:nc:cnonce:qop:HA2)
        md5.begin();
        md5.add(ha1); md5.add(":"); md5.add(nonce);
        md5.add(":"); md5.add(nc);
        md5.add(":"); md5.add(cnonce);
        md5.add(":"); md5.add(qop);
        md5.add(":"); md5.add(ha2);
        md5.calculate();
        char resp[33]; strcpy(resp, md5.toString().c_str());

        AddSipLine(
            "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"sip:%s@%s\","
            " response=\"%s\", algorithm=MD5, opaque=\"%s\", qop=\"%s\", nc=%s, cnonce=\"%s\"",
            pSipUser, realm, nonce,
            pDialNr, pSipIp,
            resp, opaque, qop,
            nc, cnonce
        );
        iAuthCnt++;
    }

    // SDP headers & body
    AddSipLine("Content-Type: application/sdp");
    AddSipLine("Content-Length: %u", (unsigned)sdpLen);
    AddSipLine(""); // blank line
    if (sdpBody && sdpLen) {
        size_t used = strlen(pbuf);
        size_t space = lbuf - used - 1;
        size_t toCp = (sdpLen < space) ? sdpLen : space;
        memcpy(pbuf + used, sdpBody, toCp);
        pbuf[used + toCp] = '\0';
    }
    SendUdp();
    iLastCSeq = cseq;
}

//Helper function to allow Auto-Answer of invites
void Sip::AnswerInvite(const char* inviteMsg) {
    remoteRtpPort = parseRemoteRtpPort(inviteMsg);

    // Building custom Invite resopnse
    static char ourSdp[256];
    snprintf(ourSdp, sizeof(ourSdp),
        "v=0\r\n"
        "o=- 0 0 IN IP4 %s\r\n"
        "s=AutoAnswer\r\n"
        "c=IN IP4 %s\r\n"
        "t=0 0\r\n"
        "m=audio 5004 RTP/AVP 0\r\n"
        "a=rtpmap:0 PCMU/8000\r\n"
        "a=recvonly\r\n",
        pMyIp, pMyIp
    );
    char uri[64] = { 0 };
    if (!ParseParameter(uri, sizeof(uri), "To: <", inviteMsg, '>')) {
        return;
    }
    static char myToTag[32];
    snprintf(myToTag, sizeof(myToTag), "%08X", (unsigned)rand());
    int cseq = GrepInteger(inviteMsg, "\nCSeq: ");

    // 200 OK response
    pbuf[0] = '\0';
    AddSipLine("SIP/2.0 200 OK");
    AddCopySipLine(inviteMsg, "Via: ");
    AddSipLine("To: <%s>;tag=%s", uri, myToTag);
    AddCopySipLine(inviteMsg, "From: ");
    AddCopySipLine(inviteMsg, "Call-ID: ");
    AddSipLine("CSeq: %d INVITE", cseq);
    AddSipLine("Contact: <sip:%s@%s:%u;transport=udp>", pSipUser, pMyIp, iMyPort);
    AddSipLine("Content-Type: application/sdp");
    AddSipLine("Content-Length: %u", (unsigned)strlen(ourSdp));
    AddSipLine("");
    AddSipLine("%s", ourSdp);

    SendUdp();

    // Mark “in call,” so that your application will start RTP
    isInCall = true;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware dependent interface functions
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t Sip::Millis() {
	
  return (uint32_t)millis() + 1;
}


// Generate a 30 bit random number
uint32_t Sip::Random() {
	
  return ((((uint32_t)rand())&0x7fff)<<15) + ((((uint32_t)rand())&0x7fff));
  //return secureRandom(0x3fffffff);
}


int Sip::SendUdp() {
	
  Udp.beginPacket(pSipIp, iSipPort);
  Udp.write((const uint8_t*)pbuf, strlen(pbuf));
  Udp.endPacket();
  delay(10);
#ifdef DEBUGLOG
  Serial.printf("\r\n----- send %i bytes -----------------------\r\n%s", strlen(pbuf), pbuf);
  Serial.printf("------------------------------------------------\r\n");
#endif

  return 0;
}


void Sip::MakeMd5Digest(char *pOutHex33, char *pIn) {
  
  MD5Builder aMd5;
  
  aMd5.begin();
  aMd5.add(pIn);
  aMd5.calculate();
  aMd5.getChars(pOutHex33);
}
