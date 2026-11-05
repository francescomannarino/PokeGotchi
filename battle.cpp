// Motore di battaglia: vedi battle.h. Derivato da DylanPDao/TamaPoke (MIT), avviso
// completo in battle.h.
#include "battle.h"
#include "pet.h"
#include "ritratto.h"

#define SLOT_SCONTRO 0xFF  // nessuno slot: si usa Scontro
#define IRA_SEMPRE 0xFF    // turniBloccata di Ira: si ripete fino alla fine

// ---------- costruzione ----------

void battleMosseApprese(int16_t dex, uint8_t livello, uint8_t out[BATTLE_MOSSE]) {
  uint8_t n = 0;
  for (int i = 0; i < BATTLE_MOSSE; i++) out[i] = 0;
  if (dex >= 1 && dex <= DEX_COUNT) {
    for (uint16_t k = LEARN_OFS[dex]; k < LEARN_OFS[dex + 1]; k++) {
      const LearnEntry &e = LEARN_TBL[k];
      if (e.level > livello) break;  // il learnset è ordinato per livello
      if (!MOVE_TBL[e.move].pp) continue;
      bool nota = false;
      for (uint8_t i = 0; i < n; i++) nota = nota || out[i] == e.move;
      if (nota) continue;
      if (n == BATTLE_MOSSE) {  // dimentica la più vecchia
        for (uint8_t i = 1; i < BATTLE_MOSSE; i++) out[i - 1] = out[i];
        n--;
      }
      out[n++] = e.move;
    }
  }
  if (!n) out[0] = MOVE_STRUGGLE;
}

void battleCombattente(Combatant &c, int16_t dex, uint8_t livello, uint16_t ps, uint16_t att,
                       uint16_t dif, uint16_t vel, uint16_t spc, const uint8_t *mosse) {
  c = Combatant();
  const DexEntry &d = DEX_TBL[(dex >= 1 && dex <= DEX_COUNT) ? dex : 0];
  c.dex = dex;
  c.livello = livello ? livello : 1;
  c.tipo1 = d.type1;
  c.tipo2 = d.type2;
  c.velBase = d.bSpe;
  c.psMax = ps ? ps : 1;
  c.stat[STAT_ATTACCO] = att;
  c.stat[STAT_DIFESA] = dif;
  c.stat[STAT_VELOCITA] = vel;
  c.stat[STAT_SPECIALE] = spc;
  uint8_t m[BATTLE_MOSSE];
  if (mosse) {
    for (int i = 0; i < BATTLE_MOSSE; i++) m[i] = mosse[i];
  } else {
    battleMosseApprese(dex, c.livello, m);
  }
  for (int i = 0; i < BATTLE_MOSSE; i++) {
    bool valida = m[i] && m[i] <= MOVE_MAX_ID && MOVE_TBL[m[i]].pp;
    c.mosse[i] = valida ? m[i] : 0;
    c.ppMax[i] = valida ? MOVE_TBL[m[i]].pp : 0;
  }
  battleRipristina(c);
}

void battleRipristina(Combatant &c) {
  if (c.trasformato) {
    c.dex = c.orig.dex;
    c.tipo1 = c.orig.tipo1;
    c.tipo2 = c.orig.tipo2;
    c.velBase = c.orig.velBase;
    for (int i = 0; i < 4; i++) c.stat[i] = c.orig.stat[i];
    for (int i = 0; i < BATTLE_MOSSE; i++) {
      c.mosse[i] = c.orig.mosse[i];
      c.ppMax[i] = c.orig.ppMax[i];
    }
  }
  // si riparte da un Combatant nuovo: ogni stato volatile, anche quelli futuri, si azzera
  Combatant f;
  f.dex = c.dex;
  f.livello = c.livello;
  f.tipo1 = c.tipo1;
  f.tipo2 = c.tipo2;
  f.velBase = c.velBase;
  f.psMax = c.psMax;
  f.ps = c.psMax;
  for (int i = 0; i < 4; i++) f.stat[i] = c.stat[i];
  for (int i = 0; i < BATTLE_MOSSE; i++) {
    f.mosse[i] = c.mosse[i];
    f.ppMax[i] = f.pp[i] = c.ppMax[i];
  }
  c = f;
}

// ---------- dal pet ----------

uint16_t battleStatGen1(uint8_t base, uint8_t dv, uint8_t bonus, uint8_t livello, bool ps) {
  uint32_t v = ((uint32_t)(base + dv) * 2 + bonus) * livello / 100;
  return (uint16_t)(ps ? v + livello + 10 : v + 5);
}

// geni 90..110 -> DV 0..15
static uint8_t dvDaGene(uint8_t gene) {
  if (gene < 90) gene = 90;
  if (gene > 110) gene = 110;
  return (uint8_t)((gene - 90) * 15 / 20);
}

// allenamento 0..100 -> 0..63, il massimo delle "stat exp" di Gen 1 (radice di 65535 / 4)
static uint8_t bonusDaAllenamento(uint16_t tr) {
  if (tr > 100) tr = 100;
  return (uint8_t)(tr * 63 / 100);
}

// la formula comune a pet e ritratto: specie, livello (età), geni e allenamento
static void combattenteDa(Combatant &c, int16_t dex, uint16_t lv16, uint8_t gA, uint8_t gD,
                          uint8_t gV, uint8_t tA, uint8_t tD, uint8_t tV) {
  const DexEntry &d = DEX_TBL[dex];
  uint8_t lv = (uint8_t)(lv16 > 100 ? 100 : lv16);
  uint8_t dvA = dvDaGene(gA), dvD = dvDaGene(gD), dvV = dvDaGene(gV);
  uint8_t dvS = dvDaGene((uint8_t)((gA + gD) / 2));
  // come in Gen 1, il DV dei PS è fatto dai bit bassi degli altri quattro
  uint8_t dvP = (uint8_t)((dvA & 1) << 3 | (dvD & 1) << 2 | (dvV & 1) << 1 | (dvS & 1));
  uint8_t bA = bonusDaAllenamento(tA), bD = bonusDaAllenamento(tD);
  uint8_t bV = bonusDaAllenamento(tV);
  uint8_t bS = bonusDaAllenamento((tA + tD) / 2);
  uint8_t bP = bonusDaAllenamento((tA + tD + tV) / 3);
  battleCombattente(c, dex, lv, battleStatGen1(d.bHp, dvP, bP, lv, true),
                    battleStatGen1(d.bAtk, dvA, bA, lv, false),
                    battleStatGen1(d.bDef, dvD, bD, lv, false),
                    battleStatGen1(d.bSpe, dvV, bV, lv, false),
                    battleStatGen1(d.bSpc, dvS, bS, lv, false), nullptr);
}

