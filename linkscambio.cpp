#include "linkscambio.h"
#include "pet.h"
#include "savefile.h"
#include <string.h>

static_assert(4 + RIT_BYTES <= LINK_DATI_MAX, "l'OFFERTA deve stare in un pacchetto");
static_assert(LS_GIORNALE_BYTES == 127, "aggiorna il layout del giornale in linkscambio.h");

static inline void scriviLE(uint8_t *p, uint32_t v, uint8_t n) {
  for (uint8_t i = 0; i < n; i++) p[i] = (uint8_t)(v >> (8 * i));
}
static inline uint32_t leggiLE(const uint8_t *p, uint8_t n) {
  uint32_t v = 0;
  for (uint8_t i = 0; i < n; i++) v |= (uint32_t)p[i] << (8 * i);
  return v;
}

// ---------------------------------------------------------------- giornale

size_t lsScriviGiornale(const LsGiornale &g, uint8_t *o, size_t max) {
  if (max < LS_GIORNALE_BYTES) return 0;
  memset(o, 0, LS_GIORNALE_BYTES);
  o[0] = LS_GIORNALE_VERSIONE;
  o[1] = g.stato;
  scriviLE(o + 2, g.tradeId, 4);
  o[6] = (uint8_t)g.posto;
  scriviLE(o + 7, g.partner, 4);
  memcpy(o + 11, g.nome, LINK_NOME_LEN - 1);
  if (!ritrattoScrivi(g.offerto, o + 23, RIT_BYTES) ||
      !ritrattoScrivi(g.ricevuto, o + 23 + RIT_BYTES, RIT_BYTES))
    return 0;
  scriviLE(o + LS_GIORNALE_BYTES - 4, saveCrc32(o, LS_GIORNALE_BYTES - 4), 4);
  return LS_GIORNALE_BYTES;
}

bool lsLeggiGiornale(const uint8_t *d, size_t len, LsGiornale &g) {
  if (len != LS_GIORNALE_BYTES || d[0] != LS_GIORNALE_VERSIONE || d[1] != LSF_PREPARATO) return false;
  if (saveCrc32(d, len - 4) != leggiLE(d + len - 4, 4)) return false;
  LsGiornale r;
  r.stato = d[1];
  r.tradeId = leggiLE(d + 2, 4);
  r.posto = (int8_t)d[6];
  r.partner = leggiLE(d + 7, 4);
  memcpy(r.nome, d + 11, LINK_NOME_LEN - 1);
  r.nome[LINK_NOME_LEN - 1] = 0;
  if (!r.tradeId || r.posto < SCAMBIO_ATTIVO || r.posto >= COL_POSTI) return false;
  if (!ritrattoLeggi(d + 23, RIT_BYTES, &r.offerto) ||
      !ritrattoLeggi(d + 23 + RIT_BYTES, RIT_BYTES, &r.ricevuto))
    return false;
  g = r;
  return true;
}

// ---------------------------------------------------------------- LinkScambio

uint32_t LinkScambio::caso() {
  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  return rng_;
}

// CRC32 delle due offerte (nonce + ritratto), la minore per prima: uguale sulle due schede
uint32_t LinkScambio::idCorrente() const {
  if (!mia_ || !loro_) return 0;
  uint8_t a[4 + RIT_BYTES], b[4 + RIT_BYTES];
  scriviLE(a, nonceMio_, 4);
  scriviLE(b, nonceLoro_, 4);
  ritrattoScrivi(offMia_, a + 4, RIT_BYTES);
  ritrattoScrivi(offLoro_, b + 4, RIT_BYTES);
  const bool prima = memcmp(a, b, sizeof(a)) <= 0;
  uint32_t id = saveCrc32(prima ? a : b, sizeof(a));
  id = saveCrc32(prima ? b : a, sizeof(a), id);
  return id ? id : 1;
}

uint32_t LinkScambio::tradeId() const { return gOk_ ? g_.tradeId : idCorrente(); }

bool LinkScambio::applicato(uint32_t id) const {
  for (uint8_t i = 0; i < nFatti_; i++)
    if (fatti_[i] == id) return true;
  return false;
}

