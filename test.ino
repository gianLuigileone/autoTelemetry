#include "telemetry.h"

void setup()
{
  obdSetup();
}

void loop()
{
  if(checkState(STATE_OBD_READY)&&checkState(STATE_GPS_READY)&&checkState(STATE_CLIENT))
  {
    Serial.print("valori gps:");
    Serial.println(getValuesGps());
    Serial.print("valori obd:");
    Serial.println(getValuesObd());
    sendMqttTelemetry();
  }
if(checkState(STATE_GPS_READY)&&checkState(STATE_OBD_READY)&&checkState(STATE_SERIAL_CONNECTED))
{
  sendSerialTelemetry();
}
idleTask();
}
