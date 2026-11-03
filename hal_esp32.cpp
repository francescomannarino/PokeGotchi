// Implementazione dell'HAL (hal.h) per la Waveshare ESP32-S3-Touch-AMOLED-1.75:
// avvolge i driver (rtcbat, audio, CST9217, CO5300) e SD_MMC/NVS dell'ESP-IDF.
#include "hal.h"
#include "pin_config.h"
#include "rtcbat.h"
#include "audio.h"
#include "linknow.h"
#include "moto.h"
#include <Wire.h>
#include "Arduino_GFX_Library.h"
#include "TouchDrvCSTXXX.hpp"
#include <FS.h>
#include <SD_MMC.h>
#include <esp_system.h>
#include <nvs.h>
#include <esp_mac.h>
#include <atomic>

// ---------------------------------------------------------------- bus I2C

// Il sensore di movimento si legge da un task sul core 0, il resto del bus (touch, PMU, RTC,
// codec) dal loop. Wire da solo non basta: il lock di Wire copre la transazione ma non la
// lettura del suo buffer di ricezione, che un'altra transazione può sovrascrivere. Misurato
// nella prova del sensore: senza un mutex ~2 % dei campioni corrotti (scossoni falsi da ~7 g) ed
// errori I2C; con un mutex attorno a ogni transazione intera, 0 su 12 805. Ogni accesso al
// bus passa quindi da qui (ricorsivo: i driver chiamano altre funzioni avvolte).
static SemaphoreHandle_t gBus = nullptr;
struct BusI2C {
  BusI2C() { if (gBus) xSemaphoreTakeRecursive(gBus, portMAX_DELAY); }
  ~BusI2C() { if (gBus) xSemaphoreGiveRecursive(gBus); }
};

static void motoBegin();

// ---------------------------------------------------------------- schermo e touch

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *panel = new Arduino_CO5300(
  bus, LCD_RESET, 0 /*rotation*/, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
// Framebuffer completo in PSRAM: si disegna tutto e poi flush() (senza sfarfallio)
Arduino_Canvas *gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);

#define TOUCH_ADDR 0x5A  // CST9217
TouchDrvCST92xx touch;
// il CST9217 avvisa dal pin INT quando ci sono dati touch; lo usiamo per non leggere il
// bus I2C mentre il chip dorme (quella lettura si bloccava ~1s)
volatile bool gTouchIrq = false;
void IRAM_ATTR touchIsr() { gTouchIrq = true; }

void halBegin() {
  Serial.setRxBufferSize(8192);  // il trasferimento alla SD arriva a blocchi da 2 KB
  Serial.begin(115200);
  // CRITICO: senza questo, Serial.print BLOCCA il gioco quando non c'è un monitor
  // seriale aperto sull'host (il buffer TX dell'USB CDC si riempie e nessuno lo svuota)
  // -> con timeout 0 i messaggi vengono scartati
  Serial.setTxTimeoutMs(0);
  gBus = xSemaphoreCreateRecursiveMutex();
  BusI2C bus;
  Wire.begin(IIC_SDA, IIC_SCL);
  // CST9217 (touch), AXP2101 (PMU) e PCF85063 (RTC) condividono questo bus I2C. Rete di
  // sicurezza per PMU/RTC (SensorLib NON rispetta questo timeout sul touch; il blocco
  // del touch addormentato si risolve filtrando tramite INT, vedi halTouchRead).
  Wire.setTimeOut(50);

  // CRITICO: accendere l'alimentazione del pannello (BLDO1=OLED VDD 3.3V) PRIMA di
  // inizializzare il display. Se la PMU si è resettata (scarica totale), questo rail
  // resta OFF e lo schermo appare nero anche se il resto della scheda funziona.
  pmuEnablePanel();

  // QSPI a 80MHz (di default 40): il flush del framebuffer è il collo di bottiglia
  // degli fps (~56ms a 40MHz). Se il pannello mostrasse spazzatura, tornare a 40M.
  if (!gfx->begin(80000000)) Serial.println("gfx->begin() fallito");
  panel->setBrightness(180);

  touch.setPins(TP_RESET, TP_INT);
  bool touchOk = false;
  for (int i = 0; i < 3 && !touchOk; i++) {  // a volte fallisce al primo tentativo
    touchOk = touch.begin(Wire, TOUCH_ADDR, IIC_SDA, IIC_SCL);
    if (!touchOk) delay(150);
  }
  if (!touchOk) Serial.println("CST9217 non rilevato");
  // begin() lascia il chip in modalità comando (legge l'identità e non esce); serve un
  // reset hardware perché torni a segnalare i tocchi
  touch.reset();
  touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  touch.setMirrorXY(true, true);  // il pannello è montato ruotato di 180 gradi
  // INT attivo-basso: scatta quando ci sono dati. Filtra le letture I2C
  pinMode(TP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TP_INT), touchIsr, FALLING);
  motoBegin();
}

