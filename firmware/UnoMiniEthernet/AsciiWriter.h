#pragma once
#include <stdint.h>
#ifdef __AVR__
#include <avr/pgmspace.h>
#else
#define PSTR(x) x
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#endif

// Bounded ASCII serialization without printf, floats, heap, or temporary strings.
class AsciiWriter {
  char* out;
  uint16_t capacity,used=0;
  bool good=true;
public:
  AsciiWriter(char* data,uint16_t size):out(data),capacity(size) {
    if(capacity)out[0]=0;else good=false;
  }
  void put(char c) {
    if(used+1>=capacity) {good=false;return;}
    out[used++]=c;out[used]=0;
  }
  void text(const char* flash) {
    char c;while((c=pgm_read_byte(flash++)))put(c);
  }
  void number(uint32_t value) {
    char digits[10];uint8_t n=0;
    do {digits[n++]=char('0'+value%10);value/=10;}while(value);
    while(n)put(digits[--n]);
  }
  void tenths(int16_t value) {
    uint16_t magnitude=value<0?uint16_t(0)-uint16_t(value):uint16_t(value);
    if(value<0)put('-');number(magnitude/10);put('.');put('0'+magnitude%10);
  }
  uint16_t size() const {return good?used:0;}
};
