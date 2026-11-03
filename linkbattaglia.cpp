// Battaglia in collegamento: vedi linkbattaglia.h. Derivato da DylanPDao/TamaPoke (MIT),
// avviso completo in linkbattaglia.h.
#include "linkbattaglia.h"
#include <string.h>
#include "pet.h"

static_assert(LB_RISULTATO_MAX <= LINK_DATI_MAX, "il RISULTATO deve stare in un pacchetto");
static_assert(5 + BATTLE_SQUADRA * RIT_BYTES <= LINK_DATI_MAX, "il TEAM deve stare in un pacchetto");
static_assert(EV_ENTRA < 0x80, "il tipo di evento ha 7 bit nel RISULTATO");

#define LB_INIZIO_LEN (8 + 2 * 2 * BATTLE_SQUADRA)
#define LB_NESSUNA 0xFF

static inline void scriviLE(uint8_t *p, uint32_t v, uint8_t n) {
  for (uint8_t i = 0; i < n; i++) p[i] = (uint8_t)(v >> (8 * i));
}
static inline uint32_t leggiLE(const uint8_t *p, uint8_t n) {
  uint32_t v = 0;
  for (uint8_t i = 0; i < n; i++) v |= (uint32_t)p[i] << (8 * i);
  return v;
}

// ---------------------------------------------------------------- funzioni pure

uint32_t lbSeme(uint32_t nonceHost, uint32_t nonceOspite) {
  return nonceHost ^ (nonceOspite * 2654435761u + 0x7F4A7C15u);
}

bool lbCombattente(Combatant &c, const PetRitratto &r, uint8_t regola) {
  PetRitratto rr = r;
  if (regola == LB_TUTTI_AL_50) rr.ageMinutes = (uint32_t)(LB_LIVELLO_COPPA - 1) * MINUTES_PER_LEVEL;
  return combattenteDaRitratto(c, rr);
}

void lbFotografa(const Battle &b, LbRisultato &r) {
  r.turno = b.turno;
  r.vincitore = b.vincitore;
  for (uint8_t k = 0; k < 2; k++) {
    r.deveCambiare[k] = b.deveCambiare[k];
    r.inCampo[k] = b.inCampo[k];
    for (uint8_t i = 0; i < BATTLE_SQUADRA; i++) {
      const bool c = i < b.membri[k];
      r.ps[k][i] = c ? battleMembro(b, k, i).ps : 0;
      r.stato[k][i] = c ? battleMembro(b, k, i).stato : 0;
    }
    for (uint8_t j = 0; j < BATTLE_MOSSE; j++) r.pp[k][j] = b.lato[k].pp[j];
  }
  r.nEventi = b.nEventi;
  memcpy(r.eventi, b.eventi, sizeof(BattleEvento) * b.nEventi);
}

// RISULTATO
//   0 2 passo           7 1 vincitore                     29 8 PP di chi è in campo (4 + 4)
//   2 2 turno           8 1 bit 0/1: deveCambiare lato 0/1  37 1 eventi n
//   4 1 0 turno, 1 cambi   9 20 per lato: in campo 1,     38 4n eventi: tipo | lato << 7,
//   5 2 scelte lato 0, 1        3 × (PS 2, stato 1)              mossa, valore 2
uint8_t lbScriviRisultato(const LbRisultato &r, uint8_t *o) {
  scriviLE(o, r.passo, 2);
  scriviLE(o + 2, r.turno, 2);
  o[4] = r.cambio;
  o[5] = r.scelta[0];
  o[6] = r.scelta[1];
  o[7] = r.vincitore;
  o[8] = (uint8_t)((r.deveCambiare[0] ? 1 : 0) | (r.deveCambiare[1] ? 2 : 0));
  uint8_t *p = o + 9;
  for (uint8_t k = 0; k < 2; k++) {
    *p++ = r.inCampo[k];
    for (uint8_t i = 0; i < BATTLE_SQUADRA; i++) {
      scriviLE(p, r.ps[k][i], 2);
      p[2] = r.stato[k][i];
      p += 3;
    }
  }
  for (uint8_t k = 0; k < 2; k++)
    for (uint8_t j = 0; j < BATTLE_MOSSE; j++) *p++ = r.pp[k][j];
  const uint8_t n = r.nEventi > BATTLE_MAX_EVENTI ? BATTLE_MAX_EVENTI : r.nEventi;
  *p++ = n;
  for (uint8_t i = 0; i < n; i++) {
    const BattleEvento &e = r.eventi[i];
    p[0] = (uint8_t)(e.tipo | (e.lato & 1) << 7);
    p[1] = e.mossa;
    scriviLE(p + 2, e.valore, 2);
    p += 4;
  }
  return (uint8_t)(p - o);
}

