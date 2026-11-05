// Implementazione dell'HAL (hal.h) per la PWA, compilata con Emscripten insieme
// agli stessi sorgenti dell'emulatore: il core Arduino di tools/emu/arduino_host.cpp, il
// touch di tools/emu/tocco_host.cpp e la Preferences in memoria dello shim dei test. Il DOM
// (canvas, pointer, tastiera, contesto audio, visibilità) è in app.js, che chiama le funzioni
// `web*` esportate qui sotto.
//   - millis(): l'orologio virtuale di arduino_host.cpp, avanzato con performance.now()
//     solo mentre la pagina è visibile. Al ritorno in primo piano il tempo passato si
//     applica con syncClock(), la stessa progressione offline della scheda spenta.
//   - RTC: Date.now() in ora locale (come la scheda, che l'installer imposta in ora
//     locale), più lo scarto dell'ora impostata a mano nelle impostazioni.
//   - schermo: il framebuffer RGB565 di `gfx` va al canvas come RGBA (putImageData).
//   - audio: il motore di musica.h; i campioni li suona l'AudioWorklet di audio.js.
//   - movimento: DeviceMotionEvent, che app.js passa a webMoto() (permesso iOS al primo gesto).
//   - SD: una cartella in memoria (MEMFS): gli sprite li porta sprite.js su richiesta, le
//     copie dei salvataggi (/save) si salvano in IndexedDB con la NVS.
//   - NVS: la Preferences in memoria dello shim, serializzata in IndexedDB da
//     salvataggi.js (testo di mockNvsTesto()), insieme allo scarto dell'RTC.
//   - salvataggi: lo stesso .pgsave di savefile.cpp; esportazione, importazione e copie
//     automatiche (in IndexedDB) le guida salvataggi.js con le funzioni web* qui sotto.
#include "../ui_common.h"
#include "../musica.h"
#include "../musica_dati.h"
#include "../moto.h"
#include "../tools/emu/emu.h"
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <emscripten.h>
#include <emscripten/html5.h>  // emscripten_date_now
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <sys/random.h>  // getentropy
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

void setup();
void loop();

static const int LCD_W = 466, LCD_H = 466;

// ---------------------------------------------------------------- schermo

// Il pannello finto, come nell'emulatore: Arduino_Canvas::flush() gli passa l'intero
// framebuffer RGB565, che disegna() converte in RGBA per il canvas.
class PannelloWeb : public Arduino_G {
public:
  std::vector<uint16_t> fb;
  uint32_t fotogrammi = 0;
  PannelloWeb() : Arduino_G(LCD_W, LCD_H), fb((size_t)LCD_W * LCD_H, 0) {}
  bool begin(int32_t) override { return true; }
  void draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t *bmp, int16_t w, int16_t h) override {
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++)
        if (x + i >= 0 && x + i < WIDTH && y + j >= 0 && y + j < HEIGHT)
          fb[(size_t)(y + j) * WIDTH + x + i] = bmp[(size_t)j * w + i];
    fotogrammi++;
  }
  void drawBitmap(int16_t, int16_t, uint8_t *, int16_t, int16_t, uint16_t, uint16_t) override {}
  void drawIndexedBitmap(int16_t, int16_t, uint8_t *, uint16_t *, int16_t, int16_t, int16_t) override {}
  void draw3bitRGBBitmap(int16_t, int16_t, uint8_t *, int16_t, int16_t) override {}
  void draw24bitRGBBitmap(int16_t, int16_t, uint8_t *, int16_t, int16_t) override {}
};
static PannelloWeb gPannello;
Arduino_Canvas *gfx = new Arduino_Canvas(LCD_W, LCD_H, &gPannello);
static uint8_t gLuce = 180;

