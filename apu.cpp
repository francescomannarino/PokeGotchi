#include "apu.h"
#include <math.h>
#include <string.h>

// APU del Game Boy (vedi apu.h). I comportamenti seguono la Pan Docs ("Audio details"); il
// passa-alto è quello del DMG (carica 0,999958 per ciclo a 4 MHz).

// ---------------------------------------------------------------- BLEP

// Impulso a banda limitata, 32 fasi × 16 coefficienti in Q15 (ogni fase somma a 32768,
// così l'integrale di un gradino vale esattamente il gradino). Taglio a 0,45·fs, finestra
// di Blackman su ±8 campioni, ritardo di 7 campioni. Si calcola una volta, all'avvio.
static const int FASI = 32, PRESE = 16;
static int16_t gNucleo[FASI][PRESE];
static bool gNucleoPronto = false;

static void calcolaNucleo() {
  const double PI = 3.14159265358979323846, TAGLIO = 0.9;  // 2 · 0,45
  for (int f = 0; f < FASI; f++) {
    double h[PRESE], somma = 0;
    for (int j = 0; j < PRESE; j++) {
      double t = j - 7 - (double)f / FASI;
      double x = PI * TAGLIO * t;
      double sinc = fabs(x) < 1e-9 ? 1.0 : sin(x) / x;
      double w = fabs(t) >= 8 ? 0 : 0.42 + 0.5 * cos(PI * t / 8) + 0.08 * cos(2 * PI * t / 8);
      h[j] = TAGLIO * sinc * w;
      somma += h[j];
    }
    int tot = 0, max = 0;
    for (int j = 0; j < PRESE; j++) {
      gNucleo[f][j] = (int16_t)lround(h[j] * 32768.0 / somma);
      tot += gNucleo[f][j];
      if (gNucleo[f][j] > gNucleo[f][max]) max = j;
    }
    gNucleo[f][max] += (int16_t)(32768 - tot);  // l'arrotondamento va sul coefficiente maggiore
  }
  gNucleoPronto = true;
}

// ---------------------------------------------------------------- avvio e tempo

static const uint8_t DUTY[4][8] = {
  {0, 0, 0, 0, 0, 0, 0, 1}, {1, 0, 0, 0, 0, 0, 0, 1},
  {1, 0, 0, 0, 0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 0},
};
static const uint32_t SEQ_CICLI = APU_CLOCK / 512;

void Apu::inizia(uint32_t fs) {
  if (!gNucleoPronto) calcolaNucleo();
  if (fs < 8000) fs = 8000;
  if (fs > 96000) fs = 96000;
  gFs = fs;
  // il condensatore del DMG: carica 0,999958 a ogni ciclo, quindi ^(cicli per campione)
  gHpK = (int32_t)lround((1.0 - pow(0.999958, (double)APU_CLOCK / fs)) * (1 << 20));
  memset(c, 0, sizeof(c));
  memset(r, 0, sizeof(r));
  memset(onda, 0, sizeof(onda));
  memset(imp, 0, sizeof(imp));
  ondaShift = 4;
  lfsr = 0x7FFF;
  rumore7 = false;
  swOn = false;
  swConta = 0;
  swOmbra = 0;
  seqPasso = 0;
  seqProssimo = SEQ_CICLI;
  gOra = gLetti = 0;
  gInteg = gCond = 0;
  r[NR52 - 0x10] = 0x80;
  r[NR50 - 0x10] = 0x77;
}

uint64_t Apu::cicloDelCampione(uint64_t s) const {
  return ((s << 22) + gFs - 1) / gFs;
}

// un gradino del segnale al ciclo t, come BLEP nel buffer degli impulsi
void Apu::gradino(uint64_t t, int32_t delta) {
  // posizione in 1/65536 di campione: t · fs / 2^22 · 2^16 (esatta, il clock è 2^22)
  uint64_t pos = (t * gFs) >> 6;
  uint64_t base = gLetti << 16;
  uint32_t rel = pos > base ? (uint32_t)(pos - base) : 0;
  uint32_t i = rel >> 16;
  if (i >= APU_BLOCCO) i = APU_BLOCCO - 1;  // non succede se si rispetta il contratto di leggi()
  const int16_t *k = gNucleo[(rel >> 11) & (FASI - 1)];
  int32_t *d = imp + i;
  for (int j = 0; j < PRESE; j++) d[j] += delta * k[j];
}

