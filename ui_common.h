// Interfaccia condivisa dai file della UI (ui_*.cpp, console.cpp) e da PokeGotchi.ino:
// geometria dello schermo, costanti, globali e prototipi.
#pragma once

#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include <U8g2lib.h>  // font CJK (giapponese); vedi applyLangFont()
#include "hal.h"
#include "species.h"
#include "dex.h"
#include "pet.h"
#include "sdmon.h"
#include "savesd.h"
#include "i18n.h"
#include <new>

// Versione del firmware (semver). Non si cambia a mano: la aggiorna release-please nel PR di
// release, insieme a web/manifest.json, version.txt e CHANGELOG.md (vedi docs/SVILUPPO.md).
// Si mostra nelle impostazioni e via seriale all'avvio; l'aggiornamento via WiFi la confronta
// con quella dell'ultima release.
#define FW_VERSION "3.0.0"  // x-release-please-version

// Un oggetto grande e usato di rado (collegamento, battaglia, elenco dei salvataggi) nella
// PSRAM, creato al primo uso e mai liberato; nella RAM interna solo se la PSRAM manca. La
// DRAM interna serve al driver della radio e agli stack: con questi oggetti in .bss l'heap a
// riposo era 40 KB sotto quello di prima del collegamento.
template <class T> T &inPsram(T *&p) {
  if (!p) {
    void *m = ps_malloc(sizeof(T));
    p = new (m ? m : ::operator new(sizeof(T))) T();
  }
  return *p;
}

#define CX 233  // centro dello schermo rotondo
#define CY 233

static const uint16_t INK_K = 0x18C4;  // spriteColor('k')
// stelle della modalità notte
static const uint16_t STARS[][2] = { {120,140},{330,120},{370,210},{95,230},{280,90},{160,95} };

// ---------- scena di sfondo: bioma del tipo + ora reale dell'RTC ----------

#define C565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

// testo secondario (suggerimenti, «indietro», livelli, versione) sui fondi chiari della UI:
// UI_TRACK (#d8d2bd) è il binario delle barre e come testo su UI_BG_DAY dava ~1,5:1;
// questo grigio caldo dà ~5:1 su UI_BG_DAY e ~5,7:1 sul bianco
#define UI_HINT C565(0x6b, 0x66, 0x58)

// suolo di ogni bioma di giorno (di notte si mescola verso il blu notturno)
static const uint16_t BIOME_SOIL[6] = {
  C565(0x7e, 0xc0, 0x7f),  // 0 prateria
  C565(0xdc, 0xca, 0x94),  // 1 spiaggia (sabbia)
  C565(0x4f, 0x8a, 0x55),  // 2 bosco
  C565(0x8a, 0x55, 0x44),  // 3 vulcano
  C565(0xa8, 0x90, 0x6a),  // 4 montagna
  C565(0xe6, 0xee, 0xf5),  // 5 neve
};
#define GAL_CELL 80

