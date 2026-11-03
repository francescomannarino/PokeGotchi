// Scheda del Pokémon (profilo, statistiche, medaglie, progressi) e tastiera per rinominarlo.

#include "ui_common.h"
#include "battle.h"

char nameBuf[12] = "";
uint8_t nameLen = 0;
static uint8_t cardPage = 0;  // 0 profilo, 1 stat, 2 medaglie, 3 progressi

// ---------- scheda del Pokémon (scorrimento verticale) ----------

// x da cui partono le barre della scheda. Era fisso a 150, che bastava e avanzava per
// etichette latine di 3 caratteri ma non per quelle giapponesi, più larghe: こうげき finiva
// dentro la barra. Si calcola a partire dall'etichetta più lunga perché le quattro
// barre restino allineate in qualsiasi lingua, con il valore originale come minimo
// (così nelle lingue latine non cambia nulla). Richiede di avere già impostato la
// dimensione di testo 2, perché textW() ne ha bisogno.
static int statBarX() {
  const StrId ids[] = { S_STAT_ATK, S_STAT_DEF, S_STAT_SPE, S_STAT_SPC, S_STAT_WGT, S_VIN };
  int ancho = 0;
  for (StrId id : ids) {
    int w = textW(T(id), 2);
    if (w > ancho) ancho = w;
  }
  int x = 96 + ancho + 12;   // 12 px di aria tra etichetta e barra
  return x < 150 ? 150 : x;
}

void drawCardStat(int y, const char *label, uint16_t val, uint16_t maxBar, uint16_t color) {
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(96, y);
  printT(label);
  char num[8];
  snprintf(num, sizeof(num), "%u", val);
  setCur(330, y);
  printT(num);
  int bx = statBarX();
  int bw = 310 - bx;   // la barra finisce sempre a 310, lasciando aria fino al numero
  int fw = (int)val * bw / maxBar;
  if (fw > bw) fw = bw;
  gfx->fillRoundRect(bx, y + 2, bw, 11, 3, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(bx, y + 2, fw, 11, 3, color);
}

// medaglie nella scheda: badge con etichetta, colorato se ottenuta
void drawMedalBadge(int x, int y, int i) {
  bool got = pet.hasMedal(1 << i);
  gfx->fillRoundRect(x, y, 100, 24, 6, got ? UI_BAR_OK : UI_TRACK);
  if (!got) gfx->drawRoundRect(x, y, 100, 24, 6, UI_TRACK);
  gfx->setTextColor(got ? UI_BG_DAY : 0x9492);
  setSize(2);
  setCur(x + (100 - textW(medalLabel(i), 2)) / 2, y + 5);
  printT(medalLabel(i));
}

// pagina 0: profilo (ritratto grande, identità, serie, legame, bacca)
void renderCardProfile() {
  const DexEntry &d = DEX_TBL[pet.speciesId];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  char head[32];
  snprintf(head, sizeof(head), T(S_NAME_FMT), pet.shiny ? "*" : "", nm, pet.level());
  gfx->setTextColor(d.accent);
  // si rimpicciolisce da solo: a dimensione 3 i nomi lunghi non ci stanno nella fascia
  // stretta in alto dello schermo rotondo, quindi venivano tagliati dal bordo. La
  // larghezza si MISURA con textW(): strlen() conta byte, e in una riga UTF-8 non c'è
  // un byte per carattere né tutti misurano uguale. Con il font classico textW()
  // restituisce strlen()*6*size, cioè esattamente il conteggio precedente: non sposta
  // un pixel nelle sei lingue latine (11 caratteri a dimensione 3 sono quei 198 px).
  setSize(3);
  int hts = (textW(head, 3) <= 198) ? 3 : 2;
  setSize(hts);
  setCur(centerX(head, hts), hts == 3 ? 34 : 40);
  printT(head);
  if (pet.nick[0]) {  // specie reale sotto il soprannome
    const char *sp = dexName(pet.speciesId);
    char par[32];
    snprintf(par, sizeof(par), "(%s)", sp);
    gfx->setTextColor(UI_HINT);
    setSize(2);
    setCur(centerX(par, 2), 64);   // misurato, non contato in byte
    printT(par);
  }

  // ritratto grande animato
  if (pmd.loaded) drawPmdAct(PMD_IDLE, CX, 206, millis(), true, false, 4);

  // serie con fiamma
  int sx = 138, sy = 224;
  gfx->fillTriangle(sx + 8, sy, sx + 1, sy + 18, sx + 15, sy + 18, UI_BAR_BAD);
  gfx->fillTriangle(sx + 8, sy + 7, sx + 4, sy + 18, sx + 12, sy + 18, UI_BAR_WARN);
  char rl[40];  // in giapponese perde cifre con 30 da 100 giorni in poi
  snprintf(rl, sizeof(rl), T(S_STREAK_FMT), pet.streak, pet.bestStreak);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(sx + 24, sy + 2);
  printT(rl);

  drawCardStat(258, T(S_VIN), pet.bond, 100, C565(0xd4, 0x52, 0x7e));

  const char *berry = !pet.berryKnown ? T(S_BERRY_UNK)
                      : pet.lovesBerry(0) ? T(S_BERRY_RED)
                      : pet.lovesBerry(1) ? T(S_BERRY_BLUE)
                                          : T(S_BERRY_GREEN);
  char info[48];
  snprintf(info, sizeof(info), T(S_INFO_FMT), berry,
           (unsigned long)(pet.ageMinutes / 1440));
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(info, 2), 296);
  printT(info);

  // allenatore originale di un pet ricevuto con lo scambio; per i propri non si dice
  int ry = 332;
  if (pet.otId) {
    char nome[sizeof(pet.otName)], ot[40];
    size_t i = 0;  // arriva dalla radio: solo ASCII stampabile
    for (; i + 1 < sizeof(nome) && pet.otName[i]; i++)
      nome[i] = pet.otName[i] >= 0x20 && pet.otName[i] < 0x7F ? pet.otName[i] : '?';
    nome[i] = 0;
    snprintf(ot, sizeof(ot), "%s: %s", T(S_COL_OT), nome[0] ? nome : "?");
    gfx->setTextColor(d.accent);
    setCur(centerX(ot, 2), 318);
    printT(ot);
    ry = 342;
  }
  gfx->setTextColor(UI_HINT);
  setCur(centerX(T(S_RENAME_HINT), 2), ry);
  printT(T(S_RENAME_HINT));
}

