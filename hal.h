#pragma once
// Strato hardware (HAL): l'unico punto in cui il gioco tocca la piattaforma.
// Implementazioni: `hal_esp32.cpp` (la scheda: avvolge rtcbat, audio, CST9217, CO5300,
// SD_MMC e la NVS dell'ESP-IDF); su PC l'emulatore e la PWA danno la propria.
//
// Restano fuori, perché uguali ovunque o forniti dal core/shim della piattaforma:
//   - il disegno su `gfx`: il canvas è lo stesso su ogni piattaforma, che gli collega
//     il proprio output. Dall'HAL passano solo l'avvio, `halFlush()` e la luminosità;
//   - `millis()`, `random()` e `delay()` del core Arduino: fanno parte del contratto
//     dell'HAL, e su PC li definisce l'implementazione host (con `--fast`), non lo shim
//     dei test;
//   - `Serial` (la console; su PC stdin/stdout), `String` e `Preferences` (shim su PC);
//   - `ps_malloc()` (su PC = malloc).
#include <Arduino.h>

class Arduino_Canvas;
// framebuffer 466x466 RGB565 (PSRAM sulla scheda); lo crea l'implementazione dell'HAL
extern Arduino_Canvas *gfx;

// ---------------------------------------------------------------- avvio

// seriale (115200, TX che non blocca senza monitor), I2C, alimentazione del pannello,
// schermo (luminosità 180) e touch. Va chiamata per prima in `setup()`.
void halBegin();

// ---------------------------------------------------------------- schermo

void halFlush();                      // invia il framebuffer di `gfx` al pannello
void halSetBrightness(uint8_t v);     // 0 = spento, 255 = massimo

// ---------------------------------------------------------------- touch

enum : int8_t { HAL_TOUCH_NIENTE = -1, HAL_TOUCH_SU = 0, HAL_TOUCH_GIU = 1 };
// Stato del dito, in coordinate dello schermo. `giu` = il dito era giù all'ultima lettura:
// senza un avviso del chip e con il dito su non si legge nulla (HAL_TOUCH_NIENTE, stato
// invariato), e così pure se il chip non risponde. x/y valgono solo con HAL_TOUCH_GIU.
int8_t halTouchRead(int16_t *x, int16_t *y, bool giu);

// ---------------------------------------------------------------- orologio reale

bool halRtcBegin();
uint32_t halRtcEpoch();               // secondi unix (UTC); 0 se l'ora non è valida
void halRtcSetEpoch(uint32_t e);

// ---------------------------------------------------------------- alimentazione

bool halPowerBegin();                 // PMU e pulsante PWR (dopo `halBegin()`)
int halBatPercent();                  // 0-100, -1 se non c'è la batteria
bool halBatCharging();
int halBatMillivolts();               // 0 se non c'è lettura
bool halUsbPresent();
bool halPwrShortPressed();            // pressione breve del PWR (da interrogare nel loop)

// ---------------------------------------------------------------- audio

// Effetti sonori del gioco (in coda, non bloccanti). L'ordine coincide con la tabella SFX
// di sfx_note.h.
enum Sfx : uint8_t {
  SFX_TAP = 0,  // tocco / pulsante
  SFX_EAT,      // mangiare
  SFX_PLAY,     // punto del minigioco / colpo
  SFX_HEART,    // gli piace / coccola
  SFX_HATCH,    // schiusa
  SFX_EVOLVE,   // evoluzione
  SFX_MEDAL,    // medaglia / traguardo
  SFX_DENY,     // azione non consentita
  SFX_BYE,      // addio
  SFX_LEVEL,    // sale di livello
  SFX_ATTACK,   // battaglia: parte una mossa
  SFX_HIT,      // battaglia: colpo subito
  SFX_FAINT,    // battaglia: va KO
  SFX_COUNT
};

void halAudioBegin();                 // suona il jingle di avvio
void halSfx(uint8_t id);              // accoda un effetto (scartato se muto o addormentata)
void halAudioSetEnabled(bool on);     // false = volume 0; true = di nuovo udibile
bool halAudioEnabled();               // halVolume() > 0
void halAudioSetSleeping(bool sleeping);

// Musica di battaglia e volume. I brani sono i MUS_* di musica_dati.h.
void halMusica(uint8_t brano);        // dall'inizio; MUS_NESSUNA ferma; niente con MUSICA spenta
uint8_t halVolume();                  // 0..10 per musica ed effetti (chiave "vol"; da "snd":
void halSetVolume(uint8_t v);         //   spento -> 0, acceso -> 7); 0 = muto
bool halMusicaAccesa();               // l'interruttore MUSICA (chiave "mus", sì di default)
void halSetMusicaAccesa(bool on);     // spenta: ferma il brano in corso

