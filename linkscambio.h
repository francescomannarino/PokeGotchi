#pragma once
// Scambio di Pokémon in collegamento: offerta, conferma dei due
// giocatori e scambio atomico con giornale in NVS, sopra i pacchetti affidabili di `Link`
// (fascia LT_SCAMBIO). Niente UI né hardware: la UI chiama i metodi e legge fase() e i
// ritratti; il pet lo cambia `Pet::riceviScambio()`.
//
// INVARIANTE: ogni pet esiste in esattamente un dispositivo, anche se il collegamento cade
// o una scheda si spegne a qualunque passo. Le uniche eccezioni sono il ripristino di una
// copia e annullaGiornale(), l'ultima risorsa, con un avviso nella UI.
//
// PASSI
//   1. OFFERTA: ciascuno offre il pet attivo o un posto della collezione (offri(posto)); il
//      ritratto (ritratto.h, con il suo CRC32) viaggia con un nonce. Si può cambiare
//      offerta finché non si è preparato.
//   2. Ognuno vede il pet dell'altro (loro()) e accetta(): ACCETTA porta il tradeId, il CRC32
//      delle due offerte (nonce + ritratto) in ordine di byte, uguale sulle due schede;
//      un'accettazione di offerte vecchie ha un altro tradeId e non conta.
//   3. Con le due accettazioni: copia di sicurezza (copiaPrima, la SD di E1), poi il giornale
//      "trade" in NVS con lo stato PREPARATO, il tradeId, il posto, il partner e i due
//      ritratti, poi COMMIT. Fallita la copia o la scrittura: ANNULLA.
//   4. Al COMMIT (o al DONE) dell'altro: Pet::riceviScambio() (che salva insieme al pet il
//      tradeId applicato), il tradeId negli ultimi applicati, via il giornale, DONE.
//   5. Collegamento perso dopo PREPARATO: il giornale sopravvive al riavvio (SOSPESO, pet
//      bloccato). Alla riconnessione con lo stesso partner (trainerId) si rimanda COMMIT e
//      l'altro risponde in modo coerente: COMMIT se è preparato anche lui, DONE se l'ha già
//      applicato (è tra i suoi ultimi LS_FATTI), ANNULLA se non l'ha mai preparato.
//   Regola che tiene l'invariante: ANNULLA per uno scambio lo manda solo chi non ne ha il
//   giornale e non l'ha applicato (o annullaGiornale()); chi è PREPARATO e lo riceve sa
//   che l'altro non ha mandato COMMIT, quindi non ha applicato, e annulla anche lui.
//   Un riavvio a metà di riceviScambio() si ripara da solo: con il tradeId già nel blob del
//   pet carica() chiude il giornale, altrimenti la ricezione si rifà alla riconnessione.
//
// FASI (fase())
//   LIBERO     niente in corso: offri(), o arriva l'offerta dell'altro.
//   OFFERTA    trattativa: offertaMia()/mia(), offertaLoro()/loro(), accetta(), annulla().
//   PREPARATO  giornale scritto e COMMIT mandato: si aspetta l'altro (di solito un attimo).
//   SOSPESO    giornale senza partner: dopo un riavvio, un collegamento perso o con un altro
//              partner (giornalePartner()). Si riprende con avvia() e lo stesso partner;
//              annullaGiornale() è l'ultima risorsa.
//   FATTO      applicato: ricevuto(), postoMio(); offri() per un altro scambio.
//   motivo() dice perché si è tornati a LIBERO, o perché un SOSPESO non si applica (ERRORE).
//
// PACCHETTI (corpo; interi little-endian)
//   OFFERTA  nonce 4, ritratto RIT_BYTES      (OFFER)
//   ACCETTA  tradeId 4
//   ANNULLA  tradeId 4 (0 = la trattativa in corso), motivo 1
//   COMMIT   tradeId 4
//   FATTO    tradeId 4                         (DONE)
//
// NVS: namespace LS_NS, fuori da SAVE_NAMESPACE, quindi fuori dai .pgsave (il giornale
// descrive un'operazione in corso e non va ripristinato su un'altra scheda).
//   "trade"  il giornale (LS_GIORNALE_BYTES):
//            versione 1, stato 1, tradeId 4, posto 1 (int8), trainerId del partner 4,
//            nome del partner 12, offerto RIT_BYTES, ricevuto RIT_BYTES, CRC32 4
//   "fatti"  gli ultimi LS_FATTI tradeId applicati, 4 byte ciascuno, il più recente in fondo
//
// USO
//   all'avvio, subito dopo pet.begin():  ls.carica(pet);
//   con il Link in PRONTO:               ls.avvia(lk, random());
//   ogni giro:  while (lk.prendi(m)) if (!lb.ricevi(m)) ls.ricevi(m);  ls.tick();
//   collegamento perso o chiuso:         ls.ferma();
#include <stdint.h>
#include <Preferences.h>
#include "link.h"
#include "ritratto.h"

class Pet;

#define LS_NS "scambio"
#define LS_GIORNALE_KEY "trade"
#define LS_FATTI_KEY "fatti"
#define LS_GIORNALE_VERSIONE 1
#define LS_GIORNALE_BYTES (23 + 2 * RIT_BYTES + 4)
#define LS_FATTI 16
#define LS_USCITA 4   // pacchetti in attesa che Link li prenda

enum LsTipo : uint8_t {
  LS_OFFERTA = LT_SCAMBIO,
  LS_ACCETTA,
  LS_ANNULLA,
  LS_COMMIT,
  LS_FATTO,
};

