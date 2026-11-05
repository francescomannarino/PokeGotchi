// Test della battaglia in collegamento (linkbattaglia.cpp): due schede con il loro Link sul
// canale simulato che perde, duplica e riordina, squadre da 3, cambi, cambi forzati, resa,
// rivincita e pause lunghe tra i turni.
#include "framework.h"
#include "canale_link.h"
#include "../linkbattaglia.h"
#include "../pet.h"
#include <functional>

static PetRitratto rit(int16_t specie, uint16_t livello, uint8_t gene = 100, uint8_t tr = 0) {
  PetRitratto r;
  r.specie = specie;
  r.ageMinutes = (uint32_t)(livello - 1) * MINUTES_PER_LEVEL;
  r.geneAtk = r.geneDef = r.geneSpe = gene;
  r.trAtk = r.trDef = r.trSpe = tr;
  return r;
}

// l'ospite (A, trainerId più basso) e l'host (B)
static const PetRitratto SQ_OSPITE[3] = {rit(9, 52, 104), rit(65, 47, 96, 20), rit(143, 55)};
static const PetRitratto SQ_HOST[3] = {rit(6, 50, 101, 10), rit(25, 38, 110), rit(94, 49)};

struct Giocatore {
  Nodo *n = nullptr;
  LinkBattaglia lb;
  uint32_t rng = 1;
  uint32_t quando = 0;   // ora a cui scegliere (0 = non ancora deciso)
  std::vector<BattleEvento> narrati;
  int cambi = 0, forzati = 0;

  uint32_t caso() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
  }
};

static void pompa(Giocatore &g) {
  LinkMessaggio m;
  while (g.n->lk.prendi(m)) CHECK(g.lb.ricevi(m));
  g.lb.tick();
  BattleEvento e;
  while (g.lb.coda().prendi(e)) g.narrati.push_back(e);
}

// un giocatore che pensa fino a `pausaMax` ms e sceglie a caso: mosse, cambi e sostituti
static void decidi(Giocatore &g, uint32_t ora, uint32_t pausaMax) {
  if (!g.lb.devoScegliere()) return;
  if (!g.quando) g.quando = ora + 1 + g.caso() % pausaMax;
  if ((int32_t)(ora - g.quando) < 0) return;
  g.quando = 0;
  const Battle &b = g.lb.battaglia();
  const uint8_t io = g.lb.mioLato();
  uint8_t validi[BATTLE_SQUADRA], n = 0;
  for (uint8_t i = 0; i < b.membri[io]; i++)
    if (battlePuoCambiare(b, io, i)) validi[n++] = i;
  if (g.lb.fase() == LBF_CAMBIO) {
    CHECK(n > 0);
    CHECK(g.lb.scegli((uint8_t)(BATTLE_CAMBIA + validi[g.caso() % n])));
    g.forzati++;
  } else if (n && g.caso() % 5 == 0) {
    CHECK(g.lb.scegli((uint8_t)(BATTLE_CAMBIA + validi[g.caso() % n])));
    g.cambi++;
  } else {
    CHECK(g.lb.scegli((uint8_t)(g.caso() % BATTLE_MOSSE)));
  }
}

