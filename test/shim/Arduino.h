// Shim minimo di Arduino.h per eseguire la logica del firmware sull'host. Copre solo
// ciò che usano pet.cpp / i18n.cpp. L'orologio è finto e controllabile perché i test
// siano deterministi.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- orologio finto -----------------------------------------------------------
uint32_t millis();
void mockSetMillis(uint32_t ms);
void mockAdvanceMillis(uint32_t ms);

// ---- random deterministico (stessa semantica di Arduino: [0, howbig) ) -------
long random(long howbig);
long random(long howsmall, long howbig);
void randomSeed(unsigned long seed);
// forza il/i prossimo/i random() a un valore fisso (per i test)
void mockForceRandom(long value);   // tutti i random() restituiscono value
void mockClearForcedRandom();

// ---- Serial ----------------------------------------------------------------
struct MockSerial {
  int printf(const char *fmt, ...);
  void print(const char *s);
  void println(const char *s);
  void begin(unsigned long);
};
extern MockSerial Serial;
void mockSerialSilence(bool quiet);

// ---- utilità ---------------------------------------------------------------
template <class T> static inline T min(T a, T b) { return a < b ? a : b; }
template <class T> static inline T max(T a, T b) { return a > b ? a : b; }
