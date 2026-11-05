// Preferences (NVS dell'ESP32) in memoria, condiviso dai test e dall'emulatore su PC.
#include "Preferences.h"
#include <map>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------- Preferences in memoria ----------------
// ogni valore ricorda il tipo con cui è stato scritto (codici di `nvs_type_t`), come la
// NVS reale, così si possono enumerare le chiavi per l'esportazione
enum : uint8_t {
  T_U8 = 0x01, T_I8 = 0x11, T_U16 = 0x02, T_I16 = 0x12, T_U32 = 0x04, T_I32 = 0x14,
  T_U64 = 0x08, T_I64 = 0x18, T_STR = 0x21, T_BLOB = 0x42,
};
struct Valore {
  uint8_t tipo;
  std::vector<uint8_t> dati;
};
typedef std::map<std::string, Valore> KV;
static std::map<std::string, KV> gStore;
static uint32_t gScritture = 0;  // cresce a ogni modifica: l'emulatore salva solo se cambia

void mockNvsReset() { gStore.clear(); gScritture++; }
// più dispositivi nello stesso processo (test dello scambio): ognuno ha il suo archivio,
// e gStore è quello del dispositivo scelto
static std::map<int, std::map<std::string, KV> > gAltri;
static int gDispositivo = 0;
void mockNvsDispositivo(int k) {
  if (k == gDispositivo) return;
  gAltri[gDispositivo].swap(gStore);
  gStore.swap(gAltri[k]);
  gAltri.erase(k);
  gDispositivo = k;
}
uint32_t mockNvsScritture() { return gScritture; }
size_t mockNvsKeyCount(const char *ns) {
  std::map<std::string, KV>::iterator it = gStore.find(ns);
  return it == gStore.end() ? 0 : it->second.size();
}
void mockNvsElenca(const char *ns, void (*voce)(const char *chiave, uint8_t tipo, void *ctx),
                   void *ctx) {
  std::map<std::string, KV>::iterator it = gStore.find(ns);
  if (it == gStore.end()) return;
  for (KV::iterator k = it->second.begin(); k != it->second.end(); ++k)
    voce(k->first.c_str(), k->second.tipo, ctx);
}

bool Preferences::begin(const char *name, bool readOnly) {
  ns_ = name;
  ro_ = readOnly;
  open_ = true;
  return true;
}
void Preferences::end() { open_ = false; }
bool Preferences::clear() {
  if (!open_ || ro_) return false;
  gStore[ns_].clear();
  gScritture++;
  return true;
}
bool Preferences::isKey(const char *key) {
  if (!open_) return false;
  KV &kv = gStore[ns_];
  return kv.find(key) != kv.end();
}
bool Preferences::remove(const char *key) {
  if (!open_ || ro_) return false;
  if (gStore[ns_].erase(key) == 0) return false;
  gScritture++;
  return true;
}

static size_t rawPut(std::map<std::string, KV> &store, const std::string &ns, bool open,
                     bool ro, const char *k, uint8_t tipo, const void *v, size_t len) {
  if (!open || ro) return 0;
  const uint8_t *p = (const uint8_t *)v;
  Valore &val = store[ns][k];
  val.tipo = tipo;
  val.dati.assign(p, p + len);
  gScritture++;
  return len;
}

template <class T>
static size_t putScalar(std::map<std::string, KV> &store, const std::string &ns, bool open,
                        bool ro, const char *k, uint8_t tipo, T v) {
  return rawPut(store, ns, open, ro, k, tipo, &v, sizeof(T));
}

template <class T>
static T getScalar(std::map<std::string, KV> &store, const std::string &ns, bool open,
                   const char *k, T def) {
  if (!open) return def;
  KV &kv = store[ns];
  KV::iterator it = kv.find(k);
  if (it == kv.end() || it->second.dati.size() != sizeof(T)) return def;
  T out;
  memcpy(&out, &it->second.dati[0], sizeof(T));
  return out;
}

size_t Preferences::putBool(const char *k, bool v)       { return putScalar(gStore, ns_, open_, ro_, k, T_U8, (uint8_t)(v ? 1 : 0)); }
size_t Preferences::putUChar(const char *k, uint8_t v)   { return putScalar(gStore, ns_, open_, ro_, k, T_U8, v); }
size_t Preferences::putChar(const char *k, int8_t v)     { return putScalar(gStore, ns_, open_, ro_, k, T_I8, v); }
size_t Preferences::putShort(const char *k, int16_t v)   { return putScalar(gStore, ns_, open_, ro_, k, T_I16, v); }
size_t Preferences::putUShort(const char *k, uint16_t v) { return putScalar(gStore, ns_, open_, ro_, k, T_U16, v); }
size_t Preferences::putInt(const char *k, int32_t v)     { return putScalar(gStore, ns_, open_, ro_, k, T_I32, v); }
size_t Preferences::putUInt(const char *k, uint32_t v)   { return putScalar(gStore, ns_, open_, ro_, k, T_U32, v); }
size_t Preferences::putLong64(const char *k, int64_t v)  { return putScalar(gStore, ns_, open_, ro_, k, T_I64, v); }
size_t Preferences::putULong64(const char *k, uint64_t v) { return putScalar(gStore, ns_, open_, ro_, k, T_U64, v); }
size_t Preferences::putBytes(const char *k, const void *v, size_t len) {
  return rawPut(gStore, ns_, open_, ro_, k, T_BLOB, v, len);
}
// come Preferences reale: restituisce strlen, senza il NUL che pure viene salvato
size_t Preferences::putString(const char *k, const char *v) {
  return rawPut(gStore, ns_, open_, ro_, k, T_STR, v, strlen(v) + 1) ? strlen(v) : 0;
}