struct Partita {
  Coppia p;
  Giocatore a, b;   // a = ospite, b = host
  Partita(uint32_t seme, int perdita = 0, int doppi = 0, int ritardo = 0) : p(seme) {
    p.c.perdita = perdita;
    p.c.doppi = doppi;
    p.c.ritardoMax = ritardo;
    a.n = &p.a;
    b.n = &p.b;
    a.rng = seme * 7 + 3;
    b.rng = seme * 13 + 5;
    avvia(p.c, p.a);
    avvia(p.c, p.b);
    CHECK(abbina(p.c, p.a, p.b, 30000));
    a.lb.avvia(p.a.lk, seme * 3 + 1);
    b.lb.avvia(p.b.lk, seme * 5 + 2);
  }
  void passo(uint32_t ms = 5) {
    p.c.passo(ms);
    pompa(a);
    pompa(b);
  }
  bool finche(const std::function<bool()> &fatto, uint32_t maxMs = 30000) {
    for (uint32_t t = 0; t < maxMs; t += 5) {
      if (fatto()) return true;
      passo();
    }
    return fatto();
  }
  // invito, proposta, risposta e squadre: fino al primo turno
  bool inizia(uint8_t regola, const PetRitratto *sqA = SQ_OSPITE, uint8_t nA = 3,
              const PetRitratto *sqB = SQ_HOST, uint8_t nB = 3) {
    if (!a.lb.invita()) return false;
    if (!finche([&] { return b.lb.invitato(); })) return false;
    if (!b.lb.proponi(regola)) return false;
    if (!finche([&] { return a.lb.fase() == LBF_PROPOSTA; })) return false;
    CHECK_EQ(a.lb.regola(), regola);   // l'ospite vede la regola prima di accettare
    if (!a.lb.rispondi(true)) return false;
    if (!finche([&] { return b.lb.fase() == LBF_SQUADRA; })) return false;
    CHECK(a.lb.squadra(sqA, nA));
    CHECK(b.lb.squadra(sqB, nB));
    return finche([&] { return a.lb.fase() == LBF_SCELTA && b.lb.fase() == LBF_SCELTA; });
  }
  // gioca fino a FINE confermata da tutti e due
  bool gioca(uint32_t pausaMax, uint32_t maxMs, const std::function<void()> &ogniPasso = nullptr) {
    for (uint32_t t = 0; t < maxMs; t += 5) {
      if (a.lb.fineConfermata() && b.lb.fineConfermata()) return true;
      decidi(a, p.c.ora, pausaMax);
      decidi(b, p.c.ora, pausaMax);
      passo();
      if (ogniPasso) ogniPasso();
    }
    return false;
  }
};

// le due schede vedono la stessa battaglia: esito, PS, stati e narrazione
static void stessoEsito(Partita &pt, uint16_t desyncAtteso = 0) {
  const Battle &x = pt.a.lb.battaglia(), &y = pt.b.lb.battaglia();
  CHECK_EQ(pt.a.lb.vincitore(), pt.b.lb.vincitore());
  CHECK_EQ(pt.a.lb.motivo(), pt.b.lb.motivo());
  CHECK_EQ(x.turno, y.turno);
  for (uint8_t k = 0; k < 2; k++) {
    CHECK_EQ(x.membri[k], y.membri[k]);
    CHECK_EQ(x.inCampo[k], y.inCampo[k]);
    for (uint8_t i = 0; i < x.membri[k]; i++) {
      CHECK_EQ(battleMembro(x, k, i).ps, battleMembro(y, k, i).ps);
      CHECK_EQ(battleMembro(x, k, i).stato, battleMembro(y, k, i).stato);
    }
  }
  CHECK_EQ(pt.a.narrati.size(), pt.b.narrati.size());
  bool uguali = pt.a.narrati.size() == pt.b.narrati.size();
  for (size_t i = 0; uguali && i < pt.a.narrati.size(); i++) {
    const BattleEvento &e = pt.a.narrati[i], &f = pt.b.narrati[i];
    uguali = e.tipo == f.tipo && e.lato == f.lato && e.mossa == f.mossa && e.valore == f.valore;
  }
  CHECK(uguali);
  CHECK_EQ(pt.a.lb.desync(), desyncAtteso);
  CHECK_EQ(pt.b.lb.desync(), (uint16_t)0);
  CHECK_EQ(pt.a.lb.persi(), (uint16_t)0);
  CHECK_EQ(pt.b.lb.persi(), (uint16_t)0);
  CHECK_EQ(pt.a.lb.coda().persi, (uint16_t)0);
  CHECK_EQ(pt.b.lb.coda().persi, (uint16_t)0);
  CHECK_EQ(y.eventiPersi, (uint16_t)0);
  CHECK_EQ(pt.p.a.lk.stato(), LINK_PRONTO);
  CHECK_EQ(pt.p.b.lk.stato(), LINK_PRONTO);
}

