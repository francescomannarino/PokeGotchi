// Galleria del Pokédex: griglia delle miniature e vista dettaglio; sotto la griglia il
// pulsante della collezione (ui_collezione.cpp).

#include "ui_common.h"

// ---------- galleria pokédex ----------

#define GAL_X 73
#define GAL_Y 74   // la griglia finisce a 394: sotto c'è posto per un pulsante alto 38
// pulsante della collezione sotto la griglia; la zona di tocco è più grande del
// disegno (y GAL_COL_TY..GAL_COL_TY+GAL_COL_TH, x CX±GAL_COL_TW/2), soprattutto in basso
#define GAL_COL_Y 400
#define GAL_COL_W 200
#define GAL_COL_H 38
#define GAL_COL_TY 396
#define GAL_COL_TH 56
#define GAL_COL_TW 220
#define GAL_DOTS_Y 451

static bool galleryDirty = false;
static int galleryPage = 0;        // 10 pagine da 16
static int16_t galleryDetail = 0;  // dex in vista dettaglio, 0 = griglia

// onEnter di SCR_GALLERY: griglia dalla prima pagina
void galleryEnter() {
  galleryPage = 0;
  galleryDetail = 0;
  galleryDirty = true;
}

// onExit di SCR_GALLERY: libera lo sprite grande della vista dettaglio
void galleryExit() {
  galleryDetail = 0;
  galleryPmd.unload();
}

void renderGallery() {
  if (galleryDetail) {  // vista dettaglio: si ridisegna sempre (animata)
    gfx->fillScreen(RGB565_BLACK);
    gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
    const DexEntry &d = DEX_TBL[galleryDetail];
    bool reg = pet.isRegistered(galleryDetail);
    char head[32];
    snprintf(head, sizeof(head), "N.%03d %s%s", galleryDetail,
             pet.isShinyRegistered(galleryDetail) ? "*" : "", reg ? dexName(galleryDetail) : "???");
    gfx->setTextColor(reg ? d.accent : UI_INK);
    // si rimpicciolisce con i nomi lunghi (non ci stanno a t3), misurando invece di
    // contare byte; 13 caratteri a dimensione 3 sono i 234 px di prima.
    setSize(3);
    int gts = (textW(head, 3) <= 234) ? 3 : 2;
    setSize(gts);
    setCur(centerX(head, gts), gts == 3 ? 56 : 60);
    printT(head);
    if (galleryPmd.loaded) {
      // animato e a colori se registrato; silhouette statica altrimenti (stile "?")
      drawPmdActM(galleryPmd, PMD_IDLE, CX, 300, reg ? millis() : 0, true, !reg, 6);
    } else {
      const uint8_t *t = thumbs.get(galleryDetail);
      if (t) drawThumb(t, CX - GAL_CELL, 135, 4, !reg);
    }
    gfx->setTextColor(UI_INK);
    setSize(2);
    setCur(centerX(T(S_DETAIL_BACK), 2), 408);
    printT(T(S_DETAIL_BACK));
    halFlush();
    return;
  }

  if (!galleryDirty) return;  // la griglia è statica
  galleryDirty = false;

  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  char head[24];
  snprintf(head, sizeof(head), T(S_POKEDEX_FMT), pet.registeredCount());
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(head, 3), 36);
  printT(head);

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      int16_t dex = galleryPage * 16 + r * 4 + c + 1;
      if (dex > 151) break;
      int x = GAL_X + c * GAL_CELL, y = GAL_Y + r * GAL_CELL;
      const uint8_t *t = thumbs.get(dex);
      if (t) {
        drawThumb(t, x, y, 2, !pet.isRegistered(dex));
        if (pet.isShinyRegistered(dex)) {
          gfx->setTextColor(UI_BAR_WARN);
          setSize(2);
          setCur(x + 62, y + 4);
          printT("*");
        }
      } else {
        char num[6];
        snprintf(num, sizeof(num), "%d", dex);
        gfx->setTextColor(UI_HINT);
        setSize(2);
        setCur(x + 24, y + 32);
        printT(num);
      }
    }
  }
  // pulsante della collezione
  gfx->fillRoundRect(CX - GAL_COL_W / 2, GAL_COL_Y, GAL_COL_W, GAL_COL_H, 14, UI_BAR_WARN);
  gfx->setTextColor(UI_INK);
  uint8_t cs = textW(T(S_COL_TITLE), 2) <= GAL_COL_W - 16 ? 2 : 1;
  setSize(cs);
  setCur(centerX(T(S_COL_TITLE), cs), GAL_COL_Y + GAL_COL_H / 2 - (cs == 2 ? 7 : 4));
  printT(T(S_COL_TITLE));
  // punti di pagina
  for (int i = 0; i < 10; i++) {
    if (i == galleryPage) gfx->fillCircle(170 + i * 14, GAL_DOTS_Y, 4, UI_INK);
    else gfx->drawCircle(170 + i * 14, GAL_DOTS_Y, 3, UI_INK);
  }
  halFlush();
}

void galleryTap(int16_t x, int16_t y) {
  if (galleryDetail) {  // tornare alla griglia
    galleryDetail = 0;
    galleryPmd.unload();
    galleryDirty = true;
    return;
  }
  if (y < 72) {  // toccare l'intestazione = uscire
    goScreen(SCR_MAIN);
    return;
  }
  if (y >= GAL_COL_TY && y < GAL_COL_TY + GAL_COL_TH && abs(x - CX) <= GAL_COL_TW / 2) {
    halSfx(SFX_TAP);
    goScreen(SCR_COLLEZIONE);
    return;
  }
  // stesso motivo della tastiera: filtrare prima di dividere
  if (x < GAL_X || y < GAL_Y) return;
  int c = (x - GAL_X) / GAL_CELL, r = (y - GAL_Y) / GAL_CELL;
  if (c > 3 || r > 3) return;
  int16_t dex = galleryPage * 16 + r * 4 + c + 1;
  if (dex > 151) return;
  galleryDetail = dex;
  galleryPmd.load(dex, pet.isShinyRegistered(dex));
}

// SCR_GALLERY, scorrimento orizzontale: pagine della griglia; dal dettaglio si torna
// alla griglia, dalla prima pagina alla principale
void gallerySwipe(int dir) {
  if (galleryDetail) {  // nel dettaglio: tornare alla griglia
    galleryDetail = 0;
    galleryPmd.unload();
    galleryDirty = true;
    return;
  }
  int np = galleryPage - dir;  // scorrere a sinistra avanza di pagina
  if (np < 0) {                // tornare indietro dalla prima = uscire
    goScreen(SCR_MAIN);
    return;
  }
  if (np > 9) np = 9;
  if (np != galleryPage) {
    galleryPage = np;
    galleryDirty = true;
  }
}