bool combattenteDaPet(Combatant &c, const Pet &p) {
  if (p.isEgg() || p.speciesId > DEX_COUNT) {
    c = Combatant();
    return false;
  }
  combattenteDa(c, p.speciesId, p.level(), p.geneAtk, p.geneDef, p.geneSpe, p.trAtk, p.trDef,
                p.trSpe);
  return true;
}

bool combattenteDaRitratto(Combatant &c, const PetRitratto &r) {
  if (r.specie < 1 || r.specie > DEX_COUNT) {
    c = Combatant();
    return false;
  }
  combattenteDa(c, r.specie, levelFromAge(r.ageMinutes), r.geneAtk, r.geneDef, r.geneSpe,
                r.trAtk, r.trDef, r.trSpe);
  return true;
}

// ---------- scelta della mossa ----------

bool battleDeveScontro(const Combatant &c) {
  for (int i = 0; i < BATTLE_MOSSE; i++)
    if (c.mosse[i] && c.pp[i]) return false;
  return true;
}

// slot effettivo: quello scelto se usabile, se no il primo con PP; SLOT_SCONTRO se nessuno
static uint8_t slotUsabile(const Combatant &c, uint8_t slot) {
  if (slot < BATTLE_MOSSE && c.mosse[slot] && c.pp[slot]) return slot;
  for (uint8_t i = 0; i < BATTLE_MOSSE; i++)
    if (c.mosse[i] && c.pp[i]) return i;
  return SLOT_SCONTRO;
}

uint8_t battleMossaScelta(const Combatant &c, uint8_t slot) {
  uint8_t s = slotUsabile(c, slot);
  return s == SLOT_SCONTRO ? (uint8_t)MOVE_STRUGGLE : c.mosse[s];
}

bool battleDeveScegliere(const Combatant &c) {
  return !c.ricarica && !c.carica && !c.bloccata && !c.intrappolato;
}

// ---------- statistiche ----------

// StatModifierRatios di pokered, in centesimi: stadi -6..+6
static const uint16_t RAPPORTO_STADIO[13] = { 25, 28, 33, 40, 50, 66, 100,
                                              150, 200, 250, 300, 350, 400 };

uint16_t battleStadio(uint16_t valore, int8_t stadio) {
  if (stadio > 6) stadio = 6;
  if (stadio < -6) stadio = -6;
  uint32_t v = (uint32_t)valore * RAPPORTO_STADIO[stadio + 6] / 100;
  return v < 1 ? 1 : (v > 999 ? 999 : (uint16_t)v);
}

// statistica in battaglia: stadio, poi scottatura (ATK ½) e paralisi (VEL ¼) come in Gen 1
static uint16_t statEff(const Combatant &c, uint8_t i) {
  uint16_t v = battleStadio(c.stat[i], c.stadio[i]);
  if (i == STAT_ATTACCO && c.stato == ST_SCOTTATURA) v = v / 2 ? v / 2 : 1;
  if (i == STAT_VELOCITA && c.stato == ST_PARALISI) v = v / 4 ? v / 4 : 1;
  return v;
}

// ---------- danno ----------

uint8_t battleEfficacia(uint8_t tipoMossa, const Combatant &dif) {
  if (tipoMossa >= TYPE_COUNT) return 4;
  uint16_t e = TYPE_CHART[tipoMossa][dif.tipo1] * 2;  // ×4
  if (dif.tipo2 < TYPE_COUNT && dif.tipo2 != dif.tipo1) e = e * TYPE_CHART[tipoMossa][dif.tipo2] / 2;
  return (uint8_t)e;
}

bool battleTiraBruttoColpo(BattleRng &rng, uint8_t velBase, bool altoTasso,
                           bool concentrazione) {
  uint16_t soglia = velBase / 2;
  if (altoTasso) soglia *= 8;
  if (concentrazione) soglia *= 4;
  if (soglia > 255) soglia = 255;
  return rng.byte() < soglia;
}

uint16_t battleDanno(const Combatant &att, const Combatant &dif, uint8_t mossa, bool brutto,
                     uint8_t r) {
  if (!mossa || mossa > MOVE_MAX_ID) return 0;
  const MoveEntry &m = MOVE_TBL[mossa];
  if (!m.power || !battleEfficacia(m.type, dif)) return 0;

  // la Speciale vale sia in attacco sia in difesa; la categoria la decide il tipo
  bool speciale = TYPE_IS_SPECIAL(m.type);
  uint8_t ia = speciale ? STAT_SPECIALE : STAT_ATTACCO;
  uint8_t id = speciale ? STAT_SPECIALE : STAT_DIFESA;
  uint32_t A, D, L = att.livello;
  if (brutto) {
    // livello doppio e statistiche pure: niente stadi, stati né barriere
    A = att.stat[ia];
    D = dif.stat[id];
    L *= 2;
  } else {
    A = statEff(att, ia);
    D = statEff(dif, id);
    if (speciale ? dif.schermoluce : dif.riflesso) D *= 2;
  }
  if (A > 255 || D > 255) {  // i registri di Gen 1 sono a 8 bit
    A /= 4;
    D /= 4;
  }
  if (m.effect == EF_ESPLOSIONE) D /= 2;
  if (!D) D = 1;

  uint32_t d = (2 * L / 5 + 2) * m.power * A / D / 50;
  if (d > 997) d = 997;
  d += 2;
  if (m.type == att.tipo1 || m.type == att.tipo2) d += d / 2;  // STAB
  // un tipo alla volta, arrotondando ogni volta come la tabella di Gen 1
  d = d * TYPE_CHART[m.type][dif.tipo1] / 2;
  if (dif.tipo2 < TYPE_COUNT && dif.tipo2 != dif.tipo1) d = d * TYPE_CHART[m.type][dif.tipo2] / 2;
  if (d > 1) d = d * r / 255;
  return d > 65535 ? 65535 : (uint16_t)d;
}


// ---------- un'azione ----------

static void evento(Battle &b, uint8_t tipo, uint8_t lato, uint8_t mossa, uint16_t valore = 0) {
  const BattleEvento e = { tipo, lato, mossa, valore };
  if (b.nEventi < BATTLE_MAX_EVENTI) b.eventi[b.nEventi++] = e;
  else b.eventiPersi++;
  b.coda.metti(e);
}

// toglie fino a `n` PS; restituisce quelli tolti davvero
static uint16_t ferisci(Combatant &c, uint32_t n) {
  uint16_t tolti = n >= c.ps ? c.ps : (uint16_t)n;
  c.ps -= tolti;
  return tolti;
}

