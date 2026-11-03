// Protocollo di collegamento: vedi link.h. Derivato da DylanPDao/TamaPoke (MIT), avviso
// completo in link.h.
#include "link.h"
#include <string.h>
#include "moves.h"

const uint8_t LINK_BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t NESSUN_MAC[6] = {0};

// scadenze assolute di `ora`, confrontate in modo sicuro rispetto al giro del contatore
static inline bool scaduto(uint32_t ora, uint32_t t) { return (int32_t)(ora - t) >= 0; }

static inline void scriviLE(uint8_t *p, uint32_t v, uint8_t n) {
  for (uint8_t i = 0; i < n; i++) p[i] = (uint8_t)(v >> (8 * i));
}
static inline uint32_t leggiLE(const uint8_t *p, uint8_t n) {
  uint32_t v = 0;
  for (uint8_t i = 0; i < n; i++) v |= (uint32_t)p[i] << (8 * i);
  return v;
}

// stringa in un campo a lunghezza fissa, completata con NUL; in lettura il campo può
// non essere terminato
static void scriviTesto(uint8_t *p, const char *s, uint8_t n) {
  memset(p, 0, n);
  for (uint8_t i = 0; i < n && s && s[i]; i++) p[i] = (uint8_t)s[i];
}
static void leggiTesto(char *out, const uint8_t *p, uint8_t n) {
  uint8_t i = 0;
  for (; i < n && p[i]; i++) out[i] = (p[i] >= 0x20 && p[i] < 0x7F) ? (char)p[i] : '?';
  out[i] = 0;
}

// ---------------------------------------------------------------- pacchetti

uint16_t linkCrc16(const uint8_t *dati, size_t len, uint16_t crc) {
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)dati[i] << 8;
    for (uint8_t b = 0; b < 8; b++) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

static uint32_t fnv(uint32_t h, uint8_t b) { return (h ^ b) * 16777619u; }

uint32_t linkHashDati() {
  uint32_t h = 2166136261u;
  h = fnv(h, LINK_PROTO);
  h = fnv(h, TYPE_COUNT);
  for (uint8_t a = 0; a < TYPE_COUNT; a++)
    for (uint8_t d = 0; d < TYPE_COUNT; d++) h = fnv(h, TYPE_CHART[a][d]);
  h = fnv(h, MOVE_MAX_ID);
  for (uint16_t i = 0; i <= MOVE_MAX_ID; i++) {
    const MoveEntry &m = MOVE_TBL[i];
    h = fnv(h, m.type);
    h = fnv(h, m.power);
    h = fnv(h, m.accuracy);
    h = fnv(h, m.pp);
    h = fnv(h, m.effect);
    h = fnv(h, m.p1);
    h = fnv(h, (uint8_t)m.p2);
  }
  return h;
}

uint16_t linkCodice(const uint8_t a[6], const uint8_t b[6]) {
  // prima il MAC minore, così l'ordine non conta
  const uint8_t *p = memcmp(a, b, 6) <= 0 ? a : b;
  const uint8_t *q = p == a ? b : a;
  uint32_t h = 2166136261u;
  for (uint8_t i = 0; i < 6; i++) h = fnv(h, p[i]);
  for (uint8_t i = 0; i < 6; i++) h = fnv(h, q[i]);
  return (uint16_t)(h % 10000);
}

uint8_t linkCodifica(uint8_t *out, size_t max, uint8_t versione, uint8_t tipo, uint16_t seq,
                     const uint8_t *dati, uint8_t len) {
  if (len > LINK_DATI_MAX || (size_t)LINK_HDR + len > max) return 0;
  out[0] = 'T';
  out[1] = 'P';
  out[2] = versione;
  out[3] = tipo;
  scriviLE(out + 4, seq, 2);
  out[6] = len;
  if (len) memcpy(out + LINK_HDR, dati, len);
  uint16_t crc = linkCrc16(out, 7);
  crc = linkCrc16(out + LINK_HDR, len, crc);
  scriviLE(out + 7, crc, 2);
  return (uint8_t)(LINK_HDR + len);
}

bool linkDecodifica(const uint8_t *buf, size_t len, LinkFrame &f) {
  if (!buf || len < LINK_HDR || len > LINK_FRAME_MAX) return false;
  if (buf[0] != 'T' || buf[1] != 'P') return false;
  if (len != (size_t)LINK_HDR + buf[6]) return false;
  uint16_t crc = linkCrc16(buf, 7);
  crc = linkCrc16(buf + LINK_HDR, buf[6], crc);
  if (crc != (uint16_t)leggiLE(buf + 7, 2)) return false;
  f.versione = buf[2];
  f.tipo = buf[3];
  f.seq = (uint16_t)leggiLE(buf + 4, 2);
  f.len = buf[6];
  f.dati = buf + LINK_HDR;
  return true;
}

