#include "musica.h"
#include "apu.h"
#include "musica_dati.h"
#include "sfx_note.h"
#include <atomic>
#include <string.h>

// Interprete del motore audio di pokered (audio/engine_2.asm, Audio2_UpdateMusic e
// seguenti) ed effetti sonori sull'APU di apu.h. Il formato del bytecode e le regole di
// durata sono nel commento in testa a musica_dati.h.

// livello del mixer (Q8, vedi Apu::leggi) -> campione a 16 bit al volume 10: un canale a
// volume 15 con NR50 $77 oscilla di ±15·16 livelli, cioè di ±15·16·GUADAGNO
static const int32_t GUADAGNO = 24;
// volume 0..10 in Q15, passi di 4 dB (10 = 0 dB, 1 = -36 dB)
static const int32_t VOL_Q15[11] = {0, 519, 823, 1305, 2068, 3277, 5193, 8231, 13045, 20675, 32768};

static_assert(MUSICA_FPS_NUM == APU_CLOCK, "un fotogramma = MUSICA_FPS_DEN cicli dell'APU");

static Apu gApu;
static MusicaDiag gDiag;

// ---------------------------------------------------------------- coda dei comandi

// un produttore (il gioco) e un consumatore (musicaRiempi), anche su core diversi
enum : uint8_t { CMD_SUONA, CMD_FERMA, CMD_EFFETTO };
struct Comando { uint8_t tipo, val; };
static const uint8_t CODA = 32;
static Comando gCoda[CODA];
static std::atomic<uint8_t> gCodaScrivi{0}, gCodaLeggi{0};
static std::atomic<uint8_t> gVolume{7};
static std::atomic<uint8_t> gBranoChiesto{MUS_NESSUNA};  // per musicaAttiva() dal gioco
static std::atomic<uint32_t> gEffettiAvviati{0};          // per il comando AUDIO
static std::atomic<uint8_t> gUltimoEffetto{0xFF};

static void accoda(uint8_t tipo, uint8_t val) {
  uint8_t w = gCodaScrivi.load(std::memory_order_relaxed);
  uint8_t prossimo = (uint8_t)((w + 1) % CODA);
  if (prossimo == gCodaLeggi.load(std::memory_order_acquire)) return;  // piena: si scarta
  gCoda[w] = {tipo, val};
  gCodaScrivi.store(prossimo, std::memory_order_release);
}

// ---------------------------------------------------------------- effetti

// due posti: il canale 1 (onda quadra) e il 4 (rumore)
struct PostoSfx {
  uint8_t canale;            // canale dell'APU: 0 o 3
  uint8_t coda[4], nCoda;    // effetti in attesa
  int16_t id;                // effetto in corso, -1 = libero
  uint8_t nota;
  uint64_t fine;             // ciclo in cui finisce la nota
};
static PostoSfx gSfx[2];

static bool occupato(uint8_t canale) {
  for (const PostoSfx &p : gSfx)
    if (p.canale == canale && p.id >= 0) return true;
  return false;
}

static void nr51(uint8_t canale, bool on) {
  uint8_t m = (uint8_t)(0x11 << canale), v = gApu.registro(NR51);
  gApu.scrivi(NR51, on ? (v | m) : (v & ~m));
}

// i parametri del rumore (NR43) che più si avvicinano alla frequenza voluta: il rumore
// bianco (15 bit) a 128·f rimbomba attorno a f, quello a 7 bit ha periodo 127 e suona f
static uint8_t polinomio(uint16_t f, bool sette) {
  uint32_t obiettivo = (uint32_t)f * (sette ? 127 : 128), errMin = UINT32_MAX;
  uint8_t migliore = 0;
  for (uint8_t s = 0; s < 14; s++)
    for (uint8_t d = 0; d < 8; d++) {
      uint32_t hz = 524288u / ((d ? 2u * d : 1u) << s);
      uint32_t err = hz > obiettivo ? hz - obiettivo : obiettivo - hz;
      if (err < errMin) {
        errMin = err;
        migliore = (uint8_t)(s << 4 | d);
      }
    }
  return migliore | (sette ? 8 : 0);
}

