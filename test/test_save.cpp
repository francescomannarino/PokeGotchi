// Test dei salvataggi .pgsave (savefile.cpp) sulla NVS finta.
#include "framework.h"
#include "shim/Arduino.h"
#include "../pet.h"
#include "../savefile.h"
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>

// ---------------------------------------------------------------- helper
struct ElencoShim : SaveElencoChiavi {
  void elenca(const char *ns, void (*voce)(const char *, uint8_t, void *), void *ctx) override {
    mockNvsElenca(ns, voce, ctx);
  }
};
static ElencoShim gElenco;

static const uint32_t T0 = 1760000000;  // un'ora RTC qualsiasi

static void advance(Pet &p, uint32_t minutes) {
  for (uint32_t i = 0; i < minutes; i++) {
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
  }
}

// un Pokémon schiuso con un po' di storia, e altre chiavi del namespace (lingua, audio)
static void preparaPet(Pet &p, int16_t dex) {
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(dex);
  p.eggTap();
  p.eggTap();
  p.eggTap();
  p.syncClock(T0);
  advance(p, 90);
  p.rename("SPARKY");
  p.trAtk = 40;
  p.bond = 33;
  Preferences raw;
  raw.begin("pokegotchi", false);
  raw.putUChar("lang", 3);
  raw.putBool("snd", false);
  raw.end();
}

static std::vector<uint8_t> esporta(Pet &p, uint32_t ora) {
  std::vector<uint8_t> buf(SAVE_MAX_BYTES);
  size_t n = saveEsporta(p, ora, "1.17", SAVE_SCHEDA, SAVE_MANUALE, gElenco, &buf[0], buf.size());
  buf.resize(n);
  return buf;
}

// fotografia di tutta la NVS esportata, per confrontarla prima e dopo
static std::vector<uint8_t> fotografia() {
  std::vector<uint8_t> buf(SAVE_MAX_BYTES);
  SaveIntestazione h;
  buf.resize(saveScrivi(h, gElenco, &buf[0], buf.size()));
  return buf;
}

static void ricalcolaCrc(std::vector<uint8_t> &f) {
  uint32_t c = saveCrc32(&f[0], f.size() - 4);
  for (int i = 0; i < 4; i++) f[f.size() - 4 + i] = (uint8_t)(c >> (8 * i));
}

// lo stato salvato nella NVS (blob PET_SAVE_KEY)
static PetSave blobNvs() {
  PetSave s;
  Preferences raw;
  raw.begin("pokegotchi", true);
  CHECK_EQ(raw.getBytes(PET_SAVE_KEY, &s, sizeof(s)), sizeof(s));
  raw.end();
  return s;
}

// in test_pet.cpp: le chiavi singole di una 1.18 e lo stato che ne deve uscire
void writeLegacyKeys();
void checkLegacyPet(const Pet &p);

static int gCopie = 0;
static bool copiaOk(void *) { gCopie++; return true; }
static bool copiaFallita(void *) { gCopie++; return false; }

// ---------------------------------------------------------------- formato
TEST(salvataggio, crc32_come_zlib) {
  CHECK_EQ(saveCrc32((const uint8_t *)"123456789", 9), 0xCBF43926u);
  CHECK_EQ(saveCrc32((const uint8_t *)"", 0), 0u);
  // a pezzi come in un colpo solo
  CHECK_EQ(saveCrc32((const uint8_t *)"6789", 4, saveCrc32((const uint8_t *)"12345", 5)),
           0xCBF43926u);
}

