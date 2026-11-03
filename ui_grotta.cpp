// Grotta Diglett: si inclina la scheda (o si usa il joystick sul touch) e il
// pet scivola in un'arena rotonda, raccoglie bacche e schiva i Diglett che spuntano e i
// massi di Frana. Allena la DIFESA (Pet::grottaResult). I comandi comuni (zero, filtro,
// joystick, schermo sveglio) sono incl*() in ui_common.cpp. La musica è Dungeon2 di
// Rosso/Blu, con la batteria.

#include "ui_common.h"
#include "musica_dati.h"  // MUS_* (le tabelle non usate non finiscono nel firmware)

#define GR_R 205            // raggio dell'arena, come la parete della palla
#define GR_PET_R 26         // hitbox fissa del pet, indipendente dallo sprite
#define GR_DURATA 90000     // ms di una partita
#define GR_BACCA_MS 6000    // vita di una bacca (e della Caramella Rara)
#define GR_CARAMELLA_MS 20000
#define GR_TREMA_MS 1000    // Diglett: la terra trema...
#define GR_FUORI_MS 1200    //   ...poi spunta
#define GR_OMBRA_MS 1200    // Frana: l'ombra cresce...
#define GR_CADE_MS 200      //   ...il masso cade...
#define GR_DETRITI_MS 500   //   ...e restano i detriti, innocui
#define GR_OMBRA_R 38
#define GR_INVULN_MS 1000
#define GR_ACC 2.0f         // px/passo² a inclinazione piena
#define GR_ATTRITO 0.86f    // velocità che resta dopo un passo da 85 ms
#define GR_VMAX 12.0f       // px/passo

enum : uint8_t { GR_BACCA_R, GR_BACCA_B, GR_BACCA_G, GR_CARAMELLA };
enum : uint8_t { GR_DIGLETT, GR_DUGTRIO, GR_FRANA };

struct GrOggetto { float x, y; uint32_t t0; uint8_t tipo; bool vivo; };
struct GrPericolo { float x, y; uint32_t t0; uint8_t tipo; bool vivo, colpito; };

static GrOggetto grOgg[4];
static GrPericolo grPer[8];
static float grX, grY, grVX, grVY;
static uint32_t grInizio, grUltimo, grFineFino, grInvulnFino, grHopDa, grProssimo, grBaccaDa,
    grCaramellaDa;
static uint16_t grPunti;
static uint8_t grColpi, grDif;
static bool grNuovoRecord;
static int8_t grVerso;  // ultimo verso del cammino: -1 sinistra, +1 destra
static uint32_t grRenderMax;  // il frame più lento della partita (ms), per la seriale

// sassolini fissi del fondo (offset dal centro)
static const int16_t GR_SASSI[][3] = {
  {-120, -60, 5}, {90, -110, 4}, {140, 40, 6}, {-60, 120, 4}, {30, 150, 5},
  {-150, 30, 3}, {60, -30, 3}, {-20, -140, 4}, {110, 120, 3}, {-100, -130, 3},
};

void startGrotta() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  goScreen(SCR_GROTTA);
}

void grottaEnter() {
  for (auto &o : grOgg) o.vivo = false;
  for (auto &p : grPer) p.vivo = false;
  grX = CX;
  grY = CY + 40;
  grVX = grVY = 0;
  grPunti = 0;
  grColpi = 0;
  grDif = 0;
  grNuovoRecord = false;
  grFineFino = grInvulnFino = grHopDa = 0;
  grInizio = 0;  // la partita parte finito lo zero
  grVerso = 1;
  grRenderMax = 0;
  inclInizia();
  halMusica(MUS_DUNGEON2);
}

void grottaExit() {
  inclFine();
  halMusica(MUS_NESSUNA);
}

// tocco sull'intestazione = abbandonare senza premio (come la palla)
void grottaTap(int16_t, int16_t y) {
  if (grFineFino) return;
  if (y < 72) goScreen(SCR_MAIN);
}

// un punto a caso nell'arena, entro `r` dal centro
static void grPunto(float r, float *x, float *y) {
  float a = random(628) / 100.0f, d = r * sqrtf(random(1001) / 1000.0f);
  *x = CX + d * cosf(a);
  *y = CY + d * sinf(a);
}

