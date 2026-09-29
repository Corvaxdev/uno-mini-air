#include "../firmware/UnoMiniEthernet/Terra.h"
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <chrono>
#include <algorithm>
#include <string>
#include <iostream>
static void check(const Terra& w){
  unsigned cells=0,b=0,h=0,p=0;
  bool occupied[1024]={};
  for(unsigned i=0;i<1024;i++){cells+=w.plant(i)>0;b+=w.plant(i);assert(!Terra::water(i)||!w.plant(i));}
  for(auto a:w.animals)if(a){unsigned i=a&1023;assert(!occupied[i]&&!Terra::water(i));occupied[i]=true;assert((a>>10&7)>0);if(a&8192){++p;assert(!Terra::refuge(i));}else ++h;}
  assert(cells==w.plants&&b==w.biomass&&h==w.herbs&&p==w.predators&&h+p<=128&&w.credits<=12);
}
static unsigned step(Terra& w,uint32_t now){unsigned slices=0;do{w.tick(now);++slices;}while(w.busy());return slices;}
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])=="batch"){
    unsigned seeds=argc>2?atoi(argv[2]):20,ticks=argc>3?atoi(argv[3]):8640;int t=argc>4?atoi(argv[4]):230,rh=argc>5?atoi(argv[5]):500,lux=argc>6?atoi(argv[6]):353;int co2=argc>7?atoi(argv[7]):600,pressure=argc>8?atoi(argv[8]):101325,mode=argc>9?atoi(argv[9]):0;
    puts("seed,ticks,herbs,predators,plants,biomass,herb_births,pred_births,herb_deaths,pred_deaths,kills,growth,actions,capacity_blocks");
    unsigned first=argc>10?atoi(argv[10]):1;for(unsigned seed=first;seed<first+seeds;seed++){Terra w;w.begin(0,seed);for(unsigned i=1;i<=ticks;i++){int c=mode==1?((i%8640)<4320?600:2400):mode==2?((i%37)?co2:5000):mode==3?0:co2;int l=mode==4?((i%8640)<5760?lux:0):lux;int tt=t,rr=rh,pp=pressure;if(mode==5){int wave=(i%4320)<2160?(i%4320):(4320-i%4320);tt=230+60*wave/2160;rr=450-80*wave/2160;l=(i%8640)<5760?200+300*wave/2160:0;c=500+1500*wave/2160;pp=99000+4000*wave/2160;}w.environment(tt,rr,l,7,c,pp);step(w,i*10000U);check(w);}printf("%u,%u,%u,%u,%u,%u",seed,ticks,w.herbs,w.predators,w.plants,w.biomass);for(auto n:w.events)printf(",%u",n);puts(",0");}return 0;
  }
  if(argc>1&&std::string(argv[1])=="test"){
    Terra w;w.begin(0,42);check(w);assert(sizeof(w)<=610);assert(Terra::WIRE_SIZE<=512);
    uint8_t bytes[482];bytes[0]=33;bytes[481]=77;w.snapshot(bytes+1,0);assert(bytes[0]==33&&bytes[481]==77);
    auto rng=w.rng;for(int i=0;i<100;i++)w.snapshot(bytes+1,0);assert(rng==w.rng);
    unsigned water=0,safe=0;for(int i=0;i<1024;i++){water+=Terra::water(i);safe+=Terra::refuge(i);for(int d=0;d<4;d++)assert(Terra::neighbor(i,d)<1024);}assert(water==24&&safe==64);
    for(int i=0;i<17;i++)w.environment(230,500,0,7);assert(!(w.flags&1));w.environment(230,500,0,7);assert(w.flags&1);
    auto copy=w;step(w,10000);assert(w.generation==copy.generation+1);
    for(int i=0;i<18;i++)w.environment(230,500,353,7);assert(!(w.flags&1));
    for(int i=0;i<29;i++)w.environment(0,0,0,0);assert(!(w.flags&14));w.environment(0,0,0,0);assert((w.flags&14)==14);
    w.begin(0,42);assert(w.enqueue(2048+15*32+15,0));auto credit=w.credits;step(w,10000);assert(w.actionResult==2&&w.credits==credit);
    int q=0;while(Terra::water(q)||w.find(q)>=0)q++;assert(w.enqueue(1024+q,10000));step(w,20000);assert(w.actionResult==1&&w.credits==9&&!w.accepts(20000));
    step(w,600000);assert(w.credits==10);check(w);
    w.begin(0,1);for(int i=0;i<128;i++)w.animals[i]=0;w.herbs=w.predators=0;for(int i=0;i<128;i++)assert(w.insert(i,false,7));assert(!w.insert(200,false,3));w.snapshot(bytes+1,0);assert(bytes[0]==33&&bytes[481]==77);
    Terra a,b;a.begin(0,8);b=a;for(int i=1;i<=200;i++){a.environment(230,500,353,7);b.environment(230,500,353,7);step(a,i*10000);for(int n=0;n<50;n++)b.snapshot(bytes+1,(i-1)*10000);step(b,i*10000);assert(!memcmp(&a,&b,sizeof(a)));check(a);}
    w.begin(0xfffff000U,1);step(w,uint32_t(0xfffff000U+10000U));assert(w.generation==1);
    printf("PASS invariants, pond, shelter, night debounce, fallback, action validation, cooldown, credits, capacity, snapshot bounds, reader independence, millis wrap; sizeof(Terra)=%zu\n",sizeof(Terra));return 0;
  }
  Terra w;uint32_t now=0;int t=230,rh=500;unsigned lx=353;w.begin(0,42);std::string cmd;
  while(std::cin>>cmd){
    if(cmd=="step"){unsigned n;std::cin>>n;for(unsigned i=0;i<n;i++){now+=10000;w.environment(t,rh,lx,7);step(w,now);}check(w);}
    else if(cmd=="env")std::cin>>t>>rh>>lx;
    else if(cmd=="action"){unsigned p;std::cin>>p;w.enqueue(p,now);}
    else if(cmd=="reset"){unsigned seed;std::cin>>seed;w.begin(now,seed);}
    else if(cmd!="state")return 1;
    uint8_t data[480];w.snapshot(data,now);for(auto n:data)printf("%02x",n);puts("");fflush(stdout);
  }
}
