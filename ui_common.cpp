// Parte comune della UI: stato condiviso tra le schermate, testo e font, colori,
// miniature, sprite PMD e mappe di pixel.

#include "ui_common.h"
#include "moto.h"

// font CJK attivo: per ora sempre false, il caricamento arriverà con il supporto a
// giapponese/coreano/cinese (vedi PR #13)
bool gCjkFont = false;
Pet pet;

// sprite animato della SD per la specie attuale (se il file esiste)
SdMon mon;          // sprite B/N (riserva e minigioco se non c'è PMD)
PmdMon pmd;         // sprite PMD multi-azione (schermata principale)
PmdMon galleryPmd;  // sprite grande della vista dettaglio della galleria (PMD/PGK2, legale)

bool gLogTocchi = false;     // console TOCCHI: ogni gesto riconosciuto sulla seriale
bool screenOff = false;       // pressione breve del pulsante PWR

// ---------- macchina a stati della UI (vedi enum Screen in ui_common.h) ----------
Screen gScreen = SCR_MAIN;
Overlay gOverlay = OVL_NONE;
uint32_t gOverlayUntil = 0;

// Per aggiungere una schermata: la sua riga qui (nell'ordine dell'enum), i gestori
// dichiarati in ui_common.h, e aprirla/chiuderla solo con goScreen().
const ScreenOps SCREENS[SCR_COUNT] = {
  //  render               onTap          onSwipe       onSwipeV     onEnter        onExit
  { renderStarterSelect, starterTap,    nullptr,      nullptr,     nullptr,       nullptr     },  // SCR_STARTER
  { renderMain,          mainTap,       mainSwipe,    mainSwipeV,  nullptr,       mainExit    },  // SCR_MAIN
  { renderGallery,       galleryTap,    gallerySwipe, nullptr,     galleryEnter,  galleryExit },  // SCR_GALLERY
  { renderCard,          cardTap,       cardSwipe,    cardSwipeV,  cardEnter,     nullptr     },  // SCR_CARD
  { renderKeyboard,      keyboardTap,   nullptr,      nullptr,     keyboardEnter, nullptr     },  // SCR_KEYBOARD
  { renderClock,         clockTap,      nullptr,      clockSwipeV, clockEnter,    clockExit   },  // SCR_CLOCK
  { renderGame,          gameTap,       nullptr,      nullptr,     gameEnter,     nullptr     },  // SCR_GAME
  { renderSack,          sackScreenTap, nullptr,      nullptr,     sackEnter,     nullptr     },  // SCR_SACK
  { renderCeremony,      colDomandaTap, nullptr,      nullptr,     nullptr,       nullptr     },  // SCR_CEREMONY
  { renderBattle,        battleTap,     nullptr,      nullptr,     battleEnter,   battleExit  },  // SCR_BATTLE
  { renderCollezione,    collezioneTap, collezioneSwipe, nullptr,  collezioneEnter, nullptr    },  // SCR_COLLEZIONE
  { renderLink,          linkTap,       nullptr,      linkSwipeV,  linkEnter,     linkExit    },  // SCR_LINK
  { renderGrotta,        grottaTap,     nullptr,      nullptr,     grottaEnter,   grottaExit  },  // SCR_GROTTA
  { renderSurf,          surfTap,       nullptr,      nullptr,     surfEnter,     surfExit    },  // SCR_SURF
  { renderAggiorna,      aggiornaTap,   nullptr,      nullptr,     aggiornaEnter, aggiornaExit },  // SCR_AGGIORNA
};

void goScreen(Screen s) {
  if (SCREENS[gScreen].onExit) SCREENS[gScreen].onExit();
  gScreen = s;
  if (SCREENS[s].onEnter) SCREENS[s].onEnter();
}

