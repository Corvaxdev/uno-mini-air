#include <cassert>
#include <cstdio>
#include <cstring>
#include "MiniSHT.h"
#include "MiniDSL.h"
#include "MiniBMP180.h"
#include "MhZ14Pwm.h"
uint32_t testMillis=0;
TestWire Wire;
void next(){testMillis+=5000;}
void word(uint8_t a,uint8_t r,uint16_t v){Wire.bank[a][r]=v;Wire.bank[a][r+1]=v>>8;}
int main(int argc,char** argv){
  int16_t c[11]={408,-72,-14383,32741,32757,23153,6190,4,-32768,-8711,2868};
  int16_t t=0;int32_t p=0;
  if(argc>1 && !strcmp(argv[1],"corrupt")){
    // Both unsigned calibration words pass the current nonzero/non-FFFF filter.
    c[4]=int16_t(65534);c[5]=1;
    assert(!MiniBMP180::compensate(c,65535,23843,1,t,p));
    puts("PASS: corrupt calibration regression safely rejected.");return 0;
  }
  assert(MiniBMP180::compensate(c,27898,23843,0,t,p)&&t==150&&p==69964);
  c[3]=0;assert(!MiniBMP180::compensate(c,27898,23843,0,t,p));
  int16_t real[11]={8407,-1202,-14486,-31674,24975,19151,6515,48,-32768,-11786,2835};
  assert(MiniBMP180::compensate(real,28079,84381,1,t,p)&&t==269&&p==100279);
  MiniSHT sht;Wire.bank[0x20][4]=5;Wire.bank[0x20][7]=0x3c;
  word(0x20,0x11,263);word(0x20,0x13,475);
  assert(sht.tick()&&sht.fresh()&&sht.temperature10==263&&sht.humidity10==475);
  unsigned calls=Wire.calls;assert(!sht.tick()&&Wire.calls==calls);
  next();word(0x20,0x11,0x8000|123);assert(sht.tick()&&sht.temperature10==-123);
  next();word(0x20,0x13,1001);assert(!sht.tick()&&!sht.fresh());
  word(0x20,0x13,475);next();Wire.bank[0x20][4]=6;assert(!sht.tick()&&!sht.fresh());
  Wire.bank[0x20][4]=5;next();Wire.shortRead=true;assert(!sht.tick());Wire.shortRead=false;
  next();Wire.timeout=true;assert(!sht.tick());Wire.timeout=false;
  next();Wire.nack=true;assert(!sht.tick());Wire.nack=false;
  next();assert(sht.tick()&&sht.fresh());
  testMillis+=15000;assert(!sht.fresh());assert(sht.tick());
  // Advance less than INT32_MAX each time, including a millis rollover.
  for(int i=0;i<5;++i){testMillis+=0x40000000UL;assert(sht.tick()&&sht.fresh());}
  next();assert(sht.tick());
  testMillis=0;MiniDSL light;Wire.bank[0x49][4]=6;Wire.bank[0x49][7]=0x3c;
  word(0x49,0x11,0);assert(light.tick()&&light.fresh()&&light.lux==0);
  next();word(0x49,0x11,8191);assert(light.tick()&&light.lux==8191);
  next();word(0x49,0x11,8192);assert(!light.tick()&&!light.fresh());
  next();word(0x49,0x11,304);Wire.timeout=true;assert(!light.tick());Wire.timeout=false;
  next();Wire.shortRead=true;assert(!light.tick());Wire.shortRead=false;
  next();Wire.nack=true;assert(!light.tick());Wire.nack=false;
  next();Wire.bank[0x49][7]=0;assert(!light.tick());Wire.bank[0x49][7]=0x3c;
  next();assert(light.tick()&&light.fresh());
  testMillis+=15000;assert(!light.fresh());
  puts("PASS: Bosch reference and actual calibration vectors; zero divisor; SHT/DSL valid, boundary, NACK, short read, timeout, identity, stale data, recovery; SHT schedule rollover.");
}