static uint16_t cura(Combatant &c, uint32_t n) {
  uint16_t mancano = c.psMax - c.ps;
  uint16_t dati = n >= mancano ? mancano : (uint16_t)n;
  c.ps += dati;
  return dati;
}

static bool haTipo(const Combatant &c, uint8_t t) { return c.tipo1 == t || c.tipo2 == t; }

// tiro di precisione su 255 (100% = 255: manca 1 volta su 256), con gli stadi di
// precisione di chi attacca ed elusione del bersaglio
static bool colpisce(Battle &b, const Combatant &att, const Combatant &dif, const MoveEntry &m) {
  if (m.effect == EF_MAI_FALLISCE || !m.accuracy) return true;
  uint16_t soglia = (uint16_t)m.accuracy * 255 / 100;
  soglia = battleStadio(soglia, att.stadio[STAT_PRECISIONE]);
  soglia = battleStadio(soglia, -dif.stadio[STAT_ELUSIONE]);
  if (soglia > 255) soglia = 255;
  return b.rng.byte() < soglia;
}

// colpi di Gen 1 da 2 a 5: 3/8, 3/8, 1/8, 1/8
static uint8_t tiraColpi(BattleRng &rng) {
  uint8_t r = (uint8_t)rng.sotto(8);
  return r < 3 ? 2 : (r < 6 ? 3 : (r == 6 ? 4 : 5));
}

static bool ignoraTipi(uint8_t effetto) {
  return effetto == EF_DANNO_LIVELLO || effetto == EF_DANNO_FISSO || effetto == EF_META_PS;
}

// mosse senza potenza che fanno comunque danno
static bool fannoDanno(const MoveEntry &m) {
  return m.power || ignoraTipi(m.effect) || m.effect == EF_KO;
}

static bool staInIra(const Combatant &c) {
  return c.bloccata && MOVE_TBL[c.bloccata].effect == EF_IRA;
}

// sposta uno stadio di `delta` (fermo a ±6); false se era già al limite
static bool cambiaStadio(Battle &b, uint8_t lato, uint8_t stat, int8_t delta, uint8_t mossa) {
  Combatant &c = b.lato[lato];
  int8_t prima = c.stadio[stat];
  int v = prima + delta;
  if (v > 6) v = 6;
  if (v < -6) v = -6;
  if (v == prima) return false;
  c.stadio[stat] = (int8_t)v;
  evento(b, EV_STADIO, lato, mossa, (uint16_t)(stat << 8 | (uint8_t)delta));
  return true;
}

// infligge `st` a `lato`. `secondario`: effetto di una mossa di danno, che in Gen 1 non
// paralizza, scotta né congela chi ha il tipo della mossa e non dice niente se fallisce.
// false se lo stato non attecchisce.
static bool infliggi(Battle &b, uint8_t lato, uint8_t st, uint8_t mossa, bool secondario) {
  Combatant &c = b.lato[lato];
  const MoveEntry &m = MOVE_TBL[mossa];
  if (st == ST_TENTENNAMENTO) {  // vale solo se il bersaglio non ha ancora agito
    c.tentenna = true;
    return true;
  }
  if (st == ST_CONFUSIONE) {
    if (c.confusione) return false;
    c.confusione = (uint8_t)(2 + b.rng.sotto(4));  // 2..5 turni
    evento(b, EV_STATO, lato, mossa, st);
    return true;
  }
  if (c.stato != ST_NESSUNO) return false;
  bool veleno = st == ST_VELENO || st == ST_TOSSICO;
  if (veleno && haTipo(c, T_VELENO)) return false;
  if (secondario && !veleno && haTipo(c, m.type)) return false;
  c.stato = st;
  if (st == ST_SONNO) c.turniSonno = (uint8_t)(1 + b.rng.sotto(7));  // 1..7 turni
  if (st == ST_TOSSICO) c.tossico = 1;
  evento(b, EV_STATO, lato, mossa, st);
  return true;
}

// salva specie, tipi, statistiche e mosse prima di Trasformazione / Conversione
static void salvaOriginali(Combatant &c) {
  if (c.trasformato) return;
  c.trasformato = true;
  c.orig.dex = c.dex;
  c.orig.tipo1 = c.tipo1;
  c.orig.tipo2 = c.tipo2;
  c.orig.velBase = c.velBase;
  for (int i = 0; i < 4; i++) c.orig.stat[i] = c.stat[i];
  for (int i = 0; i < BATTLE_MOSSE; i++) {
    c.orig.mosse[i] = c.mosse[i];
    c.orig.ppMax[i] = c.ppMax[i];
    c.orig.pp[i] = c.pp[i];
  }
}

// l'azione è persa: un caricamento o una mossa che si ripete si interrompono (Ira no)
static void interrompi(Battle &b, uint8_t i) {
  Combatant &c = b.lato[i];
  c.carica = 0;
  c.invulnerabile = false;
  if (c.bloccata && !staInIra(c)) {
    if (MOVE_TBL[c.bloccata].effect == EF_INTRAPPOLA) b.lato[1 - i].intrappolato = false;
    c.bloccata = c.turniBloccata = 0;
  }
}

