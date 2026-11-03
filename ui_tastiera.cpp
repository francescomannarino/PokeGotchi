// Tastiera completa: tutto l'ASCII stampabile, per i testi liberi. Oggi serve alla password
// del WiFi (ui_aggiorna.cpp); i nomi dei Pokémon e dell'allenatore hanno la loro, solo
// maiuscole (ui_card.cpp). Non è una schermata: la disegna e ne riceve i tocchi chi la usa.
//
// Una riga di cifre fissa e tre pagine di tre righe (minuscole, maiuscole, simboli), tutte
// dentro il cerchio di 466 px; sotto, cambio di pagina, spazio, cancella e OK. Toccare il
// campo mostra o nasconde il testo segreto. La fascia in basso annulla.

#include "ui_common.h"

#define TS_X 63          // tasti: 10 colonne da 34 px
#define TS_W 34
#define TS_Y 146         // 4 righe da 44 px (cifre + 3 della pagina)
#define TS_H 44
#define TS_RIGA5_Y 330   // pagina, spazio, cancella, OK
#define TS_RIGA5_H 44
#define TS_CAMPO_Y 88
#define TS_CAMPO_H 42
#define TS_ANNULLA_Y 388 // da qui in giù: annulla
#define TS_VISIBILI 25   // caratteri del campo

static const char *const PAGINE[3][3] = {
  { "qwertyuiop", "asdfghjkl-", "zxcvbnm_.@" },
  { "QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM_.@" },
  { "!\"#$%&'()*", "+,/:;<=>?[", "\\]^`{|}~-_" },
};
static const char *const CIFRE = "1234567890";
static const char *const ETICHETTA_PAGINA[3] = { "ABC", "#+=", "abc" };  // la prossima

// pulsanti della quinta riga: x e larghezza
static const int16_t R5_X[4] = { 63, 137, 241, 305 };
static const int16_t R5_W[4] = { 70, 100, 60, 98 };

static char gTitolo[40];
static char *gBuf = nullptr;
static size_t gMax = 0, gLen = 0;
static bool gSegreta = false, gMostra = false;
static uint8_t gPagina = 0;

void tastieraApri(const char *titolo, char *buf, size_t max, bool segreta) {
  testoStampabile(gTitolo, sizeof(gTitolo), titolo);
  gBuf = buf;
  gMax = max;
  gLen = strnlen(buf, max - 1);
  buf[gLen] = 0;
  gSegreta = segreta;
  gMostra = false;
  gPagina = 0;
}

static void tasto(int x, int y, int w, int h, const char *etichetta, uint16_t fondo) {
  gfx->fillRoundRect(x, y, w, h, 6, fondo);
  gfx->drawRoundRect(x, y, w, h, 6, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(x + (w - textW(etichetta, 2)) / 2, y + h / 2 - 8);
  printT(etichetta);
}

void tastieraDisegna() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(gTitolo, 2), 52);
  printT(gTitolo);

  // il campo: puntini per il testo segreto; se non ci sta, la fine con "<" davanti
  gfx->fillRoundRect(TS_X, TS_CAMPO_Y, 10 * TS_W, TS_CAMPO_H, 8, UI_WHITE);
  gfx->drawRoundRect(TS_X, TS_CAMPO_Y, 10 * TS_W, TS_CAMPO_H, 8, UI_INK);
  char vista[TS_VISIBILI + 2];
  size_t da = gLen > TS_VISIBILI ? gLen - (TS_VISIBILI - 1) : 0;
  size_t n = 0;
  if (da) vista[n++] = '<';
  for (size_t i = da; i < gLen; i++) vista[n++] = gSegreta && !gMostra ? '*' : gBuf[i];
  if (n == 0) vista[n++] = '_';
  vista[n] = 0;
  setSize(2);
  setCur(TS_X + 10, TS_CAMPO_Y + 13);
  printT(vista);
  if (gSegreta) {  // l'interruttore mostra/nascondi, a destra nel campo
    const char *occhio = gMostra ? "***" : "abc";
    setCur(TS_X + 10 * TS_W - 8 - textW(occhio, 2), TS_CAMPO_Y + 13);
    gfx->setTextColor(UI_HINT);
    printT(occhio);
  }

  char c[2] = { 0, 0 };
  for (int r = 0; r < 4; r++)
    for (int k = 0; k < 10; k++) {
      c[0] = r == 0 ? CIFRE[k] : PAGINE[gPagina][r - 1][k];
      tasto(TS_X + k * TS_W, TS_Y + r * TS_H, TS_W - 3, TS_H - 4, c, UI_WHITE);
    }
  const char *r5[4] = { ETICHETTA_PAGINA[gPagina], "___", "<-", "OK" };
  for (int i = 0; i < 4; i++)
    tasto(R5_X[i], TS_RIGA5_Y, R5_W[i], TS_RIGA5_H - 4, r5[i],
          i == 3 ? UI_BAR_OK : (i == 0 || i == 2 ? UI_BAR_WARN : UI_WHITE));

  gfx->setTextColor(UI_HINT);
  setSize(2);
  setCur(centerX(T(S_BACK), 2), TS_ANNULLA_Y + 14);
  printT(T(S_BACK));
}

static void aggiungi(char c) {
  if (gLen + 1 >= gMax) {
    halSfx(SFX_DENY);
    return;
  }
  gBuf[gLen++] = c;
  gBuf[gLen] = 0;
}

int8_t tastieraTap(int16_t x, int16_t y) {
  if (y >= TS_ANNULLA_Y) return TAST_ANNULLA;
  if (y >= TS_CAMPO_Y && y < TS_CAMPO_Y + TS_CAMPO_H) {
    if (gSegreta) gMostra = !gMostra;
    return TAST_NIENTE;
  }
  if (y >= TS_RIGA5_Y && y < TS_RIGA5_Y + TS_RIGA5_H) {
    for (int i = 0; i < 4; i++) {
      if (x < R5_X[i] || x >= R5_X[i] + R5_W[i]) continue;
      if (i == 0) gPagina = (gPagina + 1) % 3;
      else if (i == 1) aggiungi(' ');
      else if (i == 2 && gLen) gBuf[--gLen] = 0;
      else if (i == 3) return TAST_OK;
    }
    return TAST_NIENTE;
  }
  // filtra prima di dividere (vedi keyboardTap)
  if (x < TS_X || y < TS_Y) return TAST_NIENTE;
  int k = (x - TS_X) / TS_W, r = (y - TS_Y) / TS_H;
  if (k >= 10 || r >= 4) return TAST_NIENTE;
  aggiungi(r == 0 ? CIFRE[k] : PAGINE[gPagina][r - 1][k]);
  return TAST_NIENTE;
}

// copia `da` in `out` tenendo solo l'ASCII stampabile: il font 5x7 non ha il resto, e un
// SSID può contenere qualunque byte
void testoStampabile(char *out, size_t max, const char *da) {
  size_t n = 0;
  for (; *da && n + 1 < max; da++) out[n++] = (*da >= 32 && *da < 127) ? *da : '?';
  out[n] = 0;
}