void LinkScambio::carica(Pet &pet) {
  pet_ = &pet;
  prefs_.end();
  prefs_.begin(LS_NS, false);
  nFatti_ = 0;
  uint8_t raw[LS_GIORNALE_BYTES > 4 * LS_FATTI ? LS_GIORNALE_BYTES : 4 * LS_FATTI];
  size_t n = prefs_.getBytesLength(LS_FATTI_KEY);
  if (n && n % 4 == 0 && n <= 4 * LS_FATTI && prefs_.getBytes(LS_FATTI_KEY, raw, n) == n)
    for (size_t i = 0; i < n; i += 4) fatti_[nFatti_++] = leggiLE(raw + i, 4);
  gOk_ = false;
  if (prefs_.isKey(LS_GIORNALE_KEY)) {
    n = prefs_.getBytesLength(LS_GIORNALE_KEY);
    gOk_ = n == LS_GIORNALE_BYTES && prefs_.getBytes(LS_GIORNALE_KEY, raw, n) == n &&
           lsLeggiGiornale(raw, n, g_);
    if (!gOk_) {
      Serial.println("scambio: giornale illeggibile, tolto");
      prefs_.remove(LS_GIORNALE_KEY);
    }
  }
  uscitaN_ = uscitaTesta_ = 0;
  if (gOk_ && pet.scambioApplicato(g_.tradeId)) {
    chiudiGiornale();  // riavvio dopo la ricezione, prima di togliere il giornale
    Serial.println("scambio: giornale di uno scambio gia' applicato, chiuso");
  }
  if (gOk_) {
    pet.scambioBlocca(g_.posto);
    fase_ = LSF_SOSPESO;
    motivo_ = LSM_NESSUNO;
  } else {
    libero(LSM_NESSUNO);
  }
}

void LinkScambio::avvia(Link &lk, uint32_t seme) {
  lk_ = &lk;
  rng_ = seme ? seme : 0x9E3779B9u;
  uscitaN_ = uscitaTesta_ = 0;
  scartati_ = persi_ = 0;
  mia_ = loro_ = false;
  accIo_ = accLoro_ = 0;
  if (!gOk_) {
    libero(LSM_NESSUNO);
  } else if (lk.peer().trainerId == g_.partner) {
    // ripresa: COMMIT vuol dire «io ho preparato»; l'altro risponde COMMIT, DONE o ANNULLA
    fase_ = LSF_PREPARATO;
    motivo_ = LSM_NESSUNO;
    mandaId(LS_COMMIT, g_.tradeId);
  } else {
    fase_ = LSF_SOSPESO;
  }
}

void LinkScambio::ferma() {
  lk_ = nullptr;
  uscitaN_ = uscitaTesta_ = 0;
  if (gOk_) fase_ = LSF_SOSPESO;
  else if (fase_ != LSF_FATTO) libero(LSM_NESSUNO);  // l'esito resta da guardare
}

void LinkScambio::libero(LsMotivo m) {
  if (pet_ && !gOk_) pet_->scambioSblocca();
  fase_ = LSF_LIBERO;
  motivo_ = m;
  mia_ = loro_ = false;
  accIo_ = accLoro_ = 0;
}

void LinkScambio::manda(uint8_t tipo, const uint8_t *dati, uint8_t len) {
  if (!lk_) return;
  if (!uscitaN_ && lk_->invia(tipo, dati, len)) return;
  if (uscitaN_ >= LS_USCITA) {
    persi_++;
    return;
  }
  LinkMessaggio &m = uscita_[(uscitaTesta_ + uscitaN_) % LS_USCITA];
  m.tipo = tipo;
  m.len = len;
  memcpy(m.dati, dati, len);
  uscitaN_++;
}

void LinkScambio::mandaId(uint8_t tipo, uint32_t id, int motivo) {
  uint8_t b[5];
  scriviLE(b, id, 4);
  b[4] = (uint8_t)(motivo < 0 ? 0 : motivo);
  manda(tipo, b, motivo < 0 ? 4 : 5);
}

void LinkScambio::tick() {
  while (uscitaN_ && lk_) {
    const LinkMessaggio &m = uscita_[uscitaTesta_];
    if (!lk_->invia(m.tipo, m.dati, m.len)) break;
    uscitaTesta_ = (uint8_t)((uscitaTesta_ + 1) % LS_USCITA);
    uscitaN_--;
  }
}

