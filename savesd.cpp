#include "savesd.h"
#include "pet.h"
#include "hal.h"
#include "sdmon.h"
#include <Preferences.h>
#include <algorithm>
#include <memory>
#include <vector>

bool saveSdSaltata = false;

namespace {
// le chiavi le elenca l'HAL (Preferences non sa farlo)
struct ElencoNvs : SaveElencoChiavi {
  void elenca(const char *ns, void (*voce)(const char *, uint8_t, void *),
              void *ctx) override {
    halNvsElenca(ns, voce, ctx);
  }
};
}  // namespace

static ElencoNvs gNvs;
SaveElencoChiavi &nvsChiavi() { return gNvs; }

static const char *gFw = "";
static bool gFwDaAnnotare = false;
static uint8_t *gFoto = nullptr;  // copia "aggiornamento", fotografata prima di load()
static size_t gFotoLen = 0;
static uint32_t gGiornoFatto = 0, gGiornoTentato = 0;

// ---------------------------------------------------------------- scrittura sulla SD

// Toglie le copie in più e i .tmp rimasti da una scrittura interrotta; `adesso` è l'ora
// della copia appena scritta (quelle datate dopo restano, vedi saveRotazione).
static void ruota(uint32_t adesso) {
  struct Nomi { std::vector<String> copie, tmp; } el;
  halSdElenca(SAVE_DIR, [](const char *nome, uint32_t, void *ctx) {
    Nomi &el = *(Nomi *)ctx;
    String n = nome;
    if (n.endsWith(".tmp")) el.tmp.push_back(n);
    else if (saveNomeHaEstensione(nome)) el.copie.push_back(n);
  }, &el);
  for (auto &n : el.tmp) halSdRemove((String(SAVE_DIR "/") + n).c_str());
  std::sort(el.copie.begin(), el.copie.end());
  std::vector<const char *> p;
  for (auto &n : el.copie) p.push_back(n.c_str());
  if (p.empty()) return;
  std::unique_ptr<bool[]> tieni(new bool[p.size()]);
  saveRotazione(&p[0], (uint16_t)p.size(), tieni.get(), adesso);
  for (size_t i = 0; i < p.size(); i++) {
    if (tieni[i]) continue;
    halSdRemove((String(SAVE_DIR "/") + p[i]).c_str());
    Serial.printf("SAVE rotazione: tolto %s\n", p[i]);
  }
}

// .tmp, rilettura con controllo del CRC, rename, rotazione
static bool scriviCopia(const uint8_t *dati, size_t len, uint32_t epoch, uint8_t motivo,
                        char *nomeOut, size_t maxOut) {
  if (!sdReady) {
    saveSdSaltata = true;
    return false;
  }
  char nome[SAVE_NOME_MAX];
  if (len < SAVE_HDR_MIN + 4 || !saveNomeFile(nome, sizeof(nome), epoch, motivo)) return false;
  halSdMkdir(SAVE_DIR);
  String fin = String(SAVE_DIR "/") + nome;
  String tmp = fin + ".tmp";
  HalFile f = halSdOpen(tmp.c_str(), true);
  if (!f) return false;
  bool ok = f.write(dati, len) == len;
  f.close();

  // si rilegge dalla SD, non dal buffer: il CRC deve tornare su ciò che è stato scritto
  if (ok) {
    f = halSdOpen(tmp.c_str());
    ok = f && f.size() == len;
    uint32_t crc = 0;
    size_t pos = 0;
    uint8_t buf[512], coda[4];
    while (ok && pos < len) {
      size_t n = f.read(buf, std::min(sizeof(buf), len - pos));
      if (n == 0) { ok = false; break; }
      for (size_t i = 0; i < n; i++)
        if (pos + i >= len - 4) coda[pos + i - (len - 4)] = buf[i];
      size_t utili = pos >= len - 4 ? 0 : std::min(n, len - 4 - pos);
      crc = saveCrc32(buf, utili, crc);
      pos += n;
    }
    if (f) f.close();
    ok = ok && crc == ((uint32_t)coda[0] | (uint32_t)coda[1] << 8 |
                       (uint32_t)coda[2] << 16 | (uint32_t)coda[3] << 24);
  }
  if (ok) {
    if (halSdExists(fin.c_str())) halSdRemove(fin.c_str());
    ok = halSdRename(tmp.c_str(), fin.c_str());
  }
  if (!ok) {
    halSdRemove(tmp.c_str());
    Serial.printf("SAVE ERR scrittura di %s\n", nome);
    return false;
  }
  saveSdSaltata = false;
  Serial.printf("SAVE %s\n", nome);
  if (nomeOut && maxOut) strlcpy(nomeOut, nome, maxOut);
  ruota(epoch);
  return true;
}