bool lbLeggiRisultato(const uint8_t *d, uint8_t len, LbRisultato &r) {
  if (len < LB_RISULTATO_FISSO) return false;
  const uint8_t n = d[LB_RISULTATO_FISSO - 1];
  if (n > BATTLE_MAX_EVENTI || len != LB_RISULTATO_FISSO + 4 * n) return false;
  if (d[4] > 1 || (d[7] > BATTLE_PAREGGIO && d[7] != BATTLE_IN_CORSO)) return false;
  r.passo = (uint16_t)leggiLE(d, 2);
  r.turno = (uint16_t)leggiLE(d + 2, 2);
  r.cambio = d[4];
  r.scelta[0] = d[5];
  r.scelta[1] = d[6];
  r.vincitore = d[7];
  r.deveCambiare[0] = d[8] & 1;
  r.deveCambiare[1] = (d[8] & 2) != 0;
  const uint8_t *p = d + 9;
  for (uint8_t k = 0; k < 2; k++) {
    r.inCampo[k] = *p++;
    if (r.inCampo[k] >= BATTLE_SQUADRA) return false;
    for (uint8_t i = 0; i < BATTLE_SQUADRA; i++) {
      r.ps[k][i] = (uint16_t)leggiLE(p, 2);
      r.stato[k][i] = p[2];
      p += 3;
    }
  }
  for (uint8_t k = 0; k < 2; k++)
    for (uint8_t j = 0; j < BATTLE_MOSSE; j++) r.pp[k][j] = *p++;
  r.nEventi = *p++;
  for (uint8_t i = 0; i < n; i++) {
    BattleEvento &e = r.eventi[i];
    e.tipo = p[0] & 0x7F;
    e.lato = p[0] >> 7;
    e.mossa = p[1];
    e.valore = (uint16_t)leggiLE(p + 2, 2);
    p += 4;
  }
  return true;
}

bool lbApplica(Battle &b, const LbRisultato &r) {
  bool diverso = false;
  for (uint8_t k = 0; k < 2; k++) {
    const uint8_t ic = r.inCampo[k] < b.membri[k] ? r.inCampo[k] : 0;
    if (b.inCampo[k] != ic) {
      b.squadra[k][b.inCampo[k]] = b.lato[k];
      b.lato[k] = b.squadra[k][ic];
      b.inCampo[k] = ic;
      diverso = true;
    }
    for (uint8_t i = 0; i < b.membri[k]; i++) {
      Combatant &c = i == ic ? b.lato[k] : b.squadra[k][i];
      const uint16_t ps = r.ps[k][i] > c.psMax ? c.psMax : r.ps[k][i];
      if (c.ps != ps || c.stato != r.stato[k][i]) {
        c.ps = ps;
        c.stato = r.stato[k][i];
        diverso = true;
      }
    }
    for (uint8_t j = 0; j < BATTLE_MOSSE; j++)
      if (b.lato[k].pp[j] != r.pp[k][j]) {
        b.lato[k].pp[j] = r.pp[k][j];
        diverso = true;
      }
    if (b.deveCambiare[k] != r.deveCambiare[k]) {
      b.deveCambiare[k] = r.deveCambiare[k];
      diverso = true;
    }
  }
  if (b.vincitore != r.vincitore || b.turno != r.turno) diverso = true;
  b.vincitore = r.vincitore;
  b.turno = r.turno;
  return diverso;
}

static bool stessiEventi(const BattleEvento *a, uint8_t na, const BattleEvento *b, uint8_t nb) {
  if (na != nb) return false;
  for (uint8_t i = 0; i < na; i++)
    if (a[i].tipo != b[i].tipo || a[i].lato != b[i].lato || a[i].mossa != b[i].mossa ||
        a[i].valore != b[i].valore)
      return false;
  return true;
}

// ---------------------------------------------------------------- LinkBattaglia

uint32_t LinkBattaglia::caso() {
  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  return rng_;
}