// inviluppo di una nota d'effetto: le brevi a volume fisso, le lunghe che si spengono
static uint8_t inviluppo(uint16_t ms, uint8_t vol) {
  return (uint8_t)(vol << 4 | (ms < 60 ? 0 : ms < 150 ? 2 : 3));
}

static void suonaNotaSfx(PostoSfx &p) {
  const Note &n = SFX[p.id].n[p.nota];
  p.fine = gApu.ora() + (uint64_t)n.ms * APU_CLOCK / 1000;
  if (p.canale == 0) {
    if (!n.f) {
      gApu.scrivi(NR12, 0x08);  // volume 0, DAC acceso: una pausa come nel motore
      gApu.scrivi(NR14, 0x80);
      return;
    }
    uint32_t f11 = 2048 - (131072u + n.f / 2) / n.f;
    if (f11 > 2047) f11 = 2047;
    gApu.scrivi(NR10, 0x08);    // sweep spento
    gApu.scrivi(NR11, (uint8_t)(SFX[p.id].duty << 6));
    gApu.scrivi(NR12, inviluppo(n.ms, SFX[p.id].duty == 2 ? 13 : 15));
    gApu.scrivi(NR13, f11 & 0xFF);
    gApu.scrivi(NR14, 0x80 | (f11 >> 8));
  } else {
    if (!n.f) {
      gApu.scrivi(NR42, 0x08);
      gApu.scrivi(NR44, 0x80);
      return;
    }
    bool colpo = p.id == SFX_HIT;
    gApu.scrivi(NR41, 0);
    gApu.scrivi(NR42, colpo ? 0xF1 : inviluppo(n.ms, 13));
    gApu.scrivi(NR43, polinomio(n.f, !colpo));
    gApu.scrivi(NR44, 0x80);
  }
  nr51(p.canale, true);
}

static bool gSuona = false;  // un brano è in corso (lato di musicaRiempi)

// fine dell'effetto: il canale torna muto, e la musica lo riprende alla sua nota successiva
static void liberaSfx(PostoSfx &p) {
  p.id = -1;
  if (p.canale == 0) {
    if (gSuona) {
      gApu.scrivi(NR12, 0x08);
      gApu.scrivi(NR14, 0x80);
    } else {
      gApu.scrivi(NR12, 0);
      nr51(0, false);
    }
  } else {
    gApu.scrivi(NR42, 0);
    nr51(3, false);
  }
}

static void prossimoSfx(PostoSfx &p) {
  if (p.id >= 0 && ++p.nota < SFX[p.id].len) {
    suonaNotaSfx(p);
    return;
  }
  if (!p.nCoda) {
    if (p.id >= 0) liberaSfx(p);
    return;
  }
  p.id = p.coda[0];
  memmove(p.coda, p.coda + 1, --p.nCoda);
  p.nota = 0;
  suonaNotaSfx(p);
}

static void avviaSfx(uint8_t id) {
  if (id >= SFX_COUNT) return;
  PostoSfx &p = gSfx[(id == SFX_HIT || id == SFX_FAINT) ? 1 : 0];
  if (p.nCoda >= sizeof(p.coda)) return;
  p.coda[p.nCoda++] = id;
  gEffettiAvviati.fetch_add(1, std::memory_order_relaxed);
  gUltimoEffetto.store(id, std::memory_order_relaxed);
  if (p.id < 0) prossimoSfx(p);
}

// ---------------------------------------------------------------- interprete

struct CanaleMus {
  bool attivo, chiamata, perfetto, vibGiu;
  uint16_t pc, ritorno;
  uint8_t conta, fraz;       // fotogrammi alla nota successiva (0 = 256) e resto
  uint8_t vel, volume, onda, ottava, duty, ripeti;
  uint8_t vibRit, vibRitIni, vibEst, vibVel, freqBassa;
};
static const uint8_t N_CANALI = 4;  // il 4 (rumore) conta solo il tempo e suona la batteria
static const MusicaBrano *gBrano = nullptr;
static CanaleMus gC[N_CANALI];
static uint16_t gTempo;
static uint64_t gProssimoFotogramma;

