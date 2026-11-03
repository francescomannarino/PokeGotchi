// Schermata principale: scelta dell'iniziale, scena, Pokémon, cerimonie e dialoghi,
// render() e gestione dei gesti.

#include "ui_common.h"

PmdMon evoPmd;      // forma precedente, solo durante il lampeggio dell'evoluzione
int16_t monFor = -2;
bool monShinyFor = false;

// comportamento del Pokémon a schermo
struct {
  uint8_t mode = 0;     // 0 idle, 1 passeggiata, 2 gesto one-shot
  uint8_t act = PMD_IDLE;
  uint32_t t0 = 0;      // inizio dell'animazione in corso
  uint32_t until = 0;   // fine dello stato attuale
  float x = 233, targetX = 233;
} beh;
#define PET_GROUND 304  // linea del suolo del Pokémon

// scena del bagnetto: schiuma sul Pokémon e pulizia allo scoppio
uint32_t bathUntil = 0;
bool bathPending = false;
struct { int16_t x, y; uint8_t r, ph; } bubbles[14];
#define PET_CY 202  // centro verticale dello sprite

// pulsanti icona lungo l'arco inferiore dello schermo rotondo
// (quelli esterni stanno più in alto per non uscire dal cerchio)
struct Btn {
  int16_t cx, cy;
  const char *const *icon;
};
Btn buttons[4] = {
  { 140, 390, SPR_ICON_FOOD },   // mangiare
  { 202, 404, SPR_ICON_PLAY },   // giocare
  { 264, 404, SPR_ICON_LIGHT },  // luce
  { 326, 390, SPR_ICON_CLEAN },  // bagnetto
};
#define BTN_HALF 26  // pulsante da 52x52
#define BTN_HIT 36   // raggio touch (un po' più generoso)

// crepe dell'uovo (pixel 'k' sopra lo sprite)
static const uint8_t CRACK1[][2] = { {15,8},{16,9},{15,10} };
static const uint8_t CRACK2[][2] = { {11,13},{12,14},{11,15},{20,12},{19,13},{20,14} };

bool wasPressed = false;
bool gDitoGiu = false;   // il dito tenuto e la sua ultima posizione (joystick dei giochi)
uint32_t gUltimoTocco = 0;  // ultimo campione del touch col dito giù o appena alzato
int16_t gDitoX = CX, gDitoY = CY;
// scelta dell'iniziale (prima partita): Bulbasaur / Charmander / Squirtle, 3 righe
static const int16_t STARTER_DEX[3] = { 1, 4, 7 };
#define STARTER_ROW_Y 110
#define STARTER_ROW_H 70
#define STARTER_ROW_GAP 8
// pulsante-CTA di evoluzione (centrato, metà schermo)
#define EVO_BTN_W 256
#define EVO_BTN_H 64
#define EVO_BTN_X (CX - EVO_BTN_W / 2)
#define EVO_BTN_Y 172
// pulsante-CTA di addio (più largo: contiene il nome + frase)
#define FAR_BTN_W 408
#define FAR_BTN_H 58
#define FAR_BTN_X (CX - FAR_BTN_W / 2)
#define FAR_BTN_Y 176
// scelta dei giochi (OVL_GIOCHI), sul modello del menu del cibo: una casella per gioco,
// Palla, Grotta e Surf
#define GIOCHI_N 3
#define GIOCHI_SLOT 80
#define GIOCHI_W (GIOCHI_N * GIOCHI_SLOT)
#define GIOCHI_X (CX - GIOCHI_W / 2)
#define GIOCHI_Y 284
#define GIOCHI_H 72
bool swallowGesture = false; // il tocco che sveglia non aziona nulla
int16_t tX0, tY0, tXl, tYl; // gesto in corso (inizio e ultima posizione)
uint32_t tStart = 0;
bool holdFired = false;

// carica/scarica lo sprite della SD quando cambia la specie
void ensureMon() {
  if (pet.speciesId == monFor && monShinyFor == pet.shiny && !sdDirty) return;
  // i file appena ricevuti possono includere un nuovo thumbs.bin, che si caricava solo
  // in setup(): senza questo non si vedeva nella galleria fino al riavvio
  if (sdDirty) { thumbs.unload(); thumbs.load(); }
  sdDirty = false;
  monFor = pet.speciesId;
  monShinyFor = pet.shiny;
  mon.unload();
  pmd.unload();
  beh.x = beh.targetX = 233;
  beh.mode = 0;
  beh.until = 0;
  if (pet.speciesId >= 1 && pet.speciesId <= DEX_COUNT) {
    pmd.load(pet.speciesId, pet.shiny);          // principale: PMD
    if (!pmd.loaded) mon.load(pet.speciesId, pet.shiny);  // riserva: B/N
  }
}

// ---------- input touch ----------

// apre un overlay della principale, che si chiude da solo dopo ms
static void openOverlay(Overlay o, uint32_t ms) {
  gOverlay = o;
  gOverlayUntil = millis() + ms;
}

bool inPetZone(int16_t x, int16_t y) {
  return x > 110 && x < 356 && y > 95 && y < 310;
}

// il tocco si risolve al SOLLEVARE il dito per distinguere tap da scorrimento
void handleTouch() {
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll < 20) return;  // 50 Hz bastano e avanzano per un dito
  lastPoll = millis();
  int16_t x, y;
  int8_t t = halTouchRead(&x, &y, wasPressed);
  if (t == HAL_TOUCH_NIENTE) return;  // si riprova tra 20 ms
  bool pressed = t == HAL_TOUCH_GIU;
  gDitoGiu = pressed;
  if (pressed || wasPressed) gUltimoTocco = millis();  // anche l'alzata: scuote il vetro
  if (pressed) {
    gDitoX = x;
    gDitoY = y;
  }

  // sacco da allenamento: ogni tocco conta subito (martellare veloce)
  if (gScreen == SCR_SACK && !saveDlg) {
    if (pressed && !wasPressed) {
      lastInteract = millis();
      SCREENS[SCR_SACK].onTap(x, y);
    }
    wasPressed = pressed;
    return;
  }

  if (pressed && !wasPressed) {  // inizia il gesto
    tX0 = tXl = x;
    tY0 = tYl = y;
    tStart = millis();
    holdFired = false;
    swallowGesture = (dimStage > 0) || screenOff;  // se era buio, si limita a svegliare
    screenOff = false;
    lastInteract = millis();
  } else if (pressed) {  // resta appoggiato
    tXl = x;
    tYl = y;
    // pressione lunga senza muoversi sul Pokémon -> dialogo di rilascio
    if (!holdFired && !swallowGesture && !saveDlg && gScreen == SCR_MAIN && millis() - tStart > 3000 &&
        abs(tXl - tX0) < 30 && abs(tYl - tY0) < 30 && inPetZone(tX0, tY0) &&
        !pet.isEgg() && !pet.evolving() && gOverlay == OVL_NONE) {
      openOverlay(OVL_CONFIRM, 10000);
      holdFired = true;
    }
  } else if (wasPressed) {  // solleva il dito: risolvere il gesto
    lastInteract = millis();
    int dx = tXl - tX0, dy = tYl - tY0;
    uint32_t dt = millis() - tStart;
    if (!holdFired && !swallowGesture) {
      if (abs(dx) > 80 && abs(dy) < 70 && dt < 800) onSwipe(dx > 0 ? 1 : -1);
      else if (abs(dy) > 80 && abs(dx) < 70 && dt < 800) onSwipeV(dy > 0 ? 1 : -1);
      else if (dt < 1500 && abs(dx) < 40 && abs(dy) < 40) onTap(tX0, tY0);
    }
  }
  wasPressed = pressed;
}

