// Arduino.h per l'emulatore su PC: il core che il firmware e Arduino_GFX si aspettano.
// Orologio, random e Serial sono quelli dell'emulatore (arduino_host.cpp): `millis()`
// segue il tempo reale moltiplicato per `--fast`, oppure il copione in `--headless`.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // strlcpy: glibc >= 2.38 e macOS
#include <math.h>
#include <time.h>
#include <string>
#include "pgmspace.h"
#include "Print.h"
#include "WString.h"

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#define IRAM_ATTR

// ---- tempo e random (stessa semantica di Arduino: random(n) in [0, n) ) ----
uint32_t millis();
void delay(uint32_t ms);
long random(long howbig);
long random(long howsmall, long howbig);
void randomSeed(unsigned long seed);

static inline void *ps_malloc(size_t n) { return malloc(n); }  // niente PSRAM: è RAM

template <class T> static inline T min(T a, T b) { return a < b ? a : b; }
template <class T> static inline T max(T a, T b) { return a > b ? a : b; }

// ---- Serial: stdout in uscita, stdin (o il copione) in entrata ----
class SerialeHost : public Print {
public:
  void begin(unsigned long) {}
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buf, size_t n) override;
  using Print::write;
  // byte pronti; 0 finché la riga in arrivo non è completa (la console legge righe)
  int available();
  String readStringUntil(char fine);
  size_t readBytes(uint8_t *buf, size_t n);  // aspetta al massimo setTimeout() ms
  void setTimeout(unsigned long ms) { timeoutMs = ms; }
  void setTxTimeoutMs(uint32_t) {}
  void flush() { fflush(stdout); }

private:
  unsigned long timeoutMs = 1000;
};
extern SerialeHost Serial;
