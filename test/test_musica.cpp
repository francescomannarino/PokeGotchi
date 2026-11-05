// Musica di battaglia e dei minigiochi: APU emulata (apu.cpp) e
// interprete del motore di pokered con gli effetti e la batteria (musica.cpp), sui brani originali
// di tools/musica/originali/. Le durate attese vengono da `python3 tools/gen_musica.py durate`,
// calcolate in Python dagli stessi sorgenti.
#include "framework.h"
#include "../apu.h"
#include "../hal.h"
#include "../musica.h"
#include "../musica_dati.h"
#include <climits>
#include <map>
#include <vector>

static const uint32_t FS = 32000;

// avanza l'APU fino al campione `fino` e ne legge i campioni (Q8 del mixer)
static std::vector<int32_t> leggiApu(Apu &a, uint32_t n) {
  std::vector<int32_t> v(n);
  for (uint32_t i = 0; i < n; i += APU_BLOCCO) {
    uint32_t k = n - i < APU_BLOCCO ? n - i : APU_BLOCCO;
    a.avanza(a.cicloDelCampione(a.campioniLetti() + k));
    a.leggi(&v[i], k);
  }
  return v;
}

// un'onda quadra sul canale 1 a volume 15 fisso, in mezzo al mixer
static void quadra(Apu &a, uint16_t f11, uint8_t duty) {
  a.scrivi(NR51, 0x11);
  a.scrivi(NR11, (uint8_t)(duty << 6));
  a.scrivi(NR12, 0xF0);
  a.scrivi(NR13, f11 & 0xFF);
  a.scrivi(NR14, 0x80 | (f11 >> 8));
}

static std::vector<int16_t> rendi(uint32_t n) {
  std::vector<int16_t> v(n);
  for (uint32_t i = 0; i < n; i += 700) musicaRiempi(&v[i], n - i < 700 ? n - i : 700);
  return v;
}

static uint32_t campioniPerFotogrammi(uint32_t f, uint32_t fs) {
  return (uint32_t)((uint64_t)f * MUSICA_FPS_DEN * fs / MUSICA_FPS_NUM) + 1;
}

// ---------------------------------------------------------------- APU

TEST(apu, frequenza_della_quadra_dagli_zero_crossing) {
  const uint16_t NOTE[] = {1546, 1750, 1899, 1981};  // ~261, ~440, ~880, ~1956 Hz
  for (uint16_t f11 : NOTE) {
    Apu a;
    a.inizia(FS);
    quadra(a, f11, 2);
    std::vector<int32_t> v = leggiApu(a, FS + FS / 10);
    int salite = 0;
    for (uint32_t i = FS / 10 + 1; i < v.size(); i++)  // un secondo, dopo 100 ms
      if (v[i - 1] < 0 && v[i] >= 0) salite++;
    double atteso = 131072.0 / (2048 - f11);
    CHECK_MSG(salite >= atteso * 0.99 - 1 && salite <= atteso * 1.01 + 1,
              std::to_string(f11) + ": " + std::to_string(salite) + " vs " + std::to_string(atteso));
  }
}

TEST(apu, duty_misurato) {
  const double ATTESO[4] = {0.125, 0.25, 0.5, 0.75};
  for (uint8_t d = 0; d < 4; d++) {
    Apu a;
    a.inizia(FS);
    quadra(a, 1750, d);  // ~440 Hz, ben sopra il passa-alto
    std::vector<int32_t> v = leggiApu(a, FS);
    int alti = 0, tot = 0;
    for (uint32_t i = FS / 4; i < FS; i++, tot++) alti += v[i] > 0;
    double frazione = (double)alti / tot;
    CHECK_MSG(frazione > ATTESO[d] - 0.03 && frazione < ATTESO[d] + 0.03,
              "duty " + std::to_string(d) + ": " + std::to_string(frazione));
  }
}

