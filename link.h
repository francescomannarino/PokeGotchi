#pragma once
// Protocollo di collegamento tra due PokeGotchi: pacchetti, abbinamento,
// affidabilità e compatibilità. Niente hardware: il trasporto (ESP-NOW sulla scheda,
// un canale simulato nei test) è una funzione che spedisce byte, la ricezione la chiama
// chi possiede la radio, e il tempo arriva sempre da fuori (`ora`, in ms).
//
// Derivato da DylanPDao/TamaPoke (MIT): l'idea del trasporto come puntatore a funzione,
// della ritrasmissione con jitter e della prova su un canale che perde.
// Copyright (c) 2026 Quique Tortosa
// Permission is hereby granted, free of charge, to any person obtaining a copy of this
// software and associated documentation files (the "Software"), to deal in the Software
// without restriction, including without limitation the rights to use, copy, modify,
// merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following
// conditions: The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
// CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// PACCHETTO (tutti i numeri little-endian, scritti byte per byte)
//   ofs len
//    0   2   magic 'T' 'P'
//    2   1   versione del protocollo (LINK_PROTO)
//    3   1   tipo (LinkTipo)
//    4   2   sequenza: cresce di 1 a ogni pacchetto nuovo, le ritrasmissioni la ripetono
//    6   1   lunghezza del corpo (0..LINK_DATI_MAX)
//    7   2   CRC16 CCITT-FALSE (poly 0x1021, init 0xFFFF) dei byte 0..6 e del corpo
//    9   n   corpo
//   Un pacchetto più corto, più lungo, con magic o CRC sbagliati si scarta intero.
//   Fissi PER SEMPRE, così due versioni diverse si riconoscono e lo dicono: l'intestazione,
//   LT_HELLO = 1 e i primi LINK_HELLO_FISSO byte del suo corpo (hash e versione del fw).
//
// CORPO DI HELLO (LINK_HELLO_LEN byte; quelli in più si ignorano)
//    0  4  hash dei dati (linkHashDati)        12  6  MAC a cui sono agganciato (0 = nessuno)
//    4  8  versione del firmware, ASCII + NUL  18  4  trainerId
//                                              22  2  specie (numero del Pokédex)
//                                              24 12  nome dell'allenatore, ASCII + NUL
//
// ABBINAMENTO
//   CERCA: HELLO in broadcast. Al primo HELLO valido e compatibile di un altro MAC ci si
//   aggancia (AGGANCIO) e gli si manda HELLO in unicast col suo MAC nel campo 12. Quando
//   anche l'altro nomina noi (o manda un pacchetto che solo un peer agganciato manda), si
//   passa a CONFERMA: i due schermi mostrano codice() e la specie dell'altro e il
//   giocatore chiama conferma(). Con le due conferme: PRONTO. Da AGGANCIO in poi i
//   pacchetti di ogni altro MAC si scartano: un terzo dispositivo non dirotta l'abbinamento
//   (resta possibile falsificare il MAC: ESP-NOW qui non è cifrato).
//   Hash o versione diversi: INCOMPATIBILE, con le due versioni in peer() e versioneMia().
//
// AFFIDABILITÀ
//   CONFERMA e i tipi da LT_APP in su sono affidabili: uno alla volta in volo, ritrasmessi
//   ogni LINK_RITRASMETTI_MS + 0..LINK_JITTER_MS finché arriva l'ACK con la loro sequenza,
//   consegnati una volta sola e in ordine. HELLO, PING, ACK e BYE no. Da AGGANCIO in poi c'è
//   un PING ogni LINK_PING_MS in ogni stato; dopo LINK_TIMEOUT_MS senza niente dal peer, o
//   con un pacchetto affidabile senza ACK da tanto, il collegamento è PERSO.
//
// USO
//   Link lk;
//   LinkConfig c; memcpy(c.mac, mioMac, 6); c.specie = ...; strcpy(c.nome, "ASH");
//   c.fw = FW_VERSION; c.seme = random();
//   lk.avvia(c, {ctx, inviaRadio}, millis());
//   ogni giro: lk.ricevi(mac, dati, len, millis()) per ogni pacchetto arrivato, poi
//              lk.tick(millis());
//   in CONFERMA: mostra lk.codice() e lk.peer().specie, poi lk.conferma() o lk.chiudi()
//   in PRONTO:   lk.invia(LT_BATTAGLIA + k, dati, n); while (lk.prendi(m)) ...
//   ricevi() può spedire (gli ACK): il trasporto non deve consegnare dentro invia().
#include <stddef.h>
#include <stdint.h>

