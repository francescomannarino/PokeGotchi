// Test dello scambio in collegamento (linkscambio.cpp e Pet::riceviScambio): due schede,
// ognuna con la sua NVS finta, sul canale simulato. Interruzione a ogni passo, riavvio
// simulato (Pet e LinkScambio nuovi dalla NVS) e riconnessione, offrendo il pet attivo o
// uno della collezione. Invariante: ogni pet esiste in esattamente un dispositivo.
#include "framework.h"
#include "canale_link.h"
#include "../linkscambio.h"
#include "../pet.h"
#include "../dex.h"
#include "../savefile.h"
#include <map>
#include <string>
#include <vector>

static PetRitratto rit(int16_t specie, const char *nick, uint8_t gene = 100) {
  PetRitratto r;
  r.specie = specie;
  r.ageMinutes = 60u * 45;
  r.geneAtk = gene;
  r.geneDef = (uint8_t)(gene - 3);
  r.geneSpe = (uint8_t)(gene + 2);
  r.trAtk = 12;
  r.bond = 55;
  strncpy(r.nick, nick, RIT_NOME - 1);
  return r;
}

struct Scheda {
  Nodo n;
  int nvs = 0;
  Pet *pet = nullptr;
  LinkScambio *ls = nullptr;
  ~Scheda() {
    delete ls;
    delete pet;
  }
};

static void usa(Scheda &s) { mockNvsDispositivo(s.nvs); }

// accensione: il Pet dalla sua NVS, poi il giornale
static void accendi(Scheda &s) {
  usa(s);
  delete s.ls;
  delete s.pet;
  s.pet = new Pet;
  s.pet->begin();
  s.ls = new LinkScambio;
  s.ls->carica(*s.pet);
}

// una scheda nuova con `attivo` come pet attivo (nullptr = un uovo) e la collezione `col`
static void preparaScheda(Scheda &s, int nvs, const PetRitratto *attivo,
                          const std::vector<PetRitratto> &col) {
  s.nvs = nvs;
  usa(s);
  mockNvsReset();
  {
    Preferences raw;  // lo stesso trainerId del Link (canale_link.h), come sulla scheda
    raw.begin("pokegotchi", false);
    raw.putUInt("tid", 0x1000u + nvs);
    raw.end();
    Pet p;
    p.begin();
    if (attivo) {
      CHECK(p.colDeposita(*attivo));
      CHECK(p.colRiprendi(0));  // l'uovo si scarta, il ritratto diventa il pet attivo
    }
    for (const PetRitratto &r : col) CHECK(p.colDeposita(r));
  }
  accendi(s);
}

static void pompa(Scheda &s) {
  usa(s);
  LinkMessaggio m;
  while (s.n.lk.prendi(m)) CHECK(s.ls->ricevi(m));
  s.ls->tick();
}

// A (ASH): Charizard attivo, Kadabra in collezione; B (GARY): Haunter attivo, Lapras e
// Machoke in collezione. trainerId 0x1001 e 0x1002 (canale_link.h).
struct Banco {
  Canale c;
  Scheda a, b;
  explicit Banco(uint32_t seme = 1, bool uovoA = false) {
    c.rng = seme;
    prepara(c, a.n, 1, 25, "ASH");
    prepara(c, b.n, 2, 133, "GARY");
    const PetRitratto attA = rit(6, "A0", 101), attB = rit(93, "B0", 95);
    preparaScheda(a, 1, uovoA ? nullptr : &attA, {rit(64, "A1", 104)});
    preparaScheda(b, 2, &attB, {rit(131, "B1", 99), rit(67, "B2", 92)});
  }
  void passo(uint32_t ms = 5) {
    c.passo(ms);
    pompa(a);
    pompa(b);
  }
  void corri(uint32_t ms, uint32_t p = 5) {
    for (uint32_t t = 0; t < ms; t += p) passo(p);
  }
  bool collega() {
    usa(a);
    avvia(c, a.n);
    usa(b);
    avvia(c, b.n);
    if (!abbina(c, a.n, b.n, 30000)) return false;
    usa(a);
    a.ls->avvia(a.n.lk, 11);
    usa(b);
    b.ls->avvia(b.n.lk, 22);
    return true;
  }
  // il collegamento cade: ciò che è in volo si perde
  void stacca() {
    c.sordo = true;
    for (Scheda *s : {&a, &b}) {
      usa(*s);
      s->ls->ferma();
      s->n.lk.chiudi();
    }
    c.voli.clear();
    c.sordo = false;
  }
  // offerte e trattativa fino alle due offerte arrivate
  bool offri(int8_t pa, int8_t pb) {
    usa(a);
    if (!a.ls->offri(pa)) return false;
    usa(b);
    if (!b.ls->offri(pb)) return false;
    for (int i = 0; i < 2000; i++) {
      passo();
      if (a.ls->offertaLoro() && b.ls->offertaLoro()) return true;
    }
    return false;
  }
  void accettano() {
    usa(a);
    CHECK(a.ls->accetta());
    usa(b);
    CHECK(b.ls->accetta());
  }
  // corre finché nessuno dei due ha più un giornale né una trattativa in corso
  bool risolvi(uint32_t maxMs = 20000) {
    for (uint32_t t = 0; t < maxMs; t += 5) {
      passo();
      if (!a.ls->giornale() && !b.ls->giornale() && a.ls->fase() != LSF_OFFERTA &&
          b.ls->fase() != LSF_OFFERTA)
        return true;
    }
    return false;
  }
};