static void attacca(Battle &b, uint8_t i, uint8_t mossa) {
  const uint8_t j = 1 - i;
  Combatant &att = b.lato[i], &dif = b.lato[j];
  const MoveEntry &m = MOVE_TBL[mossa];
  uint8_t eff = battleEfficacia(m.type, dif);

  if ((m.effect == EF_ASSORBI && m.p2 && dif.stato != ST_SONNO) ||
      (m.effect == EF_KO && statEff(att, STAT_VELOCITA) < statEff(dif, STAT_VELOCITA))) {
    evento(b, EV_FALLITA, i, mossa);
    return;
  }
  if (!eff && !ignoraTipi(m.effect)) {
    evento(b, EV_NON_HA_EFFETTO, j, mossa);
    return;
  }
  // sotto terra o in volo non lo raggiunge niente, nemmeno Comete
  if (dif.invulnerabile || !colpisce(b, att, dif, m)) {
    evento(b, EV_MANCATA, i, mossa);
    if (m.effect == EF_CALCIO_SALTO) evento(b, EV_SCHIANTO, i, mossa, ferisci(att, 1));
    return;
  }

  uint32_t danno = 0;
  uint8_t colpi = 1;
  bool brutto = false;
  switch (m.effect) {
    case EF_DANNO_LIVELLO:
      if (m.p1 == 150) {  // Psicoonda: tra 1 e 1,5 volte il livello, escluso
        uint16_t max = att.livello * 3 / 2;
        danno = 1 + (max > 1 ? b.rng.sotto(max - 1) : 0);
      } else {
        danno = att.livello;
      }
      break;
    case EF_DANNO_FISSO: danno = m.p1; break;
    case EF_META_PS: danno = dif.ps / 2 ? dif.ps / 2 : 1; break;
    case EF_KO: danno = dif.ps; break;
    default:
      brutto = battleTiraBruttoColpo(b.rng, att.velBase, m.effect == EF_BRUTTO_COLPO,
                                     att.concentrazione);
      danno = battleDanno(att, dif, mossa, brutto, (uint8_t)(217 + b.rng.sotto(39)));
      if (!danno) {  // i tipi l'hanno ridotto a 0: in Gen 1 è un colpo mancato
        evento(b, EV_MANCATA, i, mossa);
        return;
      }
      if (m.effect == EF_MULTIPLO) colpi = m.p1 ? m.p1 : tiraColpi(b.rng);
      break;
  }

  // in Gen 1 i colpi multipli ripetono lo stesso danno, e si fermano al KO
  uint32_t totale = 0;
  uint8_t fatti = 0;
  while (fatti < colpi && !dif.ko()) {
    totale += ferisci(dif, danno);
    fatti++;
  }
  if (brutto) evento(b, EV_BRUTTO_COLPO, i, mossa);
  if (eff != 4 && !ignoraTipi(m.effect) && m.effect != EF_KO) evento(b, EV_EFFICACIA, j, mossa, eff);
  evento(b, EV_DANNO, j, mossa, (uint16_t)totale);
  if (colpi > 1) evento(b, EV_COLPI, i, mossa, fatti);
  if (totale && !dif.ko() && staInIra(dif) && dif.stadio[STAT_ATTACCO] < 6) {
    dif.stadio[STAT_ATTACCO]++;
    evento(b, EV_IRA, j, mossa);
  }

  switch (m.effect) {
    case EF_RINCULO:
      evento(b, EV_RINCULO, i, mossa, ferisci(att, totale / m.p1 ? totale / m.p1 : 1));
      break;
    case EF_ASSORBI: {
      uint32_t c = totale * m.p1 / 100;
      evento(b, EV_ASSORBE, i, mossa, cura(att, c ? c : 1));
      break;
    }
    case EF_RICARICA:
      if (!dif.ko()) att.ricarica = true;  // niente ricarica se ha mandato KO
      break;
    case EF_INTRAPPOLA:  // da 2 a 5 colpi in tutto, sempre con lo stesso danno
      if (!dif.ko()) {
        att.bloccata = mossa;
        att.turniBloccata = (uint8_t)(tiraColpi(b.rng) - 1);
        att.dannoBloccato = (uint16_t)(danno > 65535 ? 65535 : danno);
        dif.intrappolato = true;
      }
      break;
    default: break;
  }
  if (dif.ko()) return;

  // effetti secondari, sul bersaglio ancora in piedi
  bool fuoco = m.type == T_FUOCO && m.effect == EF_STATO_SECONDARIO && m.p1 == ST_SCOTTATURA;
  if (fuoco && dif.stato == ST_CONGELAMENTO) {  // in Gen 1 solo il fuoco scongela
    dif.stato = ST_NESSUNO;
    evento(b, EV_SCONGELA, j, mossa);
    return;
  }
  if (m.effect == EF_STATO_SECONDARIO && b.rng.sotto(100) < (uint8_t)m.p2) {
    infliggi(b, j, m.p1, mossa, true);
  } else if (m.effect == EF_MULTIPLO && m.p2 && b.rng.sotto(100) < (uint8_t)m.p2) {
    infliggi(b, j, ST_VELENO, mossa, true);  // Doppio Ago
  } else if (m.effect == EF_STADIO_SECONDARIO && !dif.nebbia &&
             b.rng.sotto(100) < (uint8_t)m.p2) {
    cambiaStadio(b, j, m.p1, -1, mossa);
  }
}

// Nube: stadi a 0 e stati volatili via per entrambi; Tossina torna veleno normale; il
// bersaglio guarisce anche dal suo stato (come in Gen 1)
static void azzera(Battle &b, uint8_t i) {
  for (int k = 0; k < 2; k++) {
    Combatant &c = b.lato[k];
    for (int s = 0; s < 6; s++) c.stadio[s] = 0;
    c.confusione = 0;
    c.parassiseme = c.concentrazione = c.nebbia = false;
    c.riflesso = c.schermoluce = false;
    if (c.stato == ST_TOSSICO) c.stato = ST_VELENO;
    c.tossico = 0;
  }
  Combatant &dif = b.lato[1 - i];
  dif.stato = ST_NESSUNO;
  dif.turniSonno = 0;
}

static void trasforma(Combatant &att, const Combatant &dif) {
  salvaOriginali(att);
  att.dex = dif.dex;
  att.tipo1 = dif.tipo1;
  att.tipo2 = dif.tipo2;
  att.velBase = dif.velBase;
  for (int s = 0; s < 4; s++) att.stat[s] = dif.stat[s];
  for (int s = 0; s < 6; s++) att.stadio[s] = dif.stadio[s];
  for (int s = 0; s < BATTLE_MOSSE; s++) {  // le mosse copiate hanno 5 PP
    att.mosse[s] = dif.mosse[s];
    att.ppMax[s] = att.pp[s] = dif.mosse[s] ? 5 : 0;
  }
}