// solo se è cambiato qualcosa: un render ogni 100 ms, il ciclo a ogni fotogramma del browser
static void disegna() {
  static std::vector<uint8_t> rgba((size_t)LCD_W * LCD_H * 4);
  static uint32_t fatti = 0;
  static int luceFatta = -1;
  if (gPannello.fotogrammi == fatti && gLuce == luceFatta) return;
  fatti = gPannello.fotogrammi;
  luceFatta = gLuce;
  // la luminosità dell'AMOLED, non lineare all'occhio (come nell'emulatore)
  uint32_t m = gLuce == 0 ? 0 : 48 + gLuce * 207 / 255;
  uint8_t *p = rgba.data();
  for (uint16_t c : gPannello.fb) {
    uint32_t r = (c >> 11) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, b = (c & 31) * 255 / 31;
    *p++ = (uint8_t)(r * m / 255);
    *p++ = (uint8_t)(g * m / 255);
    *p++ = (uint8_t)(b * m / 255);
    *p++ = 255;
  }
  EM_ASM({ Module.disegna(HEAPU8.subarray($0, $0 + $1)); }, rgba.data(), rgba.size());
}

void halFlush() { gfx->flush(); }
void halSetBrightness(uint8_t v) { gLuce = v; }

// ---------------------------------------------------------------- avvio

void halBegin() {
  emuSerialeStdin(false);  // la console arriva da webSeriale(), non da stdin
  if (!gfx->begin()) Serial.println("gfx->begin() fallito");
}

// ---------------------------------------------------------------- orologio reale

static int64_t gRtcScarto = 0;  // secondi: l'ora impostata a mano meno quella del browser
static bool gRtcCambiato = false;  // lo scarto va risalvato in IndexedDB
// L'RTC della scheda conta in ora locale (TIME <epoch locale>), non in UTC: lo stesso qui,
// altrimenti scena, sonno, serie e date dei salvataggi scambiati con la scheda sbagliano
// di 1-2 ore. getTimezoneOffset() segue anche l'ora legale.
EM_JS(double, scartoFusoMs, (), { return -new Date().getTimezoneOffset() * 60000; });
static uint32_t oraBrowser() { return (uint32_t)((emscripten_date_now() + scartoFusoMs()) / 1000.0); }

bool halRtcBegin() { return true; }
uint32_t halRtcEpoch() { return (uint32_t)((int64_t)oraBrowser() + gRtcScarto); }
void halRtcSetEpoch(uint32_t e) {
  gRtcScarto = (int64_t)e - oraBrowser();
  gRtcCambiato = true;
}

// ---------------------------------------------------------------- alimentazione

// come al cavo USB con la batteria carica; il PWR è il pulsante fuori dal cerchio
static bool gPwr = false;
bool halPowerBegin() { return true; }
int halBatPercent() { return 100; }
bool halBatCharging() { return false; }
int halBatMillivolts() { return 4150; }
bool halUsbPresent() { return true; }
bool halPwrShortPressed() {
  bool p = gPwr;
  gPwr = false;
  return p;
}

// ---------------------------------------------------------------- audio

// Musica ed effetti dal motore di musica.h, come sulla scheda e nell'emulatore. Il WASM gira
// sul thread principale: app.js apre il contesto audio al primo gesto (webAudioApri, alla sua
// frequenza) e, a ogni richiesta del worklet di audio.js, chiede i campioni con
// webAudioRiempi(), che chiama musicaRiempi() e li converte in float per WebAudio. Prima del
// primo gesto non c'è uscita: effetti e brani si scartano (anche il jingle di avvio), così la
// coda del motore non si riempie. Volume e MUSICA nelle chiavi "vol" e "mus" come sulla
// scheda, con la migrazione da "snd".
static uint8_t gVolume = 7;
static bool gMusicaAccesa = true, gDorme = false, gAudioAcceso = false;
static uint32_t gFs = 0;  // 0 = contesto audio non ancora aperto
// diagnostica per AUDIO, come nell'emulatore (dall'ultima lettura, tranne blocchi e ritardi)
static double gLavoroMs = 0, gSuonoMs = 0;
static uint32_t gMaxUs = 0, gBlocchi = 0, gRitardi = 0, gBloccoCampioni = 0;