bool LinkScambio::offri(int8_t posto) {
  if (!pet_ || !lk_ || gOk_) return false;
  if (fase_ != LSF_LIBERO && fase_ != LSF_OFFERTA && fase_ != LSF_FATTO) return false;
  if (mia_) pet_->scambioSblocca();  // si cambia offerta
  if (pet_->scambioPerche(posto) != SCB_OK) {
    if (mia_) pet_->scambioBlocca(postoMio_);
    return false;
  }
  if (fase_ == LSF_FATTO) libero(LSM_NESSUNO);
  offMia_ = pet_->scambioRitratto(posto);
  postoMio_ = posto;
  nonceMio_ = caso();
  mia_ = true;
  accIo_ = 0;
  pet_->scambioBlocca(posto);
  fase_ = LSF_OFFERTA;
  motivo_ = LSM_NESSUNO;
  uint8_t b[4 + RIT_BYTES];
  scriviLE(b, nonceMio_, 4);
  ritrattoScrivi(offMia_, b + 4, RIT_BYTES);
  manda(LS_OFFERTA, b, sizeof(b));
  return true;
}

bool LinkScambio::accetta() {
  if (fase_ != LSF_OFFERTA || !mia_ || !loro_) return false;
  accIo_ = idCorrente();
  mandaId(LS_ACCETTA, accIo_);
  if (accLoro_ == accIo_) prepara();
  return true;
}

void LinkScambio::annulla() {
  if (fase_ != LSF_OFFERTA) return;
  mandaId(LS_ANNULLA, 0, LSM_ANNULLATO);  // 0: la trattativa, qualunque siano le offerte
  libero(LSM_RITIRATO);
}

void LinkScambio::prepara() {
  const uint32_t id = idCorrente();
  if (copia_ && !copia_(copiaCtx_)) {
    mandaId(LS_ANNULLA, id, LSM_COPIA);
    libero(LSM_COPIA);
    return;
  }
  LsGiornale g;
  g.stato = LSF_PREPARATO;
  g.tradeId = id;
  g.posto = postoMio_;
  g.partner = lk_->peer().trainerId;
  strncpy(g.nome, lk_->peer().nome, LINK_NOME_LEN - 1);
  g.offerto = offMia_;
  g.ricevuto = offLoro_;
  uint8_t raw[LS_GIORNALE_BYTES], letto[LS_GIORNALE_BYTES];
  const size_t n = lsScriviGiornale(g, raw, sizeof(raw));
  // riletto: se la NVS non l'ha preso davvero, niente COMMIT
  if (!n || prefs_.putBytes(LS_GIORNALE_KEY, raw, n) != n ||
      prefs_.getBytes(LS_GIORNALE_KEY, letto, n) != n || memcmp(raw, letto, n)) {
    prefs_.remove(LS_GIORNALE_KEY);
    mandaId(LS_ANNULLA, id, LSM_COPIA);
    libero(LSM_COPIA);
    return;
  }
  g_ = g;
  gOk_ = true;
  fase_ = LSF_PREPARATO;
  mandaId(LS_COMMIT, id);
}

void LinkScambio::ricordaFatto(uint32_t id) {
  if (applicato(id)) return;
  if (nFatti_ == LS_FATTI) {
    memmove(fatti_, fatti_ + 1, sizeof(fatti_[0]) * (LS_FATTI - 1));
    nFatti_--;
  }
  fatti_[nFatti_++] = id;
  uint8_t raw[4 * LS_FATTI];
  for (uint8_t i = 0; i < nFatti_; i++) scriviLE(raw + 4 * i, fatti_[i], 4);
  prefs_.putBytes(LS_FATTI_KEY, raw, 4 * nFatti_);
}

// dopo la ricezione: prima l'elenco degli applicati, poi via il giornale (un riavvio in
// mezzo lo ritrova e, con il tradeId già nel blob del pet, lo richiude)
void LinkScambio::chiudiGiornale() {
  ricordaFatto(g_.tradeId);
  prefs_.remove(LS_GIORNALE_KEY);
  gOk_ = false;
  if (pet_) pet_->scambioSblocca();
}

void LinkScambio::applica() {
  if (!pet_->riceviScambio(g_.posto, g_.offerto, g_.ricevuto, g_.tradeId, g_.partner, g_.nome)) {
    Serial.println("scambio: il pet offerto non e' piu' al suo posto");
    fase_ = LSF_SOSPESO;
    motivo_ = LSM_ERRORE;
    return;
  }
  const uint32_t id = g_.tradeId;
  ricevuto_ = pet_->scambioDaRicevere(g_.ricevuto, g_.partner, g_.nome);
  postoMio_ = g_.posto;
  chiudiGiornale();
  fase_ = LSF_FATTO;
  motivo_ = LSM_NESSUNO;
  mia_ = loro_ = false;
  accIo_ = accLoro_ = 0;
  mandaId(LS_FATTO, id);
}

