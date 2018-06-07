#include <telemetry.h>
#include <pid.h>
#include <MqttSim5360Client.h>
#include <state.h>

//setup
static String apn;
//connect
static String username;
static String password;
static String clientID;
static String url;
static String willtopic;
static String willmsg;
static int qOSwillTopic;
//publish
static String topic;
static int qOSTopic;
//subscribe
static String subtopic;
static int qOSsubtopic;
static String telemetryValue;
static String stringGps;
static String stringObd;
static unsigned long sampling_frequency = 10000; // frequanza di invio telemetria sul server in millisecondi
unsigned long starting_timer; // inizializza il timer per l'invio delle telemetria sul server
static unsigned long serial_sampling_frequency = 5000; // frequanza di invio telemetria sulla seriale
unsigned long serial_starting_timer;  // inizializza il timer per l'invio della telemetria sulla seriale
char vin[128];

GPS_DATA gd = {0};
//Oggetti
COBDSPI obd;
SDLib::SDClass sd;
SDLib::File file;
SDLib::File lastGpsDataFile;
SDLib::File confFile;
MqttSim5360Client sim;
FreematicsESP32 device;
State state;
void onReceive(char* top,char* payload);
void saveConfiguration(const char *fileName);
void Telemetry::readConfiguration(){
  if(state.checkState(STATE_SD_READY))
  {
    confFile = sd.open("config.txt",SD_FILE_READ);
    String s;
    int n=0;
    while (confFile.available()){
      s = confFile.readStringUntil('\r');
      Serial.println(s);
      n++;
      switch (n) {
        case 1:
          apn = s.substring(4);
          Serial.println(apn);
          break;
        case 2:
          username = s.substring(10);
          break;
        case 3:
          password = s.substring(10);
          break;
        case 4:
          clientID = s.substring(10);
          break;
        case 5:
          url = s.substring(5);
          break;
        case 6:
          willtopic = s.substring(11);
          break;
        case 7:
          willmsg = s.substring(9);
          break;
        case 8:
          qOSwillTopic = s.substring(14).toInt();
          break;
        case 9:
          topic = s.substring(7);
          break;
        case 10:
          qOSTopic = s.substring(10).toInt();
          break;
        case 11:
          subtopic = s.substring(10);
          break;
        case 12:
          qOSsubtopic = s.substring(13).toInt();
          break;
        case 13:
          sampling_frequency = s.substring(20).toInt();
          break;
        case 14:
          serial_sampling_frequency = s.substring(27).toInt();
          break;
        case 15:
          s.substring(5).toCharArray(vin, sizeof(vin));
          break;
      }
    }
    confFile.close();
  }
}

