// Aggiornamento del firmware via WiFi (SCR_AGGIORNA), dalla pillola della versione in fondo
// alle impostazioni. Con una rete ricordata si collega e controlla da solo; altrimenti
// elenca le reti, chiede la password con la tastiera completa (ui_tastiera.cpp) e la
// ricorda solo se la connessione riesce. Se l'ultima release è più recente di FW_VERSION
// propone INSTALLA: scarica nella partizione libera (con dimensione e SHA-256 controllati
// dall'HAL) e riavvia. Versioni e manifest in ota.h, rete e scaricamenti nell'HAL.
//
// All'uscita il WiFi si spegne. Durante lo scaricamento non si esce, lo schermo resta acceso
// e il loop() non scrive la NVS (aggiornaOccupato()).

#include "ui_common.h"
#include "ota.h"
#include "musica_dati.h"  // MUS_NESSUNA

enum AgFase : uint8_t {
  AG_NO_WIFI,       // la piattaforma non ce l'ha (PWA)
  AG_CERCA,         // scansione in corso
  AG_RETI,          // elenco delle reti
  AG_PASSWORD,      // tastiera
  AG_CONNETTE,
  AG_CONN_FALLITA,
  AG_CONTROLLA,     // scarica il manifest
  AG_AGGIORNATO,
  AG_DISPONIBILE,
  AG_BATTERIA,      // INSTALLA rifiutato: batteria scarica e niente USB
  AG_SCARICA,
  AG_FATTO,         // riavvio fra poco
  AG_ERRORE,
};

#define AG_RIGHE 4         // reti per pagina
#define AG_RIGA_Y 112
#define AG_RIGA_H 56
#define AG_RIGA_X 76
#define AG_RIGA_W 314
#define AG_FRECCIA_Y 196   // frecce di pagina ai lati dell'elenco
#define AG_BTN_X 113       // pulsante principale
#define AG_BTN_Y 262
#define AG_BTN_W 240
#define AG_BTN_H 50
#define AG_PILL_Y 326      // ALTRA RETE | DIMENTICA
#define AG_PILL_H 40
#define AG_PILL_SX 62
#define AG_PILL_DX 238
#define AG_PILL_W 166
#define AG_INDIETRO_Y 388  // da qui in giù: indietro
#define AG_RIAVVIO_MS 2500

static AgFase gFase = AG_RETI;
static HalRete gReti[HAL_WIFI_RETI];
static int gNReti = 0;
static uint8_t gPag = 0;
static char gSsid[OTA_SSID_MAX + 1] = "";
static char gPass[OTA_PASS_MAX + 1] = "";
static bool gProtetta = false;
static bool gSalvate = false;     // la rete è quella ricordata
static char gTesto[OTA_MANIFEST_MAX];
static OtaManifest gMan;
static StrId gErrore = S_AG_ERR_RETE;
static uint32_t gFattoDa = 0;

bool aggiornaOccupato() { return gScreen == SCR_AGGIORNA && (gFase == AG_SCARICA || gFase == AG_FATTO); }

static void cercaReti() {
  gNReti = 0;
  gPag = 0;
  gFase = halWifiScansiona() ? AG_CERCA : AG_RETI;
}

static void connetti() {
  gFase = halWifiConnetti(gSsid, gPass) ? AG_CONNETTE : AG_CONN_FALLITA;
}

static void errore(StrId s) {
  gErrore = s;
  gFase = AG_ERRORE;
}

static void controlla() {
  if (halScaricaTesto(otaUrlManifest(), gTesto, sizeof(gTesto))) gFase = AG_CONTROLLA;
  else errore(S_AG_ERR_RETE);
}

static void installa() {
  if (!otaBatteriaOk(halBatPercent(), halUsbPresent())) {
    gFase = AG_BATTERIA;
    return;
  }
  // la partita salvata prima: dopo il riavvio riparte da qui (il tempo lo recupera l'RTC)
  if (pet.savePending()) pet.flushSave();
  halMusica(MUS_NESSUNA);
  if (halScaricaFirmware(gMan.url, gMan.size, gMan.sha256)) gFase = AG_SCARICA;
  else errore(S_AG_ERR_RETE);
}

void aggiornaEnter() {
  gSsid[0] = gPass[0] = 0;
  gSalvate = false;
  if (!halWifiPresente()) {
    gFase = AG_NO_WIFI;
  } else if (otaCredenziali(gSsid, sizeof(gSsid), gPass, sizeof(gPass))) {
    gSalvate = true;
    connetti();
  } else {
    cercaReti();
  }
}