TEST(salvataggio, base64_andata_e_ritorno) {
  const char *casi[] = { "", "f", "fo", "foo", "foob", "fooba", "foobar" };
  const char *attesi[] = { "", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy" };
  for (int i = 0; i < 7; i++) {
    char t[16];
    uint8_t b[16];
    size_t n = strlen(casi[i]);
    CHECK_EQ(saveBase64Codifica((const uint8_t *)casi[i], n, t, sizeof(t)), strlen(attesi[i]));
    CHECK_STREQ(t, attesi[i]);
    CHECK_EQ(saveBase64Decodifica(t, strlen(t), b, sizeof(b)), n);
    CHECK(memcmp(b, casi[i], n) == 0);
  }
  uint8_t b[16];
  CHECK_EQ(saveBase64Decodifica("Zm9v\r\n", 6, b, sizeof(b)), (size_t)3);  // a capo saltati
  CHECK_EQ(saveBase64Decodifica("Zm9v!", 5, b, sizeof(b)), (size_t)0);     // carattere estraneo
  CHECK_EQ(saveBase64Decodifica("Zg==Zg==", 8, b, sizeof(b)), (size_t)0);  // dati dopo '='
  CHECK_EQ(saveBase64Decodifica("Zm9vYmFy", 8, b, 5), (size_t)0);          // buffer corto
  char t[8];
  CHECK_EQ(saveBase64Codifica((const uint8_t *)"foobar", 6, t, 8), (size_t)0);  // manca il NUL
}

TEST(salvataggio, l_intestazione_porta_il_riepilogo) {
  Pet a;
  preparaPet(a, 25);
  std::vector<uint8_t> f = esporta(a, T0 + 5400);
  CHECK(f.size() > (size_t)SAVE_HDR_MIN);
  CHECK(memcmp(&f[0], "PGSV", 4) == 0);
  SaveIntestazione h;
  CHECK_EQ(saveVerifica(&f[0], f.size(), &h), SAVE_OK);
  CHECK_EQ(h.schema, (uint16_t)SAVE_SCHEMA);
  CHECK_EQ(h.lunghezza, (uint32_t)f.size());
  CHECK_EQ(h.epoch, T0 + 5400);
  CHECK_STREQ(h.fw, "1.17");
  CHECK_EQ(h.motivo, (uint8_t)SAVE_MANUALE);
  CHECK_EQ(h.riepilogo.specie, a.speciesId);
  CHECK_EQ(h.riepilogo.livello, a.level());
  CHECK_EQ(h.riepilogo.shiny, a.shiny);
  CHECK_EQ(h.riepilogo.pokedex, a.registeredCount());
  CHECK_STREQ(h.riepilogo.nome, "SPARKY");
  // il riepilogo si legge dai soli primi byte
  SaveIntestazione h2;
  CHECK_EQ(saveLeggiIntestazione(&f[0], SAVE_HDR_MIN, &h2), SAVE_OK);
  CHECK_EQ(h2.riepilogo.livello, a.level());
}

TEST(salvataggio, esportare_allinea_l_orologio) {
  Pet a;
  preparaPet(a, 4);
  esporta(a, T0 + 7200);
  Preferences raw;
  raw.begin("pokegotchi", true);
  CHECK_EQ(raw.getUInt("seen", 0), T0 + 7200);
  raw.end();
  CHECK_EQ(blobNvs().ageMinutes, a.ageMinutes);  // salvato fino a ora
}

TEST(salvataggio, un_buffer_corto_non_produce_un_file_a_meta) {
  Pet a;
  preparaPet(a, 4);
  uint8_t buf[100];
  CHECK_EQ(saveEsporta(a, 0, "1.17", SAVE_SCHEDA, SAVE_MANUALE, gElenco, buf, sizeof(buf)),
           (size_t)0);
}

// ---------------------------------------------------------------- i quattro casi del piano
TEST(salvataggio, andata_e_ritorno_stato_identico_chiave_per_chiave) {
  Pet a;
  preparaPet(a, 25);
  std::vector<uint8_t> f = esporta(a, T0 + 5400);
  std::vector<uint8_t> prima = fotografia();
  CHECK_EQ(blobNvs().speciesId, (int16_t)25);

  // nuova installazione: NVS cancellata, poi un'altra partita
  Pet altro;
  preparaPet(altro, 7);
  gCopie = 0;
  CHECK_STREQ(saveEsitoNome(saveImporta(&f[0], f.size(), T0 + 5400, copiaOk, 0)), "OK");
  CHECK_EQ(gCopie, 1);
  CHECK(fotografia() == prima);  // stesso "seen": byte per byte

  // le chiavi dell'altra partita non restano (la specie è quella importata)
  Pet b;
  b.begin();
  CHECK_EQ(b.speciesId, a.speciesId);
  CHECK_EQ(b.ageMinutes, a.ageMinutes);
  CHECK_EQ(b.trAtk, a.trAtk);
  CHECK_EQ(b.bond, a.bond);
  CHECK_EQ(b.geneAtk, a.geneAtk);
  CHECK_STREQ(b.nick, "SPARKY");
  CHECK_EQ(b.registeredCount(), a.registeredCount());
  Preferences raw;
  raw.begin("pokegotchi", true);
  CHECK_EQ(raw.getUChar("lang", 0), (uint8_t)3);
  CHECK_EQ(raw.getBool("snd", true), false);
  raw.end();
}

TEST(salvataggio, file_rovinati_rifiutati_senza_scrivere_nulla) {
  Pet a;
  preparaPet(a, 25);
  std::vector<uint8_t> buono = esporta(a, T0 + 5400);
  Pet altro;
  preparaPet(altro, 7);
  std::vector<uint8_t> prima = fotografia();

  struct Caso { const char *nome; std::vector<uint8_t> f; SaveEsito atteso; };
  std::vector<Caso> casi;
  std::vector<uint8_t> f = buono;
  f[SAVE_HDR_MIN + 3] ^= 0x40;  // un bit nel corpo
  casi.push_back({ "crc", f, SAVE_ERR_CRC });
  f = buono;
  f.resize(f.size() - 7);
  casi.push_back({ "troncato", f, SAVE_ERR_LUNGHEZZA });
  f = buono;
  f.push_back(0);
  casi.push_back({ "byte in piu", f, SAVE_ERR_LUNGHEZZA });
  f = buono;
  f[8] = SAVE_SCHEMA + 1;  // schema più recente, con un CRC valido
  ricalcolaCrc(f);
  casi.push_back({ "schema", f, SAVE_ERR_SCHEMA });
  f = buono;
  f[4] = SAVE_FORMATO + 1;
  casi.push_back({ "formato", f, SAVE_ERR_FORMATO });
  f = buono;
  f[0] = 'X';
  casi.push_back({ "magic", f, SAVE_ERR_MAGIC });
  f = buono;
  f[SAVE_HDR_MIN + 1 + 8 + 2] = 0x33;  // tipo della prima voce sconosciuto, CRC valido
  ricalcolaCrc(f);
  casi.push_back({ "tipo", f, SAVE_ERR_DATI });

  for (size_t i = 0; i < casi.size(); i++) {
    gCopie = 0;
    SaveEsito e = saveImporta(&casi[i].f[0], casi[i].f.size(), T0, copiaOk, 0);
    CHECK_MSG(e == casi[i].atteso, std::string(casi[i].nome) + ": " + saveEsitoNome(e));
    CHECK_MSG(gCopie == 0, casi[i].nome);  // nemmeno la copia di sicurezza
    CHECK_MSG(fotografia() == prima, casi[i].nome);
  }

  // e se la copia di sicurezza non riesce, non si scrive
  gCopie = 0;
  CHECK_STREQ(saveEsitoNome(saveImporta(&buono[0], buono.size(), T0, copiaFallita, 0)), "COPIA");
  CHECK_EQ(gCopie, 1);
  CHECK(fotografia() == prima);
}

TEST(salvataggio, le_chiavi_di_una_versione_precedente_vengono_migrate) {
  // salvataggio fatto con le chiavi legacy (indici di flash "spec"/"eggT", niente "dexn")
  mockNvsReset();
  Preferences raw;
  raw.begin("pokegotchi", false);
  raw.putBool("init", true);
  raw.putChar("spec", 3);  // OLD2DEX[3] = BULBASAUR
  raw.putChar("eggT", 0);
  raw.putUInt("age", 300);
  raw.putUChar("full", 55);
  raw.end();
  SaveIntestazione h;
  std::vector<uint8_t> f(SAVE_MAX_BYTES);
  f.resize(saveScrivi(h, gElenco, &f[0], f.size()));
  CHECK(f.size() > 0);

  Pet altro;
  preparaPet(altro, 25);  // la NVS attuale ha "dexn": non deve sopravvivere
  CHECK_STREQ(saveEsitoNome(saveImporta(&f[0], f.size(), T0, 0, 0)), "OK");
  Pet b;
  b.begin();
  CHECK_EQ(b.speciesId, (int16_t)1);
  CHECK_EQ(b.ageMinutes, (uint32_t)300);
  CHECK_EQ(b.fullness, (uint8_t)55);
  CHECK(b.isRegistered(1));
  CHECK_RANGE(b.geneAtk, 90, 110);  // pet precedente ai geni: tirati da load()
}

// i file delle versioni a chiavi singole restano importabili
TEST(salvataggio, un_pgsave_a_chiavi_singole_si_importa_e_migra_nel_blob) {
  // file esportato da una 1.18: chiavi singole, schema 1
  mockNvsReset();
  writeLegacyKeys();
  SaveIntestazione h;
  std::vector<uint8_t> f(SAVE_MAX_BYTES);
  f.resize(saveScrivi(h, gElenco, &f[0], f.size()));
  CHECK(f.size() > 0);
  f[8] = 1;
  f[9] = 0;
  ricalcolaCrc(f);

  Pet altro;
  preparaPet(altro, 7);  // la partita attuale, già nel blob: non deve sopravvivere
  CHECK_STREQ(saveEsitoNome(saveImporta(&f[0], f.size(), T0 + 60, 0, 0)), "OK");
  mockSetMillis(0);
  Pet b;
  b.begin();
  checkLegacyPet(b);
  Preferences raw;
  raw.begin("pokegotchi", true);
  CHECK_EQ(raw.getBytesLength(PET_SAVE_KEY), sizeof(PetSave));
  CHECK(!raw.isKey("dexn") && !raw.isKey("age"));
  CHECK_EQ(raw.getUInt("seen", 0), T0 + 60);  // tempo congelato all'importazione
  raw.end();

  // riesportato, il file ha lo schema nuovo e il blob al posto delle chiavi singole
  std::vector<uint8_t> g = esporta(b, 0);
  SaveIntestazione h2;
  CHECK_EQ(saveVerifica(&g[0], g.size(), &h2), SAVE_OK);
  CHECK_EQ(h2.schema, (uint16_t)SAVE_SCHEMA);
  CHECK_EQ(h2.riepilogo.specie, (int16_t)25);
  CHECK(g.size() < f.size());
}

TEST(salvataggio, un_pgsave_col_blob_fa_andata_e_ritorno) {
  Pet a;
  preparaPet(a, 25);
  std::vector<uint8_t> f = esporta(a, T0 + 5400);
  PetSave sa = blobNvs();

  Pet altro;
  preparaPet(altro, 7);
  CHECK_STREQ(saveEsitoNome(saveImporta(&f[0], f.size(), T0 + 5400, 0, 0)), "OK");
  PetSave sb = blobNvs();
  CHECK(memcmp(&sa, &sb, sizeof(sa)) == 0);  // il blob arriva intatto
  CHECK_EQ(mockNvsKeyCount("pokegotchi"), (size_t)5);  // st, init, seen, lang, snd

  mockSetMillis(0);
  Pet b;
  b.begin();
  b.syncClock(T0 + 5400);
  CHECK_EQ(b.speciesId, a.speciesId);
  CHECK_EQ(b.ageMinutes, a.ageMinutes);
  CHECK_EQ(b.fullness, a.fullness);
  CHECK_EQ(b.geneDef, a.geneDef);
  CHECK_EQ(b.trAtk, a.trAtk);
  CHECK_EQ(b.bond, a.bond);
  CHECK_EQ(b.streak, a.streak);
  CHECK_EQ(b.registeredCount(), a.registeredCount());
  CHECK_STREQ(b.nick, "SPARKY");

  // e il file riesportato si reimporta uguale
  std::vector<uint8_t> g = esporta(b, T0 + 5400);
  mockNvsReset();
  CHECK_STREQ(saveEsitoNome(saveImporta(&g[0], g.size(), T0 + 5400, 0, 0)), "OK");
  Pet c;
  c.begin();
  CHECK_EQ(c.speciesId, a.speciesId);
  CHECK_EQ(c.ageMinutes, a.ageMinutes);
  CHECK_STREQ(c.nick, "SPARKY");
}

TEST(salvataggio, tempo_congelato_all_importazione) {
  Pet a;
  preparaPet(a, 25);
  std::vector<uint8_t> f = esporta(a, T0 + 5400);
  uint32_t eta = a.ageMinutes;

  // riflash tre giorni dopo: la NVS è vuota, poi si importa
  mockNvsReset();
  uint32_t dopo = T0 + 5400 + 3 * 86400;
  CHECK_STREQ(saveEsitoNome(saveImporta(&f[0], f.size(), dopo, 0, 0)), "OK");
  mockSetMillis(0);
  Pet b;
  b.begin();
  b.syncClock(dopo);
  CHECK_EQ(b.ageMinutes, eta);
  CHECK_EQ(b.fullness, a.fullness);
  CHECK_EQ(b.joy, a.joy);
  CHECK_EQ(b.energy, a.energy);
  CHECK_EQ(b.hygiene, a.hygiene);

  // senza ora nota "seen" sparisce: anche così nessun minuto applicato
  mockNvsReset();
  CHECK_STREQ(saveEsitoNome(saveImporta(&f[0], f.size(), 0, 0, 0)), "OK");
  Pet c;
  c.begin();
  c.syncClock(dopo);
  CHECK_EQ(c.ageMinutes, eta);
  CHECK_EQ(c.fullness, a.fullness);
}

// ---------------------------------------------------------------- copie sulla SD
TEST(salvataggio, completare_l_intestazione_ricalcola_il_crc) {
  Pet a;
  preparaPet(a, 25);
  std::vector<uint8_t> f = fotografia();  // come la copia "aggiornamento", senza riepilogo
  SaveRiepilogo r = saveRiepilogo(a);
  CHECK(saveCompletaIntestazione(&f[0], f.size(), T0 + 60, r));
  SaveIntestazione h;
  CHECK_EQ(saveVerifica(&f[0], f.size(), &h), SAVE_OK);
  CHECK_EQ(h.epoch, T0 + 60);
  CHECK_EQ(h.riepilogo.specie, a.speciesId);
  CHECK_EQ(h.riepilogo.livello, a.level());
  CHECK_STREQ(h.riepilogo.nome, "SPARKY");
  // un file troncato non si tocca
  CHECK(!saveCompletaIntestazione(&f[0], f.size() - 1, T0, r));
}

TEST(salvataggio, nome_del_file_e_motivo) {
  char n[SAVE_NOME_MAX];
  // 2026-10-21 13:05:09 UTC
  CHECK(saveNomeFile(n, sizeof(n), 1792587909u, SAVE_GIORNALIERA) > 0);
  CHECK_STREQ(n, "20261021-130509-giornaliera.pgsave");
  CHECK_EQ(saveMotivoDaNome(n), (int)SAVE_GIORNALIERA);
  CHECK(saveNomeFile(n, sizeof(n), 951782400u, SAVE_PRIMA_IMPORT) > 0);  // 29 febbraio 2000
  CHECK_STREQ(n, "20000229-000000-importazione.pgsave");
  CHECK_EQ(saveMotivoDaNome(n), (int)SAVE_PRIMA_IMPORT);
  CHECK_EQ(saveNomeFile(n, 10, 0, SAVE_MANUALE), (size_t)0);  // buffer corto
  CHECK_EQ(saveMotivoDaNome("importa.pgsave"), -1);
  CHECK_EQ(saveMotivoDaNome("20261021-130509-manuale.tmp"), -1);
  CHECK_EQ(saveMotivoDaNome("20261021-130509-boh.pgsave"), -1);
  CHECK_EQ(saveMotivoDaNome("2026102x-130509-manuale.pgsave"), -1);
}

TEST(salvataggio, la_rotazione_tiene_dieci_copie_e_l_ultima_di_ogni_motivo) {
  std::vector<std::string> s;
  char n[SAVE_NOME_MAX];
  saveNomeFile(n, sizeof(n), T0, SAVE_AGGIORNAMENTO);
  s.push_back(n);
  saveNomeFile(n, sizeof(n), T0 + 10, SAVE_PRIMA_IMPORT);
  s.push_back(n);
  saveNomeFile(n, sizeof(n), T0 + 15, SAVE_PRIMA_IMPORT);
  s.push_back(n);
  for (int i = 0; i < 12; i++) {
    saveNomeFile(n, sizeof(n), T0 + 100 + i, SAVE_MANUALE);
    s.push_back(n);
  }
  s.push_back("importa.pgsave");  // forma estranea: mai toccato
  std::sort(s.begin(), s.end());
  std::vector<const char *> p;
  for (auto &x : s) p.push_back(x.c_str());
  bool tieni[32];
  saveRotazione(&p[0], (uint16_t)p.size(), tieni);
  int tenute = 0;
  for (size_t i = 0; i < p.size(); i++) {
    if (tieni[i]) tenute++;
    if (strcmp(p[i], "importa.pgsave") == 0) CHECK(tieni[i]);
  }
  CHECK_EQ(tenute, 13);  // 10 manuali + aggiornamento + l'ultima importazione + importa
  CHECK(tieni[0]);       // aggiornamento: unica del suo motivo
  CHECK(!tieni[1]);      // la prima importazione ha una copia più recente
  CHECK(tieni[2]);
  CHECK(!tieni[3] && !tieni[4]);  // le due manuali più vecchie
  CHECK(tieni[5]);
}

// copie datate nel futuro (TIME avanti nelle prove, o RTC rimasto indietro) non
// contano tra le recenti e non si tolgono
TEST(salvataggio, la_rotazione_salta_le_copie_dal_futuro) {
  std::vector<std::string> s;
  char n[SAVE_NOME_MAX];
  for (int i = 0; i < 12; i++) {  // 12 manuali di oggi
    saveNomeFile(n, sizeof(n), T0 + 100 + i, SAVE_MANUALE);
    s.push_back(n);
  }
  for (int i = 0; i < 3; i++) {  // 3 di domani
    saveNomeFile(n, sizeof(n), T0 + 86400 + i, SAVE_MANUALE);
    s.push_back(n);
  }
  std::sort(s.begin(), s.end());
  std::vector<const char *> p;
  for (auto &x : s) p.push_back(x.c_str());
  bool tieni[32];
  saveRotazione(&p[0], (uint16_t)p.size(), tieni, T0 + 200);
  int oggi = 0;
  for (int i = 0; i < 12; i++) oggi += tieni[i];
  CHECK_EQ(oggi, SAVE_TIENI);           // le 10 più recenti di oggi, non 7
  CHECK(!tieni[0] && !tieni[1]);
  CHECK(tieni[12] && tieni[13] && tieni[14]);  // quelle di domani restano
  // senza l'ora, come prima: domani conta tra le recenti
  saveRotazione(&p[0], (uint16_t)p.size(), tieni, 0);
  oggi = 0;
  for (int i = 0; i < 12; i++) oggi += tieni[i];
  CHECK_EQ(oggi, SAVE_TIENI - 3);
  CHECK(saveDalFuturo(p[12], T0 + 200));
  CHECK(!saveDalFuturo(p[11], T0 + 200));
  CHECK(!saveDalFuturo(p[11], T0 + 111 - SAVE_FUTURO_S));  // entro il margine
  CHECK(!saveDalFuturo(p[12], 0));
}

// File di esempio per `tools/salvataggi.py info` (TestSalvataggi in test_tools.py),
// versionato in test/dati/. Seme, epoch e dati fissi: l'output è sempre lo stesso, quindi
// riscriverlo non cambia nulla in git finché non cambia il formato.
TEST(salvataggio, scrive_il_file_di_esempio_per_gli_strumenti) {
  randomSeed(20261021);
  mockClearForcedRandom();
  Pet a;
  preparaPet(a, 25);
  a.grottaHi = 42;  // i record dei giochi a inclinazione (versione 5)
  a.surfHi = 7;
  PetRitratto r = a.ritratto();  // la collezione: un Pokémon conservato
  r.specie = 26;
  strcpy(r.nick, "RAICHU");
  a.colDeposita(r);
  std::vector<uint8_t> f = esporta(a, T0 + 5400);
  CHECK_EQ(saveVerifica(&f[0], f.size()), SAVE_OK);
  FILE *fh = fopen("dati/esempio.pgsave", "wb");
  CHECK(fh != NULL);
  if (!fh) return;
  CHECK_EQ(fwrite(&f[0], 1, f.size(), fh), f.size());
  fclose(fh);
}

// la NVS su file dell'emulatore (tools/emu): tutte le chiavi tornano con tipo e dati
TEST(salvataggio, la_nvs_su_file_torna_identica) {
  Pet a;
  preparaPet(a, 4);
  a.flushSave();
  struct Raccolta {
    std::vector<std::string> v;
    static void voce(const char *k, uint8_t t, void *ctx) {
      Preferences p;
      p.begin("pokegotchi", true);
      size_t n = t == 0x42 ? p.getBytesLength(k) : 0;
      std::vector<uint8_t> b(n);
      if (n) p.getBytes(k, &b[0], n);
      p.end();
      ((Raccolta *)ctx)->v.push_back(std::string(k) + "/" + std::to_string(t) + "/" +
                                     std::string(b.begin(), b.end()));
    }
  } prima, dopo;
  mockNvsElenca("pokegotchi", Raccolta::voce, &prima);
  CHECK(prima.v.size() > 3);
  const char *path = "build/nvs_prova.txt";
  uint32_t w = mockNvsScritture();
  CHECK(mockNvsSalva(path));
  CHECK_EQ(mockNvsScritture(), w);  // salvare non conta come modifica
  mockNvsReset();
  CHECK_EQ(mockNvsKeyCount("pokegotchi"), (size_t)0);
  CHECK(mockNvsCarica(path));
  mockNvsElenca("pokegotchi", Raccolta::voce, &dopo);
  CHECK(prima.v == dopo.v);
  Pet b;
  b.begin();
  CHECK_EQ(b.speciesId, a.speciesId);
  CHECK(strcmp(b.nick, "SPARKY") == 0);
  remove(path);
}

// la collezione è una chiave del namespace del pet: viaggia nel .pgsave
TEST(salvataggio, la_collezione_fa_andata_e_ritorno) {
  Pet a;
  preparaPet(a, 25);
  PetRitratto r = a.ritratto();
  strcpy(r.nick, "CONSERVATO");
  CHECK(a.colDeposita(r));
  std::vector<uint8_t> f = esporta(a, T0 + 5400);

  Pet altro;
  preparaPet(altro, 7);
  CHECK_STREQ(saveEsitoNome(saveImporta(&f[0], f.size(), T0 + 5400, 0, 0)), "OK");
  mockSetMillis(0);
  Pet b;
  b.begin();
  CHECK_EQ(b.colNumero(), (uint8_t)1);
  CHECK_EQ(b.colRitratto(0).specie, (int16_t)25);
  CHECK_STREQ(b.colRitratto(0).nick, "CONSERVATO");
}

TEST(salvataggio, un_pgsave_senza_collezione_la_lascia_vuota) {
  Pet a;
  preparaPet(a, 25);  // nessun deposito: niente chiave "col"
  std::vector<uint8_t> f = esporta(a, T0 + 5400);

  Pet altro;
  preparaPet(altro, 7);
  CHECK(altro.colDeposita(altro.ritratto()));  // la partita attuale ne ha una
  CHECK_STREQ(saveEsitoNome(saveImporta(&f[0], f.size(), T0 + 5400, 0, 0)), "OK");
  mockSetMillis(0);
  Pet b;
  b.begin();
  CHECK_EQ(b.speciesId, (int16_t)25);
  CHECK_EQ(b.colNumero(), (uint8_t)0);
}

TEST(salvataggio, l_identita_dell_allenatore_segue_il_ripristino_su_un_altra_scheda) {
  Pet a;
  preparaPet(a, 25);
  uint32_t id = a.trainerId();
  a.setTrainerName("ROSSO");
  std::vector<uint8_t> f = esporta(a, T0 + 5400);

  Pet altro;  // un'altra scheda: un altro allenatore
  preparaPet(altro, 7);
  altro.setTrainerName("BLU");
  CHECK(altro.trainerId() != id);
  CHECK_STREQ(saveEsitoNome(saveImporta(&f[0], f.size(), T0 + 5400, copiaOk, 0)), "OK");

  Pet b;
  b.begin();
  CHECK_EQ(b.trainerId(), id);
  char nome[13];
  CHECK(b.trainerName(nome, sizeof(nome)));
  CHECK_STREQ(nome, "ROSSO");
}

// ---------------------------------------------------------------- cambio di nome
// Fino alla 2.1 il namespace NVS, il magic e l'estensione avevano il nome vecchio
// (SAVE_NS_VECCHIO, "TPSV", SAVE_ESTENSIONE_VECCHIA): le partite e i file di allora valgono.

// la NVS come la lascia una 2.1: le stesse chiavi, nel namespace col nome vecchio
static void nvsComeVecchia() {
  std::string t = mockNvsTesto(), v, testa = std::string(SAVE_NS_PET) + "\t";
  for (size_t i = 0; i < t.size();) {
    size_t f = t.find('\n', i);
    std::string riga = t.substr(i, f - i + 1);
    i = f + 1;
    if (riga.compare(0, testa.size(), testa) == 0)
      riga = SAVE_NS_VECCHIO + riga.substr(strlen(SAVE_NS_PET));
    v += riga;
  }
  mockNvsDaTesto(v.c_str());
}

TEST(salvataggio, la_partita_col_nome_vecchio_si_migra) {
  Pet a;
  preparaPet(a, 25);
  a.flushSave();
  std::string prima = mockNvsTesto();
  nvsComeVecchia();
  CHECK_EQ(mockNvsKeyCount(SAVE_NS_PET), (size_t)0);
  CHECK(mockNvsKeyCount(SAVE_NS_VECCHIO) > 3);
  CHECK(saveMigraNamespace(gElenco));
  CHECK(mockNvsTesto() == prima);  // stesse chiavi, tipi e valori; il vecchio è vuoto
  CHECK_EQ(mockNvsKeyCount(SAVE_NS_VECCHIO), (size_t)0);
  Pet b;
  b.begin();
  CHECK_EQ(b.speciesId, a.speciesId);
  CHECK(strcmp(b.nick, "SPARKY") == 0);
  CHECK(!saveMigraNamespace(gElenco));  // la seconda volta non c'è niente da fare
}

TEST(salvataggio, la_migrazione_non_tocca_una_partita_gia_nuova) {
  Pet a;
  preparaPet(a, 25);
  a.flushSave();
  Preferences raw;
  raw.begin(SAVE_NS_VECCHIO, false);
  raw.putUChar("lang", 1);
  raw.end();
  std::string prima = mockNvsTesto();
  CHECK(!saveMigraNamespace(gElenco));
  CHECK(mockNvsTesto() == prima);
}

TEST(salvataggio, un_salvataggio_della_2_1_si_importa) {
  Pet a;
  preparaPet(a, 25);
  std::vector<uint8_t> f = esporta(a, T0 + 5400);
  CHECK_EQ(f[SAVE_HDR_MIN], (uint8_t)strlen(SAVE_NS_PET));
  // com'era scritto da una 2.1: magic "TPSV" e il blocco del namespace col nome vecchio
  std::vector<uint8_t> v(f.begin(), f.begin() + SAVE_HDR_MIN);
  memcpy(&v[0], "TPSV", 4);
  size_t lv = strlen(SAVE_NS_VECCHIO);
  v.push_back((uint8_t)lv);
  v.insert(v.end(), SAVE_NS_VECCHIO, SAVE_NS_VECCHIO + lv);
  v.insert(v.end(), f.begin() + SAVE_HDR_MIN + 1 + strlen(SAVE_NS_PET), f.end());
  uint32_t tot = (uint32_t)v.size();
  for (int i = 0; i < 4; i++) v[12 + i] = (uint8_t)(tot >> (8 * i));
  ricalcolaCrc(v);
  CHECK_EQ(saveVerifica(&v[0], v.size()), SAVE_OK);
  mockNvsReset();
  CHECK_EQ(saveImporta(&v[0], v.size(), T0 + 5400, nullptr, nullptr), SAVE_OK);
  CHECK_EQ(mockNvsKeyCount(SAVE_NS_VECCHIO), (size_t)0);
  Pet b;
  b.begin();
  CHECK_EQ(b.speciesId, a.speciesId);
  CHECK(strcmp(b.nick, "SPARKY") == 0);
}

TEST(salvataggio, le_copie_col_nome_vecchio_restano_nell_elenco) {
  CHECK(saveNomeHaEstensione("20261021-130509-giornaliera" SAVE_ESTENSIONE));
  CHECK(saveNomeHaEstensione("20261021-130509-giornaliera" SAVE_ESTENSIONE_VECCHIA));
  CHECK(!saveNomeHaEstensione("20261021-130509-giornaliera.tmp"));
  CHECK_EQ(saveMotivoDaNome("20261021-130509-giornaliera" SAVE_ESTENSIONE_VECCHIA),
           (int)SAVE_GIORNALIERA);
}
