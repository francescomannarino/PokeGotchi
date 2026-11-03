#pragma once
// Battaglia in collegamento: invito e regola Coppa, squadre da 1 a 3
// (pacchetto TEAM), turni host-autoritativi, cambi forzati, fine (END) e rivincita
// (REMATCH), sopra i pacchetti affidabili di `Link` (fascia LT_BATTAGLIA). Niente UI né
// hardware: la UI chiama i metodi e legge battaglia() e coda().
//
// Derivato da DylanPDao/TamaPoke (MIT): il modello host-autoritativo con l'azione
// dell'ospite numerata e il risultato completo del turno.
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
// MODELLO
//   Fa da host il trainerId più alto (a parità, il MAC più alto): nella Battle è il lato 0,
//   l'ospite il lato 1, su tutte e due le schede. Il seme dell'RNG è concordato: ognuno lo
//   mette nel suo TEAM (nonce) e il seme è lbSeme(nonce dell'host, nonce dell'ospite).
//   L'host risolve ogni passo con il motore di battle.h e manda RISULTATO dopo il turno
//   intero (danni di fine turno compresi): scelte, PS e stati di tutti, chi è in campo, PP,
//   cambi forzati richiesti, vincitore e gli eventi della narrazione nell'ordine reale.
//   L'ospite tiene una copia della Battle e ripete lo stesso passo con le stesse scelte
//   (stesso seme, stessi dati: l'hash di Link lo garantisce), così ha anche ciò che il
//   RISULTATO non porta (stadi, mosse dopo Trasformazione, caricamenti…). Poi confronta:
//   se qualcosa differisce vale l'host (PS, stati, in campo, PP, vincitore) e desync()
//   conta il caso. La narrazione dell'ospite viene sempre dagli eventi del RISULTATO.
//
// FASI (fase())
//   LIBERO   nessuna battaglia. L'ospite può invita(); l'host vede invitato() e
//            proponi(regola) (anche senza invito).
//   INVITO   ospite: ha invitato, aspetta la proposta.
//   PROPOSTA host: aspetta la risposta. Ospite: regola() a schermo, poi rispondi(sì/no).
//   SQUADRA  ognuno manda squadra(ritratti, 1..3), una volta. Con le due squadre l'host
//            manda INIZIO; da lì battaglia() è valida su entrambe le parti.
//   SCELTA   un turno: scegli(slot 0..3 | BATTLE_CAMBIA + i). Se
//            !battleDeveScegliere(battaglia().lato[mioLato()]) va bene una scelta qualsiasi.
//   CAMBIO   cambio forzato dopo un KO: sceglie solo chi ha battaglia().deveCambiare[lato],
//            con scegli(BATTLE_CAMBIA + i); non è un turno e non usa l'RNG.
//   FINE     vincitore() e motivo() (KO o RESA); rivincita() o esci().
//   Si legge devoScegliere() per sapere se tocca a noi; arrenditi() in SCELTA e CAMBIO è
//   la resa (la FUGA del collegamento); esci() in ogni fase torna a LIBERO e lo dice
//   all'altro (motivo() USCITA). Una squadra o un pacchetto non validi: LIBERO, ERRORE.
//
// FINE E RIVINCITA
//   END: chi arriva alla fine (ultimo RISULTATO, o resa) manda FINE {motivo, vincitore};
//   l'altro risponde col suo. fineConfermata() quando si è mandato e ricevuto: è il
//   momento delle ricompense. Se i due differiscono vale l'host.
//   REMATCH: rivincita() da entrambi riporta a SQUADRA con la stessa regola e un seme nuovo.
//   La ritrasmissione è quella di Link: ogni pacchetto qui è affidabile e arriva in ordine.
//
// PACCHETTI (corpo; interi little-endian)
//   INVITO     -
//   PROPOSTA   regola (LbRegola)
//   RISPOSTA   1 sì / 0 no
//   TEAM       nonce 4, n 1, n ritratti da RIT_BYTES (ritratto.h)
//   INIZIO     seme 4, regola 1, membri 2 (host, ospite), psMax 2 × 3 × 2 (0 = nessuno)
//   AZIONE     passo 2, scelta 1
//   RISULTATO  vedi lbScriviRisultato (al massimo LB_RISULTATO_MAX byte)
//   FINE       motivo 1, vincitore 1
//   RIVINCITA  -
//   ESCI       motivo 1 (USCITA o ERRORE)
//
// USO (dal loop, con il Link in PRONTO)
//   lb.avvia(lk, random());
//   while (lk.prendi(m)) if (!lb.ricevi(m)) { ...scambio... }
//   lb.tick();
//   while (lb.coda().prendi(e)) narra(e);   // lato 0 = host: mioLato() dice chi sono io
#include <stdint.h>
#include "battle.h"
#include "link.h"
#include "ritratto.h"