// Gesti: prima il dialogo dei salvataggi (modale, fuori da gScreen), poi i gestori
// della schermata attiva nella tabella SCREENS (nullptr = gesto ignorato).

void onSwipeV(int dir) {  // +1 = verso il basso
  if (gLogTocchi) Serial.printf("GESTO scorri-v %d scr=%d\n", dir, (int)gScreen);
  if (saveDlg) return;
  if (SCREENS[gScreen].onSwipeV) SCREENS[gScreen].onSwipeV(dir);
}

void onSwipe(int dir) {  // +1 = verso destra
  if (gLogTocchi) Serial.printf("GESTO scorri-o %d scr=%d\n", dir, (int)gScreen);
  if (saveDlg) return;
  if (SCREENS[gScreen].onSwipe) SCREENS[gScreen].onSwipe(dir);
}

void onTap(int16_t x, int16_t y) {
  if (gLogTocchi) Serial.printf("GESTO tocco %d %d scr=%d\n", x, y, (int)gScreen);
  if (saveDlg) {  // ripristino o importazione: precede anche la scelta dell'iniziale
    saveDlgTap(x, y);
    return;
  }
  // il sacco non arriva qui: handleTouch lo serve alla pressione
  if (SCREENS[gScreen].onTap) SCREENS[gScreen].onTap(x, y);
}

// SCR_STARTER, prima partita: scegliere l'iniziale
void starterTap(int16_t x, int16_t y) {
  for (int i = 0; i < 3; i++) {
    int ry = STARTER_ROW_Y + i * (STARTER_ROW_H + STARTER_ROW_GAP);
    if (x >= 70 && x <= 396 && y >= ry && y <= ry + STARTER_ROW_H) {
      pet.chooseStarter(STARTER_DEX[i]);
      halSfx(SFX_TAP);
      goScreen(SCR_MAIN);
      break;
    }
  }
}

// SCR_MAIN, scorrimento verticale: in basso le impostazioni, in alto la scheda. Durante
// l'evoluzione i gesti sono ignorati, come in cerimonia: l'animazione si guarda fino in fondo
void mainSwipeV(int dir) {
  if (pet.evolving()) return;
  if (gOverlay == OVL_FEED || gOverlay == OVL_CONFIRM || gOverlay == OVL_GIOCHI) return;
  if (dir > 0) goScreen(SCR_CLOCK);
  else if (!pet.isEgg()) goScreen(SCR_CARD);
}

// SCR_MAIN, scorrimento orizzontale: apre la galleria
void mainSwipe(int) {
  if (pet.evolving()) return;
  if (gOverlay != OVL_CONFIRM) goScreen(SCR_GALLERY);
}

// onExit di SCR_MAIN: gli overlay non sopravvivono al cambio di schermata
void mainExit() { gOverlay = OVL_NONE; }

void mainTap(int16_t x, int16_t y) {
  if (pet.evolving()) return;
  if (gOverlay != OVL_NONE) {  // un overlay aperto consuma il tocco e si chiude
    Overlay o = gOverlay;
    gOverlay = OVL_NONE;
    if (!timeLeft(gOverlayUntil)) return;  // già scaduto: il tocco lo chiude e basta
    if (o == OVL_EVOLVE || o == OVL_FAREWELL || o == OVL_RUNAWAY) {  // azione (sopra) / mantieni (sotto)
      bool b1 = (x >= 93 && x <= 373 && y >= 206 && y <= 258);  // azione
      bool b2 = (x >= 93 && x <= 373 && y >= 268 && y <= 320);  // mantieni / restare insieme
      if (o == OVL_EVOLVE) {
        if (b1) { int16_t old = pet.speciesId; pet.evolve(); evoPmd.load(old, pet.shiny); }
        else if (b2) pet.declineEvolve();
      } else if (o == OVL_FAREWELL) {
        if (b1) pet.startFarewell();
        else if (b2) pet.declineFarewell();
      } else if (b1 && pet.canRunawayNow()) {  // fuga; «me ne prendo cura» chiude e basta
        pet.startRunaway();
      }
    } else if (o == OVL_CONFIRM) {  // dialogo "rilasciare?": SÌ / NO
      if (x >= 118 && x <= 218 && y >= 252 && y <= 304) pet.release();
    } else if (o == OVL_GIOCHI) {  // scelta dei giochi
      if (y >= GIOCHI_Y && y <= GIOCHI_Y + GIOCHI_H && x >= GIOCHI_X && x < GIOCHI_X + GIOCHI_W) {
        int g = (x - GIOCHI_X) / GIOCHI_SLOT;
        halSfx(SFX_TAP);
        if (g == 0) startGame();
        else if (g == 1) startGrotta();
        else startSurf();
      }
    } else if (y >= 288 && y <= 352 && x >= 101 && x <= 365) {  // selettore del cibo
      int item = (x - 101) / 66;
      if (item == 3) pet.feedCandy();
      else pet.feedBerry(item);
      halSfx(SFX_EAT);
    }
    return;
  }
  if (pet.isEgg()) {
    pet.eggTap();
    halSfx(SFX_TAP);
    return;
  }
  // pulsante di evoluzione: apre il dialogo evolvere/mantenere
  if (pet.wantEvolveButton() && x >= EVO_BTN_X && x <= EVO_BTN_X + EVO_BTN_W &&
      y >= EVO_BTN_Y && y <= EVO_BTN_Y + EVO_BTN_H) {
    openOverlay(OVL_EVOLVE, 12000);
    return;
  }
  // pulsanti di finale (stesso riquadro): fuga e addio aprono il loro dialogo (la fuga
  // partiva con un tocco solo)
  if (x >= FAR_BTN_X && x <= FAR_BTN_X + FAR_BTN_W &&
      y >= FAR_BTN_Y && y <= FAR_BTN_Y + FAR_BTN_H) {
    if (pet.canRunawayNow()) { openOverlay(OVL_RUNAWAY, 12000); return; }
    if (pet.wantFarewellButton()) { openOverlay(OVL_FAREWELL, 12000); return; }
  }
  for (int i = 0; i < 4; i++) {
    int dx = x - buttons[i].cx, dy = y - buttons[i].cy;
    if (dx * dx + dy * dy <= BTN_HIT * BTN_HIT) {
      Serial.printf("BTN %d\n", i);
      halSfx(SFX_TAP);
      if (i == 0) {
        if (!pet.sleeping) openOverlay(OVL_FEED, 6000);
      } else if (i == 1) {
        if (!pet.sleeping) openOverlay(OVL_GIOCHI, 6000);
      } else if (i == 2) {
        pet.toggleLight();
      } else {
        startBath();
      }
      return;
    }
  }
  // toccare il Pokémon = carezza
  if (inPetZone(x, y)) {
    Serial.println("PET");
    pet.caress();
    if (!pet.sleeping) halSfx(SFX_HEART);
  }
}
#define HORIZON 232  // linea dove il cielo incontra il suolo

void drawClouds(uint32_t now, uint16_t col) {
  for (int k = 0; k < 2; k++) {
    int cx = (int)((now / 50 + k * 250) % 560) - 40;
    int cy = 70 + k * 34;
    gfx->fillCircle(cx, cy, 16, col);
    gfx->fillCircle(cx + 18, cy + 3, 13, col);
    gfx->fillCircle(cx - 15, cy + 4, 12, col);
  }
}

