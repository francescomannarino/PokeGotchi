#include "audio.h"
#include "pin_config.h"
#include "musica.h"
#include "musica_dati.h"
#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <Preferences.h>
#include "savefile.h"   // SAVE_NS_PET

// ---------------------------------------------------------------------------
// Audio di PokeGotchi: codec ES8311 (DAC -> amplificatore PA -> altoparlante) via I2S.
// Musica di battaglia ed effetti escono dal motore di musica.h (APU del Game Boy emulata):
// un task sul core 0 riempie blocchi di campioni con musicaRiempi() e li scrive in I2S, a
// 32 kHz, o a 16 kHz se l'I2S non parte a 32. Il volume è digitale (musicaVolume), il DAC
// dell'ES8311 resta fisso. Init dell'ES8311 portato dal driver ufficiale Espressif
// (esp-bsp), slave I2S a 16 bit.
// ---------------------------------------------------------------------------

#define ES8311_ADDR 0x18
#define FS_PREFERITA 32000
#define FS_RISERVA 16000
#define BLOCCO 256          // campioni per blocco: 8 ms a 32 kHz
#define AMP_AVVIO_MS 60     // zeri all'accensione dell'amplificatore, prima del suono
#define AMP_CODA_MS 10000   // resta acceso dopo l'ultimo suono (i tocchi ravvicinati)

static I2SClass i2s;
static bool gReady = false;
static uint32_t gFs = 0;
static volatile uint8_t gVolume = 7;
static volatile bool gMusicaAccesa = true;
static volatile bool gSleeping = false;
static volatile bool gAmp = false;  // lo scrive solo il task
static TaskHandle_t gTask = nullptr;

// diagnostica per AUDIO: carico e blocco più lento si azzerano a ogni lettura
static portMUX_TYPE gDiagMux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t gLavoroUs = 0, gSuonoUs = 0, gMaxUs = 0, gBlocchi = 0, gRitardi = 0;

// L'NS4150B impiega ben più di 8 ms a stabilizzarsi dopo ogni accensione: acceso subito
// prima di un bip da 35 ms se lo mangiava. Così lo accende il task, quando c'è suono da
// sentire, e prima del primo blocco scrive AMP_AVVIO_MS di silenzio; poi resta acceso
// fino a AMP_CODA_MS dopo l'ultimo suono. Volume 0, sonno e schermo spento lo spengono
// subito: lì la musica va avanti in silenzio, per riprendere a tempo.
static bool udibile() { return gVolume > 0 && !gSleeping; }

static void ampSet(bool on) {
  gAmp = on;
  digitalWrite(PA, on ? HIGH : LOW);
}

// sveglia il task che aspetta un comando a motore fermo
static void sveglia() {
  if (gTask) xTaskNotifyGive(gTask);
}

// ---- I2C del codec ----
static bool esW(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}
static uint8_t esR(uint8_t reg) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ES8311_ADDR, 1);
  return Wire.available() ? Wire.read() : 0;
}