void halFlush() { gfx->flush(); }
void halSetBrightness(uint8_t v) { panel->setBrightness(v); }

int8_t halTouchRead(int16_t *x, int16_t *y, bool giu) {
  // tocchiamo il bus solo se il chip ha avvisato tramite INT o se il dito è ancora giù
  // (bisogna rilevare il sollevamento). Leggere il CST9217 addormentato si bloccava ~1s
  // e congelava l'intero loop; SensorLib non rispetta il timeout di Wire.
  if (!gTouchIrq && !giu) return HAL_TOUCH_NIENTE;
  gTouchIrq = false;
  BusI2C bus;
  // Un ciclo di solo indirizzo prima di leggere. Senza questo, getPoint() si bloccava
  // esattamente 1000 ms (il timeout predefinito del driver I2C, che SensorLib non
  // delimita) e congelava l'intero loop: è ciò che si percepiva come "il minigioco si
  // blocca 2-3 secondi toccando la palla" (issue #16), perché lì i tocchi sono rapidi e
  // ravvicinati.
  //
  // Misurato sulla scheda, giocando lo stesso (~470 letture, punteggio ~50):
  //   senza questa riga: 5 blocchi da 1000 ms in 60 s
  //   con essa:          0
  // Il contatore dei rifiuti è uscito 0 in entrambi i casi, quindi NON funziona
  // saltando letture quando il chip non risponde: ciò che fa è svegliarlo, perché la
  // lettura successiva non trovi il CST9217 addormentato.
  Wire.beginTransmission(TOUCH_ADDR);
  if (Wire.endTransmission() != 0) return HAL_TOUCH_NIENTE;  // non risponde
  return touch.getPoint(x, y, 1) > 0 ? HAL_TOUCH_GIU : HAL_TOUCH_SU;
}

// ---------------------------------------------------------------- orologio e alimentazione

// PMU e RTC sono sul bus del touch e del sensore: tutto sotto BusI2C
bool halRtcBegin() { BusI2C b; return rtcBegin(); }
uint32_t halRtcEpoch() { BusI2C b; return rtcEpoch(); }
void halRtcSetEpoch(uint32_t e) { BusI2C b; rtcSetEpoch(e); }

bool halPowerBegin() {
  BusI2C b;
  bool ok = batBegin();
  pwrSetup();
  return ok;
}
int halBatPercent() { BusI2C b; return batPercent(); }
bool halBatCharging() { BusI2C b; return batCharging(); }
int halBatMillivolts() { BusI2C b; return batMillivolts(); }
bool halUsbPresent() { BusI2C b; return usbPresent(); }
bool halPwrShortPressed() { BusI2C b; return pwrShortPressed(); }

// ---------------------------------------------------------------- audio

void halAudioBegin() { BusI2C b; audioBegin(); }  // il codec ES8311 è sul bus
void halSfx(uint8_t id) { sfxPlay(id); }
void halAudioSetEnabled(bool on) {
  uint8_t v = audioVolume();
  audioSetVolume(on ? (v ? v : 7) : 0);
}
bool halAudioEnabled() { return audioVolume() > 0; }
void halAudioSetSleeping(bool sleeping) { audioSetSleeping(sleeping); }
void halMusica(uint8_t brano) { audioMusica(brano); }
uint8_t halVolume() { return audioVolume(); }
void halSetVolume(uint8_t v) { audioSetVolume(v); }
bool halMusicaAccesa() { return audioMusicaAccesa(); }
void halSetMusicaAccesa(bool on) { audioSetMusicaAccesa(on); }
void halAudioDiag(HalAudioDiag *d) { audioDiag(d); }

// ---------------------------------------------------------------- movimento