void drawScene(uint8_t biome, uint32_t now, bool night) {
  int h = sceneHour();
  uint16_t top, bot;
  if (night)            { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  else if (h < 8)       { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }  // alba
  else if (h < 18)      { top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }  // giorno
  else                  { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }  // tramonto

  // cielo a bande
  for (int y = 0; y < HORIZON; y += 8)
    gfx->fillRect(0, y, 466, 8, lerp565(top, bot, y, HORIZON));

  // sole o luna
  if (night) {
    gfx->fillCircle(360, 78, 24, C565(0xe8, 0xee, 0xf5));
    gfx->fillCircle(370, 72, 22, lerp565(top, bot, 78, HORIZON));  // crescente
    for (auto &st : STARS) gfx->fillRect(st[0], st[1], 4, 4, UI_WHITE);
  } else if (h < 18) {
    gfx->fillCircle(360, 84, 26, h < 8 ? C565(0xff, 0xd9, 0x8a) : C565(0xff, 0xe7, 0x9f));
    drawClouds(now, C565(0xff, 0xff, 0xff));
  } else {
    gfx->fillCircle(233, HORIZON - 6, 34, C565(0xff, 0xf1, 0xc8));  // sole al tramonto
  }

  // mare della spiaggia: una fascia d'acqua sopra la sabbia
  uint16_t soil = BIOME_SOIL[biome < 6 ? biome : 0];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);
  if (biome == 1) {
    uint16_t sea = night ? C565(0x1c, 0x34, 0x52) : C565(0x4f, 0x96, 0xc4);
    gfx->fillRect(0, HORIZON - 26, 466, 26, sea);
    for (int i = 0; i < 3; i++) {
      int wy = HORIZON - 22 + i * 7;
      uint16_t fc = night ? C565(0x3a, 0x58, 0x78) : C565(0xbf, 0xe6, 0xf5);
      gfx->fillRect(60 + ((now / 60 + i * 30) % 60), wy, 26, 2, fc);
      gfx->fillRect(300 - ((now / 60 + i * 20) % 60), wy, 26, 2, fc);
    }
  }

  // suolo
  gfx->fillRect(0, HORIZON, 466, 466 - HORIZON, soil);
  uint16_t hill = lerp565(soil, night ? C565(0x0c, 0x12, 0x24) : C565(0xff, 0xff, 0xff), 3, 16);
  gfx->fillRoundRect(-60, HORIZON - 14, 586, 60, 30, hill);

  // dettagli del bioma
  uint16_t dk = lerp565(soil, C565(0x10, 0x18, 0x20), night ? 11 : 7, 16);
  if (biome == 2) {  // bosco: conifere in silhouette
    for (int tx : { 60, 150, 360, 416 }) {
      gfx->fillTriangle(tx, HORIZON - 46, tx - 16, HORIZON, tx + 16, HORIZON, dk);
      gfx->fillTriangle(tx, HORIZON - 60, tx - 12, HORIZON - 28, tx + 12, HORIZON - 28, dk);
    }
  } else if (biome == 3) {  // vulcano: rocce e braci
    gfx->fillTriangle(70, HORIZON, 40, HORIZON + 30, 100, HORIZON + 30, dk);
    gfx->fillTriangle(400, HORIZON + 4, 372, HORIZON + 30, 430, HORIZON + 30, dk);
    if (!night)
      for (int e = 0; e < 4; e++)
        gfx->fillRect(120 + e * 70, HORIZON + 8 + (e % 2) * 6, 4, 4, C565(0xff, 0x9b, 0x3a));
  } else if (biome == 4) {  // montagna: cime sullo sfondo
    gfx->fillTriangle(140, HORIZON - 50, 60, HORIZON, 220, HORIZON, dk);
    gfx->fillTriangle(330, HORIZON - 38, 250, HORIZON, 410, HORIZON, dk);
  } else if (biome == 5 && !night) {  // neve: fiocchi che cadono
    for (int f = 0; f < 10; f++) {
      int fx = (f * 53 + now / 40) % 466;
      int fy = (f * 90 + now / 18) % HORIZON;
      gfx->fillRect(fx, fy, 3, 3, UI_WHITE);
    }
  } else if (biome == 0) {  // prateria: ciuffi d'erba
    for (int gx : { 80, 175, 300, 395 })
      for (int b = -1; b <= 1; b++)
        gfx->fillRect(gx + b * 5, HORIZON + 6, 2, 8 + (b == 0 ? 4 : 0), dk);
  }
}

// prima partita: sceglie l'iniziale tra Bulbasaur / Charmander / Squirtle
void renderStarterSelect() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  const char *t = T(S_CHOOSE_STARTER);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(t, 2), 68);
  printT(t);
  for (int i = 0; i < 3; i++) {
    int16_t d = STARTER_DEX[i];
    const DexEntry &de = DEX_TBL[d];
    int ry = STARTER_ROW_Y + i * (STARTER_ROW_H + STARTER_ROW_GAP);
    gfx->fillRoundRect(70, ry, 326, STARTER_ROW_H, 14, lerp565(de.accent, UI_WHITE, 6, 8));
    gfx->drawRoundRect(70, ry, 326, STARTER_ROW_H, 14, de.accent);
    const uint8_t *th = thumbs.get(d);     // miniatura dell'iniziale (se la SD è pronta)
    if (th) drawThumb(th, 76, ry - 5, 3, false);
    gfx->setTextColor(UI_INK);
    setSize(3);
    setCur(178, ry + 24);
    printT(dexName(d));
  }
  halFlush();
}

void render() {
  syncScreen();
  if (saveDlg) {  // copre tutto, anche la scelta dell'iniziale
    renderSaveDialog();
    return;
  }
  SCREENS[gScreen].render();
}

// sfondo comune di principale e cerimonia; fissa gNight
static void drawMainScene() {
  int h = sceneHour();
  gNight = pet.sleeping || h < 6 || h >= 20;
  // drawScene copre interamente i 466x466: nessun fillScreen(NERO) prima, perché un
  // flush DMA sovrapposto non catturi mai nero a metà (anti-sfarfallio)
  drawScene(pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome, millis(), gNight);
}

// SCR_CEREMONY: addio, fuga o rilascio sulla scena della principale; a cerimonia finita,
// dopo un addio o il rilascio di una forma finale, la domanda della collezione (i tocchi
// vanno a colDomandaTap)
void renderCeremony() {
  drawMainScene();
  const DexEntry &d = DEX_TBL[pet.speciesId];
  const char *msg = (pet.ceremony == CER_FAREWELL) ? T(S_FAREWELL)
                    : (pet.ceremony == CER_RUNAWAY) ? T(S_RUNAWAY)
                                                    : T(S_GOODBYE);
  drawHeader(dexName(pet.speciesId), d.accent, msg);
  drawCeremony();
  renderColDomanda();
  halFlush();
}

