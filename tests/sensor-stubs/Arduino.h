#pragma once
#include <cstdint>
#include <algorithm>
using std::max;
extern uint32_t testMillis;
inline uint32_t millis(){return testMillis;}
inline void delayMicroseconds(unsigned int){}
