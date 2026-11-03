// Trasporto ESP-NOW: vedi linknow.h. Derivato da DylanPDao/TamaPoke (MIT), avviso completo
// in link.h.
#include "linknow.h"
#include <Arduino.h>
#include <esp_event.h>
#include <esp_mac.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string.h>

#define LINKNOW_LEN 250   // ESP_NOW_MAX_DATA_LEN della v1

struct Pacchetto {
  uint8_t mac[6];
  uint8_t len;
  uint8_t dati[LINKNOW_LEN];
};

static QueueHandle_t gCoda = nullptr;   // ~2 KB, creata una volta e tenuta
static bool gAcceso = false;
static uint8_t gCanale = 1;
static volatile uint32_t gPersi = 0, gTxKo = 0;
static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// nel task del WiFi: niente Serial né stato del gioco, solo la copia in coda
static void alRicevere(const esp_now_recv_info_t *info, const uint8_t *dati, int len) {
  if (!info || len <= 0 || len > LINKNOW_LEN) return;
  Pacchetto p;
  memcpy(p.mac, info->src_addr, 6);
  p.len = (uint8_t)len;
  memcpy(p.dati, dati, len);
  if (xQueueSend(gCoda, &p, 0) != pdTRUE) gPersi++;
}

static void allInvio(const esp_now_send_info_t *info, esp_now_send_status_t stato) {
  if (stato != ESP_NOW_SEND_SUCCESS) gTxKo++;
}

static bool aggiungiPeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t p = {};
  memcpy(p.peer_addr, mac, 6);
  p.channel = gCanale;
  p.ifidx = WIFI_IF_STA;
  p.encrypt = false;
  return esp_now_add_peer(&p) == ESP_OK;
}

// Solo il driver del WiFi, senza la libreria WiFi di Arduino: WiFi.mode() avvia anche
// Network (lwIP, esp_netif e il suo task degli eventi), che non si ferma più e teneva ~20 KB
// di RAM interna dopo il primo collegamento. ESP-NOW non ne ha bisogno. Il ciclo
// degli eventi predefinito (dove il driver pubblica i suoi: senza, scrive un errore per
// ognuno) vive quanto la radio; dopo restano ~3 KB del driver.
static bool gCicloNostro = false;  // il ciclo degli eventi l'ha creato linknow

static void spegniWifi() {
  esp_wifi_stop();
  esp_wifi_deinit();
  if (gCicloNostro) esp_event_loop_delete_default();
  gCicloNostro = false;
}

static bool accendiWifi(uint8_t canale) {
  esp_err_t e = esp_event_loop_create_default();
  if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) return false;  // già creato da altri: va bene
  gCicloNostro = e == ESP_OK;
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  // i buffer di trasmissione dinamici, come la libreria di Arduino senza useStaticBuffers:
  // quelli statici del sdkconfig (8 + 8) starebbero tutti nella RAM interna
  cfg.static_tx_buf_num = 0;
  cfg.dynamic_tx_buf_num = 32;
  cfg.tx_buf_type = 1;
  cfg.cache_tx_buf_num = 4;
  cfg.static_rx_buf_num = 4;
  cfg.dynamic_rx_buf_num = 32;
  cfg.nvs_enable = 0;  // niente configurazione del WiFi nella NVS del gioco
  if (esp_wifi_init(&cfg) != ESP_OK) return false;
  if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK ||
      esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK || esp_wifi_start() != ESP_OK ||
      esp_wifi_set_channel(canale, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    spegniWifi();
    return false;
  }
  return true;
}

bool linknowBegin(uint8_t canale) {
  if (gAcceso) return true;
  if (!gCoda) gCoda = xQueueCreate(LINKNOW_CODA, sizeof(Pacchetto));
  if (!gCoda) return false;
  xQueueReset(gCoda);
  gPersi = gTxKo = 0;
  gCanale = canale;
  if (!accendiWifi(canale)) return false;
  if (esp_now_init() != ESP_OK) {
    spegniWifi();
    return false;
  }
  esp_now_register_recv_cb(alRicevere);
  esp_now_register_send_cb(allInvio);
  aggiungiPeer(BROADCAST);
  gAcceso = true;
  return true;
}

void linknowEnd() {
  if (!gAcceso) return;
  esp_now_deinit();   // toglie anche i peer registrati
  spegniWifi();       // restituisce la RAM del driver
  gAcceso = false;
  xQueueReset(gCoda);
}

bool linknowAcceso() { return gAcceso; }

void linknowMac(uint8_t mac[6]) { esp_read_mac(mac, ESP_MAC_WIFI_STA); }

bool linknowSend(const uint8_t *mac, const uint8_t *dati, uint8_t len) {
  if (!gAcceso || !len || len > LINKNOW_LEN) return false;
  if (!mac) mac = BROADCAST;
  if (!aggiungiPeer(mac)) return false;
  if (esp_now_send(mac, dati, len) == ESP_OK) return true;
  gTxKo++;
  return false;
}

int linknowRicevi(uint8_t mac[6], uint8_t *dati, uint8_t max) {
  if (!gCoda) return -1;
  Pacchetto p;
  if (xQueueReceive(gCoda, &p, 0) != pdTRUE) return -1;
  memcpy(mac, p.mac, 6);
  uint8_t n = p.len < max ? p.len : max;
  memcpy(dati, p.dati, n);
  return n;
}

uint32_t linknowPersi() { return gPersi; }
uint32_t linknowTxKo() { return gTxKo; }