void renderMain() {
  // evoluzione per scambio del pet attivo: la cerimonia esistente, al ritorno a casa
  if (pet.evoScambioDa() && pet.evoScambioPosto() == SCAMBIO_ATTIVO) {
    int16_t old = pet.evoScambioDa();
    pet.avviaEvoScambio();
    evoPmd.load(old, pet.shiny);
  }
  drawMainScene();

  if (pet.isEgg()) {
    drawHeader(T(S_EGG_HDR), inkColor(), eggMsg());
    int s = 5, x = CX - 16 * s + uovoDondolo(), y = PET_CY - 16 * s;
    drawMap(SPR_EGG, SPRITE_H, x, y, s, false);
    if (pet.eggCracks() >= 1)
      for (auto &c : CRACK1) gfx->fillRect(x + c[0] * s, y + c[1] * s, s, s, INK_K);
    if (pet.eggCracks() >= 2)
      for (auto &c : CRACK2) gfx->fillRect(x + c[0] * s, y + c[1] * s, s, s, INK_K);
    if (pet.eggRarity() >= R_RARO) {
      const char *rar = (pet.eggRarity() == R_LEGENDARIO) ? T(S_EGG_LEGEND) : T(S_EGG_RARE);
      gfx->setTextColor(pet.eggRarity() == R_LEGENDARIO ? UI_BAR_WARN : 0x4C98);
      setSize(2);
      setCur(centerX(rar, 2), 316);
      printT(rar);
    }
    char reg[24];
    snprintf(reg, sizeof(reg), T(S_POKEDEX_FMT), pet.registeredCount());
    gfx->fillRect(0, 312, 466, 154, gNight ? UI_BG_NIGHT : UI_BG_DAY);
    gfx->setTextColor(inkColor());
    setSize(2);
    setCur(centerX(reg, 2), 348);
    printT(reg);
  } else {
    const DexEntry &d = DEX_TBL[pet.speciesId];
    char name[28];
    const char *base = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
    snprintf(name, sizeof(name), T(S_NAME_FMT), pet.shiny ? "*" : "", base, pet.level());
    drawHeader(name, gNight ? UI_INK_NIGHT : d.accent, statusMsg());
    drawStreakBadge();
    drawPet();
    drawReazione();
    drawBath();
    drawPoops();
    // pannello inferiore: base pulita per barre e pulsanti sopra il paesaggio
    gfx->fillRect(0, 312, 466, 154, gNight ? UI_BG_NIGHT : UI_BG_DAY);
    drawBars();
    drawButtons();
    drawCelebration();
    if (pet.wantEvolveButton()) drawEvolveButton();        // CTA rosso: evolvere
    else if (pet.canRunawayNow()) drawRunawayButton();     // CTA cupo: fuga (abbandono)
    else if (pet.wantFarewellButton()) drawFarewellButton();  // CTA dorato: addio
  }

  if (pet.sleeping) {
    gfx->setTextColor(UI_INK_NIGHT);
    setSize(3);
    setCur(320, 130);
    printT("Zz");
  }

  // overlay (uno alla volta): si chiudono da soli alla scadenza
  if (gOverlay != OVL_NONE && !timeLeft(gOverlayUntil)) gOverlay = OVL_NONE;
  if (gOverlay == OVL_FEED) {  // selettore del cibo
    gfx->fillRoundRect(101, 288, 264, 64, 14, UI_WHITE);
    gfx->drawRoundRect(101, 288, 264, 64, 14, inkColor());
    drawMap(SPR_ICON_FOOD, 16, 110, 296, 3, false);
    drawMap(SPR_ICON_BERRY_B, 16, 176, 296, 3, false);
    drawMap(SPR_ICON_BERRY_G, 16, 242, 296, 3, false);
    drawMap(SPR_ICON_CANDY, 16, 308, 296, 3, false);
  } else if (gOverlay == OVL_CONFIRM) {  // "rilasciare?" (pressione lunga sul Pokémon)
    gfx->fillRoundRect(94, 168, 278, 152, 16, UI_WHITE);
    gfx->drawRoundRect(94, 168, 278, 152, 16, UI_INK);
    // 48 e non 28: il buffer era dimensionato quando ogni stringa era di un byte per
    // carattere. Con una riga UTF-8, "%s 놓아줄까요?" più il nome supera 28 e snprintf
    // taglia a metà di una sequenza da 3 byte, che il font non sa più disegnare.
    char q[48];
    snprintf(q, sizeof(q), T(S_RELEASE_FMT), dexName(pet.speciesId));
    gfx->setTextColor(UI_INK);
    setSize(2);
    setCur(centerX(q, 2), 196);
    printT(q);
    gfx->fillRoundRect(118, 252, 100, 52, 12, UI_BAR_OK);
    gfx->setTextColor(UI_WHITE);
    setCur(118 + (100 - textW(T(S_YES), 2)) / 2, 270);
    printT(T(S_YES));
    gfx->fillRoundRect(248, 252, 100, 52, 12, UI_BAR_BAD);
    setCur(248 + (100 - textW(T(S_NO), 2)) / 2, 270);
    printT(T(S_NO));
  } else if (gOverlay == OVL_GIOCHI) {  // scelta dei giochi: Palla, Grotta, Surf
    gfx->fillRoundRect(GIOCHI_X, GIOCHI_Y, GIOCHI_W, GIOCHI_H, 14, UI_WHITE);
    gfx->drawRoundRect(GIOCHI_X, GIOCHI_Y, GIOCHI_W, GIOCHI_H, 14, inkColor());
    int cy = GIOCHI_Y + GIOCHI_H / 2, cx = GIOCHI_X + GIOCHI_SLOT / 2;
    drawMap(SPR_ICON_PLAY, 16, cx - 24, cy - 24, 3, false);
    cx += GIOCHI_SLOT;
    if (const uint8_t *th = thumbs.get(50)) drawThumb(th, cx - GAL_CELL / 2, cy - GAL_CELL / 2, 2, false);
    else grottaDisegnaDiglett(cx, cy + 14, 2);
    cx += GIOCHI_SLOT;
    if (const uint8_t *th = thumbs.get(25)) drawThumb(th, cx - GAL_CELL / 2, cy - GAL_CELL / 2, 2, false);
    else surfDisegnaIcona(cx, cy);
  } else if (gOverlay != OVL_NONE) {  // decisione: evolvere/mantenere, addio/restare
    drawChoiceDialog();
  }

  halFlush();
}

// fiamma + numero della serie in alto a sinistra
void drawStreakBadge() {
  if (pet.streak < 1) return;
  int x = 26, y = 16;
  gfx->fillTriangle(x + 8, y, x + 1, y + 17, x + 15, y + 17, UI_BAR_BAD);
  gfx->fillTriangle(x + 8, y + 7, x + 4, y + 17, x + 12, y + 17, UI_BAR_WARN);
  char s[6];
  snprintf(s, sizeof(s), "%u", pet.streak);
  gfx->setTextColor(inkColor());
  setSize(2);
  setCur(x + 22, y + 2);
  printT(s);
}

// banner temporaneo: medaglia nuova o traguardo di serie
void drawCelebration() {
  const char *l1 = nullptr, *l2 = nullptr;
  // 32 e non 20: "%u にちれんぞく！" supera già 20 con una sola cifra, e snprintf tagliava a
  // metà di un carattere da 3 byte.
  char buf[32];
  if (pet.showMedal()) {
    for (int i = 0; i < MED_COUNT; i++)
      if (pet.newMedal & (1 << i)) { l2 = medalName(i); break; }
    l1 = T(S_MEDAL_BANNER);
  } else if (pet.showMilestone()) {
    snprintf(buf, sizeof(buf), T(S_STREAK_DAYS_FMT), pet.streak);
    l1 = T(S_GREAT);
    l2 = buf;
  }
  if (!l1 || !l2) return;  // medaglia senza bit valido: niente banner, non un crash
  gfx->fillRoundRect(73, 150, 320, 96, 16, UI_BAR_WARN);
  gfx->drawRoundRect(73, 150, 320, 96, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(l1, 3), 176);
  printT(l1);
  setSize(2);
  setCur(centerX(l2, 2), 212);
  printT(l2);
}