// La batteria: drum_note sul canale 4 suona uno strumento di rumore, che in pokered è un
// effetto del canale 8 (Audio2_PlaySound con l'id 1-19). Come lì parte solo se il canale è
// libero (né lo strumento di prima né un effetto del gioco, che ha la precedenza), suona
// nello stesso fotogramma la sua prima nota e alla fine lascia il canale com'è.
static bool gRumAttivo;
static uint8_t gRumNota, gRumFine;  // nota successiva e fine (esclusa) in MUSICA_RUMORI_NOTE
static uint8_t gRumConta;           // fotogrammi alla nota successiva

static void avviaRumore(uint8_t strumento) {
  if (!strumento || strumento > MUSICA_N_RUMORI || gRumAttivo || occupato(3)) return;
  gRumNota = MUSICA_RUMORI[strumento - 1];
  gRumFine = MUSICA_RUMORI[strumento];
  gRumConta = 1;
  gRumAttivo = true;
  gDiag.rumori++;
  gDiag.ultimoRumore = (int32_t)gDiag.fotogramma;
}

// un fotogramma del canale 8, dopo i quattro della musica (l'ordine del motore)
static void passoRumore() {
  if (!gRumAttivo) return;
  if (occupato(3)) {  // un effetto ha preso il rumore: lo strumento finisce qui
    gRumAttivo = false;
    return;
  }
  if (gRumConta != 1) {
    gRumConta--;
    return;
  }
  if (gRumNota >= gRumFine) {  // il sound_ret dello strumento
    gRumAttivo = false;
    return;
  }
  // Audio2_sfx_note sul canale 8: lunghezza in NR41 (non usata), inviluppo, polinomio e
  // riavvio senza contatore
  const uint8_t *n = MUSICA_RUMORI_NOTE[gRumNota++];
  gRumConta = (uint8_t)(n[0] + 1);
  gApu.scrivi(NR41, gRumConta & 0x3F);
  gApu.scrivi(NR42, n[1]);
  gApu.scrivi(NR43, n[2]);
  gApu.scrivi(NR44, 0x80);
  nr51(3, true);
}

static uint8_t leggiByte(uint16_t off) { return off < gBrano->lunghezza ? gBrano->dati[off] : 0xFF; }
static uint16_t parola(uint16_t off) { return (uint16_t)(leggiByte(off) | leggiByte(off + 1) << 8); }

// Audio2_note_length e Audio2_note_pitch; sul canale 4 `strumento` è quello del drum_note
static void nota(uint8_t i, uint8_t b, uint8_t strumento = 0) {
  CanaleMus &k = gC[i];
  uint8_t prod = (uint8_t)(((b & 0xF) + 1) * k.vel);
  uint16_t v = (uint16_t)(k.fraz + prod * gTempo);
  k.fraz = v & 0xFF;
  k.conta = v >> 8;
  if (i == 3) {  // rumore: niente frequenza, e la pausa non tocca il canale
    if (b < MCMD_REST) avviaRumore(strumento);
    return;
  }
  if (occupato(i)) return;
  const uint8_t base = (uint8_t)(0x10 + 5 * i);
  if (b >= MCMD_REST) {
    if (i == 2) nr51(2, false);
    else {
      gApu.scrivi(base + 2, 0x08);
      gApu.scrivi(base + 4, 0x80);
    }
    return;
  }
  // Audio2_CalculateFrequency: (note >> (7 - ottava)) + $800, nei due byte separati
  int16_t f = (int16_t)((int16_t)MUSICA_NOTE[b >> 4] >> (7 - (k.ottava & 7)));
  uint8_t alto = (uint8_t)(((f >> 8) + 8) & 0xFF), basso = (uint8_t)(f & 0xFF);
  gApu.scrivi(base + 2, k.volume);
  gApu.scrivi(base + 1, i == 2 ? k.conta : (uint8_t)((k.conta & 0x3F) | k.duty));
  nr51(i, true);
  if (k.perfetto) basso++;  // senza riporto, come nell'originale
  k.freqBassa = basso;
  if (i == 2) {
    gApu.scrivi(NR30, 0);
    const uint8_t *w = MUSICA_ONDE[k.onda % MUSICA_N_ONDE];
    for (uint8_t j = 0; j < 16; j++) gApu.scrivi(WAVE_RAM + j, w[j]);
    gApu.scrivi(NR30, 0x80);
  }
  gApu.scrivi(base + 3, basso);
  gApu.scrivi(base + 4, (uint8_t)((alto | 0x80) & 0xC7));
}