// ---------------------------------------------------------------- pacchetti

TEST(LinkBattaglia, il_risultato_peggiore_sta_in_un_pacchetto) {
  CHECK(LB_RISULTATO_MAX <= LINK_DATI_MAX);
  LbRisultato r, s;
  r.passo = 0x1234;
  r.turno = 0x0102;
  r.cambio = 1;
  r.scelta[0] = BATTLE_CAMBIA + 2;
  r.scelta[1] = 0xFF;
  r.vincitore = BATTLE_PAREGGIO;
  r.deveCambiare[1] = true;
  r.inCampo[0] = 2;
  r.ps[1][2] = 999;
  r.stato[0][1] = ST_PARALISI;
  r.pp[1][3] = 35;
  r.nEventi = BATTLE_MAX_EVENTI;
  for (uint8_t i = 0; i < BATTLE_MAX_EVENTI; i++)
    r.eventi[i] = {(uint8_t)(i % (EV_ENTRA + 1)), (uint8_t)(i & 1), (uint8_t)(i * 3), (uint16_t)(i * 1000)};
  uint8_t d[LB_RISULTATO_MAX];
  const uint8_t n = lbScriviRisultato(r, d);
  CHECK_EQ(n, (uint8_t)LB_RISULTATO_MAX);
  CHECK(lbLeggiRisultato(d, n, s));
  CHECK_EQ(s.passo, r.passo);
  CHECK_EQ(s.turno, r.turno);
  CHECK_EQ(s.cambio, r.cambio);
  CHECK_EQ(s.scelta[0], r.scelta[0]);
  CHECK_EQ(s.scelta[1], r.scelta[1]);
  CHECK_EQ(s.vincitore, r.vincitore);
  CHECK(!s.deveCambiare[0] && s.deveCambiare[1]);
  CHECK_EQ(s.inCampo[0], (uint8_t)2);
  CHECK_EQ(s.ps[1][2], (uint16_t)999);
  CHECK_EQ(s.stato[0][1], (uint8_t)ST_PARALISI);
  CHECK_EQ(s.pp[1][3], (uint8_t)35);
  CHECK_EQ(s.nEventi, r.nEventi);
  bool uguali = true;
  for (uint8_t i = 0; i < BATTLE_MAX_EVENTI; i++)
    uguali &= s.eventi[i].tipo == r.eventi[i].tipo && s.eventi[i].lato == r.eventi[i].lato &&
              s.eventi[i].mossa == r.eventi[i].mossa && s.eventi[i].valore == r.eventi[i].valore;
  CHECK(uguali);
  // troncato, con un evento in più dichiarato o in campo fuori squadra: rifiutato
  CHECK(!lbLeggiRisultato(d, (uint8_t)(n - 1), s));
  d[LB_RISULTATO_FISSO - 1] = BATTLE_MAX_EVENTI + 1;
  CHECK(!lbLeggiRisultato(d, n, s));
  d[LB_RISULTATO_FISSO - 1] = BATTLE_MAX_EVENTI;
  d[9] = BATTLE_SQUADRA;
  CHECK(!lbLeggiRisultato(d, n, s));
}

TEST(LinkBattaglia, fa_da_host_il_trainer_id_piu_alto) {
  Partita pt(1);
  CHECK(!pt.a.lb.host());
  CHECK(pt.b.lb.host());
  CHECK_EQ(pt.a.lb.mioLato(), (uint8_t)1);
  CHECK_EQ(pt.b.lb.mioLato(), (uint8_t)0);
  // solo l'ospite invita, solo l'host propone
  CHECK(!pt.b.lb.invita());
  CHECK(!pt.a.lb.proponi(LB_TUTTI_AL_50));
}