// QMI8658 a 0x6B (o 0x6A) sul bus del touch, letto a registri con Wire (SensorLib non
// rispetta il timeout di Wire) da un task sul core 0 ogni 8 ms: dal loop, che col ridisegno
// gira a ~14 giri/s, arrivavano 14 campioni su 125. Solo accelerometro, niente
// giroscopio. Sequenze e registri dalla prova del sensore (tools/prova_imu).
#define Q_WHO_AM_I 0x00   // 0x05
#define Q_CTRL1 0x02      // interfaccia: auto-incremento, little-endian, uscite INT
#define Q_CTRL2 0x03      // accelerometro: fondo scala <6:4>, ODR <3:0>
#define Q_CTRL7 0x08      // abilitazioni: aEN bit 0, DRDY_DIS bit 5
#define Q_CTRL8 0x09      // motori: Tap_EN bit 0; bit 7 handshake di CTRL9 su STATUSINT
#define Q_CTRL9 0x0A      // comandi
#define Q_CAL1_L 0x0B     // CAL1_L .. CAL4_H = 0x0B .. 0x12
#define Q_STATUSINT 0x2D  // bit 7 CmdDone
#define Q_STATUS0 0x2E    // bit 0 accelerometro pronto; da qui si legge tutto in una volta
#define Q_TAP_STATUS 0x59 // <1:0> numero (1 singolo, 2 doppio)
#define Q_RESET 0x60      // scrivere 0xB0
#define Q_RST_RESULT 0x4D // 0x80 a reset finito
#define Q_CMD_ACK 0x00
#define Q_CMD_TAP 0x0C
#define Q_ODR_125 6       // aODR in modalità solo accelerometro
#define MOTO_LETTURA_MS 8

static uint8_t gQAddr = 0;                       // 0 = nessun sensore
static volatile uint8_t gMotoModo = HAL_MOTO_SPENTO;
static volatile bool gMotoPausa = true;          // il task non legge mentre si configura
static float gLsbPerG = 4096;                    // ±8 g
static uint32_t gScartaFino = 0;                 // millis(): niente campioni fino a qui
static uint32_t gUltimoTs = 0;
static bool gTsValido = false;
static MotoScossone gScossone;
static std::atomic<uint8_t> gMotoEventi{0};
static volatile uint32_t gMotoLetture = 0, gMotoPersi = 0, gMotoErrori = 0;
static volatile uint32_t gMotoScossoni = 0, gMotoColpetti = 0;
// l'ultima gravità, scritta dal task e letta dal loop
static portMUX_TYPE gMotoMux = portMUX_INITIALIZER_UNLOCKED;
static float gGx = 0, gGy = 0, gGz = 0;
static uint32_t gGravitaMs = 0;
static bool gGravitaOk = false;

static bool qLeggi(uint8_t reg, uint8_t *buf, uint8_t n) {
  BusI2C b;
  Wire.beginTransmission(gQAddr);
  Wire.write(reg);
  bool ok = Wire.endTransmission(false) == 0 && Wire.requestFrom(gQAddr, (size_t)n) == n;
  if (ok) for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  else { gMotoErrori = gMotoErrori + 1; while (Wire.available()) Wire.read(); }
  return ok;
}

static bool qScrivi(uint8_t reg, uint8_t v) {
  BusI2C b;
  Wire.beginTransmission(gQAddr);
  Wire.write(reg);
  Wire.write(v);
  bool ok = Wire.endTransmission() == 0;
  if (!ok) gMotoErrori = gMotoErrori + 1;
  return ok;
}

// Protocollo CTRL9 (datasheet 5.10.4): comando, attesa di CmdDone in STATUSINT, ACK. Al
// massimo 100 ms, così non si blocca come il driver.
static bool qComando(uint8_t cmd) {
  if (!qScrivi(Q_CTRL9, cmd)) return false;
  uint32_t t0 = millis();
  uint8_t s = 0;
  bool fatto = false;
  while (millis() - t0 < 100) {
    if (qLeggi(Q_STATUSINT, &s, 1) && (s & 0x80)) { fatto = true; break; }
    delay(1);
  }
  qScrivi(Q_CTRL9, Q_CMD_ACK);
  return fatto;
}