// Allinea gScreen allo stato del gioco, che cambia anche fuori dalla UI (console,
// ripristino di un salvataggio, fine della cerimonia nel tick): la scelta dell'iniziale
// prevale su ogni schermata, la cerimonia sostituisce la principale (dalle altre
// subentra al ritorno) e interrompe la battaglia, che durerebbe più della cerimonia.
void syncScreen() {
  if (pet.awaitingStarter()) {
    if (gScreen != SCR_STARTER) goScreen(SCR_STARTER);
    return;
  }
  if (gScreen == SCR_STARTER) goScreen(SCR_MAIN);
  if ((gScreen == SCR_MAIN || gScreen == SCR_BATTLE) && pet.ceremony) goScreen(SCR_CEREMONY);
  else if (gScreen == SCR_CEREMONY && !pet.ceremony) goScreen(SCR_MAIN);
}

// le 9 specie con sprite proprio in flash (riserva senza SD): dex -> indice
int flashIdxForDex(int16_t dex) {
  static const int8_t IDX[10] = { -1, 3, 4, 5, 0, 1, 2, 6, 7, 8 };
  return (dex >= 1 && dex <= 9) ? IDX[dex] : -1;
}
// protezione dell'AMOLED: attenuato per inattività
uint32_t lastInteract = 0;
uint8_t dimStage = 0;        // 0 sveglio, 1 attenuato (90s), 2 quasi spento (5min)
uint32_t holdStart = 0;     // pressione lunga sul Pokémon
// salvataggi: sottoschermata delle impostazioni e dialogo di ripristino/importazione
uint8_t saveDlg = 0;        // 0 nessuno, 1 conferma, 2 errore, 3 ripristino in corso

// ---------- render ----------

bool gNight = false;  // notte reale (per ora) o dorme: lo fissa render()
uint16_t inkColor() { return gNight ? UI_INK_NIGHT : UI_INK; }

uint16_t lerp565(uint16_t a, uint16_t b, int i, int n) {
  if (n <= 0) return a;
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)((((ar + (br - ar) * i / n) << 11)) |
                    (((ag + (bg - ag) * i / n) << 5)) | (ab + (bb - ab) * i / n));
}

// ora del giorno 0-23 (dall'ora reale in cache ogni 30s; 13 se non c'è orologio)
int sceneHour() {
  uint32_t e = pet.lastSeenEpoch;
  return e ? (int)((e / 3600) % 24) : 13;
}

// ---------- testo: dimensione e cursore (predisposto per i font CJK) ----------
// Il font classico àncora il cursore nell'ANGOLO SUPERIORE del testo, e le 74
// coordinate Y del codice sono scritte così. I font U8g2 lo ancorano sulla
// LINEA DI BASE (drawChar fa curY = y - baseline*size), quindi cambiando
// font tutto il testo salirebbe. setCur() compensa questa differenza in un solo
// punto, così la Y continua a significare "in alto" con qualunque font.
//
// setSize() esiste solo perché la libreria ha textsize_x protetto e setCur()
// deve conoscere la scala attiva per moltiplicare l'ascendente.
//
// Con il font classico gFontAscent è 0, quindi setCur(x,y) è esattamente
// setCur(x,y): questo non sposta un pixel nelle sei lingue attuali.
uint8_t gTextSize = 1;
int gFontAscent = 0;  // px dal bordo superiore alla linea di base, 0 = font classico

