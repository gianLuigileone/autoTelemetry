#include "FreematicsPlus.h"
#include "MqttSim5360Client.h"
#include "telemetry_config.h"

#define STATE_SERIAL_CONNECTED 0x1
#define STATE_OBD_READY 0x2
//#define STATE_GPS_FOUND 0x4
//#define STATE_GPS_READY 0x8
#define STATE_GPS_READY 0x4
//#define STATE_STORE_READY 0x10
#define STATE_SD_READY 0x8
#define STATE_CLIENT 0x80
//#define STATE_FILE_READY 0x20
#define STATE_STANDBY 0x40
//#define STATE_CLIENT 0x80
byte m_state = 0;
bool checkState(byte flags)
 {
    return (m_state & flags) == flags;
   }
void setState(byte flags)
 {
   m_state |= flags;
  }
void clearState(byte flags)
 {
    m_state &= ~flags;
   }

FreematicsESP32 sys;
MqttSim5360Client client;
COBDSPI obd;
GPS_DATA gd = {0};
char vin[128]; // Veicle Identifier Number
unsigned long sampling_frequency = 10000; // frequanza di invio telemetria sul server in millisecondi
unsigned long starting_timer; // inizializza il timer per l'invio delle telemetria sul server
unsigned long serial_sampling_frequency = 5000; // frequanza di invio telemetria sulla seriale
unsigned long serial_starting_timer;  // inizializza il timer per l'invio della telemetria sulla seriale

void setSamplingFrequency(unsigned long frequency) {
  sampling_frequency = frequency;
}

void setSerialSamplingFrequency(unsigned long frequency) {
  serial_sampling_frequency = frequency;
}
// array di byte contenete tutti i pid da inserire nella telemetria
const byte pids[] = { PID_ENGINE_LOAD, PID_COOLANT_TEMP,
                      PID_SHORT_TERM_FUEL_TRIM_1, PID_LONG_TERM_FUEL_TRIM_1,
                      PID_SHORT_TERM_FUEL_TRIM_2, PID_LONG_TERM_FUEL_TRIM_2,
                      PID_FUEL_PRESSURE, PID_INTAKE_MAP, PID_RPM, PID_SPEED,
                      PID_TIMING_ADVANCE, PID_INTAKE_TEMP, PID_MAF_FLOW,
                      PID_THROTTLE, PID_AUX_INPUT, PID_RUNTIME, PID_DISTANCE_WITH_MIL, PID_COMMANDED_EGR,
                      PID_EGR_ERROR, PID_COMMANDED_EVAPORATIVE_PURGE, PID_FUEL_LEVEL,
                      PID_WARMS_UPS, PID_DISTANCE, PID_EVAP_SYS_VAPOR_PRESSURE,
                      PID_BAROMETRIC,
                      PID_CATALYST_TEMP_B1S1, PID_CATALYST_TEMP_B2S1, PID_CATALYST_TEMP_B1S2,
                      PID_CATALYST_TEMP_B2S2, PID_CONTROL_MODULE_VOLTAGE,
                      PID_ABSOLUTE_ENGINE_LOAD, PID_AIR_FUEL_EQUIV_RATIO,
                      PID_RELATIVE_THROTTLE_POS, PID_AMBIENT_TEMP,
                      PID_ABSOLUTE_THROTTLE_POS_B,
                      PID_ABSOLUTE_THROTTLE_POS_C, PID_ACC_PEDAL_POS_D, PID_ACC_PEDAL_POS_E,
                      PID_ACC_PEDAL_POS_F, PID_COMMANDED_THROTTLE_ACTUATOR,
                      PID_TIME_WITH_MIL, PID_TIME_SINCE_CODES_CLEARED, PID_ETHANOL_FUEL,
                      PID_FUEL_RAIL_PRESSURE, PID_HYBRID_BATTERY_PERCENTAGE, PID_ENGINE_OIL_TEMP,
                      PID_FUEL_INJECTION_TIMING, PID_ENGINE_FUEL_RATE,
                      PID_ENGINE_TORQUE_DEMANDED,
                      PID_ENGINE_TORQUE_PERCENTAGE, PID_ENGINE_REF_TORQUE
                    };

String getValuesDtc()
{
  byte maxCodes = 1;
  uint16_t codes[8];
      byte dtcCount = obd.readDTC(codes,maxCodes);
      if (dtcCount == 0)
       {
          Serial.println("No DTC");
        }
        else
        {
          Serial.println("DTC");
          for (byte i = 0; i < maxCodes; i++)
          {
            Serial.println(codes[i],HEX);
          }
        }
      }

// metodo che restituisce una stringa con la telemetria del veicolo
String getValuesGps()
 {
   String res;
   char tmp[32];
   sys.gpsGetData(&gd);
   for(;;)
   {
     sys.gpsGetData(&gd);
     if(gd.date!=0)
     {
       Serial.println("DATI GPS READY");
       setState(STATE_GPS_READY);
       sprintf(tmp,"%.4f$%.4f",(gd.lat/1000000.0),(gd.lng/1000000.0));
       res.concat("$");
       res.concat(gd.date); // date
       res.concat("$");
       res.concat(gd.time); //time
       res.concat("$");
       res.concat(tmp);
       res.concat("$");
       res.concat((float)gd.alt/1000);
       return res;
       break;
     }
     else
     {
       Serial.print("ATTEMPT DATA GPS..");
       Serial.print(gd.date);
       delay(500);
        Serial.print(".");
     }

   }
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
String getTelemetry()
 {
   String telemetry;
   telemetry.concat(vin);

   telemetry.concat(getValuesGps());
   telemetry.concat(getValuesObd());
   return telemetry;
 }
// metodo per ascolare sulla seriale
void idleTask() {
  String in="";
  while (Serial.available()) {
    // get the new byte:
    in = Serial.readStringUntil('\r');
      if (in.equals("start"))

       {
         Serial.println("start serial");
        setState(STATE_SERIAL_CONNECTED); // setta il bit relativo allo stato ad uno
      }
      if (in.equals("stop")) {
        Serial.println("stop serial");
        clearState(STATE_SERIAL_CONNECTED); // setta il bit relativo allo stato a zero
      }
      if (in.startsWith("s_sampling")){
        int n = in.substring(11).toInt();
        setSerialSamplingFrequency(n);
      }
  }
  delay(20);
}



void sendSerialTelemetry() {
  if (checkState(STATE_SERIAL_CONNECTED)&&((millis() - serial_starting_timer) >= serial_sampling_frequency)) {
    String s = getTelemetry();
    Serial.print("#");
    Serial.println(s);
    serial_starting_timer = millis();
  }
}

// send Mqtt request every N seconds
void sendMqttTelemetry() {
  if ((millis() - starting_timer) >= sampling_frequency) {
    String s = getTelemetry();
  Serial.println(s);
  const char *str;
  str = s.c_str();
  delay(500);
  client.send(TOPIC_MEASURES, str);
  starting_timer = millis();
  }
}

void obdSetup()
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
