#pragma once
#include <stdint.h>
#include <string.h>
#ifdef __AVR__
#include <avr/pgmspace.h>
#else
#define PSTR(x) x
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define strcmp_P strcmp
#define strncmp_P strncmp
#endif

static const char HTTP_ROUTES[8][25] PROGMEM={
  "GET / HTTP/1.1","GET /index.html HTTP/1.1",
  "GET /api HTTP/1.1","GET /terra HTTP/1.1","GET /lab HTTP/1.1",
  "GET /devlog HTTP/1.1","GET /life HTTP/1.1","PUT /life/000 HTTP/1.1"
};
// Strict bounded routes. One bodyless PUT; cross-origin browsers require CORS preflight.
struct HttpRequest {
  enum Route : uint8_t { MISSING, PAGE, API, GUIDE, LAB, WORLD, STAMP };
  uint16_t bytes=0,point=0;
  // 0..79: request-line position; 254/255: empty/nonempty header line.
  // Header parsing needs only emptiness, not a saturating character counter.
  uint8_t used=0,mask=255;
  void reset() { bytes=0;point=0;used=0;mask=255; }
  // -1 reject, 0 incomplete, 1 complete. Absolute deadline is owned by the peer.
  int8_t feed(char c) {
    if(++bytes>2048 || (!c) || (uint8_t(c)<32 && c!='\r' && c!='\n' && c!='\t'))return -1;
    if(c=='\r')return 0;
    if(c=='\n') {
      if(used<254) {
        for(uint8_t i=0;i<8;++i)
          if((mask&(1<<i)) && pgm_read_byte(&HTTP_ROUTES[i][used]))mask&=~(1<<i);
      }
      else if(used==254)return 1;
      used=254;return 0;
    }
    if(used<254) {
      if(used>=79)return -1;
      // Retain only matching-route bits. No per-connection request string in RAM.
      for(uint8_t i=0;i<8;++i)if(mask&(1<<i)) {
        if(i==7 && used>=10 && used<=12) {
          uint8_t v=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:255;
          if(v>15 || (used==10 && v>11))mask&=127;else point=(point<<4)|v;
          continue;
        }
        char expected=pgm_read_byte(&HTTP_ROUTES[i][used]);
        bool http10=c=='0' && expected=='1' && !pgm_read_byte(&HTTP_ROUTES[i][used+1]);
        if(!expected || (c!=expected && !http10))mask&=~(1<<i);
      }
      ++used;
    } else used=255;
    return 0;
  }
  Route route() const {
    // The report and devlog share one compressed Flash document.
    return mask&3?PAGE:mask&4?API:mask&8?GUIDE:mask&48?LAB:mask&64?WORLD:mask&128?STAMP:MISSING;
  }
};
