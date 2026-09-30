#include <cassert>
#include <cstdio>
#include <cstdint>
#include <climits>
#include "MiniBMP180.h"
uint32_t testMillis=0;TestWire Wire;
uint32_t randomWord(){static uint32_t s=0x32a096u;s^=s<<13;s^=s>>17;s^=s<<5;return s;}
bool reference(const int16_t* c,uint16_t ut,uint32_t up,uint8_t oss,int16_t& t,int32_t& p){
  if(oss>3 || up>((uint32_t(1)<<(16+oss))-1))return false;
  for(int i=3;i<6;++i)if(!c[i]||uint16_t(c[i])==65535)return false;
  int64_t x1=((int64_t(ut)-uint16_t(c[5]))*uint16_t(c[4]))>>15;
  int64_t d=x1+c[10];if(!d)return false;
  int64_t b5=x1+int64_t(c[9])*2048/d,tt=(b5+8)>>4;
  if(tt< -400||tt>850)return false;
  int64_t b6=b5-4000;
  x1=(int64_t(c[7])*((b6*b6)>>12))>>11;
  int64_t x2=(int64_t(c[1])*b6)>>11;
  int64_t b3=((int64_t(c[0])*4+x1+x2)*(int64_t(1)<<oss)+2)>>2;
  x1=(int64_t(c[2])*b6)>>13;x2=(int64_t(c[6])*((b6*b6)>>12))>>16;
  int64_t x3=(x1+x2+2)>>2;if(x3<=-32768||up<b3)return false;
  int64_t b4=(int64_t(uint16_t(c[3]))*(x3+32768))>>15;if(!b4)return false;
  int64_t b7=(int64_t(up)-b3)*(50000>>oss);if(b7>UINT32_MAX)return false;
  int64_t pp=b7<0x80000000LL ? b7*2/b4 : (b7/b4)*2;
  if(pp<25000||pp>120000)return false;
  x1=((pp>>8)*(pp>>8)*3038)>>16;x2=(-7357*pp)>>16;pp+=(x1+x2+3791)>>4;
  if(pp<30000||pp>110000)return false;t=tt;p=pp;return true;
}
void check(const int16_t* c,uint16_t ut,uint32_t up,uint8_t oss){
  int16_t a=1234,b=1234;int32_t p=123456,q=123456;
  bool ok=MiniBMP180::compensate(c,ut,up,oss,a,p), ref=reference(c,ut,up,oss,b,q);
  assert(ok==ref);assert(a==b&&p==q); // Failed calls leave both outputs unchanged.
}
int main(){
  int16_t c[11]={8407,-1202,-14486,-31674,24975,19151,6515,48,-32768,-11786,2835};
  for(uint32_t ut=0;ut<65536;++ut)for(uint8_t oss=0;oss<4;++oss)
    for(uint32_t up: {0u,1u,23843u,42190u,65535u})check(c,ut,up<<oss,oss);
  for(unsigned i=0;i<2000000;++i){
    for(auto& v:c)v=int16_t(randomWord());
    uint8_t oss=(i%8)?(randomWord()&3):uint8_t(randomWord());
    uint32_t up=(i%4)?(randomWord()&0x7ffffu):randomWord();
    check(c,uint16_t(randomWord()),up,oss);
  }
  puts("PASS: 1,310,720 board-calibration cases + 2,000,000 arbitrary calibration/raw/OSS cases match independent 64-bit reference; no sanitizer errors.");
}