// ---------------------------------------------------------------- Link

uint32_t Link::jitter(uint32_t max) {
  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  return max ? rng_ % (max + 1) : 0;
}

void Link::avvia(const LinkConfig &cfg, const LinkTrasporto &tr, uint32_t ora) {
  *this = Link();
  cfg_ = cfg;
  cfg_.nome[LINK_NOME_LEN] = 0;
  if (!cfg_.fw) cfg_.fw = "";
  tr_ = tr;
  hash_ = cfg.hash ? cfg.hash : linkHashDati();
  rng_ = cfg.seme ? cfg.seme : (leggiLE(cfg.mac, 4) ^ (leggiLE(cfg.mac + 4, 2) << 16) ^ 0x9E3779B9u);
  if (!rng_) rng_ = 1;
  seqTx_ = (uint16_t)jitter(0xFFFF);
  ora_ = inizio_ = ora;
  stato_ = LINK_CERCA;
  prossimoHello_ = ora;   // il primo HELLO al primo tick
}

void Link::chiudi() {
  if (stato_ >= LINK_AGGANCIO && stato_ <= LINK_PRONTO) {
    // senza ACK: due copie bastano quasi sempre, altrimenti c'è il timeout
    spedisci(peer_.mac, LT_BYE, nullptr, 0);
    spedisci(peer_.mac, LT_BYE, nullptr, 0);
  }
  stato_ = LINK_SPENTO;
  inVolo_ = false;
  codaN_ = arriviN_ = 0;
}

void Link::spedisci(const uint8_t mac[6], uint8_t tipo, const uint8_t *dati, uint8_t len) {
  if (!tr_.invia) return;
  uint8_t buf[LINK_FRAME_MAX];
  uint8_t n = linkCodifica(buf, sizeof(buf), LINK_PROTO, tipo, seqTx_++, dati, len);
  if (n) tr_.invia(tr_.ctx, mac, buf, n);
}

void Link::mandaHello(const uint8_t mac[6]) {
  uint8_t b[LINK_HELLO_LEN];
  scriviLE(b, hash_, 4);
  scriviTesto(b + 4, cfg_.fw, LINK_FW_LEN);
  memcpy(b + 12, stato_ >= LINK_AGGANCIO && stato_ <= LINK_PRONTO ? peer_.mac : NESSUN_MAC, 6);
  scriviLE(b + 18, cfg_.trainerId, 4);
  scriviLE(b + 22, cfg_.specie, 2);
  scriviTesto(b + 24, cfg_.nome, LINK_NOME_LEN);
  spedisci(mac, LT_HELLO, b, sizeof(b));
}

void Link::perso(LinkMotivo m) {
  stato_ = LINK_PERSO;
  motivo_ = m;
  inVolo_ = false;
  codaN_ = 0;
}

void Link::torna() {
  stato_ = LINK_CERCA;
  peer_ = LinkPeer();
  prossimoHello_ = ora_ + jitter(LINK_HELLO_MS);
}

void Link::incompatibile(const uint8_t mac[6], uint8_t proto, const uint8_t *corpo) {
  peer_ = LinkPeer();
  memcpy(peer_.mac, mac, 6);
  peer_.proto = proto;
  peer_.hash = leggiLE(corpo, 4);
  leggiTesto(peer_.fw, corpo + 4, LINK_FW_LEN);
  stato_ = LINK_INCOMPATIBILE;
}

void Link::aggancia(const uint8_t mac[6], const LinkFrame &f) {
  peer_ = LinkPeer();
  memcpy(peer_.mac, mac, 6);
  peer_.proto = f.versione;
  peer_.hash = leggiLE(f.dati, 4);
  leggiTesto(peer_.fw, f.dati + 4, LINK_FW_LEN);
  peer_.trainerId = leggiLE(f.dati + 18, 4);
  peer_.specie = (uint16_t)leggiLE(f.dati + 22, 2);
  leggiTesto(peer_.nome, f.dati + 24, LINK_NOME_LEN);
  codice_ = linkCodice(cfg_.mac, mac);
  stato_ = LINK_AGGANCIO;
  agganciatoDa_ = ultimoRx_ = ora_;
  rxSeqValido_ = false;
  mandaHello(peer_.mac);
  prossimoHello_ = ora_ + LINK_HELLO_MS + jitter(LINK_HELLO_MS / 2);
}

