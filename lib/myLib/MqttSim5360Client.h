#include <FreematicsBase.h>

class MqttSim5360Client
{
public:
    bool begin(CFreematics* device);
    void end();
    bool setup(const char* apn, bool only3G = false, bool roaming = false, unsigned int timeout = 60000);
    String getIP();
    int getSignal();
    String getOperatorName();
    bool open();
    void close();
	bool connect(const char* clientID,const char* url);
    bool send(const char* topic, const char* message);
    const char* deviceName() { return "SIM5360"; }
protected:
    // send command and check for expected response
    bool sendCommand(const char* cmd, unsigned int timeout = 1000, const char* expected = "\r\nOK", bool terminated = false);

    char m_buffer[256] = {0};
	   uint8_t m_stage = 0;
    CFreematics* m_device = 0;
};
