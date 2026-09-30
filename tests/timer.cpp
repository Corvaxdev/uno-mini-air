#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "../firmware/UnoMiniEthernet/Terra.h"
int main(){
 const uint64_t wrap=uint64_t(1)<<32;unsigned checks=0;
 for(uint64_t start:{uint64_t(0),wrap-30000,wrap+100000}){
  Terra w;w.begin(uint32_t(start));for(unsigned p=0;p<1024;p++)w.setPlant(p,0);
  assert(w.enqueue(0,uint32_t(start)));w.apply(uint32_t(start));assert(w.actionResult==1);
  for(uint64_t t=start;t<start+2*wrap+70000;t+=10000){
   // Skip ecological work; exercise the real periodic service and timers.
   w.phase=0;w.flags=1;w.generation=1;w.tick(uint32_t(t));
   assert(w.cooldown(uint32_t(t))==(t-start<60000?(60000-(t-start)+999)/1000:0));++checks;
  }
 }
 Terra w;w.begin(0);
 for(uint64_t t=0;t<wrap+70000;t+=10000){w.phase=0;w.flags=1;w.generation=1;w.tick(uint32_t(t));assert(w.accepts(uint32_t(t)));++checks;}
 printf("PASS %u cooldown checks across two millis wraps and active-wrap actions\n",checks);
}