// i soprannomi dei pet di una scheda, letti dalla sua NVS (attivo e collezione)
static std::vector<std::string> petDi(Scheda &s) {
  usa(s);
  Pet q;
  q.begin();
  std::vector<std::string> v;
  if (!q.isEgg()) v.push_back(q.nick);
  for (uint8_t i = 0; i < q.colNumero(); i++) v.push_back(q.colRitratto(i).nick);
  return v;
}

static bool ha(const std::vector<std::string> &v, const char *nick) {
  for (const std::string &x : v)
    if (x == nick) return true;
  return false;
}

// ogni pet in esattamente un dispositivo; restituisce true se lo scambio pa/pb è avvenuto
static bool invariante(Banco &k, const char *offA, const char *offB) {
  std::vector<std::string> va = petDi(k.a), vb = petDi(k.b);
  std::map<std::string, int> conta;
  for (auto &x : va) conta[x]++;
  for (auto &x : vb) conta[x]++;
  for (const char *n : {"A0", "A1", "B0", "B1", "B2"}) CHECK_EQ(conta[n], 1);
  CHECK_EQ(conta.size(), (size_t)5);
  const bool fatto = ha(va, offB);
  CHECK_EQ(ha(vb, offA), fatto);  // o tutti e due o nessuno
  return fatto;
}

// ---------------------------------------------------------------- giornale

TEST(scambio, il_giornale_va_e_torna_e_il_crc_rovinato_si_rifiuta) {
  LsGiornale g;
  g.stato = LSF_PREPARATO;
  g.tradeId = 0xCAFE1234;
  g.posto = 3;
  g.partner = 0x1002;
  strcpy(g.nome, "GARY");
  g.offerto = rit(64, "A1");
  g.ricevuto = rit(131, "B1");
  uint8_t b[LS_GIORNALE_BYTES];
  CHECK_EQ(lsScriviGiornale(g, b, sizeof(b)), (size_t)LS_GIORNALE_BYTES);
  LsGiornale r;
  CHECK(lsLeggiGiornale(b, sizeof(b), r));
  CHECK_EQ(r.tradeId, g.tradeId);
  CHECK_EQ(r.posto, (int8_t)3);
  CHECK_EQ(r.partner, (uint32_t)0x1002);
  CHECK_STREQ(r.nome, "GARY");
  CHECK_EQ(r.offerto.specie, (int16_t)64);
  CHECK_STREQ(r.ricevuto.nick, "B1");
  g.posto = SCAMBIO_ATTIVO;
  lsScriviGiornale(g, b, sizeof(b));
  CHECK(lsLeggiGiornale(b, sizeof(b), r));
  CHECK_EQ(r.posto, (int8_t)SCAMBIO_ATTIVO);
  b[40] ^= 1;
  CHECK(!lsLeggiGiornale(b, sizeof(b), r));
  CHECK(!lsLeggiGiornale(b, sizeof(b) - 1, r));
}

// ---------------------------------------------------------------- scambio riuscito

