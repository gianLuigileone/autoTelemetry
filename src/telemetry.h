#include <FreematicsPlus.h>
#include <FreematicsBase.h>

class Telemetry
{
public:
  Telemetry() {};
  String getObdValues();
  String getDtcValues();
  String getGpsValues();
  String getTelemetry();
  void sendSerialTelemetry();
  void readConfiguration();
  void sendMqttTelemetry();
  bool init();
  void setSerialSampling(unsigned int sampling);
  void setMqttSampling(unsigned int sampling);
  void saveOnSd(const char *fileName);

private:
  void saveLastGpsData(String stringGps);
  String readLastGpsData();
};
void idleTask();
