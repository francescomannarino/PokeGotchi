// HAL della scheda (hal.h): WiFi con un indirizzo IP, scaricamenti HTTP(S) e scrittura del
// firmware nella partizione OTA libera. Lo usa solo l'aggiornamento (ui_aggiorna.cpp, ota.h).
//
// Qui si usa la libreria WiFi di Arduino, che avvia anche lwIP e il suo task: dopo il primo
// uso restano occupati ~20 KB di RAM interna fino al riavvio (che segue sempre un
// aggiornamento installato). Il collegamento (linknow.cpp) usa invece il solo driver: i due
// non sono mai accesi insieme. WiFi.mode(WIFI_OFF) rilascia il driver (esp_wifi_deinit),
// così linknow può riaccenderlo a modo suo.
#include "hal.h"
#include "linknow.h"
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <mbedtls/sha256.h>
#include <atomic>

#define WIFI_TEMPO_MS 20000      // oltre, la connessione è fallita
#define SCARICA_STACK 16384
#define SCARICA_TEMPO_MS 15000   // senza byte per tanto, la rete è caduta

static bool gAcceso = false;
static bool gConnessioneChiesta = false;
static uint32_t gConnessioneDa = 0;

static bool accendi() {
  if (linknowAcceso()) return false;  // la radio del collegamento ha il driver
  if (!gAcceso) {
    WiFi.persistent(false);           // niente credenziali nella NVS del driver
    if (!WiFi.mode(WIFI_STA)) return false;
    gAcceso = true;
  }
  return true;
}

bool halWifiPresente() { return true; }

bool halWifiScansiona() {
  if (!accendi()) return false;
  WiFi.scanDelete();
  return WiFi.scanNetworks(true, false) == WIFI_SCAN_RUNNING;
}

int halWifiReti(HalRete *reti, int max) {
  if (!gAcceso) return 0;
  int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return -1;
  int k = 0;
  for (int16_t i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0 || s.length() > 32) continue;  // reti nascoste
    int8_t rssi = (int8_t)WiFi.RSSI(i);
    int j = 0;
    while (j < k && strcmp(reti[j].ssid, s.c_str()) != 0) j++;
    if (j < k) {  // stessa rete da più punti di accesso: la più forte
      if (rssi > reti[j].rssi) reti[j].rssi = rssi;
      continue;
    }
    if (k == max) continue;
    strlcpy(reti[k].ssid, s.c_str(), sizeof(reti[k].ssid));
    reti[k].rssi = rssi;
    reti[k].protetta = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    k++;
  }
  // dalla più forte (pochi elementi: inserimento)
  for (int a = 1; a < k; a++)
    for (int b = a; b > 0 && reti[b].rssi > reti[b - 1].rssi; b--) {
      HalRete t = reti[b];
      reti[b] = reti[b - 1];
      reti[b - 1] = t;
    }
  return k;
}

bool halWifiConnetti(const char *ssid, const char *pass) {
  if (!accendi()) return false;
  WiFi.disconnect(false, false);
  WiFi.begin(ssid, pass && pass[0] ? pass : nullptr);
  gConnessioneChiesta = true;
  gConnessioneDa = millis();
  return true;
}

uint8_t halWifiStato() {
  if (!gAcceso) return HAL_WIFI_SPENTO;
  if (!gConnessioneChiesta) return HAL_WIFI_ACCESO;
  wl_status_t s = WiFi.status();
  if (s == WL_CONNECTED) return HAL_WIFI_CONNESSO;
  if (s == WL_CONNECT_FAILED || s == WL_NO_SSID_AVAIL || s == WL_CONNECTION_LOST ||
      millis() - gConnessioneDa > WIFI_TEMPO_MS)
    return HAL_WIFI_FALLITO;
  return HAL_WIFI_CONNETTE;
}

// ---------------------------------------------------------------- scaricamenti

static std::atomic<int32_t> gEsito{0};
static std::atomic<bool> gOccupato{false};
static std::atomic<uint32_t> gFatti{0}, gTotale{0};
static struct {
  bool firmware;
  char url[256];
  char *buf;
  size_t max;
  uint32_t size;
  char sha[65];
} gLavoro;

void halWifiSpegni() {
  if (!gAcceso || gOccupato) return;
  WiFi.scanDelete();
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  gAcceso = false;
  gConnessioneChiesta = false;
}

// Apre la richiesta: 200 o un errore. `client` e `http` restano del chiamante.
static int32_t apri(HTTPClient &http, NetworkClientSecure *tls, NetworkClient *tcp,
                    const char *url) {
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setTimeout(SCARICA_TEMPO_MS);
  http.setConnectTimeout(SCARICA_TEMPO_MS);
  bool ok = strncmp(url, "https://", 8) == 0 ? http.begin(*tls, url) : http.begin(*tcp, url);
  if (!ok) return HAL_SCARICA_ERR_RETE;
  int codice = http.GET();
  if (codice < 0) return HAL_SCARICA_ERR_RETE;
  if (codice != HTTP_CODE_OK) return HAL_SCARICA_ERR_HTTP;
  return 0;
}