void halAudioBegin() {
  Preferences p;
  p.begin(SAVE_NS_PET, true);
  gVolume = p.isKey("vol") ? p.getUChar("vol", 7) : (p.getBool("snd", true) ? 7 : 0);
  gMusicaAccesa = p.getBool("mus", true);
  p.end();
  if (gVolume > 10) gVolume = 10;
  halSfx(SFX_HATCH);  // jingle di avvio, come sulla scheda
}
void halSfx(uint8_t id) {
  if (gFs && gVolume && !gDorme) musicaEffetto(id);
}
void halAudioSetEnabled(bool on) { halSetVolume(on ? (gVolume ? gVolume : 7) : 0); }
bool halAudioEnabled() { return gVolume > 0; }
void halAudioSetSleeping(bool sleeping) { gDorme = sleeping; }

void halMusica(uint8_t brano) {
  if (!gFs || (brano < MUS_COUNT && !gMusicaAccesa)) return;
  musicaSuona(brano);
}
uint8_t halVolume() { return gVolume; }
void halSetVolume(uint8_t v) {
  gVolume = v > 10 ? 10 : v;
  if (gFs) musicaVolume(gVolume);
  Preferences p;
  p.begin(SAVE_NS_PET, false);
  p.putUChar("vol", gVolume);
  p.end();
}
bool halMusicaAccesa() { return gMusicaAccesa; }
void halSetMusicaAccesa(bool on) {
  gMusicaAccesa = on;
  if (!on && gFs) musicaFerma();
  Preferences p;
  p.begin(SAVE_NS_PET, false);
  p.putBool("mus", on);
  p.end();
}
void halAudioDiag(HalAudioDiag *d) {
  *d = {};
  d->fs = gFs;
  d->amplificatore = gAudioAcceso;
  d->bloccoCampioni = (uint16_t)gBloccoCampioni;
  d->caricoPm = gSuonoMs > 0 ? (uint16_t)(gLavoroMs * 1000 / gSuonoMs) : 0;
  d->maxUs = gMaxUs;
  d->blocchi = gBlocchi;
  d->ritardi = gRitardi;
  d->effetti = musicaEffettiAvviati(&d->ultimoEffetto);
  gLavoroMs = gSuonoMs = 0;
  gMaxUs = 0;
}

// ---------------------------------------------------------------- movimento

// DeviceMotionEvent da app.js (webMoto): c'è solo dopo il primo campione valido, quindi non
// sul desktop, non con il permesso negato su iOS e non in http (lì i browser tacciono), e il
// gioco usa il joystick. Lo scossone con il riconoscitore di moto.h; niente colpetti: il
// telefono non li distingue dai tocchi sul vetro.
static bool gMotoPresente = false;
static uint8_t gMotoModo = HAL_MOTO_SPENTO;
static float gMx = 0, gMy = 0, gMz = 0;
static uint32_t gMotoMs = 0;  // millis() dell'ultimo campione
static MotoScossone gScossone;
static uint8_t gMotoEventi = 0;
static HalMotoDiag gMotoDiag = {};

bool halMotoPresente() { return gMotoPresente; }
void halMotoModo(uint8_t modo) {
  if (modo > HAL_MOTO_GIOCO || modo == gMotoModo) return;
  gMotoModo = modo;
  gScossone.azzera();
}
uint8_t halMotoModoAttuale() { return gMotoModo; }
bool halMotoGravita(float *x, float *y, float *z) {
  if (!gMotoPresente || gMotoModo == HAL_MOTO_SPENTO) return false;
  *x = gMx;
  *y = gMy;
  *z = gMz;
  return millis() - gMotoMs < 500;
}
uint8_t halMotoEventi() {
  uint8_t e = gMotoEventi;
  gMotoEventi = 0;
  return e;
}
void halMotoDiag(HalMotoDiag *d) {
  *d = gMotoDiag;
  d->modo = gMotoModo;
  d->gx = gMx;
  d->gy = gMy;
  d->gz = gMz;
}

// ---------------------------------------------------------------- SD (in memoria)