bool saveSdCopia(Pet &pet, uint8_t motivo, char *nome, size_t max) {
  if (!sdReady) {
    saveSdSaltata = true;
    return false;
  }
  uint8_t *buf = (uint8_t *)malloc(SAVE_MAX_BYTES);
  if (!buf) return false;
  uint32_t e = halRtcEpoch();
  size_t n = saveEsporta(pet, e, gFw, SAVE_SCHEDA, motivo, gNvs, buf, SAVE_MAX_BYTES);
  bool ok = n > 0 && scriviCopia(buf, n, e, motivo, nome, max);
  free(buf);
  return ok;
}

// ---------------------------------------------------------------- copie automatiche

static void contaVoce(const char *, uint8_t, void *ctx) { (*(uint16_t *)ctx)++; }

void saveSdAvvio(const char *fw) {
  gFw = fw;
  char vecchia[13] = "";
  Preferences p;
  if (p.begin(SAVE_NS_PET, true)) {  // un namespace mai scritto non si apre
    if (p.isKey("fwver")) p.getString("fwver", vecchia, sizeof(vecchia));
    gGiornoFatto = p.getUInt("bkday", 0);
    p.end();
  }
  if (strcmp(vecchia, fw) == 0) return;
  gFwDaAnnotare = true;
  // NVS vuota (prima installazione, flash con cancellazione, WIPE): niente da conservare
  uint16_t voci = 0;
  gNvs.elenca(SAVE_NS_PET, contaVoce, &voci);
  if (voci == 0) return;
  gFoto = (uint8_t *)malloc(SAVE_MAX_BYTES);
  if (!gFoto) return;
  // epoch e riepilogo si completano dopo load(): qui il Pet non è ancora caricato
  SaveIntestazione h;
  h.motivo = SAVE_AGGIORNAMENTO;
  strlcpy(h.fw, vecchia, sizeof(h.fw));  // chi ha scritto i dati; "" = prima della 1.18
  gFotoLen = saveScrivi(h, gNvs, gFoto, SAVE_MAX_BYTES);
  Serial.printf("SAVE firmware cambiato (%s -> %s): %u byte da copiare\n",
                vecchia[0] ? vecchia : "?", fw, (unsigned)gFotoLen);
}

void saveSdAvvioFine(Pet &pet) {
  if (!gFwDaAnnotare) return;
  bool ok = true;
  if (gFoto) {
    uint32_t e = halRtcEpoch();
    ok = gFotoLen > 0 && saveCompletaIntestazione(gFoto, gFotoLen, e, saveRiepilogo(pet)) &&
         scriviCopia(gFoto, gFotoLen, e, SAVE_AGGIORNAMENTO, 0, 0);
    free(gFoto);
    gFoto = nullptr;
  }
  if (!ok) return;  // si riprova al prossimo avvio
  Preferences p;
  if (p.begin(SAVE_NS_PET, false)) {
    p.putString("fwver", gFw);
    p.end();
  }
  gFwDaAnnotare = false;
}

void saveSdGiornaliera(Pet &pet) {
  uint32_t e = pet.lastSeenEpoch;  // aggiornato dal loop ogni 30 s: niente I2C qui
  if (!e) return;
  uint32_t g = e / 86400;
  if (g == gGiornoFatto || g == gGiornoTentato) return;
  gGiornoTentato = g;
  if (!saveSdCopia(pet, SAVE_GIORNALIERA)) return;
  gGiornoFatto = g;
  Preferences p;
  if (p.begin(SAVE_NS_PET, false)) {
    p.putUInt("bkday", g);
    p.end();
  }
}

bool saveSdCopiaPrimaImport(void *ctx) {
  if (!sdReady) {
    saveSdSaltata = true;
    return true;  // senza SD si importa lo stesso (scheda nuova, file dalla PWA)
  }
  return saveSdCopia(*(Pet *)ctx, SAVE_PRIMA_IMPORT);
}

bool saveSdCopiaPrimaScambio(void *ctx) {
  if (!sdReady) {
    saveSdSaltata = true;
    return true;  // il giornale dello scambio protegge comunque i due pet
  }
  return saveSdCopia(*(Pet *)ctx, SAVE_PRIMA_SCAMBIO);
}

// ---------------------------------------------------------------- elenco e ripristino

namespace {
struct Elenco {
  SaveSdVoce *out;
  uint8_t max, n;
};
}  // namespace