TEST(LinkBattaglia, regola_coppa_su_tutta_la_squadra) {
  Partita pt(2);
  CHECK(pt.inizia(LB_TUTTI_AL_50));
  for (uint8_t k = 0; k < 2; k++)
    for (uint8_t i = 0; i < 3; i++)
      CHECK_EQ(battleMembro(pt.a.lb.battaglia(), k, i).livello, (uint8_t)LB_LIVELLO_COPPA);
  Partita pl(3);
  CHECK(pl.inizia(LB_LIVELLO_LIBERO));
  CHECK_EQ(battleMembro(pl.a.lb.battaglia(), 0, 1).livello, (uint8_t)38);   // Pikachu dell'host
  CHECK_EQ(battleMembro(pl.a.lb.battaglia(), 1, 2).livello, (uint8_t)55);   // Snorlax dell'ospite
  CHECK_EQ(battleMembro(pl.b.lb.battaglia(), 1, 2).psMax, battleMembro(pl.a.lb.battaglia(), 1, 2).psMax);
  CHECK_EQ(pl.a.lb.battaglia().rng.s, pl.b.lb.battaglia().rng.s);   // seme concordato
}

TEST(LinkBattaglia, la_proposta_rifiutata_torna_libera) {
  Partita pt(4);
  CHECK(pt.b.lb.proponi(LB_LIVELLO_LIBERO));   // l'host può proporre anche senza invito
  CHECK(pt.finche([&] { return pt.a.lb.fase() == LBF_PROPOSTA; }));
  CHECK_EQ(pt.a.lb.regola(), (uint8_t)LB_LIVELLO_LIBERO);
  CHECK(pt.a.lb.rispondi(false));
  CHECK(pt.finche([&] { return pt.b.lb.fase() == LBF_LIBERO; }));
  CHECK_EQ(pt.b.lb.motivo(), LBM_RIFIUTO);
  CHECK_EQ(pt.a.lb.fase(), LBF_LIBERO);
}

// ---------------------------------------------------------------- battaglie

TEST(LinkBattaglia, tre_contro_tre_su_canale_cattivo_stesso_esito) {
  int cambi = 0, forzati = 0, finite = 0;
  for (uint32_t seme = 11; seme < 17; seme++) {
    Partita pt(seme, 30, 20, 60);
    CHECK(pt.inizia(LB_TUTTI_AL_50));
    CHECK(pt.gioca(1500, 3600000));
    stessoEsito(pt);
    CHECK_EQ(pt.a.lb.fase(), LBF_FINE);
    CHECK_EQ(pt.a.lb.motivo(), LBM_KO);
    CHECK(pt.p.a.lk.ritrasmessi() > 0);
    if (pt.a.lb.vincitore() != BATTLE_IN_CORSO) finite++;
    // la squadra che perde è tutta KO
    const uint8_t v = pt.b.lb.vincitore();
    if (v < 2) CHECK_EQ(battleRestanti(pt.b.lb.battaglia(), (uint8_t)(1 - v)), (uint8_t)0);
    cambi += pt.a.cambi + pt.b.cambi;
    forzati += pt.a.forzati + pt.b.forzati;
  }
  CHECK_EQ(finite, 6);
  CHECK(cambi > 0);
  CHECK(forzati > 0);
}

TEST(LinkBattaglia, pause_lunghe_tra_i_turni) {
  // fino a 25 s per scegliere: più del timeout del Link, che resta vivo coi PING
  Partita pt(21, 10, 5, 40);
  CHECK(pt.inizia(LB_TUTTI_AL_50));
  CHECK(pt.gioca(25000, 4 * 3600000u));
  stessoEsito(pt);
  CHECK(pt.b.lb.battaglia().turno > 3);
}