static void mossaDiStato(Battle &b, uint8_t i, uint8_t mossa) {
  const uint8_t j = 1 - i;
  Combatant &att = b.lato[i], &dif = b.lato[j];
  const MoveEntry &m = MOVE_TBL[mossa];
  // le mosse sul bersaglio tirano la precisione e non raggiungono Fossa e Volo
  bool suAltro = (m.effect == EF_STADIO && m.p2 < 0) || m.effect == EF_STATO ||
                 m.effect == EF_PARASSISEME;
  if (suAltro && (dif.invulnerabile || !colpisce(b, att, dif, m))) {
    evento(b, EV_MANCATA, i, mossa);
    return;
  }

  switch (m.effect) {
    case EF_STADIO:
      if (m.p2 < 0 && dif.nebbia) evento(b, EV_PROTETTO, j, mossa);
      else if (!cambiaStadio(b, m.p2 > 0 ? i : j, m.p1, m.p2, mossa))
        evento(b, EV_NESSUN_EFFETTO, i, mossa);
      break;
    case EF_CURA:
      if (att.ps == att.psMax) evento(b, EV_FALLITA, i, mossa);
      else evento(b, EV_CURA, i, mossa, cura(att, (uint32_t)att.psMax * m.p1 / 100));
      break;
    case EF_STATO: {
      bool veleno = m.p1 == ST_VELENO || m.p1 == ST_TOSSICO;
      // Onda Tuono non tocca i tipi terra, il veleno non tocca i tipi veleno
      if ((m.p1 == ST_PARALISI && !battleEfficacia(m.type, dif)) ||
          (veleno && haTipo(dif, T_VELENO)))
        evento(b, EV_NON_HA_EFFETTO, j, mossa);
      else if (!infliggi(b, j, m.p1, mossa, false))
        evento(b, EV_FALLITA, i, mossa);
      break;
    }
    case EF_RIPOSO:
      if (att.ps == att.psMax) {
        evento(b, EV_FALLITA, i, mossa);
        break;
      }
      evento(b, EV_CURA, i, mossa, cura(att, att.psMax));
      att.stato = ST_SONNO;  // sostituisce lo stato che aveva
      att.turniSonno = 2;
      att.tossico = 0;
      evento(b, EV_STATO, i, mossa, ST_SONNO);
      break;
    case EF_PARASSISEME:
      if (haTipo(dif, T_ERBA)) evento(b, EV_NON_HA_EFFETTO, j, mossa);
      else if (dif.parassiseme) evento(b, EV_FALLITA, i, mossa);
      else {
        dif.parassiseme = true;
        evento(b, EV_SEME, j, mossa);
      }
      break;
    case EF_TRASFORMAZIONE:
      trasforma(att, dif);
      evento(b, EV_TRASFORMA, i, mossa, (uint16_t)dif.dex);
      break;
    case EF_NEBBIA:
      if (att.nebbia) evento(b, EV_FALLITA, i, mossa);
      else {
        att.nebbia = true;
        evento(b, EV_NEBBIA, i, mossa);
      }
      break;
    case EF_CONCENTRAZIONE:
      if (att.concentrazione) evento(b, EV_FALLITA, i, mossa);
      else {
        att.concentrazione = true;
        evento(b, EV_CONCENTRAZIONE, i, mossa);
      }
      break;
    case EF_BARRIERA: {
      bool &barriera = m.p1 ? att.schermoluce : att.riflesso;
      if (barriera) evento(b, EV_FALLITA, i, mossa);
      else {
        barriera = true;
        evento(b, EV_BARRIERA, i, mossa, m.p1);
      }
      break;
    }
    case EF_AZZERA:
      azzera(b, i);
      evento(b, EV_AZZERA, i, mossa);
      break;
    case EF_CONVERSIONE:
      salvaOriginali(att);
      att.tipo1 = dif.tipo1;
      att.tipo2 = dif.tipo2;
      evento(b, EV_CONVERSIONE, i, mossa, (uint16_t)(att.tipo1 | att.tipo2 << 8));
      break;
    default:  // Splash
      evento(b, EV_NESSUN_EFFETTO, i, mossa);
      break;
  }
}

// danno della confusione: Botta da 40 senza tipo, con l'attacco e la difesa di chi si
// colpisce, senza brutti colpi né fattore casuale (pokered)
static uint16_t dannoConfusione(const Combatant &c) {
  uint32_t A = statEff(c, STAT_ATTACCO), D = statEff(c, STAT_DIFESA);
  if (A > 255 || D > 255) {
    A /= 4;
    D /= 4;
  }
  if (!D) D = 1;
  uint32_t d = (2 * (uint32_t)c.livello / 5 + 2) * 40 * A / D / 50;
  return (uint16_t)((d > 997 ? 997 : d) + 2);
}

// i controlli di pokered prima della mossa, nello stesso ordine; false = azione persa
static bool puoAgire(Battle &b, uint8_t i) {
  Combatant &c = b.lato[i];
  if (c.stato == ST_SONNO) {
    if (c.turniSonno) c.turniSonno--;
    if (!c.turniSonno) {
      c.stato = ST_NESSUNO;
      evento(b, EV_SVEGLIA, i, 0);
    } else {
      evento(b, EV_DORME, i, 0);
    }
    interrompi(b, i);
    return false;
  }
  if (c.stato == ST_CONGELAMENTO) {
    evento(b, EV_CONGELATO, i, 0);
    interrompi(b, i);
    return false;
  }
  if (c.intrappolato) {
    evento(b, EV_INTRAPPOLATO, i, 0);
    interrompi(b, i);
    return false;
  }
  if (c.tentenna) {
    c.tentenna = false;
    evento(b, EV_TENTENNA, i, 0);
    interrompi(b, i);
    return false;
  }
  if (c.ricarica) {
    c.ricarica = false;
    evento(b, EV_RICARICA, i, 0);
    return false;
  }
  if (c.confusione) {
    if (!--c.confusione) {
      evento(b, EV_NON_PIU_CONFUSO, i, 0);
    } else {
      evento(b, EV_CONFUSO, i, 0);
      if (b.rng.sotto(2)) {
        evento(b, EV_SI_COLPISCE, i, 0, ferisci(c, dannoConfusione(c)));
        interrompi(b, i);
        return false;
      }
    }
  }
  if (c.stato == ST_PARALISI && !b.rng.sotto(4)) {
    evento(b, EV_PARALIZZATO, i, 0);
    interrompi(b, i);
    return false;
  }
  return true;
}

// l'azione vera e propria; restituisce la mossa usata
static uint8_t agisci(Battle &b, uint8_t i, uint8_t slot) {
  Combatant &att = b.lato[i], &dif = b.lato[1 - i];
  uint8_t mossa;
  bool continua = false;  // secondo turno o ripetizione: niente PP
  if (att.carica) {
    mossa = att.carica;
    att.carica = 0;
    att.invulnerabile = false;
    continua = true;
  } else if (att.bloccata) {
    mossa = att.bloccata;
    continua = true;
  } else {
    uint8_t s = slotUsabile(att, slot);
    if (s == SLOT_SCONTRO) {
      mossa = MOVE_STRUGGLE;  // Scontro non consuma PP
      evento(b, EV_SCONTRO, i, mossa);
    } else {
      mossa = att.mosse[s];
      att.pp[s]--;  // i PP si consumano anche se la mossa poi manca
    }
  }
  evento(b, EV_MOSSA, i, mossa);
  const MoveEntry &m = MOVE_TBL[mossa];

  if (!continua) {
    if (m.effect == EF_CARICA) {
      att.carica = mossa;
      att.invulnerabile = m.p1 == 1;
      evento(b, EV_CARICA, i, mossa);
      return mossa;
    }
    if (m.effect == EF_FURIA) {  // 2 o 3 turni in tutto
      att.bloccata = mossa;
      att.turniBloccata = (uint8_t)(1 + b.rng.sotto(2));
    } else if (m.effect == EF_IRA) {
      att.bloccata = mossa;
      att.turniBloccata = IRA_SEMPRE;
    }
  } else if (m.effect == EF_INTRAPPOLA) {
    // stessi danni del primo colpo, senza tiro di precisione
    evento(b, EV_DANNO, 1 - i, mossa, ferisci(dif, att.dannoBloccato));
    if (!--att.turniBloccata || dif.ko()) {
      att.bloccata = 0;
      dif.intrappolato = false;
    }
    return mossa;
  }

  if (fannoDanno(m)) attacca(b, i, mossa);
  else mossaDiStato(b, i, mossa);
  if (m.effect == EF_ESPLOSIONE) att.ps = 0;  // anche se manca o non ha effetto

  // fine della Furia: chi la usava resta confuso
  if (continua && m.effect == EF_FURIA && !--att.turniBloccata) {
    att.bloccata = 0;
    if (!att.ko()) infliggi(b, i, ST_CONFUSIONE, mossa, false);
  }
  return mossa;
}