static void voceElenco(const char *nome, uint32_t, void *ctx) {
  Elenco &el = *(Elenco *)ctx;
  String n = nome;
  if (!saveNomeHaEstensione(nome) || n.length() >= SAVE_NOME_MAX) return;
  uint8_t hdr[SAVE_HDR_MIN];
  SaveSdVoce v;
  HalFile f = halSdOpen((String(SAVE_DIR "/") + n).c_str());
  if (!f || f.read(hdr, sizeof(hdr)) != sizeof(hdr) ||
      saveLeggiIntestazione(hdr, sizeof(hdr), &v.h) != SAVE_OK)
    return;
  strlcpy(v.nome, nome, sizeof(v.nome));
  SaveSdVoce *out = el.out;
  // un elenco pieno tiene le più recenti: si sostituisce la più vecchia
  if (el.n < el.max) out[el.n++] = v;
  else if (v.h.epoch > out[el.n - 1].h.epoch) out[el.n - 1] = v;
  // ordine: epoch decrescente, a parità il nome (alfabetico = cronologico)
  for (uint8_t i = el.n - 1; i > 0; i--) {
    const SaveSdVoce &a = out[i - 1], &b = out[i];
    if (a.h.epoch > b.h.epoch || (a.h.epoch == b.h.epoch && strcmp(a.nome, b.nome) > 0))
      break;
    std::swap(out[i - 1], out[i]);
  }
}

uint8_t saveSdElenco(SaveSdVoce *out, uint8_t max) {
  if (!sdReady || !max) return 0;
  Elenco el = {out, max, 0};
  halSdElenca(SAVE_DIR, voceElenco, &el);
  return el.n;
}

bool saveSdImportaPresente() {
  return sdReady && halSdExists(SAVE_DIR "/" SAVE_FILE_IMPORTA);
}

bool saveSdArchiviaImporta() {
  if (!saveSdImportaPresente()) return false;
  char d[SAVE_NOME_MAX];
  if (!saveNomeFile(d, sizeof(d), halRtcEpoch(), SAVE_MANUALE)) return false;
  d[15] = 0;  // AAAAMMGG-HHMMSS
  String fin = String(SAVE_DIR "/importato-") + d + SAVE_ESTENSIONE;
  bool ok = !halSdExists(fin.c_str()) && halSdRename(SAVE_DIR "/" SAVE_FILE_IMPORTA, fin.c_str());
  Serial.printf("SAVE %s -> %s: %s\n", SAVE_FILE_IMPORTA, fin.c_str(), ok ? "ok" : "ERR");
  return ok;
}

static uint8_t *gAttesa = nullptr;  // file intero, verificato, in attesa di conferma
static size_t gAttesaLen = 0;
static uint8_t gAttesaDa = SAVE_DA_NESSUNO;
static SaveIntestazione gAttesaH;
static uint32_t gAttesaFino = 0;  // scadenza della conferma di IMPORT (millis, timeLeft)

void saveSdAnnulla() {
  if (gAttesaDa == SAVE_DA_SERIALE) Serial.println("ERR ANNULLATO");
  free(gAttesa);
  gAttesa = nullptr;
  gAttesaLen = 0;
  gAttesaDa = SAVE_DA_NESSUNO;
}

// prende possesso di `dati` (malloc): lo tiene se il file è valido, altrimenti lo libera
static SaveEsito preparaAttesa(uint8_t *dati, size_t len, uint8_t origine) {
  if (gAttesaDa != SAVE_DA_NESSUNO) {
    gAttesaDa = SAVE_DA_NESSUNO;  // la nuova sostituisce la vecchia, senza "ANNULLATO"
    free(gAttesa);
  }
  SaveEsito r = saveVerifica(dati, len, &gAttesaH);
  if (r != SAVE_OK) {
    free(dati);
    gAttesa = nullptr;
    gAttesaLen = 0;
    return r;
  }
  gAttesa = dati;
  gAttesaLen = len;
  gAttesaDa = origine;
  return SAVE_OK;
}

SaveEsito saveSdPreparaFile(const char *nome, uint8_t origine) {
  if (!sdReady) return SAVE_ERR_LUNGHEZZA;
  HalFile f = halSdOpen((String(SAVE_DIR "/") + nome).c_str());
  if (!f) return SAVE_ERR_LUNGHEZZA;
  size_t len = f.size();
  uint8_t *buf = len <= SAVE_MAX_BYTES ? (uint8_t *)malloc(len ? len : 1) : nullptr;
  bool ok = buf && f.read(buf, len) == len;
  f.close();
  if (!ok) {
    free(buf);
    return SAVE_ERR_LUNGHEZZA;
  }
  return preparaAttesa(buf, len, origine);
}

