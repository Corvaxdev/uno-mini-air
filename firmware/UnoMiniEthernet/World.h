#pragma once
#include <stdint.h>
#include <string.h>

// 32x32 torus, B3/S23. One stored generation; eight cells per byte.
struct RamWorldStore {
  uint8_t bits[128];
  void read(uint8_t y,uint8_t* dst) const { memcpy(dst,bits+uint8_t(y*4),4); }
  void write(uint8_t y,const uint8_t* src) { memcpy(bits+uint8_t(y*4),src,4); }
};

template<class Store> struct World {
  Store store;
  uint32_t generation=0,at=0,acceptedAt=uint32_t(-2000L);
  uint16_t pending=0xffff;
  uint8_t row=32,previous[4],first[4];
  bool busy() const { return row!=32; }
  uint16_t untilNext(uint32_t now) const {
    const uint32_t elapsed=now-at;
    return elapsed<1000 ? uint16_t(1000-elapsed) : 0;
  }
  bool accepts(uint32_t now) const { return pending==0xffff && uint32_t(now-acceptedAt)>=2000; }
  void enqueue(uint16_t point,uint32_t now) {
    // Arrival-time jitter selects two reflection bits in the unused coordinate bits.
    // No RNG state, heap or additional protocol bytes.
    pending=point|uint16_t((now^(now>>8))&3)<<10;acceptedAt=now;
  }
  void stamp(uint16_t point) {
    const uint8_t x=point&31,y=(point>>5)&31;
    for(uint8_t j=0;j<3;++j) {
      uint8_t ry=(y+((point&0x800)?2-j:j))&31;
      uint8_t r[4];store.read(ry,r);
      for(uint8_t k=0;k<3;++k) if(j==2 || (j==0 && k==1) || (j==1 && k==2)) {
        uint8_t c=(x+((point&0x400)?2-k:k))&31;r[c>>3]|=uint8_t(1<<(c&7));
      }
      store.write(ry,r);
    }
  }
  void begin(uint32_t now) {
    const uint8_t zero[4]={0,0,0,0};
    for(uint8_t y=0;y<32;++y)store.write(y,zero);
    generation=0;at=now;acceptedAt=now-2000;pending=0xffff;row=32;
    stamp(8+8*32);stamp(22+20*32);
  }
  static uint8_t left(const uint8_t* r,uint8_t i) { return (r[i]<<1)|(r[(i+3)&3]>>7); }
  static uint8_t right(const uint8_t* r,uint8_t i) { return (r[i]>>1)|(r[(i+1)&3]<<7); }
  static void count(uint8_t n,uint8_t& one,uint8_t& two,uint8_t& four) {
    uint8_t a=one&n;one^=n;uint8_t b=two&a;two^=a;four^=b;
  }
  __attribute__((noinline)) void tick(uint32_t now) {
    if(!busy()) {
      if(pending!=0xffff) {stamp(pending);pending=0xffff;}
      if(uint32_t(now-at)<1000)return;
      at=now;store.read(31,previous);store.read(0,first);row=0;
    }
    uint8_t current[4],next[4],output[4];store.read(row,current);
    if(row==31)memcpy(next,first,4);else store.read(row+1,next);
    for(uint8_t i=0;i<4;++i) {
      uint8_t one=0,two=0,four=0;
      const uint8_t neighbors[]={left(previous,i),previous[i],right(previous,i),left(current,i),right(current,i),left(next,i),next[i],right(next,i)};
      for(uint8_t j=0;j<8;++j)count(neighbors[j],one,two,four);
      // Count 8 wraps to 0; both are dead under B3/S23, so no fourth bit plane.
      output[i]=uint8_t(~four)&two&(one|current[i]);
    }
    store.write(row,output);memcpy(previous,current,4);
    if(++row==32)++generation;
  }
  // Caller must wait for a complete generation. Copy to the existing HTTP buffer.
  void snapshot(uint8_t* dst) const {
    uint32_t g=generation;for(uint8_t i=0;i<4;++i){*dst++=g;g>>=8;}
    for(uint8_t y=0;y<32;++y) {store.read(y,dst);dst+=4;}
  }
};
