// Impostazioni (ora, suono, lingua), pannello Audio e sottoschermata dei salvataggi con il
// suo dialogo.

#include "ui_common.h"

int clockH = 12, clockM = 0;  // ora in modifica
int clockY = 2026, clockMo = 1, clockD = 1;  // data in modifica
static bool clockData = false;  // scheda DATA (true) o ORA (false)
// l'elenco dei salvataggi (~2 KB) in PSRAM (inPsram)
struct ListaSalvataggi { SaveSdVoce v[SAVE_ELENCO_MAX]; };
static ListaSalvataggi *gLista = nullptr;
static SaveSdVoce *savesList() { return inPsram(gLista).v; }
uint8_t savesN = 0, savesPage = 0;
int16_t savesMsg = -1;      // esito dell'ultima azione (StrId), -1 nessuno
uint8_t saveDlgDa = 0;      // origine (SAVE_DA_*): decide il titolo
SaveEsito saveDlgEsito = SAVE_OK;
SaveIntestazione saveDlgH;
static bool savesOpen = false;  // sottoschermata dei salvataggi dentro SCR_CLOCK
static bool audioOpen = false;  // pannello Audio dentro SCR_CLOCK (volume e musica)

// ---------- impostazione di data e ora a schermo (scorri in basso) ----------
// L'utente imposta la sua ora LOCALE a occhio; il firmware la usa così com'è, quindi
// non serve gestire il fuso orario. La data serve dopo un RTC azzerato (scheda nuova o
// senza batteria riparte dall'1/1/2026): senza, l'avrebbe corretta solo la console di
// debug. Cambiarla non applica tempo offline (setClock), come cambiare l'ora.

#define CLK_TAB_Y 42      // schede ORA / DATA al posto del titolo
#define CLK_TAB_H 30
#define CLK_TAB_W 120
#define CLK_TAB_ORA_X 109
#define CLK_TAB_DATA_X 237
#define CLK_ANNO_MIN 2025
#define CLK_ANNO_MAX 2099
#define VER_PILL_Y 418  // la pillola della versione, in fondo
#define VER_PILL_H 28
// pulsanti della data: tre coppie -/+ (anno, mese, giorno), come la data dei salvataggi
static const int16_t CLK_DATA_X[6] = { 43, 105, 173, 235, 303, 365 };