// Sequenza di init VERIFICATA SU QUESTA SCHEDA (progetto PlaneRadar2.0, stessa Waveshare
// 1.75). Chiave: clock DERIVATO DAL BCLK (reg01=0xBF, senza MCLK esterno) e riferimento
// interno che alimenta il DAC (reg44=0x58); senza questi due il codec rispondeva via I2C ma
// non usciva audio. Slave I2S a 16 bit.
// La frequenza non compare nei registri: il clock interno è BCLK*8 = 256*fs e ogni
// divisore è un rapporto su quello, quindi la stessa sequenza vale a 16 e a 32 kHz (nella
// tabella dei coefficienti di Espressif 4,096 MHz/16 kHz e 8,192 MHz/32 kHz hanno la
// stessa riga).
static bool es8311Init() {
  Wire.beginTransmission(ES8311_ADDR);
  if (Wire.endTransmission() != 0) return false;

  // open()
  esW(0x0D, 0xFA); esW(0x44, 0x08); esW(0x44, 0x08);  // power up + stranezza della prima scrittura
  esW(0x01, 0x30); esW(0x02, 0x00); esW(0x03, 0x10); esW(0x16, 0x24);
  esW(0x04, 0x10); esW(0x05, 0x00); esW(0x0B, 0x00); esW(0x0C, 0x00);
  esW(0x10, 0x1F); esW(0x11, 0x7F);
  esW(0x00, 0x80); esW(0x00, 0x80);                   // reset del clock, slave
  esW(0x01, 0xBF);                                    // sorgente clock = BCLK (senza MCLK esterno)
  { uint8_t r = esR(0x06); r &= ~0x20; esW(0x06, r); }  // SCLK non invertito
  esW(0x13, 0x10); esW(0x1B, 0x0A); esW(0x1C, 0x6A);
  esW(0x44, 0x58);                                    // riferimento interno -> alimenta il DAC

  // config_sample(): BCLK*8 = DIG_MCLK
  esW(0x02, 0x18); esW(0x05, 0x00); esW(0x03, 0x10); esW(0x04, 0x20);
  { uint8_t r = esR(0x07); r &= 0xC0; esW(0x07, r); }
  esW(0x08, 0xFF);
  { uint8_t r = esR(0x06); r &= 0xE0; r |= 0x03; esW(0x06, r); }  // bclk_div=4

  // formato I2S 16-bit
  esW(0x09, 0x0C); esW(0x0A, 0x0C);

  // start() DAC slave
  esW(0x00, 0x80); esW(0x01, 0xBF); esW(0x09, 0x0C); esW(0x0A, 0x0C);
  esW(0x17, 0xBF); esW(0x0E, 0x02); esW(0x12, 0x00); esW(0x14, 0x1A);
  esW(0x0D, 0x01); esW(0x15, 0x40); esW(0x37, 0x08); esW(0x45, 0x00);

  // volume fisso + unmute: il volume del gioco è digitale, in musica.cpp
  esW(0x32, 0xBF);                                    // volume DAC ~0 dB
  { uint8_t r = esR(0x31); r &= 0x9F; esW(0x31, r); }  // unmute
  return true;
}

// ---- task audio (core 0) ----

static int16_t gMono[BLOCCO];
static int16_t gStereo[BLOCCO * 2];  // interlacciato, L = R

static void scriviZeri(uint32_t ms) {
  memset(gStereo, 0, sizeof(gStereo));
  for (uint32_t n = gFs * ms / 1000; n > 0;) {
    uint32_t k = n < BLOCCO ? n : BLOCCO;
    i2s.write((uint8_t *)gStereo, k * 4);
    n -= k;
  }
}

static void audioTask(void *) {
  const uint32_t bloccoUs = (uint32_t)BLOCCO * 1000000UL / gFs;
  bool fermo = true;  // niente brano né effetti: si aspetta un comando
  for (;;) {
    if (fermo) {
      // a motore fermo l'I2S non riceve nulla (il DMA ripete zeri); l'amplificatore si
      // spegne se per AMP_CODA_MS non arriva un comando
      TickType_t attesa = gAmp ? pdMS_TO_TICKS(AMP_CODA_MS) : portMAX_DELAY;
      if (!ulTaskNotifyTake(pdTRUE, attesa)) {
        ampSet(false);
        continue;
      }
    }
    bool vuoi = udibile();
    if (vuoi && !gAmp) {
      ampSet(true);
      scriviZeri(AMP_AVVIO_MS);
    } else if (!vuoi && gAmp) {
      ampSet(false);
    }

    uint32_t t0 = micros();
    musicaRiempi(gMono, BLOCCO);
    uint32_t dt = micros() - t0;
    for (int i = 0; i < BLOCCO; i++) gStereo[2 * i] = gStereo[2 * i + 1] = gMono[i];
    i2s.write((uint8_t *)gStereo, sizeof(gStereo));
    fermo = musicaSilenzio();

    portENTER_CRITICAL(&gDiagMux);
    gLavoroUs += dt;
    gSuonoUs += bloccoUs;
    if (dt > gMaxUs) gMaxUs = dt;
    gBlocchi++;
    if (dt > bloccoUs) gRitardi++;
    portEXIT_CRITICAL(&gDiagMux);
  }
}

