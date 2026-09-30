#pragma once
#include <cstdint>
#include <cstddef>
struct TestWire {
  uint8_t bank[128][256]={};
  uint8_t address=0,reg=0,index=0;
  bool timeout=false,shortRead=false,nack=false,flag=false;
  unsigned calls=0;
  void clearWireTimeoutFlag(){flag=false;}
  bool getWireTimeoutFlag(){return flag;}
  void beginTransmission(uint8_t a){address=a;index=0;++calls;}
  void write(uint8_t b){if(!index)reg=b;else bank[address][reg]=b;++index;}
  uint8_t endTransmission(bool=true){return nack?2:0;}
  uint8_t requestFrom(uint8_t a,uint8_t n){address=a;index=0;flag=timeout;return shortRead?n-1:n;}
  int read(){return bank[address][uint8_t(reg+index++)];}
};
extern TestWire Wire;