static bool bisestile(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

static int giorniNelMese(int y, int m) {
  static const uint8_t G[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  return m == 2 && bisestile(y) ? 29 : G[m - 1];
}

// giorni dal 1970-01-01 (algoritmo days_from_civil di H. Hinnant)
static uint32_t giorniDaCivile(int y, int m, int d) {
  y -= m <= 2;
  int era = y / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (uint32_t)(era * 146097 + (int)doe - 719468);
}

void clockEnter() {
  uint32_t e = pet.lastSeenEpoch ? pet.lastSeenEpoch : halRtcEpoch();
  clockH = (e / 3600) % 24;
  clockM = (e / 60) % 60;
  time_t t = e;
  struct tm tm;
  gmtime_r(&t, &tm);
  clockY = tm.tm_year + 1900;
  if (clockY < CLK_ANNO_MIN) clockY = CLK_ANNO_MIN;
  if (clockY > CLK_ANNO_MAX) clockY = CLK_ANNO_MAX;
  clockMo = tm.tm_mon + 1;
  clockD = tm.tm_mday;
  clockData = false;
}

void clockExit() { savesOpen = audioOpen = false; }

// scorrere in verticale: da salvataggi e Audio si torna alle impostazioni, da lì alla
// principale
void clockSwipeV(int) {
  if (savesOpen || audioOpen) savesOpen = audioOpen = false;
  else goScreen(SCR_MAIN);
}

void applyClock() {
  uint32_t e = giorniDaCivile(clockY, clockMo, clockD) * 86400UL + (uint32_t)clockH * 3600 +
               (uint32_t)clockM * 60;
  halRtcSetEpoch(e);
  pet.setClock(e);
  goScreen(SCR_MAIN);
}

void drawClockBtn(int x, int y, const char *l) {
  gfx->fillRoundRect(x, y, 58, 58, 12, UI_WHITE);
  gfx->drawRoundRect(x, y, 58, 58, 12, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(4);
  setCur(x + 17, y + 15);
  printT(l);
}

// pillole di lingua centrate in y; riempie quella attiva
#define LANG_PILL_Y 296
#define LANG_PILL_H 30
#define LANG_PILL_X 336          // pillola di lingua (cicla LANG_COUNT al tocco)
#define LANG_PILL_W 96
#define SAVES_PILL_X 141         // pillola "Salvataggi", tra il suono e la lingua
#define SAVES_PILL_W 184
static const char *const LANG_CODES[LANG_COUNT] = { "ES", "EN", "FR", "DE", "IT", "PT", "JA", "KO" };

void renderClock() {
  if (savesOpen) {
    renderSaves();
    return;
  }
  if (audioOpen) {
    renderAudio();
    return;
  }
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  // schede ORA / DATA: quella attiva è piena
  const char *tab[2] = { T(S_CLK_TIME), T(S_CLK_DATE) };
  for (int i = 0; i < 2; i++) {
    int x = i ? CLK_TAB_DATA_X : CLK_TAB_ORA_X;
    bool attiva = clockData == (i == 1);
    gfx->fillRoundRect(x, CLK_TAB_Y, CLK_TAB_W, CLK_TAB_H, 8, attiva ? UI_INK : UI_WHITE);
    gfx->drawRoundRect(x, CLK_TAB_Y, CLK_TAB_W, CLK_TAB_H, 8, UI_INK);
    gfx->setTextColor(attiva ? UI_BG_DAY : UI_INK);
    setSize(2);
    setCur(x + (CLK_TAB_W - textW(tab[i], 2)) / 2, CLK_TAB_Y + 8);
    printT(tab[i]);
  }

  gfx->setTextColor(UI_INK);
  if (clockData) {
    char d[12];
    snprintf(d, sizeof(d), "%04d-%02d-%02d", clockY, clockMo, clockD);
    setSize(5);
    setCur(centerX(d, 5), 116);
    printT(d);
    for (int i = 0; i < 6; i++) drawClockBtn(CLK_DATA_X[i], 190, i % 2 ? "+" : "-");
    const char *et[3] = { T(S_YEAR), T(S_MONTH), T(S_DAY) };
    setSize(2);
    gfx->setTextColor(UI_HINT);
    for (int i = 0; i < 3; i++) {
      int cx = CLK_DATA_X[2 * i] + 61;  // centro della coppia -/+
      setCur(cx - textW(et[i], 2) / 2, 256);
      printT(et[i]);
    }
  } else {
    char t[8];
    snprintf(t, sizeof(t), "%02d:%02d", clockH, clockM);
    setSize(7);
    setCur(CX - 105, 108);
    printT(t);

    drawClockBtn(104, 190, "-");  // ora -
    drawClockBtn(170, 190, "+");  // ora +
    drawClockBtn(252, 190, "-");  // min -
    drawClockBtn(318, 190, "+");  // min +
    setSize(2);
    gfx->setTextColor(UI_HINT);
    setCur(120, 256);
    printT(T(S_HOUR));
    setCur(276, 256);
    printT(T(S_MIN));
  }

  // il volume (a sinistra della riga della lingua): apre il pannello Audio
  uint8_t vol = halVolume();
  char sl[16];
  if (vol) snprintf(sl, sizeof(sl), T(S_VOL_FMT), (unsigned)vol);
  else strlcpy(sl, T(S_MUTE), sizeof(sl));
  gfx->fillRoundRect(34, LANG_PILL_Y, 96, LANG_PILL_H, 8, vol ? UI_BAR_OK : UI_WHITE);
  gfx->drawRoundRect(34, LANG_PILL_Y, 96, LANG_PILL_H, 8, UI_INK);
  gfx->setTextColor(vol ? UI_BG_DAY : UI_INK);
  setSize(2);
  setCur(34 + (96 - textW(sl, 2)) / 2, LANG_PILL_Y + 8);
  printT(sl);

  // apre i salvataggi (tra il suono e la lingua)
  gfx->fillRoundRect(SAVES_PILL_X, LANG_PILL_Y, SAVES_PILL_W, LANG_PILL_H, 8, UI_WHITE);
  gfx->drawRoundRect(SAVES_PILL_X, LANG_PILL_Y, SAVES_PILL_W, LANG_PILL_H, 8, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(SAVES_PILL_X + (SAVES_PILL_W - textW(T(S_SAVES), 2)) / 2, LANG_PILL_Y + 8);
  printT(T(S_SAVES));

  // selettore di lingua: una pillola che cicla le 6 lingue al tocco
  gfx->fillRoundRect(LANG_PILL_X, LANG_PILL_Y, LANG_PILL_W, LANG_PILL_H, 8, UI_WHITE);
  gfx->drawRoundRect(LANG_PILL_X, LANG_PILL_Y, LANG_PILL_W, LANG_PILL_H, 8, UI_INK);
  char lp[10];
  snprintf(lp, sizeof(lp), "%s >", LANG_CODES[gLang]);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(LANG_PILL_X + (LANG_PILL_W - textW(lp, 2)) / 2, LANG_PILL_Y + 8);
  printT(lp);

  gfx->fillRoundRect(133, 340, 200, 48, 14, UI_BAR_OK);
  gfx->setTextColor(UI_BG_DAY);
  setSize(3);
  setCur(CX - 18, 352);
  printT("OK");

  gfx->setTextColor(UI_HINT);
  setSize(2);
  setCur(centerX(T(S_CLOCK_CANCEL), 2), 396);
  printT(T(S_CLOCK_CANCEL));

  // versione del firmware, in fondo: è anche il pulsante dell'aggiornamento via WiFi
  // (SCR_AGGIORNA), con due frecce in cerchio
  char ver[20];
  snprintf(ver, sizeof(ver), "v%s", FW_VERSION);
  int w = textW(ver, 2) + 46;
  int x = CX - w / 2;
  gfx->fillRoundRect(x, VER_PILL_Y, w, VER_PILL_H, 10, UI_WHITE);
  gfx->drawRoundRect(x, VER_PILL_Y, w, VER_PILL_H, 10, UI_INK);
  gfx->setTextColor(UI_INK);
  setCur(x + 10, VER_PILL_Y + 6);
  printT(ver);
  int ix = x + w - 15, iy = VER_PILL_Y + VER_PILL_H / 2;
  gfx->drawCircle(ix, iy, 6, UI_INK);
  gfx->fillRect(ix - 7, iy - 1, 4, 3, UI_WHITE);   // due interruzioni nel cerchio...
  gfx->fillRect(ix + 4, iy - 1, 4, 3, UI_WHITE);
  gfx->fillTriangle(ix - 9, iy - 2, ix - 3, iy - 2, ix - 6, iy + 2, UI_INK);  // ...e le punte
  gfx->fillTriangle(ix + 3, iy + 2, ix + 9, iy + 2, ix + 6, iy - 2, UI_INK);
  halFlush();
}

void clockTap(int16_t x, int16_t y) {
  if (savesOpen) {
    savesTap(x, y);
    return;
  }
  if (audioOpen) {
    audioTap(x, y);
    return;
  }
  if (y >= CLK_TAB_Y && y <= CLK_TAB_Y + CLK_TAB_H) {  // schede ORA / DATA
    bool data = x >= CLK_TAB_DATA_X && x <= CLK_TAB_DATA_X + CLK_TAB_W;
    if (data || (x >= CLK_TAB_ORA_X && x <= CLK_TAB_ORA_X + CLK_TAB_W)) {
      if (clockData != data) halSfx(SFX_TAP);
      clockData = data;
    }
    return;
  }
  if (clockData && y >= 190 && y <= 248) {  // anno, mese, giorno -/+
    for (int i = 0; i < 6; i++) {
      if (x < CLK_DATA_X[i] || x >= CLK_DATA_X[i] + 58) continue;
      int passo = i % 2 ? 1 : -1;
      if (i < 2) {
        clockY += passo;
        if (clockY < CLK_ANNO_MIN) clockY = CLK_ANNO_MAX;
        if (clockY > CLK_ANNO_MAX) clockY = CLK_ANNO_MIN;
      } else if (i < 4) {
        clockMo = (clockMo + 11 + passo) % 12 + 1;
      } else {
        int n = giorniNelMese(clockY, clockMo);
        clockD = (clockD + n - 1 + passo) % n + 1;
      }
      // 31 marzo -> febbraio: il giorno scende all'ultimo del mese
      int n = giorniNelMese(clockY, clockMo);
      if (clockD > n) clockD = n;
    }
    return;
  }
  if (y >= 190 && y <= 248) {  // riga di pulsanti +/-
    if (x >= 104 && x < 162) clockH = (clockH + 23) % 24;
    else if (x >= 170 && x < 228) clockH = (clockH + 1) % 24;
    else if (x >= 252 && x < 310) clockM = (clockM + 59) % 60;
    else if (x >= 318 && x < 376) clockM = (clockM + 1) % 60;
    return;
  }
  if (y >= LANG_PILL_Y && y <= LANG_PILL_Y + LANG_PILL_H) {
    if (x >= 34 && x < 130) {                  // volume: apre il pannello Audio
      halSfx(SFX_TAP);
      openAudio();
      return;
    }
    if (x >= LANG_PILL_X && x < LANG_PILL_X + LANG_PILL_W) {  // cicla la lingua
      setLang((Lang)((gLang + 1) % LANG_COUNT));
      applyLangFont();  // il font cambia con la lingua
      halSfx(SFX_TAP);
      return;
    }
    if (x >= SAVES_PILL_X && x < SAVES_PILL_X + SAVES_PILL_W) {
      halSfx(SFX_TAP);
      openSaves();
      return;
    }
  }
  if (y >= 340 && y <= 388 && x >= 133 && x <= 333) { applyClock(); return; }
  if (y >= VER_PILL_Y - 4 && x >= CX - 90 && x <= CX + 90) {  // la versione: aggiornamento
    halSfx(SFX_TAP);
    goScreen(SCR_AGGIORNA);
  }
}

// ---------- pannello Audio: volume generale e interruttori della musica e del movimento ----------
// I cambiamenti valgono subito, come lingua e suono prima, senza OK. A ogni tocco di -/+
// un SFX_TAP suona al volume nuovo. MOVIMENTO sta qui perché la pagina
// dell'orologio è piena: accende scossone e colpetti sulla principale.

#define AUDIO_HDR_Y 80       // sopra: intestazione, il tocco chiude
#define AUDIO_BTN_Y 118      // - e + grandi, ai lati della barra
#define AUDIO_BTN 80
#define AUDIO_MENO_X 50
#define AUDIO_PIU_X 336
#define AUDIO_BAR_X 144      // 10 tacche da 14 px, una ogni 18
#define AUDIO_BAR_Y 132
#define AUDIO_BAR_H 44
#define AUDIO_MUS_X 113      // pillola MUSICA SI/NO
#define AUDIO_MUS_Y 250
#define AUDIO_MUS_W 240
#define AUDIO_MUS_H 50
#define AUDIO_MOT_Y 310      // pillola MOVIMENTO SI/NO, stesse x e misure
#define AUDIO_BACK_Y 380     // sotto: tornare alle impostazioni

void openAudio() { audioOpen = true; }

void renderAudio() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(T(S_AUDIO), 3), 36);
  printT(T(S_AUDIO));

  uint8_t vol = halVolume();
  for (int b = 0; b < 2; b++) {
    int x = b ? AUDIO_PIU_X : AUDIO_MENO_X;
    gfx->fillRoundRect(x, AUDIO_BTN_Y, AUDIO_BTN, AUDIO_BTN, 16, UI_WHITE);
    gfx->drawRoundRect(x, AUDIO_BTN_Y, AUDIO_BTN, AUDIO_BTN, 16, UI_INK);
    gfx->setTextColor(UI_INK);
    setSize(6);
    setCur(x + (AUDIO_BTN - 30) / 2, AUDIO_BTN_Y + (AUDIO_BTN - 42) / 2);
    printT(b ? "+" : "-");
  }
  // le tacche crescono come l'icona del volume; piene fino al volume attuale
  for (int i = 0; i < 10; i++) {
    int h = AUDIO_BAR_H * (i + 3) / 12;
    int x = AUDIO_BAR_X + i * 18, y = AUDIO_BAR_Y + AUDIO_BAR_H - h;
    if (i < vol) gfx->fillRect(x, y, 14, h, UI_BAR_OK);
    else gfx->drawRect(x, y, 14, h, UI_INK);
  }
  char n[16];
  if (vol) snprintf(n, sizeof(n), "%u", (unsigned)vol);
  else strlcpy(n, T(S_MUTE), sizeof(n));
  gfx->setTextColor(vol ? UI_INK : UI_BAR_BAD);
  setSize(3);
  setCur(centerX(n, 3), AUDIO_BAR_Y + AUDIO_BAR_H + 12);
  printT(n);

  bool mus = halMusicaAccesa();
  const char *ml = mus ? T(S_MUSIC_ON) : T(S_MUSIC_OFF);
  gfx->fillRoundRect(AUDIO_MUS_X, AUDIO_MUS_Y, AUDIO_MUS_W, AUDIO_MUS_H, 12,
                     mus ? UI_BAR_OK : UI_WHITE);
  gfx->drawRoundRect(AUDIO_MUS_X, AUDIO_MUS_Y, AUDIO_MUS_W, AUDIO_MUS_H, 12, UI_INK);
  gfx->setTextColor(mus ? UI_BG_DAY : UI_INK);
  setSize(2);
  setCur(AUDIO_MUS_X + (AUDIO_MUS_W - textW(ml, 2)) / 2, AUDIO_MUS_Y + 18);
  printT(ml);

  bool mot = pet.movimento();
  const char *mo = mot ? T(S_MOTION_ON) : T(S_MOTION_OFF);
  gfx->fillRoundRect(AUDIO_MUS_X, AUDIO_MOT_Y, AUDIO_MUS_W, AUDIO_MUS_H, 12,
                     mot ? UI_BAR_OK : UI_WHITE);
  gfx->drawRoundRect(AUDIO_MUS_X, AUDIO_MOT_Y, AUDIO_MUS_W, AUDIO_MUS_H, 12, UI_INK);
  gfx->setTextColor(mot ? UI_BG_DAY : UI_INK);
  setSize(2);
  setCur(AUDIO_MUS_X + (AUDIO_MUS_W - textW(mo, 2)) / 2, AUDIO_MOT_Y + 18);
  printT(mo);

  gfx->setTextColor(UI_HINT);
  setSize(2);
  setCur(centerX(T(S_BACK), 2), AUDIO_BACK_Y + 20);
  printT(T(S_BACK));
  halFlush();
}

void audioTap(int16_t x, int16_t y) {
  if (y < AUDIO_HDR_Y || y >= AUDIO_BACK_Y) {
    audioOpen = false;
    return;
  }
  if (y >= AUDIO_BTN_Y && y < AUDIO_BTN_Y + AUDIO_BTN) {
    uint8_t vol = halVolume();
    if (x >= AUDIO_MENO_X && x < AUDIO_MENO_X + AUDIO_BTN) {
      if (vol > 0) halSetVolume(vol - 1);
      halSfx(SFX_TAP);  // al volume nuovo (a 0 tace)
    } else if (x >= AUDIO_PIU_X && x < AUDIO_PIU_X + AUDIO_BTN) {
      if (vol < 10) halSetVolume(vol + 1);
      halSfx(SFX_TAP);
    }
    return;
  }
  if (y >= AUDIO_MUS_Y && y < AUDIO_MUS_Y + AUDIO_MUS_H && x >= AUDIO_MUS_X &&
      x < AUDIO_MUS_X + AUDIO_MUS_W) {
    halSetMusicaAccesa(!halMusicaAccesa());
    halSfx(SFX_TAP);
  }
  if (y >= AUDIO_MOT_Y && y < AUDIO_MOT_Y + AUDIO_MUS_H && x >= AUDIO_MUS_X &&
      x < AUDIO_MUS_X + AUDIO_MUS_W) {
    pet.setMovimento(!pet.movimento());
    halSfx(SFX_TAP);
  }
}

// ---------- salvataggi: elenco, copia manuale, ripristino e importazione ----------
// Il file intero e la verifica stanno in savesd (saveSdPreparaFile / saveSdConferma);
// qui ci sono solo la sottoschermata delle impostazioni e il dialogo, che ha la
// precedenza su tutto, anche sulla scelta dell'iniziale.

#define SAVES_BTN_Y 80   // "Crea salvataggio ora"
#define SAVES_BTN_H 44
#define SAVES_ROW_Y 154
#define SAVES_ROW_H 52
#define SAVES_ROWS 4
#define SAVES_ARR_Y 228  // frecce di pagina, ai lati dell'elenco
#define SAVES_BACK_Y 380 // sotto: tornare alle impostazioni
#define SAVE_DLG_BTN_Y 336
#define THUMB_HALF 40     // metà di GAL_CELL (più sotto): drawThumb centra in quella cella

void openSaves() {
  savesOpen = true;
  savesPage = 0;
  savesMsg = -1;
  savesN = saveSdElenco(savesList(), SAVE_ELENCO_MAX);
}

// data e ora dell'epoch, che è già l'ora locale (vedi applyClock)
void fmtEpoch(char *out, size_t n, uint32_t e) {
  if (!e) {
    strlcpy(out, "----------  --:--", n);
    return;
  }
  time_t t = e;
  struct tm tm;
  gmtime_r(&t, &tm);
  snprintf(out, n, "%04d-%02d-%02d %02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1,
           tm.tm_mday, tm.tm_hour, tm.tm_min);
}

const char *saveMotivoT(uint8_t m) {
  return m <= SAVE_PRIMA_IMPORT ? T((StrId)(S_MOT_MANUAL + m)) : "";
}

// soprannome, nome della specie o "uovo"
const char *saveNomeT(const SaveRiepilogo &r) {
  if (r.nome[0]) return r.nome;
  if (r.specie >= 1 && r.specie <= DEX_COUNT) return dexName(r.specie);
  return T(S_EGG_HDR);
}

StrId saveEsitoT(SaveEsito e) {
  switch (e) {
    case SAVE_ERR_FORMATO:
    case SAVE_ERR_SCHEMA: return S_SAVE_ERR_FW;
    case SAVE_ERR_COPIA: return S_SAVE_ERR_COPY;
    case SAVE_ERR_SCRITTURA: return S_SAVE_FAIL;
    default: return S_SAVE_ERR_FILE;
  }
}

void renderSaves() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(T(S_SAVES), 3), 36);
  printT(T(S_SAVES));

  gfx->fillRoundRect(83, SAVES_BTN_Y, 300, SAVES_BTN_H, 12, sdReady ? UI_BAR_OK : UI_TRACK);
  gfx->setTextColor(UI_BG_DAY);
  setSize(2);
  setCur(centerX(T(S_SAVE_NOW), 2), SAVES_BTN_Y + 14);
  printT(T(S_SAVE_NOW));

  // una riga di stato: esito dell'ultima copia, oppure SD assente o elenco vuoto
  const char *st = nullptr;
  if (savesMsg >= 0) st = T((StrId)savesMsg);
  else if (!sdReady) st = T(S_SAVE_NO_SD);
  else if (!savesN) st = T(S_SAVE_EMPTY);
  if (st) {
    gfx->setTextColor(savesMsg == S_SAVE_DONE ? UI_BAR_OK : UI_BAR_BAD);
    setCur(centerX(st, 2), 132);
    printT(st);
  }

  for (int i = 0; i < SAVES_ROWS; i++) {
    int k = savesPage * SAVES_ROWS + i;
    if (k >= savesN) break;
    const SaveIntestazione &h = savesList()[k].h;
    int ry = SAVES_ROW_Y + i * SAVES_ROW_H;
    gfx->fillRoundRect(70, ry, 326, SAVES_ROW_H - 6, 10, UI_WHITE);
    gfx->drawRoundRect(70, ry, 326, SAVES_ROW_H - 6, 10, UI_INK);
    const uint8_t *th = h.riepilogo.specie >= 1 ? thumbs.get(h.riepilogo.specie) : nullptr;
    if (th) drawThumb(th, 96 - THUMB_HALF, ry + 23 - THUMB_HALF, 1, false);
    char d[24];
    fmtEpoch(d, sizeof(d), h.epoch);
    gfx->setTextColor(UI_INK);
    setSize(2);
    setCur(126, ry + 5);
    printT(d);
    char lv[16];
    snprintf(lv, sizeof(lv), T(S_LVL_FMT), h.riepilogo.livello);
    char l2[48];
    snprintf(l2, sizeof(l2), "%s%s  %s", h.riepilogo.shiny ? "*" : "",
             h.riepilogo.specie < 0 ? T(S_EGG_HDR) : lv, saveMotivoT(h.motivo));
    uint8_t sz = textW(l2, 2) <= 262 ? 2 : 1;
    gfx->setTextColor(UI_HINT);
    setSize(sz);
    setCur(126, ry + (sz == 2 ? 25 : 29));
    printT(l2);
  }

  if (savesN > SAVES_ROWS) {  // frecce e numero di pagina
    char pg[8];
    snprintf(pg, sizeof(pg), "%u/%u", savesPage + 1, (savesN + SAVES_ROWS - 1) / SAVES_ROWS);
    gfx->setTextColor(UI_INK);
    setSize(3);
    setCur(36, SAVES_ARR_Y + 12);
    printT("<");
    setCur(412, SAVES_ARR_Y + 12);
    printT(">");
    setSize(2);
    setCur(centerX(pg, 2), SAVES_ROW_Y + SAVES_ROWS * SAVES_ROW_H);
    printT(pg);
  }

  gfx->setTextColor(UI_HINT);
  setSize(2);
  setCur(centerX(T(S_BACK), 2), SAVES_BACK_Y + 20);
  printT(T(S_BACK));
  halFlush();
}

