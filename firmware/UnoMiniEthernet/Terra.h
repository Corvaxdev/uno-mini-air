#pragma once
#include <stdint.h>
#include <string.h>
#ifdef __AVR__
#define TERRA_FN
#else
#define TERRA_FN
#endif

// Fixed storage, integer probabilities and one bounded work item per loop.
// Animal record: position:10, energy:3, species:1, acted:1. Zero = free slot.
struct Terra {
  enum { CAP=128, WIRE_SIZE=480, NONE=65535 };
  uint8_t grass[256];
  uint16_t animals[CAP];
  uint32_t rng, generation, at, creditAt, acceptedAt;
  uint32_t lux;
  uint16_t soil, humidity10, plants, biomass, pending, cursor, stride, progress;
  uint16_t events[7], metabolismRate; // births H/P, starvation H/P, kills, growth, actions
  int16_t temperature10;
  uint8_t herbs, predators, credits, phase, nightCount, stale[3], light;
  uint8_t lightDebt; // cumulative photoperiod/excess-light stress; recovers in darkness
  uint8_t flags, actionResult, growthBase, airLevel; // filtered CO2 partial-pressure equivalent, 25 ppm units
  // phase: 0 idle, 1 plant pass, 2 animal pass. flags bit0: dormant.
  uint32_t random() { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
  bool chance(uint16_t q) { return uint16_t(random()>>16)<q; }
  static uint16_t neighbor(uint16_t p,uint8_t d) {
    if(d==0)return (p&992)|((p+1)&31);
    if(d==1)return (p&992)|((p+31)&31);
    return (p+(d==2?32:992))&1023;
  }
  static bool refuge(uint16_t p) { return ((p&15)>=6 && (p&15)<10 && ((p>>5)&15)>=6 && ((p>>5)&15)<10); }
  static bool water(uint16_t p) {uint8_t x=p&31,y=p>>5;if(x>15)x=31-x;if(y>15)y=31-y;return x>=13&&y>=13&&x+y>=28;}
  static bool shore(uint16_t p) {return uint8_t((p&31)-11)<10 && uint8_t((p>>5)-11)<10 && !water(p);}
  uint8_t plant(uint16_t p) const { return grass[p>>2]>>((p&3)*2)&3; }
  void setPlant(uint16_t p,uint8_t n) {
    uint8_t old=plant(p),s=(p&3)*2;
    biomass=biomass-old+n; plants=plants-(old!=0)+(n!=0);
    grass[p>>2]=(grass[p>>2]&~(3<<s))|(n<<s);
  }
  int16_t find(uint16_t p) const {
    for(uint8_t k=0;k<CAP;k++)if(animals[k] && (animals[k]&1023)==p)return k;
    return -1;
  }
  int16_t freeSlot() const { for(uint8_t k=0;k<CAP;k++)if(!animals[k])return k;return -1; }
  void erase(uint8_t k) { if(animals[k]&8192)--predators;else --herbs;animals[k]=0; }
  bool insert(uint16_t p,bool pred,uint8_t e,bool acted=true) {
    int16_t k=freeSlot(); if(k<0)return false;
    animals[k]=p|(uint16_t(e)<<10)|(pred?8192:0)|(acted?16384:0);
    if(pred)++predators;else ++herbs;return true;
  }
  uint8_t danger(uint16_t p) const {
    if(refuge(p))return 0; // Shelters actually protect their occupants.
    uint8_t n=0;for(uint8_t d=0;d<4;d++){int16_t k=find(neighbor(p,d));if(k>=0 && (animals[k]&8192))++n;}return n;
  }
  TERRA_FN void begin(uint32_t now,uint32_t seed=0x6d2b79f5UL) {
    memset(this,0,sizeof(*this));rng=seed?seed:1;at=creditAt=now;acceptedAt=now-60000UL;
    pending=NONE;credits=12;soil=153*256;temperature10=230;humidity10=500;lux=227;light=178;flags=14;growthBase=178;metabolismRate=8192;airLevel=24;
    while(plants<384){uint16_t p=random()&1023;if(!water(p)&&!plant(p))setPlant(p,2);}
    while(herbs<60){uint16_t p=random()&1023;if(!water(p)&&find(p)<0)insert(p,false,5,false);}
    while(predators<8){uint16_t p=random()&1023;if(!water(p)&&!refuge(p)&&find(p)<0)insert(p,true,5,false);}
  }
  // Called once per 10-second boundary. Missing inputs retain the last value
  // for 5 minutes, then approach explicit fallback values without a jump.
  static int16_t approach(int16_t v,int16_t target) {
    int16_t delta=target-v;return v+(delta/8?delta/8:delta>0?1:delta<0?-1:0);
  }
  TERRA_FN void environment(int16_t t,uint16_t rh,uint32_t lx,uint8_t valid,uint16_t co2=600,uint32_t pressure=101325) {
    if(t<-500||t>800)valid&=~1;if(rh>1000)valid&=~2;
    for(uint8_t i=0;i<3;i++){if(valid&(1<<i))stale[i]=0;else if(stale[i]<30)++stale[i];}
    flags&=1;
    if(valid&1)temperature10=t;else if(stale[0]>=30){temperature10=approach(temperature10,230);flags|=2;}
    if(valid&2)humidity10=rh;else if(stale[1]>=30){humidity10=approach(humidity10,500);flags|=4;}
    if(valid&4)lux=lx;else if(stale[2]>=30){int32_t delta=227L-int32_t(lux);lux+=delta/8?delta/8:delta>0?1:delta<0?-1:0;flags|=8;}
    light=lux<=10?0:lux>=320?255:uint32_t(lux-10)*255/310;
    int16_t delta=temperature10-230;if(delta<0)delta=-delta;
    uint16_t tf=delta>=250?0:255-uint16_t(delta)*51/50;
    growthBase=uint16_t(383-(49151U+light)/(light+128))*tf>>8;
    int16_t metabolism=temperature10+170;metabolism=metabolism<200?200:metabolism>600?600:metabolism;
    metabolismRate=uint32_t(metabolism)*8192/400;
    // Game response, not an indoor-air safety score. Pressure scales partial
    // pressure; bounded gains avoid runaway growth and abrupt animal deaths.
    uint8_t targetAir=24;
    if(pressure<30000UL||pressure>110000UL){pressure=101325;flags|=128;}
    if(co2>=400&&co2<=5000)targetAir=(uint32_t(co2)*pressure+1266562UL)/2533125UL;else flags|=128;
    airLevel=approach(airLevel,targetAir);
    int16_t gain=64-int16_t(3072U/(airLevel+24));
    int16_t growth=growthBase+int16_t(growthBase)*gain/256;
    growthBase=growth>255?255:growth;
    uint8_t load=airLevel<=32?0:64-4096U/(airLevel+32);
    metabolismRate+=(uint16_t(metabolismRate>>4)*load)>>4;
    if(valid!=7)flags|=128;
    // Room calibration: full day at320lx; sleep<=25lx, wake>=41lx.
    bool switching=(flags&1)?light>24:light<13;
    if(switching){if(++nightCount==18){flags^=1;nightCount=0;}}else nightCount=0;
    // One dose update per64 world boundaries (10m40s), no extra timer.
    // Normal16h light/8h dark clears the debt. Extra irradiance accelerates it.
    if(!(generation&63)){
      if(flags&1)lightDebt=lightDebt>3?lightDebt-3:0;
      else {uint8_t dose=lux<640?1:lux>=2560?8:lux/320;
        uint16_t sum=uint16_t(lightDebt)+dose;lightDebt=sum>255?255:sum;}
    }
    if(lightDebt>128)growthBase=uint16_t(growthBase)*(255-lightDebt)>>7;
    if(!(flags&1)){
      uint16_t target=humidity10<=200?0:humidity10>=700?65280:uint32_t(humidity10-200)*65280/500;
      soil+=int32_t(int32_t(target)-soil)/180;
    }
  }
  bool busy() const {return phase!=0;}
  uint16_t untilNext(uint32_t now) const {uint32_t dt=now-at;return dt<10000?10000-dt:0;}
  bool due(uint32_t now) const {return !phase && now-at>=10000UL;}
  uint8_t cooldown(uint32_t now) const {uint32_t dt=now-acceptedAt;return dt<60000UL?(60000UL-dt+999)/1000:0;}
  bool accepts(uint32_t now) const {return pending==NONE&&!cooldown(now);}
  bool enqueue(uint16_t command,uint32_t now) {
    if(command>=3072||!accepts(now))return false;pending=command;actionResult=0;return true;
  }
  TERRA_FN void apply(uint32_t now) {
    if(pending==NONE)return;
    uint8_t kind=pending>>10,cost=kind==0?1:kind==1?3:8;uint16_t p=pending&1023;pending=NONE;
    actionResult=2;if(water(p)||credits<cost)return;
    if(kind==0){bool changed=false;
      for(int8_t y=-1;y<=1;y++)for(int8_t x=-1;x<=1;x++){
        uint16_t q=(((p>>5)+y+32)&31)*32+(((p&31)+x+32)&31);
        if(!water(q)&&!plant(q)){setPlant(q,1);changed=true;}
      }if(!changed){actionResult=4;return;}
    }else{if(find(p)>=0||(kind==2&&refuge(p)))return;if(!insert(p,kind==2,3)){actionResult=3;return;}}
    credits-=cost;acceptedAt=now;actionResult=1;++events[6];
  }
  TERRA_FN void plantStep(uint16_t p) {
    if(water(p))return;
    uint8_t b=plant(p),x=p&31;
    int16_t m=int16_t(soil>>8)+38-int16_t(x)*76/31+(shore(p)?64:0);m=m<0?0:m>255?255:m;
    uint16_t localLight=uint16_t(growthBase)*(179+uint16_t(x)*76/31)>>8;
    uint16_t growth=localLight*uint16_t(64+3*m/4)>>5;
    uint16_t decay=m<64?(64-m)*8:0;
    if(b&&chance(decay)){setPlant(p,b-1);return;}
    if(b==3)return;
    if(!b){bool seeded=false;for(uint8_t d=0;d<4;d++)seeded|=plant(neighbor(p,d))!=0;growth/=seeded?4:64;}
    if(chance(growth)){setPlant(p,b+1);++events[5];}
  }
  int16_t freeNeighbor(uint16_t p,bool pred) {
    int16_t result=-1;uint8_t ties=0;
    for(uint8_t d=0;d<4;d++){uint16_t q=neighbor(p,d);if(!water(q)&&find(q)<0&&(!pred||!refuge(q))&&random()%++ties==0)result=q;}
    return result;
  }
  TERRA_FN void animalStep(uint8_t k) {
    uint16_t v=animals[k];if(!v||(v&16384))return;
    uint16_t pos=v&1023;bool pred=v&8192;uint8_t e=v>>10&7;
    animals[k]|=16384;
    if(!pred){uint8_t risk=danger(pos),food=plant(pos),ties=1;
      uint16_t origin=pos;
      for(uint8_t d=0;d<4;d++){uint16_t q=neighbor(origin,d);if(water(q)||find(q)>=0)continue;
        uint8_t a=danger(q),b=plant(q);
        if(a<risk||(a==risk&&b>food)){pos=q;risk=a;food=b;ties=1;}
        else if(a==risk&&b==food&&random()%++ties==0)pos=q;
      }
      if(e<=4&&plant(pos)){setPlant(pos,plant(pos)-1);e+=3;}
    }else{
      int16_t prey=-1;uint8_t ties=0;
      if(e<=5)for(uint8_t d=0;d<4;d++){uint16_t q=neighbor(pos,d);int16_t z=find(q);
        if(!refuge(q)&&z>=0&&!(animals[z]&8192)&&random()%++ties==0)prey=z;
      }
      if(prey>=0){pos=animals[prey]&1023;erase(prey);e=e>3?7:e+4;++events[4];}
      else {int8_t best=-1;uint16_t origin=pos;ties=0;
        for(uint8_t d=0;d<4;d++){uint16_t q=neighbor(origin,d);if(water(q)||refuge(q)||find(q)>=0)continue;
          uint8_t score=0;for(uint8_t dd=0;dd<4;dd++){uint16_t z=neighbor(q,dd);int16_t j=find(z);score+=!refuge(z)&&j>=0&&!(animals[j]&8192);}
          if(int8_t(score)>best){best=score;pos=q;ties=1;}
          else if(score==best&&random()%++ties==0)pos=q;
        }
      }
    }
    if(chance(pred?metabolismRate/2:metabolismRate))--e;
    if(!e){erase(k);++events[pred?3:2];return;}
    animals[k]=pos|(uint16_t(e)<<10)|(pred?8192:0)|16384;
    uint8_t h=0,food=plant(pos);
    if(!pred)for(uint8_t d=0;d<4;d++){uint16_t q=neighbor(pos,d);int16_t j=find(q);h+=j>=0&&!(animals[j]&8192);food+=plant(q);}
    // Resource-gated births damp boom/bust without spawning or rescuing animals.
    bool foodBudget=pred ? herbs>=uint16_t(predators)*8 : biomass>=uint16_t(herbs)*8;
    uint16_t birthChance=uint16_t(CAP-herbs-predators)<<(pred?0:4);
    if(e==7&&foodBudget&&(pred||(h<2&&food>=6))&&chance(birthChance)){
      int16_t q=freeNeighbor(pos,pred);
      if(q>=0){if(insert(q,pred,3)){animals[k]=(animals[k]&~7168)|3072;++events[pred?1:0];}}
    }
  }
  TERRA_FN void tick(uint32_t now) {
    if(now-creditAt>=600000UL){uint32_t n=(now-creditAt)/600000UL;credits=n>=uint8_t(12-credits)?12:credits+n;creditAt+=n*600000UL;}
    if(!phase){if(!due(now))return;at=now;apply(now);
      if((flags&1)&&(generation&31)){++generation;return;} // Night work at1/32 rate, never frozen.
      cursor=random()&1023;stride=(random()&1023)|1;progress=0;phase=1;
    }
    if(phase==1){plantStep(cursor);if(++progress==1024){for(uint8_t k=0;k<CAP;k++)animals[k]&=~16384;cursor=random()&127;stride=(random()&127)|1;progress=0;phase=2;return;}}
    else {animalStep(cursor);if(++progress==CAP){phase=0;++generation;return;}}
    cursor=(cursor+stride)&(phase==1?1023:127);
  }
  static void put16(uint8_t* p,uint16_t v){p[0]=v;p[1]=v>>8;}
  static void put32(uint8_t* p,uint32_t v){put16(p,v);put16(p+2,v>>16);}
  // Only called between passes. One coherent generation, no RNG consumption.
  TERRA_FN void snapshot(uint8_t* dst,uint32_t now) const {
    memset(dst,0,WIRE_SIZE);put32(dst,generation);put32(dst+4,now/1000);
    put16(dst+8,plants);put16(dst+10,biomass);dst[12]=herbs;dst[13]=predators;
    dst[14]=credits;dst[15]=flags|(nightCount?16:0)|(herbs+predators==CAP?32:0)|(pending!=NONE?64:0);
    dst[16]=soil>>8;dst[17]=light;put16(dst+18,temperature10);put16(dst+20,humidity10);put32(dst+22,lux);
    dst[26]=actionResult;dst[27]=cooldown(now);put16(dst+28,untilNext(now));put16(dst+30,(600000UL-(now-creditAt))/1000);
    for(uint8_t k=0;k<7;k++)put16(dst+32+k*2,events[k]); // bytes46..47 remain zero for old clients
    memcpy(dst+48,grass,256);uint16_t bit=0;
    for(uint8_t k=0;k<CAP;k++)if(animals[k]){
      uint16_t v=(animals[k]&1023)|((animals[k]&8192)?1024:0),byte=304+(bit>>3);uint8_t shift=bit&7;
      uint32_t packed=uint32_t(v)<<shift;dst[byte]|=packed;dst[byte+1]|=packed>>8;if(shift>5)dst[byte+2]|=packed>>16;bit+=11;
    }
  }
};
