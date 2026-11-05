// Il core Arduino dell'emulatore: orologio virtuale, random e Serial su stdin/stdout.
#include <Arduino.h>
#include <poll.h>
#include <unistd.h>
#include <chrono>
#include "emu.h"

// ---------------------------------------------------------------- tempo

// millis() non legge l'orologio del PC: il ciclo principale lo fa avanzare (tempo reale
// per --fast, oppure i passi del copione), così il gioco accelerato resta coerente.
static uint64_t gMs = 0;
uint32_t millis() { return (uint32_t)gMs; }
void emuTempoAvanza(uint32_t ms) { gMs += ms; }
uint64_t emuTempoMs() { return gMs; }
// delay() serve solo prima dei riavvii (WIPE YES, import): basta far passare il tempo
void delay(uint32_t ms) { gMs += ms; }

// ---------------------------------------------------------------- random

// LCG come quello dei test: riproducibile con --seed
static uint32_t gSeme = 12345;
void randomSeed(unsigned long seed) { gSeme = seed ? (uint32_t)seed : 1; }
static uint32_t prossimo() {
  gSeme = gSeme * 1664525u + 1013904223u;
  return gSeme >> 1;
}
long random(long howbig) { return howbig <= 0 ? 0 : (long)(prossimo() % (uint32_t)howbig); }
long random(long howsmall, long howbig) {
  return howsmall >= howbig ? howsmall : howsmall + random(howbig - howsmall);
}

// ---------------------------------------------------------------- Serial

SerialeHost Serial;
static std::string gIn;     // byte arrivati e non ancora letti dal firmware
static bool gStdin = true;  // false a fine file o se lo si spegne

size_t SerialeHost::write(uint8_t c) { return fputc(c, stdout) == EOF ? 0 : 1; }
size_t SerialeHost::write(const uint8_t *buf, size_t n) { return fwrite(buf, 1, n, stdout); }

void emuSerialeRiceve(const char *dati, size_t n) { gIn.append(dati, n); }
void emuSerialeStdin(bool attivo) { gStdin = attivo; }

bool emuSerialeLeggiStdin(int attesaMs) {
  if (!gStdin) return false;
  struct pollfd p = {0, POLLIN, 0};
  if (poll(&p, 1, attesaMs) <= 0) return false;
  char buf[4096];
  ssize_t n = read(0, buf, sizeof(buf));
  if (n <= 0) {  // fine file: niente più console (il gioco continua)
    gStdin = false;
    return false;
  }
  gIn.append(buf, (size_t)n);
  return true;
}

int SerialeHost::available() {
  return gIn.find('\n') == std::string::npos ? 0 : (int)gIn.size();
}

String SerialeHost::readStringUntil(char fine) {
  size_t i = gIn.find(fine);
  std::string riga = gIn.substr(0, i);
  gIn.erase(0, i == std::string::npos ? gIn.size() : i + 1);
  return String(riga);
}

// il protocollo PUT: dati grezzi dopo la riga di comando, attesi in tempo reale
size_t SerialeHost::readBytes(uint8_t *buf, size_t n) {
  auto fine = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  while (gIn.size() < n && std::chrono::steady_clock::now() < fine)
    if (!emuSerialeLeggiStdin(20) && !gStdin) break;
  size_t k = gIn.size() < n ? gIn.size() : n;
  memcpy(buf, gIn.data(), k);
  gIn.erase(0, k);
  return k;
}