// Audio2_PlayNextNote: i comandi fino alla nota o pausa successiva
static void prossimaNota(uint8_t i) {
  CanaleMus &k = gC[i];
  k.vibRit = k.vibRitIni;
  for (int guardia = 0; guardia < 4096; guardia++) {
    uint8_t b = leggiByte(k.pc++);
    if (b < MCMD_NOTE_TYPE) {
      // canale 4: drum_note ha lo strumento nel byte dopo; sotto $B0 la forma corta
      // (strumento nel nibble alto, mai usata dai brani) come in Audio2_note
      if (i == 3 && b < MCMD_REST)
        nota(i, b, (b & 0xF0) == MCMD_DRUM_NOTE ? leggiByte(k.pc++) : (uint8_t)(b >> 4));
      else
        nota(i, b);
      return;
    }
    switch (b) {
      case MCMD_SOUND_RET:
        if (k.chiamata) {
          k.chiamata = false;
          k.pc = k.ritorno;
          continue;
        }
        k.attivo = false;
        gDiag.fine[i] = (int32_t)gDiag.fotogramma;
        if (!occupato(i)) nr51(i, false);
        return;
      case MCMD_SOUND_CALL:
        k.ritorno = k.pc + 2;
        k.pc = parola(k.pc);
        k.chiamata = true;
        continue;
      case MCMD_SOUND_LOOP: {
        uint8_t n = leggiByte(k.pc);
        if (n == 0) {
          if (!gDiag.salti[i]++) gDiag.primoSalto[i] = gDiag.fotogramma;
          gDiag.ultimoSalto[i] = gDiag.fotogramma;
        } else if (k.ripeti == n) {
          k.ripeti = 1;
          k.pc += 3;
          continue;
        } else {
          k.ripeti++;
        }
        k.pc = parola(k.pc + 1);
        continue;
      }
      case MCMD_TOGGLE_PERFECT_PITCH:
        k.perfetto = !k.perfetto;
        continue;
      case MCMD_VIBRATO: {
        k.vibRit = k.vibRitIni = leggiByte(k.pc++);
        uint8_t p = leggiByte(k.pc++), x = p >> 4, y = p & 15;
        k.vibEst = (uint8_t)(((x >> 1) + (x & 1)) << 4 | (x >> 1));  // sopra | sotto
        k.vibVel = (uint8_t)(y << 4 | y);                              // ricarica | contatore
        continue;
      }
      case MCMD_DUTY_CYCLE:
        k.duty = (uint8_t)((leggiByte(k.pc++) & 3) << 6);
        continue;
      case MCMD_TEMPO:
        gTempo = (uint16_t)(leggiByte(k.pc) << 8 | leggiByte(k.pc + 1));
        k.pc += 2;
        for (CanaleMus &c : gC) c.fraz = 0;
        continue;
      case MCMD_VOLUME:
        gApu.scrivi(NR50, leggiByte(k.pc++));
        continue;
      case MCMD_EXECUTE_MUSIC:
        continue;
    }
    if ((b & 0xF0) == MCMD_NOTE_TYPE) {
      k.vel = b & 0xF;
      if (i == 3) continue;  // drum_speed: senza parametro
      uint8_t p = leggiByte(k.pc++);
      if (i == 2) {
        k.onda = p & 0xF;
        k.volume = (uint8_t)((p & 0x30) << 1);  // il livello nel formato di NR32
      } else {
        k.volume = p;
      }
    } else if ((b & 0xF0) == MCMD_OCTAVE) {
      k.ottava = b & 0xF;
    } else {
      break;  // comando sconosciuto: gen_musica.py non lo genera mai
    }
  }
  k.attivo = false;  // nessuna nota trovata
  gDiag.fine[i] = (int32_t)gDiag.fotogramma;
}