// La SD è la cartella /sd del file system in memoria di Emscripten (MEMFS), con le stesse
// chiamate stdio di tools/emu/hal_host.cpp.
//   - /mons: gli sprite arrivano su richiesta. halSdOpen() di un file che manca lo chiede a
//     sprite.js (Module.chiediFile), che lo prende da Cache Storage o lo converte dai PNG di
//     SpriteCollab (pmd.js) e lo scrive qui; webFileArrivato() fa ricaricare lo sprite del
//     pet e le miniature.
//   - /save: le copie di savesd.cpp. Vanno in IndexedDB con la NVS (Module.salvaCartella)
//     e salvataggi.js le riscrive qui prima di main().
#define SD_RADICE "/sd"
static bool gSdSaveCambiata = false;  // /save va risalvata in IndexedDB

static std::string percorso(const char *p) { return std::string(SD_RADICE) + p; }
static void segnaSave(const char *p) {
  if (strncmp(p, SAVE_DIR "/", sizeof(SAVE_DIR)) == 0) gSdSaveCambiata = true;
}

bool halSdBegin() { return mkdir(SD_RADICE, 0755) == 0 || errno == EEXIST; }
bool halSdExists(const char *path) {
  struct stat s;
  return stat(percorso(path).c_str(), &s) == 0;
}
bool halSdRemove(const char *path) {
  segnaSave(path);
  return unlink(percorso(path).c_str()) == 0;
}
bool halSdRename(const char *da, const char *a) {
  segnaSave(a);
  return rename(percorso(da).c_str(), percorso(a).c_str()) == 0;
}
bool halSdMkdir(const char *path) {
  return mkdir(percorso(path).c_str(), 0755) == 0 || halSdExists(path);
}
// lo spazio di una SD da 64 MB, quanto basta agli sprite (LS)
void halSdSpazio(uint64_t *totale, uint64_t *usato) {
  *totale = 64ull << 20;
  *usato = 0;
}

void halSdElenca(const char *dir, void (*voce)(const char *nome, uint32_t size, void *ctx),
                 void *ctx) {
  std::string d = percorso(dir);
  DIR *h = opendir(d.c_str());
  if (!h) return;
  std::vector<std::pair<std::string, uint32_t>> file;  // prima l'elenco: voce() può aprire
  while (struct dirent *e = readdir(h)) {
    struct stat s;
    if (stat((d + "/" + e->d_name).c_str(), &s) == 0 && S_ISREG(s.st_mode))
      file.push_back({e->d_name, (uint32_t)s.st_size});
  }
  closedir(h);
  for (auto &f : file) voce(f.first.c_str(), f.second, ctx);
}

static FILE *fileDi(void *h) { return (FILE *)h; }
uint32_t HalFile::size() {
  struct stat s;
  return h && fstat(fileno(fileDi(h)), &s) == 0 ? (uint32_t)s.st_size : 0;
}
size_t HalFile::read(uint8_t *buf, size_t n) { return h ? fread(buf, 1, n, fileDi(h)) : 0; }
size_t HalFile::write(const uint8_t *buf, size_t n) { return h ? fwrite(buf, 1, n, fileDi(h)) : 0; }
void HalFile::close() {
  if (!h) return;
  fclose(fileDi(h));
  h = nullptr;
}

HalFile halSdOpen(const char *path, bool scrittura) {
  if (scrittura) segnaSave(path);
  FILE *f = fopen(percorso(path).c_str(), scrittura ? "wb" : "rb");
  if (f) return HalFile(f);
  // uno sprite non ancora arrivato: sprite.js lo cerca (e ignora i file che non sa fare,
  // come gli sprite PGK1 di SdMon)
  if (!scrittura && strncmp(path, "/mons/", 6) == 0)
    EM_ASM({ if (Module.chiediFile) Module.chiediFile(UTF8ToString($0)); }, path);
  return HalFile();
}

// ---------------------------------------------------------------- NVS in IndexedDB