void audioBegin() {
  pinMode(PA, OUTPUT);
  digitalWrite(PA, LOW);   // lo accende il task, quando c'è suono

  Preferences p;
  p.begin(SAVE_NS_PET, true);
  uint8_t v = p.isKey("vol") ? p.getUChar("vol", 7) : (p.getBool("snd", true) ? 7 : 0);
  gMusicaAccesa = p.getBool("mus", true);
  p.end();
  gVolume = v > 10 ? 10 : v;

  // prima I2S: avvia il BCLK da cui il codec ricava il suo clock
  i2s.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO, I2S_MCK_IO);
  gFs = FS_PREFERITA;
  if (!i2s.begin(I2S_MODE_STD, gFs, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO,
                 I2S_STD_SLOT_BOTH)) {
    gFs = FS_RISERVA;
    Serial.printf("audio: I2S a %u Hz non parte, riserva a %u Hz\n", FS_PREFERITA, gFs);
    if (!i2s.begin(I2S_MODE_STD, gFs, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO,
                   I2S_STD_SLOT_BOTH)) {
      Serial.println("audio: I2S non parte (audio off)");
      gFs = 0;
      return;
    }
  }
  if (!es8311Init()) {
    Serial.println("audio: ES8311 non risponde (audio off)");
    gFs = 0;
    return;
  }

  musicaInizia(gFs);  // prima del task, che chiama musicaRiempi()
  musicaVolume(gVolume);
  gReady = true;
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 3, &gTask, 0);
  sfxPlay(SFX_HATCH);  // jingle di avvio (conferma che suona)
}

void sfxPlay(uint8_t id) {
  if (!gReady || !udibile() || id >= SFX_COUNT) return;
  musicaEffetto(id);
  sveglia();
}

void audioMusica(uint8_t brano) {
  if (!gReady || (brano < MUS_COUNT && !gMusicaAccesa)) return;
  musicaSuona(brano);
  sveglia();
}

uint8_t audioVolume() { return gVolume; }
void audioSetVolume(uint8_t v) {
  gVolume = v > 10 ? 10 : v;
  if (gReady) {
    musicaVolume(gVolume);
    sveglia();  // l'amplificatore segue il volume (0 = spento)
  }
  Preferences p;
  p.begin(SAVE_NS_PET, false);
  p.putUChar("vol", gVolume);
  p.end();
}

bool audioMusicaAccesa() { return gMusicaAccesa; }
void audioSetMusicaAccesa(bool on) {
  gMusicaAccesa = on;
  if (!on && gReady) {
    musicaFerma();
    sveglia();
  }
  Preferences p;
  p.begin(SAVE_NS_PET, false);
  p.putBool("mus", on);
  p.end();
}

void audioSetSleeping(bool sleeping) {
  if (gSleeping == sleeping) return;
  gSleeping = sleeping;
  sveglia();  // il task spegne o riaccende l'amplificatore
}

void audioDiag(HalAudioDiag *d) {
  d->fs = gFs;
  d->riserva = gFs == FS_RISERVA;
  d->amplificatore = gAmp;
  d->bloccoCampioni = BLOCCO;
  portENTER_CRITICAL(&gDiagMux);
  d->caricoPm = gSuonoUs ? (uint16_t)((uint64_t)gLavoroUs * 1000 / gSuonoUs) : 0;
  d->maxUs = gMaxUs;
  d->blocchi = gBlocchi;
  d->ritardi = gRitardi;
  d->effetti = musicaEffettiAvviati(&d->ultimoEffetto);
  gLavoroUs = gSuonoUs = gMaxUs = 0;
  portEXIT_CRITICAL(&gDiagMux);
}
