#include "FreematicsPlus.h"
#include "MqttSim5360Client.h"
#include "telemetry_config.h"
#include "telemetry.h"
void obdSetup(FreematicsESP32 sys,COBDSPI obd,)
 {
   delay(100);
	Serial.begin(115200L);
  sys.begin();
   if(obd.begin())
   {
     Serial.println("OBD Connessa");
     setState(STATE_OBD_READY);
   }

    if (client.begin(&sys))
    {
      Serial.println("CLIENT Connesso");
        setState(STATE_CLIENT);
    }
   if (client.setup(APN) && checkState(STATE_CLIENT))
      {
           Serial.println("APN Connesso");
       }

   if (client.open() && checkState(STATE_CLIENT))
    {
   if (client.connect(CLIENT_MQTT_ID, MQTT_URL))
     Serial.println("MQTT CONNESSO");

}
if(checkState(STATE_OBD_READY))
{
  obd.getVIN(vin, sizeof(vin));
}
  if (sys.gpsInit(GPS_SERIAL_BAUDRATE))
  {
    Serial.println("GPS CONNESSO");
    setState(STATE_GPS_READY);

  }
  else
  {
    Serial.println("GPS NON CONNESSO");
  }
   Serial.println("FINE SETUP");
}


String getValuesObd()
 {
   if(checkState(STATE_OBD_READY))
   {
   String res;
   int result;
 for (byte i = 0; i < sizeof(pids) / sizeof(pids[0]); i++)
  {
    byte pid = pids[i];
    if (obd.readPID(pid, result))
     {
       res.concat("$");
       res.concat(result);
     }
     else
     {
       res.concat("$");
       res.concat("-666");
     }

   }

  return res;
}
}
