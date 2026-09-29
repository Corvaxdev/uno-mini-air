#include <array>
#include <set>
#include <cassert>
#include <cstdio>
#include "../firmware/UnoMiniEthernet/ViewJournal.h"
struct Store {
 static inline std::array<uint8_t,1024> bytes;
 static inline std::array<bool,1024> bad;
 static inline std::set<unsigned> slots;
 static inline unsigned launches=0;
 static bool ready(){return true;}
 static uint8_t read(uint16_t a){assert(a<1024);return bytes[a];}
 static void update(uint16_t a,uint8_t v){assert(a<1024);++launches;slots.insert(a/8);if(!bad[a])bytes[a]=v;}
 static void reset(){bytes.fill(255);bad.fill(false);slots.clear();launches=0;}
};
using J=ViewJournal<Store>;
int flush(J& j,uint32_t t){int r=0;for(int n=0;n<10;n++){auto before=Store::launches;r=j.tick(t);assert(Store::launches-before<=1);}return r;}
uint32_t restored(){J j;j.begin(0);return j.total;}
int main(){unsigned cases=0;
 // Every payload/CRC/format/commit byte, every position in the ring.
 for(unsigned slot=0;slot<128;slot++)for(unsigned byte=0;byte<8;byte++){
  Store::reset();J j;j.begin(0);uint32_t now=0;
  for(unsigned n=0;n<slot;n++){j.increment();now+=J::INTERVAL;assert(flush(j,now)==1);}
  auto before=Store::bytes;J probe=j;probe.increment();assert(flush(probe,now+J::INTERVAL)==1);
  uint8_t stuck=Store::bytes[slot*8+byte]^1;Store::bytes=before;Store::bytes[slot*8+byte]=stuck;
  Store::bad[slot*8+byte]=true;j.increment();now+=J::INTERVAL;assert(flush(j,now)==-1);assert(j.saved==slot&&restored()==slot);
  auto launches=Store::launches;j.tick(now+1);assert(Store::launches==launches);
  j.increment();now+=J::INTERVAL;assert(flush(j,now)==1);assert(j.saved==slot+2&&restored()==slot+2);++cases;
 }
 // Entire remainder of ring broken: wrap twice without invalidating the latest good record.
 Store::reset();J j;j.begin(0);j.increment();assert(flush(j,J::INTERVAL)==1);
 for(unsigned a=8;a<1024;a++)Store::bad[a]=true;Store::slots.clear();
 for(unsigned n=2;n<=260;n++){j.increment();assert(flush(j,n*J::INTERVAL)==-1);assert(j.saved==1&&restored()==1);++cases;}
 assert(!Store::slots.count(0));assert(Store::slots.size()==127);
 // One repaired slot resumes persistence and gets a CRC for its new slot number.
 for(unsigned a=8;a<1024;a++)Store::bad[a]=false;
 assert(flush(j,261*J::INTERVAL)==1);assert(restored()==260);++cases;
 // Even after a reboot with only one good record, a full failed circuit preserves it.
 J reboot;reboot.begin(0);auto intact=Store::bytes;unsigned last=0;
 for(unsigned s=0;s<128;s++)if(Store::bytes[s*8]==4&&Store::bytes[s*8+1]==1)last=s;
 for(unsigned a=0;a<1024;a++)Store::bad[a]=(a/8)!=last;Store::slots.clear();
 for(unsigned n=1;n<=130;n++){reboot.increment();assert(flush(reboot,n*J::INTERVAL)==-1);assert(restored()==260);++cases;}
 assert(!Store::slots.count(last));
 // Completely uninitialized failed EEPROM: bounded work, no bogus persistence.
 Store::reset();Store::bad.fill(true);J blank;blank.begin(0);blank.increment();
 for(unsigned n=1;n<=260;n++){assert(flush(blank,n*J::INTERVAL)==-1);assert(blank.saved==0&&restored()==0);++cases;}
 assert(Store::slots.size()==128);
 printf("PASS %u persistent-fault cases: every byte/slot, deferred retry, wrap, protected checkpoint, reboot, recovery, empty failed store; <=1 byte write/tick.\n",cases);
}