// --- regolazione del font CJK (i due valori vanno insieme) ---
//
// Il font classico misura 8 px di altezza a scala 1. Se il CJK è più alto, bisogna
// dividere la scala perché le dimensioni tornino... ma dividere ASSOTTIGLIA il tratto:
// a scala 1 la unifont disegna linee da 1 px mentre il latino a scala 2 le disegna da
// 2, e il giapponese appare tenue anche se ha la stessa altezza.
//
// Per questo conviene un font base piccolo E IN GRASSETTO (_b_), che permetta di usare
// la STESSA scala del latino: così lo spessore coincide e non serve dividere.
//
// Si è provato un font davvero grassetto (b10_b) e NON va bene a questa dimensione: i
// tratti si fondono e i kana perdono dettaglio, illeggibili anche per un madrelingua.
// Si torna alla unifont, la più leggibile, e il tratto sottile si compensa in printT()
// ridisegnando spostato di 1 px (pseudo-grassetto).
//
// japanese3 e non japanese1: i sottoinsiemi 1 e 2 contengono kana e kanji ma NESSUNA
// punteggiatura CJK, e Arduino_GFX non disegna nulla quando manca il glifo (né avanza
// il cursore), quindi i 25 segni ！ e ？ delle stringhe sparivano. È la stessa famiglia
// unifont: intestazione delle metriche identica e sovrainsieme stretto, verificato
// glifo per glifo sui 226 codepoint che usa il firmware. Costa 102 KB di flash in più e
// non sposta un pixel.
//
// Il coreano richiede un font con hangul, e CJK_FONT era un solo #define. Invece di
// dare per scontato che un sottoinsieme serva per entrambe le scritture, il font si
// sceglie per lingua. korean2 e non korean1: il coreano di qui usa 296 sillabe distinte
// e sono tutte in KS X 1001, l'insieme da 2350 che è la dimensione misurata per
// korean2; korean1 ne ha 478. Il controllo guarda l'appartenenza a KS X 1001, non
// scorre la tabella del font, quindi si appoggia al fatto che l'insieme di korean2 sia
// quello; l'ASCII si dà per presente.
//
// Non verificato sulla scheda: se CJK_SIZE_DIV 2 vada bene a korean2 come al
// giapponese. L'ascendente invece si misura a runtime in applyLangFont().
#define CJK_FONT_JA u8g2_font_unifont_t_japanese3
#define CJK_FONT_KO u8g2_font_unifont_t_korean2
#define CJK_SIZE_DIV 2

void setSize(uint8_t n) {
  // gTextSize conserva la scala REALMENTE applicata, non quella richiesta: setCur() la
  // moltiplica per l'ascendente e deve corrispondere a ciò che si disegna.
  gTextSize = gCjkFont ? (n >= CJK_SIZE_DIV ? n / CJK_SIZE_DIV : 1) : n;
  gfx->setTextSize(gTextSize);
}

void setCur(int x, int y) {
  gfx->setCursor(x, y + gFontAscent * gTextSize);
}

// Imposta il font della lingua attiva. È l'UNICO punto che lo tocca: il resto del
// codice disegna uguale per tutte le lingue grazie a textW()/setCur().
//
// L'ascendente si misura invece di codificarlo: getTextBounds() restituisce y1 come
// scostamento del bordo superiore rispetto al cursore, negativo con i font U8g2 (che
// ancorano sulla linea di base). Così setCur() può continuare a trattare la Y come "in
// alto" qualunque sia il font, senza costanti magiche per font.
void applyLangFont() {
  gCjkFont = LANG_IS_CJK(gLang);
  if (!gCjkFont) {
    gfx->setFont();            // 5x7 classico, CP437: la Y è già il bordo superiore
    gfx->setUTF8Print(false);
    gFontAscent = 0;
    return;
  }
  gfx->setFont((gLang == LANG_KO) ? CJK_FONT_KO : CJK_FONT_JA);
  gfx->setUTF8Print(true);     // le stringhe CJK sono UTF-8 multibyte
  int16_t x1, y1;
  uint16_t w, h;
  uint8_t antes = gTextSize;
  gfx->setTextSize(1);
  gfx->getTextBounds("A", 0, 0, &x1, &y1, &w, &h);
  gFontAscent = -y1;           // y1 negativo: salire dalla linea di base
  gfx->setTextSize(antes);
}

