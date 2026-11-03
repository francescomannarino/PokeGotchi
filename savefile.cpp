#include "savefile.h"
#include "pet.h"
#include <stdio.h>

// ---------------------------------------------------------------- byte LE

static void scriviLE(uint8_t *p, uint64_t v, uint8_t n) {
  for (uint8_t i = 0; i < n; i++) p[i] = (uint8_t)(v >> (8 * i));
}
static uint64_t leggiLE(const uint8_t *p, uint8_t n) {
  uint64_t v = 0;
  for (uint8_t i = 0; i < n; i++) v |= (uint64_t)p[i] << (8 * i);
  return v;
}

// dimensione dei tipi interi, 0 per stringhe/blob, -1 per un tipo sconosciuto
static int dimTipo(uint8_t t) {
  switch (t) {
    case SAVE_T_U8: case SAVE_T_I8: return 1;
    case SAVE_T_U16: case SAVE_T_I16: return 2;
    case SAVE_T_U32: case SAVE_T_I32: return 4;
    case SAVE_T_U64: case SAVE_T_I64: return 8;
    case SAVE_T_STR: case SAVE_T_BLOB: return 0;
  }
  return -1;
}

static bool magicValido(const uint8_t *d) {
  return memcmp(d, "PGSV", 4) == 0 || memcmp(d, "TPSV", 4) == 0;
}

// namespace di destinazione di un blocco del corpo: uno di SAVE_NAMESPACE, oppure quello
// vecchio, che diventa SAVE_NS_PET; nullptr = sconosciuto, si salta
static const char *nsDestinazione(const uint8_t *nome, uint8_t n) {
  if (strlen(SAVE_NS_VECCHIO) == n && memcmp(SAVE_NS_VECCHIO, nome, n) == 0) return SAVE_NS_PET;
  for (uint8_t i = 0; i < SAVE_N_NAMESPACE; i++)
    if (strlen(SAVE_NAMESPACE[i]) == n && memcmp(SAVE_NAMESPACE[i], nome, n) == 0)
      return SAVE_NAMESPACE[i];
  return nullptr;
}

const char *saveEsitoNome(SaveEsito e) {
  switch (e) {
    case SAVE_OK: return "OK";
    case SAVE_ERR_MAGIC: return "MAGIC";
    case SAVE_ERR_FORMATO: return "FORMATO";
    case SAVE_ERR_LUNGHEZZA: return "LUNGHEZZA";
    case SAVE_ERR_CRC: return "CRC";
    case SAVE_ERR_SCHEMA: return "SCHEMA";
    case SAVE_ERR_DATI: return "DATI";
    case SAVE_ERR_COPIA: return "COPIA";
    case SAVE_ERR_SCRITTURA: return "SCRITTURA";
  }
  return "?";
}

// ---------------------------------------------------------------- CRC32 e base64

uint32_t saveCrc32(const uint8_t *dati, size_t len, uint32_t crc) {
  crc = ~crc;
  for (size_t i = 0; i < len; i++) {
    crc ^= dati[i];
    for (uint8_t b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
  }
  return ~crc;
}

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t saveBase64Codifica(const uint8_t *dati, size_t len, char *out, size_t max) {
  size_t n = SAVE_B64_LEN(len);
  if (n + 1 > max) return 0;
  char *o = out;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t v = (uint32_t)dati[i] << 16;
    if (i + 1 < len) v |= (uint32_t)dati[i + 1] << 8;
    if (i + 2 < len) v |= dati[i + 2];
    *o++ = B64[(v >> 18) & 63];
    *o++ = B64[(v >> 12) & 63];
    *o++ = i + 1 < len ? B64[(v >> 6) & 63] : '=';
    *o++ = i + 2 < len ? B64[v & 63] : '=';
  }
  *o = 0;
  return n;
}