TEST(apu, inviluppo_scende_di_un_passo_ogni_n_sessantaquattresimi) {
  for (uint8_t n = 1; n <= 3; n++) {
    Apu a;
    a.inizia(FS);
    a.scrivi(NR12, (uint8_t)(0xF0 | n));
    a.scrivi(NR14, 0x80 | 0x06);
    CHECK_EQ(a.volume(0), 15);
    const uint32_t SESSANTAQUATTRESIMO = APU_CLOCK / 64;
    for (uint32_t k = 1; k <= 10; k++) {
      a.avanza((uint64_t)k * n * SESSANTAQUATTRESIMO + 1);
      CHECK_EQ(a.volume(0), (uint8_t)(15 - k));
      a.avanza((uint64_t)(k + 1) * n * SESSANTAQUATTRESIMO - 1);
      CHECK_EQ(a.volume(0), (uint8_t)(15 - k));
    }
  }
}

TEST(apu, canale_d_onda_rispetta_campioni_e_livelli) {
  static const uint8_t ONDA[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                                   0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
  const uint8_t LIVELLO[4] = {0x00, 0x20, 0x40, 0x60}, SHIFT[4] = {4, 0, 1, 2};
  for (uint8_t l = 0; l < 4; l++) {
    Apu a;
    a.inizia(FS);
    a.passaAlto(false);
    a.scrivi(NR51, 0x44);
    for (uint8_t j = 0; j < 16; j++) a.scrivi(WAVE_RAM + j, ONDA[j]);
    a.scrivi(NR30, 0x80);
    a.scrivi(NR32, LIVELLO[l]);
    a.scrivi(NR33, 0);
    a.scrivi(NR34, 0x80);  // f = 0: un campione ogni 4096 cicli, ~31 campioni d'uscita
    std::vector<int32_t> v = leggiApu(a, FS / 4);
    for (uint32_t k = 0; k < 40; k++) {
      uint8_t s = (uint8_t)(k % 32);
      uint8_t nib = (ONDA[s / 2] >> (s & 1 ? 0 : 4)) & 15;
      int32_t atteso = (2 * (nib >> SHIFT[l]) - 15) * 16 * 256;  // NR50 $77: fattore 8 + 8
      // centro del passo k (il passo 0 dura dal trigger a un periodo), più il ritardo del BLEP
      uint64_t ciclo = (uint64_t)k * 4096 + 2048;
      uint32_t i = (uint32_t)((ciclo * FS) >> 22) + 7;
      CHECK_MSG(v[i] == atteso, "livello " + std::to_string(l) + " passo " + std::to_string(k) +
                                    ": " + std::to_string(v[i]) + " vs " + std::to_string(atteso));
    }
  }
}

TEST(apu, nr51_a_zero_tace_e_nr50_scala_in_ottavi) {
  Apu a;
  a.inizia(FS);
  a.passaAlto(false);
  quadra(a, 1750, 2);
  a.scrivi(NR51, 0);
  for (int32_t x : leggiApu(a, 2000)) CHECK_EQ(x, 0);

  // il livello alto della quadra a 64 Hz (f = 0): il valore più alto che dura a lungo,
  // senza l'overshoot del BLEP sui fronti
  int32_t alto[2];
  const uint8_t NR50_[2] = {0x00, 0x77};
  for (int j = 0; j < 2; j++) {
    Apu b;
    b.inizia(FS);
    b.passaAlto(false);
    b.scrivi(NR50, NR50_[j]);
    quadra(b, 0, 2);
    std::map<int32_t, int> conta;
    for (int32_t x : leggiApu(b, FS / 4)) conta[x]++;
    alto[j] = INT32_MIN;
    for (auto &c : conta)
      if (c.second > 100 && c.first > alto[j]) alto[j] = c.first;
  }
  CHECK_EQ(alto[0], 15 * 2 * 256);  // (0 + 1) per lato
  CHECK_EQ(alto[1], 15 * 16 * 256);
}

TEST(apu, rumore_a_7_bit_e_periodico) {
  Apu a;
  a.inizia(FS);
  a.passaAlto(false);
  a.scrivi(NR51, 0x88);
  a.scrivi(NR42, 0xF0);
  a.scrivi(NR43, 0x08 | 0x20 | 1);  // 7 bit, divisore 1, shift 2: un passo ogni 64 cicli
  a.scrivi(NR44, 0x80);
  std::vector<int32_t> v = leggiApu(a, 4096);
  // 127 passi = 8128 cicli: lo stesso campione si ripete dopo un periodo esatto in cicli,
  // che a 32 kHz non è un numero intero di campioni; basta che ci siano entrambi i livelli
  bool alto = false, basso = false;
  for (int32_t x : v) {
    alto |= x > 0;
    basso |= x < 0;
  }
  CHECK(alto && basso);
  CHECK(a.attivo(3));
}

// ---------------------------------------------------------------- interprete

struct DurataBrano { uint8_t brano; uint32_t primo, giro; };
// python3 tools/gen_musica.py durate: «primo sound_loop 0 a …» e «giro …»; il Surf ha i tre
// canali allineati come le battaglie
static const DurataBrano BATTAGLIE[] = {
  {MUS_WILDBATTLE, 2184, 1872}, {MUS_TRAINERBATTLE, 3024, 2688},
  {MUS_GYMLEADERBATTLE, 2808, 2496}, {MUS_FINALBATTLE, 3024, 2688},
  {MUS_SURFINGPIKACHU, 1566, 1392},
};
// la Grotta, per canale: i tre melodici girano insieme, la batteria su 2 battute che
// dividono il giro (2592 = 12 x 216)
static const DurataBrano GROTTA[4] = {
  {MUS_DUNGEON2, 2592, 2592}, {MUS_DUNGEON2, 2592, 2592}, {MUS_DUNGEON2, 2592, 2592},
  {MUS_DUNGEON2, 216, 216},
};
// la batteria della Grotta (drum_speed 12, tempo 144: 6,75 fotogrammi per unità; le
// lunghezze 2 durano 13 e 14 fotogrammi a turno): i fotogrammi dei colpi in un giro, e gli
// strumenti
static const uint32_t GIRO_BATTERIA = 216;
static const uint32_t COLPI[] = {0, 27, 40, 54, 81, 108, 135, 148, 162, 189, 202};
static const uint8_t STRUMENTI[] = {1, 3, 3, 2, 3, 1, 3, 1, 2, 3, 4};
static const uint32_t N_COLPI = sizeof(COLPI) / sizeof(COLPI[0]);

// i colpi partiti nei primi f fotogrammi (0 .. f - 1), se nessuno tace
static uint32_t colpiPrima(uint32_t f) {
  uint32_t n = f / GIRO_BATTERIA * N_COLPI;
  for (uint32_t c : COLPI) n += c < f % GIRO_BATTERIA;
  return n;
}
// «fine a …»
static const DurataBrano VITTORIE[] = {
  {MUS_DEFEATEDWILDMON, 672, 0}, {MUS_DEFEATEDTRAINER, 780, 0}, {MUS_DEFEATEDGYMLEADER, 2016, 0},
};

TEST(musica, giri_dei_brani_di_battaglia_come_in_python_e_allineati_dopo_10) {
  const uint32_t fs = 16000;  // la temporizzazione non dipende da fs: si fa prima
  for (const DurataBrano &d : BATTAGLIE) {
    musicaInizia(fs);
    musicaSuona(d.brano);
    const MusicaDiag &g = musicaDiagnostica();
    std::vector<int16_t> buf(4000);
    uint32_t limite = d.primo + 11 * d.giro;
    while (g.salti[0] < 11 && g.fotogramma < limite) musicaRiempi(buf.data(), buf.size());
    for (int c = 0; c < 3; c++) {
      CHECK_EQ(g.primoSalto[c], d.primo);
      CHECK(g.salti[c] >= 11);
      CHECK_EQ(g.ultimoSalto[c], d.primo + (g.salti[c] - 1) * d.giro);
      CHECK_EQ(g.ultimoSalto[c], g.ultimoSalto[0]);
      CHECK_EQ(g.fine[c], -1);
    }
    CHECK(musicaAttiva());
  }
}

TEST(musica, i_quattro_canali_della_grotta_girano_come_in_python) {
  musicaInizia(16000);
  musicaSuona(MUS_DUNGEON2);
  const MusicaDiag &g = musicaDiagnostica();
  std::vector<int16_t> buf(4000);
  while (g.salti[2] < 3 && g.fotogramma < 4 * 2592) musicaRiempi(buf.data(), buf.size());
  for (int c = 0; c < 4; c++) {
    const DurataBrano &d = GROTTA[c];
    CHECK_EQ(g.primoSalto[c], d.primo);
    CHECK_EQ(g.ultimoSalto[c], d.primo + (g.salti[c] - 1) * d.giro);
    CHECK_EQ(g.fine[c], -1);
  }
  CHECK_EQ(g.salti[2], 3u);
  CHECK_EQ(g.salti[0], 3u);
  CHECK_EQ(g.salti[3], 36u);
  // ogni drum_note suona il suo strumento: nessuno trova il canale ancora occupato
  CHECK_EQ(g.rumori, colpiPrima(g.fotogramma));
  CHECK(musicaAttiva());
}

TEST(musica, la_batteria_suona_sul_rumore_con_i_suoi_strumenti) {
  musicaInizia(FS);
  musicaSuona(MUS_DUNGEON2);
  // il primo colpo (la cassa) parte nel fotogramma 0: inviluppo e polinomio dello strumento.
  // La sua prima nota dura un fotogramma, così all'inizio del fotogramma 1 suona la seconda
  rendi(campioniPerFotogrammi(1, FS));
  const Apu &a = musicaApu();
  const uint8_t *n = MUSICA_RUMORI_NOTE[MUSICA_RUMORI[STRUMENTI[0] - 1] + 1];
  CHECK(a.attivo(3));
  CHECK_EQ(a.registro(NR42), n[1]);
  CHECK_EQ(a.registro(NR43), n[2]);
  CHECK_EQ(musicaDiagnostica().rumori, 1u);
  CHECK_EQ(musicaDiagnostica().ultimoRumore, 0);
  // il secondo (il charleston chiuso) 27 fotogrammi dopo
  rendi(campioniPerFotogrammi(COLPI[1], FS));
  CHECK_EQ(musicaDiagnostica().ultimoRumore, (int32_t)COLPI[1]);
  CHECK_EQ(a.registro(NR43), MUSICA_RUMORI_NOTE[MUSICA_RUMORI[STRUMENTI[1] - 1]][2]);
  // si sente: il rumore oscilla
  std::vector<int16_t> v = rendi(FS / 2);
  bool alto = false, basso = false;
  for (int16_t x : v) {
    alto |= x > 0;
    basso |= x < 0;
  }
  CHECK(alto && basso);
}

TEST(musica, il_colpo_sospende_la_batteria_che_poi_riprende_a_tempo) {
  // riferimento senza effetto, poi un SFX_HIT (130 ms, ~8 fotogrammi) dal fotogramma 106:
  // copre il colpo del fotogramma 108 (la cassa all'inizio del secondo giro), che tace; i
  // successivi cadono agli stessi fotogrammi
  uint32_t rumori[2];
  int32_t ultimo[2];
  uint8_t nr43[2];
  for (int prova = 0; prova < 2; prova++) {
    musicaInizia(FS);
    musicaSuona(MUS_DUNGEON2);
    uint32_t fatti = campioniPerFotogrammi(106, FS);
    rendi(fatti);
    if (prova) {
      musicaEffetto(SFX_HIT);
      rendi(campioniPerFotogrammi(COLPI[5] + 2, FS) - fatti);  // durante l'effetto
      CHECK(musicaApu().attivo(3));
      CHECK_EQ(musicaDiagnostica().rumori, 5u);  // i colpi 0, 27, 40, 54, 81: il 108 ha taciuto
      fatti = campioniPerFotogrammi(COLPI[5] + 2, FS);
    }
    rendi(campioniPerFotogrammi(200, FS) - fatti);
    const MusicaDiag &g = musicaDiagnostica();
    rumori[prova] = g.rumori;
    ultimo[prova] = g.ultimoRumore;
    nr43[prova] = musicaApu().registro(NR43);
  }
  CHECK_EQ(rumori[0], colpiPrima(200));
  CHECK_EQ(rumori[1], rumori[0] - 1);
  CHECK_EQ(ultimo[1], ultimo[0]);
  CHECK_EQ(nr43[1], nr43[0]);
}

TEST(musica, le_vittorie_finiscono_e_poi_tacciono) {
  for (const DurataBrano &d : VITTORIE) {
    musicaInizia(FS);
    musicaSuona(d.brano);
    CHECK(musicaAttiva());
    std::vector<int16_t> v = rendi(campioniPerFotogrammi(d.primo + 2, FS));
    const MusicaDiag &g = musicaDiagnostica();
    for (int c = 0; c < 3; c++) CHECK_EQ(g.fine[c], (int32_t)d.primo);
    CHECK(!musicaAttiva());
    bool suona = false;
    for (int16_t x : v) suona |= x != 0;
    CHECK(suona);
    rendi(FS);  // il passa-alto scarica il condensatore
    std::vector<int16_t> dopo = rendi(1000);
    for (int16_t x : dopo) CHECK_EQ(x, 0);
    CHECK(musicaSilenzio());
  }
}

TEST(musica, stessi_campioni_a_parita_di_frequenza_e_comandi) {
  std::vector<int16_t> a, b;
  for (int giro = 0; giro < 2; giro++) {
    musicaInizia(FS);
    musicaSuona(MUS_GYMLEADERBATTLE);
    std::vector<int16_t> v = rendi(FS * 3);
    musicaEffetto(SFX_ATTACK);
    musicaEffetto(SFX_HIT);
    std::vector<int16_t> w = rendi(FS * 2);
    v.insert(v.end(), w.begin(), w.end());
    (giro ? b : a) = v;
  }
  CHECK(a == b);
}

TEST(musica, durante_un_effetto_il_ch1_suona_l_effetto_e_poi_riprende_a_tempo) {
  // riferimento: lo stesso brano senza effetto, fotografato agli stessi istanti
  struct Foto { bool on1, on3; uint16_t f1, f3; uint8_t v1; };
  const uint32_t ISTANTI[] = {2 * FS + FS / 10, 2 * FS + FS / 4, 4 * FS, 5 * FS, 6 * FS};
  Foto rif[5], con[5];
  for (int prova = 0; prova < 2; prova++) {
    musicaInizia(FS);
    musicaSuona(MUS_WILDBATTLE);
    uint32_t fatti = 0;
    for (int j = 0; j < 5; j++) {
      if (j == 0) {
        rendi(2 * FS);
        fatti = 2 * FS;
        if (prova) musicaEffetto(SFX_HATCH);  // 4 note, 440 ms
      }
      rendi(ISTANTI[j] - fatti);
      fatti = ISTANTI[j];
      const Apu &a = musicaApu();
      Foto x = {a.attivo(0), a.attivo(2), a.frequenza(0), a.frequenza(2), a.volume(0)};
      (prova ? con : rif)[j] = x;
    }
  }
  // a 100 ms la seconda nota di SFX_HATCH (659 Hz), a 250 ms la terza (784 Hz)
  CHECK_EQ(con[0].f1, (uint16_t)(2048 - (131072 + 659 / 2) / 659));
  CHECK_EQ(con[1].f1, (uint16_t)(2048 - (131072 + 784 / 2) / 784));
  // intanto il canale 3 suona la musica come senza effetto
  for (int j = 0; j < 2; j++) {
    CHECK_EQ(con[j].on3, rif[j].on3);
    CHECK_EQ(con[j].f3, rif[j].f3);
  }
  // dopo l'effetto il canale 1 è di nuovo quello della musica
  for (int j = 2; j < 5; j++) {
    CHECK_EQ(con[j].on1, rif[j].on1);
    CHECK_EQ(con[j].f1, rif[j].f1);
    CHECK_EQ(con[j].v1, rif[j].v1);
    CHECK_EQ(con[j].f3, rif[j].f3);
  }
}

TEST(musica, il_colpo_usa_il_rumore_e_non_il_ch1) {
  musicaInizia(FS);
  musicaEffetto(SFX_HIT);
  rendi(FS / 50);
  CHECK(musicaApu().attivo(3));
  CHECK(!musicaApu().attivo(0));
  CHECK(!musicaSilenzio());
}

// il rifiuto è un'onda stretta (12,5 %): a 300/200 Hz al 50 % spariva sotto la musica di
// battaglia; gli altri effetti restano al 50 %
TEST(musica, il_rifiuto_suona_a_duty_stretto) {
  double frazione[2];
  const uint8_t ID[2] = {SFX_DENY, SFX_PLAY};
  for (int k = 0; k < 2; k++) {
    musicaInizia(FS);
    musicaEffetto(ID[k]);
    std::vector<int16_t> v = rendi(FS / 10);
    int alti = 0, tot = 0;
    for (uint32_t i = FS / 50; i < v.size(); i++, tot++) alti += v[i] > 0;
    frazione[k] = (double)alti / tot;
  }
  CHECK_MSG(frazione[0] < 0.2, "rifiuto: " + std::to_string(frazione[0]));
  CHECK_MSG(frazione[1] > 0.4, "gioco: " + std::to_string(frazione[1]));
}

TEST(musica, conta_gli_effetti_arrivati_al_motore) {
  musicaInizia(FS);
  uint8_t ultimo = 0;
  uint32_t prima = musicaEffettiAvviati(&ultimo);
  musicaEffetto(SFX_TAP);
  musicaEffetto(SFX_DENY);
  rendi(FS / 100);
  CHECK_EQ(musicaEffettiAvviati(&ultimo), prima + 2);
  CHECK_EQ(ultimo, (uint8_t)SFX_DENY);
}

TEST(musica, effetti_a_musica_ferma_poi_silenzio) {
  musicaInizia(FS);
  CHECK(musicaSilenzio());
  musicaEffetto(SFX_TAP);
  musicaEffetto(SFX_EAT);
  std::vector<int16_t> v = rendi(FS / 5);
  bool suona = false;
  for (int16_t x : v) suona |= x != 0;
  CHECK(suona);
  rendi(FS);
  rendi(1000);
  CHECK(musicaSilenzio());
}

TEST(musica, ferma_spegne_la_musica) {
  musicaInizia(FS);
  musicaSuona(MUS_WILDBATTLE);
  rendi(FS);
  musicaFerma();
  CHECK(!musicaAttiva());
  rendi(FS);
  for (int16_t x : rendi(1000)) CHECK_EQ(x, 0);
  CHECK(musicaSilenzio());
}

// ---------------------------------------------------------------- volume

TEST(musica, volume_zero_tutto_a_zero_e_curva_monotona) {
  int64_t energia[11];
  for (uint8_t vol = 0; vol <= 10; vol++) {
    musicaInizia(FS);
    musicaVolume(vol);
    musicaSuona(MUS_TRAINERBATTLE);
    std::vector<int16_t> v = rendi(FS * 2);
    energia[vol] = 0;
    for (int16_t x : v) energia[vol] += (int64_t)x * x;
  }
  CHECK_EQ(energia[0], (int64_t)0);
  for (int vol = 1; vol <= 10; vol++) CHECK_MSG(energia[vol] > energia[vol - 1], std::to_string(vol));
  musicaVolume(7);
}

TEST(musica, al_volume_10_il_picco_resta_sotto_il_limite) {
  // ogni brano per intero (intro + un giro), con un effetto sopra
  for (uint8_t b = 0; b < MUS_COUNT; b++) {
    musicaInizia(FS);
    musicaVolume(10);
    musicaSuona(b);
    uint32_t f = 0;
    for (const DurataBrano &d : BATTAGLIE) if (d.brano == b) f = d.primo + 1;
    for (const DurataBrano &d : VITTORIE) if (d.brano == b) f = d.primo + 1;
    if (b == MUS_DUNGEON2) f = GROTTA[2].primo + 1;
    rendi(FS);
    musicaEffetto(SFX_EVOLVE);
    musicaEffetto(SFX_FAINT);
    rendi(campioniPerFotogrammi(f, FS));
    int32_t p = musicaDiagnostica().picco;
    CHECK_MSG(p > 4000 && p < 32767, std::string(MUSICA_BRANI[b].nome) + ": " + std::to_string(p));
  }
  musicaVolume(7);
}