// Audio2_UpdateMusic + Audio2_ApplyMusicAffects: un fotogramma
static void passoMusica() {
  bool qualcuno = false;
  for (uint8_t i = 0; i < N_CANALI; i++) {
    CanaleMus &k = gC[i];
    if (!k.attivo) continue;
    if (k.conta == 1) {
      prossimaNota(i);
      qualcuno |= k.attivo;
      continue;
    }
    qualcuno = true;
    k.conta--;
    if (occupato(i) || i == 3) continue;
    // vibrato: dopo il ritardo, ogni (velocità + 1) fotogrammi alterna sopra e sotto la
    // nota, solo sul byte basso della frequenza (e senza riavviarla)
    if (k.vibRit) {
      k.vibRit--;
      continue;
    }
    if (!k.vibEst) continue;
    if (k.vibVel & 0xF) {
      k.vibVel--;
      continue;
    }
    k.vibVel = (uint8_t)((k.vibVel & 0xF0) | (k.vibVel >> 4));
    uint8_t f = k.freqBassa, d;
    if (k.vibGiu) {
      d = k.vibEst & 0xF;
      f = f > d ? f - d : 0;
    } else {
      d = k.vibEst >> 4;
      f = f + d > 0xFF ? 0xFF : f + d;
    }
    k.vibGiu = !k.vibGiu;
    gApu.scrivi((uint8_t)(0x10 + 5 * i + 3), f);
  }
  passoRumore();
  gDiag.fotogramma++;
  if (!qualcuno) {  // una vittoria è finita
    gSuona = false;
    uint8_t atteso = (uint8_t)(gBrano - MUSICA_BRANI);
    gBranoChiesto.compare_exchange_strong(atteso, MUS_NESSUNA);
  }
}

// i canali della musica tacciono (non quelli presi da un effetto)
static void zittisci() {
  for (uint8_t i = 0; i < N_CANALI; i++) {
    if (occupato(i)) continue;
    gApu.scrivi(i == 2 ? (uint8_t)NR30 : (uint8_t)(0x10 + 5 * i + 2), 0);
    nr51(i, false);
  }
}

static void avviaBrano(uint8_t brano) {
  memset(gC, 0, sizeof(gC));
  gRumAttivo = false;
  for (int i = 0; i < 4; i++) {
    gDiag.salti[i] = gDiag.primoSalto[i] = gDiag.ultimoSalto[i] = 0;
    gDiag.fine[i] = -1;
  }
  gDiag.fotogramma = 0;
  gDiag.rumori = 0;
  gDiag.ultimoRumore = -1;
  zittisci();
  gSuona = brano < MUS_COUNT;
  if (!gSuona) return;
  // Audio2_PlaySound, .playMusic: contatori a 1, tempo $0100, NR50 $77, canale 3 acceso
  gBrano = &MUSICA_BRANI[brano];
  gTempo = 0x100;
  for (uint8_t i = 0; i < N_CANALI && i < gBrano->canali; i++) {
    CanaleMus &k = gC[i];
    k.attivo = true;
    k.pc = gBrano->inizio[i];
    k.conta = k.vel = k.ripeti = 1;
  }
  if (!occupato(2)) gApu.scrivi(NR30, 0x80);
  gApu.scrivi(NR50, 0x77);
  gProssimoFotogramma = gApu.ora();
}

// ---------------------------------------------------------------- interfaccia

static bool gZero = true;

