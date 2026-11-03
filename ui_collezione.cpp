// Collezione personale: SCR_COLLEZIONE con le 5 caselle, il dettaglio con
// RIPRENDI e LIBERA, e il dialogo «Conservare nella collezione?» alla fine della
// cerimonia (su SCR_CEREMONY). Le regole sono in pet.h (col*); qui solo la UI.

#include "ui_common.h"
#include "battle.h"

// ---------- geometria ----------

#define COL_CW 110  // casella della griglia: 3 in alto, 2 sotto
#define COL_CH 126
#define COL_GAP 6
#define COL_Y0 80
#define COL_BTN_Y 298  // pulsanti del dettaglio (alti 44, come nella scheda)
#define COL_BTN_H 44
#define COL_BTN_W 160
#define COL_TAKE_X (CX - 3 - COL_BTN_W)
#define COL_FREE_X (CX + 3)
// dialogo di conferma (SI/NO affiancati), sopra il dettaglio o la cerimonia
#define DLG_X 73
#define DLG_Y 150
#define DLG_W 320
#define DLG_H 190
#define DLG_BTN_Y 268
#define DLG_BTN_W 134
#define DLG_BTN_H 52
#define DLG_SI_X 93
#define DLG_NO_X 239
// scelta del posto da liberare (collezione piena): una riga per posto, poi RINUNCIA
#define SCE_Y 96
#define SCE_H 296
#define SCE_RIGA_Y 152
#define SCE_RIGA_H 34

enum : uint8_t { CONF_NO = 0, CONF_RIPRENDI, CONF_LIBERA };

static int8_t colSel = -1;      // posto in dettaglio, -1 = griglia
static uint8_t colConf = CONF_NO;
static int8_t domLibera = -1;   // domanda di fine ciclo: posto scelto da liberare (piena)

// ---------- disegno comune ----------

static const char *nomeRitratto(const PetRitratto &r) {
  return r.nick[0] ? r.nick : dexName(r.specie);
}

static const char *nomePet() {
  return pet.nick[0] ? pet.nick : dexName(pet.speciesId);
}

// testo centrato a dimensione 2, oppure 1 se a 2 supera `maxW`
static void testoAdatto(const char *s, int y, int maxW) {
  uint8_t sz = textW(s, 2) <= maxW ? 2 : 1;
  setSize(sz);
  setCur(centerX(s, sz), sz == 2 ? y : y + 4);
  printT(s);
}

static void pulsante(int x, int y, int w, int h, uint16_t col, uint16_t ink, const char *txt) {
  gfx->fillRoundRect(x, y, w, h, 12, col);
  gfx->setTextColor(ink);
  uint8_t sz = textW(txt, 2) <= w - 12 ? 2 : 1;
  setSize(sz);
  setCur(x + (w - textW(txt, sz)) / 2, y + h / 2 - (sz == 2 ? 7 : 4));
  printT(txt);
}

