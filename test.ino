#include "telemetry.h"

void setup()
{
  obdSetup();
}

void loop()
{
  if(checkState(STATE_OBD_READY)&&checkState(STATE_GPS_READY)&&checkState(STATE_CLIENT))
  {
    sendMqttTelemetry();
  }
if(checkState(STATE_GPS_READY)&&checkState(STATE_OBD_READY)&&checkState(STATE_SERIAL_CONNECTED))
{
  sendSerialTelemetry();
}
idleTask();
}
