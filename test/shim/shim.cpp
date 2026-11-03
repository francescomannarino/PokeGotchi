// Orologio, random, Serial e audio finti dei test (le Preferences sono in preferences.cpp).
#include "Arduino.h"
#include "../../hal.h"
#include <stdarg.h>

// ---------------- orologio finto ----------------
static uint32_t gMillis = 0;
uint32_t millis() { return gMillis; }
void mockSetMillis(uint32_t ms) { gMillis = ms; }
void mockAdvanceMillis(uint32_t ms) { gMillis += ms; }

// ---------------- random deterministico ----------------
// LCG proprio: riproducibile tra piattaforme (non dipende da rand()).
static uint32_t gSeed = 12345;
static bool gForced = false;
static long gForcedValue = 0;

void randomSeed(unsigned long seed) { gSeed = seed ? (uint32_t)seed : 1; }
void mockForceRandom(long value) { gForced = true; gForcedValue = value; }
void mockClearForcedRandom() { gForced = false; }

static uint32_t nextRand() {
  gSeed = gSeed * 1664525u + 1013904223u;
  return gSeed >> 1;
}

long random(long howbig) {
  if (howbig <= 0) return 0;
  if (gForced) return gForcedValue < howbig ? gForcedValue : howbig - 1;
  return (long)(nextRand() % (uint32_t)howbig);
}

long random(long howsmall, long howbig) {
  if (howsmall >= howbig) return howsmall;
  return howsmall + random(howbig - howsmall);
}

// ---------------- Serial ----------------
MockSerial Serial;
static bool gQuiet = true;
void mockSerialSilence(bool quiet) { gQuiet = quiet; }

int MockSerial::printf(const char *fmt, ...) {
  if (gQuiet) return 0;
  va_list ap;
  va_start(ap, fmt);
  int n = vprintf(fmt, ap);
  va_end(ap);
  return n;
}
void MockSerial::print(const char *s) { if (!gQuiet) fputs(s, stdout); }
void MockSerial::println(const char *s) { if (!gQuiet) printf("%s\n", s); }
void MockSerial::begin(unsigned long) {}

// ---------------- audio dell'HAL (stub) ----------------
static bool gAudioOn = true;
void halAudioBegin() {}
void halSfx(uint8_t) {}
void halAudioSetEnabled(bool on) { gAudioOn = on; }
bool halAudioEnabled() { return gAudioOn; }