static bool dentro(int16_t x, int16_t y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

// domanda SI/NO con una riga di spiegazione (sub può essere nullptr)
static void dialogoSiNo(const char *titolo, const char *q, const char *sub) {
  gfx->fillRoundRect(DLG_X, DLG_Y, DLG_W, DLG_H, 16, UI_WHITE);
  gfx->drawRoundRect(DLG_X, DLG_Y, DLG_W, DLG_H, 16, UI_INK);
  gfx->setTextColor(UI_HINT);
  testoAdatto(titolo, DLG_Y + 16, DLG_W - 24);
  gfx->setTextColor(UI_INK);
  testoAdatto(q, DLG_Y + 46, DLG_W - 24);
  if (sub) testoAdatto(sub, DLG_Y + 78, DLG_W - 24);
  pulsante(DLG_SI_X, DLG_BTN_Y, DLG_BTN_W, DLG_BTN_H, UI_BAR_OK, UI_WHITE, T(S_YES));
  pulsante(DLG_NO_X, DLG_BTN_Y, DLG_BTN_W, DLG_BTN_H, UI_TRACK, UI_INK, T(S_NO));
}

// 1 = SI, 0 = NO o fuori dai pulsanti (annulla)
static bool tapSi(int16_t x, int16_t y) {
  return dentro(x, y, DLG_SI_X, DLG_BTN_Y, DLG_BTN_W, DLG_BTN_H);
}

// casella i della griglia: rettangolo
static void cellaRect(uint8_t i, int &x, int &y) {
  if (i < 3) {
    x = CX - (3 * COL_CW + 2 * COL_GAP) / 2 + i * (COL_CW + COL_GAP);
    y = COL_Y0;
  } else {
    x = CX - (2 * COL_CW + COL_GAP) / 2 + (i - 3) * (COL_CW + COL_GAP);
    y = COL_Y0 + COL_CH + COL_GAP;
  }
}

static const char *motivoT(ColMotivo m) {
  switch (m) {
    case COL_OCCUPATO: return T(S_COL_WHY_BUSY);
    case COL_NON_FINALE: return T(S_COL_WHY_FINAL);
    case COL_NON_IN_FORMA: return T(S_COL_WHY_FIT);
    case COL_OGGI: return T(S_COL_WHY_TODAY);
    default: return "";
  }
}

// ---------- SCR_COLLEZIONE ----------

// onEnter di SCR_COLLEZIONE: griglia, senza conferme aperte
void collezioneEnter() {
  colSel = -1;
  colConf = CONF_NO;
}

static void renderGriglia() {
  char head[40];
  snprintf(head, sizeof(head), T(S_COL_FMT), pet.colNumero(), COL_POSTI);
  gfx->setTextColor(UI_INK);
  setSize(3);
  int hs = textW(head, 3) <= 256 ? 3 : 2;  // la corda del cerchio a y 44
  setSize(hs);
  setCur(centerX(head, hs), hs == 3 ? 44 : 48);
  printT(head);

  for (uint8_t i = 0; i < COL_POSTI; i++) {
    int x, y;
    cellaRect(i, x, y);
    if (i >= pet.colNumero()) {  // posto vuoto
      gfx->drawRoundRect(x, y, COL_CW, COL_CH, 12, UI_TRACK);
      gfx->setTextColor(UI_HINT);
      uint8_t sz = textW(T(S_COL_EMPTY), 2) <= COL_CW - 8 ? 2 : 1;
      setSize(sz);
      setCur(x + (COL_CW - textW(T(S_COL_EMPTY), sz)) / 2, y + COL_CH / 2 - 7);
      printT(T(S_COL_EMPTY));
      continue;
    }
    const PetRitratto &r = pet.colRitratto(i);
    gfx->fillRoundRect(x, y, COL_CW, COL_CH, 12, UI_WHITE);
    gfx->drawRoundRect(x, y, COL_CW, COL_CH, 12, DEX_TBL[r.specie].accent);
    const uint8_t *t = thumbs.get(r.specie);
    if (t) drawThumb(t, x + (COL_CW - GAL_CELL) / 2, y, 2, false);
    if (r.shiny) {
      gfx->setTextColor(UI_BAR_WARN);
      setSize(2);
      setCur(x + COL_CW - 18, y + 6);
      printT("*");
    }
    const char *nm = nomeRitratto(r);
    uint8_t sz = textW(nm, 2) <= COL_CW - 2 ? 2 : 1;
    gfx->setTextColor(UI_INK);
    setSize(sz);
    setCur(x + (COL_CW - textW(nm, sz)) / 2, y + 82 + (sz == 2 ? 0 : 4));
    printT(nm);
    char lv[12];
    snprintf(lv, sizeof(lv), T(S_LVL_FMT), levelFromAge(r.ageMinutes));
    gfx->setTextColor(UI_HINT);
    setSize(2);
    setCur(x + (COL_CW - textW(lv, 2)) / 2, y + 102);
    printT(lv);
  }
}

static void renderDettaglio() {
  const PetRitratto &r = pet.colRitratto(colSel);
  const DexEntry &d = DEX_TBL[r.specie];
  char head[24];
  snprintf(head, sizeof(head), "%s%s", r.shiny ? "*" : "", nomeRitratto(r));
  gfx->setTextColor(d.accent);
  setSize(3);
  int hs = textW(head, 3) <= 234 ? 3 : 2;
  setSize(hs);
  setCur(centerX(head, hs), hs == 3 ? 36 : 40);
  printT(head);

  bool due = d.type2 < TYPE_COUNT;
  int tw = typePillW(d.type1) + (due ? 8 + typePillW(d.type2) : 0);
  int tx = CX - tw / 2;
  drawTypePill(tx, 68, d.type1);
  if (due) drawTypePill(tx + typePillW(d.type1) + 8, 68, d.type2);

  uint16_t st[4];
  statDaRitratto(r, st);
  drawCardStat(102, T(S_STAT_ATK), st[0], 260, UI_BAR_BAD);
  drawCardStat(124, T(S_STAT_DEF), st[1], 260, 0x4C98);
  drawCardStat(146, T(S_STAT_SPE), st[2], 260, UI_BAR_WARN);
  drawCardStat(168, T(S_STAT_SPC), st[3], 260, 0x9A7F);

  // livello e allenatore originale
  uint16_t lv = levelFromAge(r.ageMinutes);
  char lvb[12], riga[48];
  snprintf(lvb, sizeof(lvb), T(S_LVL_FMT), lv);
  const char *ot = r.otId == 0 ? T(S_COL_ME) : (r.otNome[0] ? r.otNome : "?");
  snprintf(riga, sizeof(riga), "%s   %s: %s", lvb, T(S_COL_OT), ot);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(riga, 2), 192);
  printT(riga);

  // le 4 mosse che userebbe in battaglia (come nella scheda)
  uint8_t mosse[BATTLE_MOSSE];
  battleMosseApprese(r.specie, lv > 100 ? 100 : (uint8_t)lv, mosse);
  for (uint8_t i = 0; i < BATTLE_MOSSE; i++) {
    int y = 216 + i * 20;
    uint8_t mv = mosse[i];
    if (!mv) {
      gfx->fillRoundRect(96, y + 1, 8, 13, 3, UI_TRACK);
      continue;
    }
    gfx->fillRoundRect(96, y + 1, 8, 13, 3, TYPE_COLOR[MOVE_TBL[mv].type % TYPE_COUNT]);
    gfx->setTextColor(UI_INK);
    setSize(2);
    setCur(112, y);
    printT(moveName(mv));
    char pp[12];
    snprintf(pp, sizeof(pp), "PP %u", MOVE_TBL[mv].pp);
    setCur(370 - textW(pp, 2), y);
    printT(pp);
  }

  ColMotivo m = pet.colPerche(colSel);
  pulsante(COL_TAKE_X, COL_BTN_Y, COL_BTN_W, COL_BTN_H, m == COL_OK ? UI_BAR_OK : UI_TRACK,
           m == COL_OK ? UI_WHITE : UI_INK, T(S_COL_TAKE));
  bool libera = pet.colLiberabile(colSel);  // no con uno scambio in corso o sospeso
  pulsante(COL_FREE_X, COL_BTN_Y, COL_BTN_W, COL_BTN_H, libera ? UI_BAR_BAD : UI_TRACK,
           libera ? UI_WHITE : UI_INK, T(S_COL_FREE));
  if (m != COL_OK) {  // perché RIPRENDI è grigio
    gfx->setTextColor(UI_BAR_BAD);
    testoAdatto(motivoT(m), 354, 344);  // la corda a y 354 è ~386 px
  }
  gfx->setTextColor(UI_HINT);
  setSize(2);
  setCur(centerX(T(S_DETAIL_BACK), 2), 392);
  printT(T(S_DETAIL_BACK));

  if (colConf != CONF_NO) {
    char q[64];
    if (colConf == CONF_RIPRENDI) snprintf(q, sizeof(q), T(S_COL_TAKE_Q), nomeRitratto(r));
    else snprintf(q, sizeof(q), T(S_COL_FREE_Q), nomeRitratto(r));
    const char *sub = colConf == CONF_LIBERA ? nullptr
                      : pet.isEgg()          ? T(S_COL_EGG_LOST)
                                             : T(S_COL_SWAP);
    dialogoSiNo(T(S_COL_TITLE), q, sub);
  }
}

