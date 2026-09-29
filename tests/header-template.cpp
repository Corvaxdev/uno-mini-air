#include <stdint.h>
#include <cstring>
#include <string>
#include <cassert>
#include <iostream>
#include "../firmware/UnoMiniEthernet/AsciiWriter.h"
uint8_t buffer[512];const uint16_t BODY_AT=176;
bool freshCO2;uint16_t co2Ppm;
bool co2Fresh(){return freshCO2;}
struct {bool valid;uint32_t pressurePa;bool fresh(){return valid;}} barometer;
uint16_t header(uint16_t length,uint8_t html=0) {
  AsciiWriter w((char*)buffer,html?sizeof(buffer):BODY_AT);
  w.text(PSTR("HTTP/1.1 200 OK\r\nContent-Type: "));
  w.text(html==2?PSTR("application/octet-stream"):html?PSTR("text/html; charset=utf-8\r\nContent-Encoding: gzip"):PSTR("application/json"));
  w.text(PSTR("\r\nCache-Control: no-store\r\nConnection: close\r\nX-Source: ATmega328P\r\nContent-Length: "));
  w.number(length);
  if(html==2) {
    // The snapshot already carries the next-step time at bytes28..29.
    // Optional sensor metadata keeps the 480-byte world compatible with old Terra tabs.
    w.text(PSTR("\r\nX-Air: "));w.number(co2Fresh()?co2Ppm:0);
    w.put(',');w.number(barometer.fresh()?barometer.pressurePa:0);
  }
  w.text(PSTR("\r\n\r\n"));return w.size();
}

int main(){unsigned count=0;
for(uint8_t mode=0;mode<3;mode++)for(uint16_t length:{0,480,65535})for(bool co2:{false,true})for(bool pressure:{false,true})for(uint16_t c:{0,400,5000,65535})for(uint32_t p:{0u,30000u,101325u,110000u,0xffffffffu}){
 memset(buffer,0xa5,sizeof buffer);freshCO2=co2;co2Ppm=c;barometer.valid=pressure;barometer.pressurePa=p;
 auto n=header(length,mode);assert(n>0&&n<(mode?512:176));std::string text((char*)buffer,n);
 assert(text.substr(text.size()-4)=="\r\n\r\n");assert(text.find("Content-Length: "+std::to_string(length)+"\r\n")!=std::string::npos);
 const std::string air="\r\nX-Air: "+std::to_string(co2?c:0)+","+std::to_string(pressure?p:0)+"\r\n";
 assert(mode==2?text.find(air)!=std::string::npos:text.find("X-Air:")==std::string::npos);
 for(unsigned i=n+1;i<512;i++)assert(buffer[i]==0xa5);++count;
}std::cout<<"PASS "<<count<<" header cases: freshness, zero/max values, body length, buffer guards\n";}
