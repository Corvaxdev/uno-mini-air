#include "../firmware/UnoMiniEthernet/Terra.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <initializer_list>
namespace baseline {
#include "Terra.neutral-reference.h"
}
template<class T> void step(T& w,uint32_t now){do{w.tick(now);}while(w.busy());}
int main(){
 unsigned long cases=0;
 Terra probe;probe.begin(0);
 for(uint32_t p:{0UL,29999UL,30000UL,70000UL,95000UL,101325UL,105000UL,110000UL,110001UL,4294967295UL})
  for(uint32_t c=0;c<65536;c++){
   auto& w=probe;w.airLevel=24;w.environment(230,500,353,7,c,p);
   bool validC=c>=400&&c<=5000,validP=p>=30000&&p<=110000;
   uint32_t pressure=validP?p:101325;
   int target=validC?(uint64_t(c)*pressure+1266562)/2533125:24;
   assert(target>=5&&target<=217);
   assert(w.airLevel==Terra::approach(24,target));
   assert(w.growthBase>=140&&w.growthBase<=255);
   assert(w.metabolismRate>=8192&&w.metabolismRate<=10240);
   assert(bool(w.flags&128)==(!validC||!validP));++cases;
  }
 Terra w;w.begin(0);int lastGrowth=-1,lastLoad=-1;
 for(int a=5;a<=217;a++){
  w.airLevel=a;
  w.environment(230,500,353,7,a*25,101325);
  if(a>=16&&a<=200){assert(w.growthBase>=lastGrowth);assert(w.metabolismRate>=lastLoad);lastGrowth=w.growthBase;lastLoad=w.metabolismRate;}
 }
 w.begin(0);w.environment(230,500,353,7,5000,101325);assert(w.airLevel==46);
 for(int n=0;n<200;n++)w.environment(230,500,353,7,5000,101325);assert(w.airLevel==200);
 for(int n=0;n<200;n++)w.environment(230,500,353,7,0,0);assert(w.airLevel==24&&w.growthBase==221&&w.metabolismRate==8192);
 for(int t:{-500,-100,0,230,600,800})for(int a=16;a<=200;a++){
  w.begin(0);w.airLevel=a;w.environment(t,500,500,7,a*25,101325);
  assert(w.metabolismRate<=15360&&w.metabolismRate>=4096);assert(w.growthBase<=255);++cases;
 }
 for(int seed=1;seed<=20;seed++){
  Terra a;baseline::Terra b;a.begin(0,seed);b.begin(0,seed);
  for(int n=1;n<=500;n++){
   a.environment(230,500,353,7);b.environment(230,500,353,7);step(a,n*10000);step(b,n*10000);
   uint8_t x[480],y[480];a.snapshot(x,n*10000);b.snapshot(y,n*10000);assert(!memcmp(x,y,480));++cases;
  }
 }
 puts("co2,pressure,growth_base,energy_rate,equivalent_ppm");
 for(int c:{400,600,800,1000,1500,2000,5000})for(uint32_t p:{95000UL,101325UL,105000UL}){
  w.begin(0);for(int n=0;n<200;n++)w.environment(230,500,353,7,c,p);
  printf("%d,%u,%u,%u,%u\n",c,p,w.growthBase,w.metabolismRate,w.airLevel*25);
 }
 fprintf(stderr,"PASS %lu arithmetic/regression cases; max CO2 step bounded; invalid input fallback; monotonic responses; baseline snapshots identical.\n",cases);
}