void aggiornaExit() {
  halWifiSpegni();
  memset(gPass, 0, sizeof(gPass));
}

// avanza la fase con quello che arriva dall'HAL; dal render, a ogni fotogramma
static void avanza() {
  // schermo acceso durante le attese della rete: l'esito si vede senza toccare
  if (gFase == AG_CERCA || gFase == AG_CONNETTE || gFase == AG_CONTROLLA) lastInteract = millis();
  switch (gFase) {
    case AG_CERCA: {
      int n = halWifiReti(gReti, HAL_WIFI_RETI);
      if (n >= 0) {
        gNReti = n;
        gFase = AG_RETI;
      }
      break;
    }
    case AG_CONNETTE: {
      uint8_t s = halWifiStato();
      if (s == HAL_WIFI_CONNESSO) {
        if (!gSalvate && otaSalvaCredenziali(gSsid, gPass)) gSalvate = true;
        controlla();
      } else if (s != HAL_WIFI_CONNETTE) {
        gFase = AG_CONN_FALLITA;
      }
      break;
    }
    case AG_CONTROLLA: {
      int32_t e = halScaricaEsito();
      if (e == HAL_SCARICA_IN_CORSO) break;
      if (e < 0) errore(e == HAL_SCARICA_ERR_RETE ? S_AG_ERR_RETE : S_AG_ERR_SERVER);
      else if (!otaLeggiManifest(gTesto, &gMan, otaUrlProva())) errore(S_AG_ERR_SERVER);
      else gFase = otaPiuRecente(gMan.versione, FW_VERSION) ? AG_DISPONIBILE : AG_AGGIORNATO;
      break;
    }
    case AG_SCARICA: {
      lastInteract = millis();  // schermo acceso fino alla fine
      int32_t e = halScaricaEsito();
      if (e == HAL_SCARICA_IN_CORSO) break;
      if (e < 0) {
        errore(e == HAL_SCARICA_ERR_RETE ? S_AG_ERR_RETE : S_AG_ERR_FILE);
      } else {
        gFase = AG_FATTO;
        gFattoDa = millis();
        halSfx(SFX_LEVEL);
      }
      break;
    }
    case AG_FATTO:
      lastInteract = millis();
      if (millis() - gFattoDa > AG_RIAVVIO_MS) halRestart();
      break;
    default:
      break;
  }
}

// ---------------------------------------------------------------- disegno

static void riga(StrId s, int y, uint16_t colore) {
  gfx->setTextColor(colore);
  setSize(2);
  setCur(centerX(T(s), 2), y);
  printT(T(s));
}

static void pulsante(int x, int y, int w, int h, const char *t, uint16_t fondo, uint16_t testo) {
  gfx->fillRoundRect(x, y, w, h, 12, fondo);
  gfx->drawRoundRect(x, y, w, h, 12, UI_INK);
  gfx->setTextColor(testo);
  setSize(2);
  setCur(x + (w - textW(t, 2)) / 2, y + h / 2 - 8);
  printT(t);
}

static void principale(StrId s) {
  pulsante(AG_BTN_X, AG_BTN_Y, AG_BTN_W, AG_BTN_H, T(s), UI_BAR_OK, UI_BG_DAY);
}

static void pillole() {
  pulsante(AG_PILL_SX, AG_PILL_Y, AG_PILL_W, AG_PILL_H, T(S_AG_ALTRA_RETE), UI_WHITE, UI_INK);
  if (gSalvate)
    pulsante(AG_PILL_DX, AG_PILL_Y, AG_PILL_W, AG_PILL_H, T(S_AG_DIMENTICA), UI_WHITE, UI_INK);
}

// barre del segnale (1-4) e lucchetto, a destra di una riga dell'elenco
static void segnale(int x, int y, int8_t rssi, bool protetta) {
  int barre = rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : 1;
  for (int b = 0; b < 4; b++) {
    int h = 6 + b * 5;
    if (b < barre) gfx->fillRect(x + b * 7, y + 22 - h, 5, h, UI_INK);
    else gfx->drawRect(x + b * 7, y + 22 - h, 5, h, UI_HINT);
  }
  if (protetta) {  // lucchetto: archetto e corpo
    gfx->drawRoundRect(x - 17, y + 3, 9, 10, 4, UI_INK);
    gfx->fillRect(x - 19, y + 10, 13, 11, UI_INK);
  }
}