// Motore di tocco (datasheet cap. 10), con i parametri predefiniti della prova a 125 Hz:
// picco 40 ms, quiete 100 ms, doppio entro 500 ms, soglie 0.8 e 0.4 g^2. Le prove dei
// colpetti li confermano o li correggono.
static bool configuraTocco() {
  const uint16_t p = 5, t = 12, d = 62;  // campioni a 125 Hz
  const uint16_t sp = 800, sq = 400;     // g^2 / 1000
  bool ok = qScrivi(Q_CAL1_L, p) && qScrivi(Q_CAL1_L + 1, 0)  // priorità X > Y > Z
    && qScrivi(Q_CAL1_L + 2, t & 0xFF) && qScrivi(Q_CAL1_L + 3, t >> 8)
    && qScrivi(Q_CAL1_L + 4, d & 0xFF) && qScrivi(Q_CAL1_L + 5, d >> 8)
    && qScrivi(Q_CAL1_L + 7, 0x01) && qComando(Q_CMD_TAP);
  return ok && qScrivi(Q_CAL1_L, 8) && qScrivi(Q_CAL1_L + 1, 32)  // alfa 0.0625, gamma 0.25 (x128)
    && qScrivi(Q_CAL1_L + 2, sp & 0xFF) && qScrivi(Q_CAL1_L + 3, sp >> 8)
    && qScrivi(Q_CAL1_L + 4, sq & 0xFF) && qScrivi(Q_CAL1_L + 5, sq >> 8)
    && qScrivi(Q_CAL1_L + 7, 0x02) && qComando(Q_CMD_TAP);
}

// GIOCO: ±4 g. VEGLIA: ±8 g e motore di tocco, la configurazione provata con la prova del sensore. Lo
// scossone (3.78 g misurati) satura a 4 g in GIOCO, sempre sopra la soglia di moto.h.
static bool configuraSensore(uint8_t modo) {
  gMotoPausa = true;
  delay(MOTO_LETTURA_MS + 2);                    // lascia finire la lettura in corso del task
  bool ok = qScrivi(Q_CTRL7, 0x00)               // tutto spento mentre si configura
    && qScrivi(Q_CTRL8, 0x80);                   // handshake di CTRL9, motori spenti
  if (ok && modo != HAL_MOTO_SPENTO) {
    bool veglia = modo == HAL_MOTO_VEGLIA;
    gLsbPerG = veglia ? 4096 : 8192;
    ok = qScrivi(Q_CTRL1, 0x58)                  // auto-incremento, little-endian, INT1 e INT2
      && qScrivi(Q_CTRL2, ((veglia ? 2 : 1) << 4) | Q_ODR_125)
      && (!veglia || configuraTocco())
      && qScrivi(Q_CTRL7, 0x21)                  // accelerometro acceso, DRDY tolto da INT2
      && (!veglia || qScrivi(Q_CTRL8, 0x81));    // motore di tocco acceso
    gTsValido = false;
    gScossone.azzera();
    gScartaFino = millis() + 100;                // appena acceso, un campione da ~14 g
    portENTER_CRITICAL(&gMotoMux);
    gGravitaOk = false;
    portEXIT_CRITICAL(&gMotoMux);
  }
  gMotoModo = modo;
  gMotoPausa = modo == HAL_MOTO_SPENTO;
  return ok;
}

// Una lettura sola da STATUS0 a AZ_H (0x2E..0x3A): stato, stato delle attività, contatore
// dei campioni, temperatura, accelerazioni.
static void leggiSensore() {
  uint8_t b[13];
  if (!qLeggi(Q_STATUS0, b, sizeof(b))) {
    portENTER_CRITICAL(&gMotoMux);
    gGravitaOk = false;
    portEXIT_CRITICAL(&gMotoMux);
    return;
  }
  uint32_t ora = millis();
  uint32_t ts = b[2] | (b[3] << 8) | ((uint32_t)b[4] << 16);
  bool nuovo = b[0] & 0x01;
  if (gTsValido && ts != gUltimoTs) {
    nuovo = true;
    uint32_t d = (ts - gUltimoTs) & 0xFFFFFF;
    // i tre byte non si leggono insieme: un riporto a metà lettura sembra un salto di 256
    if (d > 1 && d < 200) gMotoPersi = gMotoPersi + d - 1;
  }
  if (ts != gUltimoTs || !gTsValido) { gUltimoTs = ts; gTsValido = true; }
  if ((b[1] & 0x02) && gMotoModo == HAL_MOTO_VEGLIA) {  // STATUS1: colpetto
    uint8_t t;
    // appena acceso, il motore parte con la media a zero e vede un colpo su Z: non conta
    if (qLeggi(Q_TAP_STATUS, &t, 1) && (int32_t)(ora - gScartaFino) >= 900 && (t & 0x03) == 2) {
      gMotoColpetti = gMotoColpetti + 1;
      gMotoEventi.fetch_or(HAL_EV_COLPETTI);
    }
  }
  if (!nuovo || (int32_t)(ora - gScartaFino) < 0) return;
  // assi dello schermo dalla prova del sensore: destra = -Y, basso = +X, uscente = +Z del sensore
  float sx = (int16_t)(b[7] | (b[8] << 8)) / gLsbPerG;
  float sy = (int16_t)(b[9] | (b[10] << 8)) / gLsbPerG;
  float sz = (int16_t)(b[11] | (b[12] << 8)) / gLsbPerG;
  float gx = -sy, gy = sx, gz = sz;
  gMotoLetture = gMotoLetture + 1;
  portENTER_CRITICAL(&gMotoMux);
  gGx = gx;
  gGy = gy;
  gGz = gz;
  gGravitaMs = ora;
  gGravitaOk = true;
  portEXIT_CRITICAL(&gMotoMux);
  if (gScossone.campione(gx, gy, gz, ora)) {
    gMotoScossoni = gMotoScossoni + 1;
    gMotoEventi.fetch_or(HAL_EV_SCOSSONE);
  }
}

