#pragma once
#include <Arduino.h>
#include <Wire.h>

// Trema Flash-I2C DSL, manufacturer getLux(): little-endian registers 0x11-0x12.
// The connected module was identified at 0x49, model 6, family 0x3C.
// Runtime only reads; address and smoothing settings are preserved.
class MiniDSL {
public:
  uint16_t lux=0;
  uint32_t samples=0,errors=0,sampleAt=0;
  void invalidate() {valid=false;}
  bool fresh() const {return valid && uint32_t(millis()-sampleAt)<15000UL;}
  bool tick() {
    uint32_t now=millis();if(int32_t(now-due)<0)return false;due=now+5000UL;
    uint8_t b[4];
    if(!read(0x04,b,4)||b[0]!=6||b[3]!=0x3C||!read(0x11,b,2)){valid=false;++errors;return false;}
    uint16_t value=(uint16_t(b[1])<<8)|b[0];
    if(value>8191){valid=false;++errors;return false;}
    lux=value;sampleAt=millis();valid=true;++samples;return true;
  }
private:
  bool valid=false;
  uint32_t due=0;
  bool read(uint8_t reg,uint8_t* b,uint8_t n) {
    Wire.clearWireTimeoutFlag();Wire.beginTransmission(0x49);Wire.write(reg);
    if(Wire.endTransmission())return false;
    if(Wire.requestFrom(uint8_t(0x49),n)!=n || Wire.getWireTimeoutFlag())return false;
    for(uint8_t i=0;i<n;++i)b[i]=Wire.read();
    delayMicroseconds(500);return true;
  }
};
