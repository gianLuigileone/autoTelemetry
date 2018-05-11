#include "MqttSim5360Client.h"

bool MqttSim5360Client::begin(CFreematics* device)
 {
  m_device = device;
	if (m_stage == 0) {
    m_device->xbBegin(115200);
    m_stage = 1;
  }
  for (byte n = 0; n < 10; n++) {
    // try turning on module
    m_device->xbTogglePower();
    delay(2000);
    // discard any stale data
    m_device->xbPurge();
    for (byte m = 0; m < 5; m++) {
      if (sendCommand("AT\r")) {
        m_stage = 2;
        return true;
      }
    }
  }
  return false;
}

void MqttSim5360Client::end()
{
  if (m_stage == 2 || sendCommand("AT\r")) {
    m_device->xbTogglePower();
    m_stage = 1;
  }
}

bool MqttSim5360Client::setup(const char* apn, bool only3G, bool roaming, unsigned int timeout)
{
  uint32_t t = millis();
  bool success = false;
  sendCommand("ATE0\r");
  if (only3G) sendCommand("AT+CNMP=14\r"); // use WCDMA only
  do {
    do {
      Serial.print('.');
      delay(500);
      success = sendCommand("AT+CPSI?\r", 1000, "Online");
      if (success) {
        if (!strstr(m_buffer, "NO SERVICE"))
          break;
        success = false;
      } else {
        if (strstr(m_buffer, "Off")) break;
      }
    } while (millis() - t < timeout);
    if (!success) break;

    do {
      success = sendCommand("AT+CREG?\r", 5000, roaming ? "+CREG: 0,5" : "+CREG: 0,1");
    } while (!success && millis() - t < timeout);
    if (!success) break;

    do {
      success = sendCommand("AT+CGREG?\r",1000, roaming ? "+CGREG: 0,5" : "+CGREG: 0,1");
    } while (!success && millis() - t < timeout);
    if (!success) break;

    do {
      sprintf(m_buffer, "AT+CGSOCKCONT=1,\"IP\",\"%s\"\r", apn);
      success = sendCommand(m_buffer);
    } while (!success && millis() - t < timeout);
    if (!success) break;

    //sendCommand("AT+CSOCKAUTH=1,1,\"APN_PASSWORD\",\"APN_USERNAME\"\r");

    sendCommand("AT+CSOCKSETPN=1\r");
    sendCommand("AT+CIPMODE=0\r");
    sendCommand("AT+NETOPEN\r");
  } while(0);
  if (!success) Serial.println(m_buffer);
  return success;
}

String MqttSim5360Client::getIP()
{
  uint32_t t = millis();
  do {
    if (sendCommand("AT+IPADDR\r", 3000, "\r\nOK\r\n", true)) {
      char *p = strstr(m_buffer, "+IPADDR:");
      if (p) {
        char *ip = p + 9;
        if (*ip != '0') {
					char *q = strchr(ip, '\r');
					if (q) *q = 0;
          return ip;
        }
      }
    }
    delay(500);
  } while (millis() - t < 15000);
  return "";
}

int MqttSim5360Client::getSignal()
{
    if (sendCommand("AT+CSQ\r", 500)) {
        char *p = strchr(m_buffer, ':');
        if (p) {
          p += 2;
          int db = atoi(p) * 10;
          p = strchr(p, '.');
          if (p) db += *(p + 1) - '0';
          return db;
        }
    }
    return -1;
}

String MqttSim5360Client::getOperatorName()
{
    // display operator name
    if (sendCommand("AT+COPS?\r") == 1) {
        char *p = strstr(m_buffer, ",\"");
        if (p) {
            p += 2;
            char *s = strchr(p, '\"');
            if (s) *s = 0;
            return p;
        }
    }
    return "";
}

bool MqttSim5360Client::open(){

	return sendCommand("AT+CMQTTSTART\r", 3000);
}

void MqttSim5360Client::close(){

	sendCommand("AT+CMQTTSTOP\r");
}

bool MqttSim5360Client::connect(const char* clientID, const char* url)
{
	m_buffer[0] = '\0';
	sprintf(m_buffer, "AT+CMQTTACCQ=0,\"%s\"\r",clientID);
	bool returnValue = sendCommand(m_buffer);
      if(returnValue)
	  {

        do{
			m_buffer[0] = '\0';
			sprintf(m_buffer, "AT+CMQTTCONNECT=0,\"%s\",31,1,\"a\",\"a\"\r",url);
        returnValue = sendCommand(m_buffer);
		}while(!returnValue);
        if(returnValue)
		return true;
      }
      else {

        return false;
      }

	}

 bool MqttSim5360Client::send(const char* topic, const char* message) {
	 bool returnValue;
		m_buffer[0] = '\0';
		sprintf(m_buffer,"AT+CMQTTTOPIC=0,%d\r",strlen(topic));
        returnValue = sendCommand(m_buffer, 200, ">");
        if(returnValue)
			Serial.println(F("Prepared topic"));
        else Serial.println(F("Not Prepared topic"));
		sendCommand(topic);
        m_buffer[0] = '\0';


        sprintf(m_buffer, "AT+CMQTTPAYLOAD=0,%u\r",  strlen(message));
        returnValue = sendCommand(m_buffer, 200, ">");
		if(returnValue)
			Serial.println(F("Prepared payload"));
        else Serial.println(F("Not Prepared payload"));
        sendCommand(message);

          returnValue = sendCommand("AT+CMQTTPUB=0,1,60\r",200);
          if(returnValue)Serial.println(F("Sended"));
          else Serial.println(F("Not Sended"));
 }

 bool MqttSim5360Client::sendCommand(const char* cmd, unsigned int timeout, const char* expected, bool terminated)
{
  if (cmd) {
    m_device->xbWrite(cmd);
  }
  m_buffer[0] = 0;
  byte ret = m_device->xbReceive(m_buffer, sizeof(m_buffer), timeout, &expected, 1);
  if (ret) {
    if (terminated) {
      char *p = strstr(m_buffer, expected);
      if (p) *p = 0;
    }
    return true;
  } else {
    return false;
  }
}