static void usaMossa(Battle &b, uint8_t i, uint8_t slot) {
  bool koPrima[2] = { b.lato[0].ko(), b.lato[1].ko() };
  uint8_t mossa = puoAgire(b, i) ? agisci(b, i, slot) : 0;
  // prima il bersaglio, poi chi attacca (contraccolpo, Esplosione, confusione)
  const uint8_t ordine[2] = { (uint8_t)(1 - i), i };
  for (uint8_t k : ordine)
    if (!koPrima[k] && b.lato[k].ko()) evento(b, EV_KO, k, mossa);
}

// veleno, scottatura e Parassiseme dopo l'azione di `i`: 1/16 dei PS massimi
// (Tossina: 1/16, 2/16, 3/16…)
static void residui(Battle &b, uint8_t i) {
  Combatant &c = b.lato[i], &o = b.lato[1 - i];
  uint16_t sedicesimo = c.psMax / 16 ? c.psMax / 16 : 1;
  if (c.stato == ST_VELENO || c.stato == ST_TOSSICO || c.stato == ST_SCOTTATURA) {
    uint32_t d = sedicesimo;
    if (c.stato == ST_TOSSICO) {
      d *= c.tossico;
      if (c.tossico < 15) c.tossico++;
    }
    evento(b, c.stato == ST_SCOTTATURA ? EV_SCOTTATURA : EV_VELENO, i, 0, ferisci(c, d));
    if (c.ko()) {
      evento(b, EV_KO, i, 0);
      return;
    }
  }
  if (c.parassiseme) {
    uint16_t tolti = ferisci(c, sedicesimo);
    evento(b, EV_PARASSISEME, i, 0, tolti);
    evento(b, EV_ASSORBE, 1 - i, 0, cura(o, tolti));
    if (c.ko()) evento(b, EV_KO, i, 0);
  }
}

// ---------- turno ----------

static int8_t priorita(uint8_t mossa) {
  if (!mossa) return 0;
  const MoveEntry &m = MOVE_TBL[mossa];
  return m.effect == EF_PRIORITA ? (int8_t)m.p1 : 0;
}

void battleIniziaSquadre(Battle &b, const Combatant *a, uint8_t na, const Combatant *c,
                         uint8_t nc, uint32_t seme) {
  b = Battle();
  const Combatant *sq[2] = { a, c };
  const uint8_t n[2] = { na, nc };
  for (int k = 0; k < 2; k++) {
    b.membri[k] = n[k] < 1 ? 1 : (n[k] > BATTLE_SQUADRA ? BATTLE_SQUADRA : n[k]);
    for (uint8_t i = 0; i < b.membri[k]; i++) {
      b.squadra[k][i] = sq[k][i];
      battleRipristina(b.squadra[k][i]);
    }
    b.lato[k] = b.squadra[k][0];
  }
  b.rng.seme(seme);
}

void battleInizia(Battle &b, const Combatant &a, const Combatant &c, uint32_t seme) {
  battleIniziaSquadre(b, &a, 1, &c, 1, seme);
}

const Combatant &battleMembro(const Battle &b, uint8_t lato, uint8_t i) {
  lato &= 1;
  if (i >= b.membri[lato]) i = b.inCampo[lato];
  return i == b.inCampo[lato] ? b.lato[lato] : b.squadra[lato][i];
}

uint8_t battleRestanti(const Battle &b, uint8_t lato) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < b.membri[lato & 1]; i++)
    if (!battleMembro(b, lato, i).ko()) n++;
  return n;
}

bool battlePuoCambiare(const Battle &b, uint8_t lato, uint8_t i) {
  lato &= 1;
  if (i >= b.membri[lato] || i == b.inCampo[lato] || b.squadra[lato][i].ko()) return false;
  if (b.deveCambiare[lato]) return true;
  const Combatant &c = b.lato[lato];
  return !c.ricarica && !c.carica && !c.bloccata;
}

// chi esce perde gli stati volatili, come in Gen 1: restano PS, PP e lo stato non
// volatile (Tossina torna veleno normale); dopo una Trasformazione tornano specie, tipi,
// statistiche e mosse originali, con i loro PP
static void aRiposo(Combatant &c) {
  if (c.trasformato) {
    c.dex = c.orig.dex;
    c.tipo1 = c.orig.tipo1;
    c.tipo2 = c.orig.tipo2;
    c.velBase = c.orig.velBase;
    for (int i = 0; i < 4; i++) c.stat[i] = c.orig.stat[i];
    for (int i = 0; i < BATTLE_MOSSE; i++) {
      // Conversione non cambia le mosse: i PP spesi dopo restano spesi
      bool copiata = c.mosse[i] != c.orig.mosse[i] || c.ppMax[i] != c.orig.ppMax[i];
      if (copiata) c.pp[i] = c.orig.pp[i];
      c.mosse[i] = c.orig.mosse[i];
      c.ppMax[i] = c.orig.ppMax[i];
    }
  }
  Combatant f;
  f.dex = c.dex;
  f.livello = c.livello;
  f.tipo1 = c.tipo1;
  f.tipo2 = c.tipo2;
  f.velBase = c.velBase;
  f.psMax = c.psMax;
  f.ps = c.ps;
  for (int i = 0; i < 4; i++) f.stat[i] = c.stat[i];
  for (int i = 0; i < BATTLE_MOSSE; i++) {
    f.mosse[i] = c.mosse[i];
    f.pp[i] = c.pp[i];
    f.ppMax[i] = c.ppMax[i];
  }
  f.stato = c.stato == ST_TOSSICO ? (uint8_t)ST_VELENO : c.stato;
  f.turniSonno = c.turniSonno;
  c = f;
}

