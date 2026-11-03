#pragma once
// Trasporto ESP-NOW del collegamento sul core Arduino-ESP32 3.x: solo datagrammi,
// le garanzie le dà il protocollo di link.h. Il gioco non lo chiama direttamente: passa da
// halRadio*() di hal.h (hal_esp32.cpp); lo usa così com'è lo sketch della seconda scheda di
// prova (tools/peer_link/).
//
// La radio è accesa solo tra linknowBegin() e linknowEnd(): il driver del WiFi in STA
// (esp_wifi_*, senza la libreria WiFi di Arduino e quindi senza lwIP), senza connettersi a
// nessuna rete, sul canale fisso; all'uscita esp_wifi_deinit(), che restituisce la RAM
// interna (~31 KB); restano ~3 KB del driver dopo il primo uso. Il callback di ricezione gira nel task del WiFi:
// copia il pacchetto in una coda FreeRTOS e basta; il loop la svuota con linknowRicevi()
// e passa i byte a Link::ricevi().
//
// Derivato da DylanPDao/TamaPoke (MIT; avviso completo in link.h): l'idea della coda tra il
// callback ESP-NOW e il loop. Il loro linknow non è mai stato provato su una scheda; questo
// riprende l'avvio di tools/prova_radio, provato tra due schede.
#include <stdint.h>

#define LINKNOW_CODA 8     // pacchetti in arrivo in attesa del loop; quelli in più si perdono

bool linknowBegin(uint8_t canale);   // false se ESP-NOW non parte (la radio resta spenta)
void linknowEnd();
bool linknowAcceso();
void linknowMac(uint8_t mac[6]);     // MAC della STA (valido anche a radio spenta)
// mac nullo o FF:FF:FF:FF:FF:FF = broadcast; il peer unicast si registra al primo invio
bool linknowSend(const uint8_t *mac, const uint8_t *dati, uint8_t len);
// il pacchetto più vecchio: lunghezza (troncata a max) e mittente; -1 se non ce n'è
int linknowRicevi(uint8_t mac[6], uint8_t *dati, uint8_t max);
// contatori dall'ultimo linknowBegin(): arrivati con la coda piena, invii rifiutati dalla
// radio o senza conferma del destinatario (solo unicast)
uint32_t linknowPersi();
uint32_t linknowTxKo();
