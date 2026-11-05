// Prova radio ESP-NOW tra due schede.
//
// Sketch autonomo, in una cartella propria perché non entri nella build di PokeGotchi. Serve a
// decidere, PRIMA di scrivere il collegamento, se ESP-NOW funziona tra due schede
// Waveshare ESP32-S3-Touch-AMOLED-1.75 e se con il WiFi acceso resta abbastanza RAM interna.
// Il codice ESP-NOW di DylanPDao/TamaPoke non è mai stato eseguito su hardware (lo dice il
// suo linknow.h): da lì viene solo l'idea di passare i pacchetti dal callback al loop con un
// anello, perché il callback gira nel task del WiFi.
//
// COMPILARE E CARICARE (su tutte e due le schede, lo stesso firmware)
//   FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
//   arduino-cli compile --fqbn "$FQBN" tools/prova_radio
//   arduino-cli upload -p <porta> --fqbn "$FQBN" tools/prova_radio
//   arduino-cli monitor -p <porta> -c baudrate=115200
// Dopo la prova si torna al gioco ricaricando PokeGotchi (o dall'installer web, SENZA «Erase
// device»: la NVS del gioco resta; questo sketch usa solo il namespace "provaradio").
//
// RUOLI. La scheda B (risponditore, "PONG") è quella predefinita: basta accenderla, anche a
// batteria e lontano dal PC. Sulla scheda A, collegata al PC, invia PING: da lì manda un
// ping ogni INT ms e ogni 2 s stampa una riga di statistiche. Il ruolo e il canale restano
// salvati. Lo schermo non si usa (resta spento o nero): la prova è tutta sulla seriale.
//
// COMANDI (seriale, 115200, una riga ciascuno)
//   PING / PONG     ruolo di questa scheda        INT <ms>    intervallo dei ping (10-2000)
//   LEN <byte>      lunghezza del pacchetto (16-250, il collegamento ne userà fino a ~200)
//   CANALE <1-13>   canale WiFi fisso (uguale sulle due schede)
//   OFF / ON        spegne / riaccende WiFi ed ESP-NOW (per il consumo)
//   AZZERA          azzera le statistiche         MEM         stampa la memoria
//
// RIGA DI STATISTICHE (scheda A)
//   inv=  ric=  perdita=%   ping inviati e pong tornati negli ultimi 2 s
//   rtt=min/med/max ms      andata e ritorno, misurato all'arrivo del pong nel callback
//   rssi=  dBm              del pong ricevuto (sulla B si vede quello del ping)
//   txko=                   invii che la radio segnala come non consegnati (solo unicast)
//   modo=bcast|uni          broadcast finché non risponde nessuno, poi unicast al MAC trovato
//   int=  min=  blocco=     RAM interna libera, minimo storico, blocco libero più grande
//
// LA PROVA
//   0. All'avvio la scheda stampa la memoria tre volte: "avvio", "con framebuffer" (un buffer
//      di 466x466x2 = 434312 byte in PSRAM, come quello del gioco) e "con radio". Annota
//      "costo radio" (la RAM interna che WiFi ed ESP-NOW si prendono) e "int" con radio.
//      Annota anche il valore heap= di HEALTH nel gioco vero, in schermata principale.
//   1. 1 m in linea d'aria, INT 100, LEN 200: 2 minuti. Annota perdita, rtt e rssi tipici.
//   2. 10 m in linea d'aria: come sopra.
//   3. Ostacoli: una o due pareti, oppure la scheda B in tasca o dentro la scocca.
//   4. Stress: INT 10 per 1 minuto (il collegamento ne avrà molto meno bisogno).
//   5. Consumo: con un misuratore USB in linea, corrente con OFF e poi con ON (sulla B,
//      mentre riceve i ping). La differenza è il costo della radio accesa.
//   6. Abbinamento: spegni e riaccendi la B durante la prova; la A deve ritrovarla da sola
//      (torna a modo=bcast e poi a uni).
//
// ESITO: il collegamento ha senso solo se
//   - l'abbinamento funziona e a 1 m la perdita resta sotto il 5 %;
//   - con la radio accesa resta RAM interna sufficiente nel gioco vero:
//     heap= di HEALTH nel gioco  -  costo radio di questa prova  >=  50 KB (SOGLIA_KB).
//   Altrimenti: annota i numeri e si decide come procedere.

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_heap_caps.h>
#include <Preferences.h>

#define SCHERMO_W 466   // LCD_WIDTH / LCD_HEIGHT di pin_config.h
#define SCHERMO_H 466
#define SOGLIA_KB 50    // RAM interna che deve restare al gioco con la radio accesa
#define STAT_MS 2000    // una riga di statistiche ogni 2 s
#define ANELLO 16       // pacchetti in attesa tra il callback e il loop

enum : uint8_t { PK_PING = 1, PK_PONG = 2 };
static const uint8_t BROADCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// configurazione, salvata nel namespace "provaradio"
static Preferences prefs;
static bool pinger = false;
static uint16_t intervallo = 100;
static uint8_t lunghezza = 200;
static uint8_t canale = 1;