TEST(scambio, attivo_contro_attivo_su_un_canale_perfetto) {
  Banco k;
  CHECK(k.collega());
  CHECK(k.offri(SCAMBIO_ATTIVO, SCAMBIO_ATTIVO));
  CHECK_EQ(k.a.ls->loro().specie, (int16_t)93);  // ognuno vede il pet dell'altro
  CHECK_EQ(k.b.ls->loro().specie, (int16_t)6);
  CHECK_EQ(k.a.ls->tradeId(), k.b.ls->tradeId());
  const uint32_t id = k.a.ls->tradeId();
  CHECK(id != 0);
  k.accettano();
  CHECK(k.risolvi());
  CHECK_EQ(k.a.ls->fase(), LSF_FATTO);
  CHECK_EQ(k.b.ls->fase(), LSF_FATTO);
  CHECK(invariante(k, "A0", "B0"));
  // effetti sul pet ricevuto
  usa(k.a);
  Pet &p = *k.a.pet;
  CHECK_EQ(p.speciesId, (int16_t)94);  // Haunter evolve alla ricezione
  CHECK_STREQ(p.nick, "B0");
  CHECK_EQ(p.ageMinutes, (uint32_t)(60u * 45));
  CHECK_EQ(p.geneAtk, (uint8_t)95);
  CHECK_EQ(p.trAtk, (uint8_t)12);
  CHECK_EQ(p.bond, (uint8_t)0);
  CHECK_EQ(p.fullness, (uint8_t)80);
  CHECK_EQ(p.otId, (uint32_t)0x1002);  // AO: GARY
  CHECK_STREQ(p.otName, "GARY");
  CHECK(p.isRegistered(93));
  CHECK(p.isRegistered(94));
  CHECK(!p.canFarewellNow());
  CHECK_EQ(p.scambioBloccato(), (int8_t)SCAMBIO_NESSUNO);
  CHECK(k.a.ls->applicato(id));
  CHECK(p.scambioApplicato(id));
  CHECK_EQ(p.evoScambioDa(), (int16_t)93);  // la cerimonia la mostra la UI
  CHECK_EQ(p.evoScambioPosto(), (int8_t)SCAMBIO_ATTIVO);
  p.avviaEvoScambio();
  CHECK(p.evolving());
  CHECK_EQ(p.prevSpeciesId, (int16_t)93);
  CHECK_EQ(p.evoScambioDa(), (int16_t)0);
  usa(k.b);
  CHECK_EQ(k.b.pet->speciesId, (int16_t)6);  // Charizard non evolve
  CHECK_EQ(k.b.pet->evoScambioDa(), (int16_t)0);
  CHECK_EQ(k.b.ls->scartati(), (uint16_t)0);
  CHECK_EQ(k.a.ls->persi(), (uint16_t)0);
  CHECK_EQ(mockNvsKeyCount(LS_NS), (size_t)1);  // resta solo "fatti"
  // l'altro chiude subito: l'esito resta da guardare (la UI lo mostra fino a OK)
  k.stacca();
  CHECK_EQ(k.a.ls->fase(), LSF_FATTO);
  CHECK_EQ(k.a.ls->ricevuto().specie, (int16_t)94);
}

TEST(scambio, attivo_contro_collezione_e_kadabra_evolve_anche_in_collezione) {
  Banco k;
  CHECK(k.collega());
  CHECK(k.offri(0, 0));  // A: Kadabra della collezione; B: Lapras della collezione
  k.accettano();
  CHECK(k.risolvi());
  CHECK(invariante(k, "A1", "B1"));
  usa(k.b);
  CHECK_EQ(k.b.pet->colRitratto(0).specie, (int16_t)65);  // Alakazam
  CHECK_STREQ(k.b.pet->colRitratto(0).nick, "A1");
  CHECK_EQ(k.b.pet->colRitratto(0).bond, (uint8_t)0);
  CHECK_EQ(k.b.pet->colRitratto(0).otId, (uint32_t)0x1001);
  CHECK(k.b.pet->isRegistered(64) && k.b.pet->isRegistered(65));
  CHECK_EQ(k.b.pet->evoScambioDa(), (int16_t)64);
  CHECK_EQ(k.b.pet->evoScambioPosto(), (int8_t)0);
  CHECK_EQ(k.b.pet->speciesId, (int16_t)93);  // il pet attivo non si tocca
  usa(k.a);
  CHECK_EQ(k.a.pet->colRitratto(0).specie, (int16_t)131);
  CHECK_EQ(k.a.pet->speciesId, (int16_t)6);
  // un secondo scambio nella stessa sessione: il pet attivo di A contro Machoke di B. Prima
  // la cerimonia dell'evoluzione di B, che finché c'è blocca un nuovo scambio
  usa(k.b);
  CHECK_EQ(k.b.pet->scambioPerche(1), SCB_OCCUPATO);
  CHECK(!k.b.ls->offri(1));
  k.b.pet->avviaEvoScambio();
  CHECK(!k.b.pet->evolving());  // in collezione: l'animazione la disegna la UI
  CHECK(k.offri(SCAMBIO_ATTIVO, 1));
  k.accettano();
  CHECK(k.risolvi());
  usa(k.a);
  CHECK_EQ(k.a.pet->speciesId, (int16_t)68);  // Machamp
  std::vector<std::string> va = petDi(k.a), vb = petDi(k.b);
  CHECK(ha(va, "B2") && ha(va, "B1") && ha(vb, "A0") && ha(vb, "A1"));
}