// un punto vicino a (x, y), entro `r`, ma dentro l'arena
static void grVicino(float x, float y, float r, float *ox, float *oy) {
  float a = random(628) / 100.0f, d = random((long)r + 1);
  float px = x + d * cosf(a) - CX, py = y + d * sinf(a) - CY;
  float m = sqrtf(px * px + py * py), lim = GR_R - 40;
  if (m > lim) { px *= lim / m; py *= lim / m; }
  *ox = CX + px;
  *oy = CY + py;
}

static GrPericolo *grSlot() {
  for (auto &p : grPer)
    if (!p.vivo) return &p;
  return nullptr;
}

static void grNuovoPericolo(uint32_t now) {
  long dado = random(100);
  uint8_t tipo = (grPunti < 5 || dado < 50) ? GR_DIGLETT : (grPunti >= 15 && dado >= 85) ? GR_DUGTRIO
                                                                                          : GR_FRANA;
  if (tipo == GR_DUGTRIO) {  // tre buche in fila, attraverso la strada del pet
    float a = random(628) / 100.0f, cx, cy;
    grVicino(grX, grY, 50, &cx, &cy);
    for (int i = -1; i <= 1; i++) {
      GrPericolo *p = grSlot();
      if (!p) return;
      float px = cx + i * 58 * cosf(a) - CX, py = cy + i * 58 * sinf(a) - CY;
      float m = sqrtf(px * px + py * py), lim = GR_R - 30;
      if (m > lim) { px *= lim / m; py *= lim / m; }
      *p = { CX + px, CY + py, now, GR_DUGTRIO, true, false };
    }
    return;
  }
  GrPericolo *p = grSlot();
  if (!p) return;
  float x, y;
  grVicino(grX, grY, tipo == GR_FRANA ? 40 : 90, &x, &y);
  *p = { x, y, now, tipo, true, false };
}

static void grColpito(uint32_t now) {
  if (timeLeft(grInvulnFino)) return;
  grColpi++;
  grInvulnFino = now + GR_INVULN_MS;
  halSfx(SFX_HIT);
}

static void grFine() {
  halMusica(MUS_NESSUNA);  // la fine si sente da sola
  grNuovoRecord = grPunti > pet.grottaHi;
  grDif = pet.grottaResult(grPunti);
  halSfx(grNuovoRecord && grPunti > 0 ? SFX_MEDAL : SFX_LEVEL);
  grFineFino = millis() + 4000;
  Serial.printf("GROTTA punti=%u colpi=%u s=%lu dif=+%u rec=%u render=%lums\n", grPunti, grColpi,
                (unsigned long)((millis() - grInizio) / 1000), grDif, pet.grottaHi,
                (unsigned long)grRenderMax);
}

