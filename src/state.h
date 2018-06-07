#include <Arduino.h>
#define STATE_SERIAL_CONNECTED 0x1
#define STATE_OBD_READY 0x2
#define STATE_GPS_READY 0x4
#define STATE_SD_READY 0x8
#define STATE_CLIENT 0x80
#define STATE_STANDBY 0x40

class State {

public:
  State() {}
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
  byte m_state = 0;
  byte getState()
  {
    return m_state;
  }
};