TEST(scambio, si_puo_offrire_dalla_collezione_con_un_uovo_attivo) {
  Banco k(3, true);
  CHECK(k.collega());
  usa(k.a);
  CHECK_EQ(k.a.pet->scambioPerche(SCAMBIO_ATTIVO), SCB_UOVO);
  CHECK(!k.a.ls->offri(SCAMBIO_ATTIVO));
  CHECK(k.offri(0, SCAMBIO_ATTIVO));
  k.accettano();
  CHECK(k.risolvi());
  usa(k.a);
  CHECK(k.a.pet->isEgg());
  CHECK_EQ(k.a.pet->colRitratto(0).specie, (int16_t)94);
  usa(k.b);
  CHECK_EQ(k.b.pet->speciesId, (int16_t)65);
}

TEST(scambio, un_pet_che_torna_a_casa_riprende_l_allenatore_io) {
  Banco k;
  CHECK(k.collega());
  CHECK(k.offri(SCAMBIO_ATTIVO, SCAMBIO_ATTIVO));
  k.accettano();
  CHECK(k.risolvi());
  usa(k.a);
  k.a.pet->avviaEvoScambio();
  CHECK_EQ(k.a.pet->scambioPerche(SCAMBIO_ATTIVO), SCB_OCCUPATO);  // evolve a schermo
  mockAdvanceMillis(EVOLVE_ANIM_MS + 1);
  CHECK(k.offri(SCAMBIO_ATTIVO, SCAMBIO_ATTIVO));  // e indietro
  k.accettano();
  CHECK(k.risolvi());
  usa(k.a);
  CHECK_STREQ(k.a.pet->nick, "A0");
  CHECK_EQ(k.a.pet->otId, (uint32_t)0);
  CHECK_STREQ(k.a.pet->otName, "");
  usa(k.b);
  CHECK_STREQ(k.b.pet->nick, "B0");
  CHECK_EQ(k.b.pet->speciesId, (int16_t)94);  // evoluto da A: resta Gengar
}

TEST(scambio, su_un_canale_che_perde_duplica_e_riordina) {
  for (uint32_t seme = 1; seme <= 12; seme++) {
    Banco k(seme);
    k.c.perdita = 30;
    k.c.doppi = 20;
    k.c.ritardoMax = 40;
    CHECK(k.collega());
    CHECK(k.offri(seme % 2 ? SCAMBIO_ATTIVO : 0, seme % 3 ? SCAMBIO_ATTIVO : 1));
    k.accettano();
    CHECK(k.risolvi(60000));
    CHECK_EQ(k.a.ls->fase(), LSF_FATTO);
    CHECK_EQ(k.b.ls->fase(), LSF_FATTO);
    CHECK(invariante(k, seme % 2 ? "A0" : "A1", seme % 3 ? "B0" : "B2"));
  }
}

// ---------------------------------------------------------------- regole

TEST(scambio, rifiuto_e_cambio_di_offerta) {
  Banco k;
  CHECK(k.collega());
  CHECK(k.offri(SCAMBIO_ATTIVO, SCAMBIO_ATTIVO));
  usa(k.a);
  CHECK_EQ(k.a.pet->scambioBloccato(), (int8_t)SCAMBIO_ATTIVO);
  usa(k.b);
  k.b.ls->annulla();  // B ci ripensa
  CHECK_EQ(k.b.ls->fase(), LSF_LIBERO);
  CHECK_EQ(k.b.ls->motivo(), LSM_RITIRATO);
  k.corri(200);
  CHECK_EQ(k.a.ls->fase(), LSF_LIBERO);
  CHECK_EQ(k.a.ls->motivo(), LSM_ANNULLATO);
  usa(k.a);
  CHECK_EQ(k.a.pet->scambioBloccato(), (int8_t)SCAMBIO_NESSUNO);
  // B accetta, poi A cambia offerta: l'accettazione vecchia non vale
  CHECK(k.offri(SCAMBIO_ATTIVO, SCAMBIO_ATTIVO));
  usa(k.b);
  CHECK(k.b.ls->accetta());
  k.corri(200);
  CHECK(k.a.ls->accettatoLoro());
  usa(k.a);
  CHECK(k.a.ls->offri(0));
  CHECK(!k.a.ls->accettatoLoro());
  k.corri(200);
  CHECK(!k.b.ls->accettatoIo());
  CHECK_EQ(k.b.ls->loro().specie, (int16_t)64);
  usa(k.a);
  CHECK(k.a.ls->accetta());
  k.corri(500);
  CHECK_EQ(k.a.ls->fase(), LSF_OFFERTA);  // B non ha ancora accettato Kadabra
  CHECK(!k.a.ls->giornale());
  usa(k.b);
  CHECK(k.b.ls->accetta());
  CHECK(k.risolvi());
  CHECK(invariante(k, "A1", "B0"));
}

