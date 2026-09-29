#include "../firmware/UnoMiniEthernet/Terra.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>

static bool pond(unsigned x,unsigned y){
  // Independent row spans, also used to verify browser geometry.
  if(y<13||y>18)return false;
  const unsigned left[]={15,14,13,13,14,15}, right[]={16,17,18,18,17,16};
  return x>=left[y-13]&&x<=right[y-13];
}
int main(){
  unsigned cases=0,water=0;
  for(unsigned p=0;p<1024;p++){
    assert(Terra::water(p)==pond(p%32,p/32));water+=pond(p%32,p/32);
    for(unsigned kind=0;kind<3;kind++)for(unsigned low=0;low<2;low++){
      Terra w;w.begin(0,1);memset(w.grass,0,sizeof(w.grass));memset(w.animals,0,sizeof(w.animals));
      w.plants=w.biomass=w.herbs=w.predators=0;
      if(low)w.credits=0;
      Terra before=w;assert(w.enqueue(kind*1024+p,10000));w.apply(10000);
      const bool expected=!low&&!pond(p%32,p/32)&&!(kind==2&&Terra::refuge(p));
      assert((w.actionResult==1)==expected);
      assert(w.pending==Terra::NONE);
      if(!expected){
        assert(w.credits==before.credits&&w.acceptedAt==before.acceptedAt&&w.events[6]==before.events[6]);
        assert(w.plants==0&&w.biomass==0&&w.herbs==0&&w.predators==0);
        assert(!memcmp(before.grass,w.grass,sizeof(w.grass))&&!memcmp(before.animals,w.animals,sizeof(w.animals)));
        assert(w.accepts(10000));
      }else{
        assert(w.credits==12-(kind==0?1:kind==1?3:8));assert(w.events[6]==1&&!w.accepts(10000));
        if(kind==0){
          unsigned count=0;
          for(unsigned q=0;q<1024;q++){
            unsigned dx=(q%32+32-p%32)%32,dy=(q/32+32-p/32)%32;
            bool seeded=(dx<=1||dx==31)&&(dy<=1||dy==31)&&!pond(q%32,q/32);
            assert(w.plant(q)==seeded);count+=seeded;
          }
          assert(w.biomass==count&&w.plants==count);
        }else{assert(w.find(p)>=0&&w.herbs+w.predators==1);}
      }
      cases++;
    }
  }
  assert(water==24);printf("PASS %u placement cases; all 1024 cells, three actions, normal/empty reserve; 24 water cells; shoreline clipping; toroidal edges; no cost/cooldown on rejection\n",cases);
}
