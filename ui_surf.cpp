// Surf, dalla Spiaggia di Pikachu di Pokemon Giallo, per tutte le specie: il
// pet cavalca le onde su una tavola. Inclinare in avanti (lato alto dello schermo che si
// allontana) spinge, all'indietro frena; lasciando la cresta abbastanza veloce si salta, e in
// aria inclinare a destra o a sinistra fa girare la tavola. Atterrare storti fa cadere (3
// cadute e la partita finisce). Nessuna statistica: Pet::surfResult. Pikachu, Raichu e i
// tipi Acqua hanno la tolleranza larga e la tavola gialla. I comandi comuni (zero, filtro,
// joystick, schermo sveglio) sono incl*() in ui_common.cpp, l'atterraggio motoAtterra().

#include "ui_common.h"
#include "moto.h"
#include "musica_dati.h"  // MUS_* (le tabelle non usate non finiscono nel firmware)

#define SF_PET_X 155        // il pet sta a un terzo dello schermo; il mare scorre
#define SF_MARE_Y 300       // livello medio del mare
#define SF_DURATA 90000     // ms di una partita
#define SF_CADUTA_MS 1500   // il pet in acqua, prima di ripartire
#define SF_PUNTI_MS 900     // il «+n» dopo un atterraggio
#define SF_VMIN 3.0f        // px/passo: l'onda scorre sempre
#define SF_VMAX 15.0f
#define SF_V_SALTO 8.0f     // velocità minima sulla cresta per saltare
#define SF_PENDENZA 1.1f    // px/passo² della discesa a pendenza piena (g·sin)
#define SF_SPINTA 0.5f      // px/passo² a inclinazione piena in avanti
#define SF_ATTRITO 0.97f    // velocità che resta dopo un passo da 85 ms
#define SF_G 1.3f           // gravità in aria, px/passo²
#define SF_SALTO 7.0f       // velocità verticale alla partenza a SF_V_SALTO...
#define SF_SALTO_K 1.3f     //   ...più tanto per ogni px/passo in più
#define SF_GIRO 36.0f       // gradi/passo della tavola a inclinazione piena
#define SF_TAVOLA 34        // mezza lunghezza della tavola, px

static const float SF_RAD = 0.01745329f;

enum : uint8_t { SF_ONDA, SF_ARIA, SF_ACQUA };

static uint8_t sfStato;
static float sfS, sfV;            // posizione nel mare (px) e velocità (px/passo)
static float sfY, sfVY;           // in aria: altezza della tavola e velocità verticale
static float sfAng, sfRot;        // angolo della tavola e rotazione dall'ultimo stacco (gradi)
static float sfFasePrima;         // fase dell'onda lunga al passo prima, per trovarne la cresta
static uint32_t sfInizio, sfUltimo, sfFineFino, sfAcquaDa, sfPuntiDa, sfHopDa;
static uint16_t sfPunti, sfSalti;
static uint8_t sfCadute, sfGiriTot, sfUltimiPunti;
static bool sfBonus, sfNuovoRecord;
static uint32_t sfRenderMax;      // il frame più lento della partita (ms), per la seriale

// Il profilo dell'onda, in y dello schermo (giù = positivo): un'onda lunga e un'increspatura.
// Si salta solo dalla cresta dell'onda lunga, così il salto atterra sul suo lato in discesa
// (pendenza fino a ~20°): senza giri si atterra quasi sempre, il rischio sono i giri.
#define SF_A1 30.0f
#define SF_K1 (6.2831853f / 560)
#define SF_A2 5.0f
#define SF_K2 (6.2831853f / 170)
static float sfOnda(float s) { return SF_MARE_Y + SF_A1 * sinf(s * SF_K1) + SF_A2 * sinf(s * SF_K2 + 1.3f); }
// pendenza dy/dx in gradi (positivo = scende verso destra)
static float sfPendenza(float s) {
  return atanf(SF_A1 * SF_K1 * cosf(s * SF_K1) + SF_A2 * SF_K2 * cosf(s * SF_K2 + 1.3f)) / SF_RAD;
}
// la cresta dell'onda lunga (il suo punto più alto) è dove il coseno passa da - a +
static float sfFase(float s) { return cosf(s * SF_K1); }

void startSurf() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  goScreen(SCR_SURF);
}

// Pikachu (25), Raichu (26) e i tipi Acqua
static bool sfHaBonus() {
  if (pet.speciesId == 25 || pet.speciesId == 26) return true;
  const DexEntry &d = DEX_TBL[pet.speciesId];
  return d.type1 == T_ACQUA || d.type2 == T_ACQUA;
}