TEST(scambio, niente_uova_ne_cerimonie_e_il_pet_offerto_resta_fermo) {
  Banco k;
  CHECK(k.collega());
  usa(k.a);
  Pet &p = *k.a.pet;
  CHECK_EQ(p.scambioPerche(4), SCB_POSTO);
  p.startFarewell();
  CHECK_EQ(p.scambioPerche(SCAMBIO_ATTIVO), SCB_OCCUPATO);
  CHECK_EQ(p.scambioPerche(0), SCB_OCCUPATO);  // nessuno scambio durante una cerimonia
  CHECK(!k.a.ls->offri(0));
  accendi(k.a);  // l'addio interrotto da un riavvio non riparte (senza domanda in sospeso)
  usa(k.a);
  k.a.ls->avvia(k.a.n.lk, 9);
  Pet &q = *k.a.pet;
  CHECK_EQ(q.ceremony, (uint8_t)CER_NONE);
  q.ageMinutes = 3 * FAREWELL_AGE_MIN;  // Charizard pronto all'addio (anche dopo RIPRENDI)
  CHECK(q.canFarewellNow());
  CHECK(k.a.ls->offri(SCAMBIO_ATTIVO));
  CHECK(!q.canFarewellNow());
  CHECK(!q.canEvolveNow());
  CHECK_EQ(q.colPerche(0), COL_OCCUPATO);
  q.release();
  CHECK_EQ(q.ceremony, (uint8_t)CER_NONE);
  q.startFarewell();
  CHECK_EQ(q.ceremony, (uint8_t)CER_NONE);
  CHECK_EQ(q.scambioPerche(0), SCB_BLOCCATO);
  // offerta della collezione: il posto non può scalare
  CHECK(k.a.ls->offri(0));
  CHECK_EQ(q.scambioBloccato(), (int8_t)0);
  CHECK(q.canFarewellNow());
  CHECK(!q.colLiberabile(0));  // il pulsante LIBERA è grigio
  CHECK(!q.colLiberabile(1));
  CHECK(!q.colLibera(0));
  k.a.ls->annulla();
  CHECK_EQ(q.scambioBloccato(), (int8_t)SCAMBIO_NESSUNO);
  CHECK(q.colLiberabile(0));
  CHECK(q.colLibera(0));
}

TEST(scambio, il_giornale_non_entra_nei_salvataggi) {
  Banco k;
  CHECK(k.collega());
  CHECK(k.offri(SCAMBIO_ATTIVO, SCAMBIO_ATTIVO));
  k.accettano();
  for (int i = 0; i < 400 && !(k.a.ls->giornale() && !k.a.ls->applicato(k.a.ls->tradeId())); i++) {
    k.c.passo(1);
    pompa(k.a);  // solo A: B non risponde, A resta PREPARATO
  }
  CHECK_EQ(k.a.ls->fase(), LSF_PREPARATO);
  usa(k.a);
  CHECK(mockNvsKeyCount(LS_NS) >= 1);
  struct Elenco : SaveElencoChiavi {
    void elenca(const char *ns, void (*voce)(const char *, uint8_t, void *), void *ctx) override {
      mockNvsElenca(ns, voce, ctx);
    }
  } el;
  std::vector<uint8_t> buf(SAVE_MAX_BYTES);
  size_t n = saveEsporta(*k.a.pet, 0, "1.20", SAVE_SCHEDA, SAVE_PRIMA_SCAMBIO, el, &buf[0], buf.size());
  CHECK(n > 0);
  const std::string f(buf.begin(), buf.begin() + n);
  CHECK(f.find(LS_NS) == std::string::npos);
  CHECK(f.find(LS_GIORNALE_KEY) == std::string::npos);
  CHECK(f.find("pokegotchi") != std::string::npos);
  for (const char *ns : SAVE_NAMESPACE) CHECK(strcmp(ns, LS_NS) != 0);
}

TEST(scambio, la_copia_prima_dello_scambio_che_fallisce_annulla) {
  Banco k;
  CHECK(k.collega());
  int chiamate = 0;
  k.b.ls->copiaPrima([](void *ctx) { ++*(int *)ctx; return false; }, &chiamate);
  CHECK(k.offri(SCAMBIO_ATTIVO, SCAMBIO_ATTIVO));
  k.accettano();
  CHECK(k.risolvi());
  CHECK_EQ(chiamate, 1);
  CHECK_EQ(k.a.ls->fase(), LSF_LIBERO);
  CHECK_EQ(k.a.ls->motivo(), LSM_COPIA);
  CHECK_EQ(k.b.ls->motivo(), LSM_COPIA);
  CHECK(!invariante(k, "A0", "B0"));
}

// ---------------------------------------------------------------- interruzioni

