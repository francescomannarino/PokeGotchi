#include "ritratto.h"
#include "savefile.h"
#include <string.h>

static void scriviLE(uint8_t *p, uint32_t v, uint8_t n) {
  for (uint8_t i = 0; i < n; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static uint32_t leggiLE(const uint8_t *p, uint8_t n) {
  uint32_t v = 0;
  for (uint8_t i = 0; i < n; i++) v |= (uint32_t)p[i] << (8 * i);
  return v;
}

size_t ritrattoScrivi(const PetRitratto &r, uint8_t *out, size_t max) {
  if (max < RIT_BYTES) return 0;
  memset(out, 0, RIT_BYTES);
  out[0] = RIT_VERSIONE;
  out[1] = RIT_BYTES;
  scriviLE(out + 2, (uint16_t)r.specie, 2);
  out[4] = r.shiny ? 1 : 0;
  scriviLE(out + 5, r.ageMinutes, 4);
  out[9] = r.geneAtk;
  out[10] = r.geneDef;
  out[11] = r.geneSpe;
  out[12] = r.trAtk;
  out[13] = r.trDef;
  out[14] = r.trSpe;
  out[15] = r.bond;
  scriviLE(out + 16, r.medals, 2);
  strncpy((char *)out + 18, r.nick, RIT_NOME - 1);  // l'ultimo byte resta NUL
  scriviLE(out + 30, r.otId, 4);
  strncpy((char *)out + 34, r.otNome, RIT_NOME - 1);
  scriviLE(out + 46, saveCrc32(out, 46), 4);
  return RIT_BYTES;
}

size_t ritrattoLeggi(const uint8_t *dati, size_t len, PetRitratto *r) {
  if (len < 2 || dati[0] == 0) return 0;
  size_t n = dati[1];
  if (n < RIT_BYTES || n > len) return 0;
  if (saveCrc32(dati, n - 4) != leggiLE(dati + n - 4, 4)) return 0;
  int16_t specie = (int16_t)leggiLE(dati + 2, 2);
  if (specie < 1 || specie > 151) return 0;
  auto gene = [](uint8_t g) -> uint8_t { return g < 90 ? 90 : (g > 110 ? 110 : g); };
  auto cento = [](uint8_t v) -> uint8_t { return v > 100 ? 100 : v; };
  PetRitratto x;
  x.specie = specie;
  x.shiny = dati[4] != 0;
  x.ageMinutes = leggiLE(dati + 5, 4);
  x.geneAtk = gene(dati[9]);
  x.geneDef = gene(dati[10]);
  x.geneSpe = gene(dati[11]);
  x.trAtk = cento(dati[12]);
  x.trDef = cento(dati[13]);
  x.trSpe = cento(dati[14]);
  x.bond = cento(dati[15]);
  x.medals = (uint16_t)leggiLE(dati + 16, 2);
  memcpy(x.nick, dati + 18, RIT_NOME - 1);
  memcpy(x.otNome, dati + 34, RIT_NOME - 1);
  x.otId = leggiLE(dati + 30, 4);
  *r = x;
  return n;
}
