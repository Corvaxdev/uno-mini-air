#pragma once
#include <stdint.h>

// Single-controller bus. NXP UM10204 3.1.16: up to nine clocks for stuck SDA.
// Each tick is bounded and leaves interrupts enabled. Io must use open-drain
// GPIO (drive LOW or release); never drive a bus line HIGH. A held-low SCL or
// SDA after nine clocks needs hardware repair/reset; retry only every 5 s.
template<class Io> class I2cRecovery {
public:
  enum Result : uint8_t { READY, BLOCKED, RECOVERED, FAILED };
  Result tick(uint32_t now) {
    switch(state) {
    case IDLE:
      if(Io::sdaHigh() && Io::sclHigh())return READY;
      state=QUALIFY;at=now;break;
    case QUALIFY:
      if(Io::sdaHigh() && Io::sclHigh()){state=IDLE;return READY;}
      if(uint32_t(now-at)>=3000UL)start(now);
      break;
    case WAIT_CLOCK:
      if(!Io::sclHigh()) {if(expired(now))return fail(now);break;}
      if(Io::sdaHigh())return finish();
      Io::sclLow();state=CLOCK_LOW;at=now;break;
    case CLOCK_LOW:
      if(uint32_t(now-at)>=5){Io::sclRelease();state=WAIT_RISE;at=now;}
      break;
    case WAIT_RISE:
      if(Io::sclHigh()){state=CLOCK_HIGH;at=now;}
      else if(expired(now))return fail(now);
      break;
    case CLOCK_HIGH:
      if(uint32_t(now-at)<5)break;
      ++pulses;
      if(Io::sdaHigh()) {
        Io::sclLow();Io::sdaLow();state=STOP_LOW;at=now;
      } else if(pulses==9)return fail(now);
      else {Io::sclLow();state=CLOCK_LOW;at=now;}
      break;
    case STOP_LOW:
      if(uint32_t(now-at)>=5){Io::sclRelease();state=STOP_WAIT;at=now;}
      break;
    case STOP_WAIT:
      if(Io::sclHigh()){state=STOP_HIGH;at=now;}
      else if(expired(now))return fail(now);
      break;
    case STOP_HIGH:
      if(uint32_t(now-at)>=5){Io::sdaRelease();state=VERIFY;at=now;}
      break;
    case VERIFY:
      if(uint32_t(now-at)<5)break;
      if(Io::sdaHigh() && Io::sclHigh())return finish();
      return fail(now);
    case COOLDOWN:
      if(uint32_t(now-at)>=5000000UL)start(now);
      break;
    }
    return BLOCKED;
  }
private:
  enum State : uint8_t { IDLE, QUALIFY, WAIT_CLOCK, CLOCK_LOW, WAIT_RISE,
    CLOCK_HIGH, STOP_LOW, STOP_WAIT, STOP_HIGH, VERIFY, COOLDOWN };
  uint32_t at=0;
  State state=IDLE;
  uint8_t pulses=0;
  bool expired(uint32_t now) const {return uint32_t(now-at)>=3000UL;}
  void start(uint32_t now) {
    Io::end();Io::sdaRelease();Io::sclRelease();
    pulses=0;state=WAIT_CLOCK;at=now;
  }
  Result finish() {Io::begin();state=IDLE;return RECOVERED;}
  Result fail(uint32_t now) {
    Io::sdaRelease();Io::sclRelease();state=COOLDOWN;at=now;return FAILED;
  }
};
