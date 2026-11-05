#pragma once
// APU del Game Boy (DMG) emulata, per la musica e gli effetti. Senza
// dipendenze hardware: la usano la scheda, i test, l'emulatore e la PWA tramite musica.h.
//
// - Canali 1 e 2 a onda quadra (4 duty, inviluppo NRx2, frequenza a 11 bit,
//   131072 / (2048 - f) Hz; lo sweep del canale 1), canale 3 d'onda (32 campioni a 4 bit,
//   livelli 0/100/50/25 %), canale 4 di rumore (LFSR a 15 o 7 bit), lunghezze, sequencer a
//   512 Hz (lunghezza 256 Hz, sweep 128 Hz, inviluppo 64 Hz).
// - DAC come sul DMG: il valore digitale 0..15 diventa 2·d − 15 (canale spento con DAC
//   acceso = −15), mixer NR51/NR50 (volume (v + 1) / 8: NR50 = 0 non è muto, NR51 = 0 sì).
//   L'altoparlante è mono: sinistra e destra si sommano.
// - Sintesi a banda limitata: ogni gradino del segnale diventa un BLEP (sinc finestrato,
//   32 fasi × 16 coefficienti) in un buffer di impulsi che si integra in uscita; poi il
//   passa-alto del DMG (condensatore, 0,999958 per ciclo).
// - Il tempo si misura in cicli a 4194304 Hz (2^22), quindi da un ciclo t si arriva al
//   campione t·fs / 2^22 in modo esatto. Nel percorso dei campioni solo aritmetica intera.
//
// Uso: inizia(fs); poi, alternando, scrivi(...) dei registri all'istante attuale e
// avanza(fino) per far scorrere il tempo; leggi(buf, n) consegna i campioni fino a
// cicloDelCampione(campioniLetti() + n), che deve essere già stato raggiunto con avanza().
#include <stdint.h>

#define APU_CLOCK 4194304u       // cicli al secondo (2^22)
#define APU_BLOCCO 512           // campioni al massimo per leggi()

// indirizzi dei registri ($FF10..$FF3F, solo il byte basso)
enum : uint8_t {
  NR10 = 0x10, NR11, NR12, NR13, NR14,
  NR21 = 0x16, NR22, NR23, NR24,
  NR30 = 0x1A, NR31, NR32, NR33, NR34,
  NR41 = 0x20, NR42, NR43, NR44,
  NR50 = 0x24, NR51, NR52,
  WAVE_RAM = 0x30,               // 16 byte, $FF30..$FF3F
};

class Apu {
public:
  void inizia(uint32_t fs);                   // 16000..48000 Hz; tutto spento, NR50 = $77
  void scrivi(uint8_t reg, uint8_t val);      // effetto all'istante ora()
  void avanza(uint64_t fino);                 // fa scorrere il tempo fino al ciclo `fino`
  // n <= APU_BLOCCO campioni (prima avanza fino a cicloDelCampione(campioniLetti() + n)),
  // in unità di 1/256 di livello del mixer (un canale a volume 15 con NR50 $77 = ±15·16)
  void leggi(int32_t *buf, uint32_t n);
  void passaAlto(bool on) { gPassaAlto = on; }  // spento solo nei test

  uint8_t registro(uint8_t reg) const { return r[reg - 0x10]; }  // l'ultimo valore scritto
  uint64_t ora() const { return gOra; }
  uint64_t campioniLetti() const { return gLetti; }
  uint64_t cicloDelCampione(uint64_t s) const;  // il primo ciclo che cade nel campione s
  uint32_t fs() const { return gFs; }

  // per i test e la diagnostica
  bool attivo(uint8_t canale) const { return c[canale].on; }        // canale 0..3
  uint8_t volume(uint8_t canale) const { return c[canale].vol; }    // inviluppo attuale
  uint16_t frequenza(uint8_t canale) const { return c[canale].freq; }

private:
  struct Canale {
    bool on, dac, lungOn;
    uint8_t duty, passo;         // passo: 0..7 della quadra, 0..31 dell'onda
    uint16_t freq;               // 11 bit (rumore: non usato)
    uint32_t periodo;            // cicli tra due passi
    uint64_t prossimo;           // ciclo del passo successivo
    uint16_t lung;               // contatore di lunghezza
    uint8_t vol, envIni, envSu, envPasso, envConta;
    int32_t uscita;              // contributo attuale al mixer (livello × fattore)
  };
  Canale c[4];
  uint8_t r[0x30];               // ultimo valore scritto, $FF10..$FF3F
  uint8_t onda[32];              // campioni del canale 3
  uint8_t ondaShift;             // 4 = muto, 0 = 100 %, 1 = 50 %, 2 = 25 %
  uint16_t lfsr;
  bool rumore7;
  // sweep del canale 1
  bool swOn;
  uint8_t swConta;
  uint16_t swOmbra;
  uint8_t seqPasso;              // sequencer a 512 Hz, 0..7
  uint64_t seqProssimo;

  uint32_t gFs = 32000;
  uint64_t gOra = 0, gLetti = 0;
  bool gPassaAlto = true;
  int32_t gHpK = 0;              // (1 − carica per campione) in Q20
  int32_t gInteg = 0, gCond = 0; // integratore del BLEP (Q15) e condensatore (Q8)
  int32_t imp[APU_BLOCCO + 16];  // impulsi a banda limitata, dal campione gLetti

  void gradino(uint64_t t, int32_t delta);
  void aggiorna(uint8_t i, uint64_t t);       // ricalcola l'uscita del canale i all'istante t
  void aggiornaTutti(uint64_t t);
  int32_t fattore(uint8_t i) const;           // NR51/NR50 per il canale i
  uint8_t digitale(uint8_t i) const;          // 0..15
  void avvia(uint8_t i);                      // trigger (bit 7 di NRx4)
  void periodoDi(uint8_t i);
  void passoCanale(uint8_t i);
  void sequencer();
  uint16_t sweepCalcola();
};