void musicaInizia(uint32_t fs) {
  gApu.inizia(fs);
  memset(&gDiag, 0, sizeof(gDiag));
  for (int i = 0; i < 4; i++) gDiag.fine[i] = -1;
  gDiag.ultimoRumore = -1;
  memset(gC, 0, sizeof(gC));
  gRumAttivo = false;
  gSuona = false;
  gBrano = nullptr;
  gZero = true;
  for (uint8_t j = 0; j < 2; j++) {
    gSfx[j] = PostoSfx();
    gSfx[j].canale = j ? 3 : 0;
    gSfx[j].id = -1;
  }
  gCodaScrivi.store(0);
  gCodaLeggi.store(0);
  gBranoChiesto.store(MUS_NESSUNA);
}

void musicaSuona(uint8_t brano) {
  if (brano >= MUS_COUNT) {
    musicaFerma();
    return;
  }
  gBranoChiesto.store(brano);
  accoda(CMD_SUONA, brano);
}

void musicaFerma() {
  gBranoChiesto.store(MUS_NESSUNA);
  accoda(CMD_FERMA, 0);
}

bool musicaAttiva() { return gBranoChiesto.load() != MUS_NESSUNA; }
void musicaEffetto(uint8_t id) { accoda(CMD_EFFETTO, id); }
void musicaVolume(uint8_t v) { gVolume.store(v > 10 ? 10 : v); }
uint32_t musicaEffettiAvviati(uint8_t *ultimo) {
  if (ultimo) *ultimo = gUltimoEffetto.load(std::memory_order_relaxed);
  return gEffettiAvviati.load(std::memory_order_relaxed);
}

bool musicaSilenzio() {
  return !gSuona && gSfx[0].id < 0 && gSfx[1].id < 0 && gZero &&
         gCodaLeggi.load() == gCodaScrivi.load();
}

static void applicaComandi() {
  uint8_t r = gCodaLeggi.load(std::memory_order_relaxed);
  while (r != gCodaScrivi.load(std::memory_order_acquire)) {
    Comando c = gCoda[r];
    r = (uint8_t)((r + 1) % CODA);
    gCodaLeggi.store(r, std::memory_order_release);
    if (c.tipo == CMD_SUONA) avviaBrano(c.val);
    else if (c.tipo == CMD_FERMA) avviaBrano(MUS_NESSUNA);
    else avviaSfx(c.val);
  }
}

void musicaRiempi(int16_t *buf, uint32_t n) {
  applicaComandi();
  const int64_t g = (int64_t)GUADAGNO * VOL_Q15[gVolume.load()];
  int32_t tmp[APU_BLOCCO];
  bool zero = true;
  while (n) {
    uint32_t k = n < APU_BLOCCO ? n : APU_BLOCCO;
    const uint64_t fine = gApu.cicloDelCampione(gApu.campioniLetti() + k);
    // gli eventi (fotogrammi della musica, note degli effetti) nell'ordine del tempo
    for (;;) {
      uint64_t t = fine;
      if (gSuona && gProssimoFotogramma < t) t = gProssimoFotogramma;
      for (const PostoSfx &p : gSfx)
        if (p.id >= 0 && p.fine < t) t = p.fine;
      if (t >= fine) break;
      gApu.avanza(t);
      for (PostoSfx &p : gSfx)
        if (p.id >= 0 && p.fine == t) prossimoSfx(p);
      if (gSuona && gProssimoFotogramma == t) {
        passoMusica();
        gProssimoFotogramma += MUSICA_FPS_DEN;  // 70224 cicli: il clock dell'APU è MUSICA_FPS_NUM
      }
    }
    gApu.avanza(fine);
    gApu.leggi(tmp, k);
    for (uint32_t i = 0; i < k; i++) {
      int32_t v = (int32_t)((tmp[i] * g) >> 23);  // Q8 · Q15
      if (v > 32767) v = 32767;
      if (v < -32767) v = -32767;
      int32_t a = v < 0 ? -v : v;
      if (a > gDiag.picco) gDiag.picco = a;
      zero &= v == 0;
      buf[i] = (int16_t)v;
    }
    buf += k;
    n -= k;
  }
  gZero = zero;
}

const MusicaDiag &musicaDiagnostica() { return gDiag; }
const Apu &musicaApu() { return gApu; }