// la griglia e il dettaglio si ridisegnano a ogni giro, come la scheda: niente da
// caricare (le miniature sono già in PSRAM), quindi aprirla non tocca l'heap
void renderCollezione() {
  if (colSel >= pet.colNumero()) colSel = -1;  // posto sparito (console, ripristino)
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  if (colSel < 0) renderGriglia();
  else renderDettaglio();
  halFlush();
}

// SCR_COLLEZIONE: griglia -> dettaglio di un posto occupato; nel dettaglio RIPRENDI e
// LIBERA chiedono conferma, il resto torna alla griglia; l'intestazione della griglia
// torna alla galleria
void collezioneTap(int16_t x, int16_t y) {
  if (colSel >= 0 && colConf != CONF_NO) {  // conferma aperta
    uint8_t c = colConf;
    colConf = CONF_NO;
    if (!tapSi(x, y)) return;  // NO o fuori: annulla
    halSfx(SFX_TAP);
    if (c == CONF_RIPRENDI) {
      if (pet.colRiprendi(colSel)) goScreen(SCR_MAIN);
      else halSfx(SFX_DENY);
    } else if (pet.colLibera(colSel)) {
      colSel = -1;
    } else {
      halSfx(SFX_DENY);
    }
    return;
  }
  if (colSel >= 0) {
    if (dentro(x, y, COL_TAKE_X, COL_BTN_Y, COL_BTN_W, COL_BTN_H)) {
      if (pet.colPerche(colSel) == COL_OK) colConf = CONF_RIPRENDI;
      else halSfx(SFX_DENY);  // il motivo è già scritto sotto
    } else if (dentro(x, y, COL_FREE_X, COL_BTN_Y, COL_BTN_W, COL_BTN_H)) {
      if (pet.colLiberabile(colSel)) colConf = CONF_LIBERA;
      else halSfx(SFX_DENY);  // bloccato da uno scambio: il motivo è scritto sotto
    } else {
      colSel = -1;
    }
    return;
  }
  if (y < 72) {  // intestazione: torna alla galleria
    goScreen(SCR_GALLERY);
    return;
  }
  for (uint8_t i = 0; i < pet.colNumero(); i++) {
    int cx, cy;
    cellaRect(i, cx, cy);
    if (dentro(x, y, cx, cy, COL_CW, COL_CH)) {
      colSel = i;
      halSfx(SFX_TAP);
      return;
    }
  }
}

