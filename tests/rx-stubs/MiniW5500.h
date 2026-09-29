#pragma once
#include <cstdint>
#include <cstring>
// Match the pinned Arduino AVR core macro, including repeated evaluation.
#define min(a,b) ((a)<(b)?(a):(b))
enum SockCMD { Sock_CLOSE, Sock_OPEN, Sock_LISTEN, Sock_RECV, Sock_SEND };
struct SnMR { enum { TCP=1, ND=2 }; };
struct SnIR { enum { SEND_OK=1, TIMEOUT=2 }; };
struct ChipMock {
  uint16_t first=0, second=0, rx=0, copied=0;
  unsigned calls=0;
  bool busy=false, unstable=false;
  static constexpr uint16_t SMASK=2047, SSIZE=2048;
  uint8_t readSnCR(uint8_t) { return busy; }
  uint8_t readSnSR(uint8_t) { return 0; }
  uint8_t readSnIR(uint8_t) { return 0; }
  uint16_t readSnRX_RSR(uint8_t) { ++calls; return unstable?calls:calls<=2?first:second; }
  uint16_t readSnTX_FSR(uint8_t) { return SSIZE; }
  uint16_t readSnRX_RD(uint8_t) { return rx; }
  uint16_t readSnTX_RD(uint8_t) { return 0; }
  uint16_t readSnTX_WR(uint8_t) { return 0; }
  void writeSnRX_RD(uint8_t,uint16_t n) { rx=n; }
  void writeSnTX_WR(uint8_t,uint16_t) {}
  void writeSnCR(uint8_t,SockCMD) {}
  void writeSnMR(uint8_t,int) {}
  void writeSnPORT(uint8_t,int) {}
  void writeSnIR(uint8_t,int) {}
  uint16_t RBASE(uint8_t) { return 0; }
  uint16_t SBASE(uint8_t) { return 0; }
  void read(uint16_t,uint8_t* p,uint16_t n) { copied=n; std::memset(p,0x3A,n); }
  void write(uint16_t,const uint8_t*,uint16_t) {}
};
inline ChipMock W5100;
