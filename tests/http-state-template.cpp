#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include "../firmware/UnoMiniEthernet/HttpRequest.h"
#include "../firmware/UnoMiniEthernet/Terra.h"
using std::min;
#define MAX_SOCK_NUM 4
#define memcpy_P memcpy
#define strcpy_P strcpy
#include "asset-size.h"
uint64_t clockUs=0;
uint32_t millis(){return uint32_t(clockUs/1000);}
uint32_t micros(){return uint32_t(clockUs);}
uint8_t buffer[512];Terra world;
struct Views{int count=0;void increment(){++count;}}views;
namespace SnSR{enum{CLOSED,INIT,LISTEN,ESTABLISHED,CLOSE_WAIT};}
enum{Sock_CLOSE,Sock_LISTEN,Sock_DISCON};
struct Conn{unsigned submitted=0,acked=0,sends=0;bool first=true,pending=false,finished=false,dropped=false;uint64_t ackAt=0;uint16_t outstanding=0,readAt=0;};
Conn conns[4];unsigned clients=1,ackDelay=0,readDelay=0,resets=0;
bool noAck=false,noTx=false,halfClosed=false;std::string request;
uint64_t beginUs=0;
struct AirSocket{
 static uint8_t status(uint8_t s){return s<clients?(halfClosed?SnSR::CLOSE_WAIT:SnSR::ESTABLISHED):SnSR::LISTEN;}
 static bool open(uint8_t){return false;}
 static bool command(uint8_t s,uint8_t command){auto& c=conns[s];if(command==Sock_CLOSE)c.dropped=true;if(command==Sock_DISCON)c.finished=true;return true;}
 static int8_t sent(uint8_t s){auto& c=conns[s];if(noAck||!c.pending||clockUs<c.ackAt)return 0;c.pending=false;c.acked+=c.outstanding;return 1;}
 static bool drained(uint8_t s){return !conns[s].pending;}
 static uint16_t read(uint8_t s,uint8_t* dst,uint16_t cap){auto& c=conns[s];size_t limit=readDelay?min<size_t>(request.size(),(clockUs-beginUs)/(readDelay*1000)):request.size();auto n=min<size_t>(cap,limit-c.readAt);memcpy(dst,request.data()+c.readAt,n);c.readAt+=n;return n;}
 static bool send(uint8_t s,const uint8_t*,uint16_t n){if(noTx)return false;auto& c=conns[s];assert(!c.pending);++c.sends;c.outstanding=c.first?0:n;c.first=false;c.submitted+=c.outstanding;c.pending=true;c.ackAt=clockUs+ackDelay*1000;return true;}
};
void resetNetwork(){++resets;}
uint16_t header(uint16_t,bool=false){return 170;}
uint16_t jsonResponse(){return 400;}
#include "http.inc"
unsigned tests=0;
void run(uint64_t startMs,const char* route,unsigned delay,unsigned count,bool expectOK,unsigned trickle=0,bool held=false,bool unavailable=false,bool half=false){
 clockUs=startMs*1000;beginUs=clockUs;clients=count;ackDelay=delay;readDelay=trickle;noAck=held;noTx=unavailable;halfClosed=half;resets=0;views.count=0;
 for(auto& c:conns)c=Conn{};for(auto& p:peers)p=HttpPeer{};nextSocket=0;listener=MAX_SOCK_NUM;world.begin(millis());
 request=std::string("GET ")+route+" HTTP/1.1\r\nHost: test\r\n\r\n";
 for(;;){bool done=true;for(unsigned i=0;i<clients;i++)done&=conns[i].finished||conns[i].dropped;if(done)break;
   assert(clockUs-beginUs<14000000);clockUs+=250;httpTick();}
 assert(resets==0);for(unsigned i=0;i<clients;i++){auto& c=conns[i];assert(c.finished==expectOK);assert(c.dropped!=expectOK);
  if(expectOK&&!strcmp(route,"/")){assert(c.acked==sizeof(SITE_GZ));assert(c.sends==1+(sizeof(SITE_GZ)+511)/512);}}
 if(trickle)assert(clockUs-beginUs<=1510000);else if(held||unavailable)assert(clockUs-beginUs<=4020000);else assert(clockUs-beginUs<=12020000);
 if(expectOK&&!strcmp(route,"/"))assert(views.count==int(clients));
 ++tests;
 printf("%s delay=%u peers=%u start=%llu elapsed=%llu complete=%d\n",route,delay,count,(unsigned long long)startMs,(unsigned long long)((clockUs-beginUs)/1000),expectOK);
}
int main(){
 for(uint64_t start:{1000ULL,64000ULL,4294967000ULL})for(unsigned n:{1U,4U}){
  for(const char* route:{"/","/api","/life"})for(unsigned delay:{25U,125U,175U,250U,400U})run(start,route,delay,n,true);
  run(start,"/",550,n,false);run(start,"/",4500,n,false);run(start,"/",1,n,false,300);run(start,"/",1,n,false,0,true);run(start,"/",1,n,false,0,false,true);
  run(start,"/",250,n,true,0,false,false,true);
 }
 printf("PASS %u HTTP deadline/held-peer/wrap/half-close cases\n",tests);
}