TEST(LinkBattaglia, squadre_diverse_uno_contro_tre) {
  Partita pt(22, 20, 10, 30);
  CHECK(pt.inizia(LB_LIVELLO_LIBERO, SQ_OSPITE + 2, 1, SQ_HOST, 3));
  CHECK_EQ(pt.a.lb.battaglia().membri[1], (uint8_t)1);
  CHECK_EQ(pt.a.lb.membri(0), (uint8_t)3);
  CHECK_EQ(pt.a.lb.ritratto(0, 2).specie, (int16_t)94);
  CHECK(pt.gioca(800, 3600000));
  stessoEsito(pt);
}

TEST(LinkBattaglia, l_host_corregge_la_copia_dell_ospite) {
  Partita pt(23, 10, 0, 20);
  CHECK(pt.inizia(LB_TUTTI_AL_50));
  bool rovinato = false;
  CHECK(pt.gioca(500, 3600000, [&] {
    // dopo il primo passo l'ospite «sbaglia» i PS di un suo Pokémon in panchina
    if (!rovinato && pt.a.lb.passo() == 1 && pt.a.lb.fase() == LBF_SCELTA) {
      Battle &b = const_cast<Battle &>(pt.a.lb.battaglia());
      uint8_t i = b.inCampo[1] == 2 ? 1 : 2;
      b.squadra[1][i].ps = (uint16_t)(b.squadra[1][i].ps - 1);
      rovinato = true;
    }
  }));
  CHECK(rovinato);
  stessoEsito(pt, 1);
}

TEST(LinkBattaglia, resa_dell_ospite_vince_l_host) {
  Partita pt(24, 20, 10, 30);
  CHECK(pt.inizia(LB_TUTTI_AL_50));
  CHECK(pt.finche([&] {
    decidi(pt.a, pt.p.c.ora, 300);
    decidi(pt.b, pt.p.c.ora, 300);
    return pt.b.lb.passo() >= 3 && pt.a.lb.passo() == pt.b.lb.passo() && pt.a.lb.devoScegliere();
  }, 600000));
  CHECK(pt.a.lb.arrenditi());
  CHECK_EQ(pt.a.lb.fase(), LBF_FINE);
  CHECK(pt.finche([&] { return pt.a.lb.fineConfermata() && pt.b.lb.fineConfermata(); }));
  CHECK_EQ(pt.b.lb.motivo(), LBM_RESA);
  CHECK_EQ(pt.a.lb.vincitore(), (uint8_t)0);
  CHECK_EQ(pt.b.lb.vincitore(), (uint8_t)0);
  stessoEsito(pt);
}

TEST(LinkBattaglia, rivincita_e_uscita) {
  Partita pt(25, 20, 10, 30);
  CHECK(pt.inizia(LB_TUTTI_AL_50));
  CHECK(pt.gioca(300, 3600000));
  CHECK(pt.b.lb.rivincita());
  CHECK(!pt.b.lb.rivincita());                  // una volta sola
  CHECK(pt.finche([&] { return pt.a.lb.rivincitaAltro(); }));
  CHECK_EQ(pt.b.lb.fase(), LBF_FINE);           // aspetta l'altro
  CHECK(pt.a.lb.rivincita());
  CHECK(pt.finche([&] { return pt.b.lb.fase() == LBF_SQUADRA; }));
  CHECK_EQ(pt.a.lb.fase(), LBF_SQUADRA);
  CHECK_EQ(pt.a.lb.regola(), (uint8_t)LB_TUTTI_AL_50);
  const uint32_t semePrima = pt.a.lb.battaglia().rng.s;
  pt.a.narrati.clear();
  pt.b.narrati.clear();
  CHECK(pt.a.lb.squadra(SQ_OSPITE, 2));
  CHECK(pt.b.lb.squadra(SQ_HOST + 1, 2));
  CHECK(pt.finche([&] { return pt.a.lb.fase() == LBF_SCELTA && pt.b.lb.fase() == LBF_SCELTA; }));
  CHECK(pt.a.lb.battaglia().rng.s != semePrima);   // seme nuovo
  CHECK(pt.gioca(300, 3600000));
  stessoEsito(pt);
  // chi non vuole la rivincita esce: l'altro torna libero
  pt.a.lb.esci();
  CHECK_EQ(pt.a.lb.fase(), LBF_LIBERO);
  CHECK(pt.finche([&] { return pt.b.lb.fase() == LBF_LIBERO; }));
  CHECK_EQ(pt.b.lb.motivo(), LBM_USCITA);
}