static void sfSullOnda() {
  sfStato = SF_ONDA;
  sfY = sfOnda(sfS);
  sfVY = 0;
  sfAng = sfPendenza(sfS);
  sfFasePrima = sfFase(sfS);
  sfRot = 0;
}

void surfEnter() {
  sfS = 0;
  sfV = SF_VMIN;
  sfSullOnda();
  sfPunti = sfSalti = 0;
  sfCadute = sfGiriTot = sfUltimiPunti = 0;
  sfFineFino = sfAcquaDa = sfPuntiDa = sfHopDa = 0;
  sfInizio = 0;  // la partita parte finito lo zero
  sfNuovoRecord = false;
  sfBonus = sfHaBonus();
  sfRenderMax = 0;
  inclInizia();
  halMusica(MUS_SURFINGPIKACHU);
}

void surfExit() {
  inclFine();
  halMusica(MUS_NESSUNA);
}

// tocco sull'intestazione = abbandonare senza premio (come la palla)
void surfTap(int16_t, int16_t y) {
  if (sfFineFino) return;
  if (y < 72) goScreen(SCR_MAIN);
}

static void sfFine() {
  halMusica(MUS_NESSUNA);
  sfNuovoRecord = sfPunti > pet.surfHi;
  pet.surfResult(sfPunti);
  halSfx(sfNuovoRecord && sfPunti > 0 ? SFX_MEDAL : SFX_LEVEL);
  sfFineFino = millis() + 4000;
  Serial.printf("SURF punti=%u salti=%u giri=%u cadute=%u s=%lu bonus=%u rec=%u render=%lums\n", sfPunti,
                sfSalti, sfGiriTot, sfCadute, (unsigned long)((millis() - sfInizio) / 1000), sfBonus ? 1 : 0,
                pet.surfHi, (unsigned long)sfRenderMax);
}

static void sfAtterra(uint32_t now) {
  float onda = sfPendenza(sfS);
  if (!motoAtterra(sfAng, onda, sfBonus ? MOTO_SURF_TOLLERANZA_BONUS : MOTO_SURF_TOLLERANZA)) {
    sfCadute++;  // storto: in acqua, e si riparte piano
    sfStato = SF_ACQUA;
    sfAcquaDa = now;
    sfY = sfOnda(sfS);
    sfV = SF_VMIN;
    halSfx(SFX_HIT);
    return;
  }
  uint8_t giri = motoGiri(sfRot);
  sfUltimiPunti = (uint8_t)(1 + 2 * giri);
  sfPunti += sfUltimiPunti;
  sfSalti++;
  sfGiriTot += giri;
  sfPuntiDa = sfHopDa = now;
  halSfx(giri ? SFX_HEART : SFX_PLAY);
  sfSullOnda();
}

// passo a tempo reale (come stepGame): k = passi da 85 ms trascorsi
static void sfPasso(uint32_t now) {
  float k = sfUltimo ? (now - sfUltimo) / 85.0f : 1.0f;
  if (k > 3.0f) k = 3.0f;
  sfUltimo = now;

  if (sfStato == SF_ACQUA) {
    sfS += SF_VMIN * k;  // il mare continua a scorrere sotto il pet che galleggia
    if (now - sfAcquaDa >= SF_CADUTA_MS) {
      if (sfCadute >= 3) {
        sfFine();
        return;
      }
      sfSullOnda();
    }
  } else if (sfStato == SF_ONDA) {
    // la discesa accelera, la salita frena; in avanti spinge, indietro frena
    float p = sfPendenza(sfS);
    sfV += (SF_PENDENZA * sinf(p * SF_RAD) - inclY() * SF_SPINTA) * k;
    sfV *= powf(SF_ATTRITO, k);
    if (sfV < SF_VMIN) sfV = SF_VMIN;
    if (sfV > SF_VMAX) sfV = SF_VMAX;
    sfS += sfV * k;
    float f = sfFase(sfS);
    sfY = sfOnda(sfS);
    sfAng = sfPendenza(sfS);
    if (sfFasePrima < 0 && f >= 0 && sfV >= SF_V_SALTO) {  // la cresta, abbastanza veloce
      sfStato = SF_ARIA;
      sfVY = -(SF_SALTO + (sfV - SF_V_SALTO) * SF_SALTO_K);
      sfRot = 0;
    }
    sfFasePrima = f;
  } else {  // in aria: la tavola gira con l'inclinazione laterale
    sfS += sfV * k;
    sfY += sfVY * k;
    sfVY += SF_G * k;
    float d = inclX() * SF_GIRO * k;
    sfAng += d;
    sfRot += d;
    if (sfVY > 0 && sfY >= sfOnda(sfS)) {
      sfY = sfOnda(sfS);
      sfAtterra(now);
    }
  }

  if (!sfFineFino && now - sfInizio >= SF_DURATA) sfFine();
}