// salvataggi.js legge lo stato da IndexedDB prima di avviare il modulo e lo passa a
// webCaricaStato() prima di main(). Si risalva quando la NVS cambia (azioni del gioco,
// flushSave()), al più ogni 2 s, e subito quando la pagina va in secondo piano.
static uint32_t gNvsSalvate = 0;  // mockNvsScritture() all'ultimo salvataggio
static double gNvsSalvateA = 0;   // performance.now() dell'ultimo salvataggio
static bool gRiavvio = false;     // importazione o riavvio in corso: niente più scritture
static char gFwPrima[13] = "";    // il firmware che ha scritto lo stato caricato
static bool gNvsVuota = true;     // nessuno stato in IndexedDB: primo avvio

// `ricarica`: salvataggi.js ricarica la pagina quando la scrittura è finita (halRestart)
static void persisti(bool ricarica) {
  // prima /save: la scrittura dello stato, se `ricarica`, è l'ultima prima di ricaricare
  if (gSdSaveCambiata) EM_ASM({ Module.salvaCartella(UTF8ToString($0)); }, SD_RADICE SAVE_DIR);
  gSdSaveCambiata = false;
  std::string t = mockNvsTesto();
  EM_ASM({ Module.salvaStato(UTF8ToString($0), $1, UTF8ToString($2), $3); }, t.c_str(),
         (double)gRtcScarto, FW_VERSION, ricarica);
  gNvsSalvate = mockNvsScritture();
  gNvsSalvateA = emscripten_get_now();
  gRtcCambiato = false;
}

static void persistiSeCambiata(bool subito) {
  if (gRiavvio || (mockNvsScritture() == gNvsSalvate && !gRtcCambiato && !gSdSaveCambiata)) return;
  if (subito || emscripten_get_now() - gNvsSalvateA >= 2000) persisti(false);
}

void halNvsElenca(const char *ns, void (*voce)(const char *chiave, uint8_t tipo, void *ctx),
                  void *ctx) {
  mockNvsElenca(ns, voce, ctx);
}

// ---------------------------------------------------------------- radio

// niente radio nel browser: il collegamento non parte
bool halRadioBegin(uint8_t) { return false; }
void halRadioEnd() {}
void halRadioMac(uint8_t mac[6]) { memset(mac, 0, 6); }
bool halRadioSend(const uint8_t *, const uint8_t *, uint8_t) { return false; }
int halRadioRicevi(uint8_t *, uint8_t *, uint8_t) { return -1; }

// ---------------------------------------------------------------- WiFi e aggiornamento

// niente WiFi né aggiornamento del firmware: la PWA si aggiorna da sola (service worker)
bool halWifiPresente() { return false; }
bool halWifiScansiona() { return false; }
int halWifiReti(HalRete *, int) { return 0; }
bool halWifiConnetti(const char *, const char *) { return false; }
uint8_t halWifiStato() { return HAL_WIFI_SPENTO; }
void halWifiSpegni() {}
bool halScaricaTesto(const char *, char *, size_t) { return false; }
bool halScaricaFirmware(const char *, uint32_t, const char *) { return false; }
int32_t halScaricaEsito() { return HAL_SCARICA_ERR_NON_C_E; }
void halScaricaProgresso(uint32_t *fatti, uint32_t *totale) { *fatti = *totale = 0; }

// ---------------------------------------------------------------- sistema

// il riavvio (import, WIPE YES) ricarica la pagina, dopo aver scritto la NVS: lo stato in
// RAM del Pet non si salva più (come sulla scheda, che riavvia senza salvarlo)
static void riavvia() {
  if (gRiavvio) return;
  gRiavvio = true;
  emscripten_cancel_main_loop();
  persisti(true);
}
void halRestart() {
  riavvia();
  emscripten_exit_with_live_runtime();
}
uint32_t halFreeHeap() { return 200000; }
uint32_t halMinFreeHeap() { return 180000; }
uint32_t halFreePsram() { return 7 * 1024 * 1024; }
const char *halMotivoRiavvio() { return "ACCESO"; }

// ---------------------------------------------------------------- ciclo e app.js

static double gPrima = 0;          // performance.now() dell'ultimo fotogramma
static bool gVisibile = true;
static double gNascostaDa = 0;     // Date.now() quando la pagina è stata nascosta