#define LINK_PROTO 1
#define LINK_FRAME_MAX 250                         // ESP-NOW v1
#define LINK_HDR 9
#define LINK_DATI_MAX (LINK_FRAME_MAX - LINK_HDR)  // 241
#define LINK_FW_LEN 8
#define LINK_NOME_LEN 12
#define LINK_HELLO_FISSO 12                        // hash + fw: uguali in ogni versione
#define LINK_HELLO_LEN 36
#define LINK_CODA 4                                // affidabili in uscita e messaggi in arrivo
#define LINK_CANALE 1                              // canale WiFi fisso del trasporto (ESP-NOW)

#define LINK_HELLO_MS 500
#define LINK_PING_MS 2000
#define LINK_RITRASMETTI_MS 250
#define LINK_JITTER_MS 150
#define LINK_TIMEOUT_MS 10000     // 5 PING persi di fila
#define LINK_AGGANCIO_MS 3000     // l'altro non ci nomina: si torna a cercare
#define LINK_CERCA_MS 60000       // nessuno nei paraggi

// Tipi di pacchetto. Le fasce per E6 ed E7 si consegnano all'applicazione (prendi()).
enum LinkTipo : uint8_t {
  LT_HELLO = 1,       // fisso per sempre
  LT_PING = 2,
  LT_ACK = 3,         // corpo: la sequenza (2 byte) del pacchetto affidabile ricevuto
  LT_CONFERMA = 4,    // il giocatore ha confermato il codice
  LT_BYE = 5,         // esco: l'altro non aspetta il timeout
  LT_APP = 0x10,      // da qui in poi: affidabili e consegnati all'applicazione
  LT_BATTAGLIA = 0x10,  // 0x10..0x2F: battaglie in collegamento
  LT_SCAMBIO = 0x30,    // 0x30..0x4F: scambio di Pokémon
  LT_APP_FINE = 0x50,
};

enum LinkStato : uint8_t {
  LINK_SPENTO = 0,
  LINK_CERCA,          // HELLO in broadcast, nessun peer
  LINK_AGGANCIO,       // agganciato a un peer, aspetta che lui agganci noi
  LINK_CONFERMA,       // agganciati a vicenda: codice a schermo, aspetta le due conferme
  LINK_PRONTO,         // confermato da entrambi: invia()/prendi()
  LINK_INCOMPATIBILE,  // versione o hash diversi: «aggiorna il firmware»
  LINK_PERSO,          // vedi motivo()
};

enum LinkMotivo : uint8_t {
  LINK_OK = 0,
  LINK_NESSUNO,    // la ricerca è scaduta
  LINK_SILENZIO,   // il peer tace da LINK_TIMEOUT_MS, o non dà l'ACK
  LINK_ADDIO,      // il peer ha mandato BYE
};

struct LinkTrasporto {
  void *ctx = nullptr;
  // mac = LINK_BROADCAST per tutti; i byte valgono solo durante la chiamata
  void (*invia)(void *ctx, const uint8_t mac[6], const uint8_t *dati, uint8_t len) = nullptr;
};

extern const uint8_t LINK_BROADCAST[6];

struct LinkConfig {
  uint8_t mac[6] = {0};
  uint32_t trainerId = 0;
  uint16_t specie = 0;
  char nome[LINK_NOME_LEN + 1] = "";
  const char *fw = "";       // FW_VERSION
  uint32_t hash = 0;         // 0 = linkHashDati(); i test lo cambiano per simulare un altro fw
  uint32_t seme = 0;         // per il jitter; 0 = dal MAC
};

// Chi c'è dall'altra parte (da HELLO; con INCOMPATIBILE solo proto, hash e fw).
struct LinkPeer {
  uint8_t mac[6] = {0};
  uint8_t proto = 0;
  uint32_t hash = 0;
  char fw[LINK_FW_LEN + 1] = "";
  uint32_t trainerId = 0;
  uint16_t specie = 0;
  char nome[LINK_NOME_LEN + 1] = "";
};

struct LinkMessaggio {
  uint8_t tipo = 0;
  uint8_t len = 0;
  uint8_t dati[LINK_DATI_MAX];
};

// Un pacchetto decodificato; `dati` punta dentro il buffer di partenza.
struct LinkFrame {
  uint8_t versione, tipo;
  uint16_t seq;
  uint8_t len;
  const uint8_t *dati;
};