// Stato dell'uscita audio, per il comando AUDIO della console. Carico e blocco più lento
// valgono dall'ultima lettura, che li azzera; i contatori dei blocchi dall'avvio.
struct HalAudioDiag {
  uint32_t fs;              // frequenza di campionamento in Hz; 0 = nessuna uscita
  bool riserva;             // 16 kHz perché i 32 kHz non sono partiti
  bool amplificatore;       // acceso adesso (dove non c'è vale true con l'uscita aperta)
  uint16_t bloccoCampioni;  // campioni per blocco
  uint16_t caricoPm;        // tempo di calcolo / durata del suono prodotto, in per mille
  uint32_t maxUs;           // il blocco calcolato più lentamente, in µs
  uint32_t blocchi;         // blocchi prodotti
  uint32_t ritardi;         // blocchi calcolati in più tempo di quanto suonano
  uint32_t effetti;         // effetti sonori arrivati al motore (musicaEffettiAvviati)
  uint8_t ultimoEffetto;    // l'ultimo, un valore di Sfx; 0xFF = nessuno
};
void halAudioDiag(HalAudioDiag *d);

// ---------------------------------------------------------------- movimento

// Sensore di movimento: il QMI8658 sulla scheda, DeviceMotion nella PWA, tastiera
// e copione nell'emulatore. Dove non c'è, `halMotoPresente()` è falso e i giochi usano il
// joystick sul touch. La logica (filtro, scossone) è in moto.h.
enum : uint8_t {
  HAL_MOTO_SPENTO = 0,  // sensore spento (predefinito all'avvio)
  HAL_MOTO_VEGLIA,      // sulla principale: tocco hardware e scossone, consumo basso
  HAL_MOTO_GIOCO,       // giochi a inclinazione: accelerometro a 125 Hz, ±4 g
};
enum : uint8_t {
  HAL_EV_COLPETTI = 1,  // doppio colpetto sulla scocca (solo in VEGLIA, solo la scheda)
  HAL_EV_SCOSSONE = 2,  // scossone (moto.h), in VEGLIA e in GIOCO
};
bool halMotoPresente();
void halMotoModo(uint8_t modo);       // esce subito se il modo non cambia
uint8_t halMotoModoAttuale();
// Gravità in g negli assi dello schermo: x a destra, y in basso, z uscente (in piano, schermo
// in su: 0, 0, -1). false se il sensore non c'è, è spento o la lettura è fallita.
bool halMotoGravita(float *x, float *y, float *z);
uint8_t halMotoEventi();              // bit HAL_EV_*, letti e azzerati

// Stato del sensore, per il comando MOTO della console. Contatori dall'avvio.
struct HalMotoDiag {
  uint8_t indirizzo;        // I2C (0x6B); 0 = nessun sensore o non è un chip
  uint8_t modo;             // HAL_MOTO_*
  uint32_t letture;         // campioni letti
  uint32_t persi;           // campioni saltati (dal contatore del sensore)
  uint32_t erroriI2c;
  uint32_t scossoni, colpetti;
  float gx, gy, gz;         // l'ultima gravità, negli assi dello schermo
};
void halMotoDiag(HalMotoDiag *d);

// ---------------------------------------------------------------- SD

// Percorsi assoluti dalla radice della scheda ("/mons/p025.bin", "/save/...").
bool halSdBegin();                    // monta la SD; true se c'è ed è leggibile
bool halSdExists(const char *path);
bool halSdRemove(const char *path);
bool halSdRename(const char *da, const char *a);
bool halSdMkdir(const char *path);    // true anche se esiste già
void halSdSpazio(uint64_t *totale, uint64_t *usato);
// Chiama `voce` per ogni file (non le cartelle) di `dir`, con il nome senza cartella.
// Non cancellare né rinominare file di `dir` dentro `voce`.
void halSdElenca(const char *dir, void (*voce)(const char *nome, uint32_t size, void *ctx),
                 void *ctx);

// File aperto sulla SD: si chiude alla distruzione, si sposta ma non si copia.
class HalFile {
 public:
  HalFile() {}
  explicit HalFile(void *h) : h(h) {}
  ~HalFile() { close(); }
  HalFile(HalFile &&o) : h(o.h) { o.h = nullptr; }
  HalFile &operator=(HalFile &&o) {
    if (this != &o) { close(); h = o.h; o.h = nullptr; }
    return *this;
  }
  HalFile(const HalFile &) = delete;
  HalFile &operator=(const HalFile &) = delete;

  explicit operator bool() const { return h != nullptr; }
  uint32_t size();
  size_t read(uint8_t *buf, size_t n);
  size_t write(const uint8_t *buf, size_t n);
  void close();

 private:
  void *h = nullptr;  // handle della piattaforma
};
// Lettura, oppure scrittura su un file che riparte sempre vuoto (FILE_WRITE dell'ESP32
// accodava a un file esistente: un PUT ripreso allungava lo sprite invece di sostituirlo).
// Un HalFile falso se non si apre.
HalFile halSdOpen(const char *path, bool scrittura = false);

// ---------------------------------------------------------------- NVS

// Chiama `voce` per ogni chiave del namespace `ns`, con il tipo in codici `nvs_type_t`
// (SAVE_T_* di savefile.h). `Preferences` non sa elencare le chiavi.
void halNvsElenca(const char *ns, void (*voce)(const char *chiave, uint8_t tipo, void *ctx),
                  void *ctx);

