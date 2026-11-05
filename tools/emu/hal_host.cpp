// Implementazione dell'HAL (hal.h) per l'emulatore su PC: lo schermo è un framebuffer
// che emu.cpp mostra in una finestra SDL2, la SD è una cartella, la NVS un file di testo
// (lo shim Preferences dei test) e l'RTC segue l'orologio virtuale di millis().
#include "../../hal.h"
#include "emu.h"
#include "../../musica.h"
#include "../../musica_dati.h"
#include "../../savefile.h"  // SAVE_NS_PET
#include "../../ota.h"       // PG_REPO
#include "../../ui_common.h" // FW_VERSION
#include "png.h"
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <SDL.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <atomic>
#include <cstring>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include <vector>

EmuOpzioni emuOpz;
char **emuArgv = nullptr;

static const int LCD_W = 466, LCD_H = 466;

// ---------------------------------------------------------------- schermo

// Il pannello finto: Arduino_Canvas::flush() gli passa l'intero framebuffer RGB565. Le
// altre primitive di Arduino_G non servono a una tela, quindi non fanno nulla.
class PannelloFinto : public Arduino_G {
public:
  std::vector<uint16_t> fb;
  uint32_t fotogrammi = 0;
  PannelloFinto() : Arduino_G(LCD_W, LCD_H), fb((size_t)LCD_W * LCD_H, 0) {}
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
static PannelloFinto gPannello;
Arduino_Canvas *gfx = new Arduino_Canvas(LCD_W, LCD_H, &gPannello);
static uint8_t gLuce = 180;

const uint16_t *emuFramebuffer() { return gPannello.fb.data(); }
uint32_t emuFotogrammi() { return gPannello.fotogrammi; }
uint8_t emuLuminosita() { return gLuce; }

bool emuSalvaPng(const char *path) {
  // fuori dal cerchio il vetro nero della scheda; qui grigio scuro, per vederne il bordo
  std::vector<uint16_t> fb(gPannello.fb);
  for (int y = 0; y < LCD_H; y++)
    for (int x = 0; x < LCD_W; x++) {
      int dx = 2 * x + 1 - LCD_W, dy = 2 * y + 1 - LCD_H;
      if (dx * dx + dy * dy > LCD_W * LCD_W) fb[(size_t)y * LCD_W + x] = 0x2104;
    }
  return scriviPng(path, fb.data(), LCD_W, LCD_H);
}

// ---------------------------------------------------------------- NVS e RTC su file

// L'RTC della scheda ha la batteria tampone: tra un avvio e l'altro dell'emulatore il
// tempo scorre come quello del PC, partendo dall'ora (anche accelerata) dell'ultima
// uscita. Il file `<nvs>.rtc` ne tiene l'ora e quella del PC.
static uint32_t gRtcBase = 0;     // epoch emulato al momento gRtcBaseMs
static uint64_t gRtcBaseMs = 0;
static uint32_t gNvsSalvate = 0;  // mockNvsScritture() all'ultimo salvataggio

static std::string fileRtc() { return std::string(emuOpz.nvs) + ".rtc"; }

static void caricaStato() {
  if (mockNvsCarica(emuOpz.nvs)) Serial.printf("[emu] NVS da %s\n", emuOpz.nvs);
  gNvsSalvate = mockNvsScritture();
  // l'RTC in ora locale, come la scheda (l'installer la imposta così); nel file .rtc il
  // secondo numero è l'ora del PC in UTC, che serve solo per il tempo passato
  time_t adesso = time(nullptr);
  struct tm loc;
  localtime_r(&adesso, &loc);
  uint32_t ora = (uint32_t)adesso;
  gRtcBase = (uint32_t)(adesso + loc.tm_gmtoff);
  unsigned long e = 0, pc = 0;
  FILE *f = fopen(fileRtc().c_str(), "r");
  if (f) {
    if (fscanf(f, "%lu %lu", &e, &pc) == 2 && e && pc <= ora) gRtcBase = (uint32_t)(e + (ora - pc));
    fclose(f);
  }
  if (emuOpz.epoch) gRtcBase = emuOpz.epoch;
  gRtcBaseMs = emuTempoMs();
}

void emuSalvaStato() {
  if (mockNvsSalva(emuOpz.nvs)) gNvsSalvate = mockNvsScritture();
  FILE *f = fopen(fileRtc().c_str(), "w");
  if (f) {
    fprintf(f, "%lu %lu\n", (unsigned long)halRtcEpoch(), (unsigned long)time(nullptr));
    fclose(f);
  }
}

void emuNvsSalvaSeCambiata() {
  if (mockNvsScritture() != gNvsSalvate) emuSalvaStato();
}

void halNvsElenca(const char *ns, void (*voce)(const char *chiave, uint8_t tipo, void *ctx),
                  void *ctx) {
  mockNvsElenca(ns, voce, ctx);
}

// ---------------------------------------------------------------- avvio

void halBegin() {
  caricaStato();
  if (!gfx->begin()) Serial.println("gfx->begin() fallito");
}

void halFlush() { gfx->flush(); }
void halSetBrightness(uint8_t v) { gLuce = v; }

// ---------------------------------------------------------------- orologio reale

bool halRtcBegin() { return true; }
uint32_t halRtcEpoch() { return gRtcBase + (uint32_t)((emuTempoMs() - gRtcBaseMs) / 1000); }
void halRtcSetEpoch(uint32_t e) {
  gRtcBase = e;
  gRtcBaseMs = emuTempoMs();
}

// ---------------------------------------------------------------- alimentazione

// sempre al cavo USB con la batteria carica: la luminosità resta quella da "in carica"
static bool gPwr = false;
void emuPremiPwr() { gPwr = true; }
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

// Musica ed effetti dal motore di musica.h, come sulla scheda: emuAudioApri() apre l'uscita
// SDL2 a 32 kHz mono (SDL converte per la scheda audio del PC) e la callback chiama
// musicaRiempi() dal thread audio. Con --muto o --headless non c'è uscita: i comandi restano
// nella coda di musica.h e si perdono. Volume e interruttore MUSICA nelle chiavi "vol" e
// "mus" del namespace del pet, con la migrazione da "snd".
static uint8_t gVolume = 7;
static bool gMusicaAccesa = true;
static std::atomic<bool> gDorme{false};
static SDL_AudioDeviceID gUscita = 0;
static int gFs = 0;  // 0 = nessuna uscita
// diagnostica per AUDIO, come sulla scheda (aggiornata nel thread audio sotto il suo lock)
static uint64_t gLavoroNs = 0, gSuonoNs = 0;
static uint32_t gMaxUs = 0, gBlocchi = 0, gRitardi = 0, gBloccoCampioni = 0;

// Con il pet addormentato (o lo schermo spento) la musica va avanti in silenzio, come
// sulla scheda con l'amplificatore spento.
static void suCampioni(void *, Uint8 *flusso, int byte) {
  uint32_t n = (uint32_t)byte / 2;
  uint64_t t0 = SDL_GetPerformanceCounter();
  musicaRiempi((int16_t *)flusso, n);
  uint64_t ns = (SDL_GetPerformanceCounter() - t0) * 1000000000ULL / SDL_GetPerformanceFrequency();
  if (gDorme) memset(flusso, 0, byte);
  uint64_t durata = (uint64_t)n * 1000000000ULL / gFs;
  gLavoroNs += ns;
  gSuonoNs += durata;
  if (ns / 1000 > gMaxUs) gMaxUs = (uint32_t)(ns / 1000);
  gBlocchi++;
  if (ns > durata) gRitardi++;
  gBloccoCampioni = n;
}

void emuAudioApri(bool suono) {
  const int FS = 32000;  // la frequenza della scheda
  SDL_AudioSpec voluto = {}, avuto = {};
  if (suono && SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
    voluto.freq = FS;
    voluto.format = AUDIO_S16SYS;
    voluto.channels = 1;
    voluto.samples = 512;
    voluto.callback = suCampioni;
    gUscita = SDL_OpenAudioDevice(nullptr, 0, &voluto, &avuto, 0);
  }
  if (suono && !gUscita) fprintf(stderr, "[emu] audio non disponibile: %s\n", SDL_GetError());
  if (gUscita) gFs = avuto.freq;
  musicaInizia(FS);  // l'uscita parte in pausa: il motore è pronto prima della callback
  if (gUscita) SDL_PauseAudioDevice(gUscita, 0);
}

void emuAudioChiudi() {
  if (gUscita) SDL_CloseAudioDevice(gUscita);
  gUscita = 0;
}

void halAudioBegin() {
  Preferences p;
  p.begin(SAVE_NS_PET, true);
  gVolume = p.isKey("vol") ? p.getUChar("vol", 7) : (p.getBool("snd", true) ? 7 : 0);
  gMusicaAccesa = p.getBool("mus", true);
  p.end();
  if (gVolume > 10) gVolume = 10;
  musicaVolume(gVolume);
  halSfx(SFX_HATCH);  // jingle di avvio, come sulla scheda
}
void halSfx(uint8_t id) {
  if (gVolume && !gDorme) musicaEffetto(id);
}
void halAudioSetEnabled(bool on) { halSetVolume(on ? (gVolume ? gVolume : 7) : 0); }
bool halAudioEnabled() { return gVolume > 0; }
void halAudioSetSleeping(bool sleeping) { gDorme = sleeping; }

void halMusica(uint8_t brano) {
  if (brano < MUS_COUNT && !gMusicaAccesa) return;
  musicaSuona(brano);
}
uint8_t halVolume() { return gVolume; }
void halSetVolume(uint8_t v) {
  gVolume = v > 10 ? 10 : v;
  musicaVolume(gVolume);
  Preferences p;
  p.begin(SAVE_NS_PET, false);
  p.putUChar("vol", gVolume);
  p.end();
}
bool halMusicaAccesa() { return gMusicaAccesa; }
void halSetMusicaAccesa(bool on) {
  gMusicaAccesa = on;
  if (!on) musicaFerma();
  Preferences p;
  p.begin(SAVE_NS_PET, false);
  p.putBool("mus", on);
  p.end();
}
void halAudioDiag(HalAudioDiag *d) {
  *d = {};
  d->fs = (uint32_t)gFs;
  d->amplificatore = gUscita != 0;
  d->effetti = musicaEffettiAvviati(&d->ultimoEffetto);
  if (!gUscita) return;
  SDL_LockAudioDevice(gUscita);
  d->bloccoCampioni = (uint16_t)gBloccoCampioni;
  d->caricoPm = gSuonoNs ? (uint16_t)(gLavoroNs * 1000 / gSuonoNs) : 0;
  d->maxUs = gMaxUs;
  d->blocchi = gBlocchi;
  d->ritardi = gRitardi;
  gLavoroNs = gSuonoNs = 0;
  gMaxUs = 0;
  SDL_UnlockAudioDevice(gUscita);
}

// ---------------------------------------------------------------- movimento

// Il sensore finto: manca finché non lo si usa (tasti I/J/K/L, Z, C o i comandi
// INCLINA, SCUOTI, COLPETTI del copione), così `make smoke` gioca col joystick tranne dove
// il copione lo simula. Come sulla scheda: con il sensore spento non arriva nulla, lo
// scossone solo in VEGLIA e GIOCO, i colpetti solo in VEGLIA.
static const float EMU_TASTO_G = 0.42f;          // ~25 gradi: la saturazione dei giochi
static bool gMotoUsato = false;
static uint8_t gMotoModo = HAL_MOTO_SPENTO;
static float gIncX = 0, gIncY = 0;               // INCLINA, in g
static int gTastiX = 0, gTastiY = 0;             // I/J/K/L tenuti
static uint8_t gMotoEventi = 0;
static HalMotoDiag gMotoDiag = {};

void emuMotoInclina(float x, float y) {
  gMotoUsato = true;
  gIncX = x;
  gIncY = y;
}
void emuMotoTasti(int dx, int dy) {
  if (dx || dy) gMotoUsato = true;
  gTastiX = dx;
  gTastiY = dy;
}
void emuMotoScuoti() {
  gMotoUsato = true;
  if (gMotoModo == HAL_MOTO_SPENTO) {
    printf("[emu] scossone perso: sensore spento\n");
    return;
  }
  gMotoEventi |= HAL_EV_SCOSSONE;
  gMotoDiag.scossoni++;
}
void emuMotoColpetti() {
  gMotoUsato = true;
  if (gMotoModo != HAL_MOTO_VEGLIA) {
    printf("[emu] colpetti persi: sensore non in VEGLIA\n");
    return;
  }
  gMotoEventi |= HAL_EV_COLPETTI;
  gMotoDiag.colpetti++;
}

bool halMotoPresente() { return gMotoUsato; }
void halMotoModo(uint8_t modo) {
  if (modo <= HAL_MOTO_GIOCO) gMotoModo = modo;
}
uint8_t halMotoModoAttuale() { return gMotoModo; }
bool halMotoGravita(float *x, float *y, float *z) {
  if (!gMotoUsato || gMotoModo == HAL_MOTO_SPENTO) return false;
  auto lim = [](float v) { return v < -1 ? -1.0f : v > 1 ? 1.0f : v; };
  float gx = lim(gIncX + gTastiX * EMU_TASTO_G), gy = lim(gIncY + gTastiY * EMU_TASTO_G);
  float r = 1 - gx * gx - gy * gy;
  *x = gx;
  *y = gy;
  *z = -sqrtf(r > 0 ? r : 0);
  gMotoDiag.letture++;
  gMotoDiag.gx = *x;
  gMotoDiag.gy = *y;
  gMotoDiag.gz = *z;
  return true;
}
uint8_t halMotoEventi() {
  uint8_t e = gMotoEventi;
  gMotoEventi = 0;
  return e;
}
void halMotoDiag(HalMotoDiag *d) {
  *d = gMotoDiag;
  d->modo = gMotoModo;
}

// ---------------------------------------------------------------- SD (una cartella)

static std::string percorso(const char *p) { return std::string(emuOpz.sd) + p; }

bool halSdBegin() {
  struct stat s;
  bool ok = stat(emuOpz.sd, &s) == 0 && S_ISDIR(s.st_mode);
  if (ok) Serial.printf("[emu] SD = %s\n", emuOpz.sd);
  return ok;
}
bool halSdExists(const char *path) {
  struct stat s;
  return stat(percorso(path).c_str(), &s) == 0;
}
bool halSdRemove(const char *path) { return unlink(percorso(path).c_str()) == 0; }
bool halSdRename(const char *da, const char *a) {
  return rename(percorso(da).c_str(), percorso(a).c_str()) == 0;
}
bool halSdMkdir(const char *path) {
  return mkdir(percorso(path).c_str(), 0755) == 0 || halSdExists(path);
}

void halSdSpazio(uint64_t *totale, uint64_t *usato) {
  struct statvfs v;
  *totale = *usato = 0;
  if (statvfs(emuOpz.sd, &v) != 0) return;
  *totale = (uint64_t)v.f_blocks * v.f_frsize;
  *usato = *totale - (uint64_t)v.f_bfree * v.f_frsize;
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
  FILE *f = fopen(percorso(path).c_str(), scrittura ? "wb" : "rb");
  return f ? HalFile(f) : HalFile();
}

// ---------------------------------------------------------------- radio

// Il collegamento tra emulatori sulla stessa macchina: datagrammi UDP su 127.0.0.1.
// Ogni istanza prende la prima porta libera da EMU_RADIO_PORTA (fino a EMU_RADIO_N istanze)
// e il suo MAC finto ne porta l'indice nell'ultimo byte; il broadcast va a tutte le altre
// porte, un unicast solo a quella del MAC. Il canale non conta. Su localhost non si perde
// né si disordina nulla: perdita, duplicati e riordino li prova test/test_link.cpp.
#define EMU_RADIO_PORTA 47470
#define EMU_RADIO_N 8
static const uint8_t MAC_EMU[5] = {0x02, 'T', 'P', 'E', 'M'};
static int gRadioSock = -1;
static uint8_t gRadioIndice = 0;

static sockaddr_in indirizzoRadio(uint8_t i) {
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = htons((uint16_t)(EMU_RADIO_PORTA + i));
  return a;
}

bool halRadioBegin(uint8_t) {
  if (gRadioSock >= 0) return true;
  if (halWifiStato() != HAL_WIFI_SPENTO) return false;  // come sulla scheda: un driver solo
  int s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) return false;
  for (uint8_t i = 0; i < EMU_RADIO_N; i++) {
    sockaddr_in a = indirizzoRadio(i);
    if (bind(s, (sockaddr *)&a, sizeof(a)) == 0) {
      fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
      gRadioSock = s;
      gRadioIndice = i;
      printf("[emu] radio: UDP 127.0.0.1:%d, MAC 02:54:50:45:4D:%02X\n", EMU_RADIO_PORTA + i, i);
      return true;
    }
  }
  close(s);
  return false;
}