// Il collegamento cade dopo `passi` ms dalle accettazioni; poi si riavvia nessuno, A, B o
// tutti e due, e ci si ricollega. Ogni taglio deve finire coerente senza annullamenti a mano.
static void tagliaERiprendi(uint32_t passi, int riavvio, int8_t pa, int8_t pb, uint32_t seme) {
  Banco k(seme);
  CHECK(k.collega());
  CHECK(k.offri(pa, pb));
  k.accettano();
  for (uint32_t i = 0; i < passi; i++) k.passo(1);
  k.stacca();
  if (riavvio & 1) accendi(k.a);
  if (riavvio & 2) accendi(k.b);
  const char *offA = pa == SCAMBIO_ATTIVO ? "A0" : "A1";
  const char *offB = pb == SCAMBIO_ATTIVO ? "B0" : (pb == 0 ? "B1" : "B2");
  // scollegati: ognuno ha ancora i suoi pet o quelli già scambiati, mai un doppione
  invariante(k, offA, offB);
  CHECK(k.collega());
  CHECK(k.risolvi());
  CHECK(!k.a.ls->giornale() && !k.b.ls->giornale());
  invariante(k, offA, offB);
  usa(k.a);
  CHECK_EQ(k.a.pet->scambioBloccato(), (int8_t)SCAMBIO_NESSUNO);
  usa(k.b);
  CHECK_EQ(k.b.pet->scambioBloccato(), (int8_t)SCAMBIO_NESSUNO);
}

TEST(scambio, interruzione_a_ogni_passo_con_riavvio_e_riconnessione) {
  // quanti ms dura lo scambio sul canale perfetto
  uint32_t durata = 0;
  {
    Banco k;
    CHECK(k.collega());
    CHECK(k.offri(SCAMBIO_ATTIVO, 0));
    k.accettano();
    while (durata < 2000 && (k.a.ls->fase() != LSF_FATTO || k.b.ls->fase() != LSF_FATTO)) {
      k.passo(1);
      durata++;
    }
    CHECK(durata < 2000);
  }
  int fatti = 0, annullati = 0;
  for (uint32_t t = 0; t <= durata + 1; t++)
    for (int r = 0; r < 4; r++) {
      tagliaERiprendi(t, r, SCAMBIO_ATTIVO, 0, 5);
      tagliaERiprendi(t, r, 0, SCAMBIO_ATTIVO, 6);
    }
  // e un campione di tagli di entrambi i tipi, per vedere che si arriva sia al fatto sia
  // all'annullato
  for (uint32_t t = 0; t <= durata + 1; t++) {
    Banco k(7);
    CHECK(k.collega());
    CHECK(k.offri(SCAMBIO_ATTIVO, SCAMBIO_ATTIVO));
    k.accettano();
    for (uint32_t i = 0; i < t; i++) k.passo(1);
    k.stacca();
    accendi(k.a);
    accendi(k.b);
    CHECK(k.collega());
    CHECK(k.risolvi());
    (invariante(k, "A0", "B0") ? fatti : annullati)++;
  }
  CHECK(fatti > 0);
  CHECK(annullati > 0);
}

// un taglio che lascia A preparato e B no (B non ha ricevuto l'ACCETTA di A)
static uint32_t soloAPreparato(Banco &k) {
  CHECK(k.collega());
  CHECK(k.offri(SCAMBIO_ATTIVO, SCAMBIO_ATTIVO));
  usa(k.b);
  CHECK(k.b.ls->accetta());
  k.corri(100);
  usa(k.a);
  CHECK(k.a.ls->accetta());  // A ha già l'ACCETTA di B: prepara subito
  CHECK_EQ(k.a.ls->fase(), LSF_PREPARATO);
  const uint32_t id = k.a.ls->tradeId();
  k.stacca();               // l'ACCETTA e il COMMIT di A non arrivano
  return id;
}

TEST(scambio, la_ripresa_annulla_se_l_altro_non_aveva_preparato) {
  Banco k;
  soloAPreparato(k);
  accendi(k.a);
  CHECK_EQ(k.a.ls->fase(), LSF_SOSPESO);
  usa(k.a);
  CHECK_EQ(k.a.pet->scambioBloccato(), (int8_t)SCAMBIO_ATTIVO);  // bloccato dal giornale
  CHECK(!k.a.pet->canEvolveNow());
  CHECK(k.collega());
  CHECK(k.risolvi());
  CHECK_EQ(k.a.ls->motivo(), LSM_ANNULLATO);
  CHECK(!invariante(k, "A0", "B0"));
}