// ---------------------------------------------------------------- canali

int32_t Apu::fattore(uint8_t i) const {
  uint8_t nr50 = r[NR50 - 0x10], nr51 = r[NR51 - 0x10];
  if (!(r[NR52 - 0x10] & 0x80)) return 0;
  int32_t f = 0;
  if (nr51 & (1 << i)) f += (nr50 & 7) + 1;              // destra
  if (nr51 & (0x10 << i)) f += ((nr50 >> 4) & 7) + 1;    // sinistra
  return f;
}

uint8_t Apu::digitale(uint8_t i) const {
  const Canale &k = c[i];
  if (!k.on) return 0;
  if (i < 2) return DUTY[k.duty][k.passo] ? k.vol : 0;
  if (i == 2) return onda[k.passo] >> ondaShift;
  return (lfsr & 1) ? 0 : k.vol;
}

void Apu::aggiorna(uint8_t i, uint64_t t) {
  Canale &k = c[i];
  int32_t a = k.dac ? 2 * digitale(i) - 15 : 0;  // DAC: 0..15 -> -15..+15
  int32_t u = a * fattore(i);
  if (u != k.uscita) {
    gradino(t, u - k.uscita);
    k.uscita = u;
  }
}

void Apu::aggiornaTutti(uint64_t t) {
  for (uint8_t i = 0; i < 4; i++) aggiorna(i, t);
}

void Apu::periodoDi(uint8_t i) {
  Canale &k = c[i];
  if (i < 2) k.periodo = (2048u - k.freq) * 4;
  else if (i == 2) k.periodo = (2048u - k.freq) * 2;
  else {
    uint8_t v = r[NR43 - 0x10], div = v & 7, s = v >> 4;
    k.periodo = (div ? div * 16u : 8u) << s;
  }
}

void Apu::passoCanale(uint8_t i) {
  Canale &k = c[i];
  if (i < 2) k.passo = (k.passo + 1) & 7;
  else if (i == 2) k.passo = (k.passo + 1) & 31;
  else {
    uint16_t x = (lfsr ^ (lfsr >> 1)) & 1;
    lfsr = (lfsr >> 1) | (x << 14);
    if (rumore7) lfsr = (lfsr & ~(1u << 6)) | (x << 6);
  }
}

uint16_t Apu::sweepCalcola() {
  uint8_t v = r[NR10 - 0x10];
  uint16_t d = swOmbra >> (v & 7);
  uint16_t f = (v & 8) ? swOmbra - d : swOmbra + d;
  if (f > 2047) {
    c[0].on = false;
    aggiorna(0, gOra);
  }
  return f;
}

void Apu::avvia(uint8_t i) {
  Canale &k = c[i];
  k.on = k.dac;
  if (k.lung == 0) k.lung = i == 2 ? 256 : 64;
  if (i == 2) k.passo = 0;
  if (i == 3) lfsr = 0x7FFF;
  if (i != 2) {
    k.vol = k.envIni;
    k.envConta = k.envPasso;
  }
  periodoDi(i);
  k.prossimo = gOra + k.periodo;
  if (i == 0) {
    uint8_t v = r[NR10 - 0x10], ritmo = (v >> 4) & 7;
    swOmbra = k.freq;
    swConta = ritmo ? ritmo : 8;
    swOn = ritmo || (v & 7);
    if (v & 7) sweepCalcola();
  }
  aggiorna(i, gOra);
}