// passo a tempo reale (come stepGame): k = passi da 85 ms trascorsi
static void grPasso(uint32_t now) {
  float k = grUltimo ? (now - grUltimo) / 85.0f : 1.0f;
  if (k > 3.0f) k = 3.0f;
  grUltimo = now;

  // il pet: accelera verso l'inclinazione, con attrito e velocità massima
  grVX += inclX() * GR_ACC * k;
  grVY += inclY() * GR_ACC * k;
  float att = powf(GR_ATTRITO, k);
  grVX *= att;
  grVY *= att;
  float v = sqrtf(grVX * grVX + grVY * grVY);
  if (v > GR_VMAX) { grVX *= GR_VMAX / v; grVY *= GR_VMAX / v; }
  grX += grVX * k;
  grY += grVY * k;
  float dx = grX - CX, dy = grY - CY, d = sqrtf(dx * dx + dy * dy), lim = GR_R - GR_PET_R;
  if (d > lim) {  // rimbalzo smorzato sulla parete
    float nx = dx / d, ny = dy / d, dot = grVX * nx + grVY * ny;
    if (dot > 0) {
      grVX = (grVX - 2 * dot * nx) * 0.6f;
      grVY = (grVY - 2 * dot * ny) * 0.6f;
    }
    grX = CX + nx * lim;
    grY = CY + ny * lim;
  }
  if (grVX > 0.8f) grVerso = 1;
  else if (grVX < -0.8f) grVerso = -1;

  // bacche: due in campo, la Caramella Rara ogni 20 s; spariscono dopo 6 s
  int bacche = 0;
  for (auto &o : grOgg) {
    if (!o.vivo) continue;
    if (now - o.t0 >= GR_BACCA_MS) { o.vivo = false; continue; }
    float ox = o.x - grX, oy = o.y - grY;
    if (ox * ox + oy * oy < (GR_PET_R + 16) * (GR_PET_R + 16)) {
      grPunti += o.tipo == GR_CARAMELLA ? 5 : 1;
      o.vivo = false;
      grHopDa = now;
      halSfx(SFX_PLAY);
      continue;
    }
    if (o.tipo != GR_CARAMELLA) bacche++;
  }
  bool caramella = (int32_t)(now - grCaramellaDa) >= 0;
  if ((bacche < 2 && (int32_t)(now - grBaccaDa) >= 0) || caramella) {
    for (auto &o : grOgg) {
      if (o.vivo) continue;
      float x, y;
      for (int t = 0; t < 8; t++) {  // lontano dal pet
        grPunto(GR_R - 45, &x, &y);
        if ((x - grX) * (x - grX) + (y - grY) * (y - grY) > 90 * 90) break;
      }
      o = { x, y, now, caramella ? (uint8_t)GR_CARAMELLA : (uint8_t)random(3), true };
      if (caramella) grCaramellaDa = now + GR_CARAMELLA_MS;
      else grBaccaDa = now + 700;
      break;
    }
  }

  // pericoli: più fitti man mano che i punti salgono
  if ((int32_t)(now - grProssimo) >= 0) {
    grNuovoPericolo(now);
    int32_t iv = 2600 - (int32_t)grPunti * 50;
    if (iv < 800) iv = 800;
    grProssimo = now + iv + random(600);
  }
  for (auto &p : grPer) {
    if (!p.vivo) continue;
    uint32_t t = now - p.t0;
    float px = p.x - grX, py = p.y - grY, d2 = px * px + py * py;
    if (p.tipo == GR_FRANA) {
      if (t >= GR_OMBRA_MS + GR_CADE_MS + GR_DETRITI_MS) { p.vivo = false; continue; }
      if (!p.colpito && t >= GR_OMBRA_MS + GR_CADE_MS) {  // l'impatto: conta l'ombra
        p.colpito = true;
        float r = GR_OMBRA_R + GR_PET_R / 2;
        if (d2 < r * r) grColpito(now);
      }
    } else {
      if (t >= GR_TREMA_MS + GR_FUORI_MS) { p.vivo = false; continue; }
      float r = GR_PET_R + 20;
      if (!p.colpito && t >= GR_TREMA_MS && d2 < r * r) {
        p.colpito = true;
        grColpito(now);
      }
    }
  }

  if (grColpi >= 3 || now - grInizio >= GR_DURATA) grFine();
}

// Diglett disegnato, se la SD non ha le miniature: base sul suolo a `baseY`
void grottaDisegnaDiglett(int cx, int baseY, int s) {
  uint16_t corpo = C565(0x9a, 0x6a, 0x46);
  gfx->fillRoundRect(cx - 7 * s, baseY - 16 * s, 14 * s, 18 * s, 7 * s, corpo);
  gfx->fillRect(cx - 4 * s, baseY - 12 * s, s + 1, 3 * s, INK_K);
  gfx->fillRect(cx + 3 * s, baseY - 12 * s, s + 1, 3 * s, INK_K);
  gfx->fillEllipse(cx, baseY - 7 * s, 3 * s, 2 * s, C565(0xf0, 0x8c, 0xa8));
  gfx->fillEllipse(cx, baseY + s, 10 * s, 3 * s, C565(0x4a, 0x34, 0x22));
}

// miniatura del Pokédex (dex 50/51) con la base a `baseY`, o il disegno
static void grDiglett(int16_t dex, int cx, int baseY) {
  const uint8_t *th = thumbs.get(dex);
  if (th) drawThumb(th, cx - GAL_CELL / 2, baseY + 4 - th[1] - GAL_CELL / 2, 2, false);
  else grottaDisegnaDiglett(cx, baseY, 2);
}