TEST(scambio, con_un_altro_partner_resta_sospeso) {
  Banco k;
  soloAPreparato(k);
  // A si collega con una terza scheda (C, un altro trainerId)
  Scheda c;
  prepara(k.c, c.n, 3, 7, "MISTY");
  const PetRitratto attC = rit(7, "C0");
  preparaScheda(c, 3, &attC, {});
  usa(k.a);
  avvia(k.c, k.a.n);
  usa(c);
  avvia(k.c, c.n);
  CHECK(abbina(k.c, k.a.n, c.n, 30000));
  usa(k.a);
  k.a.ls->avvia(k.a.n.lk, 5);
  CHECK_EQ(k.a.ls->fase(), LSF_SOSPESO);
  CHECK_EQ(k.a.ls->giornaleDati().partner, (uint32_t)0x1002);
  CHECK_STREQ(k.a.ls->giornaleDati().nome, "GARY");
  CHECK(!k.a.ls->offri(0));  // niente scambi nuovi finché c'è il giornale
  k.a.ls->ferma();
  k.a.n.lk.chiudi();
  usa(c);
  c.n.lk.chiudi();
  k.c.nodi.pop_back();
  k.c.voli.clear();
  // di nuovo con GARY: si chiude (GARY non aveva preparato)
  CHECK(k.collega());
  CHECK(k.risolvi());
  CHECK(!invariante(k, "A0", "B0"));
}

// i due COMMIT partono ma non arrivano: tutti e due preparati
static uint32_t entrambiPreparati(Banco &k, bool taglia = true) {
  CHECK(k.collega());
  CHECK(k.offri(SCAMBIO_ATTIVO, SCAMBIO_ATTIVO));
  k.accettano();
  for (int i = 0; i < 500 && !(k.a.ls->fase() == LSF_PREPARATO && k.b.ls->fase() == LSF_PREPARATO); i++)
    k.passo(1);
  CHECK_EQ(k.a.ls->fase(), LSF_PREPARATO);
  CHECK_EQ(k.b.ls->fase(), LSF_PREPARATO);
  const uint32_t id = k.a.ls->tradeId();
  if (taglia) k.stacca();
  return id;
}

TEST(scambio, la_ripresa_completa_se_tutti_e_due_avevano_preparato) {
  Banco k;
  const uint32_t id = entrambiPreparati(k);
  accendi(k.a);
  accendi(k.b);
  CHECK(k.collega());
  CHECK(k.risolvi());
  CHECK(invariante(k, "A0", "B0"));
  CHECK(k.a.ls->applicato(id) && k.b.ls->applicato(id));
}

TEST(scambio, annullamento_manuale_quando_l_altro_e_ancora_preparato) {
  Banco k;
  entrambiPreparati(k);
  accendi(k.a);
  usa(k.a);
  CHECK(k.a.ls->annullaGiornale());  // offline: ultima risorsa
  CHECK_EQ(k.a.ls->fase(), LSF_LIBERO);
  CHECK_EQ(k.a.ls->motivo(), LSM_MANUALE);
  CHECK_EQ(k.a.pet->scambioBloccato(), (int8_t)SCAMBIO_NESSUNO);
  CHECK(k.collega());   // B rimanda COMMIT, A non lo conosce più: ANNULLA
  CHECK(k.risolvi());
  CHECK_EQ(k.b.ls->motivo(), LSM_ANNULLATO);
  CHECK(!invariante(k, "A0", "B0"));
}

TEST(scambio, annullamento_manuale_dopo_che_l_altro_ha_applicato_e_l_eccezione) {
  // l'avviso della UI: B ha già applicato, A annulla a mano e il pet di A è in due posti
  Banco k;
  entrambiPreparati(k, false);
  for (int i = 0; i < 200; i++) {  // solo B legge: riceve il COMMIT di A e applica
    k.c.passo(1);
    pompa(k.b);
  }
  CHECK_EQ(k.b.ls->fase(), LSF_FATTO);
  k.stacca();  // A non ha letto né il COMMIT né il DONE di B
  accendi(k.a);
  usa(k.a);
  CHECK(k.a.ls->annullaGiornale());
  std::map<std::string, int> conta;
  for (auto &x : petDi(k.a)) conta[x]++;
  for (auto &x : petDi(k.b)) conta[x]++;
  CHECK_EQ(conta["A0"], 2);  // doppione
  CHECK_EQ(conta["B0"], 0);  // e il pet di B perso
  // la ripresa non peggiora le cose: il DONE di B per uno scambio che A non ha più si scarta
  CHECK(k.collega());
  k.corri(500);
  CHECK(!k.a.ls->giornale() && !k.b.ls->giornale());
}

