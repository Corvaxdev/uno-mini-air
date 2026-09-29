#include "../firmware/UnoMiniEthernet/Terra.h"
#include <cassert>
#include <cstdio>
int main(){
  unsigned protectedCells=0,checks=0;
  for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++){
    bool refuge=((x>=6&&x<=9)||(x>=22&&x<=25))&&((y>=6&&y<=9)||(y>=22&&y<=25));
    unsigned q=y*32+x;assert(Terra::refuge(q)==refuge);protectedCells+=refuge;
    for(unsigned species=0;species<2;species++){
      Terra w;w.begin(0,1);memset(w.animals,0,sizeof(w.animals));w.herbs=w.predators=0;
      assert(w.enqueue((species?2048:1024)+q,0));w.apply(0);
      bool expected=!Terra::water(q)&&!(species&&refuge);
      assert((w.actionResult==1)==expected);
      assert(w.credits==(expected?species?4:9:12));
      assert((w.herbs+w.predators)==unsigned(expected));checks++;
    }
  }
  assert(protectedCells==64);printf("PASS %u direct MCU action cases;64 refuge cells reject predators, allow herbivores; rejected actions cost0\n",checks);
}