void savesTap(int16_t x, int16_t y) {
  if (y >= SAVES_BTN_Y && y <= SAVES_BTN_Y + SAVES_BTN_H && x >= 83 && x <= 383) {
    halSfx(SFX_TAP);
    if (!sdReady) savesMsg = S_SAVE_NO_SD;
    else savesMsg = saveSdCopia(pet, SAVE_MANUALE) ? S_SAVE_DONE : S_SAVE_FAIL;
    savesN = saveSdElenco(savesList(), SAVE_ELENCO_MAX);
    savesPage = 0;
    return;
  }
  uint8_t pagine = (savesN + SAVES_ROWS - 1) / SAVES_ROWS;
  if (pagine > 1 && y >= SAVES_ARR_Y && y <= SAVES_ARR_Y + 48 && (x < 66 || x > 400)) {
    savesPage = (savesPage + (x < 66 ? pagine - 1 : 1)) % pagine;
    return;
  }
  for (int i = 0; i < SAVES_ROWS; i++) {
    int k = savesPage * SAVES_ROWS + i;
    int ry = SAVES_ROW_Y + i * SAVES_ROW_H;
    if (k < savesN && x >= 70 && x <= 396 && y >= ry && y < ry + SAVES_ROW_H) {
      halSfx(SFX_TAP);
      saveDlgApri(saveSdPreparaFile(savesList()[k].nome, SAVE_DA_ELENCO), SAVE_DA_ELENCO);
      return;
    }
  }
  if (y >= SAVES_BACK_Y) savesOpen = false;
}