static void grSfondo() {
  gfx->fillScreen(C565(0x2b, 0x21, 0x18));
  gfx->fillCircle(CX, CY, GR_R + 6, C565(0x4a, 0x36, 0x26));
  gfx->fillCircle(CX, CY, GR_R, C565(0x7a, 0x5a, 0x3c));
  for (auto &s : GR_SASSI) gfx->fillCircle(CX + s[0], CY + s[1], s[2], C565(0x5e, 0x45, 0x2e));
}

static void grDisegnaPericoli(uint32_t now) {
  uint16_t buca = C565(0x3a, 0x2a, 0x1c), terra = C565(0x9c, 0x7a, 0x52);
  for (auto &p : grPer) {
    if (!p.vivo) continue;
    uint32_t t = now - p.t0;
    int x = (int)p.x, y = (int)p.y;
    if (p.tipo == GR_FRANA) {
      if (t < GR_OMBRA_MS + GR_CADE_MS) {
        uint32_t to = t < GR_OMBRA_MS ? t : GR_OMBRA_MS;
        int r = 10 + (int)((GR_OMBRA_R - 10) * to / GR_OMBRA_MS);
        gfx->fillEllipse(x, y, r, r * 2 / 3, C565(0x3e, 0x2e, 0x20));
        if (t >= GR_OMBRA_MS) {  // il masso che cade dall'alto
          int h = (int)(140 * (GR_OMBRA_MS + GR_CADE_MS - t) / GR_CADE_MS);
          gfx->fillCircle(x, y - 14 - h, 26, C565(0x8c, 0x88, 0x80));
          gfx->drawCircle(x, y - 14 - h, 26, INK_K);
        }
      } else {  // detriti
        gfx->fillCircle(x - 16, y + 2, 9, C565(0x8c, 0x88, 0x80));
        gfx->fillCircle(x + 12, y - 4, 12, C565(0x7a, 0x76, 0x70));
        gfx->fillCircle(x + 2, y + 10, 7, C565(0x9a, 0x96, 0x8e));
      }
      continue;
    }
    if (t < GR_TREMA_MS) {  // la terra trema: crepe e zolle che saltano
      int j = (int)((now / 40) % 3) - 1;
      gfx->fillEllipse(x + j, y, 14 + (int)(10 * t / GR_TREMA_MS), 7, buca);
      for (int i = 0; i < 4; i++) {
        int a = (int)((now / 60 + i * 7) % 5);
        gfx->fillRect(x - 22 + i * 13, y - 6 - a * 2, 4, 4, terra);
      }
    } else {
      gfx->fillEllipse(x, y, 24, 9, buca);
      grDiglett(p.tipo == GR_DUGTRIO ? 51 : 50, x, y);
    }
  }
}

static void grDisegnaPet(uint32_t now) {
  int x = (int)grX, gy = (int)grY + GR_PET_R;
  bool inv = timeLeft(grInvulnFino) > 0;
  if (inv && (now / 100) % 2) return;  // lampeggia mentre è invulnerabile
  if (!pmd.loaded) {
    gfx->fillCircle(x, (int)grY, GR_PET_R, DEX_TBL[pet.speciesId].accent);
    return;
  }
  uint8_t act = PMD_IDLE;
  uint32_t t = now;
  bool giro = true;
  if (inv && pmd.has(PMD_HURT)) {
    act = PMD_HURT;
    t = now - (grInvulnFino - GR_INVULN_MS);
    giro = false;
  } else if (grHopDa && now - grHopDa < 400 && pmd.has(PMD_HOP)) {
    act = PMD_HOP;
    t = now - grHopDa;
    giro = false;
  } else if (fabsf(grVX) + fabsf(grVY) > 1.2f) {
    act = grVerso > 0 ? PMD_WALKR : PMD_WALKL;
    if (!pmd.has(act)) act = PMD_IDLE;
  }
  drawPmdAct(act, x, gy, t, giro, false, 2);
}

static void grTesto(const char *s, uint8_t size, int y, uint16_t col) {
  gfx->setTextColor(col);
  setSize(size);
  setCur(centerX(s, size), y);
  printT(s);
}

