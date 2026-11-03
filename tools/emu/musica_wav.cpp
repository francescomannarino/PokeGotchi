// `make musica`: un WAV per brano (mono, 16 bit, 32 kHz come la scheda) suonato da musica.h,
// da ascoltare contro l'originale. Battaglie: intro, un giro e 5 s del giro successivo
// (si sente la ripresa del ciclo); vittorie: fino alla fine più 1 s. Stampa anche il costo
// del motore sul PC, in campioni al secondo.
//
//   ./build/musica_wav CARTELLA [fs]
#include "../../musica.h"
#include "../../musica_dati.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <vector>

static void scrivi32(FILE *f, uint32_t v) {
  uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
  fwrite(b, 1, 4, f);
}
static void scrivi16(FILE *f, uint16_t v) {
  uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
  fwrite(b, 1, 2, f);
}

static bool salvaWav(const std::string &nome, const std::vector<int16_t> &v, uint32_t fs) {
  FILE *f = fopen(nome.c_str(), "wb");
  if (!f) return false;
  uint32_t dati = (uint32_t)v.size() * 2;
  fwrite("RIFF", 1, 4, f);
  scrivi32(f, 36 + dati);
  fwrite("WAVEfmt ", 1, 8, f);
  scrivi32(f, 16);
  scrivi16(f, 1);       // PCM
  scrivi16(f, 1);       // mono
  scrivi32(f, fs);
  scrivi32(f, fs * 2);
  scrivi16(f, 2);
  scrivi16(f, 16);
  fwrite("data", 1, 4, f);
  scrivi32(f, dati);
  for (int16_t x : v) scrivi16(f, (uint16_t)x);
  return fclose(f) == 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "uso: musica_wav CARTELLA [fs]\n");
    return 2;
  }
  uint32_t fs = argc > 2 ? (uint32_t)atol(argv[2]) : 32000;
  const uint32_t BLOCCO = 512;
  double secondi = 0;
  uint64_t campioni = 0;
  for (uint8_t b = 0; b < MUS_COUNT; b++) {
    musicaInizia(fs);
    musicaVolume(10);
    musicaSuona(b);
    const MusicaDiag &g = musicaDiagnostica();
    std::vector<int16_t> v;
    uint32_t coda = 0, codaMax = MUSICA_BRANI[b].ripeti ? 5 * fs : fs;
    clock_t t0 = clock();
    // fino al secondo sound_loop 0 (intro + un giro) o alla fine, poi la coda
    while (coda < codaMax && v.size() < 600u * fs) {
      size_t n = v.size();
      v.resize(n + BLOCCO);
      musicaRiempi(&v[n], BLOCCO);
      if (MUSICA_BRANI[b].ripeti ? g.salti[0] >= 2 : !musicaAttiva()) coda += BLOCCO;
    }
    secondi += (double)(clock() - t0) / CLOCKS_PER_SEC;
    campioni += v.size();
    std::string nome = std::string(argv[1]) + "/" + MUSICA_BRANI[b].nome + ".wav";
    if (!salvaWav(nome, v, fs)) {
      fprintf(stderr, "non scritto: %s\n", nome.c_str());
      return 1;
    }
    printf("%-34s %6.1f s, picco %5d\n", nome.c_str(), (double)v.size() / fs, (int)g.picco);
  }
  if (secondi > 0)
    printf("costo sul PC: %.1f milioni di campioni al secondo (%.0f volte il tempo reale a %u Hz)\n",
           campioni / secondi / 1e6, campioni / secondi / fs, (unsigned)fs);
  return 0;
}