// Un file valido apre la conferma (con i dati del riepilogo), uno non valido il motivo.
void saveDlgApri(SaveEsito r, uint8_t da) {
  saveDlgDa = da;
  saveDlgEsito = r;
  saveDlg = (r == SAVE_OK && saveSdAttesa(&saveDlgH) == da) ? 1 : 2;
  screenOff = false;  // il dialogo si deve vedere (IMPORT arriva anche a schermo spento)
  lastInteract = millis();
}

// Proposte all'avvio, una alla volta: prima /save/importa.pgsave, poi, se la partita è
// nuova (scelta dell'iniziale) e la SD ha delle copie, la più recente che si verifica.
void proponiAvvio() {
  static bool importaVisto = false, primoVisto = false;
  if (saveDlg || saveSdAttesa() != SAVE_DA_NESSUNO) return;
  if (!importaVisto && saveSdImportaPresente()) {
    importaVisto = true;
    SaveEsito r = saveSdPreparaFile(SAVE_FILE_IMPORTA, SAVE_DA_FILE);
    saveSdArchiviaImporta();  // subito: non si ripropone e resta nell'elenco
    saveDlgApri(r, SAVE_DA_FILE);
    return;
  }
  if (primoVisto || !pet.awaitingStarter()) return;
  primoVisto = true;
  uint8_t n = saveSdElenco(savesList(), SAVE_ELENCO_MAX);
  // prima le copie non datate nel futuro (prove con TIME avanti), poi, se non ce n'è
  // nessuna (RTC rimasto indietro), anche quelle
  uint32_t adesso = halRtcEpoch();
  for (uint8_t giro = 0; giro < 2; giro++)
    for (uint8_t i = 0; i < n; i++) {
      if (saveDalFuturo(savesList()[i].nome, adesso) != (giro == 1)) continue;
      if (saveSdPreparaFile(savesList()[i].nome, SAVE_DA_PRIMO_AVVIO) == SAVE_OK) {
        saveDlgApri(SAVE_OK, SAVE_DA_PRIMO_AVVIO);
        return;
      }
    }
}