// ---------- disegno ----------

static void sfMare() {
  gfx->fillScreen(C565(0x7c, 0xc8, 0xf0));                      // cielo
  gfx->fillCircle(360, 110, 30, C565(0xf8, 0xe0, 0x60));        // sole
  uint16_t acqua = C565(0x20, 0x70, 0xc8), schiuma = C565(0xe8, 0xf4, 0xff);
  for (int x = 0; x < 466; x += 2) {  // a righe verticali, il profilo dove sta il pet
    int y = (int)sfOnda(sfS + x - SF_PET_X);
    gfx->fillRect(x, y, 2, 466 - y, acqua);
    gfx->fillRect(x, y, 2, 3, schiuma);
  }
}

// la tavola: un segmento spesso centrato in (cx, cy) all'angolo `ang`
static void sfTavola(int cx, int cy, float ang) {
  float c = cosf(ang * SF_RAD), s = sinf(ang * SF_RAD);
  float lx = c * SF_TAVOLA, ly = s * SF_TAVOLA, nx = -s * 4, ny = c * 4;
  int16_t x0 = (int16_t)(cx - lx - nx), y0 = (int16_t)(cy - ly - ny);
  int16_t x1 = (int16_t)(cx + lx - nx), y1 = (int16_t)(cy + ly - ny);
  int16_t x2 = (int16_t)(cx + lx + nx), y2 = (int16_t)(cy + ly + ny);
  int16_t x3 = (int16_t)(cx - lx + nx), y3 = (int16_t)(cy - ly + ny);
  uint16_t col = sfBonus ? C565(0xf8, 0xd0, 0x30) : C565(0xe0, 0x50, 0x40);
  gfx->fillTriangle(x0, y0, x1, y1, x2, y2, col);
  gfx->fillTriangle(x0, y0, x2, y2, x3, y3, col);
  gfx->drawLine(x0, y0, x1, y1, INK_K);
  gfx->drawLine(x3, y3, x2, y2, INK_K);
}

static void sfPet(uint32_t now) {
  int x = SF_PET_X, y = (int)sfY;
  if (sfStato == SF_ACQUA) {  // affonda fino alla vita, la tavola galleggia accanto
    sfTavola(x + 50, (int)sfOnda(sfS + 50) - 2, sfPendenza(sfS + 50));
    int giu = (int)((now - sfAcquaDa) / 40);
    if (giu > 24) giu = 24;
    int mare = (int)sfOnda(sfS) + 4;
    if (!pmd.loaded) {
      gfx->fillCircle(x, mare - 10 + giu, 22, DEX_TBL[pet.speciesId].accent);
      return;
    }
    uint8_t act = pmd.has(PMD_HURT) ? PMD_HURT : PMD_IDLE;
    drawPmdActM(pmd, act, x, mare + giu, now - sfAcquaDa, act == PMD_IDLE, false, 2, false, mare);
    return;
  }
  sfTavola(x, y, sfAng);
  // lo sprite non ruota: guarda verso la punta della tavola, e cambia verso a ogni mezzo giro
  bool destra = cosf(sfAng * SF_RAD) >= 0;
  int piedi = y - 5;
  if (!pmd.loaded) {
    gfx->fillCircle(x, piedi - 22, 22, DEX_TBL[pet.speciesId].accent);
    return;
  }
  if (sfHopDa && now - sfHopDa < 400 && pmd.has(PMD_HOP)) {
    drawPmdActM(pmd, PMD_HOP, x, piedi, now - sfHopDa, false, false, 2, !destra);
    return;
  }
  uint8_t act = destra ? PMD_WALKR : PMD_WALKL;
  bool flip = false;
  if (!pmd.has(act)) {  // un solo verso: l'altro è specchiato
    act = pmd.has(PMD_WALKR) ? PMD_WALKR : pmd.has(PMD_WALKL) ? PMD_WALKL : PMD_IDLE;
    flip = act != PMD_IDLE && (act == PMD_WALKR) != destra;
  }
  drawPmdActM(pmd, act, x, piedi, now, true, false, 2, flip);
}

static void sfTesto(const char *s, uint8_t size, int y, uint16_t col) {
  gfx->setTextColor(col);
  setSize(size);
  setCur(centerX(s, size), y);
  printT(s);
}

