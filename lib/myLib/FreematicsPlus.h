/*************************************************************************
* Arduino library for Freematics ONE+ (ESP32)
* Distributed under BSD license
* Visit http://freematics.com for more information
* (C)2017 Developed by Stanley Huang <support@freematics.com.au>
*************************************************************************/

#include <Arduino.h>
#include "esp_system.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_spi_flash.h"
#include "FreematicsBase.h"
#include "FreematicsNetwork.h"
#include "FreematicsMEMS.h"
#include "FreematicsDMP.h"
#include "FreematicsOBD.h"
#include "FreematicsSD.h"

#define PIN_XBEE_PWR 27
#define PIN_GPS_POWER 15
#define PIN_LED 4
#define PIN_SD_CS 5

#define BEE_UART_PIN_RXD  (16)
#define BEE_UART_PIN_TXD  (17)
#define BEE_UART_NUM UART_NUM_1

#define GPS_UART_PIN_RXD  (32)
#define GPS_UART_PIN_TXD  (33)
#define GPS_UART_NUM UART_NUM_2

#define UART_BUF_SIZE (256)

#define GPS_TIMEOUT 1000 /* ms */

typedef struct {
    uint32_t date;
    uint32_t time;
    int32_t lat;
    int32_t lng;
    int16_t alt;
    uint8_t speed;
    uint8_t sat;
    int16_t heading;
} GPS_DATA;

bool gps_decode_start();
void gps_decode_stop();
int gps_get_data(GPS_DATA* gdata);
int gps_write_string(const char* string);
void gps_decode_task(int timeout);
void bee_start();
int bee_write_string(const char* string);
int bee_write_data(uint8_t* data, int len);
int bee_read(uint8_t* buffer, size_t bufsize, unsigned int timeout);
void bee_flush();
uint8_t readChiTemperature();
int32_t readChipHallSensor();
uint16_t getFlashSize(); /* KB */

class Task
{
public:
    Task():xHandle(0) {}
    bool create(void (*task)(void*), const char* name, int priority = 0);
    void destroy();
    void suspend();
    void resume();
    bool running();
    void sleep(uint32_t ms);
private:
	void* xHandle = 0;
};

class Mutex
{
public:
  Mutex();
  void lock();
  void unlock();
private:
  void* xSemaphore;
};

class FreematicsESP32 : public CFreematics
{
public:
    void begin(int cpuMHz = CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ);
    // initialize GPS (set baudrate to 0 to power off GPS)
    bool gpsInit(unsigned long baudrate = 115200L);
    // get parsed GPS data (returns the number of data parsed since last invoke)
    int gpsGetData(GPS_DATA* gdata);
    // send command string to GPS
    void gpsSendCommand(const char* cmd);
	// start xBee UART communication
	bool xbBegin(unsigned long baudrate = 115200L);
	// read data to xBee UART
	int xbRead(char* buffer, int bufsize, unsigned int timeout = 1000);
	// send data to xBee UART
	void xbWrite(const char* cmd);
    // send data to xBee UART
	void xbWrite(const char* data, int len);
	// receive data from xBee UART (returns 0/1/2)
	int xbReceive(char* buffer, int bufsize, unsigned int timeout = 1000, const char** expected = 0, byte expectedCount = 0);
	// purge xBee UART buffer
	void xbPurge();
	// toggle xBee module power
	void xbTogglePower();
};

class CStorageNull;

