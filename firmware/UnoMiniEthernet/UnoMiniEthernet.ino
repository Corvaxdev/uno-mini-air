#include <stdint.h>
#include <SPI.h>
#include <MiniW5500.h>
#include <MiniBMP180.h>
#include <MiniSHT.h>
#include <MiniDSL.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include "AirSocket.h"
#include "HttpRequest.h"
#include "AsciiWriter.h"
#include "ViewJournal.h"
#include "MhZ14Pwm.h"
#include "I2cRecovery.h"
#include "site_gz.h"
#include "Terra.h"
#include "NetworkConfig.h"
Terra world;


static_assert(BUFFER_LENGTH==22,"Use the project-local Wire library.");

// UNO Mini + W5500: CS D10, MOSI D11, MISO D12, SCK D13.
// Trema BMP085/BMP180: SDA D18/SDA, SCL D19/SCL, module 5V/GND.
// One real sample every 5 s; bounded I2C and cooperative conversion waits.

uint8_t buffer[512]; // One shared RX / JSON / Flash transmission workspace.
const uint16_t BODY_AT=176;
struct HttpPeer {
  enum Phase : uint8_t { IDLE, READING, RESPONSE, PAGE_DATA, FINISH, CLOSING, OPENING, LISTENING, RELEASING, WORLD_DATA };
  HttpRequest request;
  uint32_t at=0;
  Phase phase=IDLE;
  HttpRequest::Route route=HttpRequest::MISSING;
  bool pending=false;
};
HttpPeer peers[MAX_SOCK_NUM];
bool ready=false;
uint32_t linkAt=0,retryAt=0;
uint8_t nextSocket=0,listener=MAX_SOCK_NUM;
// Optiboot normally disables WDT; also handle application entry with WDT active.
void earlyWatchdogOff() __attribute__((naked,used,section(".init3")));
void earlyWatchdogOff() { MCUSR=0;wdt_disable(); }
MiniBMP180 barometer;
MiniSHT sht;
MiniDSL light;
struct I2cPins {
  // UNO Mini ATmega328P: D18/SDA=PC4, D19/SCL=PC5. Constant bit operations
  // compile to atomic SBI/CBI; release DDR before enabling the weak pull-up.
  static bool sdaHigh(){return PINC&_BV(PC4);}
  static bool sclHigh(){return PINC&_BV(PC5);}
  static void sdaLow(){PORTC&=~_BV(PC4);DDRC|=_BV(PC4);}
  static void sclLow(){PORTC&=~_BV(PC5);DDRC|=_BV(PC5);}
  static void sdaRelease(){DDRC&=~_BV(PC4);PORTC|=_BV(PC4);}
  static void sclRelease(){DDRC&=~_BV(PC5);PORTC|=_BV(PC5);}
  static void end(){Wire.end();}
  static void begin(){Wire.begin();Wire.setClock(100000);Wire.setWireTimeout(3000,true);}
};
I2cRecovery<I2cPins> i2cRecovery;
MhZ14Capture co2Capture;
uint8_t co2Sequence=0;
uint16_t co2Ppm=UINT16_MAX;
uint32_t co2At=0;
bool co2Warmed=false;
// D5 = PD5 / PCINT21. No UART runtime is linked.
ISR(PCINT2_vect) { co2Capture.edge(PIND & _BV(PD5),micros()); }
bool co2Fresh() { return co2Warmed && co2Ppm!=UINT16_MAX && millis()-co2At<3500UL; }
void co2Tick() {
  uint32_t now=millis();
  if(!co2Warmed && now>=60000UL)co2Warmed=true;
  if(co2Sequence!=co2Capture.sequence) {
    uint8_t saved=SREG;cli();
    uint32_t high=co2Capture.highUs,period=co2Capture.periodUs;
    co2Sequence=co2Capture.sequence;SREG=saved;
    co2Ppm=mhZ14Decode(high,period);co2At=now;
  }
  if(now-co2At>=3500UL)co2Ppm=UINT16_MAX;

}

struct EepromStore {
  static bool ready() { return eeprom_is_ready(); }
  static uint8_t read(uint16_t a) { return eeprom_read_byte((const uint8_t*)a); }
  static void update(uint16_t a,uint8_t b) { eeprom_update_byte((uint8_t*)a,b); }
};
ViewJournal<EepromStore> views;

