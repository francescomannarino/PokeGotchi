#pragma once
// Ritratto di un Pokémon: i soli campi dell'individuo, senza bisogni né timer.
// Serve alla collezione personale (blob NVS "col", vedi pet.h) e allo scambio (è il
// PetTrade, lo scambio non definisce un formato suo). Niente hardware: lo usano scheda,
// test, emulatore e PWA.
//
// FORMATO (interi little-endian, scritti byte per byte; RIT_BYTES = 50)
//   off  dim  campo
//    0    1   versione (RIT_VERSIONE)
//    1    1   lunghezza totale in byte, CRC compreso (≥ RIT_BYTES)
//    2    2   specie (int16, numero di Pokédex 1..151)
//    4    1   shiny (0/1)
//    5    4   ageMinutes (età in minuti di gioco: il livello si ricava da qui)
//    9    1   geneAtk (90..110)
//   10    1   geneDef
//   11    1   geneSpe
//   12    1   trAtk (allenamento 0..100)
//   13    1   trDef
//   14    1   trSpe
//   15    1   bond (legame 0..100)
//   16    2   medals (medaglie dell'individuo, MED_* di pet.h)
//   18   12   nick (soprannome, completato con NUL; "" = nome della specie)
//   30    4   otId (allenatore originale: 0 = io, se manca l'identità dell'allenatore)
//   34   12   otNome (nome dell'allenatore originale, completato con NUL)
//   46    4   crc (CRC32 IEEE, `saveCrc32` di savefile.h, dei byte che lo precedono)
//
// Versioni: i campi nuovi si aggiungono in coda, prima del CRC, alzando RIT_VERSIONE e la
// lunghezza. Chi legge usa la lunghezza del byte 1: da un ritratto più nuovo prende i
// campi che conosce e ignora il resto.
// tools/salvataggi.py decodifica lo stesso layout (TestSalvataggi lo confronta con questo
// commento).
#include <Arduino.h>

#define RIT_VERSIONE 1
#define RIT_BYTES 50
#define RIT_NOME 12  // byte di nick e otNome, NUL compreso

struct PetRitratto {
  int16_t specie = -1;
  bool shiny = false;
  uint32_t ageMinutes = 0;
  uint8_t geneAtk = 100, geneDef = 100, geneSpe = 100;
  uint8_t trAtk = 0, trDef = 0, trSpe = 0;
  uint8_t bond = 0;
  uint16_t medals = 0;
  char nick[RIT_NOME] = "";
  uint32_t otId = 0;
  char otNome[RIT_NOME] = "";
};

// Scrive il ritratto in `out`: RIT_BYTES byte, 0 se `max` non basta.
size_t ritrattoScrivi(const PetRitratto &r, uint8_t *out, size_t max);
// Legge un ritratto da `dati` (al massimo `len` byte). Restituisce i byte che occupa (la
// sua lunghezza), 0 se non si legge: troppo corto, versione 0, CRC errato o specie fuori
// intervallo. Riporta geni, allenamento e legame negli intervalli validi.
size_t ritrattoLeggi(const uint8_t *dati, size_t len, PetRitratto *r);