// ---------- misura del testo (predisposto per i font CJK) ----------
//
// Il font classico di Arduino_GFX avanza di 6 px per carattere a dimensione 1, e la UI
// centrava con strlen(s)*6*n a occhio. Questo vale solo per un byte per carattere e
// larghezza fissa: con un font U8g2 per giapponese/coreano/cinese, né strlen() conta i
// caratteri (UTF-8 è multibyte) né tutti misurano uguale. Centralizzarlo qui è ciò che
// permetterà di aggiungere quelle lingue senza toccare ogni punto di disegno.
//
// Per il font classico restituisce ESATTAMENTE lo stesso conteggio di prima, quindi
// questa modifica non sposta un pixel nelle sei lingue attuali.
uint16_t textW(const char *s, uint8_t size) {
  if (!gCjkFont) return (uint16_t)strlen(s) * 6 * size;
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return w;
}

// x del cursore per lasciare il testo centrato in CX
int centerX(const char *s, uint8_t size) { return CX - textW(s, size) / 2; }

// nome del tipo T_* nella lingua attiva ("" per T_NESSUNO)
static_assert(S_TYPE_DRAGON - S_TYPE_NORMAL + 1 == TYPE_COUNT, "S_TYPE_*: uno per tipo T_*");
const char *typeName(uint8_t t) {
  return t < TYPE_COUNT ? T((StrId)(S_TYPE_NORMAL + t)) : "";
}

// Stampa rispettando il font attivo. Con il CJK ridisegna il testo spostato di 1 px: la
// unifont a scala ridotta lascia tratti da 1 px e appare tenue, e un font davvero
// grassetto a questa dimensione fonde i tratti e rovina i kana. Ridisegnare ingrassa il
// tratto senza deformare il glifo. Provato sulla scheda da usakomint, che ha
// confrontato le due opzioni in giapponese.
//
// La tastiera dei soprannomi è solo alfabeto (lasciata così di proposito), quindi qui
// non c'è mai CJK e non serve ingrassare nulla.
void printT(char c) { gfx->print(c); }

void printT(const char *s) {
  if (gCjkFont) {
    int16_t x = gfx->getCursorX(), y = gfx->getCursorY();
    gfx->print(s);
    gfx->setCursor(x + 1, y);
  }
  gfx->print(s);
}

// disegna una miniatura centrata nella sua cella; sil=true la dipinge in inchiostro
void drawThumb(const uint8_t *b, int x, int y, int s, bool sil) {
  uint8_t w = b[0], h = b[1], n = b[2];
  const uint8_t *pal = b + 3;
  const uint8_t *d = pal + n * 2;
  int ox = x + (GAL_CELL - w * s) / 2;
  int oy = y + (GAL_CELL - h * s) / 2;
  for (int r = 0; r < h; r++) {
    for (int c = 0; c < w; c++) {
      uint8_t idx = d[r * w + c];
      if (idx == 0xFF) continue;
      uint16_t col = sil ? INK_K : (uint16_t)(pal[idx * 2] | (pal[idx * 2 + 1] << 8));
      gfx->fillRect(ox + c * s, oy + r * s, s, s, col);
    }
  }
}

// ---------- Pokémon PMD: comportamento ----------

uint32_t pmdActTotalMs(const PmdAct &a) {
  uint32_t t = 0;
  for (uint8_t i = 0; i < a.frames; i++) t += a.ms[i];
  return t ? t : 100;
}

uint8_t pmdFrameAt(const PmdAct &a, uint32_t t, bool loop) {
  uint32_t total = pmdActTotalMs(a);
  if (!loop && t >= total) return a.frames - 1;
  t %= total;
  uint8_t i = 0;
  // guardia: mai più giri che frame, nel caso un .bin corrotto porti ms[]=0 nonostante
  // la sanificazione al caricamento (difesa in profondità)
  for (uint8_t guard = 0; guard < a.frames && t >= a.ms[i]; guard++) {
    t -= a.ms[i];
    i = (i + 1) % a.frames;
  }
  return i;
}