void drawBattery() {
  int pc = halBatPercent();
  if (pc < 0) return;  // senza batteria collegata
  int x = CX - 14, y = 12, w = 24, h = 11;
  bool charging = halBatCharging();
  uint16_t col = charging ? UI_BAR_OK
                 : (pc >= 40) ? inkColor()
                 : (pc >= 15) ? UI_BAR_WARN
                              : UI_BAR_BAD;
  gfx->drawRoundRect(x, y, w, h, 2, col);
  gfx->fillRect(x + w, y + 3, 3, 5, col);  // morsetto
  if (charging) {
    // fulmine di carica (zigzag) al posto della barra del livello
    uint16_t bolt = C565(0xff, 0xd9, 0x4a);
    int bx = x + w / 2;
    gfx->fillTriangle(bx + 3, y + 1, bx - 4, y + 6, bx + 1, y + 6, bolt);
    gfx->fillTriangle(bx - 1, y + 5, bx + 4, y + 5, bx - 3, y + 10, bolt);
  } else {
    int fw = (w - 4) * pc / 100;
    if (fw > 0) gfx->fillRect(x + 2, y + 2, fw, h - 4, col);
  }
}

void drawHeader(const char *name, uint16_t nameColor, const char *msg) {
  drawBattery();
  gfx->setTextColor(nameColor);
  setSize(3);
  setCur(centerX(name, 3), 52);
  printT(name);
  gfx->setTextColor(inkColor());
  setSize(2);
  setCur(centerX(msg, 2), 90);
  printT(msg);
}

// animazione della cerimonia (10s): addio = inchino con cuori e si allontana
// camminando; fuga = si spaventa ed esce correndo. Sostituisce l'idle.
void drawCeremony() {
  if (!pmd.loaded) { drawPet(); return; }  // riserva se non c'è lo sprite PMD
  uint32_t now = millis();
  float t = pet.ceremonyT();               // 0..1 lungo i 10s
  bool panic = (pet.ceremony == CER_RUNAWAY);
  int x = CX, y = PET_GROUND;
  uint8_t act = PMD_IDLE;

  if (panic) {
    // finale triste: penombra bluastra + pioggia
    for (int i = 0; i < 46; i++) {
      int rx = (i * 47 + now / 3) % 466;
      int ry = (i * 91 + now / 2) % 470;
      gfx->drawLine(rx, ry, rx - 3, ry + 12, C565(0x6a, 0x84, 0xb0));
    }
    bool fade = false;
    if (t < 0.30f) {                       // a testa bassa, tremando
      act = pmd.has(PMD_HURT) ? PMD_HURT : PMD_IDLE;
      x = CX + (int)(4 * sinf(now * 0.04f));
    } else {                               // si allontana lentamente e svanisce
      act = pmd.has(PMD_WALKL) ? PMD_WALKL : PMD_IDLE;
      x = CX - (int)(((t - 0.30f) / 0.70f) * (CX + 120));
      fade = (t > 0.6f) && ((now / 160) % 2 == 0);  // lampeggia verso la silhouette
    }
    drawPmdAct(act, x, y, now, true, fade, 5);  // fade=silhouette: si sfuma mentre se ne va
    // lacrima che cade dal Pokémon
    if (t < 0.55f) {
      int ty = y - 150 + (int)((now / 6) % 40);
      gfx->fillRect(x + 6, ty, 3, 6, C565(0x9a, 0xc4, 0xe8));
    }
    return;
  }

  // addio epico: alone dorato pulsante + scintille e cuori che salgono
  int gcy = PET_GROUND - 96;
  for (int k = 0; k < 4; k++) {
    int r = 60 + k * 34 + (int)(10 * sinf(now * 0.02f));
    gfx->drawCircle(CX, gcy, r, C565(0xff, 0xdf, 0x8a));
  }
  for (int i = 0; i < 16; i++) {
    int px = (i * 71 + 28) % 466;
    int py = 410 - (int)((now / 8 + i * 70) % 360);   // salgono e riappaiono in basso
    if (py < 30) continue;
    if (i % 4 == 0) drawMap(SPR_HEART, 32, px - 8, py - 8, 1, false);  // cuoricino
    else gfx->fillRect(px, py, 4, 4, (i % 2) ? C565(0xff, 0xe7, 0x9f) : C565(0xff, 0x9a, 0xc0));
  }

  if (t < 0.45f) {                         // inchino / posa di addio
    act = pmd.has(PMD_POSE) ? PMD_POSE : (pmd.has(PMD_NOD) ? PMD_NOD : PMD_IDLE);
  } else {                                 // si allontana verso destra
    act = pmd.has(PMD_WALKR) ? PMD_WALKR : PMD_IDLE;
    x = CX + (int)(((t - 0.45f) / 0.55f) * (CX + 140));
  }
  drawPmdAct(act, x, y, now, true, false, 5);
  if (pet.showHeart())                     // cuore grande che segue il Pokémon
    drawMap(SPR_HEART, 32, x + 50, y - 190, 2, false);
}

// dialogo di decisione (2 pulsanti impilati): evolvere/mantenere, dire addio/restare
// insieme o lasciarlo scappare/prendersene cura
void drawChoiceDialog() {
  const char *q, *o1, *o2;
  uint16_t c1, c2, t1, t2;
  if (gOverlay == OVL_EVOLVE) {
    q = T(S_EVO_Q); o1 = T(S_EVO_TAP); o2 = T(S_EVO_KEEP);
    c1 = UI_BAR_BAD; t1 = UI_WHITE; c2 = UI_TRACK; t2 = UI_INK;
  } else if (gOverlay == OVL_FAREWELL) {
    q = T(S_FAR_Q); o1 = T(S_FAR_GO); o2 = T(S_FAR_STAY);
    c1 = UI_BAR_WARN; t1 = UI_INK; c2 = UI_BAR_OK; t2 = UI_WHITE;
  } else {                // fuga: i colori cupi del suo pulsante
    q = T(S_RUN_Q); o1 = T(S_RUN_GO); o2 = T(S_RUN_STAY);
    c1 = C565(0x3a, 0x44, 0x5a); t1 = C565(0xc8, 0xd2, 0xe0); c2 = UI_BAR_OK; t2 = UI_WHITE;
  }
  gfx->fillRoundRect(73, 156, 320, 188, 16, UI_WHITE);
  gfx->drawRoundRect(73, 156, 320, 188, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(q, 2), 176);
  printT(q);
  gfx->fillRoundRect(93, 206, 280, 52, 12, c1);     // pulsante azione
  gfx->setTextColor(t1);
  setCur(centerX(o1, 2), 224);
  printT(o1);
  gfx->fillRoundRect(93, 268, 280, 52, 12, c2);     // pulsante mantieni/restare insieme
  gfx->setTextColor(t2);
  setCur(centerX(o2, 2), 286);
  printT(o2);
}

// pulsante-CTA rosso e grande per evolvere (pulsa per attirare l'attenzione)
void drawEvolveButton() {
  uint32_t now = millis();
  int p = (int)(5 * sinf(now * 0.006f));  // pulsa: -5..5
  int x = EVO_BTN_X - p, y = EVO_BTN_Y - p, w = EVO_BTN_W + 2 * p, h = EVO_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 18, UI_BAR_BAD);
  gfx->drawRoundRect(x, y, w, h, 18, UI_WHITE);
  gfx->drawRoundRect(x + 2, y + 2, w - 4, h - 4, 16, UI_WHITE);
  gfx->setTextColor(UI_WHITE);
  setSize(3);
  const char *t = T(S_EVO_TAP);
  setCur(centerX(t, 3), y + h / 2 - 11);
  printT(t);
}

