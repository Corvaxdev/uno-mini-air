#include <array>
#include <vector>
#include <cassert>
#include <cstdio>
#include <algorithm>
#include "../firmware/UnoMiniEthernet/ViewJournal.h"
struct Store {
  static std::array<uint8_t,1024> bytes;
  static std::array<unsigned,1024> wear;
  static std::vector<std::pair<uint16_t,uint8_t>> trace;
  static bool busy;
  static bool ready(){return !busy;}
  static uint8_t read(uint16_t a){assert(a<1024);return bytes[a];}
  static void update(uint16_t a,uint8_t v){
    assert(a<1024);trace.push_back({a,v});
    if(bytes[a]!=v){bytes[a]=v;++wear[a];}
  }
  static void clear(){bytes.fill(255);wear.fill(0);trace.clear();busy=false;}
};
std::array<uint8_t,1024> Store::bytes;
std::array<unsigned,1024> Store::wear;
std::vector<std::pair<uint16_t,uint8_t>> Store::trace;
bool Store::busy=false;
using Journal=ViewJournal<Store>;
uint32_t restored(){Journal j;j.begin(0);return j.total;}
void flush(Journal& j,uint32_t now){
  int result=0;for(int i=0;i<10;++i)result=j.tick(now);
  assert(result==1 && j.saved>0);
}
int main(){
  Store::clear();Journal j;j.begin(0);assert(j.total==0 && Store::trace.empty());
  j.tick(900000);assert(Store::trace.empty());
  j.increment();j.tick(1799999);assert(Store::trace.empty());
  Store::busy=true;j.tick(1800000);assert(Store::trace.empty());Store::busy=false;
  flush(j,1800000);assert(j.saved==1 && restored()==1);
  auto writes=Store::trace.size();j.tick(2700000);assert(Store::trace.size()==writes);
  for(uint32_t n=2;n<=1024;++n){j.increment();flush(j,(n+2)*900000UL);assert(restored()==n);}
  for(unsigned slot=0;slot<128;++slot){
    assert(Store::wear[slot*8+7]==16);
    for(unsigned b=0;b<7;++b)assert(Store::wear[slot*8+b]<=8);
  }
  unsigned cuts=0;
  // Both first-use and wrapped slots; each possible torn byte at each phase.
  for(int wrapped=0;wrapped<2;++wrapped){
    if(!wrapped)Store::clear();
    auto initial=Store::bytes;uint32_t before=restored();Journal a;a.begin(0);
    for(int k=0;k<257;++k)a.increment();
    uint32_t after=a.total;Store::trace.clear();flush(a,900000);
    const auto operations=Store::trace;assert(operations.size()==9);
    for(unsigned cut=0;cut<operations.size();++cut){
      Store::bytes=initial;
      for(unsigned i=0;i<cut;++i)Store::bytes[operations[i].first]=operations[i].second;
      const auto prefix=Store::bytes;
      for(unsigned torn=0;torn<256;++torn){
        Store::bytes=prefix;Store::bytes[operations[cut].first]=uint8_t(torn);
        uint32_t r=restored();assert(r==before||r==after);++cuts;
      }
    }
    Store::bytes=initial;Journal ring;ring.begin(0);
    for(uint32_t n=1;n<=256;++n){ring.increment();flush(ring,n*900000UL);}
  }
  // Corruption of the newest record falls back to the previous valid record.
  Store::clear();Journal c;c.begin(0);c.increment();flush(c,900000);c.increment();flush(c,1800000);
  auto intact=Store::bytes;
  for(unsigned bit=0;bit<64;++bit){Store::bytes=intact;Store::bytes[8+bit/8]^=1<<(bit%8);assert(restored()==1);}
  Store::bytes=intact;
  // New views during a flush are not marked as persisted prematurely.
  Journal d;d.begin(0);d.increment();d.tick(900000);d.increment();
  for(int k=0;k<9;++k)d.tick(900000);
  assert(d.saved==3&&d.total==4&&restored()==3);flush(d,1800000);assert(restored()==4);
  // millis wraps; the 15-minute interval stays exact.
  Journal e;e.begin(UINT32_MAX-500000);e.increment();Store::trace.clear();
  e.tick(399998);assert(Store::trace.empty());flush(e,399999);
  // Saturation avoids ambiguous sequence wrap after 4.29 billion views.
  e.total=UINT32_MAX;flush(e,1299999);e.increment();assert(e.total==UINT32_MAX&&restored()==UINT32_MAX);
  // A failed physical write is detected without replacing the prior record.
  Store::clear();Journal f;f.begin(0);f.increment();flush(f,900000);f.increment();
  for(int k=0;k<9;++k)f.tick(1800000);
  Store::bytes[8]^=1;assert(f.tick(1800000)==-1&&f.saved==1&&restored()==1);
  writes=Store::trace.size();f.tick(1800001);assert(Store::trace.size()==writes);
  flush(f,2700000);assert(restored()==2);
  std::printf("PASS: 1024 commits / 8 ring cycles; %u torn-write cases; 64 corrupt-record cases; interval, wrap, busy, unchanged, saturation, snapshot, readback failure.\n",cuts);
}
