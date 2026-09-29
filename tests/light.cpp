#include "../firmware/UnoMiniEthernet/Terra.h"
#include <cassert>
#include <cstdio>
#include <algorithm>
static unsigned finish(Terra& w,uint32_t now){unsigned n=0;do{w.tick(now);++n;}while(w.busy());return n;}
int main(){
  unsigned checks=0;
  Terra base;base.begin(0,91);
  for(unsigned lx=0;lx<65536;lx++)for(unsigned debt: {0U,127U,128U,129U,248U,254U,255U}){
    Terra w=base;w.lightDebt=debt;w.environment(230,500,lx,7);
    unsigned dose=std::max(1U,std::min(8U,lx/320));
    unsigned expected=std::min(255U,debt+dose);assert(w.lightDebt==expected);
    Terra reference=base;reference.generation=1;reference.environment(230,500,lx,7);
    unsigned g=reference.growthBase;
    assert(w.growthBase==(expected>128?g*(255-expected)/128:g));
    assert(w.light==(lx<=10?0:lx>=320?255:(lx-10)*255/310));checks++;
  }
  for(unsigned debt=0;debt<256;debt++){
    Terra w=base;w.flags=1;w.lightDebt=debt;w.environment(230,500,0,7);
    assert(w.lightDebt==(debt>3?debt-3:0)&&w.growthBase==0);checks++;
  }
  for(uint32_t lx: {65536UL,0x7fffffffUL,0x80000000UL,0xffffffffUL}){
    Terra w=base;w.environment(230,500,lx,7);assert(w.lightDebt==8&&w.light==255);checks++;
  }
  Terra night=base;night.flags=1;unsigned active=0;
  for(unsigned i=1;i<=64;i++){
    night.environment(230,500,0,7);unsigned n=finish(night,i*10000U);
    if(n>1)active++;assert(night.generation==i);assert(n==1||n==1152);
  }
  assert(active==2);checks++;
  // A debt-free16h/8h cycle never reaches the stress knee and clears nightly.
  Terra regular=base;unsigned peak=0;
  for(unsigned i=0;i<7*8640;i++){
    regular.generation=i;regular.environment(230,500,(i%8640)<5760?320:0,7);
    peak=std::max(peak,unsigned(regular.lightDebt));assert(regular.lightDebt<=91);
    if(i%8640==8639)assert(regular.lightDebt==0);checks++;
  }
  assert(peak>=89);
  // First3min debounce and41/25lx hysteresis remain exact.
  Terra w=base;w.generation=1;
  for(unsigned i=0;i<17;i++)w.environment(230,500,25,7);assert(!(w.flags&1));
  w.environment(230,500,25,7);assert(w.flags&1);
  for(unsigned i=0;i<18;i++)w.environment(230,500,40,7);assert(w.flags&1);
  for(unsigned i=0;i<17;i++)w.environment(230,500,41,7);assert(w.flags&1);
  w.environment(230,500,41,7);assert(!(w.flags&1));checks++;
  printf("PASS %u arithmetic/dose/night/cycle checks; normal7-day peak debt=%u; host sizeof=%zu\n",checks,peak,sizeof(Terra));
}
