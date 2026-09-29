#include <cassert>
#include <initializer_list>
#include <cstdio>
#include <cstring>
#ifndef AIR_HEADER
#define AIR_HEADER "../firmware/UnoMiniEthernet/AirSocket.h"
#endif
#include AIR_HEADER
int main() {
  // Extra storage makes corruption observable without invoking host UB.
  uint8_t memory[2112]; std::memset(memory,0xA5,sizeof memory);
  W5100.first=0;W5100.second=800;
  uint16_t n=AirSocket::read(0,memory,128);
  std::printf("arrival_between_reads: returned=%u copied=%u register_reads=%u sensor_guard=%u\n",
    n,W5100.copied,W5100.calls,memory[512]);
#ifdef EXPECT_BUG
  assert(n==800&&W5100.copied==800&&memory[512]!=0xA5);
  std::puts("CONFIRMED: original header exceeds both the requested 128 B and the physical 512 B buffer");
#else
  assert(n==0&&W5100.copied==0&&memory[512]==0xA5);
  unsigned cases=0;
  for(uint16_t cap: {uint16_t(0),uint16_t(1),uint16_t(22),uint16_t(128),uint16_t(512)})
  for(uint16_t first=0;first<=2048;++first)
  for(uint16_t later: {uint16_t(0),uint16_t(127),uint16_t(128),uint16_t(511),uint16_t(512),uint16_t(513),uint16_t(800),uint16_t(2048)}) {
    W5100=ChipMock{};W5100.first=first;W5100.second=later;
    std::memset(memory,0xA5,sizeof memory);
    n=AirSocket::read(0,memory,cap);
    const uint16_t expected=first<cap?first:cap;
    assert(n==expected&&W5100.copied==expected&&W5100.rx==expected);
    assert(W5100.calls==2);
    for(unsigned i=cap;i<sizeof memory;++i)assert(memory[i]==0xA5);
    ++cases;
  }
  W5100=ChipMock{};W5100.unstable=true;
  assert(AirSocket::read(0,memory,128)==0&&W5100.copied==0&&W5100.calls==5);++cases;
  W5100=ChipMock{};W5100.busy=true;
  assert(AirSocket::read(0,memory,128)==0&&W5100.copied==0&&W5100.calls==0);++cases;
  std::printf("PASS %u bounded-read cases, including growth, shrink, unstable counters and a busy command\n",cases);
#endif
}