static int valB64(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

size_t saveBase64Decodifica(const char *testo, size_t len, uint8_t *out, size_t max) {
  size_t n = 0;
  uint32_t acc = 0;
  uint8_t bit = 0, pad = 0;
  for (size_t i = 0; i < len; i++) {
    char c = testo[i];
    if (c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
    if (c == '=') { pad++; continue; }
    int v = valB64(c);
    if (v < 0 || pad) return 0;  // carattere estraneo, o dati dopo il riempimento
    acc = (acc << 6) | (uint32_t)v;
    bit += 6;
    if (bit >= 8) {
      bit -= 8;
      if (n >= max) return 0;
      out[n++] = (uint8_t)(acc >> bit);
    }
  }
  if (pad > 2 || bit >= 6) return 0;  // un solo carattere nell'ultimo gruppo
  return n;
}

// ---------------------------------------------------------------- esportazione

namespace {
struct Scrittura {
  Preferences *prefs;
  uint8_t *out;
  size_t pos, max;
  uint16_t voci;
  bool pieno;
};
}  // namespace

static void scriviRiepilogo(uint8_t *out, const SaveRiepilogo &r);

static bool spazio(Scrittura &s, size_t n) {
  if (s.pieno || s.pos + n > s.max) { s.pieno = true; return false; }
  return true;
}

static void voceNvs(const char *chiave, uint8_t tipo, void *ctx) {
  Scrittura &s = *(Scrittura *)ctx;
  size_t lk = strlen(chiave);
  int dim = dimTipo(tipo);
  if (lk == 0 || lk > SAVE_MAX_CHIAVE || dim < 0) return;  // non è nostra: la si salta
  Preferences &p = *s.prefs;
  size_t testa = 1 + 1 + lk + 2;
  if (!spazio(s, testa)) return;
  uint8_t *v = s.out + s.pos + testa;  // il valore si legge direttamente al suo posto
  size_t lv;
  if (dim > 0) {
    if (!spazio(s, testa + dim)) return;
    uint64_t x = 0;
    switch (tipo) {
      case SAVE_T_U8:  x = p.getUChar(chiave); break;
      case SAVE_T_I8:  x = (uint8_t)p.getChar(chiave); break;
      case SAVE_T_U16: x = p.getUShort(chiave); break;
      case SAVE_T_I16: x = (uint16_t)p.getShort(chiave); break;
      case SAVE_T_U32: x = p.getUInt(chiave); break;
      case SAVE_T_I32: x = (uint32_t)p.getInt(chiave); break;
      case SAVE_T_U64: x = p.getULong64(chiave); break;
      case SAVE_T_I64: x = (uint64_t)p.getLong64(chiave); break;
    }
    scriviLE(v, x, (uint8_t)dim);
    lv = (size_t)dim;
  } else if (tipo == SAVE_T_STR) {
    size_t libero = s.max - s.pos - testa;
    if (libero > SAVE_MAX_STRINGA + 1) libero = SAVE_MAX_STRINGA + 1;
    // 0 = non ci sta (nel buffer o nel limite del formato): meglio fallire che perderla
    if (p.getString(chiave, (char *)v, libero) == 0) { s.pieno = true; return; }
    lv = strnlen((const char *)v, libero);
  } else {
    lv = p.getBytesLength(chiave);
    if (lv == 0) return;  // un blob vuoto non si potrebbe riscrivere: lo si salta
    if (lv > 0xFFFF || !spazio(s, testa + lv)) { s.pieno = true; return; }
    if (p.getBytes(chiave, v, lv) != lv) { s.pieno = true; return; }
  }
  uint8_t *q = s.out + s.pos;
  q[0] = tipo;
  q[1] = (uint8_t)lk;
  memcpy(q + 2, chiave, lk);
  scriviLE(q + 2 + lk, lv, 2);
  s.pos += testa + lv;
  s.voci++;
}

size_t saveScrivi(const SaveIntestazione &h, SaveElencoChiavi &chiavi, uint8_t *out,
                  size_t max) {
  if (max < SAVE_HDR_MIN + 4) return 0;
  memset(out, 0, SAVE_HDR_MIN);
  memcpy(out, "PGSV", 4);
  out[4] = SAVE_FORMATO;
  out[5] = h.piattaforma;
  out[6] = h.motivo;
  scriviLE(out + 8, SAVE_SCHEMA, 2);
  scriviLE(out + 10, SAVE_HDR_MIN, 2);
  scriviLE(out + 16, h.epoch, 4);
  strncpy((char *)out + 20, h.fw, 12);
  scriviRiepilogo(out, h.riepilogo);

  Scrittura s = { 0, out, SAVE_HDR_MIN, max - 4, 0, false };
  for (uint8_t i = 0; i < SAVE_N_NAMESPACE; i++) {
    const char *ns = SAVE_NAMESPACE[i];
    size_t ln = strlen(ns);
    if (!spazio(s, 1 + ln + 2)) return 0;
    out[s.pos] = (uint8_t)ln;
    memcpy(out + s.pos + 1, ns, ln);
    size_t posVoci = s.pos + 1 + ln;
    s.pos += 1 + ln + 2;
    Preferences p;
    s.prefs = &p;
    s.voci = 0;
    if (p.begin(ns, true)) {  // un namespace mai scritto non si apre: blocco vuoto
      chiavi.elenca(ns, voceNvs, &s);
      p.end();
    }
    if (s.pieno) return 0;
    scriviLE(out + posVoci, s.voci, 2);
  }
  size_t tot = s.pos + 4;
  scriviLE(out + 12, tot, 4);
  scriviLE(out + s.pos, saveCrc32(out, s.pos), 4);
  return tot;
}

size_t saveEsporta(Pet &pet, uint32_t oraEpoch, const char *fw, uint8_t piattaforma,
                   uint8_t motivo, SaveElencoChiavi &chiavi, uint8_t *out, size_t max) {
  if (oraEpoch) pet.setClock(oraEpoch);  // salva: la NVS ha i minuti fino a ora
  SaveIntestazione h;
  h.piattaforma = piattaforma;
  h.motivo = motivo;
  h.epoch = oraEpoch;
  strncpy(h.fw, fw, sizeof(h.fw) - 1);
  h.riepilogo = saveRiepilogo(pet);
  return saveScrivi(h, chiavi, out, max);
}

SaveRiepilogo saveRiepilogo(Pet &pet) {
  SaveRiepilogo r;
  r.specie = pet.speciesId;
  r.livello = pet.level();
  r.shiny = pet.shiny;
  r.pokedex = pet.registeredCount();
  strncpy(r.nome, pet.nick, sizeof(r.nome) - 1);
  return r;
}

static void scriviRiepilogo(uint8_t *out, const SaveRiepilogo &r) {
  scriviLE(out + 32, (uint16_t)r.specie, 2);
  scriviLE(out + 34, r.livello, 2);
  out[36] = r.shiny ? 1 : 0;
  scriviLE(out + 38, r.pokedex, 2);
  memset(out + 40, 0, 12);
  strncpy((char *)out + 40, r.nome, 12);
}

bool saveCompletaIntestazione(uint8_t *dati, size_t len, uint32_t epoch,
                              const SaveRiepilogo &r) {
  if (len < SAVE_HDR_MIN + 4 || !magicValido(dati)) return false;
  if (leggiLE(dati + 12, 4) != len) return false;
  scriviLE(dati + 16, epoch, 4);
  scriviRiepilogo(dati, r);
  scriviLE(dati + len - 4, saveCrc32(dati, len - 4), 4);
  return true;
}

// ---------------------------------------------------------------- lettura e importazione

SaveEsito saveLeggiIntestazione(const uint8_t *dati, size_t len, SaveIntestazione *h) {
  if (len < 4 || !magicValido(dati)) return SAVE_ERR_MAGIC;
  if (len < SAVE_HDR_MIN) return SAVE_ERR_LUNGHEZZA;
  if (dati[4] > SAVE_FORMATO) return SAVE_ERR_FORMATO;
  if (h) {
    h->formato = dati[4];
    h->piattaforma = dati[5];
    h->motivo = dati[6];
    h->schema = (uint16_t)leggiLE(dati + 8, 2);
    h->lunghezzaHdr = (uint16_t)leggiLE(dati + 10, 2);
    h->lunghezza = (uint32_t)leggiLE(dati + 12, 4);
    h->epoch = (uint32_t)leggiLE(dati + 16, 4);
    memcpy(h->fw, dati + 20, 12);
    h->fw[12] = 0;
    h->riepilogo.specie = (int16_t)leggiLE(dati + 32, 2);
    h->riepilogo.livello = (uint16_t)leggiLE(dati + 34, 2);
    h->riepilogo.shiny = dati[36] != 0;
    h->riepilogo.pokedex = (uint16_t)leggiLE(dati + 38, 2);
    memcpy(h->riepilogo.nome, dati + 40, 12);
    h->riepilogo.nome[11] = 0;
  }
  return SAVE_OK;
}

// Percorre il corpo; con `scrivi` riscrive le chiavi dei namespace esportati (che il
// chiamante ha già svuotato), altrimenti si limita a controllarne la forma.
static SaveEsito percorriCorpo(const uint8_t *d, size_t pos, size_t fine, bool scrivi) {
  while (pos < fine) {
    uint8_t ln = d[pos];
    if (ln == 0 || ln > SAVE_MAX_CHIAVE || pos + 1 + ln + 2 > fine) return SAVE_ERR_DATI;
    const uint8_t *nome = d + pos + 1;
    uint16_t voci = (uint16_t)leggiLE(d + pos + 1 + ln, 2);
    pos += 1 + ln + 2;
    const char *dest = nsDestinazione(nome, ln);  // un namespace sconosciuto si salta
    bool nostro = dest != nullptr;
    Preferences p;
    if (scrivi && nostro && !p.begin(dest, false)) return SAVE_ERR_SCRITTURA;
    for (uint16_t i = 0; i < voci; i++) {
      if (pos + 2 > fine) return SAVE_ERR_DATI;
      uint8_t tipo = d[pos], lk = d[pos + 1];
      int dim = dimTipo(tipo);
      if (dim < 0 || lk == 0 || lk > SAVE_MAX_CHIAVE || pos + 2 + lk + 2 > fine)
        return SAVE_ERR_DATI;
      char k[SAVE_MAX_CHIAVE + 1];
      memcpy(k, d + pos + 2, lk);
      k[lk] = 0;
      if (strlen(k) != lk) return SAVE_ERR_DATI;
      size_t lv = (size_t)leggiLE(d + pos + 2 + lk, 2);
      const uint8_t *v = d + pos + 2 + lk + 2;
      pos += 2 + lk + 2 + lv;
      if (pos > fine) return SAVE_ERR_DATI;
      if (dim > 0 && lv != (size_t)dim) return SAVE_ERR_DATI;
      if (tipo == SAVE_T_STR && (lv > SAVE_MAX_STRINGA || memchr(v, 0, lv)))
        return SAVE_ERR_DATI;
      if (!scrivi || !nostro) continue;

      uint64_t x = dim > 0 ? leggiLE(v, (uint8_t)dim) : 0;
      size_t fatto = 0;
      switch (tipo) {
        case SAVE_T_U8:  fatto = p.putUChar(k, (uint8_t)x); break;
        case SAVE_T_I8:  fatto = p.putChar(k, (int8_t)x); break;
        case SAVE_T_U16: fatto = p.putUShort(k, (uint16_t)x); break;
        case SAVE_T_I16: fatto = p.putShort(k, (int16_t)x); break;
        case SAVE_T_U32: fatto = p.putUInt(k, (uint32_t)x); break;
        case SAVE_T_I32: fatto = p.putInt(k, (int32_t)x); break;
        case SAVE_T_U64: fatto = p.putULong64(k, x); break;
        case SAVE_T_I64: fatto = p.putLong64(k, (int64_t)x); break;
        case SAVE_T_STR: {
          char str[SAVE_MAX_STRINGA + 1];
          memcpy(str, v, lv);
          str[lv] = 0;
          fatto = p.putString(k, str);  // restituisce strlen: 0 per una stringa vuota
          break;
        }
        case SAVE_T_BLOB:
          if (lv == 0) continue;  // la NVS non accetta blob vuoti
          fatto = p.putBytes(k, v, lv);
          break;
      }
      if (fatto != (dim > 0 ? (size_t)dim : lv)) return SAVE_ERR_SCRITTURA;
    }
  }
  return SAVE_OK;
}

SaveEsito saveVerifica(const uint8_t *dati, size_t len, SaveIntestazione *h) {
  SaveIntestazione tmp;
  if (!h) h = &tmp;
  SaveEsito e = saveLeggiIntestazione(dati, len, h);
  if (e != SAVE_OK) return e;
  if (h->lunghezza != len || h->lunghezzaHdr < SAVE_HDR_MIN || h->lunghezzaHdr > len - 4)
    return SAVE_ERR_LUNGHEZZA;
  if (saveCrc32(dati, len - 4) != (uint32_t)leggiLE(dati + len - 4, 4)) return SAVE_ERR_CRC;
  if (h->schema > SAVE_SCHEMA) return SAVE_ERR_SCHEMA;
  return percorriCorpo(dati, h->lunghezzaHdr, len - 4, false);
}

SaveEsito saveImporta(const uint8_t *dati, size_t len, uint32_t oraEpoch,
                      bool (*copiaPrima)(void *ctx), void *ctx) {
  SaveIntestazione h;
  SaveEsito e = saveVerifica(dati, len, &h);
  if (e != SAVE_OK) return e;
  if (copiaPrima && !copiaPrima(ctx)) return SAVE_ERR_COPIA;

  // da qui si scrive: lo stato importato sostituisce quello attuale per intero, anche le
  // chiavi che il file non ha (un file vecchio con "spec" non deve trovare un "dexn")
  for (uint8_t i = 0; i < SAVE_N_NAMESPACE; i++) {
    Preferences p;
    if (!p.begin(SAVE_NAMESPACE[i], false)) return SAVE_ERR_SCRITTURA;
    bool ok = p.clear();
    p.end();
    if (!ok) return SAVE_ERR_SCRITTURA;
  }
  e = percorriCorpo(dati, h.lunghezzaHdr, len - 4, true);
  if (e != SAVE_OK) return e;

  // tempo congelato: "seen" è la chiave di lastSeenEpoch (Pet::save/syncClock)
  Preferences p;
  if (!p.begin(SAVE_NS_PET, false)) return SAVE_ERR_SCRITTURA;
  if (oraEpoch) {
    if (p.putUInt("seen", oraEpoch) != 4) e = SAVE_ERR_SCRITTURA;
  } else {
    p.remove("seen");  // senza "seen" syncClock() non applica minuti
  }
  p.end();
  return e;
}

// ---------------------------------------------------------------- migrazione del nome

namespace {
struct Conta { uint16_t n; };
}  // namespace

static void contaVoce(const char *, uint8_t, void *ctx) { ((Conta *)ctx)->n++; }

static uint16_t chiaviIn(SaveElencoChiavi &chiavi, const char *ns) {
  Conta c = { 0 };
  chiavi.elenca(ns, contaVoce, &c);
  return c.n;
}

bool saveMigraNamespace(SaveElencoChiavi &chiavi) {
  if (chiaviIn(chiavi, SAVE_NS_VECCHIO) == 0 || chiaviIn(chiavi, SAVE_NS_PET) > 0) return false;
  // il namespace vecchio diventa un blocco del corpo, poi lo si riscrive come un'importazione
  // (percorriCorpo lo manda in SAVE_NS_PET): stessi tipi, stessi limiti
  uint8_t *buf = (uint8_t *)malloc(SAVE_MAX_BYTES);
  if (!buf) return false;
  size_t ln = strlen(SAVE_NS_VECCHIO);
  buf[0] = (uint8_t)ln;
  memcpy(buf + 1, SAVE_NS_VECCHIO, ln);
  Preferences p;
  Scrittura s = { &p, buf, 1 + ln + 2, SAVE_MAX_BYTES, 0, false };
  bool ok = p.begin(SAVE_NS_VECCHIO, true);
  if (ok) {
    chiavi.elenca(SAVE_NS_VECCHIO, voceNvs, &s);
    p.end();
  }
  ok = ok && !s.pieno;
  if (ok) {
    scriviLE(buf + 1 + ln, s.voci, 2);
    ok = percorriCorpo(buf, 0, s.pos, true) == SAVE_OK;
  }
  free(buf);
  if (!ok) return false;
  // copiato: il vecchio si svuota (se qui si spegne, al prossimo avvio SAVE_NS_PET non è
  // più vuoto e il vecchio resta lì, innocuo)
  if (p.begin(SAVE_NS_VECCHIO, false)) {
    p.clear();
    p.end();
  }
  return true;
}

// ---------------------------------------------------------------- copie in una cartella

static const char *const MOTIVI[] = {
  "manuale", "giornaliera", "aggiornamento", "scambio", "importazione",
};
static const uint8_t N_MOTIVI = sizeof(MOTIVI) / sizeof(MOTIVI[0]);

const char *saveMotivoNome(uint8_t motivo) {
  return motivo < N_MOTIVI ? MOTIVI[motivo] : "?";
}

size_t saveNomeFile(char *out, size_t max, uint32_t epoch, uint8_t motivo) {
  // da giorni dal 1970 a data civile (algoritmo di H. Hinnant), senza dipendere da
  // gmtime e dal fuso del sistema
  uint32_t giorni = epoch / 86400, sec = epoch % 86400;
  int32_t z = (int32_t)giorni + 719468;
  int32_t era = z / 146097;
  uint32_t doe = (uint32_t)(z - era * 146097);
  uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  uint32_t mp = (5 * doy + 2) / 153;
  uint32_t g = doy - (153 * mp + 2) / 5 + 1;
  uint32_t m = mp < 10 ? mp + 3 : mp - 9;
  uint32_t a = yoe + era * 400 + (m <= 2 ? 1 : 0);
  int n = snprintf(out, max, "%04u%02u%02u-%02u%02u%02u-%s" SAVE_ESTENSIONE, (unsigned)a,
                   (unsigned)m, (unsigned)g, (unsigned)(sec / 3600),
                   (unsigned)(sec / 60 % 60), (unsigned)(sec % 60), saveMotivoNome(motivo));
  return (n > 0 && (size_t)n < max) ? (size_t)n : 0;
}

// lunghezza dell'estensione (nuova o vecchia) con cui finisce il nome, 0 se nessuna
static size_t lunghezzaEstensione(const char *nome) {
  size_t l = strlen(nome);
  for (const char *e : { SAVE_ESTENSIONE, SAVE_ESTENSIONE_VECCHIA }) {
    size_t le = strlen(e);
    if (l > le && strcmp(nome + l - le, e) == 0) return le;
  }
  return 0;
}

bool saveNomeHaEstensione(const char *nome) { return lunghezzaEstensione(nome) > 0; }

int saveMotivoDaNome(const char *nome) {
  // AAAAMMGG-HHMMSS-  (16 caratteri), poi il motivo e l'estensione
  size_t l = strlen(nome), le = lunghezzaEstensione(nome);
  if (le == 0 || l <= 16 + le || nome[8] != '-' || nome[15] != '-') return -1;
  for (int i = 0; i < 15; i++)
    if (i != 8 && (nome[i] < '0' || nome[i] > '9')) return -1;
  size_t lm = l - le - 16;
  for (uint8_t m = 0; m < N_MOTIVI; m++)
    if (strlen(MOTIVI[m]) == lm && strncmp(nome + 16, MOTIVI[m], lm) == 0) return m;
  return -1;
}

bool saveDalFuturo(const char *nome, uint32_t adesso) {
  char ora[SAVE_NOME_MAX];
  // "AAAAMMGG-HHMMSS": i primi 15 caratteri si confrontano come testo
  if (!adesso || !saveNomeFile(ora, sizeof(ora), adesso + SAVE_FUTURO_S, SAVE_MANUALE))
    return false;
  return strncmp(nome, ora, 15) > 0;
}

void saveRotazione(const char *const *nomi, uint16_t n, bool *tieni, uint32_t adesso) {
  bool visto[N_MOTIVI] = {};
  uint16_t recenti = 0;
  for (int i = (int)n - 1; i >= 0; i--) {  // dalla più recente
    int m = saveMotivoDaNome(nomi[i]);
    if (m < 0 || saveDalFuturo(nomi[i], adesso)) { tieni[i] = true; continue; }
    tieni[i] = recenti < SAVE_TIENI || !visto[m];
    recenti++;
    visto[m] = true;
  }
}