void halRadioEnd() {
  if (gRadioSock < 0) return;
  close(gRadioSock);
  gRadioSock = -1;
}

void halRadioMac(uint8_t mac[6]) {
  memset(mac, 0, 6);
  if (gRadioSock < 0) return;
  memcpy(mac, MAC_EMU, 5);
  mac[5] = gRadioIndice;
}

bool halRadioSend(const uint8_t *mac, const uint8_t *dati, uint8_t len) {
  if (gRadioSock < 0 || !len || len > HAL_RADIO_MAX) return false;
  static const uint8_t TUTTI[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  bool broadcast = !mac || !memcmp(mac, TUTTI, 6);
  for (uint8_t i = 0; i < EMU_RADIO_N; i++) {
    if (i == gRadioIndice) continue;
    if (!broadcast && (memcmp(mac, MAC_EMU, 5) || mac[5] != i)) continue;
    sockaddr_in a = indirizzoRadio(i);
    sendto(gRadioSock, dati, len, 0, (sockaddr *)&a, sizeof(a));  // nessuno in ascolto: perso
  }
  return true;
}

int halRadioRicevi(uint8_t mac[6], uint8_t *dati, uint8_t max) {
  if (gRadioSock < 0) return -1;
  uint8_t buf[HAL_RADIO_MAX];
  for (;;) {
    sockaddr_in da = {};
    socklen_t dl = sizeof(da);
    ssize_t n = recvfrom(gRadioSock, buf, sizeof(buf), 0, (sockaddr *)&da, &dl);
    if (n < 0) return -1;
    int i = (int)ntohs(da.sin_port) - EMU_RADIO_PORTA;
    if (i < 0 || i >= EMU_RADIO_N || n == 0) continue;  // non è un altro emulatore
    memcpy(mac, MAC_EMU, 5);
    mac[5] = (uint8_t)i;
    if (n > max) n = max;
    memcpy(dati, buf, (size_t)n);
    return (int)n;
  }
}

// ---------------------------------------------------------------- WiFi e aggiornamento

// Finti, per vedere e fotografare le schermate dell'aggiornamento: sei reti fisse, la
// password giusta di quelle protette è "pikachu1", e l'ultima release è --ota-ultima (senza,
// la versione del firmware: «sei aggiornato»). Il firmware "scaricato" non si scrive: alla
// fine l'esito è riuscito e il riavvio rilancia lo stesso emulatore. I tempi sono in
// millis() virtuali, quindi seguono --fast.
static const HalRete RETI_FINTE[] = {
  { "CasaRossi", -48, true }, { "Bar_Pikachu", -61, false }, { "Lab-Oak", -66, true },
  { "Palestra Celeste", -73, true }, { "Vicino_5G", -80, true }, { "Ospiti", -86, false },
};
static uint8_t gWifiFinto = HAL_WIFI_SPENTO;
static uint32_t gWifiDa = 0;                 // millis() dell'ultima richiesta
static bool gWifiPassOk = false;
static int8_t gScaricaFinto = 0;             // 0 nessuno, 1 testo, 2 firmware
static uint32_t gScaricaDa = 0, gScaricaSize = 0;
static char *gScaricaBuf = nullptr;
static size_t gScaricaMax = 0;
#define FINTO_SCANSIONE_MS 800
#define FINTO_CONNESSIONE_MS 1200
#define FINTO_TESTO_MS 600
#define FINTO_FIRMWARE_MS 6000

bool halWifiPresente() { return true; }

bool halWifiScansiona() {
  if (gRadioSock >= 0) return false;
  if (gWifiFinto == HAL_WIFI_SPENTO) gWifiFinto = HAL_WIFI_ACCESO;
  gWifiDa = millis();
  return true;
}

int halWifiReti(HalRete *reti, int max) {
  if (gWifiFinto == HAL_WIFI_SPENTO) return 0;
  if (millis() - gWifiDa < FINTO_SCANSIONE_MS) return -1;
  int n = 0;
  for (const HalRete &r : RETI_FINTE)
    if (n < max) reti[n++] = r;
  return n;
}

bool halWifiConnetti(const char *ssid, const char *pass) {
  if (gRadioSock >= 0) return false;
  const HalRete *r = nullptr;
  for (const HalRete &x : RETI_FINTE)
    if (!strcmp(x.ssid, ssid)) r = &x;
  gWifiPassOk = r && (!r->protetta || !strcmp(pass, "pikachu1"));
  gWifiFinto = HAL_WIFI_CONNETTE;
  gWifiDa = millis();
  return true;
}

uint8_t halWifiStato() {
  if (gWifiFinto == HAL_WIFI_CONNETTE && millis() - gWifiDa >= FINTO_CONNESSIONE_MS)
    gWifiFinto = gWifiPassOk ? HAL_WIFI_CONNESSO : HAL_WIFI_FALLITO;
  return gWifiFinto;
}

void halWifiSpegni() {
  if (gScaricaFinto) return;
  gWifiFinto = HAL_WIFI_SPENTO;
}

bool halScaricaTesto(const char *url, char *buf, size_t max) {
  if (gScaricaFinto || halWifiStato() != HAL_WIFI_CONNESSO) return false;
  printf("[emu] scarico %s\n", url);
  gScaricaFinto = 1;
  gScaricaDa = millis();
  gScaricaBuf = buf;
  gScaricaMax = max;
  return true;
}

bool halScaricaFirmware(const char *url, uint32_t size, const char *) {
  if (gScaricaFinto || halWifiStato() != HAL_WIFI_CONNESSO) return false;
  printf("[emu] scarico il firmware %s (%u byte), senza scriverlo\n", url, (unsigned)size);
  gScaricaFinto = 2;
  gScaricaDa = millis();
  gScaricaSize = size;
  return true;
}

int32_t halScaricaEsito() {
  uint32_t t = millis() - gScaricaDa;
  if (gScaricaFinto == 1) {
    if (t < FINTO_TESTO_MS) return HAL_SCARICA_IN_CORSO;
    const char *v = emuOpz.otaUltima ? emuOpz.otaUltima : FW_VERSION;
    int n = snprintf(gScaricaBuf, gScaricaMax,
                     "{\"version\":\"%s\",\"url\":\"https://github.com/" PG_REPO
                     "/releases/download/v%s/pokegotchi-%s-ota.bin\",\"size\":1504752,"
                     "\"sha256\":\"%064d\"}", v, v, v, 0);
    gScaricaFinto = 0;
    return n > 0 && (size_t)n < gScaricaMax ? n : HAL_SCARICA_ERR_TROPPO;
  }
  if (gScaricaFinto == 2) {
    if (t < FINTO_FIRMWARE_MS) return HAL_SCARICA_IN_CORSO;
    gScaricaFinto = 0;
    return (int32_t)gScaricaSize;
  }
  return 0;
}

void halScaricaProgresso(uint32_t *fatti, uint32_t *totale) {
  *fatti = *totale = 0;
  if (gScaricaFinto != 2) return;
  uint32_t t = millis() - gScaricaDa;
  *totale = gScaricaSize;
  *fatti = (uint32_t)((uint64_t)gScaricaSize * (t < FINTO_FIRMWARE_MS ? t : FINTO_FIRMWARE_MS) /
                      FINTO_FIRMWARE_MS);
}

// ---------------------------------------------------------------- sistema

// il riavvio rilancia l'emulatore con gli stessi argomenti, dopo aver salvato NVS e RTC
void halRestart() {
  emuSalvaStato();
  fflush(stdout);
  if (emuArgv) execv("/proc/self/exe", emuArgv);
  if (emuArgv) execvp(emuArgv[0], emuArgv);  // macOS: niente /proc
  exit(0);
}
// valori fissi, come una scheda appena avviata (HEALTH e STATS li stampano)
uint32_t halFreeHeap() { return 200000; }
uint32_t halMinFreeHeap() { return 180000; }
uint32_t halFreePsram() { return 7 * 1024 * 1024; }
const char *halMotivoRiavvio() { return "ACCESO"; }