// disegna un'azione ancorata alla base (centro-x, suolo) e restituisce la sua scala
// disegna un'azione di un PmdMon concreto (m); drawPmdAct usa il globale pmd.
// flip = specchiato in orizzontale; le righe da clipY in giù non si disegnano (il
// Pokémon che sviene affonda sotto il suolo)
void drawPmdActM(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS,
                 bool flip, int clipY) {
  const PmdAct &a = m.acts[actId];
  if (!a.frames) return;
  uint8_t sBase = m.acts[PMD_IDLE].h ? 170 / m.acts[PMD_IDLE].h : 5;
  if (sBase < 2) sBase = 2;
  if (sBase > maxS) sBase = maxS;
  uint8_t s = sBase;
  while (s > 2 && a.h * s > 250) s--;  // azioni con frame grande (attacco)
  uint8_t fi = pmdFrameAt(a, t, loop);
  const uint8_t *fr = a.data + (uint32_t)fi * a.w * a.h;
  // ancorare ai piedi (a.base), non all'altezza della tela: così le azioni con padding
  // diverso (Hurt, Eat...) stanno tutte alla stessa altezza di suolo
  int x0 = cx - a.w * s / 2, y0 = groundY - (a.base ? a.base : a.h) * s;
  for (int r = 0; r < a.h; r++) {
    int y = y0 + r * s;
    if (y + s > clipY) break;
    const uint8_t *row = fr + r * a.w;
    for (int c = 0; c < a.w; c++) {
      uint8_t idx = row[flip ? a.w - 1 - c : c];
      if (idx == 0xFF) continue;
      gfx->fillRect(x0 + c * s, y, s, s, sil ? INK_K : m.pal[idx]);
    }
  }
}
void drawPmdAct(uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS) {
  drawPmdActM(pmd, actId, cx, groundY, t, loop, sil, maxS);
}

// disegna una mappa di n x n pixel scalata; silhouette=true la dipinge in inchiostro
void drawMap(const char *const *map, int n, int x, int y, int s, bool silhouette) {
  for (int r = 0; r < n; r++) {
    for (int c = 0; c < n; c++) {
      char ch = map[r][c];
      if (ch == '.') continue;
      gfx->fillRect(x + c * s, y + r * s, s, s, silhouette ? INK_K : spriteColor(ch));
    }
  }
}

// ---------- comandi dei giochi a inclinazione (vedi ui_common.h) ----------

static MotoFiltro gIncl;
static bool gInclSensore = false;
static uint32_t gInclZeroFino = 0, gInclUltimo = 0;
static float gInclX = 0, gInclY = 0;

void inclInizia() {
  gInclSensore = halMotoPresente();
  if (gInclSensore) halMotoModo(HAL_MOTO_GIOCO);
  gIncl.zeroInizia();
  gInclZeroFino = millis() + INCL_ZERO_MS;
  gInclUltimo = 0;
  gInclX = gInclY = 0;
}

void inclFine() {
  halMotoModo(HAL_MOTO_SPENTO);
  gInclZeroFino = 0;
}

bool inclAggiorna() {
  uint32_t now = millis();
  lastInteract = now;  // nessuno tocca lo schermo mentre si gioca: non deve attenuarsi
  float gx, gy, gz;
  bool g = gInclSensore && halMotoGravita(&gx, &gy, &gz);
  if (gInclZeroFino) {
    if (g) gIncl.zeroAggiungi(gx, gy, gz);
    if (timeLeft(gInclZeroFino)) return true;
    gInclZeroFino = 0;
    gIncl.zeroFissa();  // senza campioni: la scheda in piano
  }
  if (gInclSensore) {  // una lettura fallita lascia l'ultimo valore
    if (g) {
      gIncl.aggiorna(gx, gy, gz, gInclUltimo ? now - gInclUltimo : 0);
      gInclUltimo = now;
      gInclX = gIncl.x();
      gInclY = gIncl.y();
    }
  } else if (gDitoGiu) {
    motoJoystick(gDitoX - CX, gDitoY - CY, 150, 8, &gInclX, &gInclY);
  } else {
    gInclX = gInclY = 0;
  }
  return false;
}

float inclX() { return gInclX; }
float inclY() { return gInclY; }
bool inclSensore() { return gInclSensore; }
uint32_t inclZeroResta() { return timeLeft(gInclZeroFino); }
