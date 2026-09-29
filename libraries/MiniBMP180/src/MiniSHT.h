#pragma once
#include <Arduino.h>
#include <Wire.h>

// Trema Flash-I2C SHT protocol: iarduino_I2C_SHT by iarduino / tremaru.
// https://github.com/tremaru/iarduino_I2C_SHT
// Commissioned 0x20; model=5, family=0x3C. Runtime never changes configuration.
class MiniSHT {
public:
  int16_t temperature10=0;
  uint16_t humidity10=0;
  uint32_t samples=0,errors=0,sampleAt=0;
  void invalidate() {valid=false;}
  bool fresh() const {return valid && uint32_t(millis()-sampleAt)<15000UL;}
  bool tick() {
    uint32_t now=millis();if(int32_t(now-due)<0)return false;due=now+5000UL;
    uint8_t b[4];
    // Recheck identity every cycle: fail closed on address collision/replacement.
    if(!read(0x04,b,4)||b[0]!=5||b[3]!=0x3C||!read(0x11,b,4)){valid=false;++errors;return false;}
    int16_t t=(uint16_t(b[1]&0x7F)<<8)|b[0];if(b[1]&0x80)t=-t;
    uint16_t h=(uint16_t(b[3])<<8)|b[2];
    if(t < -400 || t > 1250 || h>1000){valid=false;++errors;return false;}
    temperature10=t;humidity10=h;sampleAt=millis();valid=true;++samples;return true;
  }
private:
  bool valid=false;
  uint32_t due=0;
  bool read(uint8_t reg,uint8_t* b,uint8_t n) {
    Wire.clearWireTimeoutFlag();Wire.beginTransmission(0x20);Wire.write(reg);
    if(Wire.endTransmission())return false;
    if(Wire.requestFrom(uint8_t(0x20),n)!=n || Wire.getWireTimeoutFlag())return false;
    for(uint8_t i=0;i<n;++i)b[i]=Wire.read();
    delayMicroseconds(500);return true; // Same inter-read gap as the vendor driver.
  }
};
