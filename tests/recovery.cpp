#include <cassert>
#include <cstdio>
#include <cstdint>
#include "I2cRecovery.h"

struct Pins {
  static inline bool sdaLowOut=false,sclLowOut=false,sdaHeld=false,sclHeld=false;
  static inline unsigned clocks=0,stops=0,begins=0,ends=0,releaseAfter=0;
  static inline uint32_t now=0,stretchUntil=0;
  static inline bool stretch=false;
  static bool sdaHigh(){return !sdaLowOut && !sdaHeld;}
  static bool sclHigh(){return !sclLowOut && !sclHeld && (!stretch || int32_t(now-stretchUntil)>=0);}
  static void sdaLow(){assert(!sclHigh());sdaLowOut=true;}
  static void sclLow(){sclLowOut=true;}
  static void sdaRelease(){if(sdaLowOut && sclHigh())++stops;sdaLowOut=false;}
  static void sclRelease(){
    if(sclLowOut){++clocks;if(releaseAfter && clocks>=releaseAfter)sdaHeld=false;}
    sclLowOut=false;
  }
  static void end(){++ends;}
  static void begin(){assert(sdaHigh()&&sclHigh());++begins;}
  static void reset(){sdaLowOut=sclLowOut=sdaHeld=sclHeld=stretch=false;clocks=stops=begins=ends=releaseAfter=0;now=stretchUntil=0;}
};
using Bus=I2cRecovery<Pins>;
Bus::Result step(Bus& bus,uint32_t us=10){Pins::now+=us;return bus.tick(Pins::now);}
Bus::Result untilResult(Bus& bus){
  for(unsigned i=0;i<4000;++i){auto r=step(bus);if(r==Bus::RECOVERED||r==Bus::FAILED)return r;}
  assert(false);return Bus::FAILED;
}
int main(){
  Pins::reset();Bus idle;
  for(int i=0;i<1000;++i)assert(step(idle)==Bus::READY);
  assert(!Pins::begins&&!Pins::ends&&!Pins::clocks);
  Pins::sdaHeld=true;assert(step(idle)==Bus::BLOCKED);
  Pins::sdaHeld=false;assert(step(idle)==Bus::READY);assert(!Pins::ends);
  for(unsigned n=1;n<=9;++n){
    Pins::reset();Bus bus;Pins::sdaHeld=true;Pins::releaseAfter=n;
    assert(untilResult(bus)==Bus::RECOVERED);
    assert(Pins::clocks==n+1 && Pins::stops==1 && Pins::begins==1 && Pins::ends==1);
    assert(!Pins::sdaLowOut&&!Pins::sclLowOut);assert(step(bus)==Bus::READY);
  }
  Pins::reset();Bus stuck;Pins::sdaHeld=true;
  assert(untilResult(stuck)==Bus::FAILED);assert(Pins::clocks==9&&!Pins::begins);
  for(int i=0;i<4900;++i)assert(step(stuck,1000)==Bus::BLOCKED);
  assert(Pins::clocks==9&&Pins::ends==1);
  Pins::sdaHeld=false;step(stuck,100001);assert(untilResult(stuck)==Bus::RECOVERED);
  Pins::reset();Bus clock;Pins::sclHeld=true;
  assert(untilResult(clock)==Bus::FAILED);assert(!Pins::clocks&&!Pins::begins);
  assert(!Pins::sdaLowOut&&!Pins::sclLowOut);
  Pins::sclHeld=false;step(clock,5000000);assert(untilResult(clock)==Bus::RECOVERED);
  Pins::reset();Bus stretching;Pins::sdaHeld=true;Pins::releaseAfter=4;
  Pins::stretch=true;Pins::stretchUntil=5000;
  assert(untilResult(stretching)==Bus::RECOVERED);
  Pins::reset();Bus wrap;Pins::now=UINT32_MAX-1500;Pins::sdaHeld=true;Pins::releaseAfter=9;
  assert(untilResult(wrap)==Bus::RECOVERED);
  // MCU resumes between every short GPIO step; a held line never busy-waits.
  puts("PASS: healthy bus untouched, transient glitch, release on clocks 1..9, STOP, stuck SDA/SCL, 5 s cooldown, stretching, timer wrap.");
}