TEST(LinkBattaglia, uscita_a_meta_battaglia_e_una_resa) {
  Partita pt(26);
  CHECK(pt.inizia(LB_LIVELLO_LIBERO));
  pt.b.lb.esci();
  CHECK(pt.finche([&] { return pt.a.lb.fase() == LBF_LIBERO; }));
  CHECK_EQ(pt.a.lb.motivo(), LBM_USCITA);
  CHECK_EQ(pt.b.lb.fase(), LBF_LIBERO);
}

TEST(LinkBattaglia, squadra_non_valida_errore_dalle_due_parti) {
  Partita pq(28);
  CHECK(pq.a.lb.invita());
  CHECK(pq.finche([&] { return pq.b.lb.invitato(); }));
  CHECK(pq.b.lb.proponi(LB_TUTTI_AL_50));
  CHECK(pq.finche([&] { return pq.a.lb.fase() == LBF_PROPOSTA; }));
  CHECK(pq.a.lb.rispondi(true));
  CHECK(pq.finche([&] { return pq.b.lb.fase() == LBF_SQUADRA; }));
  // una squadra vuota o con un ritratto non valido non parte da qui…
  CHECK(!pq.a.lb.squadra(SQ_OSPITE, 0));
  PetRitratto uovo = rit(0, 5);
  CHECK(!pq.a.lb.squadra(&uovo, 1));
  // …e se arriva rovinata (CRC) dall'altra parte, si chiude con ERRORE da tutte e due
  uint8_t d[5 + RIT_BYTES] = {1, 2, 3, 4, 1};
  ritrattoScrivi(SQ_OSPITE[0], d + 5, RIT_BYTES);
  d[5 + 20] ^= 0x40;
  CHECK(pq.p.a.lk.invia(LB_TEAM, d, sizeof(d)));
  CHECK(pq.finche([&] { return pq.a.lb.fase() == LBF_LIBERO; }));
  CHECK_EQ(pq.b.lb.motivo(), LBM_ERRORE);
  CHECK_EQ(pq.a.lb.motivo(), LBM_ERRORE);
}

TEST(LinkBattaglia, pacchetti_fuori_fase_si_scartano) {
  Partita pt(29);
  LinkMessaggio m;
  m.tipo = LB_AZIONE;
  m.len = 3;
  m.dati[0] = m.dati[1] = 0;
  m.dati[2] = 1;
  CHECK(pt.b.lb.ricevi(m));             // è della fascia battaglia, ma non si aspetta
  CHECK_EQ(pt.b.lb.scartati(), (uint16_t)1);
  CHECK_EQ(pt.b.lb.fase(), LBF_LIBERO);
  m.tipo = LT_SCAMBIO;
  CHECK(!pt.b.lb.ricevi(m));            // lo scambio non è affar suo
  CHECK(pt.inizia(LB_TUTTI_AL_50));
  m.tipo = LB_AZIONE;
  m.dati[0] = 7;                        // passo sbagliato
  CHECK(pt.b.lb.ricevi(m));
  CHECK_EQ(pt.b.lb.scartati(), (uint16_t)2);
  CHECK(pt.b.lb.devoScegliere());
  // il cambio forzato vuole un cambio, e verso un membro che può entrare
  CHECK(!pt.b.lb.scegli(BATTLE_CAMBIA + 0));   // è già in campo
  CHECK(!pt.b.lb.scegli(BATTLE_MOSSE));
}