static bool radioAccesa = false;
static uint8_t *framebuffer = nullptr;
static uint32_t internaPrimaRadio = 0;

// pacchetto ricevuto, copiato dal callback del WiFi e consumato nel loop
struct Ricevuto {
  uint8_t mac[6];
  int8_t rssi;
  uint8_t tipo;
  uint32_t seq, t0;
  uint32_t arrivoUs;
};
static Ricevuto anello[ANELLO];
static volatile uint8_t testa = 0, coda = 0;
static volatile uint32_t persiAnello = 0, txKo = 0;
static portMUX_TYPE muxAnello = portMUX_INITIALIZER_UNLOCKED;

// scheda A
static bool abbinato = false;
static uint8_t peer[6];
static uint32_t seq = 0, ultimoPing = 0;
// statistiche della finestra corrente
static uint32_t inviati = 0, ricevuti = 0, rttMin = UINT32_MAX, rttMax = 0;
static uint64_t rttSomma = 0;
static int16_t rssiUltimo = 0;
static uint32_t ultimaStat = 0, ultimoPong = 0;

static uint32_t leggi32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void scrivi32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

// nel task del WiFi: solo una copia nell'anello, niente Serial né stato della prova
static void alRicevere(const esp_now_recv_info_t *info, const uint8_t *dati, int len) {
  if (len < 12 || dati[0] != 'T' || dati[1] != 'R') return;
  uint32_t ora = micros();
  portENTER_CRITICAL(&muxAnello);
  uint8_t prossimo = (uint8_t)((testa + 1) % ANELLO);
  if (prossimo == coda) {
    persiAnello++;
  } else {
    Ricevuto &r = anello[testa];
    memcpy(r.mac, info->src_addr, 6);
    r.rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
    r.tipo = dati[2];
    r.seq = leggi32(dati + 4);
    r.t0 = leggi32(dati + 8);
    r.arrivoUs = ora;
    testa = prossimo;
  }
  portEXIT_CRITICAL(&muxAnello);
}

static void allInvio(const esp_now_send_info_t *info, esp_now_send_status_t stato) {
  if (stato != ESP_NOW_SEND_SUCCESS) txKo++;
}

static bool prendi(Ricevuto &r) {
  bool c = false;
  portENTER_CRITICAL(&muxAnello);
  if (coda != testa) {
    r = anello[coda];
    coda = (uint8_t)((coda + 1) % ANELLO);
    c = true;
  }
  portEXIT_CRITICAL(&muxAnello);
  return c;
}

