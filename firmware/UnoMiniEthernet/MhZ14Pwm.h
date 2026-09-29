#pragma once
#include <stdint.h>

// MH-Z14A 400-5000 ppm. Winsen v1.4: ppm = range*(TH-2ms)/(T-4ms).
// micros() on the 16 MHz AVR resolves 4 us. Divide durations by four before
// multiplication: the complete 5000 ppm range fits uint32_t, without float.
inline uint16_t mhZ14Decode(uint32_t highUs,uint32_t periodUs) {
  if(periodUs<953000UL || periodUs>1055000UL ||
     highUs<2000UL || highUs>periodUs-2000UL)return UINT16_MAX;
  uint32_t span=(periodUs-4000UL)/4;
  uint16_t ppm=(5000UL*((highUs-2000UL)/4)+span/2)/span;
  return ppm>=400 && ppm<=5000 ? ppm : UINT16_MAX;
}

// Called only from PCINT2. Publish a completed high + low cycle on rising edges.
// Startup in the middle of a pulse cannot publish a partial measurement.
struct MhZ14Capture {
  volatile uint32_t rise=0,fall=0,highUs=0,periodUs=0;
  volatile uint8_t sequence=0;
  uint8_t phase=0;
  void edge(bool high,uint32_t now) {
    if(high) {
      if(phase==2) {highUs=fall-rise;periodUs=now-rise;++sequence;}
      rise=now;phase=1;
    } else if(phase==1) {fall=now;phase=2;}
  }
};