// pagina 1: combattimento. Dall'alto: tipi, 5 barre (con la Speciale), le 4 mosse che
// userebbe in battaglia con i PP, e i pulsanti ALLENA FORZA, LOTTA e COLLEGA in una riga
// larga 406 px, quanto il cerchio a quell'altezza.
#define CS_BTN_Y 298   // pulsanti (alti 44: area di tocco ≥44 px)
#define CS_BTN_H 44
#define CS_TRAIN_W 156  // "ALLENA FORZA" ancora a dimensione 2
#define CS_BTN_W 121    // LOTTA e COLLEGA
#define CS_TRAIN_X (CX - 203)
#define CS_FIGHT_X (CS_TRAIN_X + CS_TRAIN_W + 4)
#define CS_LINK_X (CS_FIGHT_X + CS_BTN_W + 4)

// pillola colorata del tipo, larga quanto il nome; restituisce la larghezza
int typePillW(uint8_t t) { return textW(typeName(t), 2) + 20; }
void drawTypePill(int x, int y, uint8_t t) {
  int w = typePillW(t);
  gfx->fillRoundRect(x, y, w, 24, 12, TYPE_COLOR[t % TYPE_COUNT]);
  gfx->setTextColor(UI_WHITE);
  setSize(2);
  setCur(x + 10, y + 5);
  printT(typeName(t));
}

// pulsante della pagina: il testo scende a dimensione 1 se a 2 non ci sta
static void drawCardBtn(int x, int w, uint16_t col, uint16_t ink, const char *txt) {
  gfx->fillRoundRect(x, CS_BTN_Y, w, CS_BTN_H, 12, col);
  gfx->setTextColor(ink);
  uint8_t sz = textW(txt, 2) <= w - 12 ? 2 : 1;
  setSize(sz);
  setCur(x + (w - textW(txt, sz)) / 2, CS_BTN_Y + CS_BTN_H / 2 - (sz == 2 ? 7 : 4));
  printT(txt);
}