String Telemetry::getObdValues(){
  if(state.checkState(STATE_OBD_READY))
  {
    int result;
    stringObd="";
    for (byte i = 0; i < sizeof(pids) / sizeof(pids[0]); i++)
    {
      byte pid = pids[i];
      if (obd.readPID(pid, result))
      {
        stringObd.concat("$");
        stringObd.concat(result);
      }
      else
      {
        stringObd.concat("$");
        stringObd.concat("-666");
      }
    }
    return stringObd;
  }
}
String Telemetry::getDtcValues(){
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
String Telemetry::getGpsValues(){
    char tmp[32];
    device.gpsGetData(&gd);
    for(;;)
    {
      if(state.checkState(STATE_GPS_READY))
      {
        if(gd.date!=0)
        {
          sprintf(tmp,"%.4f$%.4f",(gd.lat/1000000.0),(gd.lng/1000000.0));
          stringGps="";
          stringGps.concat("$");
          stringGps.concat(gd.date); // date
          stringGps.concat("$");
          stringGps.concat(gd.time); //time
          stringGps.concat("$");
          stringGps.concat(tmp);
          stringGps.concat("$");
          stringGps.concat((float)gd.alt/100);
          saveLastGpsData(stringGps);
          return stringGps;
          break;
        }
        else
        {
          Serial.println("Gps can't connect to satellite");
          Serial.println(readLastGpsData());
          return readLastGpsData();
          break;
        }
      }
      else
      {
        stringGps="";
        stringGps.concat("$");
        stringGps.concat("404"); // date
        stringGps.concat("$");
        stringGps.concat("404"); //time
        stringGps.concat("$");
        stringGps.concat("404");
        stringGps.concat("$");
        stringGps.concat("404");
        return stringGps;
      }
   }
}
void Telemetry::saveLastGpsData(String stringGps){
  delay(100);
  lastGpsDataFile = sd.open("GpsDATA.txt", (SD_FILE_WRITE | SD_O_TRUNC));
  delay(100);
  lastGpsDataFile.println(stringGps);
  delay(100);
  lastGpsDataFile.flush();
  delay(100);
  lastGpsDataFile.close();
}
String Telemetry::readLastGpsData(){
  lastGpsDataFile = sd.open("GpsDATA.txt",SD_FILE_READ);
  return lastGpsDataFile.readString();
}
String Telemetry::getTelemetry(){
  for(;;)
  {
    if(state.checkState(STATE_OBD_READY))
    {
      telemetryValue="";
      telemetryValue.concat(vin);
      telemetryValue.concat(getGpsValues());
      telemetryValue.concat(getObdValues());
      return  telemetryValue;
    }
    else
    {
      Serial.println("Error: Device not working");
    }
  }
}
void Telemetry::sendSerialTelemetry(){
  if (state.checkState(STATE_SERIAL_CONNECTED)&&((millis() - serial_starting_timer) >= serial_sampling_frequency))
   {
    String s = getTelemetry();
    Serial.print("#");
    Serial.println(s);
    serial_starting_timer = millis();
  }
}
void Telemetry::sendMqttTelemetry(){
  if ((millis() - starting_timer) >= sampling_frequency)
  {
  if(state.checkState(STATE_CLIENT))
  {
    Serial.print("Topic:");Serial.println(topic);
    sim.publish(topic.c_str(),getTelemetry().c_str(),qOSTopic);
  }
  else if(state.checkState(STATE_SD_READY))
  {
    Serial.println("SD writing");
    saveOnSd("DATA.txt");
  }
  else
  {
    Serial.println("Data Storing is not possible");
  }
  starting_timer = millis();
}
}
void obdBegin(){
  if(!state.checkState(STATE_OBD_READY))
  {
    if(obd.begin())
    {
      char readVin[128];
      Serial.println("OBD OK");
      if(obd.getVIN(readVin, sizeof(readVin)))
      {
        Serial.println(readVin);
        state.setState(STATE_OBD_READY);
        if ((readVin != "") && (readVin != vin))
        {
          strcpy(vin,readVin);
          Serial.print("VIN:");
          Serial.println(vin);
          saveConfiguration("CONFIG.TXT");
        }
      }
      else
      {
        Serial.println("It's not possible to read VIN");
      }
    }
    else
    {
      Serial.println("OBD Not Working");
    }
  }
}
void gpsBegin(){
  if(!state.checkState(STATE_GPS_READY))
  {
    if(device.gpsInit(115200))
    {
     state.setState(STATE_GPS_READY);
     Serial.println("Gps ok");
    }
    else
    {
      Serial.println("Gps not working");
    }
  }
}
void simBegin(){
  if(!state.checkState(STATE_CLIENT))
  {
    if(sim.begin(&device))
      {
        if (sim.setup(apn.c_str()))
        {
          if(sim.open())
          {
            if(sim.connect(clientID.c_str(),url.c_str(),willtopic.c_str(),willmsg.c_str(),qOSwillTopic,username.c_str(),password.c_str()))
            {
              state.setState(STATE_CLIENT);
              Serial.println("CLIENT ok");
              delay(500);
              sim.subscribe(subtopic.c_str(),qOSsubtopic,onReceive);
            }
          }
        }
        else
        {
            Serial.println("impossibile settare l'apn");
            //config.apn=APN;
            sim.close();
            state.clearState(STATE_CLIENT);
            simBegin();

        }
    }
    else
    {
      Serial.println("DEVICE NON TROVATO");
    }
  }
}
void sdBegin(){
  if(!state.checkState(STATE_SD_READY))
  {
    if(sd.begin())
    {
      Serial.println("SD ok");
      state.setState(STATE_SD_READY);
      Serial.print("STATE_SD_READY");
    }
    else
    {
      Serial.println("SD not working");
    }
  }
}
bool Telemetry::init(){
  sdBegin();
  readConfiguration();
  delay(100);
  obdBegin();
  device.begin();
  simBegin();
  gpsBegin();

}
void Telemetry::saveOnSd(const char *fileName){
  String s = getTelemetry();
  file = sd.open(fileName, SD_FILE_WRITE);
  file.println(s);
  file.flush();
  file.close();
}

void saveConfiguration(const char *fileName){
  file = sd.open(fileName, (SD_FILE_WRITE | SD_O_TRUNC));
  delay(100);
  file.print("apn=");file.println(apn);
  delay(50);
  file.print("username=");file.println(username);
  delay(50);
  file.print("password=");file.println(password);
  delay(50);
  file.print("clientID=");file.println(clientID);
  delay(50);
  file.print("url=");file.println(url);
  delay(50);
  file.print("willtopic=");file.println(willtopic);
  delay(50);
  file.print("willmsg=");file.println(willmsg);
  delay(50);
  file.print("qOSwillTopic=");file.println(qOSwillTopic);
  delay(50);
  file.print("topic=");file.println(topic);
  delay(50);
  file.print("qOSTopic=");file.println(qOSTopic);
  delay(50);
  file.print("subtopic=");file.println(subtopic);
  delay(50);
  file.print("qOSsubtopic=");file.println(qOSsubtopic);
  delay(50);
  file.print("sampling_frequency=");file.println(sampling_frequency);
  delay(50);
  file.print("serial_sampling_frequency=");file.println(serial_sampling_frequency);
  delay(50);
  file.print("vin=");file.println(vin);
  delay(50);
  file.flush();
  file.close();
}

void idleTask(){
  sdBegin();
  gpsBegin();
  obdBegin();
  simBegin();
  String in;
  in="";
  while(Serial.available())
  {
    in=Serial.readStringUntil('\r');
    if(in.equals("start"))
     {
       state.setState(STATE_SERIAL_CONNECTED);
     }
     if(in.equals("stop"))
      {
        state.clearState(STATE_SERIAL_CONNECTED);

      }
      if(in.startsWith("s_sampling"))
      {
        int n=in.substring(11).toInt();

      }
      if(in.startsWith("topic"))
      {
        String s=in.substring(6);
        topic = s;
        sim.close();
        state.clearState(STATE_CLIENT);
        simBegin();
      }
      if(in.startsWith("apn"))
      {
        String s=in.substring(4);
        apn=s;
        sim.close();
        state.clearState(STATE_CLIENT);
        simBegin();
      }
      if(in.startsWith("username"))
      {
        String s=in.substring(9);
        username=s;
        sim.close();
        state.clearState(STATE_CLIENT);
        simBegin();
      }
      if(in.startsWith("password"))
      {
        String s=in.substring(9);
        password=s;
        sim.close();
        state.clearState(STATE_CLIENT);
        simBegin();
      }
      if(in.startsWith("url"))
      {
        String s=in.substring(4);
        url=s;
        sim.close();
        state.clearState(STATE_CLIENT);
        simBegin();
      }
      if(in.startsWith("client"))
      {
        String s=in.substring(7);
        clientID=s;
        sim.close();
        state.clearState(STATE_CLIENT);
        simBegin();
      }
      if(in.startsWith("qos"))
      {
        String s=in.substring(4);
        qOSTopic=s.toInt();
        sim.close();
        state.clearState(STATE_CLIENT);
        simBegin();
      }
      if(in.startsWith("subtopic"))
      {
        String s=in.substring(9);
        subtopic=s;
        sim.close();
        state.clearState(STATE_CLIENT);
        simBegin();
      }
      if(in.startsWith("qossubtopic"))
      {
        String s=in.substring(12);
        qOSsubtopic=s.toInt();
        sim.close();
        state.clearState(STATE_CLIENT);
        simBegin();
      }
  }
  sim.receive();
  delay(20);
}

void onReceive(char* top,char* payload) {
  char *p;
  if (strstr(top,subtopic.c_str())){
  p = strtok(payload,"=$\r\n");
  while (p != NULL)
  {
    if (strstr(p,"APN")){
      p = strtok(NULL,"=$\r\n");
	  apn = p;
    }
    if (strstr(p,"CLIENT_ID")){
      p = strtok(NULL,"=$\r\n");
	  clientID = p;
    }
    if (strstr(p,"WILLTOPIC")){
      p = strtok(NULL,"=$\r\n");
	  willtopic = p;
    }
    if (strstr(p,"WILLMSG")){
      p = strtok(NULL,"=$\r\n");
	  willmsg = p;
    }
    if (strstr(p,"MQTT_URL")){
      p = strtok(NULL,"=$\r\n");
	  url = p;
    }
    if (strstr(p,"USERNAME")){
      p = strtok(NULL,"=$\r\n");
	  username = p;
    }
    if (strstr(p,"PASSWORD")){
      p = strtok(NULL,"=$\r\n");
	  password = p;
    }
    if (strstr(p,"TOPIC")){
      p = strtok(NULL,"=$\r\n");
	  topic = p;
    }
    if (strstr(p,"SUBTOPIC")){
      p = strtok(NULL,"=$\r\n");
	  subtopic = p;
    }
	if (strstr(p,"QOSSUB")){
      p = strtok(NULL,"=$\r\n");
	  qOSsubtopic = atoi(p);
    }
	if (strstr(p,"QOSWILL")){
      p = strtok(NULL,"=$\r\n");
	  qOSwillTopic = atoi(p);
    }
	if (strstr(p,"QOSTOPIC")){
      p = strtok(NULL,"=$\r\n");
	  qOSTopic = atoi(p);
    }
  if (strstr(p,"SAMPLING")){
      p = strtok(NULL,"=$\r\n");
	  sampling_frequency = atoi(p);
    }
  if (strstr(p,"SERIAL_SAMPLING")){
      p = strtok(NULL,"=$\r\n");
	  serial_sampling_frequency = atoi(p);
    }
  if (strstr(p,"VIN")){
      p = strtok(NULL,"=$\r\n");
	    strcpy(vin,p);
    }
    p = strtok(NULL,"=$\r\n");
  }
  saveConfiguration("CONFIG.TXT");
}
}