static void disegnaReti() {
  if (gFase == AG_CERCA) {
    riga(S_AG_CERCO_RETI, 200, UI_INK);
    return;
  }
  if (gNReti == 0) {
    riga(S_AG_NESSUNA_RETE, 200, UI_INK);
    principale(S_AG_RIPROVA);
    return;
  }
  riga(S_AG_SCEGLI, 82, UI_HINT);
  char nome[OTA_SSID_MAX + 1];
  for (int i = 0; i < AG_RIGHE; i++) {
    int k = gPag * AG_RIGHE + i;
    if (k >= gNReti) break;
    int y = AG_RIGA_Y + i * AG_RIGA_H;
    gfx->fillRoundRect(AG_RIGA_X, y, AG_RIGA_W, AG_RIGA_H - 8, 10, UI_WHITE);
    gfx->drawRoundRect(AG_RIGA_X, y, AG_RIGA_W, AG_RIGA_H - 8, 10, UI_INK);
    testoStampabile(nome, 19, gReti[k].ssid);  // 18 caratteri: il resto va al segnale
    gfx->setTextColor(UI_INK);
    setSize(2);
    setCur(AG_RIGA_X + 12, y + 16);
    printT(nome);
    segnale(AG_RIGA_X + AG_RIGA_W - 40, y + 12, gReti[k].rssi, gReti[k].protetta);
  }
  int pagine = (gNReti + AG_RIGHE - 1) / AG_RIGHE;
  gfx->setTextColor(UI_INK);
  setSize(3);
  if (gPag > 0) {
    setCur(34, AG_FRECCIA_Y);
    printT("<");
  }
  if (gPag + 1 < pagine) {
    setCur(414, AG_FRECCIA_Y);
    printT(">");
  }
}

void renderAggiorna() {
  avanza();
  if (gFase == AG_PASSWORD) {
    tastieraDisegna();
    halFlush();
    return;
  }
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(T(S_AG_TITOLO), 3), 30);
  printT(T(S_AG_TITOLO));

  char b[64];
  if (gFase != AG_RETI && gFase != AG_CERCA) {
    snprintf(b, sizeof(b), T(S_AG_VERSIONE), FW_VERSION);
    gfx->setTextColor(UI_HINT);
    setSize(2);
    setCur(centerX(b, 2), 82);
    printT(b);
    if (gSsid[0] && gFase != AG_NO_WIFI) {
      char nome[OTA_SSID_MAX + 1];
      testoStampabile(nome, sizeof(nome), gSsid);
      snprintf(b, sizeof(b), T(S_AG_RETE), nome);
      setCur(centerX(b, 2), 110);
      printT(b);
    }
  }

  switch (gFase) {
    case AG_NO_WIFI:
      riga(S_AG_NO_WIFI, 200, UI_INK);
      break;
    case AG_CERCA:
    case AG_RETI:
      disegnaReti();
      break;
    case AG_CONNETTE:
      riga(S_AG_CONNETTO, 200, UI_INK);
      break;
    case AG_CONN_FALLITA:
      riga(S_AG_CONN_FALLITA, 196, UI_BAR_BAD);
      principale(S_AG_RIPROVA);
      pillole();
      break;
    case AG_CONTROLLA:
      riga(S_AG_CONTROLLO, 200, UI_INK);
      break;
    case AG_AGGIORNATO:
      riga(S_AG_AGGIORNATO, 196, UI_INK);
      pillole();
      break;
    case AG_DISPONIBILE:
      snprintf(b, sizeof(b), T(S_AG_NUOVA), gMan.versione[0] == 'v' ? gMan.versione + 1
                                                                        : gMan.versione);
      gfx->setTextColor(UI_INK);
      setSize(2);
      setCur(centerX(b, 2), 196);
      printT(b);
      principale(S_AG_INSTALLA);
      pillole();
      break;
    case AG_BATTERIA:
      riga(S_AG_BATT1, 180, UI_BAR_BAD);
      riga(S_AG_BATT2, 206, UI_BAR_BAD);
      principale(S_AG_RIPROVA);
      break;
    case AG_SCARICA: {
      uint32_t fatti, totale;
      halScaricaProgresso(&fatti, &totale);
      unsigned pc = totale ? (unsigned)((uint64_t)fatti * 100 / totale) : 0;
      snprintf(b, sizeof(b), T(S_AG_SCARICO), pc);
      gfx->setTextColor(UI_INK);
      setSize(2);
      setCur(centerX(b, 2), 170);
      printT(b);
      gfx->drawRoundRect(83, 206, 300, 30, 8, UI_INK);
      if (pc) gfx->fillRoundRect(86, 209, 294 * pc / 100, 24, 6, UI_BAR_OK);
      riga(S_AG_NON_SPEGNERE, 262, UI_BAR_BAD);
      break;
    }
    case AG_FATTO:
      riga(S_AG_FATTO, 200, UI_BAR_OK);
      break;
    case AG_ERRORE:
      riga(gErrore, 196, UI_BAR_BAD);
      principale(S_AG_RIPROVA);
      pillole();
      break;
    case AG_PASSWORD:
      break;
  }
  if (gFase != AG_SCARICA && gFase != AG_FATTO) {
    gfx->setTextColor(UI_HINT);
    setSize(2);
    setCur(centerX(T(S_BACK), 2), AG_INDIETRO_Y + 14);
    printT(T(S_BACK));
  }
  halFlush();
}