void renderCardStats() {
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(T(S_BATTLE), 3), 36);
  printT(T(S_BATTLE));

  // tipi, centrati
  const DexEntry &d = DEX_TBL[pet.speciesId];
  bool due = d.type2 < TYPE_COUNT;
  int tw = typePillW(d.type1) + (due ? 8 + typePillW(d.type2) : 0);
  int tx = CX - tw / 2;
  drawTypePill(tx, 68, d.type1);
  if (due) drawTypePill(tx + typePillW(d.type1) + 8, 68, d.type2);

  drawCardStat(102, T(S_STAT_ATK), pet.atkStat(), 260, UI_BAR_BAD);
  drawCardStat(124, T(S_STAT_DEF), pet.defStat(), 260, 0x4C98);
  drawCardStat(146, T(S_STAT_SPE), pet.speStat(), 260, UI_BAR_WARN);
  drawCardStat(168, T(S_STAT_SPC), pet.spcStat(), 260, 0x9A7F);
  drawCardStat(190, T(S_STAT_WGT), pet.weight, 100, 0xB3C8);

  // le 4 mosse: le ultime imparate al livello di battaglia, con i PP pieni
  uint8_t mosse[BATTLE_MOSSE];
  battleMosseApprese(pet.speciesId, pet.battleLevel(), mosse);
  setSize(2);
  for (uint8_t i = 0; i < BATTLE_MOSSE; i++) {
    int y = 214 + i * 20;
    uint8_t mv = mosse[i];
    if (!mv) {
      gfx->fillRoundRect(96, y + 1, 8, 13, 3, UI_TRACK);
      continue;
    }
    gfx->fillRoundRect(96, y + 1, 8, 13, 3, TYPE_COLOR[MOVE_TBL[mv].type % TYPE_COUNT]);
    gfx->setTextColor(UI_INK);
    setCur(112, y);
    printT(moveName(mv));
    char pp[12];
    snprintf(pp, sizeof(pp), "PP %u", MOVE_TBL[mv].pp);
    setCur(370 - textW(pp, 2), y);
    printT(pp);
  }

  drawCardBtn(CS_TRAIN_X, CS_TRAIN_W, UI_BAR_BAD, UI_BG_DAY, T(S_TRAIN_STR));
  bool puo = pet.canBattle();  // grigio: stanco, addormentato, in evoluzione...
  drawCardBtn(CS_FIGHT_X, CS_BTN_W, puo ? C565(0xf4, 0xa8, 0x98) : UI_TRACK, UI_INK, T(S_BTL_FIGHT));
  drawCardBtn(CS_LINK_X, CS_BTN_W, C565(0x9c, 0xc8, 0xe8), UI_INK, T(S_LINK_BTN));
}

// pagina 2: medaglie con etichetta descrittiva
// descrizione di una medaglia nella sua pillola (alta 44), da x: su una riga a scala 2 se ci
// sta, altrimenti su due righe a scala 2 spezzando su uno spazio («SERIE 7 GIORNI» usciva
// dalla pillola), altrimenti su una riga a scala 1
#define MED_PILL_W 196
#define MED_TXT_X 38
#define MED_TXT_W (MED_PILL_W - MED_TXT_X - 8)
static void drawMedalDesc(const char *d, int x, int y) {
  setSize(2);
  if (textW(d, 2) <= MED_TXT_W) {
    setCur(x, y + 14);
    printT(d);
    return;
  }
  char a[32], b[32];
  for (const char *sp = strrchr(d, ' '); sp; ) {
    size_t n = (size_t)(sp - d);
    if (n < sizeof(a) && strlen(sp + 1) < sizeof(b)) {
      memcpy(a, d, n);
      a[n] = 0;
      strcpy(b, sp + 1);
      if (textW(a, 2) <= MED_TXT_W && textW(b, 2) <= MED_TXT_W) {
        setCur(x, y + 5);
        printT(a);
        setCur(x, y + 24);
        printT(b);
        return;
      }
    }
    // uno spazio prima: la prima riga più corta
    const char *q = sp;
    sp = nullptr;
    while (q > d && !sp)
      if (*--q == ' ') sp = q;
  }
  setSize(1);
  setCur(x, y + 18);
  printT(d);
}

void renderCardMedals() {
  int got = 0;
  for (int i = 0; i < MED_COUNT; i++)
    if (pet.hasMedal(1 << i)) got++;
  char head[24];
  snprintf(head, sizeof(head), T(S_MEDALS_FMT), got, MED_COUNT);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(head, 3), 48);
  printT(head);

  for (int i = 0; i < MED_COUNT; i++) {
    int x = 28 + (i % 2) * 206, y = 104 + (i / 2) * 54;
    bool g = pet.hasMedal(1 << i);
    gfx->fillRoundRect(x, y, MED_PILL_W, 44, 10, g ? UI_BAR_OK : UI_TRACK);
    if (g) {  // segno di ottenuta
      gfx->fillCircle(x + 20, y + 22, 10, UI_BG_DAY);
      gfx->setTextColor(UI_BAR_OK);
      setSize(2);
      setCur(x + 14, y + 13);
      printT("v");
    }
    gfx->setTextColor(g ? UI_BG_DAY : 0x8410);
    drawMedalDesc(medalDesc(i), x + MED_TXT_X, y);
  }
}