// ---------- macchina a stati della UI ----------
// Una sola schermata è attiva alla volta (gScreen); render() e i gesti di ui_main.cpp
// passano per i suoi gestori in SCREENS. Si cambia solo con goScreen(). Fuori dalla
// tabella resta il dialogo dei salvataggi (saveDlg), modale: copre tutto, anche la scelta
// dell'iniziale, e ha i suoi tocchi. syncScreen() porta a SCR_STARTER quando
// pet.awaitingStarter() (da qualunque schermata) e da SCR_MAIN o SCR_BATTLE a
// SCR_CEREMONY durante pet.ceremony, e ritorno alla principale. La tastiera si apre dalla scheda e con OK torna lì.
// La collezione (SCR_COLLEZIONE) si apre dalla galleria e torna lì. Il collegamento
// (SCR_LINK) si apre dalla scheda, passando la prima volta dalla tastiera per il nome
// dell'allenatore; la radio è accesa solo lì e nella battaglia in collegamento
// (SCR_BATTLE aperta da SCR_LINK, che ci torna alla fine). I giochi (SCR_GAME, SCR_GROTTA,
// SCR_SURF) si aprono dalla scelta dei giochi della principale (OVL_GIOCHI) e tornano lì.
// L'aggiornamento via WiFi (SCR_AGGIORNA) si apre dalla pillola della versione in fondo
// alle impostazioni e torna lì; il WiFi è acceso solo lì.
enum Screen : uint8_t {
  SCR_STARTER, SCR_MAIN, SCR_GALLERY, SCR_CARD, SCR_KEYBOARD, SCR_CLOCK, SCR_GAME,
  SCR_SACK, SCR_CEREMONY, SCR_BATTLE, SCR_COLLEZIONE, SCR_LINK, SCR_GROTTA, SCR_SURF,
  SCR_AGGIORNA, SCR_COUNT
};
// overlay della schermata principale (uno alla volta), con scadenza gOverlayUntil in
// millis() (vedi timeLeft()): menu del cibo, conferma del rilascio, dialoghi di
// evoluzione e di addio, scelta dei giochi. Un tocco lo chiude; uscire da SCR_MAIN lo azzera.
enum Overlay : uint8_t { OVL_NONE, OVL_FEED, OVL_CONFIRM, OVL_EVOLVE, OVL_FAREWELL, OVL_RUNAWAY, OVL_GIOCHI };

// gestori di una schermata; nullptr = gesto ignorato. onTap del sacco scatta alla
// pressione (va martellato), quello delle altre al sollevamento del dito.
struct ScreenOps {
  void (*render)();
  void (*onTap)(int16_t x, int16_t y);
  void (*onSwipe)(int dir);   // +1 = verso destra
  void (*onSwipeV)(int dir);  // +1 = verso il basso
  void (*onEnter)();          // dopo il cambio di gScreen
  void (*onExit)();           // prima del cambio: le pulizie della schermata
};
extern const ScreenOps SCREENS[SCR_COUNT];  // ui_common.cpp

// ---------- globali condivisi ----------

extern Pet pet;                                 // ui_common.cpp
extern SdMon mon;                               // ui_common.cpp
extern PmdMon pmd;                              // ui_common.cpp
extern PmdMon galleryPmd;                       // ui_common.cpp
extern bool gLogTocchi;                         // ui_common.cpp
extern bool screenOff;                          // ui_common.cpp
extern uint32_t lastInteract;                   // ui_common.cpp
extern uint8_t dimStage;                        // ui_common.cpp
extern uint8_t saveDlg;                         // ui_common.cpp
extern bool gNight;                             // ui_common.cpp
extern Screen gScreen;                          // ui_common.cpp
extern Overlay gOverlay;                        // ui_common.cpp
extern uint32_t gOverlayUntil;                  // ui_common.cpp
extern bool gDitoGiu;                           // ui_main.cpp: il dito tenuto sul touch...
extern int16_t gDitoX, gDitoY;                  //   ...e dove (il joystick dei giochi)
extern uint32_t gUltimoTocco;                   // ui_main.cpp: millis() dell'ultimo campione col dito

// ui_common.cpp
void goScreen(Screen s);
void syncScreen();
int flashIdxForDex(int16_t dex);
uint16_t inkColor();
uint16_t lerp565(uint16_t a, uint16_t b, int i, int n);
int sceneHour();
void setSize(uint8_t n);
void setCur(int x, int y);
void applyLangFont();
uint16_t textW(const char *s, uint8_t size);
int centerX(const char *s, uint8_t size);
const char *typeName(uint8_t t);
void printT(char c);
void printT(const char *s);
void drawThumb(const uint8_t *b, int x, int y, int s, bool sil);
uint32_t pmdActTotalMs(const PmdAct &a);
uint8_t pmdFrameAt(const PmdAct &a, uint32_t t, bool loop);
void drawPmdActM(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS,
                 bool flip = false, int clipY = 0x7FFF);