// icona della scelta dei giochi: un'onda e la tavola, se la SD non ha la miniatura di Pikachu
void surfDisegnaIcona(int cx, int cy) {
  uint16_t acqua = C565(0x20, 0x70, 0xc8);
  for (int x = -26; x <= 26; x += 2) {
    int y = cy + 8 + (int)(6 * sinf(x * 0.12f));
    gfx->fillRect(cx + x, y, 2, cy + 22 - y, acqua);
  }
  gfx->fillRoundRect(cx - 20, cy - 2, 40, 7, 3, C565(0xe0, 0x50, 0x40));
}

void renderSurf() {
  uint32_t now = millis();
  sfMare();

  if (sfFineFino) {  // schermata finale, come la palla
    if (!timeLeft(sfFineFino)) {
      goScreen(SCR_MAIN);
      return;
    }
    gfx->fillRoundRect(CX - 170, 116, 340, 150, 18, C565(0x18, 0x48, 0x88));  // sul cielo chiaro
    char b[22];
    snprintf(b, sizeof(b), T(S_SCORE_FMT), sfPunti);
    sfTesto(b, 4, 140, UI_WHITE);
    if (sfNuovoRecord && sfPunti > 0) {
      sfTesto(T(S_NEW_RECORD), 2, 200, UI_BAR_WARN);
    } else {
      char r[20];
      snprintf(r, sizeof(r), T(S_RECORD_FMT), pet.surfHi);
      sfTesto(r, 2, 200, UI_WHITE);
    }
    sfTesto(sfPunti >= 10 ? T(S_GREAT_JOY) : T(S_PLUS_JOY), 2, 236, UI_WHITE);
    halFlush();
    return;
  }

  if (inclAggiorna()) {  // zero: il pet aspetta sulla tavola, e il cartello dice cosa fare
    sfPet(now);
    const char *m = inclSensore() ? T(S_TILT_HOLD) : T(S_TILT_JOY);
    int w = textW(m, 2) + 28;
    gfx->fillRoundRect(CX - w / 2, 120, w, 44, 12, UI_WHITE);
    sfTesto(m, 2, 136, UI_INK);
    int bw = 200, fw = (int)(bw * inclZeroResta() / INCL_ZERO_MS);
    gfx->fillRoundRect(CX - bw / 2, 178, bw, 10, 4, UI_TRACK);
    if (fw > 2) gfx->fillRoundRect(CX - bw / 2, 178, fw, 10, 4, UI_BAR_WARN);
    halFlush();
    return;
  }
  if (!sfInizio) sfInizio = sfUltimo = now;  // primo passo dopo lo zero: parte il tempo
  sfPasso(now);
  if (sfFineFino) {  // appena finita: il prossimo frame è la schermata finale
    halFlush();
    return;
  }

  sfPet(now);
  if (sfPuntiDa && now - sfPuntiDa < SF_PUNTI_MS) {  // i punti dell'ultimo atterraggio
    char p[8];
    snprintf(p, sizeof(p), "+%u", sfUltimiPunti);
    int y = (int)sfY - 120 - (int)((now - sfPuntiDa) / 30);
    gfx->setTextColor(sfUltimiPunti > 1 ? UI_BAR_WARN : UI_WHITE);
    setSize(3);
    setCur(SF_PET_X - textW(p, 3) / 2, y);
    printT(p);
  }

  // punteggio, record, cadute rimaste e tempo
  char b[8];
  snprintf(b, sizeof(b), "%u", sfPunti);
  sfTesto(b, 4, 30, UI_WHITE);
  char rec[24];
  snprintf(rec, sizeof(rec), T(S_REC_FMT), pet.surfHi);
  sfTesto(rec, 2, 76, UI_WHITE);
  for (int i = 0; i < 3; i++) {
    if (i < 3 - sfCadute) gfx->fillCircle(205 + i * 28, 104, 6, UI_BAR_BAD);
    else gfx->drawCircle(205 + i * 28, 104, 6, UI_WHITE);
  }
  uint32_t passato = now - sfInizio;
  int bw = 180, fw = passato >= SF_DURATA ? 0 : (int)((uint32_t)bw * (SF_DURATA - passato) / SF_DURATA);
  gfx->fillRoundRect(CX - bw / 2, 414, bw, 10, 4, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, 414, fw, 10, 4, UI_BAR_OK);

  halFlush();
  uint32_t dt = millis() - now;  // il mare si ridisegna a ogni frame: deve restare sotto 85 ms
  if (dt > sfRenderMax) sfRenderMax = dt;
}
