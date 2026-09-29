#pragma once
#include <SPI.h>
#include <MiniW5500.h>

// Small server-only W5500 adapter over the pinned, proven Mode-3 register driver.
// No EthernetClient/Server vtables: outgoing DNS/UDP/Client code stays unlinked.
// Every call performs bounded SPI work. SEND completion is polled by the loop.
struct AirSocket {
  static uint8_t status(uint8_t s) {
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    uint8_t v=W5100.readSnSR(s);SPI.endTransaction();return v;
  }
  static bool command(uint8_t s,SockCMD c) {
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    bool ready=!W5100.readSnCR(s);
    if(ready)W5100.writeSnCR(s,c);
    SPI.endTransaction();return ready;
  }
  static bool open(uint8_t s) {
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    bool ready=!W5100.readSnCR(s);
    if(ready) {
      W5100.writeSnMR(s,SnMR::TCP|SnMR::ND);
      W5100.writeSnPORT(s,80);W5100.writeSnIR(s,0xFF);
      W5100.writeSnCR(s,Sock_OPEN);
    }
    SPI.endTransaction();return ready;
  }
  // W5500 16-bit counters may change during SPI. Retry a bounded four times.
  static uint16_t size(uint8_t s,bool tx) {
    uint16_t previous=tx?W5100.readSnTX_FSR(s):W5100.readSnRX_RSR(s);
    for(uint8_t i=0;i<4;++i) {
      uint16_t next=tx?W5100.readSnTX_FSR(s):W5100.readSnRX_RSR(s);
      if(next==previous)return next;previous=next;
    }
    return 0; // Retry next loop; never use an inconsistent counter.
  }
  static uint16_t read(uint8_t s,uint8_t* data,uint16_t capacity) {
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    uint16_t n=0;
    if(!W5100.readSnCR(s)) {
      // Sample once: Arduino min() would evaluate a changing RX count twice.
      n=size(s,false);if(n>capacity)n=capacity;
      if(n) {
        uint16_t p=W5100.readSnRX_RD(s);
        W5100.read(W5100.RBASE(s)+(p&W5100.SMASK),data,n);
        W5100.writeSnRX_RD(s,p+n);W5100.writeSnCR(s,Sock_RECV);
      }
    }
    SPI.endTransaction();return n;
  }
  static bool send(uint8_t s,const uint8_t* data,uint16_t n) {
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    bool ready=!W5100.readSnCR(s) && size(s,true)>=n;
    if(ready) {
      uint16_t p=W5100.readSnTX_WR(s);
      W5100.write(W5100.SBASE(s)+(p&W5100.SMASK),data,n);
      W5100.writeSnTX_WR(s,p+n);
      W5100.writeSnIR(s,SnIR::SEND_OK|SnIR::TIMEOUT);
      W5100.writeSnCR(s,Sock_SEND);
    }
    SPI.endTransaction();return ready;
  }
  // 1 transmitted, 0 pending, -1 failed. Final ACK is checked by drained().
  static int8_t sent(uint8_t s) {
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    uint8_t flags=W5100.readSnIR(s);int8_t result=0;
    if(flags&SnIR::TIMEOUT)result=-1;
    else if(flags&SnIR::SEND_OK) {
      if(W5100.readSnTX_RD(s)==W5100.readSnTX_WR(s)) {
        W5100.writeSnIR(s,SnIR::SEND_OK);result=1;
      } else if(!W5100.readSnCR(s)) {
        // The WIZnet TCP guide requires retrying a partial SEND without recopying.
        W5100.writeSnIR(s,SnIR::SEND_OK);W5100.writeSnCR(s,Sock_SEND);
      }
    }
    SPI.endTransaction();return result;
  }
  static bool drained(uint8_t s) {
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    // TX_FSR uses the internal ACK pointer; TX_RD alone only means transmitted.
    bool acked=!W5100.readSnCR(s) && size(s,true)==W5100.SSIZE;
    SPI.endTransaction();return acked;
  }
};