void renderSaveDialog() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  const char *tit = saveDlgDa == SAVE_DA_PRIMO_AVVIO ? T(S_SAVE_FOUND)
                    : saveDlgDa == SAVE_DA_FILE      ? SAVE_FILE_IMPORTA
                    : saveDlgDa == SAVE_DA_SERIALE   ? "IMPORT (USB)"
                                                     : T(S_SAVES);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(tit, 2), 60);
  printT(tit);

  if (saveDlg == 3) {  // prima del riavvio
    setCur(centerX(T(S_SAVE_RESTORING), 2), CY - 8);
    printT(T(S_SAVE_RESTORING));
  } else if (saveDlg == 2) {  // motivo del rifiuto; un tocco chiude
    const char *m = T(saveEsitoT(saveDlgEsito));
    gfx->setTextColor(UI_BAR_BAD);
    setCur(centerX(m, 2), 196);
    printT(m);
    gfx->setTextColor(UI_HINT);
    setCur(centerX(saveEsitoNome(saveDlgEsito), 2), 228);
    printT(saveEsitoNome(saveDlgEsito));
    gfx->fillRoundRect(183, SAVE_DLG_BTN_Y, 100, 52, 12, UI_BAR_OK);
    gfx->setTextColor(UI_WHITE);
    setCur(CX - 12, SAVE_DLG_BTN_Y + 18);
    printT("OK");
  } else {  // data, motivo, miniatura, nome, livello, Pokédex; poi SI / NO
    const SaveRiepilogo &r = saveDlgH.riepilogo;
    char d[24];
    fmtEpoch(d, sizeof(d), saveDlgH.epoch);
    setCur(centerX(d, 2), 92);
    printT(d);
    gfx->setTextColor(UI_HINT);
    setCur(centerX(saveMotivoT(saveDlgH.motivo), 2), 114);
    printT(saveMotivoT(saveDlgH.motivo));
    const uint8_t *th = r.specie >= 1 ? thumbs.get(r.specie) : nullptr;
    if (th) drawThumb(th, CX - THUMB_HALF, 138, 2, false);
    const char *nome = saveNomeT(r);
    gfx->setTextColor(UI_INK);
    setSize(3);
    setCur(centerX(nome, 3), 222);
    printT(nome);
    char lv[16];
    snprintf(lv, sizeof(lv), T(S_LVL_FMT), r.livello);
    char reg[32];
    snprintf(reg, sizeof(reg), T(S_POKEDEX_FMT), r.pokedex);
    char riga[52];
    snprintf(riga, sizeof(riga), "%s%s   %s", r.shiny ? "*" : "", r.specie < 0 ? "" : lv, reg);
    setSize(2);
    setCur(centerX(riga, 2), 258);
    printT(riga);
    const char *q = (saveDlgDa == SAVE_DA_FILE || saveDlgDa == SAVE_DA_SERIALE)
                        ? T(S_SAVE_IMPORT_Q) : T(S_SAVE_RESTORE_Q);
    setCur(centerX(q, 2), 296);
    printT(q);
    gfx->fillRoundRect(118, SAVE_DLG_BTN_Y, 100, 52, 12, UI_BAR_OK);
    gfx->setTextColor(UI_WHITE);
    setCur(118 + (100 - textW(T(S_YES), 2)) / 2, SAVE_DLG_BTN_Y + 18);
    printT(T(S_YES));
    gfx->fillRoundRect(248, SAVE_DLG_BTN_Y, 100, 52, 12, UI_BAR_BAD);
    setCur(248 + (100 - textW(T(S_NO), 2)) / 2, SAVE_DLG_BTN_Y + 18);
    printT(T(S_NO));
  }
  halFlush();
}

