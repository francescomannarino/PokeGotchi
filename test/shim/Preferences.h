// Shim dell'API Preferences (NVS dell'ESP32) in memoria. Condivide un unico archivio
// globale tra le istanze, come la NVS reale, per poter testare salvataggio/caricamento
// e persistenza tra "avvii".
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>

class Preferences {
public:
  bool begin(const char *name, bool readOnly = false);
  void end();
  bool clear();
  bool isKey(const char *key);
  bool remove(const char *key);

  size_t putBool(const char *k, bool v);
  size_t putUChar(const char *k, uint8_t v);
  size_t putChar(const char *k, int8_t v);
  size_t putShort(const char *k, int16_t v);
  size_t putUShort(const char *k, uint16_t v);
  size_t putInt(const char *k, int32_t v);
  size_t putUInt(const char *k, uint32_t v);
  size_t putLong64(const char *k, int64_t v);
  size_t putULong64(const char *k, uint64_t v);
  size_t putBytes(const char *k, const void *v, size_t len);
  size_t putString(const char *k, const char *v);

  bool getBool(const char *k, bool def = false);
  uint8_t getUChar(const char *k, uint8_t def = 0);
  int8_t getChar(const char *k, int8_t def = 0);
  int16_t getShort(const char *k, int16_t def = 0);
  uint16_t getUShort(const char *k, uint16_t def = 0);
  int32_t getInt(const char *k, int32_t def = 0);
  uint32_t getUInt(const char *k, uint32_t def = 0);
  int64_t getLong64(const char *k, int64_t def = 0);
  uint64_t getULong64(const char *k, uint64_t def = 0);
  size_t getBytesLength(const char *k);
  size_t getBytes(const char *k, void *buf, size_t maxLen);
  size_t getString(const char *k, char *buf, size_t maxLen);

private:
  std::string ns_;
  bool open_ = false;
  bool ro_ = false;
};

// helper di test: cancella tutta la "NVS" simulata
void mockNvsReset();
// sceglie l'archivio del dispositivo k (0 all'avvio): due schede nello stesso test hanno
// ciascuna la sua NVS, e i `Preferences` già aperti scrivono in quella scelta
void mockNvsDispositivo(int k);
size_t mockNvsKeyCount(const char *ns);
// enumera le chiavi di un namespace con il loro tipo, codificato come `nvs_type_t`
// dell'ESP-IDF (0x01 U8, 0x11 I8, 0x02 U16, 0x12 I16, 0x04 U32, 0x14 I32, 0x08 U64,
// 0x18 I64, 0x21 STR, 0x42 BLOB): fa le veci dell'iteratore `nvs_entry_find`/`next`
void mockNvsElenca(const char *ns, void (*voce)(const char *chiave, uint8_t tipo, void *ctx),
                   void *ctx);
// l'emulatore su PC: la "NVS" su file di testo; true se riuscito. mockNvsScritture()
// cresce a ogni modifica
bool mockNvsSalva(const char *path);
bool mockNvsCarica(const char *path);
// lo stesso testo in memoria (la PWA lo tiene in IndexedDB); mockNvsDaTesto sostituisce
// tutto l'archivio
std::string mockNvsTesto();
void mockNvsDaTesto(const char *testo);
uint32_t mockNvsScritture();
