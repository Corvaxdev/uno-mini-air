#pragma once
#include <stdint.h>

// All 1024 EEPROM bytes: 128 records, no frequently rewritten head pointer.
// Record: little-endian count[4], CRC16[2], format[1], commit marker[1].
// Count is monotonic and saturates, so boot needs no separate sequence number.
// Store provides read(address), update(address, value), ready().
template<class Store> class ViewJournal {
public:
  static const uint32_t INTERVAL = 900000UL;
  uint32_t total=0;
  uint32_t saved=0;

  void begin(uint32_t now) {
    total=saved=0;next=0;phase=0;at=now;
    uint8_t r[8];
    for(uint8_t slot=0;slot<128;++slot) {
      read(slot,r);
      if(valid(slot,r) && value(r)>saved) {
        total=saved=value(r);next=(slot+1)&127;
      }
    }
  }
  void increment() { if(total!=UINT32_MAX)++total; }

  // At most one EEPROM byte launch per call. Waits are cooperative.
  // Returns 1 after verified commit, -1 on verification failure, otherwise 0.
  int8_t tick(uint32_t now) {
    if(!Store::ready())return 0;
    if(!phase) {
      if(uint32_t(now-at)<INTERVAL)return 0;
      at=now;
      if(total==saved)return 0;
      for(uint8_t i=0;i<4;++i)record[i]=uint8_t(total>>(8*i));
      record[6]=0x31;record[7]=0xA5;
      uint16_t crc=checksum(next,record);
      record[4]=uint8_t(crc);record[5]=uint8_t(crc>>8);
      phase=1;
    }
    const uint16_t base=uint16_t(next)*8;
    if(phase==1)Store::update(base+7,0); // Invalidate before touching payload.
    else if(phase<9)Store::update(base+phase-2,record[phase-2]);
    else if(phase==9)Store::update(base+7,record[7]); // Publish last.
    else {
      uint8_t check[8];read(next,check);phase=0;
      if(!valid(next,check)||value(check)!=value(record))return -1;
      saved=value(record);next=(next+1)&127;return 1;
    }
    ++phase;return 0;
  }
private:
  uint32_t at=0;
  uint8_t record[8];
  uint8_t next=0,phase=0;
  static uint32_t value(const uint8_t* r) {
    return uint32_t(r[0])|(uint32_t(r[1])<<8)|(uint32_t(r[2])<<16)|(uint32_t(r[3])<<24);
  }
  static uint16_t checksum(uint8_t slot,const uint8_t* r) {
    uint16_t crc=0xFFFF;
    for(uint8_t i=0;i<6;++i) {
      crc^=i<4?r[i]:i==4?r[6]:slot;
      for(uint8_t bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xA001:0);
    }
    return crc;
  }
  static bool valid(uint8_t slot,const uint8_t* r) {
    return r[7]==0xA5 && r[6]==0x31 && checksum(slot,r)==(uint16_t(r[4])|(uint16_t(r[5])<<8));
  }
  static void read(uint8_t slot,uint8_t* r) {
    for(uint8_t i=0;i<8;++i)r[i]=Store::read(uint16_t(slot)*8+i);
  }
};