static void fotogramma() {
  double ora = emscripten_get_now();
  double dt = ora - gPrima;
  gPrima = ora;
  // un fotogramma lunghissimo (debugger, pagina sospesa senza visibilitychange) non
  // diventa una raffica di tick: il tempo da assente lo recupera webVisibile()
  if (dt < 0) dt = 0;
  if (dt > 1000) dt = 1000;
  emuTempoAvanza((uint32_t)dt);
  loop();
  if (gRiavvio) return;  // halRestart() dentro loop()
  disegna();
  persistiSeCambiata(false);
  // salvataggi.js offre «Importa» accanto alla scelta dell'iniziale
  static int iniziale = -1;
  int attende = pet.awaitingStarter();
  if (attende != iniziale) {
    iniziale = attende;
    EM_ASM({ Module.iniziale($0); }, attende);
  }
}

// il buffer dei salvataggi scambiati con salvataggi.js (Module.HEAPU8 a webBuffer())
static uint8_t gBuf[SAVE_MAX_BYTES];

// i campioni per il worklet (Module.HEAPU8.buffer a webAudioRiempi()), in float -1..1
static const int WEB_AUDIO_MAX = 8192;
static float gCampioni[WEB_AUDIO_MAX];

extern "C" {

EMSCRIPTEN_KEEPALIVE void webDito(int giu, int x, int y) { emuDito(giu != 0, (int16_t)x, (int16_t)y); }
EMSCRIPTEN_KEEPALIVE void webScorri(int dx, int dy) { emuGestoScorri(dx, dy); }
EMSCRIPTEN_KEEPALIVE void webPwr() { gPwr = true; }

// Un DeviceMotionEvent: accelerationIncludingGravity in m/s^2 negli assi del dispositivo (x a
// destra, y verso l'alto, z uscente; in piano con lo schermo in su z = +9.8, la reazione
// alla gravità) e l'angolo di screen.orientation. Il browser fuori dall'app installata ruota
// la pagina: gli assi si riportano a quelli del contenuto ruotando di `angolo`, poi la
// gravità è la reazione cambiata di segno, con y verso il basso come halMotoGravita().
EMSCRIPTEN_KEEPALIVE void webMoto(double ax, double ay, double az, double angolo) {
  double r = angolo * M_PI / 180, c = cos(r), s = sin(r);
  double cx = ax * c - ay * s, cy = ax * s + ay * c;  // assi del contenuto (y in alto)
  const double G = 9.80665;
  float gx = (float)(-cx / G), gy = (float)(cy / G), gz = (float)(-az / G);
  if (gx * gx + gy * gy + gz * gz < 0.04f) return;  // campione vuoto (sensore assente)
  gMotoPresente = true;
  if (gMotoModo == HAL_MOTO_SPENTO) return;
  gMx = gx;
  gMy = gy;
  gMz = gz;
  gMotoMs = millis();
  gMotoDiag.letture++;
  if (gScossone.campione(gx, gy, gz, gMotoMs)) {
    gMotoEventi |= HAL_EV_SCOSSONE;
    gMotoDiag.scossoni++;
  }
}

// una riga della console seriale (STATS, HEALTH...), senza il ritorno a capo
EMSCRIPTEN_KEEPALIVE void webSeriale(const char *riga) {
  emuSerialeRiceve(riga, strlen(riga));
  emuSerialeRiceve("\n", 1);
}

// sprite.js ha scritto `path` (/mons/...) nella SD: se è lo sprite del pet o thumbs.bin,
// ensureMon() li ricarica. Gli altri (galleria, avversario) si vedono alla prossima apertura.
EMSCRIPTEN_KEEPALIVE void webFileArrivato(const char *path) {
  const char *nome = strrchr(path, '/');
  nome = nome ? nome + 1 : path;
  if (strcmp(nome, "thumbs.bin") == 0) sdDirty = true;
  else if (nome[0] == 'p') {
    int dex = atoi(nome + (nome[1] == 's' ? 2 : 1));
    if (dex == pet.speciesId) sdDirty = true;
  }
}

// La pagina va in secondo piano (visibilitychange -> hidden, pagehide): il ciclo del
// browser si ferma e millis() con lui. Si annota l'ora, come una scheda che si spegne.
EMSCRIPTEN_KEEPALIVE void webNascosta() {
  if (!gVisibile || gRiavvio) return;
  gVisibile = false;
  gNascostaDa = emscripten_date_now();
  pet.setClock(halRtcEpoch());
  persistiSeCambiata(true);  // la pagina può non tornare più
}

// Di nuovo in primo piano: meno di 2 minuti scorrono dal vivo (syncClock() non applica
// meno di 2 minuti), il resto è la progressione offline di syncClock().
EMSCRIPTEN_KEEPALIVE void webVisibile() {
  if (gVisibile) return;
  gVisibile = true;
  double via = emscripten_date_now() - gNascostaDa;
  if (via > 0 && via < 120000) emuTempoAvanza((uint32_t)via);
  else pet.syncClock(halRtcEpoch());
  lastInteract = millis();  // lo schermo si riaccende come dopo un tocco
  gPrima = emscripten_get_now();
}

// Prima di main(): lo stato salvato in IndexedDB (testo di mockNvsTesto(), "" se non ce
// n'è), lo scarto dell'RTC, il firmware che l'ha scritto e la lingua del browser (indice
// di `Lang`, -1 se non è tra quelle del gioco): vale solo senza una lingua salvata.
EMSCRIPTEN_KEEPALIVE void webCaricaStato(const char *nvs, double rtc, const char *fw,
                                         int lingua) {
  mockNvsDaTesto(nvs);
  gNvsVuota = mockNvsKeyCount(SAVE_NS_PET) == 0;
  gRtcScarto = (int64_t)rtc;
  strlcpy(gFwPrima, fw, sizeof(gFwPrima));
  if (lingua >= 0 && lingua < LANG_COUNT) gLangDefault = (Lang)lingua;
}

EMSCRIPTEN_KEEPALIVE uint8_t *webBuffer() { return gBuf; }

// Salvataggio attuale in webBuffer() (allinea l'orologio del pet, come sulla scheda);
// restituisce i byte, 0 se non riesce. `motivo` = SAVE_MANUALE, SAVE_GIORNALIERA...
EMSCRIPTEN_KEEPALIVE int webEsporta(int motivo) {
  if (gRiavvio) return 0;
  size_t n = saveEsporta(pet, halRtcEpoch(), FW_VERSION, SAVE_WEB, (uint8_t)motivo,
                         nvsChiavi(), gBuf, sizeof(gBuf));
  persistiSeCambiata(true);  // setClock() ha salvato: anche a pagina nascosta
  return (int)n;
}

// Controlla tutto (saveVerifica) il file di `len` byte messo in webBuffer(): un SaveEsito
EMSCRIPTEN_KEEPALIVE int webVerifica(int len) {
  if (len <= 0 || len > (int)sizeof(gBuf)) return SAVE_ERR_LUNGHEZZA;
  return saveVerifica(gBuf, (size_t)len);
}

// Importa il file in webBuffer() (la copia "prima dell'importazione" l'ha già fatta
// salvataggi.js) e, se riesce, ricarica la pagina senza salvare il Pet in RAM.
EMSCRIPTEN_KEEPALIVE int webImporta(int len) {
  if (gRiavvio) return SAVE_ERR_SCRITTURA;
  if (len <= 0 || len > (int)sizeof(gBuf)) return SAVE_ERR_LUNGHEZZA;
  SaveEsito r = saveImporta(gBuf, (size_t)len, halRtcEpoch(), nullptr, nullptr);
  Serial.printf("SAVE importazione: %s\n", saveEsitoNome(r));
  if (r == SAVE_OK) riavvia();
  return r;
}

EMSCRIPTEN_KEEPALIVE const char *webEsitoNome(int esito) { return saveEsitoNome((SaveEsito)esito); }
EMSCRIPTEN_KEEPALIVE const char *webNomeSpecie(int dex) { return dexName((int16_t)dex); }
// partita appena creata su una NVS vuota: importare non ha nulla da mettere da parte
EMSCRIPTEN_KEEPALIVE int webPrimoAvvio() { return gNvsVuota && pet.awaitingStarter(); }

// app.js ha aperto il contesto audio alla frequenza `fs`: il motore parte lì, una volta sola.
// 0 se musicaInizia() non la accetta (16000..48000 Hz): la PWA resta muta.
EMSCRIPTEN_KEEPALIVE int webAudioApri(int fs) {
  if (gFs) return 1;
  if (fs < 16000 || fs > 48000) return 0;
  musicaInizia((uint32_t)fs);
  musicaVolume(gVolume);
  gFs = (uint32_t)fs;
  return 1;
}
// il contesto audio suona (running) o no: «amp» di AUDIO
EMSCRIPTEN_KEEPALIVE void webAudioAcceso(int acceso) { gAudioAcceso = acceso != 0; }

// I prossimi n campioni (al massimo WEB_AUDIO_MAX) in webAudioRiempi(), per il worklet. Con
// il pet addormentato o lo schermo spento la musica va avanti in silenzio, come sulla scheda.
EMSCRIPTEN_KEEPALIVE float *webAudioRiempi(int n) {
  if (n > WEB_AUDIO_MAX) n = WEB_AUDIO_MAX;
  if (!gFs || n <= 0) return gCampioni;
  double t0 = emscripten_get_now();
  int16_t blocco[256];
  for (int i = 0; i < n; i += 256) {
    int k = n - i < 256 ? n - i : 256;
    musicaRiempi(blocco, (uint32_t)k);
    for (int j = 0; j < k; j++) gCampioni[i + j] = gDorme ? 0.0f : blocco[j] * (1.0f / 32768);
  }
  double ms = emscripten_get_now() - t0, durata = n * 1000.0 / gFs;
  gLavoroMs += ms;
  gSuonoMs += durata;
  if (ms * 1000 > gMaxUs) gMaxUs = (uint32_t)(ms * 1000);
  gBlocchi++;
  if (ms > durata) gRitardi++;
  gBloccoCampioni = (uint32_t)n;
  return gCampioni;
}

}  // extern "C"