void Apu::scrivi(uint8_t reg, uint8_t val) {
  if (reg < 0x10 || reg > 0x3F) return;
  if (!(r[NR52 - 0x10] & 0x80) && reg < NR52) return;  // spenta: i registri non si scrivono
  r[reg - 0x10] = val;
  if (reg >= WAVE_RAM) {
    uint8_t j = (reg - WAVE_RAM) * 2;
    onda[j] = val >> 4;
    onda[j + 1] = val & 15;
    aggiorna(2, gOra);
    return;
  }
  // canale e numero del registro (0..4) dentro il canale
  uint8_t i = (reg - 0x10) / 5, n = (reg - 0x10) % 5;
  if (reg >= NR50) {
    if (reg == NR52 && !(val & 0x80)) {
      for (uint8_t j = 0; j < 4; j++) c[j].on = c[j].dac = false;
      memset(r, 0, NR52 - 0x10);
    }
    aggiornaTutti(gOra);
    return;
  }
  Canale &k = c[i];
  switch (n) {
    case 0:
      if (i == 2) {
        k.dac = val & 0x80;
        if (!k.dac) k.on = false;
      }
      break;  // NR10: lo sweep legge il registro al momento
    case 1:
      if (i == 2) k.lung = 256 - val;
      else {
        k.lung = 64 - (val & 63);
        if (i < 2) k.duty = val >> 6;
      }
      break;
    case 2:
      if (i == 2) {
        static const uint8_t SHIFT[4] = {4, 0, 1, 2};
        ondaShift = SHIFT[(val >> 5) & 3];
      } else {
        k.envIni = val >> 4;
        k.envSu = val & 8;
        k.envPasso = val & 7;
        k.dac = (val & 0xF8) != 0;
        if (!k.dac) k.on = false;
      }
      break;
    case 3:
      if (i == 3) {
        rumore7 = val & 8;
        periodoDi(3);
      } else {
        k.freq = (k.freq & 0x700) | val;
        periodoDi(i);
      }
      break;
    case 4:
      if (i < 3) {
        k.freq = (k.freq & 0xFF) | ((val & 7) << 8);
        periodoDi(i);
      }
      k.lungOn = val & 0x40;
      if (val & 0x80) avvia(i);
      break;
  }
  aggiorna(i, gOra);
}

// ---------------------------------------------------------------- tempo

void Apu::sequencer() {
  if (!(seqPasso & 1)) {  // lunghezza a 256 Hz
    for (uint8_t i = 0; i < 4; i++) {
      Canale &k = c[i];
      if (k.lungOn && k.lung && --k.lung == 0) {
        k.on = false;
        aggiorna(i, gOra);
      }
    }
  }
  if ((seqPasso == 2 || seqPasso == 6) && --swConta == 0) {  // sweep a 128 Hz
    uint8_t v = r[NR10 - 0x10], ritmo = (v >> 4) & 7;
    swConta = ritmo ? ritmo : 8;
    if (swOn && ritmo && c[0].on) {
      uint16_t f = sweepCalcola();
      if (f <= 2047 && (v & 7)) {
        swOmbra = c[0].freq = f;
        periodoDi(0);
        sweepCalcola();
      }
    }
  }
  if (seqPasso == 7) {  // inviluppo a 64 Hz
    for (uint8_t i = 0; i < 4; i++) {
      if (i == 2) continue;
      Canale &k = c[i];
      if (!k.envPasso || !k.on) continue;
      if (--k.envConta == 0) {
        k.envConta = k.envPasso;
        if (k.envSu && k.vol < 15) k.vol++;
        else if (!k.envSu && k.vol > 0) k.vol--;
        aggiorna(i, gOra);
      }
    }
  }
  seqPasso = (seqPasso + 1) & 7;
}

void Apu::avanza(uint64_t fino) {
  while (gOra < fino) {
    uint64_t fine = seqProssimo < fino ? seqProssimo : fino;
    for (uint8_t i = 0; i < 4; i++) {
      Canale &k = c[i];
      if (!k.on) continue;
      while (k.prossimo < fine) {
        passoCanale(i);
        aggiorna(i, k.prossimo);
        k.prossimo += k.periodo;
      }
    }
    gOra = fine;
    if (fine == seqProssimo) {
      sequencer();
      seqProssimo += SEQ_CICLI;
    }
  }
}

void Apu::leggi(int32_t *buf, uint32_t n) {
  if (n > APU_BLOCCO) n = APU_BLOCCO;
  for (uint32_t i = 0; i < n; i++) {
    gInteg += imp[i];
    int32_t x = gInteg >> 7;  // Q15 -> Q8
    if (gPassaAlto) {
      int32_t out = x - gCond;
      int32_t d = (int32_t)(((int64_t)out * gHpK) >> 20);
      if (d == 0 && out) d = out > 0 ? 1 : -1;  // così il condensatore arriva davvero a x
      gCond += d;
      x = out;
    }
    buf[i] = x;
  }
  const uint32_t TOT = APU_BLOCCO + PRESE;
  memmove(imp, imp + n, (TOT - n) * sizeof(int32_t));
  memset(imp + TOT - n, 0, n * sizeof(int32_t));
  gLetti += n;
}