bool Preferences::getBool(const char *k, bool def)         { return getScalar<uint8_t>(gStore, ns_, open_, k, def ? 1 : 0) != 0; }
uint8_t Preferences::getUChar(const char *k, uint8_t def)  { return getScalar(gStore, ns_, open_, k, def); }
int8_t Preferences::getChar(const char *k, int8_t def)     { return getScalar(gStore, ns_, open_, k, def); }
int16_t Preferences::getShort(const char *k, int16_t def)  { return getScalar(gStore, ns_, open_, k, def); }
uint16_t Preferences::getUShort(const char *k, uint16_t def){ return getScalar(gStore, ns_, open_, k, def); }
int32_t Preferences::getInt(const char *k, int32_t def)    { return getScalar(gStore, ns_, open_, k, def); }
uint32_t Preferences::getUInt(const char *k, uint32_t def) { return getScalar(gStore, ns_, open_, k, def); }
int64_t Preferences::getLong64(const char *k, int64_t def)  { return getScalar(gStore, ns_, open_, k, def); }
uint64_t Preferences::getULong64(const char *k, uint64_t def){ return getScalar(gStore, ns_, open_, k, def); }

// come la NVS reale: solo per i blob, 0 se la chiave manca o ha un altro tipo
size_t Preferences::getBytesLength(const char *k) {
  if (!open_) return 0;
  KV &kv = gStore[ns_];
  KV::iterator it = kv.find(k);
  if (it == kv.end() || it->second.tipo != T_BLOB) return 0;
  return it->second.dati.size();
}

size_t Preferences::getBytes(const char *k, void *buf, size_t maxLen) {
  if (!open_) return 0;
  KV &kv = gStore[ns_];
  KV::iterator it = kv.find(k);
  if (it == kv.end()) return 0;   // come la NVS reale: lascia il buffer intatto
  size_t n = it->second.dati.size() < maxLen ? it->second.dati.size() : maxLen;
  memcpy(buf, &it->second.dati[0], n);
  return n;
}

size_t Preferences::getString(const char *k, char *buf, size_t maxLen) {
  // come Preferences reale: restituisce la lunghezza NUL compreso, e 0 lasciando il
  // buffer intatto se la chiave manca o la stringa non ci sta
  if (!open_ || maxLen == 0) return 0;
  KV &kv = gStore[ns_];
  KV::iterator it = kv.find(k);
  if (it == kv.end() || it->second.dati.size() > maxLen) return 0;
  memcpy(buf, &it->second.dati[0], it->second.dati.size());
  return it->second.dati.size();
}

// ---------------- su file (l'emulatore) e in IndexedDB (la PWA) ----------------
// Una riga per chiave: namespace, chiave, tipo e dati in esadecimale, separati da TAB.
// Le chiavi della NVS non contengono spazi né TAB (al massimo 15 caratteri).

std::string mockNvsTesto() {
  std::string t;
  char buf[48];
  for (std::map<std::string, KV>::iterator n = gStore.begin(); n != gStore.end(); ++n)
    for (KV::iterator k = n->second.begin(); k != n->second.end(); ++k) {
      snprintf(buf, sizeof(buf), "\t%02x\t", k->second.tipo);
      t += n->first + "\t" + k->first + buf;
      for (size_t i = 0; i < k->second.dati.size(); i++) {
        snprintf(buf, sizeof(buf), "%02x", k->second.dati[i]);
        t += buf;
      }
      t += '\n';
    }
  return t;
}

void mockNvsDaTesto(const char *testo) {
  gStore.clear();
  gScritture++;
  while (testo && *testo) {
    const char *fine = strchr(testo, '\n');
    std::string riga(testo, fine ? fine - testo : strlen(testo));
    testo = fine ? fine + 1 : nullptr;
    char *ns = strtok(&riga[0], "\t"), *chiave = strtok(nullptr, "\t");
    char *tipo = strtok(nullptr, "\t"), *hex = strtok(nullptr, "\t");
    if (!ns || !chiave || !tipo) continue;
    Valore &v = gStore[ns][chiave];
    v.tipo = (uint8_t)strtoul(tipo, nullptr, 16);
    v.dati.clear();
    for (size_t i = 0; hex && hex[i] && hex[i + 1]; i += 2) {
      char b[3] = {hex[i], hex[i + 1], 0};
      v.dati.push_back((uint8_t)strtoul(b, nullptr, 16));
    }
  }
}

bool mockNvsSalva(const char *path) {
  FILE *f = fopen(path, "w");
  if (!f) return false;
  std::string t = mockNvsTesto();
  fwrite(t.data(), 1, t.size(), f);
  return fclose(f) == 0;
}

bool mockNvsCarica(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) return false;
  std::string t;
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) t.append(buf, n);
  fclose(f);
  mockNvsDaTesto(t.c_str());
  return true;
}