void LinkBattaglia::avvia(Link &lk, uint32_t seme) {
  lk_ = &lk;
  const uint32_t mio = lk.config().trainerId, suo = lk.peer().trainerId;
  host_ = mio != suo ? mio > suo : memcmp(lk.config().mac, lk.peer().mac, 6) > 0;
  rng_ = seme ? seme : 0x2545F491u;
  uscitaN_ = uscitaTesta_ = 0;
  desync_ = scartati_ = persi_ = 0;
  libero(LBM_NESSUNO);
}

void LinkBattaglia::ferma() {
  uscitaN_ = uscitaTesta_ = 0;
  libero(LBM_NESSUNO);
}

void LinkBattaglia::libero(LbMotivo m) {
  fase_ = LBF_LIBERO;
  motivo_ = m;
  invitato_ = false;
  nSq_[0] = nSq_[1] = 0;
  inizioRicevuto_ = false;
  scelta_[0] = scelta_[1] = LB_NESSUNA;
  rivMia_ = rivAltro_ = false;
  fineMandata_ = fineRicevuta_ = false;
  vincitore_ = BATTLE_IN_CORSO;
}

void LinkBattaglia::nuovaSquadra() {
  libero(LBM_NESSUNO);
  fase_ = LBF_SQUADRA;
  passo_ = 0;
  coda_ = BattleCoda();
}

void LinkBattaglia::manda(uint8_t tipo, const uint8_t *dati, uint8_t len) {
  static const uint8_t vuoto[1] = {0};
  if (!lk_) return;
  if (!dati) dati = vuoto;
  if (!uscitaN_ && lk_->invia(tipo, dati, len)) return;
  if (uscitaN_ >= LB_USCITA) {
    persi_++;
    return;
  }
  LinkMessaggio &m = uscita_[(uscitaTesta_ + uscitaN_) % LB_USCITA];
  m.tipo = tipo;
  m.len = len;
  memcpy(m.dati, dati, len);
  uscitaN_++;
}

void LinkBattaglia::tick() {
  while (uscitaN_ && lk_) {
    const LinkMessaggio &m = uscita_[uscitaTesta_];
    if (!lk_->invia(m.tipo, m.dati, m.len)) break;
    uscitaTesta_ = (uint8_t)((uscitaTesta_ + 1) % LB_USCITA);
    uscitaN_--;
  }
}

void LinkBattaglia::errore() {
  const uint8_t d = LBM_ERRORE;
  manda(LB_ESCI, &d, 1);
  libero(LBM_ERRORE);
}

// ---------------------------------------------------------------- invito e squadre

bool LinkBattaglia::invita() {
  if (host_ || fase_ != LBF_LIBERO) return false;
  manda(LB_INVITO, nullptr, 0);
  fase_ = LBF_INVITO;
  motivo_ = LBM_NESSUNO;
  return true;
}

bool LinkBattaglia::proponi(uint8_t regola) {
  if (!host_ || fase_ != LBF_LIBERO || regola > LB_TUTTI_AL_50) return false;
  regola_ = regola;
  manda(LB_PROPOSTA, &regola, 1);
  fase_ = LBF_PROPOSTA;
  motivo_ = LBM_NESSUNO;
  invitato_ = false;
  return true;
}

bool LinkBattaglia::rispondi(bool si) {
  if (host_ || fase_ != LBF_PROPOSTA) return false;
  const uint8_t d = si ? 1 : 0;
  manda(LB_RISPOSTA, &d, 1);
  if (si) nuovaSquadra();
  else libero(LBM_NESSUNO);
  return true;
}

bool LinkBattaglia::squadra(const PetRitratto *r, uint8_t n) {
  const uint8_t io = mioLato();
  if (fase_ != LBF_SQUADRA || nSq_[io] || n < 1 || n > BATTLE_SQUADRA) return false;
  Combatant c;
  for (uint8_t i = 0; i < n; i++)
    if (!lbCombattente(c, r[i], regola_)) return false;
  uint8_t d[5 + BATTLE_SQUADRA * RIT_BYTES];
  nonce_[io] = caso();
  scriviLE(d, nonce_[io], 4);
  d[4] = n;
  for (uint8_t i = 0; i < n; i++) {
    sq_[io][i] = r[i];
    ritrattoScrivi(r[i], d + 5 + i * RIT_BYTES, RIT_BYTES);
  }
  nSq_[io] = n;
  manda(LB_TEAM, d, (uint8_t)(5 + n * RIT_BYTES));
  forseInizia();
  return true;
}

