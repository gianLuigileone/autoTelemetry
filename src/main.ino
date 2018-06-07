#include <telemetry.h>
#include <telemetry_config.h>
Telemetry telemetry;


void setup()
{
  Serial.begin(GPS_SERIAL_BAUDRATE);
  telemetry.init();

}
void loop()
{
     telemetry.sendMqttTelemetry();
     idleTask();
     telemetry.sendSerialTelemetry();
     idleTask();

}
