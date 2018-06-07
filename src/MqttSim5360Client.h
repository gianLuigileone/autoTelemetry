#include <FreematicsBase.h>
#define MQTT_CALLBACK_SIGNATURE void (*callback)(char* topic,char* payload)

typedef void (*CallbackFunction) ();
class MqttSim5360Client
{
public:
    bool begin(CFreematics* device);
    void end();
    bool setup(const char* apn, bool only3G = false, bool roaming = false, unsigned int timeout = 60000);
    String getIP();
    int getSignal();
    String getOperatorName();
    bool open(unsigned int timeout = 3000);
    void close();
    bool connect(const char* clientID, const char* url,const char* willtopic, const char* willmsg, int qOSwillTopic, const char* username, const char* password);
    bool publish(const char* topic, const char* message, int qOSTopic);
    bool subscribe(const char* subtopic, int qOSubtopic, MQTT_CALLBACK_SIGNATURE);
	MqttSim5360Client& setCallback(MQTT_CALLBACK_SIGNATURE);
    void receive();
    const char* deviceName() { return "SIM5360"; }
protected:
    // send command and check for expected response
    bool sendCommand(const char* cmd, unsigned int timeout = 1000, const char* expected = "\r\nOK", bool terminated = false);
    bool returnValue = false;
    char m_buffer[384] = {0};
	   uint8_t m_stage = 0;
    CFreematics* m_device = 0;
private:
  MQTT_CALLBACK_SIGNATURE;
};