static void taskMoto(void *) {
  TickType_t t = xTaskGetTickCount();
  for (;;) {
    if (gMotoPausa) {
      vTaskDelay(pdMS_TO_TICKS(20));
      t = xTaskGetTickCount();
      continue;
    }
    leggiSensore();
    vTaskDelayUntil(&t, pdMS_TO_TICKS(MOTO_LETTURA_MS));
  }
}

// Cerca il sensore, lo azzera e lo lascia spento; il task parte comunque fermo.
static void motoBegin() {
  for (uint8_t a : {0x6B, 0x6A}) {
    gQAddr = a;
    uint8_t id = 0;
    if (qLeggi(Q_WHO_AM_I, &id, 1) && id == 0x05) break;
    gQAddr = 0;
  }
  if (gQAddr) {
    qScrivi(Q_RESET, 0xB0);
    delay(15);                                   // durante il reset il chip non risponde
    uint32_t t0 = millis();
    uint8_t r = 0;
    while (millis() - t0 < 500 && !(qLeggi(Q_RST_RESULT, &r, 1) && r == 0x80)) delay(10);
    configuraSensore(HAL_MOTO_SPENTO);
  } else {
    Serial.println("QMI8658 non rilevato");
  }
  gMotoErrori = 0;                               // i NACK della ricerca e del reset non contano
  if (gQAddr) xTaskCreatePinnedToCore(taskMoto, "moto", 3072, nullptr, 2, nullptr, 0);
}

bool halMotoPresente() { return gQAddr != 0; }

void halMotoModo(uint8_t modo) {
  if (!gQAddr || modo == gMotoModo || modo > HAL_MOTO_GIOCO) return;
  if (!configuraSensore(modo)) Serial.printf("MOTO: configurazione del modo %u fallita\n", modo);
}

uint8_t halMotoModoAttuale() { return gMotoModo; }

bool halMotoGravita(float *x, float *y, float *z) {
  if (!gQAddr || gMotoModo == HAL_MOTO_SPENTO) return false;
  portENTER_CRITICAL(&gMotoMux);
  bool ok = gGravitaOk && millis() - gGravitaMs < 200;
  *x = gGx;
  *y = gGy;
  *z = gGz;
  portEXIT_CRITICAL(&gMotoMux);
  return ok;
}

uint8_t halMotoEventi() { return gMotoEventi.exchange(0); }

void halMotoDiag(HalMotoDiag *d) {
  d->indirizzo = gQAddr;
  d->modo = gMotoModo;
  d->letture = gMotoLetture;
  d->persi = gMotoPersi;
  d->erroriI2c = gMotoErrori;
  d->scossoni = gMotoScossoni;
  d->colpetti = gMotoColpetti;
  portENTER_CRITICAL(&gMotoMux);
  d->gx = gGx;
  d->gy = gGy;
  d->gz = gGz;
  portEXIT_CRITICAL(&gMotoMux);
}

// ---------------------------------------------------------------- SD