void drawPmdAct(uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS);
void drawMap(const char *const *map, int n, int x, int y, int s, bool silhouette);
// Comandi dei giochi a inclinazione: inclInizia() all'ingresso accende il sensore
// (GIOCO) e apre lo zero di INCL_ZERO_MS; inclAggiorna() a ogni frame tiene sveglio lo
// schermo, misura lo zero (true finché dura) e poi aggiorna inclX()/inclY() da -1 a 1
// (positivo = destra / basso), dal sensore o, se manca, dal joystick sul touch (dito tenuto,
// direzione e intensità dal centro, piena a 150 px). inclFine() all'uscita spegne il sensore.
#define INCL_ZERO_MS 1500
void inclInizia();
void inclFine();
bool inclAggiorna();
float inclX();
float inclY();
bool inclSensore();          // il sensore c'è: lo zero chiede di tenere la scheda ferma
uint32_t inclZeroResta();    // ms che mancano alla fine dello zero

// ui_main.cpp
void ensureMon();
bool inPetZone(int16_t x, int16_t y);
void handleTouch();
void onSwipeV(int dir);
void onSwipe(int dir);
void onTap(int16_t x, int16_t y);
void starterTap(int16_t x, int16_t y);
void mainSwipeV(int dir);
void mainSwipe(int dir);
void mainExit();
void mainTap(int16_t x, int16_t y);
// scossone e colpetti sulla principale: dal loop(), dopo handleTouch()
void motoPrincipale();
int uovoDondolo();
void drawReazione();
void drawClouds(uint32_t now, uint16_t col);
void drawScene(uint8_t biome, uint32_t now, bool night);
void renderStarterSelect();
void render();
void renderCeremony();
void renderMain();
void drawStreakBadge();
void drawCelebration();
void drawBattery();
void drawHeader(const char *name, uint16_t nameColor, const char *msg);
void drawCeremony();
void drawChoiceDialog();
void drawEvolveButton();
void drawFarewellButton();
void drawRunawayButton();
void drawEvolveFX(uint32_t now);
void drawPet();
void startBath();
void drawBath();
void behNext();
void drawPetPMD();
void drawPetSD();
void overlayEye(const Species &sp, int x, int y, int s, int col);
void overlayMouth(const Species &sp, int x, int y, int s, bool open);
void drawPoops();
void drawBars();
void drawBar(int x, int y, const char *label, uint8_t val);
void drawButtons();
const char *eggMsg();
const char *statusMsg();

// ui_game.cpp
void startGame();
void gameEnter();
void respawnBall();
void gameTap(int16_t x, int16_t y);
void stepGame();
void startSack();
void sackEnter();
void sackTap();
void sackScreenTap(int16_t x, int16_t y);
void renderSack();
void drawGameScene();
void renderGame();

// ui_grotta.cpp
void startGrotta();
void grottaEnter();
void grottaExit();
void grottaTap(int16_t x, int16_t y);
void renderGrotta();
void grottaDisegnaDiglett(int cx, int baseY, int s);

// ui_surf.cpp
void startSurf();
void surfEnter();
void surfExit();
void surfTap(int16_t x, int16_t y);
void renderSurf();
void surfDisegnaIcona(int cx, int cy);

// ui_card.cpp
void drawCardStat(int y, const char *label, uint16_t val, uint16_t maxBar, uint16_t color);
void drawMedalBadge(int x, int y, int i);
void renderCardProfile();
void renderCardStats();
int typePillW(uint8_t t);
void drawTypePill(int x, int y, uint8_t t);
void renderCardMedals();
void renderCardProgress();
void renderCard();
void cardEnter();
void cardTap(int16_t x, int16_t y);
void cardSwipe(int dir);
void cardSwipeV(int dir);
void keyboardTrainer();
void keyboardEnter();
void renderKeyboard();
void keyboardTap(int16_t x, int16_t y);