int main() {
  // Il seme dal caso del browser (crypto.getRandomValues). Non da Date.now(): i ~1,8e12 ms
  // in un unsigned long a 32 bit di wasm saturano a una costante, e ogni partita nuova
  // aveva gli stessi geni.
  uint32_t seme = 0;
  if (getentropy(&seme, sizeof(seme)) != 0 || !seme)
    seme = (uint32_t)fmod(emscripten_date_now(), 4294967296.0);
  randomSeed(seme);
  // Copia "aggiornamento", come sulla scheda (savesd.cpp): con un firmware diverso da
  // quello che ha scritto lo stato, le chiavi si fotografano prima che load() le migri;
  // epoch e riepilogo si completano dopo setup(). Va tra le copie di IndexedDB.
  size_t foto = 0;
  if (gFwPrima[0] && strcmp(gFwPrima, FW_VERSION) != 0 && !gNvsVuota) {
    SaveIntestazione h;
    h.piattaforma = SAVE_WEB;
    h.motivo = SAVE_AGGIORNAMENTO;
    strlcpy(h.fw, gFwPrima, sizeof(h.fw));
    foto = saveScrivi(h, nvsChiavi(), gBuf, sizeof(gBuf));
  }
  setup();
  if (foto && saveCompletaIntestazione(gBuf, foto, halRtcEpoch(), saveRiepilogo(pet)))
    EM_ASM({ Module.copiaAutomatica(HEAPU8.slice($0, $0 + $1)); }, gBuf, foto);
  persisti(false);  // annota il firmware attuale (e lo stato appena creato)
  gPrima = emscripten_get_now();
  emscripten_set_main_loop(fotogramma, 0, false);  // un passo a ogni requestAnimationFrame
  return 0;
}