// SCR_COLLEZIONE, scorrimento orizzontale: dal dettaglio alla griglia, dalla griglia alla
// galleria
void collezioneSwipe(int) {
  if (colSel >= 0) {
    colSel = -1;
    colConf = CONF_NO;
  } else {
    goScreen(SCR_GALLERY);
  }
}

// ---------- domanda di fine ciclo (su SCR_CEREMONY) ----------

// si disegna sopra la scena della cerimonia, ferma all'ultimo fotogramma
void renderColDomanda() {
  if (!pet.colChiede()) {
    domLibera = -1;
    return;
  }
  char q[64];
  snprintf(q, sizeof(q), T(S_COL_KEEP_Q), nomePet());
  if (domLibera >= 0) {  // collezione piena, posto scelto: conferma
    char lib[64];
    snprintf(lib, sizeof(lib), T(S_COL_FREE_Q), nomeRitratto(pet.colRitratto(domLibera)));
    dialogoSiNo(T(S_COL_TITLE), lib, q);
    return;
  }
  if (!pet.colPiena()) {
    char head[40];
    snprintf(head, sizeof(head), T(S_COL_FMT), pet.colNumero(), COL_POSTI);
    dialogoSiNo(head, q, nullptr);
    return;
  }
  // piena: si sceglie chi lasciare libero, oppure si rinuncia
  gfx->fillRoundRect(DLG_X, SCE_Y, DLG_W, SCE_H, 16, UI_WHITE);
  gfx->drawRoundRect(DLG_X, SCE_Y, DLG_W, SCE_H, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  testoAdatto(q, SCE_Y + 14, DLG_W - 24);
  gfx->setTextColor(UI_BAR_BAD);
  testoAdatto(T(S_COL_FULL), SCE_Y + 34, DLG_W - 24);
  for (uint8_t i = 0; i < COL_POSTI; i++) {
    const PetRitratto &r = pet.colRitratto(i);
    char riga[40], lv[12];
    snprintf(lv, sizeof(lv), T(S_LVL_FMT), levelFromAge(r.ageMinutes));
    snprintf(riga, sizeof(riga), "%s%s  %s", r.shiny ? "*" : "", nomeRitratto(r), lv);
    int ry = SCE_RIGA_Y + i * SCE_RIGA_H;
    gfx->fillRoundRect(DLG_X + 20, ry, DLG_W - 40, SCE_RIGA_H - 4, 8, UI_TRACK);
    gfx->setTextColor(UI_INK);
    testoAdatto(riga, ry + 8, DLG_W - 52);
  }
  pulsante(DLG_X + 60, SCE_RIGA_Y + COL_POSTI * SCE_RIGA_H + 2, DLG_W - 120, 34, UI_BAR_BAD,
           UI_WHITE, T(S_COL_GIVE_UP));
}

// onTap di SCR_CEREMONY: conta solo durante la domanda della collezione
void colDomandaTap(int16_t x, int16_t y) {
  if (!pet.colChiede()) return;
  if (domLibera >= 0) {  // conferma del posto da liberare
    int8_t i = domLibera;
    domLibera = -1;
    if (tapSi(x, y)) {
      halSfx(SFX_TAP);
      pet.colConserva(i);
    }
    return;
  }
  if (!pet.colPiena()) {
    if (tapSi(x, y)) {
      halSfx(SFX_TAP);
      pet.colConserva();
    } else if (dentro(x, y, DLG_NO_X, DLG_BTN_Y, DLG_BTN_W, DLG_BTN_H)) {
      halSfx(SFX_TAP);
      pet.colRinuncia();
    }
    return;  // fuori dai pulsanti: la domanda resta
  }
  if (x < DLG_X + 20 || x >= DLG_X + DLG_W - 20) return;
  if (y >= SCE_RIGA_Y && y < SCE_RIGA_Y + COL_POSTI * SCE_RIGA_H) {
    domLibera = (y - SCE_RIGA_Y) / SCE_RIGA_H;
    halSfx(SFX_TAP);
  } else if (y >= SCE_RIGA_Y + COL_POSTI * SCE_RIGA_H &&
             y < SCE_RIGA_Y + COL_POSTI * SCE_RIGA_H + 40) {
    halSfx(SFX_TAP);
    pet.colRinuncia();
  }
}