void LinkBattaglia::iniziaBattaglia() {
  Combatant c[2][BATTLE_SQUADRA];
  for (uint8_t k = 0; k < 2; k++)
    for (uint8_t i = 0; i < nSq_[k]; i++) lbCombattente(c[k][i], sq_[k][i], regola_);
  battleIniziaSquadre(bt_, c[0], nSq_[0], c[1], nSq_[1], lbSeme(nonce_[0], nonce_[1]));
  passo_ = 0;
  scelta_[0] = scelta_[1] = LB_NESSUNA;
  vincitore_ = BATTLE_IN_CORSO;
  fase_ = LBF_SCELTA;
}

void LinkBattaglia::forseInizia() {
  if (fase_ != LBF_SQUADRA || !nSq_[0] || !nSq_[1]) return;
  if (host_) {
    iniziaBattaglia();
    uint8_t d[LB_INIZIO_LEN] = {0};
    scriviLE(d, lbSeme(nonce_[0], nonce_[1]), 4);
    d[4] = regola_;
    d[5] = nSq_[0];
    d[6] = nSq_[1];
    for (uint8_t k = 0; k < 2; k++)
      for (uint8_t i = 0; i < nSq_[k]; i++)
        scriviLE(d + 8 + (k * BATTLE_SQUADRA + i) * 2, bt_.squadra[k][i].psMax, 2);
    manda(LB_INIZIO, d, LB_INIZIO_LEN);
    return;
  }
  if (!inizioRicevuto_) return;
  iniziaBattaglia();
  // le due schede devono aver costruito la stessa battaglia
  bool ok = semeInizio_ == lbSeme(nonce_[0], nonce_[1]);
  for (uint8_t k = 0; k < 2; k++)
    for (uint8_t i = 0; i < BATTLE_SQUADRA; i++)
      ok &= psInizio_[k][i] == (i < nSq_[k] ? bt_.squadra[k][i].psMax : 0);
  if (!ok) errore();
}

// ---------------------------------------------------------------- turni

bool LinkBattaglia::devoScegliere() const {
  const uint8_t io = mioLato();
  if (scelta_[io] != LB_NESSUNA) return false;
  if (fase_ == LBF_SCELTA) return true;
  return fase_ == LBF_CAMBIO && bt_.deveCambiare[io];
}

bool LinkBattaglia::scegli(uint8_t s) {
  const uint8_t io = mioLato();
  if (!devoScegliere()) return false;
  if (s >= BATTLE_CAMBIA) {
    if (!battlePuoCambiare(bt_, io, (uint8_t)(s - BATTLE_CAMBIA))) return false;
  } else if (fase_ == LBF_CAMBIO || s >= BATTLE_MOSSE) {
    return false;
  }
  scelta_[io] = s;
  if (host_) {
    prova();
  } else {
    uint8_t d[3];
    scriviLE(d, passo_, 2);
    d[2] = s;
    manda(LB_AZIONE, d, 3);
  }
  return true;
}

// host: risolve il passo quando ha tutte le scelte che servono
void LinkBattaglia::prova() {
  if (fase_ == LBF_SCELTA) {
    if (scelta_[0] != LB_NESSUNA && scelta_[1] != LB_NESSUNA) risolvi();
  } else if (fase_ == LBF_CAMBIO) {
    if ((!bt_.deveCambiare[0] || scelta_[0] != LB_NESSUNA) &&
        (!bt_.deveCambiare[1] || scelta_[1] != LB_NESSUNA))
      risolvi();
  }
}