static void entra(Battle &b, uint8_t k, uint8_t i) {
  b.inCampo[k] = i;
  b.lato[k] = b.squadra[k][i];
  b.deveCambiare[k] = false;
  evento(b, EV_ENTRA, k, 0, (uint16_t)(i << 8 | (uint8_t)b.lato[k].dex));
}

// cambio scelto come azione del turno: chi esce scioglie le prese (sue e dell'avversario
// su di lui) e torna nella squadra a riposo
static void cambia(Battle &b, uint8_t k, uint8_t i) {
  Combatant &c = b.lato[k], &o = b.lato[1 - k];
  if (c.bloccata && MOVE_TBL[c.bloccata].effect == EF_INTRAPPOLA) o.intrappolato = false;
  if (c.intrappolato && o.bloccata && MOVE_TBL[o.bloccata].effect == EF_INTRAPPOLA)
    o.bloccata = o.turniBloccata = 0;
  aRiposo(c);
  b.squadra[k][b.inCampo[k]] = c;
  evento(b, EV_RITIRO, k, 0, (uint16_t)(b.inCampo[k] << 8 | (uint8_t)c.dex));
  entra(b, k, i);
}

bool battleCambioForzato(Battle &b, uint8_t lato, uint8_t i) {
  lato &= 1;
  if (b.vincitore != BATTLE_IN_CORSO || !b.deveCambiare[lato]) return false;
  if (!battlePuoCambiare(b, lato, i)) return false;
  b.nEventi = 0;
  b.squadra[lato][b.inCampo[lato]] = b.lato[lato];  // il KO resta com'è
  entra(b, lato, i);
  return true;
}

static bool nessunoKo(const Battle &b) { return !b.lato[0].ko() && !b.lato[1].ko(); }

bool battleTurno(Battle &b, uint8_t sceltaA, uint8_t sceltaB) {
  b.nEventi = 0;
  if (b.vincitore != BATTLE_IN_CORSO) return false;

  // un cambio forzato che la UI non ha fatto: entra il primo vivo
  for (uint8_t k = 0; k < 2; k++) {
    if (!b.deveCambiare[k]) continue;
    b.squadra[k][b.inCampo[k]] = b.lato[k];
    entra(b, k, battleSceltaSostituto(b, k, false));
  }

  const uint8_t scelta[2] = { sceltaA, sceltaB };
  bool cambio[2];
  uint8_t mossa[2];
  for (uint8_t k = 0; k < 2; k++) {
    const Combatant &c = b.lato[k];
    cambio[k] = scelta[k] >= BATTLE_CAMBIA &&
                battlePuoCambiare(b, k, (uint8_t)(scelta[k] - BATTLE_CAMBIA));
    if (cambio[k]) mossa[k] = 0;
    else if (battleDeveScegliere(c)) mossa[k] = battleMossaScelta(c, scelta[k]);
    else mossa[k] = c.ricarica || c.intrappolato ? 0 : (c.carica ? c.carica : c.bloccata);
  }
  // i cambi prima delle mosse: chi entra subisce la mossa dell'avversario
  for (uint8_t k = 0; k < 2; k++)
    if (cambio[k]) cambia(b, k, (uint8_t)(scelta[k] - BATTLE_CAMBIA));

  uint8_t primo;
  int8_t p0 = priorita(mossa[0]), p1 = priorita(mossa[1]);
  if (cambio[0] || cambio[1]) {
    primo = cambio[0] ? 1 : 0;  // agisce solo chi non ha cambiato
  } else if (p0 != p1) {
    primo = p0 > p1 ? 0 : 1;
  } else {
    uint16_t v0 = statEff(b.lato[0], STAT_VELOCITA), v1 = statEff(b.lato[1], STAT_VELOCITA);
    primo = v0 != v1 ? (v0 > v1 ? 0 : 1) : (uint8_t)b.rng.sotto(2);
  }

  const uint8_t ordine[2] = { primo, (uint8_t)(1 - primo) };
  for (uint8_t k : ordine) {
    if (cambio[k]) continue;
    if (nessunoKo(b)) usaMossa(b, k, scelta[k]);
    if (nessunoKo(b)) residui(b, k);
  }
  b.lato[0].tentenna = b.lato[1].tentenna = false;  // il tentennamento dura un turno
  b.turno++;

  // finisce quando un lato è tutto KO; se no chi è KO in campo deve cambiare
  bool fine0 = !battleRestanti(b, 0), fine1 = !battleRestanti(b, 1);
  if (fine0 && fine1) b.vincitore = BATTLE_PAREGGIO;
  else if (fine0) b.vincitore = 1;
  else if (fine1) b.vincitore = 0;
  else if (b.turno >= BATTLE_MAX_TURNI) b.vincitore = BATTLE_PAREGGIO;
  if (b.vincitore == BATTLE_IN_CORSO) {
    b.deveCambiare[0] = b.lato[0].ko();
    b.deveCambiare[1] = b.lato[1].ko();
    return true;
  }
  evento(b, EV_FINE, 0, 0, b.vincitore);
  return false;
}

// ---------- IA della CPU ----------

#define IA_INUTILE (-1000)  // mossa che non farebbe niente: solo se non c'è altro

// danno atteso di una mossa di danno (fattore casuale medio, senza brutti colpi)
static uint32_t dannoAtteso(const Combatant &io, const Combatant &lui, uint8_t mossa) {
  const MoveEntry &m = MOVE_TBL[mossa];
  switch (m.effect) {
    case EF_DANNO_LIVELLO: return m.p1 == 150 ? io.livello * 3 / 4 : io.livello;
    case EF_DANNO_FISSO: return m.p1;
    case EF_META_PS: return lui.ps / 2 ? lui.ps / 2 : 1;
    case EF_KO: return lui.ps;
    default: break;
  }
  uint32_t d = battleDanno(io, lui, mossa, false, 236);
  if (m.effect == EF_MULTIPLO) d *= m.p1 ? m.p1 : 3;
  return d;
}

