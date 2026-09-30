#pragma once
#include <Arduino.h>
#include <Wire.h>

// Bosch BST-BMP180-DS000-12, sections 3.3-3.5.
// Address 0x77, ID 0x55. BMP085/BMP180 share this protocol.
// No heap allocation. All compensation uses 32-bit integer arithmetic.
class MiniBMP180 {
public:
  int16_t calibration[11]={};
  int16_t temperature10=0;
  int32_t pressurePa=0;
  uint16_t rawTemperature=0;
  uint32_t sampleAt=0;
  bool valid=false;
  static bool compensate(const int16_t* c,uint16_t ut,uint32_t up,uint8_t oss,int16_t& t,int32_t& p) {
    if(oss>3 || up>(65536UL<<oss)-1)return false;
    for(uint8_t i=3;i<6;++i)if(!c[i] || uint16_t(c[i])==0xFFFF)return false;
    // The magnitude fits uint32_t even for corrupt 16-bit calibration words.
    // Round negative products down, matching Bosch's arithmetic right shift.
    int32_t delta=int32_t(ut)-uint16_t(c[5]);
    uint32_t product=uint32_t(delta<0?-delta:delta)*uint16_t(c[4]);
    int32_t x1=delta<0 ? -int32_t((product+32767UL)>>15) : int32_t(product>>15);
    int32_t divisor=x1+c[10];if(!divisor)return false;
    int32_t b5=x1+(int32_t(c[9])*2048L)/divisor;
    int32_t temp=(b5+8)>>4;if(temp < -400 || temp > 850)return false;
    int32_t b6=b5-4000;
    x1=(int32_t(c[7])*((b6*b6)>>12))>>11;
    int32_t x2=(int32_t(c[1])*b6)>>11;
    int32_t b3=((int32_t(c[0])*4+x1+x2)*(1L<<oss)+2)>>2;
    x1=(int32_t(c[2])*b6)>>13;
    x2=(int32_t(c[6])*((b6*b6)>>12))>>16;
    int32_t x3=((x1+x2)+2)>>2;
    if(x3 <= -32768 || int32_t(up)<b3)return false;
    uint32_t b4=(uint32_t(uint16_t(c[3]))*uint32_t(x3+32768))>>15;
    if(!b4)return false;
    uint32_t difference=uint32_t(int32_t(up)-b3), factor=50000UL>>oss;
    // floor(UINT32_MAX / factor), OSS 0..3. Explicit bounds avoid avr-gcc
    // rewriting an overflow idiom into costly 64-bit multiplication helpers.
    uint32_t limit=oss==0?85899UL:oss==1?171798UL:oss==2?343597UL:687194UL;
    if(difference>limit)return false;
    uint32_t b7=difference*factor;
    uint32_t result;
    if(b7<0x80000000UL)result=(b7*2)/b4;
    else {
      result=b7/b4;
      if(result>60000UL)return false; // Check before doubling or narrowing.
      result*=2;
    }
    // Reject corrupt results before the squared correction term.
    if(result<25000 || result>120000)return false;
    int32_t pressure=result;
    x1=((pressure>>8)*(pressure>>8)*3038L)>>16;
    x2=(-7357L*pressure)>>16;
    pressure+=(x1+x2+3791)>>4;
    if(pressure<30000 || pressure>110000)return false;
    t=temp;p=pressure;return true;
  }
  void invalidate() {valid=false;initialized=false;stage=0;}
  bool fresh() const { return valid && uint32_t(millis()-sampleAt)<15000UL; }
  bool tick() {
    uint32_t now=millis();if(int32_t(now-due)<0)return false;
    if(!stage) {
      cycleAt=now;
      if(!initialized && !identify()){fail();return false;}
      if(!writeReg(0xF4,0x2E)){fail();return false;}
      stage=1;due=millis()+5;return false;
    }
    uint8_t b[3];
    if(!readReg(0xF4,b,1) || (b[0]&0x20)){fail();return false;}
    if(stage==1) {
      if(!readReg(0xF6,b,2)){fail();return false;}
      rawTemperature=uint16_t(b[0])<<8|b[1];
      if(!writeReg(0xF4,0x34|(OSS<<6))){fail();return false;}
      stage=2;due=millis()+8;return false;
    }
    if(!readReg(0xF6,b,3)){fail();return false;}
    const uint32_t rawPressure=((uint32_t(b[0])<<16)|(uint16_t(b[1])<<8)|b[2])>>(8-OSS);
    int16_t t;int32_t p;
    if(!compensate(calibration,rawTemperature,rawPressure,OSS,t,p)){fail();return false;}
    temperature10=t;pressurePa=p;valid=true;sampleAt=millis();
    stage=0;due=cycleAt+5000UL;return true;
  }
private:
  static const uint8_t OSS=1;
  uint8_t stage=0;
  bool initialized=false;
  uint32_t due=0,cycleAt=0;
  bool readReg(uint8_t reg,uint8_t* b,uint8_t n) {
    Wire.clearWireTimeoutFlag();Wire.beginTransmission(0x77);Wire.write(reg);
    if(Wire.endTransmission(false))return false;
    if(Wire.requestFrom(uint8_t(0x77),n)!=n || Wire.getWireTimeoutFlag())return false;
    for(uint8_t i=0;i<n;++i)b[i]=Wire.read();return true;
  }
  bool writeReg(uint8_t reg,uint8_t value) {
    Wire.clearWireTimeoutFlag();Wire.beginTransmission(0x77);Wire.write(reg);Wire.write(value);
    return Wire.endTransmission()==0 && !Wire.getWireTimeoutFlag();
  }
  bool identify() {
    uint8_t b[22];if(!readReg(0xD0,b,1)||b[0]!=0x55||!readReg(0xAA,b,22))return false;
    for(uint8_t i=0;i<11;++i)calibration[i]=(uint16_t(b[i*2])<<8)|b[i*2+1];
    for(uint8_t i=3;i<6;++i)if(!calibration[i]||uint16_t(calibration[i])==0xFFFF)return false;
    initialized=true;return true;
  }
  void fail() {valid=false;initialized=false;stage=0;due=millis()+5000UL;}
};