#define LB_LIVELLO_COPPA 50
#define LB_USCITA 4                   // pacchetti in attesa che Link li prenda
#define LB_RISULTATO_FISSO 38
#define LB_RISULTATO_MAX (LB_RISULTATO_FISSO + 4 * BATTLE_MAX_EVENTI)

enum LbTipo : uint8_t {
  LB_INVITO = LT_BATTAGLIA,  // ospite → host
  LB_PROPOSTA,               // host → ospite
  LB_RISPOSTA,               // ospite → host
  LB_TEAM,                   // tutti e due
  LB_INIZIO,                 // host → ospite
  LB_AZIONE,                 // ospite → host
  LB_RISULTATO,              // host → ospite
  LB_FINE,                   // END, tutti e due
  LB_RIVINCITA,              // REMATCH, tutti e due
  LB_ESCI,                   // tutti e due
};

// Regola Coppa: la sceglie l'host, l'ospite la vede prima di accettare. Tutta la squadra.
enum LbRegola : uint8_t {
  LB_LIVELLO_LIBERO = 0,
  LB_TUTTI_AL_50 = 1,        // come Pokémon Stadium; mosse: le ultime 4 fino al 50
};
#define LB_REGOLA_PREDEFINITA LB_TUTTI_AL_50  // decisione aperta 6

enum LbFase : uint8_t {
  LBF_LIBERO = 0,
  LBF_INVITO,
  LBF_PROPOSTA,
  LBF_SQUADRA,
  LBF_SCELTA,
  LBF_CAMBIO,
  LBF_FINE,
};

enum LbMotivo : uint8_t {
  LBM_NESSUNO = 0,
  LBM_RIFIUTO,   // l'ospite ha detto no alla proposta
  LBM_KO,        // una squadra è tutta KO (o pareggio)
  LBM_RESA,      // qualcuno si è arreso
  LBM_USCITA,    // l'altro ha lasciato la lotta
  LBM_ERRORE,    // squadra o pacchetto non validi, dati diversi tra le due schede
};

// Il risultato di un passo: un turno, oppure i cambi forzati.
struct LbRisultato {
  uint16_t passo = 0, turno = 0;
  uint8_t cambio = 0;                  // 0 turno, 1 cambi forzati
  uint8_t scelta[2] = {0, 0};          // turno: le scelte; cambio: il membro, 0xFF = nessuno
  uint8_t vincitore = BATTLE_IN_CORSO;
  bool deveCambiare[2] = {false, false};
  uint8_t inCampo[2] = {0, 0};
  uint16_t ps[2][BATTLE_SQUADRA] = {};
  uint8_t stato[2][BATTLE_SQUADRA] = {};
  uint8_t pp[2][BATTLE_MOSSE] = {};    // di chi è in campo
  uint8_t nEventi = 0;
  BattleEvento eventi[BATTLE_MAX_EVENTI];
};

// Lo stato di `b` dopo un passo (eventi compresi: quelli dell'ultimo passo in b.eventi).
void lbFotografa(const Battle &b, LbRisultato &r);
// Codifica in `out` (almeno LB_RISULTATO_MAX byte); restituisce la lunghezza.
uint8_t lbScriviRisultato(const LbRisultato &r, uint8_t *out);
bool lbLeggiRisultato(const uint8_t *dati, uint8_t len, LbRisultato &r);
// Riporta su `b` PS, stati, chi è in campo, PP, cambi richiesti e vincitore di `r`;
// true se qualcosa era diverso.
bool lbApplica(Battle &b, const LbRisultato &r);
// Il combattente di un ritratto con la regola Coppa. false se il ritratto non va.
bool lbCombattente(Combatant &c, const PetRitratto &r, uint8_t regola);
uint32_t lbSeme(uint32_t nonceHost, uint32_t nonceOspite);