// ---------------------------------------------------------------- radio

// Trasporto del collegamento: datagrammi verso un MAC, senza garanzie di consegna
// (le ritrasmissioni le fa il protocollo). La radio si accende solo in `SCR_LINK`.
// Dove non c'è (PC, web) `halRadioBegin()` è false e il resto non fa nulla.
#define HAL_RADIO_MAX 250             // byte per pacchetto (limite di ESP-NOW)
bool halRadioBegin(uint8_t canale);
void halRadioEnd();
void halRadioMac(uint8_t mac[6]);     // il proprio MAC (tutto 0 senza radio)
// `mac` nullo = broadcast. false se non è partito.
bool halRadioSend(const uint8_t *mac, const uint8_t *dati, uint8_t len);
// Il pacchetto più vecchio in coda: ne restituisce la lunghezza e il MAC del mittente,
// -1 se la coda è vuota.
int halRadioRicevi(uint8_t mac[6], uint8_t *dati, uint8_t max);

// ---------------------------------------------------------------- WiFi e aggiornamento

// Il WiFi con un indirizzo IP serve solo all'aggiornamento del firmware (ui_aggiorna.cpp): si
// accende con la scansione o la connessione e si spegne con halWifiSpegni(). Non convive con
// la radio del collegamento: halRadioBegin() fallisce col WiFi acceso e viceversa. Dove non
// c'è (PWA, che si aggiorna da sola) halWifiPresente() è false e il resto non fa nulla.
#define HAL_WIFI_RETI 12              // al massimo, dalla più forte
struct HalRete {
  char ssid[33];
  int8_t rssi;                        // dBm
  bool protetta;                      // serve una password
};
enum : uint8_t {
  HAL_WIFI_SPENTO = 0,
  HAL_WIFI_ACCESO,                    // acceso (scansione), non connesso
  HAL_WIFI_CONNETTE,                  // connessione in corso
  HAL_WIFI_CONNESSO,                  // con un indirizzo IP
  HAL_WIFI_FALLITO,                   // password sbagliata, rete sparita o tempo scaduto
};
bool halWifiPresente();
bool halWifiScansiona();              // avvia la scansione (asincrona); false se non parte
// -1 finché la scansione è in corso; poi il numero di reti in `reti`, dalla più forte, senza
// doppioni né nomi vuoti
int halWifiReti(HalRete *reti, int max);
bool halWifiConnetti(const char *ssid, const char *pass);  // asincrona; pass "" = rete aperta
uint8_t halWifiStato();
void halWifiSpegni();                 // non durante uno scaricamento

// Scaricamenti HTTP(S), uno alla volta, in un task: il loop() resta libero e legge l'esito.
// https verifica il certificato del server; i redirect (GitHub -> CDN) si seguono.
enum : int32_t {
  HAL_SCARICA_IN_CORSO = -1,
  HAL_SCARICA_ERR_RETE = -2,          // connessione, TLS o tempo scaduto
  HAL_SCARICA_ERR_HTTP = -3,          // risposta diversa da 200
  HAL_SCARICA_ERR_TROPPO = -4,        // il testo non sta nel buffer
  HAL_SCARICA_ERR_SPAZIO = -5,        // nessuna partizione libera o troppo piccola
  HAL_SCARICA_ERR_SCRITTURA = -6,     // scrittura della flash o immagine non valida
  HAL_SCARICA_ERR_DIMENSIONE = -7,    // byte diversi da quelli annunciati
  HAL_SCARICA_ERR_SHA = -8,           // SHA-256 diverso: niente cambio di partizione
  HAL_SCARICA_ERR_NON_C_E = -9,       // nessuno scaricamento su questa piattaforma
};
// Un testo piccolo (il manifest della release) in `buf`, terminato da NUL; `buf` deve
// restare valido finché halScaricaEsito() non smette di dare HAL_SCARICA_IN_CORSO.
bool halScaricaTesto(const char *url, char *buf, size_t max);  // false se occupato
// Il firmware nella partizione OTA libera; se dimensione e SHA-256 (64 cifre minuscole)
// tornano, diventa la partizione d'avvio. Il riavvio lo fa il chiamante (halRestart()).
bool halScaricaFirmware(const char *url, uint32_t size, const char *sha256);
// HAL_SCARICA_IN_CORSO, un HAL_SCARICA_ERR_*, oppure (riuscito) i byte scaricati
int32_t halScaricaEsito();
void halScaricaProgresso(uint32_t *fatti, uint32_t *totale);  // totale 0 = ancora ignoto

// ---------------------------------------------------------------- sistema

void halRestart();                    // non ritorna
uint32_t halFreeHeap();               // RAM interna libera, in byte
uint32_t halMinFreeHeap();            // il minimo dall'avvio
uint32_t halFreePsram();
const char *halMotivoRiavvio();      // perché è ripartita l'ultima volta (HEALTH): ACCESO, CRASH…