void Link::inConferma() {
  stato_ = LINK_CONFERMA;
  ultimoRx_ = ora_;
  // un HELLO che ci nomina, perché l'altro passi a CONFERMA anche se ha perso i nostri
  mandaHello(peer_.mac);
  prossimoPing_ = ora_ + LINK_PING_MS;
}

void Link::conferma() {
  if (stato_ != LINK_CONFERMA || confIo_) return;
  confIo_ = true;
  accoda(LT_CONFERMA, nullptr, 0);
  if (confLoro_) stato_ = LINK_PRONTO;
}

bool Link::invia(uint8_t tipo, const uint8_t *dati, uint8_t len) {
  if (stato_ != LINK_PRONTO || tipo < LT_APP || tipo >= LT_APP_FINE || len > LINK_DATI_MAX ||
      codaN_ >= LINK_CODA)
    return false;
  accoda(tipo, dati, len);
  return true;
}

bool Link::prendi(LinkMessaggio &m) {
  if (!arriviN_) return false;
  const LinkMessaggio &a = arrivi_[arriviTesta_];
  m.tipo = a.tipo;
  m.len = a.len;
  memcpy(m.dati, a.dati, a.len);
  arriviTesta_ = (uint8_t)((arriviTesta_ + 1) % LINK_CODA);
  arriviN_--;
  return true;
}

void Link::accoda(uint8_t tipo, const uint8_t *dati, uint8_t len) {
  if (codaN_ >= LINK_CODA) return;
  Uscita &u = coda_[(codaTesta_ + codaN_) % LINK_CODA];
  u.tipo = tipo;
  u.len = len;
  if (len) memcpy(u.dati, dati, len);
  codaN_++;
  pompa();
}

// manda la testa della coda se non è già in volo, o la ritrasmette se è ora
void Link::pompa() {
  if (!codaN_ || stato_ < LINK_CONFERMA || stato_ > LINK_PRONTO) return;
  const Uscita &u = coda_[codaTesta_];
  if (!inVolo_) {
    inVolo_ = true;
    voloSeq_ = seqTx_++;
    voloDa_ = ora_;
  } else if (!scaduto(ora_, voloProssimo_)) {
    return;
  } else {
    ritrasmessi_++;
  }
  uint8_t buf[LINK_FRAME_MAX];
  uint8_t n = linkCodifica(buf, sizeof(buf), LINK_PROTO, u.tipo, voloSeq_, u.dati, u.len);
  if (n && tr_.invia) tr_.invia(tr_.ctx, peer_.mac, buf, n);
  voloProssimo_ = ora_ + LINK_RITRASMETTI_MS + jitter(LINK_JITTER_MS);
}

void Link::tick(uint32_t ora) {
  ora_ = ora;
  switch (stato_) {
    case LINK_CERCA:
      if (scaduto(ora, inizio_ + LINK_CERCA_MS)) { perso(LINK_NESSUNO); return; }
      [[fallthrough]];
    case LINK_INCOMPATIBILE:
      // anche da incompatibile: così l'altro lo vede e lo dice al suo giocatore
      if (scaduto(ora, prossimoHello_)) {
        mandaHello(LINK_BROADCAST);
        prossimoHello_ = ora + LINK_HELLO_MS + jitter(LINK_HELLO_MS / 2);
      }
      return;
    case LINK_AGGANCIO:
      if (scaduto(ora, agganciatoDa_ + LINK_AGGANCIO_MS)) { torna(); return; }
      if (scaduto(ora, prossimoHello_)) {
        mandaHello(peer_.mac);
        prossimoHello_ = ora + LINK_HELLO_MS + jitter(LINK_HELLO_MS / 2);
      }
      return;
    case LINK_CONFERMA:
    case LINK_PRONTO:
      if (scaduto(ora, ultimoRx_ + LINK_TIMEOUT_MS) ||
          (inVolo_ && scaduto(ora, voloDa_ + LINK_TIMEOUT_MS))) {
        perso(LINK_SILENZIO);
        return;
      }
      if (scaduto(ora, prossimoPing_)) {
        spedisci(peer_.mac, LT_PING, nullptr, 0);
        prossimoPing_ = ora + LINK_PING_MS;
      }
      pompa();
      return;
    default:
      return;
  }
}