uint16_t linkCrc16(const uint8_t *dati, size_t len, uint16_t crc = 0xFFFF);
// Hash FNV-1a di LINK_PROTO, TYPE_CHART e MOVE_TBL, campo per campo: se differisce, le due
// schede leggerebbero diversamente la stessa battaglia.
uint32_t linkHashDati();
// Codice a 4 cifre (0..9999) dai due MAC, uguale da entrambe le parti.
uint16_t linkCodice(const uint8_t a[6], const uint8_t b[6]);
// Scrive un pacchetto in `out`; 0 se il corpo non ci sta.
uint8_t linkCodifica(uint8_t *out, size_t max, uint8_t versione, uint8_t tipo, uint16_t seq,
                     const uint8_t *dati, uint8_t len);
bool linkDecodifica(const uint8_t *buf, size_t len, LinkFrame &f);
static inline bool linkTipoAffidabile(uint8_t t) {
  return t == LT_CONFERMA || (t >= LT_APP && t < LT_APP_FINE);
}

class Link {
 public:
  void avvia(const LinkConfig &cfg, const LinkTrasporto &tr, uint32_t ora);
  void chiudi();   // BYE al peer, se c'è, e LINK_SPENTO
  void ricevi(const uint8_t mac[6], const uint8_t *dati, size_t len, uint32_t ora);
  void tick(uint32_t ora);

  void conferma();                                    // solo in CONFERMA
  bool invia(uint8_t tipo, const uint8_t *dati, uint8_t len);  // solo in PRONTO, tipo LT_APP..
  bool prendi(LinkMessaggio &m);

  LinkStato stato() const { return stato_; }
  LinkMotivo motivo() const { return motivo_; }
  bool attivo() const { return stato_ >= LINK_CERCA && stato_ <= LINK_PRONTO; }
  const LinkPeer &peer() const { return peer_; }
  uint16_t codice() const { return codice_; }          // da CONFERMA in poi
  bool confermatoIo() const { return confIo_; }
  bool confermatoLoro() const { return confLoro_; }
  const LinkConfig &config() const { return cfg_; }
  const char *versioneMia() const { return cfg_.fw; }
  uint32_t hashMio() const { return hash_; }
  uint8_t inUscita() const { return codaN_; }          // affidabili senza ACK (anche in volo)
  uint32_t scartati() const { return scartati_; }     // rotti, di altri MAC, fuori stato
  uint32_t ritrasmessi() const { return ritrasmessi_; }

 private:
  struct Uscita { uint8_t tipo, len; uint8_t dati[LINK_DATI_MAX]; };

  void spedisci(const uint8_t mac[6], uint8_t tipo, const uint8_t *dati, uint8_t len);
  void mandaHello(const uint8_t mac[6]);
  void accoda(uint8_t tipo, const uint8_t *dati, uint8_t len);
  void pompa();
  void aggancia(const uint8_t mac[6], const LinkFrame &f);
  void incompatibile(const uint8_t mac[6], uint8_t proto, const uint8_t *corpo);
  void inConferma();
  void perso(LinkMotivo m);
  void torna();   // da AGGANCIO a CERCA
  void affidabile(const LinkFrame &f);
  uint32_t jitter(uint32_t max);

  LinkConfig cfg_;
  LinkTrasporto tr_;
  LinkStato stato_ = LINK_SPENTO;
  LinkMotivo motivo_ = LINK_OK;
  LinkPeer peer_;
  uint32_t hash_ = 0;
  uint16_t codice_ = 0;
  bool confIo_ = false, confLoro_ = false;
  uint32_t rng_ = 1;
  uint16_t seqTx_ = 0;
  uint32_t ora_ = 0, inizio_ = 0, agganciatoDa_ = 0, ultimoRx_ = 0;
  uint32_t prossimoHello_ = 0, prossimoPing_ = 0;
  // affidabili in uscita: la testa è in volo, le altre aspettano il suo ACK
  Uscita coda_[LINK_CODA];
  uint8_t codaTesta_ = 0, codaN_ = 0;
  bool inVolo_ = false;
  uint16_t voloSeq_ = 0;
  uint32_t voloDa_ = 0, voloProssimo_ = 0;
  // in arrivo
  bool rxSeqValido_ = false;
  uint16_t rxUltimoSeq_ = 0;
  LinkMessaggio arrivi_[LINK_CODA];
  uint8_t arriviTesta_ = 0, arriviN_ = 0;
  uint32_t scartati_ = 0, ritrasmessi_ = 0;
};