// pulsante-CTA dorato di addio: "<nome> vuole dirti qualcosa..."
void drawFarewellButton() {
  uint32_t now = millis();
  int p = (int)(4 * sinf(now * 0.005f));
  int x = FAR_BTN_X - p, y = FAR_BTN_Y - p, w = FAR_BTN_W + 2 * p, h = FAR_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 16, UI_BAR_WARN);
  gfx->drawRoundRect(x, y, w, h, 16, UI_INK);
  char buf[52];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  snprintf(buf, sizeof(buf), T(S_FAREWELL_BTN), nm);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(buf, 2), y + h / 2 - 8);
  printT(buf);
}

// pulsante-CTA cupo di fuga per abbandono: "<nome> si sente abbandonato..."
// (finale triste: blu-grigio scuro, battito lento e spento)
void drawRunawayButton() {
  uint32_t now = millis();
  int p = (int)(3 * sinf(now * 0.003f));
  int x = FAR_BTN_X - p, y = FAR_BTN_Y - p, w = FAR_BTN_W + 2 * p, h = FAR_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 16, C565(0x3a, 0x44, 0x5a));
  gfx->drawRoundRect(x, y, w, h, 16, C565(0x70, 0x80, 0x98));
  char buf[52];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  snprintf(buf, sizeof(buf), T(S_RUNAWAY_BTN), nm);
  gfx->setTextColor(C565(0xc8, 0xd2, 0xe0));
  setSize(2);
  setCur(centerX(buf, 2), y + h / 2 - 8);
  printT(buf);
}

// animazione epica di evoluzione: alone radiale + raggi rotanti + lampeggio dello
// sprite che accelera + scintille sparate + lampo finale
void drawEvolveFX(uint32_t now) {
  float t = pet.evolveT();          // 0..1
  int cx = CX, cy = PET_GROUND - 96;

  // alone radiale che cresce e pulsa
  int halo = 36 + (int)(t * 150) + (int)(8 * sinf(now * 0.02f));
  for (int k = 0; k < 4; k++) {
    int r = halo - k * 7;
    if (r > 0) gfx->drawCircle(cx, cy, r, UI_WHITE);
  }
  // raggi rotanti dal centro del Pokémon
  float base = now * 0.004f;
  for (int i = 0; i < 12; i++) {
    float a = base + i * (float)(PI / 6);
    int len = 90 + (int)(70 * (0.5f + 0.5f * sinf(now * 0.012f + i)));
    gfx->drawLine(cx, cy, cx + (int)(cosf(a) * len), cy + (int)(sinf(a) * len), UI_WHITE);
  }
  // lampeggio tra la forma PRECEDENTE e la NUOVA (silhouette), accelerando; alla fine
  // (t>0.9) resta fissa sulla nuova per il lampo di rivelazione
  int period = 60 + (int)(220 * (1.0f - t));
  bool showOld = t < 0.9f && evoPmd.loaded && ((now / period) % 2) == 0;
  if (showOld) drawPmdActM(evoPmd, PMD_IDLE, cx, PET_GROUND, 0, true, true, 5);
  else drawPmdAct(PMD_IDLE, cx, PET_GROUND, 0, true, true, 5);
  // scintille sparate
  for (int i = 0; i < 10; i++) {
    float a = i * (float)(PI / 5) + t * 4.0f;
    int d = (int)((now / 14 + i * 33) % 200);
    int sx = cx + (int)(cosf(a) * d), sy = cy + (int)(sinf(a) * d);
    gfx->fillRect(sx - 2, sy - 2, 5, 5, (i & 1) ? C565(0xff, 0xe0, 0x70) : UI_WHITE);
  }
  // lampo finale prima di rivelare la forma nuova
  if (t > 0.9f) gfx->fillCircle(cx, cy, (int)(300 * (t - 0.9f) / 0.1f), UI_WHITE);
}

void drawPet() {
  if (pmd.loaded) {
    drawPetPMD();
    return;
  }
  if (mon.loaded) {
    drawPetSD();
    return;
  }
  int fi = flashIdxForDex(pet.speciesId);
  if (fi < 0) {
    // senza SD e senza sprite in flash: avviso chiaro che mancano gli sprite
    gfx->setTextColor(inkColor());
    setSize(6);
    setCur(CX - 18, PET_CY - 80);
    printT("?");
    setSize(2);
    const char *l1 = T(S_NO_SPRITES);
    setCur(centerX(l1, 2), PET_CY - 4);
    printT(l1);
    const char *l2 = T(S_LOAD_SPRITES);
    setCur(centerX(l2, 2), PET_CY + 20);
    printT(l2);
    return;
  }
  const Species &sp = SPECIES[fi];
  int s = sp.scale;
  int x = CX - 16 * s;
  int y = PET_CY - 16 * s;

  // animazione di evoluzione: alterna la silhouette della forma precedente e della
  // nuova
  if (pet.evolving()) {
    bool flash = (millis() / 300) % 2;
    int16_t showDex = (flash && pet.prevSpeciesId >= 0) ? pet.prevSpeciesId : pet.speciesId;
    int sfi = flashIdxForDex(showDex);
    if (sfi >= 0) {
      const Species &show = SPECIES[sfi];
      drawMap(show.sprite, SPRITE_H, CX - 16 * show.scale, PET_CY - 16 * show.scale, show.scale, flash);
    }
    return;
  }

  PetMood m = pet.mood();
  if (m == MOOD_HAPPY && (millis() / 500) % 2) y -= 6;  // saltello

  drawMap(sp.sprite, SPRITE_H, x, y, s, false);

  // espressioni sovrapposte usando gli ancoraggi della specie
  bool blink = (millis() % 3500 < 300);
  if (m == MOOD_SLEEPING || blink) {
    overlayEye(sp, x, y, s, sp.eyeColL);
    overlayEye(sp, x, y, s, sp.eyeColR);
  }
  if (m == MOOD_EATING) overlayMouth(sp, x, y, s, true);
  else if (m == MOOD_SAD) overlayMouth(sp, x, y, s, false);

  if (pet.showHeart()) drawMap(SPR_HEART, 32, x + 20 * s, y - 2 * s, 2, false);
}

// ---------- scena del bagnetto ----------

void startBath() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony || bathUntil) return;
  bathUntil = millis() + 3000;
  bathPending = true;
  int cx = (int)beh.x;
  for (auto &b : bubbles) {
    b.x = cx - 70 + random(140);
    b.y = PET_GROUND - random(150);
    b.r = 8 + random(16);
    b.ph = random(64);
  }
}