// CONFERMA o un tipo dell'applicazione: si consegna una volta sola, in ordine, e si dà
// l'ACK anche ai doppioni (il nostro primo ACK può essersi perso)
void Link::affidabile(const LinkFrame &f) {
  bool nuovo = !rxSeqValido_ || (int16_t)(f.seq - rxUltimoSeq_) > 0;
  if (nuovo) {
    if (f.tipo == LT_CONFERMA) {
      if (stato_ != LINK_CONFERMA && stato_ != LINK_PRONTO) return;
      confLoro_ = true;
      if (confIo_) stato_ = LINK_PRONTO;
    } else {
      // senza posto (o prima di PRONTO) niente ACK: l'altro ritrasmette più tardi
      if (stato_ != LINK_PRONTO || arriviN_ >= LINK_CODA) { scartati_++; return; }
      LinkMessaggio &m = arrivi_[(arriviTesta_ + arriviN_) % LINK_CODA];
      m.tipo = f.tipo;
      m.len = f.len;
      memcpy(m.dati, f.dati, f.len);
      arriviN_++;
    }
    rxSeqValido_ = true;
    rxUltimoSeq_ = f.seq;
  }
  uint8_t b[2];
  scriviLE(b, f.seq, 2);
  spedisci(peer_.mac, LT_ACK, b, 2);
}

void Link::ricevi(const uint8_t mac[6], const uint8_t *dati, size_t len, uint32_t ora) {
  ora_ = ora;
  if (!attivo() || !mac) return;
  LinkFrame f;
  if (!linkDecodifica(dati, len, f) || !memcmp(mac, cfg_.mac, 6)) { scartati_++; return; }
  bool agganciato = stato_ >= LINK_AGGANCIO;
  // anti-dirottamento: agganciato un peer, tutti gli altri MAC sono rumore
  if (agganciato && memcmp(mac, peer_.mac, 6)) { scartati_++; return; }

  if (f.versione != LINK_PROTO) {
    // di un'altra versione si legge solo la parte fissa di HELLO, per dirlo a schermo
    if (!agganciato && f.tipo == LT_HELLO && f.len >= LINK_HELLO_FISSO)
      incompatibile(mac, f.versione, f.dati);
    else
      scartati_++;
    return;
  }

  if (f.tipo == LT_HELLO) {
    if (f.len < LINK_HELLO_LEN) { scartati_++; return; }
    if (leggiLE(f.dati, 4) != hash_) {
      if (!agganciato) incompatibile(mac, f.versione, f.dati);
      return;
    }
    const uint8_t *dest = f.dati + 12;
    bool nominaMe = !memcmp(dest, cfg_.mac, 6);
    bool nominaAltri = !nominaMe && memcmp(dest, NESSUN_MAC, 6);
    if (stato_ == LINK_CERCA) {
      if (nominaAltri) return;   // è già agganciato a un altro
      aggancia(mac, f);
      if (nominaMe) inConferma();
    } else if (stato_ == LINK_AGGANCIO) {
      ultimoRx_ = ora;
      if (nominaMe) inConferma();
      else if (nominaAltri) torna();
    } else if (nominaMe) {
      ultimoRx_ = ora;           // un doppione del suo aggancio
    } else if (stato_ == LINK_CONFERMA && !nominaAltri) {
      // è tornato a cercare (ha perso i nostri pacchetti): lo si nomina perché si
      // riagganci. Non tiene vivo il collegamento: se non torna, scatta il timeout.
      // Riparte da zero anche la sua sequenza (può essersi riavviato).
      rxSeqValido_ = false;
      confLoro_ = false;
      mandaHello(peer_.mac);
    }
    return;
  }

  if (!agganciato) { scartati_++; return; }
  if (f.tipo == LT_BYE) { perso(LINK_ADDIO); return; }
  // solo un peer agganciato a noi manda altro che HELLO
  if (stato_ == LINK_AGGANCIO) inConferma();
  ultimoRx_ = ora;
  if (f.tipo == LT_PING) return;
  if (f.tipo == LT_ACK) {
    if (f.len >= 2 && inVolo_ && (uint16_t)leggiLE(f.dati, 2) == voloSeq_) {
      inVolo_ = false;
      codaTesta_ = (uint8_t)((codaTesta_ + 1) % LINK_CODA);
      codaN_--;
      pompa();
    }
    return;
  }
  if (linkTipoAffidabile(f.tipo)) { affidabile(f); return; }
  scartati_++;
}
