#pragma once
// Aggiornamento del firmware via WiFi dalle release di GitHub: la parte senza hardware
// (versioni, manifest della release, credenziali del WiFi, regole prima di installare).
// Scansione, connessione, download e scrittura nella partizione libera stanno nell'HAL
// (halWifi*, halHttp*, halOta*); la schermata è ui_aggiorna.cpp. Provato in test/test_ota.cpp.
//
// La CI pubblica in ogni release, accanto al firmware, il manifest `pokegotchi.json`:
//   {"version":"3.1.0","url":"https://github.com/<repo>/releases/download/v3.1.0/
//    pokegotchi-3.1.0-ota.bin","size":1504752,"sha256":"<64 cifre esadecimali>"}
// La scheda legge quello dell'ultima release (`/releases/latest/download/`, un redirect di
// GitHub: niente API, niente limiti di frequenza), confronta la versione con FW_VERSION e,
// se è più recente, scarica `url` controllando dimensione e SHA-256 prima di avviarlo.
#include <Arduino.h>

// Il repository delle release: la CI lo passa con -DPG_REPO="owner/nome" (github.repository).
#ifndef PG_REPO
#define PG_REPO "francescomannarino/PokeGotchi"
#endif
#define OTA_URL_MANIFEST "https://github.com/" PG_REPO "/releases/latest/download/pokegotchi.json"
#define OTA_MANIFEST_MAX 1024   // il manifest è ~250 byte; oltre non è il nostro
#define OTA_URL_MAX 256

// ---------------------------------------------------------------- versioni

// "3", "3.1", "3.1.0" e la "v" iniziale dei tag; "2.1" vale 2.1.0. Il resto dopo la terza
// cifra ("-rc1") si ignora.
struct OtaVersione {
  uint16_t maggiore = 0, minore = 0, patch = 0;
};
bool otaLeggiVersione(const char *s, OtaVersione *v);
int otaConfronta(const OtaVersione &a, const OtaVersione &b);  // <0, 0, >0 come strcmp
// `nuova` è più recente di `attuale`; false se una delle due non si legge
bool otaPiuRecente(const char *nuova, const char *attuale);

// ---------------------------------------------------------------- manifest

struct OtaManifest {
  char versione[16] = "";
  char url[OTA_URL_MAX] = "";
  uint32_t size = 0;
  char sha256[65] = "";  // minuscolo
};
// Legge il JSON del manifest (un oggetto piatto, chiavi in qualunque ordine, spazi liberi).
// false se manca un campo, la versione non si legge, l'URL non è https (http solo con
// `consentiHttp`, per le prove sulla rete di casa), la dimensione non sta in una partizione
// da 3 MB o lo SHA-256 non è di 64 cifre esadecimali.
bool otaLeggiManifest(const char *json, OtaManifest *m, bool consentiHttp = false);
#define OTA_SIZE_MAX (3UL * 1024 * 1024)

// URL del manifest: OTA_URL_MANIFEST, salvo quello impostato con otaImpostaUrl() (la console,
// solo in debug: un server sulla LAN per provare prima della release). Vale fino al riavvio.
const char *otaUrlManifest();
void otaImpostaUrl(const char *url);  // nullptr o "" = torna a quello delle release
bool otaUrlProva();                   // l'URL è quello di prova (e allora http è ammesso)

// ---------------------------------------------------------------- credenziali del WiFi

// Nel namespace NVS OTA_NS, fuori da SAVE_NAMESPACE: la password non finisce nei .pgsave
// (vedi savefile.h). Una sola rete ricordata.
#define OTA_NS "wifi"
#define OTA_SSID_MAX 32   // byte, come l'802.11
#define OTA_PASS_MAX 63   // WPA2-PSK
bool otaCredenziali(char *ssid, size_t maxSsid, char *pass, size_t maxPass);  // false se nessuna
bool otaSalvaCredenziali(const char *ssid, const char *pass);
void otaDimentica();

// ---------------------------------------------------------------- regole

// Si installa con la batteria almeno a OTA_BATTERIA_MIN % o con l'USB collegato (una
// scheda senza batteria, percentuale -1, va per forza a USB).
#define OTA_BATTERIA_MIN 30
bool otaBatteriaOk(int percento, bool usb);