class LinkBattaglia {
 public:
  void avvia(Link &lk, uint32_t seme);   // con lk in PRONTO; fase LIBERO
  void ferma();                          // LIBERO senza dire niente (collegamento perso)
  bool ricevi(const LinkMessaggio &m);   // false: non è un pacchetto di battaglia
  void tick();                           // rispedisce ciò che Link non ha ancora preso

  bool invita();                         // ospite, in LIBERO
  bool proponi(uint8_t regola);          // host, in LIBERO
  bool rispondi(bool si);                // ospite, in PROPOSTA
  bool squadra(const PetRitratto *r, uint8_t n);  // in SQUADRA, 1..BATTLE_SQUADRA
  bool scegli(uint8_t scelta);           // in SCELTA o CAMBIO, quando devoScegliere()
  bool arrenditi();                      // in SCELTA o CAMBIO
  bool rivincita();                      // in FINE
  void esci();                           // in ogni fase tranne LIBERO

  LbFase fase() const { return fase_; }
  LbMotivo motivo() const { return motivo_; }
  bool host() const { return host_; }
  uint8_t mioLato() const { return host_ ? 0 : 1; }
  bool invitato() const { return invitato_; }           // host in LIBERO
  uint8_t regola() const { return regola_; }            // da PROPOSTA in poi
  bool squadraMandata() const { return nSq_[mioLato()] > 0; }
  bool devoScegliere() const;
  bool rivincitaMia() const { return rivMia_; }
  bool rivincitaAltro() const { return rivAltro_; }     // l'altro la vuole
  uint8_t vincitore() const { return vincitore_; }      // lato, BATTLE_PAREGGIO o IN_CORSO
  bool fineConfermata() const { return fineMandata_ && fineRicevuta_; }

  // da INIZIO in poi: la battaglia (lato 0 = host) e i ritratti delle due squadre
  const Battle &battaglia() const { return bt_; }
  uint8_t membri(uint8_t lato) const { return nSq_[lato & 1]; }
  const PetRitratto &ritratto(uint8_t lato, uint8_t i) const { return sq_[lato & 1][i % BATTLE_SQUADRA]; }
  BattleCoda &coda() { return coda_; }   // narrazione, uguale sulle due schede
  uint16_t passo() const { return passo_; }
  uint16_t desync() const { return desync_; }           // passi corretti dall'host: 0
  uint16_t scartati() const { return scartati_; }       // pacchetti fuori fase o rotti
  uint16_t persi() const { return persi_; }             // uscita piena: deve restare 0

 private:
  void manda(uint8_t tipo, const uint8_t *dati, uint8_t len);
  void errore();
  void libero(LbMotivo m);
  void nuovaSquadra();
  void forseInizia();
  void iniziaBattaglia();
  void prova();
  void risolvi();
  void dopoPasso(const LbRisultato &r);
  void finisci(LbMotivo m, uint8_t vincitore);
  void riceviRisultato(const uint8_t *d, uint8_t len);
  void riceviFine(const uint8_t *d, uint8_t len);
  uint32_t caso();

  Link *lk_ = nullptr;
  bool host_ = false;
  LbFase fase_ = LBF_LIBERO;
  LbMotivo motivo_ = LBM_NESSUNO;
  uint32_t rng_ = 1;
  bool invitato_ = false;
  uint8_t regola_ = LB_REGOLA_PREDEFINITA;
  PetRitratto sq_[2][BATTLE_SQUADRA];
  uint8_t nSq_[2] = {0, 0};
  uint32_t nonce_[2] = {0, 0};
  bool inizioRicevuto_ = false;
  uint32_t semeInizio_ = 0;
  uint16_t psInizio_[2][BATTLE_SQUADRA] = {};
  Battle bt_;
  BattleCoda coda_;
  uint16_t passo_ = 0;
  uint8_t scelta_[2] = {0xFF, 0xFF};    // del passo in corso, 0xFF = non ancora
  uint8_t vincitore_ = BATTLE_IN_CORSO;
  bool fineMandata_ = false, fineRicevuta_ = false;
  bool rivMia_ = false, rivAltro_ = false;
  uint16_t desync_ = 0, scartati_ = 0, persi_ = 0;
  LinkMessaggio uscita_[LB_USCITA];
  uint8_t uscitaTesta_ = 0, uscitaN_ = 0;
};