void renderGrotta() {
  uint32_t now = millis();
  grSfondo();

  if (grFineFino) {  // schermata finale, come la palla
    if (!timeLeft(grFineFino)) {
      goScreen(SCR_MAIN);
      return;
    }
    char b[22];
    snprintf(b, sizeof(b), T(S_SCORE_FMT), grPunti);
    grTesto(b, 4, 140, UI_WHITE);
    char g[24];
    snprintf(g, sizeof(g), T(S_DEF_GAIN_FMT), grDif);
    grTesto(g, 3, 196, UI_BAR_OK);
    if (grNuovoRecord && grPunti > 0) {
      grTesto(T(S_NEW_RECORD), 2, 244, UI_BAR_WARN);
    } else {
      char r[20];
      snprintf(r, sizeof(r), T(S_RECORD_FMT), pet.grottaHi);
      grTesto(r, 2, 244, UI_WHITE);
    }
    grTesto(grPunti >= 10 ? T(S_GREAT_JOY) : T(S_PLUS_JOY), 2, 280, UI_WHITE);
    halFlush();
    return;
  }

  if (inclAggiorna()) {  // zero: il pet aspetta al centro, e il cartello dice cosa fare
    grDisegnaPet(now);
    const char *m = inclSensore() ? T(S_TILT_HOLD) : T(S_TILT_JOY);
    int w = textW(m, 2) + 28;
    gfx->fillRoundRect(CX - w / 2, 120, w, 44, 12, UI_WHITE);
    grTesto(m, 2, 136, UI_INK);
    int bw = 200, fw = (int)(bw * inclZeroResta() / INCL_ZERO_MS);
    gfx->fillRoundRect(CX - bw / 2, 178, bw, 10, 4, UI_TRACK);
    if (fw > 2) gfx->fillRoundRect(CX - bw / 2, 178, fw, 10, 4, UI_BAR_WARN);
    halFlush();
    return;
  }
  if (!grInizio) {  // primo passo dopo lo zero: parte il tempo
    grInizio = grUltimo = now;
    grProssimo = now + 2000;
    grBaccaDa = now;
    grCaramellaDa = now + GR_CARAMELLA_MS;
  }
  grPasso(now);
  if (grFineFino) {  // appena finita: il prossimo frame è la schermata finale
    halFlush();
    return;
  }

  grDisegnaPericoli(now);
  for (auto &o : grOgg) {
    if (!o.vivo) continue;
    if (now - o.t0 > GR_BACCA_MS - 1500 && (now / 150) % 2) continue;  // sta per sparire
    static const char *const *const ICONE[4] = { SPR_ICON_FOOD, SPR_ICON_BERRY_B, SPR_ICON_BERRY_G,
                                                 SPR_ICON_CANDY };
    drawMap(ICONE[o.tipo], 16, (int)o.x - 16, (int)o.y - 16, 2, false);
  }
  grDisegnaPet(now);

  // punteggio, record, colpi rimasti e tempo
  char b[8];
  snprintf(b, sizeof(b), "%u", grPunti);
  grTesto(b, 4, 30, UI_WHITE);
  char rec[24];
  snprintf(rec, sizeof(rec), T(S_REC_FMT), pet.grottaHi);
  grTesto(rec, 2, 76, UI_WHITE);
  for (int i = 0; i < 3; i++) {
    if (i < 3 - grColpi) gfx->fillCircle(205 + i * 28, 104, 6, UI_BAR_BAD);
    else gfx->drawCircle(205 + i * 28, 104, 6, UI_TRACK);
  }
  uint32_t passato = now - grInizio;
  int bw = 180, fw = passato >= GR_DURATA ? 0 : (int)((uint32_t)bw * (GR_DURATA - passato) / GR_DURATA);
  gfx->fillRoundRect(CX - bw / 2, 414, bw, 10, 4, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, 414, fw, 10, 4, UI_BAR_OK);

  halFlush();
  uint32_t dt = millis() - now;  // disegno e invio: deve restare sotto gli 85 ms
  if (dt > grRenderMax) grRenderMax = dt;
}