TEST(scambio, riavvio_dentro_la_ricezione_dalla_collezione) {
  // A riceve in collezione: la collezione è scritta, il blob del pet (con tradeFatto) no.
  // Simulato riportando "st", il giornale e "fatti" a com'erano da preparato.
  Banco k;
  CHECK(k.collega());
  CHECK(k.offri(0, SCAMBIO_ATTIVO));
  k.accettano();
  std::vector<uint8_t> st, tr;
  for (int i = 0; i < 500 && k.a.ls->fase() != LSF_PREPARATO; i++) k.passo(1);
  CHECK_EQ(k.a.ls->fase(), LSF_PREPARATO);
  {
    usa(k.a);
    Preferences p;
    p.begin("pokegotchi", true);
    st.resize(p.getBytesLength(PET_SAVE_KEY));
    p.getBytes(PET_SAVE_KEY, &st[0], st.size());
    p.end();
    p.begin(LS_NS, true);
    tr.resize(p.getBytesLength(LS_GIORNALE_KEY));
    p.getBytes(LS_GIORNALE_KEY, &tr[0], tr.size());
    p.end();
  }
  CHECK(k.risolvi());
  CHECK(invariante(k, "A1", "B0"));
  usa(k.a);
  {
    Preferences p;
    p.begin("pokegotchi", false);
    p.putBytes(PET_SAVE_KEY, &st[0], st.size());
    p.end();
    p.begin(LS_NS, false);
    p.putBytes(LS_GIORNALE_KEY, &tr[0], tr.size());
    p.remove(LS_FATTI_KEY);
    p.end();
  }
  k.stacca();
  accendi(k.a);
  CHECK_EQ(k.a.ls->fase(), LSF_SOSPESO);  // non risulta applicato
  usa(k.a);
  CHECK_EQ(k.a.pet->colRitratto(0).specie, (int16_t)94);  // ma il ricevuto è già lì
  CHECK(k.collega());     // A rimanda COMMIT, B l'ha applicato: DONE
  CHECK(k.risolvi());
  CHECK_EQ(k.a.ls->fase(), LSF_FATTO);
  CHECK(invariante(k, "A1", "B0"));
  usa(k.a);
  CHECK_EQ(k.a.pet->colNumero(), (uint8_t)1);
  CHECK(k.a.pet->isRegistered(94));
}

TEST(scambio, riavvio_dopo_la_ricezione_prima_di_togliere_il_giornale) {
  Banco k;
  CHECK(k.collega());
  CHECK(k.offri(SCAMBIO_ATTIVO, SCAMBIO_ATTIVO));
  k.accettano();
  std::vector<uint8_t> tr;
  for (int i = 0; i < 500 && k.a.ls->fase() != LSF_PREPARATO; i++) k.passo(1);
  const uint32_t id = k.a.ls->tradeId();
  {
    usa(k.a);
    Preferences p;
    p.begin(LS_NS, true);
    tr.resize(p.getBytesLength(LS_GIORNALE_KEY));
    p.getBytes(LS_GIORNALE_KEY, &tr[0], tr.size());
    p.end();
  }
  CHECK(k.risolvi());
  usa(k.a);
  {
    Preferences p;
    p.begin(LS_NS, false);
    p.putBytes(LS_GIORNALE_KEY, &tr[0], tr.size());
    p.remove(LS_FATTI_KEY);
    p.end();
  }
  k.stacca();
  accendi(k.a);  // il tradeId è nel blob del pet: carica() chiude il giornale da solo
  CHECK(!k.a.ls->giornale());
  CHECK_EQ(k.a.ls->fase(), LSF_LIBERO);
  CHECK(k.a.ls->applicato(id));
  usa(k.a);
  CHECK_EQ(k.a.pet->scambioBloccato(), (int8_t)SCAMBIO_NESSUNO);
  CHECK(!k.a.pet->riceviScambio(SCAMBIO_ATTIVO, rit(6, "A0", 101), rit(93, "B0", 95), id + 1, 0x1002, "GARY"));
  CHECK(invariante(k, "A0", "B0"));
}

TEST(scambio, la_ricezione_rifiuta_un_posto_che_non_ha_piu_il_pet_offerto) {
  Banco k;
  usa(k.a);
  Pet &p = *k.a.pet;
  const PetRitratto altro = rit(150, "X");
  CHECK(!p.riceviScambio(SCAMBIO_ATTIVO, altro, rit(93, "B0"), 77, 0x1002, "GARY"));
  CHECK(!p.riceviScambio(0, altro, rit(93, "B0"), 77, 0x1002, "GARY"));
  CHECK(!p.riceviScambio(3, p.colRitratto(0), rit(93, "B0"), 77, 0x1002, "GARY"));
  CHECK_EQ(p.speciesId, (int16_t)6);
  CHECK_EQ(p.colRitratto(0).specie, (int16_t)64);
  CHECK(p.riceviScambio(0, p.colRitratto(0), rit(93, "B0"), 77, 0x1002, "GARY"));
  CHECK(p.riceviScambio(0, rit(1, "?"), rit(1, "?"), 77, 0, ""));  // già applicato: niente
  CHECK_EQ(p.colRitratto(0).specie, (int16_t)94);
  // contatore degli scambi: uno solo, anche con la ricezione ripetuta e dopo un riavvio
  CHECK_EQ(p.linkTrades(), (uint16_t)1);
  Pet q;
  q.begin();
  CHECK_EQ(q.linkTrades(), (uint16_t)1);
}