static void memoria(const char *quando) {
  Serial.printf("MEM %-20s int=%u min=%u blocco=%u psram=%u\n", quando,
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

static bool aggiungiPeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t p = {};
  memcpy(p.peer_addr, mac, 6);
  p.channel = canale;
  p.ifidx = WIFI_IF_STA;
  p.encrypt = false;
  return esp_now_add_peer(&p) == ESP_OK;
}

static void radioOn() {
  if (radioAccesa) return;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(canale, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERR esp_now_init");
    WiFi.mode(WIFI_OFF);
    return;
  }
  esp_now_register_recv_cb(alRicevere);
  esp_now_register_send_cb(allInvio);
  aggiungiPeer(BROADCAST);
  radioAccesa = true;
  abbinato = false;
  Serial.printf("radio accesa, canale %u, MAC %s, ruolo %s\n", canale, WiFi.macAddress().c_str(),
                pinger ? "PING (A)" : "PONG (B)");
}

static void radioOff() {
  if (!radioAccesa) return;
  esp_now_deinit();
  WiFi.mode(WIFI_OFF);
  radioAccesa = false;
  Serial.println("radio spenta");
}

static void invia(const uint8_t *mac, uint8_t tipo, uint32_t s, uint32_t t0) {
  uint8_t buf[250];
  memset(buf, 0xA5, lunghezza);  // riempimento: conta la lunghezza in aria, non il contenuto
  buf[0] = 'T';
  buf[1] = 'R';
  buf[2] = tipo;
  buf[3] = 0;
  scrivi32(buf + 4, s);
  scrivi32(buf + 8, t0);
  esp_now_send(mac, buf, lunghezza);
}

static void azzera() {
  inviati = ricevuti = 0;
  rttMin = UINT32_MAX;
  rttMax = 0;
  rttSomma = 0;
}

static void stampaStat() {
  if (pinger) {
    float perdita = inviati ? 100.0f * (float)(inviati - (ricevuti > inviati ? inviati : ricevuti)) / inviati : 0;
    if (ricevuti)
      Serial.printf("inv=%u ric=%u perdita=%.1f%% rtt=%.1f/%.1f/%.1f ms rssi=%d", inviati, ricevuti,
                    perdita, rttMin / 1000.0f, (float)(rttSomma / ricevuti) / 1000.0f,
                    rttMax / 1000.0f, rssiUltimo);
    else
      Serial.printf("inv=%u ric=0 perdita=%.1f%% rtt=-", inviati, perdita);
    Serial.printf(" txko=%u modo=%s", (unsigned)txKo, abbinato ? "uni" : "bcast");
  } else {
    Serial.printf("ping ricevuti=%u rssi=%d", ricevuti, rssiUltimo);
  }
  if (persiAnello) Serial.printf(" anello_persi=%u", (unsigned)persiAnello);
  Serial.printf(" int=%u min=%u blocco=%u\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  azzera();
}

static void comando(String riga) {
  riga.trim();
  riga.toUpperCase();
  int n = riga.indexOf(' ') > 0 ? riga.substring(riga.indexOf(' ') + 1).toInt() : 0;
  if (riga == "PING" || riga == "PONG") {
    pinger = riga == "PING";
    prefs.putBool("ping", pinger);
    abbinato = false;
    azzera();
    Serial.printf("ruolo %s\n", pinger ? "PING (A)" : "PONG (B)");
  } else if (riga.startsWith("INT ") && n >= 10 && n <= 2000) {
    intervallo = n;
    prefs.putUShort("int", intervallo);
  } else if (riga.startsWith("LEN ") && n >= 16 && n <= 250) {
    lunghezza = n;
    prefs.putUChar("len", lunghezza);
  } else if (riga.startsWith("CANALE ") && n >= 1 && n <= 13) {
    canale = n;
    prefs.putUChar("ch", canale);
    radioOff();
    radioOn();
  } else if (riga == "OFF") {
    radioOff();
  } else if (riga == "ON") {
    radioOn();
  } else if (riga == "AZZERA") {
    azzera();
    txKo = persiAnello = 0;
  } else if (riga == "MEM") {
    memoria(radioAccesa ? "radio accesa" : "radio spenta");
  } else {
    Serial.println("comandi: PING PONG INT <ms> LEN <byte> CANALE <1-13> OFF ON AZZERA MEM");
    return;
  }
  Serial.printf("ok: int=%u len=%u canale=%u\n", intervallo, lunghezza, canale);
}

void setup() {
  Serial.begin(115200);
  delay(1500);  // CDC su USB: lascia al monitor il tempo di agganciarsi
  Serial.println("\n== PokeGotchi: prova radio ESP-NOW ==");
  prefs.begin("provaradio", false);
  pinger = prefs.getBool("ping", false);
  intervallo = prefs.getUShort("int", 100);
  lunghezza = prefs.getUChar("len", 200);
  canale = prefs.getUChar("ch", 1);

  memoria("avvio");
  framebuffer = (uint8_t *)heap_caps_malloc(SCHERMO_W * SCHERMO_H * 2, MALLOC_CAP_SPIRAM);
  if (framebuffer) memset(framebuffer, 0, SCHERMO_W * SCHERMO_H * 2);
  else Serial.println("ERR framebuffer in PSRAM (PSRAM=opi nel FQBN?)");
  memoria("con framebuffer");
  internaPrimaRadio = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  radioOn();
  delay(200);
  memoria("con radio");
  uint32_t dopo = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  uint32_t costo = internaPrimaRadio > dopo ? internaPrimaRadio - dopo : 0;
  Serial.printf("costo radio: %u byte di RAM interna. Soglia: heap= di HEALTH nel gioco - %u >= %u\n",
                costo, costo, SOGLIA_KB * 1024);
}

void loop() {
  static String riga;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (riga.length()) comando(riga);
      riga = "";
    } else if (riga.length() < 40) {
      riga += c;
    }
  }

  Ricevuto r;
  while (prendi(r)) {
    rssiUltimo = r.rssi;
    if (r.tipo == PK_PING && !pinger) {
      ricevuti++;
      if (aggiungiPeer(r.mac)) invia(r.mac, PK_PONG, r.seq, r.t0);  // unicast: con ACK
    } else if (r.tipo == PK_PONG && pinger) {
      ricevuti++;
      uint32_t rtt = r.arrivoUs - r.t0;
      rttSomma += rtt;
      if (rtt < rttMin) rttMin = rtt;
      if (rtt > rttMax) rttMax = rtt;
      ultimoPong = millis();
      if (!abbinato && aggiungiPeer(r.mac)) {
        memcpy(peer, r.mac, 6);
        abbinato = true;
        Serial.printf("abbinato a %02X:%02X:%02X:%02X:%02X:%02X\n", peer[0], peer[1], peer[2],
                      peer[3], peer[4], peer[5]);
      }
    }
  }

  uint32_t ora = millis();
  if (radioAccesa && pinger) {
    // 3 s senza risposta: la B è sparita, si torna a cercarla in broadcast
    if (abbinato && ora - ultimoPong > 3000) {
      abbinato = false;
      Serial.println("abbinamento perso: torno al broadcast");
    }
    if (ora - ultimoPing >= intervallo) {
      ultimoPing = ora;
      invia(abbinato ? peer : BROADCAST, PK_PING, ++seq, micros());
      inviati++;
    }
  }
  if (radioAccesa && ora - ultimaStat >= STAT_MS) {
    ultimaStat = ora;
    stampaStat();
  }
}