void drawBath() {
  uint32_t now = millis();
  if (!timeLeft(bathUntil)) {
    bathUntil = 0;
    if (bathPending) {
      bathPending = false;
      pet.clean();
      // posa di gioia quando è pulito
      if (pmd.has(PMD_POSE)) {
        beh.mode = 2;
        beh.act = PMD_POSE;
        beh.t0 = now;
        beh.until = now + pmdActTotalMs(pmd.acts[PMD_POSE]) * 2;
      }
    }
    return;
  }
  uint32_t left = bathUntil - now;
  if (left > 800) {
    // schiuma: bolle che dondolano e salgono a poco a poco
    float t = now / 220.0f;
    for (auto &b : bubbles) {
      int bx = b.x + (int)(sinf(t + b.ph) * 6);
      int by = b.y - (int)((3000 - left) / 90);
      gfx->fillCircle(bx, by, b.r, UI_WHITE);
      gfx->drawCircle(bx, by, b.r, 0x7E3D);
      gfx->fillCircle(bx - b.r / 3, by - b.r / 3, b.r / 4, UI_BG_DAY);
    }
  } else {
    // le bolle scoppiano: scintille
    for (int i = 0; i < 8; i++) {
      auto &b = bubbles[i];
      int sx = b.x + (i % 3) * 6 - 6, sy = b.y - 18;
      uint16_t col = (i % 2) ? UI_BAR_WARN : UI_WHITE;
      gfx->fillRect(sx - 6, sy - 1, 13, 3, col);
      gfx->fillRect(sx - 1, sy - 6, 3, 13, col);
    }
  }
}

// sceglie il prossimo capriccio del Pokémon quando è contento
void behNext() {
  uint32_t now = millis();
  beh.t0 = now;
  int r = random(100);
  if (r < 35 && (pmd.has(PMD_WALKL) || pmd.has(PMD_WALKR))) {
    beh.mode = 1;  // passeggiata
    beh.targetX = 150 + random(176);
    beh.until = now + 15000;
  } else if (r < 60) {
    // gesto casuale tra quelli disponibili
    // (Hop escluso: salta troppo in alto; Sit escluso: guarda indietro)
    static const uint8_t flair[] = { PMD_POSE, PMD_NOD, PMD_BREATH };
    uint8_t pick[3], n = 0;
    for (uint8_t f : flair)
      if (pmd.has(f)) pick[n++] = f;
    if (n) {
      beh.mode = 2;
      beh.act = pick[random(n)];
      beh.until = now + pmdActTotalMs(pmd.acts[beh.act]);
      return;
    }
    beh.mode = 0;
    beh.until = now + 2000 + random(3000);
  } else {
    beh.mode = 0;  // guardare avanti
    beh.until = now + 2000 + random(3000);
  }
}

// ---------- scossone e colpetti sulla principale ----------
// Il loop() tiene il sensore in VEGLIA solo qui, con lo schermo acceso o attenuato e
// MOVIMENTO acceso; gli eventi arrivano da halMotoEventi(). Il dito sul vetro scuote la
// scheda: un evento entro MOTO_TOCCO_MS da un tocco (prima o dopo) non conta, e per saperlo
// si aspetta MOTO_ATTESA_MS, perché il touch si legge ogni 20 ms e il sensore va più svelto.

#define MOTO_TOCCO_MS 300
#define MOTO_ATTESA_MS 100
#define REAZIONE_MS 1500    // il segno sopra il pet e l'uovo che oscilla

static uint8_t motoEv = 0;           // eventi in attesa
static uint32_t motoEvMs = 0;        // quando è arrivato il primo
static uint32_t reazioneFino = 0;
static char reazioneSegno = 0;       // '!' scossone, '?' colpetti, 0 nessuno

// gesto una tantum del pet (lo sprite PMD), al posto di passeggiata e capricci
static void behGesto(uint8_t act, uint32_t now) {
  if (!pmd.has(act)) return;
  beh.mode = 2;
  beh.act = act;
  beh.t0 = now;
  uint32_t d = pmdActTotalMs(pmd.acts[act]);
  beh.until = now + (d > REAZIONE_MS ? d : REAZIONE_MS);
}

void motoPrincipale() {
  if (motoConsoleAttivo()) return;  // per i 5 s di MOTO gli eventi li stampa la console
  uint32_t now = millis();
  uint8_t ev = halMotoEventi();     // si leggono sempre: anche quelli dei giochi vanno tolti
  if (ev && gScreen == SCR_MAIN) {
    if (!motoEv) motoEvMs = now;
    motoEv |= ev;
  }
  if (!motoEv || now - motoEvMs < MOTO_ATTESA_MS) return;
  ev = motoEv;
  motoEv = 0;
  // solo sulla principale senza overlay (nella palla, nel sacco e nei menu batte il dito)
  if (gScreen != SCR_MAIN || gOverlay != OVL_NONE || saveDlg || screenOff || !pet.movimento())
    return;
  if ((int32_t)(motoEvMs - gUltimoTocco) < MOTO_TOCCO_MS) return;
  if (pet.evolving()) return;
  bool attenuato = dimStage > 0;
  lastInteract = now;  // riaccende lo schermo attenuato
  if (ev & HAL_EV_SCOSSONE) {
    reazioneFino = now + REAZIONE_MS;
    if (pet.isEgg()) {  // l'uovo oscilla e basta
      reazioneSegno = 0;
      return;
    }
    uint8_t tolta = pet.spavento();
    Serial.printf("MOTO scossone gioia=-%u pausa=%d\n", (unsigned)tolta,
                  tolta ? 0 : (pet.spaventoInPausa() ? 1 : 0));
    reazioneSegno = '!';
    halSfx(SFX_DENY);
    if (!pet.sleeping) behGesto(PMD_HURT, now);  // nel sonno resta addormentato
    return;
  }
  // doppio colpetto: con lo schermo attenuato o nel sonno basta la luce; nessuna statistica
  if (attenuato || pet.sleeping) return;
  reazioneFino = now + REAZIONE_MS;
  if (pet.isEgg()) {
    reazioneSegno = 0;
    return;
  }
  reazioneSegno = '?';
  // si volta verso chi ha bussato: smette di passeggiare e fa un cenno o un saltello
  uint8_t scelta[2], n = 0;
  if (pmd.has(PMD_NOD)) scelta[n++] = PMD_NOD;
  if (pmd.has(PMD_HOP)) scelta[n++] = PMD_HOP;
  if (n) behGesto(scelta[random(n)], now);
  else behGesto(PMD_IDLE, now);
}

// l'uovo che oscilla dopo uno scossone o un colpetto, smorzato: scostamento in pixel
int uovoDondolo() {
  uint32_t resta = timeLeft(reazioneFino);
  if (!resta) return 0;
  return (int)(10.0f * resta / REAZIONE_MS * sinf(millis() * 0.035f));
}

// il segno della reazione accanto alla testa del pet
void drawReazione() {
  if (!reazioneSegno || !timeLeft(reazioneFino)) return;
  char t[2] = { reazioneSegno, 0 };
  gfx->setTextColor(reazioneSegno == '!' ? UI_BAR_BAD : inkColor());
  setSize(5);
  setCur((int)beh.x - 75, PET_GROUND - 160);
  printT(t);
}

void drawPetPMD() {
  uint32_t now = millis();

  if (pet.evolving()) {
    drawEvolveFX(now);
    return;
  }
  if (evoPmd.loaded) evoPmd.unload();  // l'evoluzione è finita: libera la forma precedente

  PetMood m = pet.mood();
  uint8_t act;
  bool loop = true;
  if (m == MOOD_SLEEPING && pmd.has(PMD_SLEEP)) {
    act = PMD_SLEEP;
    beh.mode = 0;
  } else if (m == MOOD_EATING && pmd.has(PMD_EAT)) {
    act = PMD_EAT;
    beh.t0 = 0;
  } else if (m == MOOD_SAD && pmd.has(PMD_HURT)) {
    act = PMD_HURT;
  } else {
    // contento: decide il pianificatore (idle / passeggiata / gesto)
    if (now > beh.until) behNext();
    if (beh.mode == 1) {
      float d = beh.targetX - beh.x;
      if (fabsf(d) < 4) {
        behNext();
        act = PMD_IDLE;
      } else {
        beh.x += (d > 0 ? 3.0f : -3.0f);
        act = (d > 0) ? PMD_WALKR : PMD_WALKL;
      }
    } else {
      act = (beh.mode == 2) ? beh.act : PMD_IDLE;
      loop = false;
    }
    if (!pmd.has(act)) act = PMD_IDLE;
  }

  drawPmdAct(act, (int)beh.x, PET_GROUND, now - beh.t0, loop || act == PMD_IDLE, false, 5);

  if (pet.showHeart()) drawMap(SPR_HEART, 32, (int)beh.x + 50, PET_GROUND - 190, 2, false);
}