void LinkScambio::riceviCommit(uint32_t id, bool done) {
  if (gOk_ && id == g_.tradeId) {
    applica();  // l'altro ha preparato (COMMIT) o applicato (DONE): tocca a noi
    return;
  }
  if (applicato(id)) {
    if (!done) mandaId(LS_FATTO, id);  // ripresa: l'altro aspetta ancora il nostro DONE
    return;
  }
  if (done) {   // DONE di uno scambio che qui non c'è più (annullato a mano)
    scartati_++;
    return;
  }
  mandaId(LS_ANNULLA, id, LSM_ANNULLATO);  // mai preparato qui: l'altro può annullare
}

void LinkScambio::riceviAnnulla(uint32_t id, uint8_t motivo) {
  const LsMotivo m = motivo == LSM_COPIA ? LSM_COPIA : LSM_ANNULLATO;
  if (gOk_) {
    if (id != g_.tradeId) {
      scartati_++;
      return;
    }
    // chi manda ANNULLA non ha mandato COMMIT: qui non si è applicato niente (salvo un
    // annullamento manuale dell'altro dopo il COMMIT: allora vale ciò che è già scritto)
    if (pet_->scambioApplicato(id)) {
      chiudiGiornale();
    } else {
      pet_->annullaScambio(g_.posto, g_.offerto, g_.ricevuto, g_.partner, g_.nome);
      prefs_.remove(LS_GIORNALE_KEY);
      gOk_ = false;
    }
    libero(m);
    return;
  }
  if (fase_ == LSF_OFFERTA && (id == 0 || id == idCorrente())) {
    libero(m);
    return;
  }
  scartati_++;
}

bool LinkScambio::annullaGiornale() {
  if (!gOk_ || !pet_) return false;
  const uint32_t id = g_.tradeId;
  if (pet_->scambioApplicato(id)) {
    ricevuto_ = pet_->scambioDaRicevere(g_.ricevuto, g_.partner, g_.nome);
    postoMio_ = g_.posto;
    chiudiGiornale();
    fase_ = LSF_FATTO;
    motivo_ = LSM_NESSUNO;
    return true;
  }
  pet_->annullaScambio(g_.posto, g_.offerto, g_.ricevuto, g_.partner, g_.nome);
  prefs_.remove(LS_GIORNALE_KEY);
  gOk_ = false;
  if (lk_) mandaId(LS_ANNULLA, id, LSM_MANUALE);  // se l'altro è ancora preparato, annulla
  libero(LSM_MANUALE);
  return true;
}

bool LinkScambio::ricevi(const LinkMessaggio &m) {
  if (m.tipo < LT_SCAMBIO || m.tipo >= LT_APP_FINE) return false;
  const uint8_t *d = m.dati;
  if (!pet_ || !lk_) {
    scartati_++;
    return true;
  }
  switch (m.tipo) {
    case LS_OFFERTA: {
      PetRitratto r;
      if (gOk_ || (fase_ != LSF_LIBERO && fase_ != LSF_OFFERTA && fase_ != LSF_FATTO) ||
          m.len < 4 + RIT_BYTES || !ritrattoLeggi(d + 4, m.len - 4, &r)) {
        scartati_++;
        break;
      }
      if (fase_ == LSF_FATTO) libero(LSM_NESSUNO);
      offLoro_ = r;
      nonceLoro_ = leggiLE(d, 4);
      loro_ = true;
      accIo_ = 0;  // offerta nuova: si riguarda prima di accettare
      fase_ = LSF_OFFERTA;
      motivo_ = LSM_NESSUNO;
      break;
    }
    case LS_ACCETTA:
      if (fase_ != LSF_OFFERTA || m.len < 4) {
        scartati_++;
        break;
      }
      accLoro_ = leggiLE(d, 4);
      if (accIo_ && accIo_ == idCorrente() && accLoro_ == accIo_) prepara();
      break;
    case LS_ANNULLA:
      if (m.len < 4) scartati_++;
      else riceviAnnulla(leggiLE(d, 4), m.len >= 5 ? d[4] : 0);
      break;
    case LS_COMMIT:
    case LS_FATTO:
      if (m.len < 4) scartati_++;
      else riceviCommit(leggiLE(d, 4), m.tipo == LS_FATTO);
      break;
    default:
      scartati_++;
  }
  return true;
}