// chiude il dialogo; dopo importa.pgsave può toccare alla proposta di ripristino
void saveDlgChiudi() {
  saveDlg = 0;
  if (savesOpen) savesN = saveSdElenco(savesList(), SAVE_ELENCO_MAX);
  proponiAvvio();
}

// nel loop(): un IMPORT dalla seriale non confermato in tempo chiude il suo dialogo
void saveDlgScadenza() {
  if (saveSdScaduta() && saveDlg == 1 && saveDlgDa == SAVE_DA_SERIALE) saveDlgChiudi();
}

void saveDlgTap(int16_t x, int16_t y) {
  if (saveDlg == 2) {
    saveDlgChiudi();
    return;
  }
  if (saveDlg != 1 || y < SAVE_DLG_BTN_Y || y > SAVE_DLG_BTN_Y + 52) return;
  if (x >= 248 && x <= 348) {  // NO: le copie restano, si gioca come prima
    saveSdAnnulla();
    saveDlgChiudi();
  } else if (x >= 118 && x <= 218) {  // SI: copia di sicurezza, scrittura, riavvio
    halSfx(SFX_TAP);
    saveDlg = 3;
    renderSaveDialog();
    SaveEsito r = saveSdConferma(pet);
    if (r == SAVE_OK) {
      // niente pet.save(): il Pet in RAM sovrascriverebbe lo stato importato
      delay(300);
      halRestart();
    }
    saveDlgApri(r, saveDlgDa);
  }
}