bool saveSdScaduta() {
  if (gAttesaDa != SAVE_DA_SERIALE || timeLeft(gAttesaFino)) return false;
  gAttesaDa = SAVE_DA_NESSUNO;  // niente "ANNULLATO": la risposta è questa
  saveSdAnnulla();
  Serial.println("ERR SCADUTO");
  return true;
}

uint8_t saveSdAttesa(SaveIntestazione *h) {
  if (h && gAttesaDa != SAVE_DA_NESSUNO) *h = gAttesaH;
  return gAttesaDa;
}

SaveEsito saveSdConferma(Pet &pet) {
  if (gAttesaDa == SAVE_DA_NESSUNO) return SAVE_ERR_LUNGHEZZA;
  uint8_t da = gAttesaDa;
  // al primo avvio la NVS ha solo il pet appena creato: una copia "prima
  // dell'importazione" occuperebbe un posto della rotazione senza conservare nulla
  SaveEsito r = saveImporta(gAttesa, gAttesaLen, halRtcEpoch(),
                            da == SAVE_DA_PRIMO_AVVIO ? nullptr : saveSdCopiaPrimaImport,
                            &pet);
  gAttesaDa = SAVE_DA_NESSUNO;  // niente "ANNULLATO": la risposta è qui sotto
  saveSdAnnulla();
  Serial.printf("SAVE importazione: %s\n", saveEsitoNome(r));
  if (da == SAVE_DA_SERIALE) {
    // il chiamante riavvia subito: la risposta deve arrivare all'host prima
    Serial.setTxTimeoutMs(200);
    if (r == SAVE_OK) Serial.println("DONE");
    else Serial.printf("ERR %s\n", saveEsitoNome(r));
    Serial.flush();
    Serial.setTxTimeoutMs(0);
  }
  return r;
}

// ---------------------------------------------------------------- console seriale

bool saveSerialCommand(const String &line, Pet &pet) {
  if (line == "EXPORT") {
    uint8_t *buf = (uint8_t *)malloc(SAVE_MAX_BYTES);
    size_t n = buf ? saveEsporta(pet, halRtcEpoch(), gFw, SAVE_SCHEDA, SAVE_MANUALE, gNvs, buf,
                                 SAVE_MAX_BYTES)
                   : 0;
    if (n == 0) {
      Serial.println("ERR");
    } else {
      // con timeout 0 (vedi setup) una riga lunga riempirebbe il buffer USB e andrebbe
      // persa a metà: per questa risposta si aspetta l'host
      Serial.setTxTimeoutMs(200);
      // pezzi multipli di 3 byte: il base64 si concatena senza '=' in mezzo
      char t[SAVE_B64_LEN(384) + 1];
      for (size_t i = 0; i < n; i += 384) {
        saveBase64Codifica(buf + i, std::min((size_t)384, n - i), t, sizeof(t));
        Serial.print(t);
      }
      Serial.println();
      Serial.println("DONE");
      Serial.setTxTimeoutMs(0);
    }
    free(buf);
    return true;
  }
  if (line == "BACKUP") {
    char nome[SAVE_NOME_MAX];
    if (!sdReady) Serial.println("ERR (SD assente)");
    else if (!saveSdCopia(pet, SAVE_MANUALE, nome, sizeof(nome))) Serial.println("ERR");
    else {
      Serial.printf("%s/%s\n", SAVE_DIR, nome);
      Serial.println("DONE");
    }
    return true;
  }
  if (line.startsWith("IMPORT ")) {
    if (gAttesaDa != SAVE_DA_NESSUNO) {
      Serial.println("ERR OCCUPATO");
      return true;
    }
    uint8_t *buf = (uint8_t *)malloc(SAVE_MAX_BYTES);
    size_t n = buf ? saveBase64Decodifica(line.c_str() + 7, line.length() - 7, buf,
                                          SAVE_MAX_BYTES)
                   : 0;
    if (n == 0) {
      free(buf);
      Serial.println("ERR BASE64");
      return true;
    }
    uint8_t *corto = (uint8_t *)realloc(buf, n);  // resta in RAM fino alla conferma
    if (corto) buf = corto;
    SaveEsito r = preparaAttesa(buf, n, SAVE_DA_SERIALE);
    if (r != SAVE_OK) {
      Serial.printf("ERR %s\n", saveEsitoNome(r));
    } else {
      gAttesaFino = millis() + SAVE_ATTESA_SERIALE_MS;
      Serial.printf("CONFERMA %d %u\n", gAttesaH.riepilogo.specie, gAttesaH.riepilogo.livello);
    }
    return true;
  }
  return false;
}