// legge fino a `n` byte dallo stream, aspettando al massimo SCARICA_TEMPO_MS senza dati
static int leggi(NetworkClient *s, uint8_t *buf, size_t n, uint32_t *ultimo) {
  size_t d = s->available();
  if (d == 0) {
    if (millis() - *ultimo > SCARICA_TEMPO_MS) return -1;
    delay(5);
    return 0;
  }
  *ultimo = millis();
  return s->read(buf, d < n ? d : n);
}

static int32_t scaricaTesto(HTTPClient &http) {
  int32_t e = 0;
  NetworkClient *s = http.getStreamPtr();
  int lung = http.getSize();  // -1 se a pezzi
  gTotale = lung > 0 ? (uint32_t)lung : 0;
  size_t n = 0;
  uint32_t ultimo = millis();
  while (http.connected() && (lung < 0 || n < (size_t)lung)) {
    if (n + 1 >= gLavoro.max) { e = HAL_SCARICA_ERR_TROPPO; break; }
    int r = leggi(s, (uint8_t *)gLavoro.buf + n, gLavoro.max - 1 - n, &ultimo);
    if (r < 0) { e = HAL_SCARICA_ERR_RETE; break; }
    n += r;
    gFatti = n;
  }
  gLavoro.buf[n] = 0;
  if (e == 0 && lung > 0 && n != (size_t)lung) e = HAL_SCARICA_ERR_RETE;
  return e ? e : (int32_t)n;
}

static int32_t scaricaFirmware(HTTPClient &http) {
  int lung = http.getSize();
  if (lung > 0 && (uint32_t)lung != gLavoro.size) return HAL_SCARICA_ERR_DIMENSIONE;
  gTotale = gLavoro.size;
  if (!Update.begin(gLavoro.size, U_FLASH)) return HAL_SCARICA_ERR_SPAZIO;
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  static uint8_t buf[2048];  // un solo scaricamento alla volta
  NetworkClient *s = http.getStreamPtr();
  uint32_t n = 0, ultimo = millis();
  int32_t e = 0;
  while (n < gLavoro.size) {
    if (!http.connected() && s->available() == 0) { e = HAL_SCARICA_ERR_RETE; break; }
    size_t resta = gLavoro.size - n;
    int r = leggi(s, buf, resta < sizeof(buf) ? resta : sizeof(buf), &ultimo);
    if (r < 0) { e = HAL_SCARICA_ERR_RETE; break; }
    if (r == 0) continue;
    mbedtls_sha256_update(&sha, buf, r);
    if (Update.write(buf, r) != (size_t)r) { e = HAL_SCARICA_ERR_SCRITTURA; break; }
    n += r;
    gFatti = n;
  }
  uint8_t dig[32];
  mbedtls_sha256_finish(&sha, dig);
  mbedtls_sha256_free(&sha);
  if (e == 0) {
    char hex[65];
    for (int i = 0; i < 32; i++) snprintf(hex + 2 * i, 3, "%02x", dig[i]);
    if (strcmp(hex, gLavoro.sha) != 0) e = HAL_SCARICA_ERR_SHA;
  }
  if (e) {
    Update.abort();
    return e;
  }
  // verifica l'immagine (intestazione e checksum dell'ESP-IDF) e la rende quella d'avvio
  if (!Update.end(true)) return HAL_SCARICA_ERR_SCRITTURA;
  return (int32_t)n;
}

static void taskScarica(void *) {
  int32_t e;
  {
    NetworkClientSecure *tls = new NetworkClientSecure();
    NetworkClient *tcp = new NetworkClient();
    tls->useBuiltinCACertBundle();
    HTTPClient http;
    e = apri(http, tls, tcp, gLavoro.url);
    if (e == 0) e = gLavoro.firmware ? scaricaFirmware(http) : scaricaTesto(http);
    http.end();
    delete tls;
    delete tcp;
  }
  gEsito = e;
  gOccupato = false;
  vTaskDelete(nullptr);
}

static bool avvia(const char *url) {
  if (gOccupato || halWifiStato() != HAL_WIFI_CONNESSO || strlen(url) >= sizeof(gLavoro.url))
    return false;
  strlcpy(gLavoro.url, url, sizeof(gLavoro.url));
  gFatti = 0;
  gTotale = 0;
  gEsito = HAL_SCARICA_IN_CORSO;
  gOccupato = true;
  // sul core 0, sotto l'audio: il loop() continua a disegnare il progresso
  if (xTaskCreatePinnedToCore(taskScarica, "scarica", SCARICA_STACK, nullptr, 1, nullptr, 0) !=
      pdPASS) {
    gOccupato = false;
    gEsito = HAL_SCARICA_ERR_RETE;
    return false;
  }
  return true;
}

bool halScaricaTesto(const char *url, char *buf, size_t max) {
  if (gOccupato || max < 2) return false;
  gLavoro.firmware = false;
  gLavoro.buf = buf;
  gLavoro.max = max;
  return avvia(url);
}

bool halScaricaFirmware(const char *url, uint32_t size, const char *sha256) {
  if (gOccupato || strlen(sha256) != 64) return false;
  gLavoro.firmware = true;
  gLavoro.size = size;
  strlcpy(gLavoro.sha, sha256, sizeof(gLavoro.sha));
  return avvia(url);
}

int32_t halScaricaEsito() { return gEsito; }

void halScaricaProgresso(uint32_t *fatti, uint32_t *totale) {
  *fatti = gFatti;
  *totale = gTotale;
}