// pagina 3: progressi (livello, evoluzione, trascuratezze) — porta alla luce meccaniche
// prima invisibili (quanto manca per salire/evolvere e perché)
void renderCardProgress() {
  const DexEntry &d = DEX_TBL[pet.speciesId];
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(T(S_PROGRESS), 3), 44);
  printT(T(S_PROGRESS));

  // livello grande
  char lv[10];
  snprintf(lv, sizeof(lv), T(S_LVL_FMT), pet.level());
  setSize(5);
  setCur(centerX(lv, 5), 86);
  printT(lv);

  // barra di progresso verso il livello successivo (1 livello = 60 min di gioco)
  uint8_t into = pet.ageMinutes % MINUTES_PER_LEVEL;
  int bx = 93, bw = 280, by = 158, bh = 22;
  gfx->fillRoundRect(bx, by, bw, bh, 6, UI_TRACK);
  int fw = (bw - 4) * into / MINUTES_PER_LEVEL;
  if (fw > 0) gfx->fillRoundRect(bx + 2, by + 2, fw, bh - 4, 5, UI_BAR_OK);
  char nx[32];
  snprintf(nx, sizeof(nx), T(S_NEXT_LVL_FMT), MINUTES_PER_LEVEL - into, pet.level() + 1);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(nx, 2), by + 32);
  printT(nx);

  // stato dell'evoluzione
  gfx->setTextColor(UI_HINT);
  setCur(centerX(T(S_EVO_LABEL), 2), 230);
  printT(T(S_EVO_LABEL));
  char evoBuf[32];
  const char *evo;
  uint16_t evoCol = UI_INK;
  if (d.evolvesTo == 0) {
    evo = T(S_FINAL_FORM);
  } else {
    int needed = d.evolveLevel + pet.careMistakes;
    if (pet.level() >= needed) {
      if (pet.lowestStat() >= 40) { evo = T(S_EVO_READY); evoCol = UI_BAR_OK; }
      else { evo = T(S_EVO_BLOCKED); evoCol = UI_BAR_BAD; }
    } else {
      snprintf(evoBuf, sizeof(evoBuf), T(S_EVO_IN_FMT), needed - pet.level());
      evo = evoBuf;
    }
  }
  gfx->setTextColor(evoCol);
  setCur(centerX(evo, 2), 256);
  printT(evo);

  // trascuratezze (ritardano l'evoluzione)
  char ms[24];
  snprintf(ms, sizeof(ms), T(S_MISTAKES_FMT), pet.careMistakes);
  gfx->setTextColor(pet.careMistakes > 0 ? UI_BAR_BAD : UI_INK);
  setCur(centerX(ms, 2), 312);
  printT(ms);

  // scambi effettuati, del giocatore come la serie
  char sc[32];
  snprintf(sc, sizeof(sc), T(S_LKS_TRADES), pet.linkTrades());
  gfx->setTextColor(UI_HINT);
  setCur(centerX(sc, 2), 338);
  printT(sc);
}

void renderCard() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  if (cardPage == 0) renderCardProfile();
  else if (cardPage == 1) renderCardStats();
  else if (cardPage == 2) renderCardMedals();
  else renderCardProgress();

  // indicatore delle 4 pagine + aiuto
  for (int i = 0; i < 4; i++) {
    if (i == cardPage) gfx->fillCircle(194 + i * 26, 374, 5, UI_INK);
    else gfx->drawCircle(194 + i * 26, 374, 4, UI_INK);
  }
  gfx->setTextColor(UI_HINT);
  setSize(2);
  setCur(centerX(T(S_BACK), 2), 398);
  printT(T(S_BACK));
  halFlush();
}

// onEnter di SCR_CARD: si apre sul profilo (anche tornando dalla tastiera, che si
// apre da lì)
void cardEnter() { cardPage = 0; }

// SCR_CARD: toccare il nome rinomina, ALLENA FORZA apre il sacco, LOTTA avvia una
// battaglia contro un selvatico, COLLEGA apre il collegamento, il resto chiude
void cardTap(int16_t x, int16_t y) {
  bool riga = cardPage == 1 && y >= CS_BTN_Y && y < CS_BTN_Y + CS_BTN_H;
  if (cardPage == 0 && y < 84) goScreen(SCR_KEYBOARD);  // toccare il nome = rinominare
  else if (riga && x >= CS_TRAIN_X && x < CS_TRAIN_X + CS_TRAIN_W) {
    goScreen(SCR_MAIN);          // pulsante ALLENA FORZA
    startSack();
  } else if (riga && x >= CS_FIGHT_X && x < CS_FIGHT_X + CS_BTN_W) {
    if (!startCpuBattle()) halSfx(SFX_DENY);  // non può lottare: la scheda resta aperta
  } else if (riga && x >= CS_LINK_X && x < CS_LINK_X + CS_BTN_W) {
    openLink();
  } else {
    goScreen(SCR_MAIN);
  }
}