// il punteggio di DylanPDao (aiChooseMove), con il danno in % dei PS ed esteso alle mosse
// di stato di Gen 1
static int32_t punteggioIA(const Combatant &io, const Combatant &lui, uint8_t mossa) {
  const MoveEntry &m = MOVE_TBL[mossa];
  int32_t prec = m.accuracy && m.effect != EF_MAI_FALLISCE ? m.accuracy : 100;
  if (fannoDanno(m)) {
    if (!battleEfficacia(m.type, lui) && !ignoraTipi(m.effect)) return IA_INUTILE;
    if (m.effect == EF_ASSORBI && m.p2 && lui.stato != ST_SONNO) return IA_INUTILE;
    if (m.effect == EF_KO && statEff(io, STAT_VELOCITA) < statEff(lui, STAT_VELOCITA))
      return IA_INUTILE;
    // in percentuale dei PS che gli restano: così si confronta con le mosse di stato a
    // qualunque livello
    uint32_t dmg = dannoAtteso(io, lui, mossa);
    bool ko = dmg >= lui.ps;
    int32_t sc = ko ? 100 : (int32_t)(dmg * 100 / (lui.ps ? lui.ps : 1));
    if (ko) sc += 1000;  // un KO in questo turno batte tutto
    sc = sc * prec / 100;
    if (m.effect == EF_RICARICA && !ko) sc -= sc / 4;  // un turno regalato
    if (m.effect == EF_RINCULO) sc -= sc / 6;
    if (m.effect == EF_CARICA) sc -= m.p1 ? sc / 6 : sc / 3;  // Fossa e Volo intanto schivano
    if (m.effect == EF_ESPLOSIONE && !ko) sc -= 200;
    if (m.effect == EF_STATO_SECONDARIO && m.p1 < ST_CONFUSIONE && lui.stato == ST_NESSUNO)
      sc += m.p2 / 4;
    return sc;
  }

  int32_t mancano = (int32_t)(io.psMax - io.ps) * 100 / (io.psMax ? io.psMax : 1);
  bool debole = io.ps * 3 < io.psMax;  // un altro colpo e va KO: niente preparativi
  switch (m.effect) {
    case EF_CURA: return io.ps == io.psMax ? IA_INUTILE : mancano - 30;
    case EF_RIPOSO: return io.ps == io.psMax ? IA_INUTILE : mancano - 45;
    case EF_STADIO: {
      if (m.p2 > 0) {
        if (io.stadio[m.p1] >= 6) return IA_INUTILE;
        // +2 ATK vale molto a stadio 0, niente a +6
        return 26 - io.stadio[m.p1] * 12 / m.p2 - (debole ? 40 : 0);
      }
      if (lui.nebbia || lui.stadio[m.p1] <= -6) return IA_INUTILE;
      return (22 + lui.stadio[m.p1] * 8) * prec / 100 - (debole ? 40 : 0);
    }
    case EF_STATO: {
      if (m.p1 == ST_CONFUSIONE) return lui.confusione ? IA_INUTILE : 30 * prec / 100;
      bool veleno = m.p1 == ST_VELENO || m.p1 == ST_TOSSICO;
      if (lui.stato != ST_NESSUNO || (veleno && haTipo(lui, T_VELENO)) ||
          (m.p1 == ST_PARALISI && !battleEfficacia(m.type, lui)))
        return IA_INUTILE;
      int32_t v = m.p1 == ST_SONNO ? 60 : (m.p1 == ST_VELENO ? 30 : 45);
      return v * prec / 100;
    }
    case EF_PARASSISEME:
      return lui.parassiseme || haTipo(lui, T_ERBA) ? IA_INUTILE : 35 * prec / 100;
    case EF_NEBBIA: return io.nebbia ? IA_INUTILE : 10;
    case EF_CONCENTRAZIONE: return io.concentrazione ? IA_INUTILE : 15 - (debole ? 40 : 0);
    case EF_BARRIERA:
      return (m.p1 ? io.schermoluce : io.riflesso) ? IA_INUTILE : 20 - (debole ? 40 : 0);
    case EF_AZZERA: {
      int32_t utile = 0;  // stadi buoni dell'altro e cattivi miei da cancellare
      for (int s = 0; s < 6; s++) utile += lui.stadio[s] - io.stadio[s];
      return utile > 0 ? utile * 10 : IA_INUTILE;
    }
    case EF_TRASFORMAZIONE: return io.trasformato ? IA_INUTILE : 10;
    case EF_CONVERSIONE:
      return io.tipo1 == lui.tipo1 && io.tipo2 == lui.tipo2 ? IA_INUTILE : 0;
    default: return IA_INUTILE;  // Splash
  }
}

uint8_t battleSceltaIA(Battle &b, uint8_t lato, bool difficile) {
  const Combatant &io = b.lato[lato & 1], &lui = b.lato[1 - (lato & 1)];
  uint8_t slot[BATTLE_MOSSE], n = 0;
  for (uint8_t i = 0; i < BATTLE_MOSSE; i++)
    if (io.mosse[i] && io.pp[i]) slot[n++] = i;
  if (!n) return 0;  // nessun PP: Scontro
  if (!difficile) return slot[b.rng.sotto(n)];
  uint8_t migliore = slot[0];
  int32_t record = INT32_MIN;
  for (uint8_t k = 0; k < n; k++) {
    int32_t sc = punteggioIA(io, lui, io.mosse[slot[k]]);
    if (sc > record) {
      record = sc;
      migliore = slot[k];
    }
  }
  return migliore;
}

// ---------- IA del sostituto ----------

// rapporto di tipo di `io` contro `lui`, ×64: la miglior efficacia delle sue mosse di danno
// con PP (quelle che ignorano i tipi valgono 1×) diviso la peggiore dei tipi di `lui` su `io`
static int32_t rapportoTipo(const Combatant &io, const Combatant &lui) {
  uint8_t dato = 0;
  for (uint8_t s = 0; s < BATTLE_MOSSE; s++) {
    if (!io.mosse[s] || !io.pp[s]) continue;
    const MoveEntry &m = MOVE_TBL[io.mosse[s]];
    if (!fannoDanno(m)) continue;
    uint8_t e = ignoraTipi(m.effect) ? 4 : battleEfficacia(m.type, lui);
    if (e > dato) dato = e;
  }
  uint8_t preso = battleEfficacia(lui.tipo1, io);
  if (lui.tipo2 < TYPE_COUNT) {
    uint8_t e2 = battleEfficacia(lui.tipo2, io);
    if (e2 > preso) preso = e2;
  }
  return (int32_t)dato * 64 / (preso ? preso : 1);
}

uint8_t battleSceltaSostituto(const Battle &b, uint8_t lato, bool difficile) {
  lato &= 1;
  const Combatant &lui = b.lato[1 - lato];
  uint8_t migliore = BATTLE_NESSUNO;
  int32_t record = 0;
  uint16_t psRecord = 0;
  for (uint8_t i = 0; i < b.membri[lato]; i++) {
    if (i == b.inCampo[lato] || b.squadra[lato][i].ko()) continue;
    const Combatant &c = b.squadra[lato][i];
    if (!difficile) return i;
    int32_t sc = rapportoTipo(c, lui);
    if (migliore == BATTLE_NESSUNO || sc > record || (sc == record && c.ps > psRecord)) {
      migliore = i;
      record = sc;
      psRecord = c.ps;
    }
  }
  return migliore;
}