class CStorageNull {
public:
    virtual bool init() { return true; }
    virtual void uninit() {}
    virtual bool begin() { return true; }
    virtual void end() {}
    virtual void log(uint16_t pid, int16_t value)
    {
        char buf[16];
        byte len = sprintf_P(buf, PSTR("%X=%d"), pid, (int)value);
        dispatch(buf, len);
    }
    virtual void log(uint16_t pid, int32_t value)
    {
        char buf[20];
        byte len = sprintf_P(buf, PSTR("%X=%d"), pid, value);
        dispatch(buf, len);
    }
    virtual void log(uint16_t pid, uint32_t value)
    {
        char buf[20];
        byte len = sprintf_P(buf, PSTR("%X=%u"), pid, value);
        dispatch(buf, len);
    }
    virtual void log(uint16_t pid, int value1, int value2, int value3)
    {
        char buf[24];
        byte len = sprintf_P(buf, PSTR("%X=%d;%d;%d"), pid, value1, value2, value3);
        dispatch(buf, len);
    }
    virtual void logCoordinate(uint16_t pid, int32_t value)
    {
        char buf[24];
        byte len = sprintf_P(buf, PSTR("%X=%d.%06u"), pid, (int)(value / 1000000), abs(value) % 1000000);
        dispatch(buf, len);
    }
    virtual void timestamp(uint32_t ts)
    {
        m_dataTime = ts;
        if (m_next) m_next->timestamp(ts);
    }
    virtual void setForward(CStorageNull* next) { m_next = next; }
    virtual void purge() { m_samples = 0; }
    virtual uint16_t samples() { return m_samples; }
    virtual void dispatch(const char* buf, byte len)
    {
        // output data via serial
        Serial.write((uint8_t*)buf, len);
        Serial.write('$');
        m_samples++;
        if (m_next) m_next->dispatch(buf, len);
    }
protected:
    byte checksum(const char* data, int len)
    {
        byte sum = 0;
        for (int i = 0; i < len; i++) sum += data[i];
        return sum;
    }
    virtual void header(uint16_t feedid) {}
    virtual void tailer() {}
    uint32_t m_dataTime = 0;
    uint16_t m_samples = 0;
    CStorageNull* m_next = 0;
};

class CStorageRAM: public CStorageNull {
public:
    bool init(unsigned int cacheSize)
    {
      if (m_cacheSize != cacheSize) {
        uninit();
        m_cache = new char[m_cacheSize = cacheSize];
      }
      return true;
    }
    void uninit()
    {
        if (m_cache) {
            delete m_cache;
            m_cache = 0;
            m_cacheSize = 0;
        }
    }
    void purge() { m_cacheBytes = 0; m_samples = 0; }
    unsigned int length() { return m_cacheBytes; }
    char* buffer() { return m_cache; }
    void dispatch(const char* buf, byte len)
    {
        // reserve some space for checksum
        int remain = m_cacheSize - m_cacheBytes - len - 3;
        if (remain < 0) {
          // m_cache full
          return;
        }
        if (m_dataTime) {
          // log timestamp
          uint32_t ts = m_dataTime;
          m_dataTime = 0;
          log(0, ts);
        }
        // store data in m_cache
        memcpy(m_cache + m_cacheBytes, buf, len);
        m_cacheBytes += len;
        m_cache[m_cacheBytes++] = ',';
        m_samples++;
        if (m_next) m_next->dispatch(buf, len);
    }

    void header(uint16_t feedid)
    {
        m_cacheBytes = sprintf(m_cache, "%X#", (unsigned int)feedid);
    }
    void tailer()
    {
        if (m_cache[m_cacheBytes - 1] == ',') m_cacheBytes--;
        m_cacheBytes += sprintf(m_cache + m_cacheBytes, "*%X", (unsigned int)checksum(m_cache, m_cacheBytes));
    }
    void untailer()
    {
        char *p = strrchr(m_cache, '*');
        if (p) {
            *p = ',';
            m_cacheBytes = p + 1 - m_cache;
        }
    }
protected:
    unsigned int m_cacheSize = 0;
    unsigned int m_cacheBytes = 0;
    char* m_cache = 0;
};

class CStorageSD : public CStorageNull {
public:
    bool init();
    bool begin(uint32_t dateTime = 0);
    void end();
    void dispatch(const char* buf, byte len);
    uint32_t size();
private:
    SDClass SD;
    SDLib::File file;
    uint16_t m_sizeKB = 0;
};

#define MAX_BLE_MSG_LEN 160

class GATTServer : public Print
{
public:
  bool begin(const char* deviceName = 0);
  bool send(uint8_t* data, size_t len);
  size_t write(uint8_t c);
  virtual size_t onRequest(uint8_t* buffer, size_t len)
  {
    // being requested for data
    buffer[0] = 'O';
    buffer[1] = 'K';
    return 2;
  }
  virtual void onReceive(uint8_t* buffer, size_t len)
  {
    // data received is in buffer
  }
private:
  static bool initBLE();
  String sendbuf;
};