// SCR_CARD, scorrimento orizzontale: cambiare tra le 4 pagine
void cardSwipe(int dir) {
  int p = (int)cardPage + (dir > 0 ? -1 : 1);  // sinistra avanza
  cardPage = p < 0 ? 0 : (p > 3 ? 3 : p);
}

// SCR_CARD, scorrimento verticale: in alto chiude la scheda
void cardSwipeV(int dir) {
  if (dir < 0) goScreen(SCR_MAIN);
}

// ---------- tastiera per rinominare (e per il nome dell'allenatore) ----------

static const char KB_KEYS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-";  // 28 + DEL + OK = 30
#define KB_COLS 6
#define KB_X 40
#define KB_Y 150
#define KB_W 64
#define KB_H 52

// la tastiera chiede il nome dell'allenatore invece del soprannome (keyboardTrainer)
static bool kbTrainer = false;

// SCR_KEYBOARD per il nome dell'allenatore: con OK lo salva e apre SCR_LINK
void keyboardTrainer() {
  kbTrainer = true;
  goScreen(SCR_KEYBOARD);
}

// onEnter di SCR_KEYBOARD: si apre dalla scheda e con OK si torna lì (o al collegamento,
// per il nome dell'allenatore)
void keyboardEnter() {
  if (kbTrainer) pet.trainerName(nameBuf, sizeof(nameBuf));
  else strncpy(nameBuf, pet.nick, sizeof(nameBuf) - 1);
  nameBuf[sizeof(nameBuf) - 1] = 0;
  nameLen = strlen(nameBuf);
}

void renderKeyboard() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  setSize(2);
  const char *titolo = T(kbTrainer ? S_LINK_TRAINER : S_NAME);
  setCur(centerX(titolo, 2), 56);
  printT(titolo);
  // buffer attuale
  gfx->fillRoundRect(83, 84, 300, 40, 8, UI_WHITE);
  gfx->drawRoundRect(83, 84, 300, 40, 8, UI_INK);
  setSize(3);
  setCur(95, 94);
  printT(nameLen ? nameBuf : "_");

  for (int i = 0; i < 30; i++) {
    int x = KB_X + (i % KB_COLS) * KB_W, y = KB_Y + (i / KB_COLS) * KB_H;
    bool special = (i >= 28);
    gfx->fillRoundRect(x, y, KB_W - 6, KB_H - 6, 6, special ? UI_BAR_WARN : UI_WHITE);
    gfx->drawRoundRect(x, y, KB_W - 6, KB_H - 6, 6, UI_INK);
    gfx->setTextColor(UI_INK);
    setSize(2);
    if (i < 28) {
      setCur(x + KB_W / 2 - 9, y + KB_H / 2 - 10);
      printT(KB_KEYS[i]);
    } else {
      const char *lab = (i == 28) ? "<-" : "OK";
      setCur(x + KB_W / 2 - 15, y + KB_H / 2 - 10);
      printT(lab);
    }
  }
  halFlush();
}

void keyboardTap(int16_t x, int16_t y) {
  // filtra PRIMA di dividere: con x/y minori di KB_X/KB_Y la sottrazione dà negativo e
  // la divisione intera tronca verso zero (non verso -inf), quindi col/row verrebbe 0
  // invece di negativo e il filtro sotto non lo rileverebbe
  if (x < KB_X || y < KB_Y) return;
  int col = (x - KB_X) / KB_W, row = (y - KB_Y) / KB_H;
  if (col >= KB_COLS || row >= 5) return;
  int i = row * KB_COLS + col;
  if (i >= 30) return;
  if (i == 28) {  // cancellare
    if (nameLen) nameBuf[--nameLen] = 0;
  } else if (i == 29 && kbTrainer) {  // OK: l'allenatore ha bisogno di un nome
    if (!nameLen) {
      halSfx(SFX_DENY);
      return;
    }
    pet.setTrainerName(nameBuf);
    kbTrainer = false;
    goScreen(SCR_LINK);
  } else if (i == 29) {  // OK
    pet.rename(nameBuf);
    goScreen(SCR_CARD);
  } else if (nameLen < sizeof(nameBuf) - 1) {
    nameBuf[nameLen++] = KB_KEYS[i];
    nameBuf[nameLen] = 0;
  }
}
