#pragma once
// Musica ed effetti sonori: un interprete del motore audio di
// pokered (audio/engine_2.asm, un passo per fotogramma a 59,7275 Hz) suona i brani di
// musica_dati.h sull'APU emulata di apu.h; gli effetti di sfx_note.h suonano sulla stessa
// APU, prendendo il canale 1 (onda quadra) o il 4 (rumore: SFX_HIT e SFX_FAINT) per la loro
// durata, mentre la musica continua sugli altri e su quello preso va avanti in silenzio.
// La batteria dei brani a 4 canali (la Grotta) suona sul rumore gli strumenti di
// MUSICA_RUMORI; un effetto sul canale 4 la interrompe e la tiene muta finché dura.
//
// Nessuna dipendenza hardware né da millis(): il tempo è quello dei campioni prodotti da
// musicaRiempi(). Le funzioni di comando (Suona, Ferma, Effetto, Volume) si possono
// chiamare da un altro thread o core rispetto a musicaRiempi(): passano da una coda a un
// produttore e un consumatore, che musicaRiempi() svuota all'inizio di ogni chiamata.
// musicaInizia() invece va chiamata prima di avviare chi chiama musicaRiempi().
//
// I brani sono i MUS_* di musica_dati.h (MUS_NESSUNA = silenzio), gli effetti l'enum Sfx di
// hal.h.
#include <stdint.h>

class Apu;

void musicaInizia(uint32_t fs);        // frequenza di campionamento, 16000..48000 Hz
void musicaSuona(uint8_t brano);       // riparte dall'inizio; MUS_NESSUNA = musicaFerma()
void musicaFerma();
bool musicaAttiva();                   // un brano suona (le vittorie finiscono da sole)
void musicaEffetto(uint8_t id);        // in coda sul suo canale (al massimo 4, poi scartati)
void musicaVolume(uint8_t v);          // 0..10, curva a passi di 4 dB; 0 = tutto a zero
// effetti entrati nel motore dall'avvio (non quelli scartati a coda piena) e l'ultimo
// (0xFF = nessuno): per il comando AUDIO, da qualunque thread
uint32_t musicaEffettiAvviati(uint8_t *ultimo);
// n campioni mono a 16 bit, avanzando musica ed effetti dello stesso tempo
void musicaRiempi(int16_t *buf, uint32_t n);
// niente brano, niente effetti e l'ultimo blocco era tutto zero: si può spegnere l'amplificatore
bool musicaSilenzio();

// ---- per i test e gli strumenti (stesso thread di musicaRiempi)
struct MusicaDiag {
  uint32_t fotogramma;       // fotogrammi suonati del brano (il primo è lo 0)
  uint32_t salti[4];         // sound_loop 0 eseguiti, per canale
  uint32_t primoSalto[4];    // fotogramma del primo e dell'ultimo
  uint32_t ultimoSalto[4];
  int32_t fine[4];           // fotogramma in cui il canale è finito, -1 se suona
  uint32_t rumori;           // strumenti della batteria partiti (non quelli che hanno taciuto)
  int32_t ultimoRumore;      // fotogramma dell'ultimo, -1 = nessuno
  int32_t picco;             // massimo |campione| da musicaInizia()
};
const MusicaDiag &musicaDiagnostica();
const Apu &musicaApu();