// sprite animato dalla SD: zoom intero per pixel, frame al loro ritmo
void drawPetSD() {
  int s = mon.scale;
  int w = mon.w * s, h = mon.h * s;
  int x = CX - w / 2;
  int y = PET_CY - h / 2;

  bool sil = false;
  if (pet.evolving()) {
    sil = (millis() / 300) % 2;
  } else if (pet.mood() == MOOD_HAPPY && (millis() / 500) % 2) {
    y -= 6;  // saltello
  }

  uint16_t fm = mon.frameMs ? mon.frameMs : 100;
  uint16_t fi = pet.sleeping ? 0 : (millis() / fm) % mon.frames;
  const uint8_t *fr = mon.data + (uint32_t)fi * mon.w * mon.h;
  for (int r = 0; r < mon.h; r++) {
    const uint8_t *row = fr + r * mon.w;
    for (int c = 0; c < mon.w; c++) {
      uint8_t idx = row[c];
      if (idx == 0xFF) continue;
      gfx->fillRect(x + c * s, y + r * s, s, s, sil ? INK_K : mon.pal[idx]);
    }
  }

  // emote invece di espressioni (gli sprite importati non hanno ancoraggi)
  if (pet.showHeart()) drawMap(SPR_HEART, 32, x + w - 30, y - 50, 2, false);
}

// occhio chiuso: cancella l'occhio 3x4 e disegna la palpebra
void overlayEye(const Species &sp, int x, int y, int s, int col) {
  gfx->fillRect(x + col * s, y + sp.eyeRow * s, 3 * s, 4 * s, sp.bodyColor);
  gfx->fillRect(x + col * s, y + (sp.eyeRow + 2) * s, 3 * s, s, INK_K);
}

// cancella il sorriso base e disegna bocca aperta (mangiare) o cipiglio (triste)
void overlayMouth(const Species &sp, int x, int y, int s, bool open) {
  int mc = sp.mouthCol, mr = sp.mouthRow;
  gfx->fillRect(x + (mc - 3) * s, y + mr * s, 7 * s, 2 * s, sp.bodyColor);
  if (open) {
    gfx->fillRect(x + (mc - 2) * s, y + mr * s, 5 * s, 2 * s, INK_K);
  } else {
    gfx->fillRect(x + (mc - 2) * s, y + mr * s, 5 * s, s, INK_K);
    gfx->fillRect(x + (mc - 3) * s, y + (mr + 1) * s, s, s, INK_K);
    gfx->fillRect(x + (mc + 3) * s, y + (mr + 1) * s, s, s, INK_K);
  }
}

void drawPoops() {
  for (int i = 0; i < pet.poops; i++) {
    drawMap(SPR_POOP, 32, 36 + i * 46, 244, 2, false);
  }
}

void drawBars() {
  drawBar(78, 318, T(S_BAR_FOOD), pet.fullness);
  drawBar(244, 318, T(S_BAR_JOY), pet.joy);
  drawBar(78, 346, T(S_BAR_ENE), pet.energy);
  drawBar(244, 346, T(S_BAR_HYG), pet.hygiene);
}

// Separazione tra l'etichetta e la sua barra nella riga dei bisogni. Era fissa a 48 px,
// proprio quanto occupano 4 lettere latine a scala 2, senza margine. Le etichette
// giapponesi sono più larghe (ごきげん sono 4 kana, ~64 px) e finivano dentro la barra. Si
// ricava dall'etichetta più lunga, con il 48 come minimo perché nelle lingue latine non
// cambi nulla. Richiede la dimensione di testo 2 già impostata, perché textW() ne ha
// bisogno.
static int barLabelGap() {
  const StrId ids[] = { S_BAR_FOOD, S_BAR_JOY, S_BAR_ENE, S_BAR_HYG };
  int ancho = 0;
  for (StrId id : ids) {
    int w = textW(T(id), 2);
    if (w > ancho) ancho = w;
  }
  // sotto la soglia originale si lascia ESATTAMENTE 48, per non spostare nulla nelle
  // lingue latine; sopra si aggiunge aria perché l'etichetta respiri
  return ancho <= 48 ? 48 : ancho + 8;
}

void drawBar(int x, int y, const char *label, uint8_t val) {
  gfx->setTextColor(inkColor());
  setSize(2);
  setCur(x, y);
  printT(label);
  int gap = barLabelGap();
  // le due colonne iniziano a 78 e 244: la barra della prima deve finire prima
  // dell'etichetta della seconda, ed entrambe si disegnano ugualmente larghe
  int bw = 232 - (78 + gap);
  if (bw > 100) bw = 100;   // con etichette latine viene 100, come prima
  int bx = x + gap, bh = 15;
  uint16_t fill = (val >= 50) ? UI_BAR_OK : (val >= 25) ? UI_BAR_WARN : UI_BAR_BAD;
  gfx->fillRoundRect(bx, y, bw, bh, 4, UI_TRACK);
  int fw = (bw - 4) * val / 100;
  if (fw > 0) gfx->fillRoundRect(bx + 2, y + 2, fw, bh - 4, 3, fill);
}

void drawButtons() {
  for (int i = 0; i < 4; i++) {
    bool off = pet.sleeping && i != 2;  // se dorme funziona solo LUCE
    int bx = buttons[i].cx - BTN_HALF, by = buttons[i].cy - BTN_HALF;
    if (!pet.sleeping) gfx->fillRoundRect(bx, by, 2 * BTN_HALF, 2 * BTN_HALF, 14, UI_WHITE);
    gfx->drawRoundRect(bx, by, 2 * BTN_HALF, 2 * BTN_HALF, 14, inkColor());
    if (!off) drawMap(buttons[i].icon, 16, buttons[i].cx - 16, buttons[i].cy - 16, 2, false);
  }
}

const char *eggMsg() {
  switch (pet.eggCracks()) {
    case 0: return T(S_EGG_TOUCH);
    case 1: return T(S_EGG_MOVES);
    default: return T(S_EGG_ALMOST);
  }
}

const char *statusMsg() {
  if (pet.evolving()) return T(S_EVOLVING);
  if (bathUntil) return "Splish splash!";  // onomatopea universale
  if (pet.sleeping) return "Zzz...";
  if (pet.eating()) return T(S_EATING);
  if (pet.showHeart()) return T(S_LIKES);
  if (pet.fullness < 25) return T(S_HUNGRY);
  if (pet.hygiene < 25) return T(S_NEEDS_BATH);
  if (pet.energy < 25) return T(S_EXHAUSTED);
  if (pet.joy < 25) return T(S_SAD);
  if (pet.weight > 60) return T(S_CHUBBY);
  if (pet.shiny && pet.ageMinutes < 15) return T(S_IS_SHINY);
  return T(S_HAPPY);
}