void LinkBattaglia::risolvi() {
  LbRisultato r;
  r.passo = passo_;
  if (fase_ == LBF_SCELTA) {
    r.scelta[0] = scelta_[0];
    r.scelta[1] = scelta_[1];
    battleTurno(bt_, scelta_[0], scelta_[1]);
    lbFotografa(bt_, r);
  } else {
    r.cambio = 1;
    uint8_t n = 0;
    BattleEvento ev[2];
    for (uint8_t k = 0; k < 2; k++) {
      r.scelta[k] = LB_NESSUNA;
      if (!bt_.deveCambiare[k]) continue;
      uint8_t i = (uint8_t)(scelta_[k] - BATTLE_CAMBIA);
      if (!battleCambioForzato(bt_, k, i)) {
        i = battleSceltaSostituto(bt_, k, false);
        battleCambioForzato(bt_, k, i);
      }
      r.scelta[k] = i;
      if (bt_.nEventi) ev[n++] = bt_.eventi[0];
    }
    lbFotografa(bt_, r);
    r.nEventi = n;
    memcpy(r.eventi, ev, sizeof(BattleEvento) * n);
  }
  bt_.coda = BattleCoda();  // la narrazione la tiene coda_
  uint8_t d[LB_RISULTATO_MAX];
  manda(LB_RISULTATO, d, lbScriviRisultato(r, d));
  dopoPasso(r);
}

void LinkBattaglia::dopoPasso(const LbRisultato &r) {
  for (uint8_t i = 0; i < r.nEventi; i++) coda_.metti(r.eventi[i]);
  passo_++;
  scelta_[0] = scelta_[1] = LB_NESSUNA;
  if (bt_.vincitore != BATTLE_IN_CORSO) finisci(LBM_KO, bt_.vincitore);
  else fase_ = (bt_.deveCambiare[0] || bt_.deveCambiare[1]) ? LBF_CAMBIO : LBF_SCELTA;
}

// ospite: ripete il passo sulla sua copia, poi vale ciò che dice l'host
void LinkBattaglia::riceviRisultato(const uint8_t *d, uint8_t len) {
  LbRisultato r;
  if (!lbLeggiRisultato(d, len, r)) {
    errore();
    return;
  }
  const bool dopoResa = fase_ == LBF_FINE && motivo_ == LBM_RESA;
  const bool atteso = r.cambio ? fase_ == LBF_CAMBIO : fase_ == LBF_SCELTA;
  if (r.passo != passo_ || !(atteso || dopoResa)) {
    scartati_++;
    return;
  }
  bool uguali;
  if (!r.cambio) {
    battleTurno(bt_, r.scelta[0], r.scelta[1]);
    uguali = stessiEventi(bt_.eventi, bt_.nEventi, r.eventi, r.nEventi);
  } else {
    BattleEvento ev[2];
    uint8_t n = 0;
    for (uint8_t k = 0; k < 2; k++)
      if (r.scelta[k] != LB_NESSUNA && battleCambioForzato(bt_, k, r.scelta[k]) && bt_.nEventi)
        ev[n++] = bt_.eventi[0];
    uguali = stessiEventi(ev, n, r.eventi, r.nEventi);
  }
  bt_.coda = BattleCoda();
  if (lbApplica(bt_, r) || !uguali) desync_++;
  if (dopoResa) {
    // la resa è partita mentre l'host risolveva: si tiene il suo stato, non la narrazione
    passo_++;
    return;
  }
  dopoPasso(r);
}

void LinkBattaglia::finisci(LbMotivo m, uint8_t vincitore) {
  fase_ = LBF_FINE;
  motivo_ = m;
  vincitore_ = vincitore;
  if (fineMandata_) return;
  fineMandata_ = true;
  const uint8_t d[2] = {m, vincitore};
  manda(LB_FINE, d, 2);
}

void LinkBattaglia::riceviFine(const uint8_t *d, uint8_t len) {
  if (len < 2 || d[0] < LBM_KO || d[0] > LBM_RESA ||
      (d[1] > BATTLE_PAREGGIO && d[1] != BATTLE_IN_CORSO)) {
    scartati_++;
    return;
  }
  if (fase_ == LBF_SCELTA || fase_ == LBF_CAMBIO) {
    fineRicevuta_ = true;
    finisci((LbMotivo)d[0], d[1]);   // la resa dell'altro: rispondo con lo stesso esito
  } else if (fase_ == LBF_FINE && !fineRicevuta_) {
    fineRicevuta_ = true;
    if (!host_) {   // vale l'host (una resa incrociata con l'ultimo turno)
      motivo_ = (LbMotivo)d[0];
      vincitore_ = d[1];
    }
  } else {
    scartati_++;
  }
}

bool LinkBattaglia::arrenditi() {
  if (fase_ != LBF_SCELTA && fase_ != LBF_CAMBIO) return false;
  finisci(LBM_RESA, (uint8_t)(1 - mioLato()));
  return true;
}