// ---------------------------------------------------------------- tocchi

static bool dentro(int16_t x, int16_t y, int bx, int by, int bw, int bh) {
  return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

static void scegliRete(int k) {
  strlcpy(gSsid, gReti[k].ssid, sizeof(gSsid));
  gProtetta = gReti[k].protetta;
  gSalvate = false;
  gPass[0] = 0;
  if (gProtetta) {
    tastieraApri(gSsid, gPass, sizeof(gPass), true);
    gFase = AG_PASSWORD;
  } else {
    connetti();
  }
}

void aggiornaTap(int16_t x, int16_t y) {
  if (gFase == AG_PASSWORD) {
    int8_t r = tastieraTap(x, y);
    if (r == TAST_OK) connetti();
    else if (r == TAST_ANNULLA) gFase = AG_RETI;
    return;
  }
  if (gFase == AG_SCARICA || gFase == AG_FATTO) return;  // non si esce a metà
  if (y >= AG_INDIETRO_Y) {
    halSfx(SFX_TAP);
    goScreen(SCR_CLOCK);
    return;
  }
  bool btn = dentro(x, y, AG_BTN_X, AG_BTN_Y, AG_BTN_W, AG_BTN_H);
  bool altra = dentro(x, y, AG_PILL_SX, AG_PILL_Y, AG_PILL_W, AG_PILL_H);
  bool dimentica = gSalvate && dentro(x, y, AG_PILL_DX, AG_PILL_Y, AG_PILL_W, AG_PILL_H);
  bool pillole = gFase == AG_CONN_FALLITA || gFase == AG_AGGIORNATO || gFase == AG_DISPONIBILE ||
                 gFase == AG_ERRORE;
  if (pillole && altra) {
    halSfx(SFX_TAP);
    cercaReti();
    return;
  }
  if (pillole && dimentica) {
    halSfx(SFX_TAP);
    otaDimentica();
    gSalvate = false;
    gSsid[0] = gPass[0] = 0;
    cercaReti();
    return;
  }
  switch (gFase) {
    case AG_RETI:
      if (gNReti == 0) {
        if (btn) cercaReti();
        return;
      }
      if (y < AG_RIGA_Y) {  // sopra l'elenco: cerca di nuovo
        cercaReti();
        return;
      }
      if (x < AG_RIGA_X || x >= AG_RIGA_X + AG_RIGA_W) {  // frecce di pagina
        int pagine = (gNReti + AG_RIGHE - 1) / AG_RIGHE;
        if (x < AG_RIGA_X && gPag > 0) gPag--;
        else if (x >= AG_RIGA_X + AG_RIGA_W && gPag + 1 < pagine) gPag++;
        return;
      }
      for (int i = 0; i < AG_RIGHE; i++) {
        int k = gPag * AG_RIGHE + i;
        if (k < gNReti && dentro(x, y, AG_RIGA_X, AG_RIGA_Y + i * AG_RIGA_H, AG_RIGA_W, AG_RIGA_H - 8)) {
          halSfx(SFX_TAP);
          scegliRete(k);
          return;
        }
      }
      break;
    case AG_CONN_FALLITA:
      if (!btn) break;
      halSfx(SFX_TAP);
      if (gProtetta && !gSalvate) {  // forse la password era sbagliata: di nuovo la tastiera
        tastieraApri(gSsid, gPass, sizeof(gPass), true);
        gFase = AG_PASSWORD;
      } else {
        connetti();
      }
      break;
    case AG_DISPONIBILE:
    case AG_BATTERIA:
      if (!btn) break;
      halSfx(SFX_TAP);
      installa();
      break;
    case AG_ERRORE:
      if (!btn) break;
      halSfx(SFX_TAP);
      if (halWifiStato() == HAL_WIFI_CONNESSO) controlla();
      else connetti();
      break;
    default:
      break;
  }
}