void viewTick() { views.tick(millis()); }

void sensorTick() {
  co2Tick();
  auto bus=i2cRecovery.tick(micros());
  if(bus!=I2cRecovery<I2cPins>::READY && bus!=I2cRecovery<I2cPins>::RECOVERED) {
    sht.invalidate();light.invalidate();barometer.invalidate();return;
  }
  light.tick();sht.tick();barometer.tick();
}
// Set NetworkConfig.h for your LAN. No DNS, DHCP or internet
// service is required for measurements, EEPROM or direct LAN HTTP.
void networkStart() {
  ready=false;
  // Stage fixed network configuration in the existing shared workspace.
  static const uint8_t config[] PROGMEM={
    AIR_MAC, AIR_IP_0,AIR_IP_1,AIR_IP_2,AIR_IP_3, AIR_GATEWAY, AIR_NETMASK
  };
  if(!W5100.init()) {retryAt=millis();return;}
  SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
  memcpy_P(buffer,config,sizeof(config));
  W5100.setMACAddress(buffer);
  W5100.setIPAddress(buffer+6);
  W5100.setGatewayIp(buffer+10);
  W5100.setSubnetMask(buffer+14);
  for(uint8_t s=0;s<8;++s) {
    W5100.writeSnCR(s,Sock_CLOSE);
    W5100.writeSnRX_SIZE(s,W5100.SSIZE/1024);
    W5100.writeSnTX_SIZE(s,W5100.SSIZE/1024);
  }
  W5100.setRetransmissionTime(2000);W5100.setRetransmissionCount(3);
  SPI.endTransaction();
  for(uint8_t s=0;s<MAX_SOCK_NUM;++s) {peers[s].phase=HttpPeer::IDLE;peers[s].pending=false;}
  listener=MAX_SOCK_NUM;
  retryAt=millis();
}
void networkTick() {
  uint32_t now=millis();if(now-linkAt<1000UL)return;linkAt=now;
  SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
  uint8_t version=W5100.readVERSIONR_W5500(),phy=W5100.read(0x2E);
  uint8_t ip[4];W5100.getIPAddress(ip);
  SPI.endTransaction();
  if(version!=4 || ip[0]!=AIR_IP_0 || ip[1]!=AIR_IP_1 || ip[2]!=AIR_IP_2 || ip[3]!=AIR_IP_3) {
    ready=false;
    if(now-retryAt>=3000UL) {networkStart();}
    return;
  }
  bool link=phy&1;
  if(link==ready)return;
  ready=link;
  if(!link) {
    for(uint8_t s=0;s<MAX_SOCK_NUM;++s) {
      AirSocket::command(s,Sock_CLOSE);peers[s].phase=HttpPeer::IDLE;peers[s].pending=false;
    }
    listener=MAX_SOCK_NUM;
  }
}
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
__attribute__((noinline)) uint16_t jsonResponse() {
  char* body=(char*)buffer+BODY_AT;
  AsciiWriter w(body,sizeof(buffer)-BODY_AT);
  {
    bool haveSht=sht.fresh(),haveBmp=barometer.fresh();
    w.text(PSTR("{\"demo\":false,\"temp\":"));
    if(haveSht || haveBmp)w.tenths(haveSht?sht.temperature10:barometer.temperature10);else w.text(PSTR("null"));
    w.text(PSTR(",\"hum\":"));if(haveSht)w.tenths(sht.humidity10);else w.text(PSTR("null"));
    w.text(PSTR(",\"pressure_pa\":"));if(haveBmp)w.number(barometer.pressurePa);else w.text(PSTR("null"));
    w.text(PSTR(",\"light_lux\":"));if(light.fresh())w.number(light.lux);else w.text(PSTR("null"));
    w.text(PSTR(",\"co2_ppm\":"));if(co2Fresh())w.number(co2Ppm);else w.text(PSTR("null"));
    w.text(PSTR(",\"temp_source\":"));w.text(haveSht?PSTR("\"sht\""):haveBmp?PSTR("\"bmp085/180\""):PSTR("null"));
    w.text(PSTR(",\"uptime\":"));w.number(millis()/1000UL);
    w.text(PSTR(",\"firmware\":\"" SITE_VERSION "\",\"transport\":\"w5500\""));
    w.text(PSTR(",\"views\":"));w.number(views.total);
  }
  w.put('}');uint16_t n=w.size();if(!n)return 0;
  uint16_t h=header(n);if(!h)return 0;
  memmove(buffer+h,body,n);return h+n;
}
void resetNetwork() {
  SPI.beginTransaction(SPI_ETHERNET_SETTINGS);W5100.writeMR(0x80);SPI.endTransaction();
  ready=false;retryAt=millis()-3000UL;
}
void dropPeer(uint8_t s) {
  if(AirSocket::command(s,Sock_CLOSE)) {
    peers[s].phase=HttpPeer::RELEASING;peers[s].at=millis();peers[s].pending=false;
  } else if(millis()-peers[s].at>6000UL)resetNetwork();
}
void httpTick() {
  static uint32_t pollAt=0;
  uint32_t us=micros();if(us-pollAt<250UL)return;pollAt=us;
  uint8_t s=nextSocket;nextSocket=(nextSocket+1)%MAX_SOCK_NUM;
  HttpPeer& p=peers[s];uint32_t now=millis();uint8_t status=AirSocket::status(s);
  if(p.pending) {
    int8_t sent=AirSocket::sent(s);
    if(sent<0) {dropPeer(s);return;}
    if(sent>0){p.pending=false;p.at=now;}
  }
  if(status==SnSR::CLOSED) {
    // A cleared command register is not proof that OPEN has completed.
    if(p.phase==HttpPeer::OPENING || p.phase==HttpPeer::LISTENING) {
      if(now-p.at>1000UL)resetNetwork();return;
    }
    if(listener==s)listener=MAX_SOCK_NUM;
    if(p.phase!=HttpPeer::IDLE && p.phase!=HttpPeer::RELEASING) {
      p.phase=HttpPeer::RELEASING;p.at=now;p.pending=false;
    }
    // The original driver allows 250 us before reopening. Cooperatively allow
    // at least 1 ms after CLOSE; do not busy-wait or reissue a pending command.
    if(p.phase==HttpPeer::RELEASING && now-p.at<1)return;
    p.phase=HttpPeer::IDLE;p.pending=false;
    // Keep one listener; accept another connection on a free socket.
    if(listener==MAX_SOCK_NUM && AirSocket::open(s)) {
      listener=s;p.phase=HttpPeer::OPENING;p.at=now;
    }
    return;
  }
  if(status==SnSR::INIT) {
    if(now-p.at>1000UL)resetNetwork();
    else if(p.phase==HttpPeer::OPENING && AirSocket::command(s,Sock_LISTEN))p.phase=HttpPeer::LISTENING;
    return;
  }
  if(status==SnSR::LISTEN) {p.phase=HttpPeer::IDLE;return;}
  if(listener==s)listener=MAX_SOCK_NUM;
  if(p.phase==HttpPeer::IDLE || p.phase==HttpPeer::OPENING || p.phase==HttpPeer::LISTENING) {
    p.request.reset();p.phase=HttpPeer::READING;p.at=now;p.pending=false;
  }
  if(p.phase==HttpPeer::CLOSING) {
    if(now-p.at>=200UL && AirSocket::command(s,Sock_CLOSE)) {
      p.phase=HttpPeer::RELEASING;p.at=now;
    }
    if(now-p.at>1000UL)resetNetwork();
    return;
  }
  if(p.phase==HttpPeer::RELEASING) {
    if(now-p.at>1000UL)resetNetwork();return;
  }
  // Reuse the consumed header byte count as a 16-bit response start clock.
  // Acknowledged blocks refresh the idle limit; total lifetime stays bounded.
  if(p.phase==HttpPeer::READING?now-p.at>1500UL:
      now-p.at>4000UL || uint16_t(now-p.request.bytes)>12000U) {dropPeer(s);return;}
  if(status!=SnSR::ESTABLISHED && status!=SnSR::CLOSE_WAIT)return;
  if(p.pending)return;
  if(p.phase==HttpPeer::READING) {
    uint16_t n=AirSocket::read(s,buffer,128);
    for(uint16_t i=0;i<n;++i) {
      int8_t result=p.request.feed(buffer[i]);
      if(result<0) {dropPeer(s);return;}
      if(result>0) {
        p.route=p.request.route();p.phase=HttpPeer::RESPONSE;p.at=now;p.request.bytes=uint16_t(now);return;
      }
    }
    if(!n && status==SnSR::CLOSE_WAIT)dropPeer(s);
    return;
  }
  if(p.phase==HttpPeer::RESPONSE) {
    uint16_t n;
    if(p.route==HttpRequest::PAGE || p.route==HttpRequest::LAB || p.route==HttpRequest::GUIDE)n=header(sizeof(SITE_GZ),true);
    else if(p.route==HttpRequest::WORLD) {
      if(world.busy())return;
      n=header(Terra::WIRE_SIZE,2);
    } else if(p.route==HttpRequest::STAMP) {
      strcpy_P((char*)buffer,world.accepts(now)?PSTR("HTTP/1.1 202 Accepted\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"):PSTR("HTTP/1.1 429 Too Many Requests\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"));n=strlen((char*)buffer);
    }
    else if(p.route==HttpRequest::API)n=jsonResponse();
    else {strcpy_P((char*)buffer,PSTR("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"));n=strlen((char*)buffer);}
    if(!n) {dropPeer(s);return;}
    if(AirSocket::send(s,buffer,n)) {
      if(p.route==HttpRequest::STAMP && world.accepts(now))world.enqueue(p.request.point,now);
      p.pending=true;p.phase=(p.route==HttpRequest::PAGE || p.route==HttpRequest::LAB || p.route==HttpRequest::GUIDE)?HttpPeer::PAGE_DATA:p.route==HttpRequest::WORLD?HttpPeer::WORLD_DATA:HttpPeer::FINISH;
    }
    return;
  }
  if(p.phase==HttpPeer::WORLD_DATA) {
    if(world.busy())return;
    world.snapshot(buffer,now);
    if(AirSocket::send(s,buffer,Terra::WIRE_SIZE)){p.pending=true;p.phase=HttpPeer::FINISH;}
    return;
  }
  if(p.phase==HttpPeer::PAGE_DATA) {
    // GET leaves point zero; HTML routes reuse it after parsing as their offset.
    // STAMP keeps the coordinate and never enters PAGE_DATA.
    uint16_t& pos=p.request.point;
    uint16_t length=sizeof(SITE_GZ);
    const uint8_t* asset=SITE_GZ;
    uint16_t n=min(uint16_t(sizeof(buffer)),uint16_t(length-pos));
    memcpy_P(buffer,asset+pos,n);
    if(AirSocket::send(s,buffer,n)) {
      pos+=n;p.pending=true;if(pos==length)p.phase=HttpPeer::FINISH;
    }
    return;
  }
  if(p.phase==HttpPeer::FINISH && AirSocket::drained(s) && AirSocket::command(s,Sock_DISCON)) {
    if(p.route==HttpRequest::PAGE || p.route==HttpRequest::LAB || p.route==HttpRequest::GUIDE)views.increment();
    p.phase=HttpPeer::CLOSING;p.at=now;
  }
}
void setup() {
  pinMode(5,INPUT);digitalWrite(5,LOW);
  PCMSK2|=_BV(PCINT21);PCIFR=_BV(PCIF2);PCICR|=_BV(PCIE2);
  I2cPins::begin();
  views.begin(millis());
  world.begin(millis());
  digitalWrite(10,HIGH); pinMode(10,OUTPUT); SPI.begin();
  // Exhaustive register tests remain in the dedicated W5500Probe sketch.
  networkStart();
  wdt_enable(WDTO_8S);
}
void loop() {
  sensorTick();
  viewTick();
  networkTick();
  uint32_t now=millis();
  if(world.due(now))world.environment(sht.temperature10,sht.humidity10,light.lux,(sht.fresh()?3:0)|(light.fresh()?4:0),co2Fresh()?co2Ppm:0,barometer.fresh()?barometer.pressurePa:0);
  world.tick(now);
  if(ready)httpTick();
  wdt_reset(); // A stalled sensor/network call cannot feed the watchdog.
}