bool LinkBattaglia::rivincita() {
  if (fase_ != LBF_FINE || rivMia_) return false;
  rivMia_ = true;
  manda(LB_RIVINCITA, nullptr, 0);
  if (rivAltro_) nuovaSquadra();
  return true;
}

void LinkBattaglia::esci() {
  if (fase_ == LBF_LIBERO && !invitato_) return;
  arrenditi();
  const uint8_t d = LBM_USCITA;
  manda(LB_ESCI, &d, 1);
  libero(LBM_NESSUNO);
}

// ---------------------------------------------------------------- ricezione

bool LinkBattaglia::ricevi(const LinkMessaggio &m) {
  if (m.tipo < LT_BATTAGLIA || m.tipo >= LT_SCAMBIO) return false;
  const uint8_t *d = m.dati;
  const uint8_t len = m.len;
  const uint8_t altro = (uint8_t)(1 - mioLato());
  switch (m.tipo) {
    case LB_INVITO:
      if (host_ && fase_ == LBF_LIBERO) invitato_ = true;
      else scartati_++;
      break;
    case LB_PROPOSTA:
      if (!host_ && (fase_ == LBF_LIBERO || fase_ == LBF_INVITO) && len >= 1 &&
          d[0] <= LB_TUTTI_AL_50) {
        regola_ = d[0];
        fase_ = LBF_PROPOSTA;
        motivo_ = LBM_NESSUNO;
      } else {
        scartati_++;
      }
      break;
    case LB_RISPOSTA:
      if (host_ && fase_ == LBF_PROPOSTA && len >= 1) {
        if (d[0]) nuovaSquadra();
        else libero(LBM_RIFIUTO);
      } else {
        scartati_++;
      }
      break;
    case LB_TEAM: {
      if (fase_ != LBF_SQUADRA || nSq_[altro]) {
        scartati_++;
        break;
      }
      const uint8_t n = len >= 5 ? d[4] : 0;
      bool ok = n >= 1 && n <= BATTLE_SQUADRA && len >= 5 + n * RIT_BYTES;
      Combatant c;
      for (uint8_t i = 0; ok && i < n; i++)
        ok = ritrattoLeggi(d + 5 + i * RIT_BYTES, RIT_BYTES, &sq_[altro][i]) &&
             lbCombattente(c, sq_[altro][i], regola_);
      if (!ok) {
        errore();
        break;
      }
      nonce_[altro] = leggiLE(d, 4);
      nSq_[altro] = n;
      forseInizia();
      break;
    }
    case LB_INIZIO:
      if (host_ || fase_ != LBF_SQUADRA || inizioRicevuto_ || len < LB_INIZIO_LEN) {
        scartati_++;
        break;
      }
      if (d[4] != regola_ || d[5] != nSq_[0] || d[6] != nSq_[1]) {
        errore();
        break;
      }
      semeInizio_ = leggiLE(d, 4);
      for (uint8_t k = 0; k < 2; k++)
        for (uint8_t i = 0; i < BATTLE_SQUADRA; i++)
          psInizio_[k][i] = (uint16_t)leggiLE(d + 8 + (k * BATTLE_SQUADRA + i) * 2, 2);
      inizioRicevuto_ = true;
      forseInizia();
      break;
    case LB_AZIONE: {
      const bool serve = fase_ == LBF_SCELTA || (fase_ == LBF_CAMBIO && bt_.deveCambiare[1]);
      if (!host_ || !serve || len < 3 || leggiLE(d, 2) != passo_ || scelta_[1] != LB_NESSUNA) {
        scartati_++;
        break;
      }
      scelta_[1] = d[2];
      prova();
      break;
    }
    case LB_RISULTATO:
      if (host_) scartati_++;
      else riceviRisultato(d, len);
      break;
    case LB_FINE:
      riceviFine(d, len);
      break;
    case LB_RIVINCITA:
      if (fase_ != LBF_FINE || rivAltro_) {
        scartati_++;
        break;
      }
      rivAltro_ = true;
      if (rivMia_) nuovaSquadra();
      break;
    case LB_ESCI:
      if (fase_ != LBF_LIBERO || invitato_)
        libero(len >= 1 && d[0] == LBM_ERRORE ? LBM_ERRORE : LBM_USCITA);
      else scartati_++;
      break;
    default:
      scartati_++;
  }
  return true;
}