bool halSdBegin() {
  SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
  // mai formattare in automatico: una scheda exFAT (le >32 GB di fabbrica) o con un
  // errore momentaneo veniva cancellata senza avviso
  bool ok = SD_MMC.begin("/sdcard", true /* modalità 1-bit */, false /* non formattare */);
  if (ok) Serial.printf("SD montata: %llu MB\n", SD_MMC.cardSize() / (1024ULL * 1024ULL));
  return ok;
}

bool halSdExists(const char *path) { return SD_MMC.exists(path); }
bool halSdRemove(const char *path) { return SD_MMC.remove(path); }
bool halSdRename(const char *da, const char *a) { return SD_MMC.rename(da, a); }
bool halSdMkdir(const char *path) { return SD_MMC.exists(path) || SD_MMC.mkdir(path); }

void halSdSpazio(uint64_t *totale, uint64_t *usato) {
  *totale = SD_MMC.totalBytes();
  *usato = SD_MMC.usedBytes();
}

void halSdElenca(const char *dir, void (*voce)(const char *nome, uint32_t size, void *ctx),
                 void *ctx) {
  File d = SD_MMC.open(dir);
  if (!d) return;
  File e;
  char nome[256];  // nome lungo FAT: al massimo 255 caratteri
  while ((e = d.openNextFile())) {
    bool file = !e.isDirectory();
    strlcpy(nome, e.name(), sizeof(nome));
    uint32_t size = e.size();
    e.close();  // voce() può aprire il file
    if (file) voce(nome, size, ctx);
  }
  d.close();
}

static File &fileDi(void *h) { return *(File *)h; }
uint32_t HalFile::size() { return h ? (uint32_t)fileDi(h).size() : 0; }
size_t HalFile::read(uint8_t *buf, size_t n) { return h ? fileDi(h).read(buf, n) : 0; }
size_t HalFile::write(const uint8_t *buf, size_t n) { return h ? fileDi(h).write(buf, n) : 0; }
void HalFile::close() {
  if (!h) return;
  fileDi(h).close();
  delete (File *)h;
  h = nullptr;
}

HalFile halSdOpen(const char *path, bool scrittura) {
  if (scrittura && SD_MMC.exists(path)) SD_MMC.remove(path);
  File f = SD_MMC.open(path, scrittura ? FILE_WRITE : FILE_READ);
  if (!f) return HalFile();
  return HalFile(new File(f));
}

// ---------------------------------------------------------------- NVS

// l'iteratore dell'ESP-IDF 5.x
void halNvsElenca(const char *ns, void (*voce)(const char *chiave, uint8_t tipo, void *ctx),
                  void *ctx) {
  nvs_iterator_t it = nullptr;
  esp_err_t r = nvs_entry_find(NVS_DEFAULT_PART_NAME, ns, NVS_TYPE_ANY, &it);
  while (r == ESP_OK) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    voce(info.key, (uint8_t)info.type, ctx);
    r = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);  // accetta NULL (nessuna voce o fine dell'elenco)
}

// ---------------------------------------------------------------- radio

// Il trasporto ESP-NOW del collegamento: linknow.h, acceso solo in SCR_LINK.
// Mai insieme al WiFi dell'aggiornamento (hal_esp32_rete.cpp): il driver è uno solo.
bool halRadioBegin(uint8_t canale) {
  return halWifiStato() == HAL_WIFI_SPENTO && linknowBegin(canale);
}
void halRadioEnd() { linknowEnd(); }
void halRadioMac(uint8_t mac[6]) { linknowMac(mac); }
bool halRadioSend(const uint8_t *mac, const uint8_t *dati, uint8_t len) {
  return linknowSend(mac, dati, len);
}
int halRadioRicevi(uint8_t mac[6], uint8_t *dati, uint8_t max) {
  return linknowRicevi(mac, dati, max);
}

// ---------------------------------------------------------------- sistema

void halRestart() { ESP.restart(); }
uint32_t halFreeHeap() { return ESP.getFreeHeap(); }
uint32_t halMinFreeHeap() { return ESP.getMinFreeHeap(); }
uint32_t halFreePsram() { return ESP.getFreePsram(); }
const char *halMotivoRiavvio() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "ACCESO";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "CRASH";
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_BROWNOUT: return "TENSIONE";
    case ESP_RST_DEEPSLEEP: return "SONNO";
    case ESP_RST_USB: return "USB";
    default: return "ALTRO";
  }
}
