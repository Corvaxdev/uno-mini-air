#include "../firmware/UnoMiniEthernet/HttpRequest.h"
#include <cassert>
#include <cstdio>
#include <string>
int main(){
 for(auto path:{"/","/index.html","/api","/terra","/lab","/devlog","/life"}){HttpRequest r;std::string s="GET "+std::string(path)+" HTTP/1.1\r\nHost: test\r\n\r\n";int result=0;for(char c:s)result=r.feed(c);assert(result==1&&r.route()!=HttpRequest::MISSING);}
 for(unsigned v=0;v<4096;v++){HttpRequest r;char s[80];snprintf(s,sizeof(s),"PUT /life/%03x HTTP/1.1\r\n\r\n",v);for(char* c=s;*c;c++)r.feed(*c);assert((r.route()==HttpRequest::STAMP)==(v<3072));if(v<3072)assert(r.point==v);}
 HttpRequest r;std::string s="GET /guide HTTP/1.1\r\n\r\n";for(char c:s)r.feed(c);assert(r.route()==HttpRequest::MISSING);puts("PASS seven GET routes, 4096 action codes, retired Guide route");
}