enum LsFase : uint8_t {
  LSF_LIBERO = 0,
  LSF_OFFERTA,
  LSF_PREPARATO,
  LSF_SOSPESO,
  LSF_FATTO,
};

enum LsMotivo : uint8_t {
  LSM_NESSUNO = 0,
  LSM_ANNULLATO,  // l'altro ha annullato, o non conosce lo scambio
  LSM_RITIRATO,   // annulla() mio
  LSM_COPIA,      // copia di sicurezza o giornale non scritti (mia o dell'altro)
  LSM_ERRORE,     // il posto non contiene più il pet offerto: resta SOSPESO
  LSM_MANUALE,    // annullaGiornale()
};

// Il giornale "trade".
struct LsGiornale {
  uint8_t stato = 0;        // LSF_PREPARATO
  uint32_t tradeId = 0;
  int8_t posto = 0;         // SCAMBIO_ATTIVO o un posto della collezione
  uint32_t partner = 0;     // trainerId
  char nome[LINK_NOME_LEN] = "";
  PetRitratto offerto, ricevuto;
};
size_t lsScriviGiornale(const LsGiornale &g, uint8_t *out, size_t max);
bool lsLeggiGiornale(const uint8_t *dati, size_t len, LsGiornale &g);

class LinkScambio {
 public:
  // All'avvio: legge giornale e ultimi applicati. Un giornale già applicato (riavvio a metà)
  // si chiude subito; altrimenti blocca il pet offerto e la fase è SOSPESO.
  void carica(Pet &pet);
  void avvia(Link &lk, uint32_t seme);   // con lk in PRONTO, dopo carica()
  void ferma();                          // OFFERTA → LIBERO, PREPARATO → SOSPESO, FATTO resta
  bool ricevi(const LinkMessaggio &m);   // false: non è un pacchetto di scambio
  void tick();                           // rispedisce ciò che Link non ha ancora preso
  // la copia prima dello scambio: false annulla. Senza, nessuna copia.
  void copiaPrima(bool (*f)(void *ctx), void *ctx) { copia_ = f; copiaCtx_ = ctx; }

  bool offri(int8_t posto);   // in LIBERO, OFFERTA o FATTO, se pet.scambioPerche() == SCB_OK
  bool accetta();             // in OFFERTA, con le due offerte
  void annulla();             // in OFFERTA: ANNULLA all'altro e LIBERO
  // Ultima risorsa (in PREPARATO o SOSPESO): via il giornale e il pet offerto resta qui. Se
  // l'altro ha già applicato lo scambio, il pet esisterà in due posti e il suo si perde:
  // la UI lo dice prima. Se lo scambio risulta già applicato qui, lo chiude invece.
  bool annullaGiornale();

  LsFase fase() const { return fase_; }
  LsMotivo motivo() const { return motivo_; }
  bool offertaMia() const { return mia_; }
  bool offertaLoro() const { return loro_; }
  int8_t postoMio() const { return postoMio_; }
  const PetRitratto &mia() const { return offMia_; }
  const PetRitratto &loro() const { return offLoro_; }
  bool accettatoIo() const { return accIo_ && accIo_ == idCorrente(); }
  bool accettatoLoro() const { return accLoro_ && accLoro_ == idCorrente(); }
  uint32_t tradeId() const;              // della trattativa (0 senza le due offerte) o del giornale
  bool giornale() const { return gOk_; }
  const LsGiornale &giornaleDati() const { return g_; }  // con giornale(): partner, nome, ritratti
  const PetRitratto &ricevuto() const { return ricevuto_; }  // in FATTO, come è entrato
  bool applicato(uint32_t tradeId) const;  // tra gli ultimi LS_FATTI
  uint16_t scartati() const { return scartati_; }
  uint16_t persi() const { return persi_; }   // uscita piena: deve restare 0
  uint8_t inUscita() const { return uscitaN_; }  // pacchetti che Link non ha ancora preso

 private:
  uint32_t idCorrente() const;
  void manda(uint8_t tipo, const uint8_t *dati, uint8_t len);
  void mandaId(uint8_t tipo, uint32_t id, int motivo = -1);
  void libero(LsMotivo m);
  void prepara();
  void applica();
  void chiudiGiornale();
  void ricordaFatto(uint32_t id);
  void riceviCommit(uint32_t id, bool done);
  void riceviAnnulla(uint32_t id, uint8_t motivo);
  uint32_t caso();

  Pet *pet_ = nullptr;
  Link *lk_ = nullptr;
  Preferences prefs_;
  bool (*copia_)(void *ctx) = nullptr;
  void *copiaCtx_ = nullptr;
  LsFase fase_ = LSF_LIBERO;
  LsMotivo motivo_ = LSM_NESSUNO;
  uint32_t rng_ = 1;
  bool mia_ = false, loro_ = false;
  int8_t postoMio_ = 0;
  PetRitratto offMia_, offLoro_;
  uint32_t nonceMio_ = 0, nonceLoro_ = 0;
  uint32_t accIo_ = 0, accLoro_ = 0;
  bool gOk_ = false;
  LsGiornale g_;
  PetRitratto ricevuto_;
  uint32_t fatti_[LS_FATTI] = {};
  uint8_t nFatti_ = 0;
  uint16_t scartati_ = 0, persi_ = 0;
  LinkMessaggio uscita_[LS_USCITA];
  uint8_t uscitaTesta_ = 0, uscitaN_ = 0;
};