// ui_clock.cpp
void clockEnter();
void clockExit();
void clockSwipeV(int dir);
void applyClock();
void drawClockBtn(int x, int y, const char *l);
void renderClock();
void clockTap(int16_t x, int16_t y);
void openSaves();
void openAudio();
void renderAudio();
void audioTap(int16_t x, int16_t y);
void fmtEpoch(char *out, size_t n, uint32_t e);
const char *saveMotivoT(uint8_t m);
const char *saveNomeT(const SaveRiepilogo &r);
StrId saveEsitoT(SaveEsito e);
void renderSaves();
void savesTap(int16_t x, int16_t y);
void saveDlgApri(SaveEsito r, uint8_t da);
void proponiAvvio();
void renderSaveDialog();
void saveDlgChiudi();
void saveDlgTap(int16_t x, int16_t y);
void saveDlgScadenza();

// ui_gallery.cpp
void galleryEnter();
void galleryExit();
void renderGallery();
void galleryTap(int16_t x, int16_t y);
void gallerySwipe(int dir);

// ui_collezione.cpp
void collezioneEnter();
void renderCollezione();
void collezioneTap(int16_t x, int16_t y);
void collezioneSwipe(int dir);
void renderColDomanda();
void colDomandaTap(int16_t x, int16_t y);

// ui_battle.cpp
bool startBattle(int16_t dex, uint8_t lvl);
bool startCpuBattle();
bool battleDbgPp(uint8_t mossa, uint8_t n);
bool startLinkBattle();
void battleEnter();
void battleExit();
void renderBattle();
void battleTap(int16_t x, int16_t y);

// ui_link.cpp
class LinkBattaglia;
void openLink();
void linkScambioCarica();        // all'avvio, dopo pet.begin(): giornale di uno scambio
LinkBattaglia &linkLb();         // la battaglia in collegamento (linkbattaglia.h)
const char *linkNomeAltro();     // l'altro allenatore, solo ASCII stampabile
uint8_t linkFineVincitore();     // della lotta in corso: lato, pareggio o BATTLE_IN_CORSO
uint8_t linkFineMotivo();        // LbMotivo della fine (KO o RESA)
void linkEnter();
void linkExit();
void linkLoop();
bool linkRadioAccesa();
void linkStato();
void renderLink();
void linkTap(int16_t x, int16_t y);
void linkSwipeV(int dir);

// ui_aggiorna.cpp
void aggiornaEnter();
void aggiornaExit();
void renderAggiorna();
void aggiornaTap(int16_t x, int16_t y);
// uno scaricamento del firmware è in corso (o finito, col riavvio in arrivo): il loop() non
// scrive la NVS e non si esce dalla schermata
bool aggiornaOccupato();

// ui_tastiera.cpp: tastiera completa (ASCII stampabile) disegnata dentro un'altra schermata.
// tastieraApri() la prepara su `buf` (che conserva e modifica, NUL compreso, al massimo
// max - 1 caratteri); tastieraDisegna() disegna tutto lo schermo, senza halFlush();
// tastieraTap() dice se si è confermato o annullato.
enum : int8_t { TAST_NIENTE = 0, TAST_OK, TAST_ANNULLA };
void tastieraApri(const char *titolo, char *buf, size_t max, bool segreta);
void tastieraDisegna();
int8_t tastieraTap(int16_t x, int16_t y);
// solo l'ASCII stampabile di `da` (il resto diventa '?'), per il font 5x7
void testoStampabile(char *out, size_t max, const char *da);

// console.cpp
void handleSerial();
bool motoConsoleAttivo();  // per i 5 s di MOTO: la console decide il modo e prende gli eventi

// PokeGotchi.ino (l'IDE Arduino genera da sé il prototipo; l'emulatore no)
void updateBrightness(uint32_t now);
