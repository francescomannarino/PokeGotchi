// Motore di battaglia (battle.cpp): danni di Gen 1, tipi, STAB, PP, Scontro, determinismo,
// stati, mosse a due turni e ripetute, coda della narrazione, IA, statistiche dal pet.
#include "framework.h"
#include "../battle.h"
#include "../pet.h"
#include <initializer_list>
#include <vector>

// combattente con statistiche date e mosse esplicite (slot mancanti = vuoti)
static Combatant crea(int16_t dex, uint8_t lv, uint16_t ps, uint16_t att, uint16_t dif,
                      uint16_t vel, uint16_t spc, std::initializer_list<uint8_t> mosse) {
  uint8_t m[BATTLE_MOSSE] = { 0, 0, 0, 0 };
  int i = 0;
  for (uint8_t x : mosse) m[i++] = x;
  Combatant c;
  battleCombattente(c, dex, lv, ps, att, dif, vel, spc, m);
  return c;
}

// statistiche di Gen 1 al 100 con DV 15 e stat exp al massimo
static Combatant tauros(std::initializer_list<uint8_t> m) { return crea(128, 100, 353, 298, 288, 318, 238, m); }
static Combatant chansey(std::initializer_list<uint8_t> m) { return crea(113, 100, 703, 108, 108, 198, 308, m); }
static Combatant alakazam(std::initializer_list<uint8_t> m) { return crea(65, 100, 313, 198, 188, 338, 368, m); }

static int contaEventi(const Battle &b, uint8_t tipo, int lato = -1) {
  int n = 0;
  for (int i = 0; i < b.nEventi; i++)
    if (b.eventi[i].tipo == tipo && (lato < 0 || b.eventi[i].lato == lato)) n++;
  return n;
}

static const BattleEvento *trovaEvento(const Battle &b, uint8_t tipo, int lato = -1) {
  for (int i = 0; i < b.nEventi; i++)
    if (b.eventi[i].tipo == tipo && (lato < 0 || b.eventi[i].lato == lato)) return &b.eventi[i];
  return nullptr;
}

// ---------- formula del danno ----------

TEST(battle, corpo_a_corpo_di_tauros_su_chansey_come_in_gen_1) {
  // ATK 298 e DEF 108: sopra 255, entrambi /4 -> 74 e 27
  // (2*100/5+2) = 42; 42*85*74 = 264180; /27 = 9784; /50 = 195; +2 = 197
  // STAB: 197 + 98 = 295; normale su normale 1x; r 255 -> 295, r 217 -> 295*217/255 = 251
  Combatant t = tauros({ MV_BODY_SLAM }), c = chansey({});
  CHECK_EQ(battleDanno(t, c, MV_BODY_SLAM, false, 255), (uint16_t)295);
  CHECK_EQ(battleDanno(t, c, MV_BODY_SLAM, false, 217), (uint16_t)251);
}

TEST(battle, psichico_usa_la_speciale_in_attacco_e_in_difesa) {
  // SPC 368 e 308 -> /4: 92 e 77
  // 42*90*92 = 347760; /77 = 4516; /50 = 90; +2 = 92; STAB 92 + 46 = 138
  // r 255 -> 138, r 217 -> 138*217/255 = 117
  Combatant a = alakazam({ MV_PSYCHIC }), c = chansey({});
  CHECK_EQ(battleDanno(a, c, MV_PSYCHIC, false, 255), (uint16_t)138);
  CHECK_EQ(battleDanno(a, c, MV_PSYCHIC, false, 217), (uint16_t)117);
}

TEST(battle, fulmine_superefficace_con_stab_sotto_i_255) {
  // L50, SPC 100 contro SPC 50: (2*50/5+2) = 22; 22*95*100 = 209000; /50 = 4180; /50 = 83
  // +2 = 85; STAB 85 + 42 = 127; elettro su acqua 2x = 254; r 217 -> 254*217/255 = 216
  Combatant p = crea(25, 50, 120, 60, 50, 100, 100, { MV_THUNDERBOLT });
  Combatant s = crea(7, 50, 120, 60, 80, 50, 50, {});
  CHECK_EQ(battleDanno(p, s, MV_THUNDERBOLT, false, 255), (uint16_t)254);
  CHECK_EQ(battleDanno(p, s, MV_THUNDERBOLT, false, 217), (uint16_t)216);
}

TEST(battle, il_brutto_colpo_raddoppia_il_livello) {
  // livello 200: (2*200/5+2) = 82; 82*85*74 = 515780; /27 = 19102; /50 = 382; +2 = 384
  // STAB 384 + 192 = 576
  Combatant t = tauros({ MV_BODY_SLAM }), c = chansey({});
  CHECK_EQ(battleDanno(t, c, MV_BODY_SLAM, true, 255), (uint16_t)576);
}

TEST(battle, il_brutto_colpo_ignora_stadi_scottatura_e_riflesso) {
  Combatant t = tauros({ MV_BODY_SLAM }), c = chansey({});
  t.stadio[STAT_ATTACCO] = -2;
  t.stato = ST_SCOTTATURA;
  c.stadio[STAT_DIFESA] = 6;
  c.riflesso = true;
  CHECK_EQ(battleDanno(t, c, MV_BODY_SLAM, true, 255), (uint16_t)576);
  // senza brutto colpo: ATK 298*50% = 149, scottata 74; DEF 108*400% = 432, riflesso 864
  // -> /4: 18 e 216; 42*85*18 = 64260; /216 = 297; /50 = 5; +2 = 7; STAB 7 + 3 = 10
  CHECK_EQ(battleDanno(t, c, MV_BODY_SLAM, false, 255), (uint16_t)10);
}

TEST(battle, gli_stadi_seguono_i_rapporti_di_gen_1) {
  CHECK_EQ(battleStadio(100, 0), (uint16_t)100);
  CHECK_EQ(battleStadio(100, 1), (uint16_t)150);
  CHECK_EQ(battleStadio(100, -1), (uint16_t)66);
  CHECK_EQ(battleStadio(100, 6), (uint16_t)400);
  CHECK_EQ(battleStadio(100, -6), (uint16_t)25);
  CHECK_EQ(battleStadio(300, 6), (uint16_t)999);  // tetto di Gen 1
  CHECK_EQ(battleStadio(1, -6), (uint16_t)1);
}

TEST(battle, esplosione_dimezza_la_difesa) {
  // L50 ATK 100 contro DEF 100 -> 50: 22*170*100 = 374000; /50 = 7480; /50 = 149
  // +2 = 151; senza STAB (Pikachu non è normale); r 255 -> 151
  Combatant p = crea(25, 50, 120, 100, 50, 100, 100, { MV_EXPLOSION });
  Combatant r = crea(19, 50, 120, 60, 100, 50, 50, {});
  CHECK_EQ(battleDanno(p, r, MV_EXPLOSION, false, 255), (uint16_t)151);
}

TEST(battle, sotto_i_2_ps_il_fattore_casuale_non_si_applica) {
  // L1, SPC 1 contro 999 -> /4: 0 e 249; (2/5+2) = 2; 2*55*0/249/50 = 0; +2 = 2
  // niente STAB (Rattata è normale); erba su fuoco ½ -> 1
  Combatant a = crea(19, 1, 10, 1, 1, 1, 1, { MV_RAZOR_LEAF });
  Combatant f = crea(4, 1, 10, 1, 999, 1, 999, {});
  CHECK_EQ(battleDanno(a, f, MV_RAZOR_LEAF, false, 217), (uint16_t)1);
}

// ---------- tipi, STAB, categoria ----------

TEST(battle, efficacia_dei_tipi_doppi) {
  Combatant bulbasaur = crea(1, 5, 20, 10, 10, 10, 10, {});
  Combatant paras = crea(46, 5, 20, 10, 10, 10, 10, {});
  Combatant geodude = crea(74, 5, 20, 10, 10, 10, 10, {});
  Combatant gastly = crea(92, 5, 20, 10, 10, 10, 10, {});
  Combatant alak = crea(65, 5, 20, 10, 10, 10, 10, {});
  Combatant charizard = crea(6, 5, 20, 10, 10, 10, 10, {});
  Combatant dragonite = crea(149, 5, 20, 10, 10, 10, 10, {});
  CHECK_EQ(battleEfficacia(T_FUOCO, bulbasaur), (uint8_t)8);   // erba 2x, veleno 1x
  CHECK_EQ(battleEfficacia(T_FUOCO, paras), (uint8_t)16);      // coleottero e erba
  CHECK_EQ(battleEfficacia(T_ACQUA, geodude), (uint8_t)16);
  CHECK_EQ(battleEfficacia(T_ELETTRO, geodude), (uint8_t)0);   // terra immune
  CHECK_EQ(battleEfficacia(T_NORMALE, gastly), (uint8_t)0);
  CHECK_EQ(battleEfficacia(T_SPETTRO, alak), (uint8_t)0);      // il bug di Rosso/Blu
  CHECK_EQ(battleEfficacia(T_ERBA, charizard), (uint8_t)1);    // fuoco ½, volante ½
  CHECK_EQ(battleEfficacia(T_GHIACCIO, dragonite), (uint8_t)16);
  CHECK_EQ(battleEfficacia(T_NORMALE, bulbasaur), (uint8_t)4);
}

TEST(battle, i_tipi_moltiplicano_il_danno_uno_alla_volta) {
  // L50, SPC 50 contro 50, Braciere 40: 22*40*50 = 44000; /50 = 880; /50 = 17; +2 = 19
  // senza STAB (Rattata). Paras: 19*4/2 = 38, *4/2 = 76. Charizard: fuoco 19/2 = 9,
  // volante 1x = 9
  Combatant r = crea(19, 50, 100, 50, 50, 50, 50, { MV_EMBER });
  Combatant paras = crea(46, 50, 100, 50, 50, 50, 50, {});
  Combatant charizard = crea(6, 50, 100, 50, 50, 50, 50, {});
  Combatant ratt = crea(19, 50, 100, 50, 50, 50, 50, {});
  CHECK_EQ(battleDanno(r, ratt, MV_EMBER, false, 255), (uint16_t)19);
  CHECK_EQ(battleDanno(r, paras, MV_EMBER, false, 255), (uint16_t)76);
  CHECK_EQ(battleDanno(r, charizard, MV_EMBER, false, 255), (uint16_t)9);
  Combatant gastly = crea(92, 50, 100, 50, 50, 50, 50, {});
  CHECK_EQ(battleDanno(r, gastly, MV_TACKLE, false, 255), (uint16_t)0);
}

TEST(battle, lo_stab_vale_una_volta_e_mezza) {
  // stesso attaccante, una volta di tipo fuoco: 19 -> 19 + 9 = 28
  Combatant a = crea(19, 50, 100, 50, 50, 50, 50, { MV_EMBER });
  Combatant d = crea(19, 50, 100, 50, 50, 50, 50, {});
  CHECK_EQ(battleDanno(a, d, MV_EMBER, false, 255), (uint16_t)19);
  a.tipo2 = T_FUOCO;  // secondo tipo: conta anche lui
  CHECK_EQ(battleDanno(a, d, MV_EMBER, false, 255), (uint16_t)28);
}

TEST(battle, la_categoria_dipende_dal_tipo_della_mossa) {
  // ATK alto e SPC bassa: Braciere (speciale) è debole, Azione (fisica) forte
  Combatant a = crea(19, 50, 100, 200, 50, 50, 20, { MV_EMBER, MV_TACKLE });
  Combatant d = crea(19, 50, 100, 50, 20, 50, 200, {});
  uint16_t fuoco = battleDanno(a, d, MV_EMBER, false, 255);
  uint16_t normale = battleDanno(a, d, MV_TACKLE, false, 255);
  // Braciere: 22*40*20/200/50 = 1; +2 = 3. Azione: 22*35*200/20/50 = 154; +2 = 156, STAB 234
  CHECK_EQ(fuoco, (uint16_t)3);
  CHECK_EQ(normale, (uint16_t)234);
}

// ---------- brutti colpi ----------

TEST(battle, probabilita_del_brutto_colpo_di_gen_1) {
  BattleRng rng;
  rng.seme(1234);
  const int N = 200000;
  int normali = 0, alti = 0, lenti = 0;
  for (int i = 0; i < N; i++) {
    normali += battleTiraBruttoColpo(rng, 100, false);  // 50/256
    alti += battleTiraBruttoColpo(rng, 100, true);      // 400 -> 255/256
    lenti += battleTiraBruttoColpo(rng, 1, false);      // 0/256: mai
  }
  CHECK_RANGE(normali, N * 50 / 256 - 1500, N * 50 / 256 + 1500);
  CHECK_RANGE(alti, N * 255 / 256 - 500, N);
  CHECK_EQ(lenti, 0);
}

// ---------- PP e Scontro ----------

TEST(battle, i_pp_si_consumano_poi_arriva_scontro) {
  // due muri: nessuno va KO in 40 turni
  Combatant a = crea(19, 10, 999, 10, 999, 50, 999, { MV_SONIC_BOOM });
  Combatant b = crea(19, 10, 999, 10, 999, 10, 999, { MV_SONIC_BOOM });
  Battle bt;
  battleInizia(bt, a, b, 7);
  CHECK_EQ(bt.lato[0].pp[0], (uint8_t)20);
  for (int t = 0; t < 20; t++) {
    CHECK(!battleDeveScontro(bt.lato[0]));
    CHECK(battleTurno(bt, 0, 0));
    CHECK_EQ(contaEventi(bt, EV_SCONTRO), 0);
  }
  CHECK_EQ(bt.lato[0].pp[0], (uint8_t)0);
  CHECK(battleDeveScontro(bt.lato[0]));
  CHECK_EQ(battleMossaScelta(bt.lato[0], 0), (uint8_t)MOVE_STRUGGLE);
  // da qui Scontro, finché uno dei due colpisce (manca 1 volta su 256)
  bool colpito = false;
  for (int t = 0; t < 10 && !colpito; t++) {
    CHECK(battleTurno(bt, 0, 0));
    CHECK_EQ(contaEventi(bt, EV_SCONTRO, 0), 1);
    const BattleEvento *m = trovaEvento(bt, EV_MOSSA, 0);
    CHECK(m && m->mossa == MOVE_STRUGGLE);
    for (int i = 0; i < bt.nEventi; i++) {
      // contraccolpo di Scontro: metà del danno inflitto, a chi l'ha usata
      const BattleEvento &d = bt.eventi[i];
      if (d.tipo != EV_DANNO || d.mossa != MOVE_STRUGGLE) continue;
      const BattleEvento *r = nullptr;
      for (int k = i + 1; k < bt.nEventi && !r; k++)
        if (bt.eventi[k].tipo == EV_RINCULO && bt.eventi[k].lato == 1 - d.lato) r = &bt.eventi[k];
      CHECK(r != nullptr);
      if (r) CHECK_EQ(r->valore, (uint16_t)(d.valore / 2 ? d.valore / 2 : 1));
      colpito = true;
    }
  }
  CHECK(colpito);
  CHECK_EQ(bt.lato[0].pp[0], (uint8_t)0);  // Scontro non consuma PP
}

TEST(battle, i_pp_tornano_pieni_a_fine_battaglia) {
  Combatant a = crea(19, 10, 999, 10, 999, 50, 999, { MV_SONIC_BOOM, MV_TACKLE });
  Combatant b = crea(19, 10, 999, 10, 999, 10, 999, { MV_SONIC_BOOM });
  Battle bt;
  battleInizia(bt, a, b, 3);
  for (int t = 0; t < 5; t++) battleTurno(bt, 1, 0);
  CHECK_EQ(bt.lato[0].pp[1], (uint8_t)30);
  CHECK(bt.lato[0].ps < bt.lato[0].psMax);
  Combatant dopo = bt.lato[0];
  battleRipristina(dopo);
  CHECK_EQ(dopo.pp[1], (uint8_t)35);
  CHECK_EQ(dopo.ps, dopo.psMax);
  // e una nuova battaglia riparte comunque da PP pieni
  Combatant usato = bt.lato[0];
  battleInizia(bt, usato, b, 3);
  CHECK_EQ(bt.lato[0].pp[1], (uint8_t)35);
}

TEST(battle, uno_slot_senza_pp_passa_al_primo_usabile) {
  Combatant a = crea(19, 10, 99, 10, 10, 10, 10, { MV_TACKLE, MV_EMBER, 0, 0 });
  a.pp[0] = 0;
  CHECK_EQ(battleMossaScelta(a, 0), (uint8_t)MV_EMBER);
  CHECK_EQ(battleMossaScelta(a, 3), (uint8_t)MV_EMBER);   // slot vuoto
  CHECK_EQ(battleMossaScelta(a, 9), (uint8_t)MV_EMBER);   // fuori range
  CHECK(!battleDeveScontro(a));
}

TEST(battle, le_mosse_apprese_sono_le_ultime_quattro) {
  uint8_t m[BATTLE_MOSSE];
  battleMosseApprese(4, 1, m);  // Charmander: Graffio e Ruggito
  CHECK_EQ(m[0], (uint8_t)MV_SCRATCH);
  CHECK_EQ(m[1], (uint8_t)MV_GROWL);
  CHECK_EQ(m[2], (uint8_t)0);
  battleMosseApprese(4, 100, m);
  for (int i = 0; i < BATTLE_MOSSE; i++) CHECK(m[i] != 0);
  for (int i = 0; i < BATTLE_MOSSE; i++)
    for (int k = i + 1; k < BATTLE_MOSSE; k++) CHECK(m[i] != m[k]);
  CHECK_EQ(m[3], LEARN_TBL[LEARN_OFS[5] - 1].move);  // l'ultima del learnset
  battleMosseApprese(63, 50, m);  // Abra: niente mosse
  CHECK_EQ(m[0], (uint8_t)MOVE_STRUGGLE);
  CHECK_EQ(m[1], (uint8_t)0);
}

// ---------- turno ----------

TEST(battle, la_priorita_batte_la_velocita) {
  Combatant lento = crea(19, 50, 500, 50, 50, 10, 50, { MV_QUICK_ATTACK, MV_TACKLE });
  Combatant veloce = crea(19, 50, 500, 50, 50, 200, 50, { MV_TACKLE });
  Battle bt;
  battleInizia(bt, lento, veloce, 11);
  battleTurno(bt, 0, 0);
  CHECK_EQ(bt.eventi[0].tipo, (uint8_t)EV_MOSSA);
  CHECK_EQ(bt.eventi[0].lato, (uint8_t)0);
  battleTurno(bt, 1, 0);  // senza priorità va prima il più veloce
  CHECK_EQ(bt.eventi[0].lato, (uint8_t)1);
}

TEST(battle, la_paralisi_divide_per_quattro_la_velocita) {
  Combatant a = crea(19, 50, 500, 50, 50, 100, 50, { MV_TACKLE });
  Combatant b = crea(19, 50, 500, 50, 50, 30, 50, { MV_TACKLE });
  Battle bt;
  battleInizia(bt, a, b, 5);
  bt.lato[0].stato = ST_PARALISI;  // 100 / 4 = 25 < 30
  battleTurno(bt, 0, 0);
  CHECK_EQ(bt.eventi[0].lato, (uint8_t)1);
}

TEST(battle, immune_al_tipo_non_perde_ps) {
  Combatant a = crea(19, 50, 500, 50, 50, 100, 50, { MV_TACKLE });
  Combatant g = crea(92, 50, 500, 50, 50, 10, 50, { MV_SPLASH });
  Battle bt;
  battleInizia(bt, a, g, 9);
  battleTurno(bt, 0, 0);
  CHECK_EQ(contaEventi(bt, EV_NON_HA_EFFETTO, 1), 1);
  CHECK_EQ(contaEventi(bt, EV_DANNO), 0);
  CHECK_EQ(contaEventi(bt, EV_NESSUN_EFFETTO, 1), 1);  // Splash
  CHECK_EQ(bt.lato[1].ps, bt.lato[1].psMax);
}

TEST(battle, sismico_e_sonicboom_ignorano_tipi_e_statistiche) {
  Combatant a = crea(19, 37, 500, 1, 50, 100, 1, { MV_SEISMIC_TOSS, MV_SONIC_BOOM });
  Combatant g = crea(92, 50, 500, 50, 999, 10, 999, { MV_SPLASH });
  for (uint32_t seme = 1; seme < 20; seme++) {
    Battle bt;
    battleInizia(bt, a, g, seme);
    battleTurno(bt, 0, 0);
    const BattleEvento *d = trovaEvento(bt, EV_DANNO, 1);
    if (!d) continue;  // mancato
    CHECK_EQ(d->valore, (uint16_t)37);
    battleTurno(bt, 1, 0);
    d = trovaEvento(bt, EV_DANNO, 1);
    if (d) CHECK_EQ(d->valore, (uint16_t)20);
  }
}

TEST(battle, superzanna_toglie_meta_dei_ps_attuali) {
  Combatant a = crea(19, 50, 500, 50, 50, 100, 50, { MV_SUPER_FANG });
  Combatant d = crea(7, 50, 301, 50, 50, 10, 50, { MV_SPLASH });
  int colpi = 0;
  for (uint32_t seme = 1; seme < 10; seme++) {
    Battle bt;
    battleInizia(bt, a, d, seme);
    battleTurno(bt, 0, 0);
    const BattleEvento *e = trovaEvento(bt, EV_DANNO, 1);
    if (!e) continue;
    CHECK_EQ(e->valore, (uint16_t)150);
    colpi++;
  }
  CHECK(colpi > 0);
}

TEST(battle, rinculo_e_assorbimento) {
  Combatant a = crea(19, 50, 500, 100, 50, 100, 100, { MV_TAKE_DOWN, MV_ABSORB });
  Combatant d = crea(19, 50, 500, 50, 50, 10, 50, { MV_TACKLE });
  int visti = 0;
  for (uint32_t seme = 1; seme < 30; seme++) {
    Battle bt;
    battleInizia(bt, a, d, seme);
    battleTurno(bt, 0, 0);
    const BattleEvento *dn = trovaEvento(bt, EV_DANNO, 1), *r = trovaEvento(bt, EV_RINCULO, 0);
    if (dn) {
      CHECK(r != nullptr);
      if (r) CHECK_EQ(r->valore, (uint16_t)(dn->valore / 4 ? dn->valore / 4 : 1));
      visti++;
    }
    battleTurno(bt, 1, 0);  // Assorbimento, dopo aver perso PS
    dn = trovaEvento(bt, EV_DANNO, 1);
    const BattleEvento *as = trovaEvento(bt, EV_ASSORBE, 0);
    if (dn) {
      CHECK(as != nullptr);
      if (as) CHECK(as->valore <= (dn->valore / 2 ? dn->valore / 2 : 1));
    }
  }
  CHECK(visti > 0);
}

TEST(battle, mangiasogni_fallisce_se_il_bersaglio_e_sveglio) {
  Combatant a = crea(65, 50, 500, 50, 50, 100, 100, { MV_DREAM_EATER });
  Combatant d = crea(19, 50, 500, 50, 50, 10, 50, { MV_SPLASH });
  Battle bt;
  battleInizia(bt, a, d, 1);
  battleTurno(bt, 0, 0);
  CHECK_EQ(contaEventi(bt, EV_FALLITA, 0), 1);
  CHECK_EQ(bt.lato[1].ps, bt.lato[1].psMax);
}

TEST(battle, il_ko_in_un_colpo_fallisce_su_chi_e_piu_veloce) {
  Combatant lento = crea(19, 50, 500, 50, 50, 10, 50, { MV_HORN_DRILL });
  Combatant veloce = crea(19, 50, 500, 50, 50, 100, 50, { MV_SPLASH });
  Battle bt;
  battleInizia(bt, lento, veloce, 1);
  battleTurno(bt, 0, 0);
  CHECK_EQ(contaEventi(bt, EV_FALLITA, 0), 1);
}

TEST(battle, esplosione_manda_ko_anche_chi_la_usa) {
  Combatant a = crea(74, 50, 500, 100, 50, 100, 100, { MV_EXPLOSION });
  Combatant g = crea(92, 50, 500, 50, 50, 10, 50, { MV_SPLASH });  // spettro: nessun effetto
  Battle bt;
  battleInizia(bt, a, g, 1);
  CHECK(!battleTurno(bt, 0, 0));
  CHECK_EQ(contaEventi(bt, EV_NON_HA_EFFETTO, 1), 1);
  CHECK_EQ(contaEventi(bt, EV_KO, 0), 1);
  CHECK_EQ(bt.vincitore, (uint8_t)1);
  CHECK_EQ(contaEventi(bt, EV_NESSUN_EFFETTO), 0);  // Gastly non agisce più
  const BattleEvento *f = trovaEvento(bt, EV_FINE);
  CHECK(f && f->valore == 1);
}

TEST(battle, iper_raggio_fa_saltare_il_turno_seguente) {
  Combatant a = crea(19, 50, 500, 60, 50, 100, 50, { MV_HYPER_BEAM, MV_TACKLE });
  Combatant d = crea(19, 50, 900, 50, 200, 10, 50, { MV_SPLASH });
  int visti = 0;
  for (uint32_t seme = 1; seme < 20; seme++) {
    Battle bt;
    battleInizia(bt, a, d, seme);
    battleTurno(bt, 0, 0);
    bool colpito = trovaEvento(bt, EV_DANNO, 1) != nullptr;
    battleTurno(bt, 1, 0);
    CHECK_EQ(contaEventi(bt, EV_RICARICA, 0), colpito ? 1 : 0);
    if (colpito) {
      CHECK_EQ(contaEventi(bt, EV_MOSSA, 0), 0);
      CHECK_EQ(bt.lato[0].pp[1], (uint8_t)35);  // Azione non usata
      visti++;
    }
  }
  CHECK(visti > 0);
}

TEST(battle, doppiocalcio_colpisce_due_volte_con_lo_stesso_danno) {
  Combatant a = crea(19, 50, 500, 100, 50, 100, 50, { MV_DOUBLE_KICK });
  Combatant d = crea(19, 50, 900, 50, 100, 10, 50, { MV_SPLASH });
  int visti = 0;
  for (uint32_t seme = 1; seme < 10; seme++) {
    Battle bt;
    battleInizia(bt, a, d, seme);
    battleTurno(bt, 0, 0);
    const BattleEvento *c = trovaEvento(bt, EV_COLPI, 0), *dn = trovaEvento(bt, EV_DANNO, 1);
    if (!dn) continue;
    CHECK(c && c->valore == 2);
    CHECK_EQ(dn->valore % 2, 0);
    visti++;
  }
  CHECK(visti > 0);
}

TEST(battle, comete_non_manca_mai) {
  Combatant a = crea(19, 5, 999, 10, 999, 100, 999, { MV_SWIFT });
  Combatant d = crea(19, 5, 999, 10, 999, 10, 999, { MV_SPLASH });
  Battle bt;
  battleInizia(bt, a, d, 77);
  bt.lato[1].stadio[STAT_ELUSIONE] = 6;
  for (int t = 0; t < 20; t++) {
    battleTurno(bt, 0, 0);
    CHECK_EQ(contaEventi(bt, EV_MANCATA), 0);
  }
}

// ---------- battaglia intera e determinismo ----------

// registra tutti gli eventi di una battaglia (al massimo `turni` turni)
static std::vector<uint32_t> registra(const Combatant &a, const Combatant &b, uint32_t seme,
                                      int turni, uint8_t *vincitore) {
  std::vector<uint32_t> log;
  Battle bt;
  battleInizia(bt, a, b, seme);
  for (int t = 0; t < turni; t++) {
    bool continua = battleTurno(bt, (uint8_t)(t % 4), (uint8_t)((t + 1) % 4));
    for (int i = 0; i < bt.nEventi; i++) {
      const BattleEvento &e = bt.eventi[i];
      log.push_back(((uint32_t)e.tipo << 24) | ((uint32_t)e.lato << 16) | e.valore);
      log.push_back(e.mossa);
    }
    if (!continua) break;
  }
  *vincitore = bt.vincitore;
  return log;
}

TEST(battle, stesso_seme_stessa_battaglia) {
  Combatant a, b;
  battleCombattente(a, 6, 50, 150, 100, 90, 110, 95, nullptr);   // Charizard
  battleCombattente(b, 9, 50, 155, 95, 110, 90, 100, nullptr);   // Blastoise
  uint8_t v1, v2;
  std::vector<uint32_t> l1 = registra(a, b, 42, 300, &v1);
  std::vector<uint32_t> l2 = registra(a, b, 42, 300, &v2);
  CHECK(l1 == l2);
  CHECK_EQ(v1, v2);
  CHECK(v1 != BATTLE_IN_CORSO);
  // con un altro seme la storia cambia
  bool diversa = false;
  for (uint32_t s = 43; s < 53 && !diversa; s++) {
    uint8_t v;
    diversa = registra(a, b, s, 300, &v) != l1;
  }
  CHECK(diversa);
}

TEST(battle, la_battaglia_finisce_con_un_vincitore_e_l_evento_fine) {
  Combatant a, b;
  battleCombattente(a, 25, 30, 70, 50, 40, 80, 50, nullptr);  // Pikachu
  battleCombattente(b, 7, 30, 75, 45, 60, 40, 50, nullptr);   // Squirtle
  Battle bt;
  battleInizia(bt, a, b, 2024);
  int turni = 0;
  while (battleTurno(bt, 0, 0)) turni++;
  CHECK(turni < BATTLE_MAX_TURNI);
  CHECK(bt.vincitore == 0 || bt.vincitore == 1 || bt.vincitore == BATTLE_PAREGGIO);
  CHECK_EQ(bt.eventi[bt.nEventi - 1].tipo, (uint8_t)EV_FINE);
  CHECK(!battleTurno(bt, 0, 0));
  CHECK_EQ(bt.nEventi, (uint8_t)0);
}

TEST(battle, senza_danni_finisce_in_pareggio_al_limite_dei_turni) {
  // due Gastly: finiti i PP di Splash, Scontro (normale) non ha effetto sugli spettri
  Combatant a = crea(92, 5, 50, 10, 10, 10, 10, { MV_SPLASH });
  Combatant b = crea(92, 5, 50, 10, 10, 10, 10, { MV_SPLASH });
  Battle bt;
  battleInizia(bt, a, b, 1);
  int turni = 0;
  while (battleTurno(bt, 0, 0)) turni++;
  CHECK_EQ(bt.turno, (uint16_t)BATTLE_MAX_TURNI);
  CHECK_EQ(bt.vincitore, (uint8_t)BATTLE_PAREGGIO);
}

TEST(battle, il_seme_zero_non_blocca_l_rng) {
  BattleRng r;
  r.seme(0);
  uint32_t a = r.prossimo(), b = r.prossimo();
  CHECK(a != 0);
  CHECK(a != b);
}

// ---------- ordine di turno ----------

TEST(battle, senza_priorita_agisce_prima_il_piu_veloce) {
  Combatant lento = crea(19, 50, 500, 50, 50, 60, 50, { MV_TACKLE });
  Combatant veloce = crea(19, 50, 500, 50, 50, 61, 50, { MV_TACKLE });
  for (uint32_t seme = 1; seme < 30; seme++) {
    Battle bt;
    battleInizia(bt, lento, veloce, seme);
    battleTurno(bt, 0, 0);
    CHECK_EQ(bt.eventi[0].lato, (uint8_t)1);
  }
}

TEST(battle, a_parita_di_velocita_decide_la_moneta) {
  Combatant a = crea(19, 50, 500, 50, 50, 60, 50, { MV_TACKLE });
  int primo[2] = { 0, 0 };
  for (uint32_t seme = 1; seme < 60; seme++) {
    Battle bt;
    battleInizia(bt, a, a, seme);
    battleTurno(bt, 0, 0);
    primo[bt.eventi[0].lato]++;
  }
  CHECK(primo[0] > 10);
  CHECK(primo[1] > 10);
}

TEST(battle, la_paralisi_ferma_un_turno_su_quattro) {
  Combatant a = crea(19, 50, 999, 10, 999, 100, 999, { MV_SPLASH });
  Combatant b = crea(19, 50, 999, 10, 999, 30, 999, { MV_SPLASH });
  Battle bt;
  battleInizia(bt, a, b, 3);
  bt.lato[0].stato = ST_PARALISI;
  int fermo = 0, turni = 0;
  for (; turni < 150; turni++) {
    battleTurno(bt, 0, 0);
    int p = contaEventi(bt, EV_PARALIZZATO, 0);
    CHECK_EQ(p + contaEventi(bt, EV_MOSSA, 0), 1);  // o resta fermo o agisce
    fermo += p;
  }
  CHECK_RANGE(fermo, 20, 60);  // atteso ~37
}

// ---------- stati ----------

TEST(battle, spora_addormenta_e_chi_dorme_non_agisce) {
  Combatant a = crea(46, 50, 999, 10, 999, 100, 999, { MV_SPORE, MV_SPLASH });
  Combatant b = crea(19, 50, 999, 10, 999, 10, 999, { MV_TACKLE });
  int addormentati = 0;
  for (uint32_t seme = 1; seme < 20; seme++) {
    Battle bt;
    battleInizia(bt, a, b, seme);
    battleTurno(bt, 0, 0);
    if (!contaEventi(bt, EV_STATO, 1)) continue;  // manca 1 volta su 256
    addormentati++;
    CHECK_EQ(contaEventi(bt, EV_MOSSA, 1), 0);
    CHECK(bt.lato[1].turniSonno <= 6);  // da 1 a 7, e uno è già passato
    // con 1 turno si sveglia subito, ma il risveglio costa l'azione
    CHECK_EQ(contaEventi(bt, EV_DORME, 1) + contaEventi(bt, EV_SVEGLIA, 1), 1);
    // si sveglia entro 7 turni, e il risveglio costa il turno
    int t = 1;
    while (bt.lato[1].stato == ST_SONNO && t < 10) {
      battleTurno(bt, 1, 0);
      CHECK_EQ(contaEventi(bt, EV_MOSSA, 1), 0);
      t++;
    }
    CHECK(t <= 7);
    CHECK_EQ(bt.lato[1].stato, (uint8_t)ST_NESSUNO);
    battleTurno(bt, 1, 0);
    CHECK_EQ(contaEventi(bt, EV_MOSSA, 1), 1);
  }
  CHECK(addormentati > 15);
}

TEST(battle, sonnifero_addormenta_e_non_c_e_doppio_sonno) {
  Combatant a = crea(1, 50, 999, 10, 999, 100, 999, { MV_SLEEP_POWDER });
  Combatant b = crea(19, 50, 999, 10, 999, 10, 999, { MV_SPLASH });
  int visti = 0;
  for (uint32_t seme = 1; seme < 30 && !visti; seme++) {
    Battle bt;
    battleInizia(bt, a, b, seme);
    battleTurno(bt, 0, 0);
    if (bt.lato[1].stato != ST_SONNO) continue;
    visti++;
    // finché dorme, un altro Sonnifero che va a segno fallisce
    for (int t = 0; t < 5 && bt.lato[1].stato == ST_SONNO; t++) {
      battleTurno(bt, 0, 0);
      CHECK_EQ(contaEventi(bt, EV_STATO, 1), 0);
    }
  }
  CHECK(visti);
}

TEST(battle, riposo_cura_tutto_e_dorme_due_turni) {
  Combatant a = crea(143, 50, 400, 10, 999, 100, 999, { MV_REST, MV_SPLASH });
  Combatant b = crea(19, 50, 999, 10, 999, 10, 999, { MV_SPLASH });
  Battle bt;
  battleInizia(bt, a, b, 1);
  battleTurno(bt, 0, 0);
  CHECK_EQ(contaEventi(bt, EV_FALLITA, 0), 1);  // PS pieni: fallisce
  bt.lato[0].ps = 100;
  bt.lato[0].stato = ST_TOSSICO;
  bt.lato[0].tossico = 4;
  battleTurno(bt, 0, 0);
  CHECK_EQ(bt.lato[0].ps, (uint16_t)400);
  CHECK_EQ(bt.lato[0].stato, (uint8_t)ST_SONNO);
  battleTurno(bt, 1, 0);
  CHECK_EQ(contaEventi(bt, EV_DORME, 0), 1);
  battleTurno(bt, 1, 0);
  CHECK_EQ(contaEventi(bt, EV_SVEGLIA, 0), 1);
  battleTurno(bt, 1, 0);
  CHECK_EQ(contaEventi(bt, EV_MOSSA, 0), 1);
}

TEST(battle, veleno_toglie_un_sedicesimo_e_tossina_cresce) {
  Combatant a = crea(19, 50, 320, 10, 999, 100, 999, { MV_SPLASH });
  Combatant b = crea(19, 50, 320, 10, 999, 10, 999, { MV_SPLASH });
  Battle bt;
  battleInizia(bt, a, b, 1);
  bt.lato[0].stato = ST_VELENO;
  bt.lato[1].stato = ST_TOSSICO;
  bt.lato[1].tossico = 1;
  for (int t = 1; t <= 4; t++) {
    battleTurno(bt, 0, 0);
    const BattleEvento *v0 = trovaEvento(bt, EV_VELENO, 0), *v1 = trovaEvento(bt, EV_VELENO, 1);
    CHECK(v0 && v0->valore == 20);
    CHECK(v1 && v1->valore == 20 * t);
  }
}

TEST(battle, la_scottatura_toglie_un_sedicesimo) {
  Combatant a = crea(19, 50, 160, 10, 999, 100, 999, { MV_SPLASH });
  Battle bt;
  battleInizia(bt, a, a, 1);
  bt.lato[0].stato = ST_SCOTTATURA;
  battleTurno(bt, 0, 0);
  const BattleEvento *e = trovaEvento(bt, EV_SCOTTATURA, 0);
  CHECK(e && e->valore == 10);
  CHECK_EQ(bt.lato[0].ps, (uint16_t)150);
}

TEST(battle, immunita_di_tipo_agli_stati) {
  // Onda Tuono non tocca i terra, Tossina non tocca i veleno
  Combatant p = crea(25, 50, 999, 10, 999, 100, 999, { MV_THUNDER_WAVE, MV_TOXIC });
  Combatant terra = crea(50, 50, 999, 10, 999, 10, 999, { MV_SPLASH });
  Combatant veleno = crea(23, 50, 999, 10, 999, 10, 999, { MV_SPLASH });
  Battle bt;
  battleInizia(bt, p, terra, 1);
  for (int t = 0; t < 5; t++) battleTurno(bt, 0, 0);
  CHECK_EQ(bt.lato[1].stato, (uint8_t)ST_NESSUNO);
  battleInizia(bt, p, veleno, 1);
  for (int t = 0; t < 5; t++) battleTurno(bt, 1, 0);
  CHECK_EQ(bt.lato[1].stato, (uint8_t)ST_NESSUNO);
  // Corpo a corpo non paralizza i tipi normale, come in Gen 1
  Combatant s = crea(143, 50, 999, 50, 999, 100, 50, { MV_BODY_SLAM });
  Combatant n = crea(19, 50, 999, 10, 999, 10, 999, { MV_SPLASH });
  battleInizia(bt, s, n, 4);
  for (int t = 0; t < 40; t++) {
    battleTurno(bt, 0, 0);
    bt.lato[1].ps = bt.lato[1].psMax;
  }
  CHECK_EQ(bt.lato[1].stato, (uint8_t)ST_NESSUNO);
}

TEST(battle, congelato_non_agisce_e_il_fuoco_lo_scongela) {
  Combatant a = crea(6, 50, 999, 10, 999, 100, 999, { MV_SPLASH, MV_FLAMETHROWER });
  Combatant b = crea(19, 50, 999, 10, 999, 10, 999, { MV_TACKLE });
  Battle bt;
  battleInizia(bt, a, b, 1);
  bt.lato[1].stato = ST_CONGELAMENTO;
  for (int t = 0; t < 10; t++) {  // in Gen 1 non passa da solo
    battleTurno(bt, 0, 0);
    CHECK_EQ(contaEventi(bt, EV_CONGELATO, 1), 1);
    CHECK_EQ(contaEventi(bt, EV_MOSSA, 1), 0);
  }
  for (int t = 0; t < 5 && bt.lato[1].stato == ST_CONGELAMENTO; t++) battleTurno(bt, 1, 0);
  CHECK_EQ(bt.lato[1].stato, (uint8_t)ST_NESSUNO);
}

TEST(battle, la_confusione_fa_colpire_se_stessi_e_poi_passa) {
  Combatant a = crea(93, 50, 999, 10, 999, 100, 999, { MV_CONFUSE_RAY, MV_SPLASH });
  Combatant b = crea(19, 50, 999, 100, 100, 10, 999, { MV_SPLASH });
  int colpi = 0, passata = 0;
  for (uint32_t seme = 1; seme < 30; seme++) {
    Battle bt;
    battleInizia(bt, a, b, seme);
    battleTurno(bt, 0, 0);
    if (!bt.lato[1].confusione) continue;
    for (int t = 0; t < 6; t++) {
      battleTurno(bt, 1, 0);
      const BattleEvento *e = trovaEvento(bt, EV_SI_COLPISCE, 1);
      if (e) {
        colpi++;
        CHECK(e->valore > 0);
        CHECK_EQ(contaEventi(bt, EV_MOSSA, 1), 0);
      }
      passata += contaEventi(bt, EV_NON_PIU_CONFUSO, 1);
    }
    CHECK_EQ(bt.lato[1].confusione, (uint8_t)0);  // al massimo 5 turni
    // già confuso: un altro Stordiraggio fallisce
    bt.lato[1].confusione = 3;
    battleTurno(bt, 0, 0);
    CHECK_EQ(contaEventi(bt, EV_FALLITA, 0) + contaEventi(bt, EV_MANCATA, 0), 1);
  }
  CHECK(colpi > 5);
  CHECK(passata > 5);
}

TEST(battle, il_tentennamento_ferma_solo_chi_deve_ancora_agire) {
  Combatant veloce = crea(19, 50, 999, 60, 999, 100, 999, { MV_STOMP });
  Combatant lento = crea(19, 50, 999, 60, 999, 10, 999, { MV_STOMP });
  int tentennati = 0;
  Battle bt;
  battleInizia(bt, veloce, lento, 21);
  for (int t = 0; t < 120; t++) {
    battleTurno(bt, 0, 0);
    tentennati += contaEventi(bt, EV_TENTENNA, 1);
    CHECK_EQ(contaEventi(bt, EV_TENTENNA, 0), 0);  // il veloce ha già agito
    bt.lato[0].ps = bt.lato[0].psMax;
    bt.lato[1].ps = bt.lato[1].psMax;
    for (int k = 0; k < 2; k++) bt.lato[k].pp[0] = 20;
  }
  CHECK_RANGE(tentennati, 15, 60);  // atteso ~36
}

TEST(battle, fossa_e_volo_sono_invulnerabili_mentre_caricano) {
  for (uint8_t mossa : { (uint8_t)MV_DIG, (uint8_t)MV_FLY }) {
    Combatant a = crea(51, 50, 999, 100, 999, 100, 999, { mossa });
    Combatant b = crea(25, 50, 999, 100, 999, 10, 999, { MV_SWIFT });
    Battle bt;
    battleInizia(bt, a, b, 8);
    battleTurno(bt, 0, 0);
    CHECK_EQ(contaEventi(bt, EV_CARICA, 0), 1);
    CHECK(bt.lato[0].invulnerabile);
    CHECK_EQ(contaEventi(bt, EV_MANCATA, 1), 1);  // nemmeno Comete lo raggiunge
    CHECK_EQ(bt.lato[0].ps, bt.lato[0].psMax);
    CHECK(!battleDeveScegliere(bt.lato[0]));
    uint8_t pp = bt.lato[0].pp[0];
    battleTurno(bt, 0, 0);
    CHECK(!bt.lato[0].invulnerabile);
    CHECK_EQ(bt.lato[0].pp[0], pp);  // il secondo turno non consuma PP
    const BattleEvento *m = trovaEvento(bt, EV_MOSSA, 0);
    CHECK(m && m->mossa == mossa);
    CHECK_EQ(contaEventi(bt, EV_CARICA, 0), 0);
    CHECK(bt.lato[0].ps < bt.lato[0].psMax);  // ora Comete lo prende
  }
}

TEST(battle, solarraggio_carica_ma_resta_colpibile) {
  Combatant a = crea(1, 50, 999, 100, 999, 100, 999, { MV_SOLAR_BEAM });
  Combatant b = crea(19, 50, 999, 100, 999, 10, 999, { MV_SWIFT });
  Battle bt;
  battleInizia(bt, a, b, 8);
  battleTurno(bt, 0, 0);
  CHECK_EQ(contaEventi(bt, EV_CARICA, 0), 1);
  CHECK(!bt.lato[0].invulnerabile);
  CHECK(bt.lato[0].ps < bt.lato[0].psMax);
}

TEST(battle, trasformazione_copia_e_poi_si_ripristina) {
  Combatant d = crea(132, 50, 200, 50, 50, 100, 50, { MV_TRANSFORM });
  Combatant c = crea(6, 60, 300, 120, 110, 90, 130, { MV_FLAMETHROWER, MV_SLASH, MV_FLY });
  Battle bt;
  battleInizia(bt, d, c, 5);
  bt.lato[1].stadio[STAT_ATTACCO] = 2;
  battleTurno(bt, 0, 0);
  const Combatant &t = bt.lato[0];
  CHECK_EQ(contaEventi(bt, EV_TRASFORMA, 0), 1);
  CHECK_EQ(t.dex, (int16_t)6);
  CHECK_EQ(t.tipo1, (uint8_t)T_FUOCO);
  CHECK_EQ(t.tipo2, (uint8_t)T_VOLANTE);
  CHECK_EQ(t.stat[STAT_ATTACCO], (uint16_t)120);
  CHECK_EQ(t.stat[STAT_SPECIALE], (uint16_t)130);
  CHECK_EQ(t.stadio[STAT_ATTACCO], (int8_t)2);
  CHECK_EQ(t.psMax, (uint16_t)200);  // i PS restano i suoi
  CHECK_EQ(t.mosse[0], (uint8_t)MV_FLAMETHROWER);
  CHECK_EQ(t.pp[1], (uint8_t)5);
  CHECK_EQ(t.mosse[3], (uint8_t)0);
  CHECK_EQ(t.pp[3], (uint8_t)0);
  battleTurno(bt, 0, 0);  // usa le mosse copiate
  const BattleEvento *m = trovaEvento(bt, EV_MOSSA, 0);
  CHECK(m && m->mossa == MV_FLAMETHROWER);
  Combatant r = bt.lato[0];
  battleRipristina(r);
  CHECK_EQ(r.dex, (int16_t)132);
  CHECK_EQ(r.tipo1, (uint8_t)T_NORMALE);
  CHECK_EQ(r.stat[STAT_ATTACCO], (uint16_t)50);
  CHECK_EQ(r.mosse[0], (uint8_t)MV_TRANSFORM);
  CHECK_EQ(r.pp[0], MOVE_TBL[MV_TRANSFORM].pp);
  CHECK(!r.trasformato);
}

TEST(battle, stadi_su_se_stessi_e_sul_bersaglio_con_la_nebbia) {
  Combatant a = crea(19, 50, 999, 50, 999, 100, 999, { MV_SWORDS_DANCE, MV_GROWL });
  Combatant b = crea(19, 50, 999, 50, 999, 10, 999, { MV_SPLASH, MV_MIST });
  Battle bt;
  battleInizia(bt, a, b, 3);
  battleTurno(bt, 0, 0);
  const BattleEvento *e = trovaEvento(bt, EV_STADIO, 0);
  CHECK(e && (e->valore >> 8) == STAT_ATTACCO && (int8_t)(e->valore & 0xFF) == 2);
  CHECK_EQ(bt.lato[0].stadio[STAT_ATTACCO], (int8_t)2);
  for (int t = 0; t < 3; t++) battleTurno(bt, 0, 0);
  CHECK_EQ(bt.lato[0].stadio[STAT_ATTACCO], (int8_t)6);
  battleTurno(bt, 0, 0);  // già a +6
  CHECK_EQ(contaEventi(bt, EV_NESSUN_EFFETTO, 0), 1);
  bool calato = false;
  for (int t = 0; t < 3 && !calato; t++) {
    battleTurno(bt, 1, 0);
    calato = bt.lato[1].stadio[STAT_ATTACCO] == -1;
  }
  CHECK(calato);
  battleTurno(bt, 1, 1);  // Ruggito di a (più veloce), poi Nebbia di b
  CHECK(bt.lato[1].nebbia);
  int8_t prima = bt.lato[1].stadio[STAT_ATTACCO];
  battleTurno(bt, 1, 0);
  CHECK(contaEventi(bt, EV_PROTETTO, 1) + contaEventi(bt, EV_MANCATA, 0) == 1);
  CHECK_EQ(bt.lato[1].stadio[STAT_ATTACCO], prima);
}

TEST(battle, parassiseme_passa_un_sedicesimo_e_non_tocca_l_erba) {
  Combatant a = crea(1, 50, 320, 10, 999, 100, 999, { MV_LEECH_SEED });
  Combatant b = crea(19, 50, 320, 10, 999, 10, 999, { MV_SPLASH });
  Battle bt;
  bool seminato = false;
  for (uint32_t seme = 1; seme < 10 && !seminato; seme++) {
    battleInizia(bt, a, b, seme);
    bt.lato[0].ps = 100;
    battleTurno(bt, 0, 0);
    seminato = bt.lato[1].parassiseme;
  }
  CHECK(seminato);
  const BattleEvento *e = trovaEvento(bt, EV_PARASSISEME, 1);
  CHECK(e && e->valore == 20);
  CHECK_EQ(bt.lato[0].ps, (uint16_t)120);
  CHECK_EQ(bt.lato[1].ps, (uint16_t)300);
  battleInizia(bt, a, a, 1);  // contro un erba
  battleTurno(bt, 0, 0);
  CHECK(!bt.lato[1].parassiseme && !bt.lato[0].parassiseme);
}

TEST(battle, avvolgibotta_intrappola_con_lo_stesso_danno) {
  Combatant a = crea(23, 50, 999, 100, 999, 100, 999, { MV_WRAP });
  Combatant b = crea(19, 50, 999, 50, 100, 10, 999, { MV_TACKLE });
  int visti = 0;
  for (uint32_t seme = 1; seme < 20; seme++) {
    Battle bt;
    battleInizia(bt, a, b, seme);
    battleTurno(bt, 0, 0);
    const BattleEvento *d = trovaEvento(bt, EV_DANNO, 1);
    if (!d) continue;
    visti++;
    uint16_t danno = d->valore;
    CHECK_EQ(contaEventi(bt, EV_INTRAPPOLATO, 1), 1);
    int turni = 1;
    while (bt.lato[1].intrappolato && turni < 8) {
      battleTurno(bt, 0, 0);
      const BattleEvento *e = trovaEvento(bt, EV_DANNO, 1);
      CHECK(e && e->valore == danno);
      // come in Gen 1, se la stretta finisce prima del suo turno agisce subito
      CHECK_EQ(contaEventi(bt, EV_MOSSA, 1), bt.lato[1].intrappolato ? 0 : 1);
      turni++;
    }
    CHECK_RANGE(turni, 2, 5);
    CHECK_EQ(bt.lato[0].pp[0], (uint8_t)(MOVE_TBL[MV_WRAP].pp - 1));  // un PP solo
    battleTurno(bt, 0, 0);
    CHECK(contaEventi(bt, EV_MOSSA, 1) == 1 || contaEventi(bt, EV_INTRAPPOLATO, 1) == 1);
  }
  CHECK(visti > 5);
}

TEST(battle, colpo_si_ripete_e_poi_confonde) {
  Combatant a = crea(56, 50, 999, 50, 999, 100, 999, { MV_THRASH, MV_SPLASH });
  Combatant b = crea(19, 50, 999, 50, 999, 10, 999, { MV_SPLASH });
  for (uint32_t seme = 1; seme < 10; seme++) {
    Battle bt;
    battleInizia(bt, a, b, seme);
    int usi = 0;
    for (int t = 0; t < 5 && (usi == 0 || bt.lato[0].bloccata); t++) {
      battleTurno(bt, 1, 0);  // sceglie Splash, ma Colpo continua
      const BattleEvento *m = trovaEvento(bt, EV_MOSSA, 0);
      if (t == 0) {
        battleTurno(bt, 0, 0);
        m = trovaEvento(bt, EV_MOSSA, 0);
      }
      if (m && m->mossa == MV_THRASH) usi++;
    }
    CHECK_RANGE(usi, 2, 3);
    CHECK(bt.lato[0].confusione > 0);
    CHECK_EQ(bt.lato[0].pp[0], (uint8_t)(MOVE_TBL[MV_THRASH].pp - 1));
  }
}

TEST(battle, nube_azzera_stadi_e_stati_volatili) {
  Combatant a = crea(19, 50, 999, 50, 999, 100, 999, { MV_HAZE });
  Combatant b = crea(19, 50, 999, 50, 999, 10, 999, { MV_SPLASH });
  Battle bt;
  battleInizia(bt, a, b, 1);
  bt.lato[0].stadio[STAT_DIFESA] = -3;
  bt.lato[0].stato = ST_TOSSICO;
  bt.lato[0].tossico = 3;
  bt.lato[1].stadio[STAT_ATTACCO] = 4;
  bt.lato[1].riflesso = bt.lato[1].nebbia = true;
  bt.lato[1].confusione = 3;
  bt.lato[1].stato = ST_PARALISI;
  battleTurno(bt, 0, 0);
  CHECK_EQ(contaEventi(bt, EV_AZZERA, 0), 1);
  CHECK_EQ(bt.lato[0].stadio[STAT_DIFESA], (int8_t)0);
  CHECK_EQ(bt.lato[1].stadio[STAT_ATTACCO], (int8_t)0);
  CHECK(!bt.lato[1].riflesso && !bt.lato[1].nebbia);
  CHECK_EQ(bt.lato[1].confusione, (uint8_t)0);
  CHECK_EQ(bt.lato[1].stato, (uint8_t)ST_NESSUNO);  // il bersaglio guarisce
  CHECK_EQ(bt.lato[0].stato, (uint8_t)ST_VELENO);   // Tossina torna veleno normale
}

TEST(battle, ira_alza_l_attacco_a_ogni_colpo_subito) {
  Combatant a = crea(19, 50, 999, 50, 999, 100, 999, { MV_RAGE, MV_SPLASH });
  Combatant b = crea(19, 50, 999, 50, 999, 10, 999, { MV_SWIFT });
  Battle bt;
  battleInizia(bt, a, b, 2);
  battleTurno(bt, 0, 0);
  CHECK_EQ(contaEventi(bt, EV_IRA, 0), 1);
  CHECK_EQ(bt.lato[0].stadio[STAT_ATTACCO], (int8_t)1);
  battleTurno(bt, 1, 0);  // resta bloccato in Ira
  const BattleEvento *m = trovaEvento(bt, EV_MOSSA, 0);
  CHECK(m && m->mossa == MV_RAGE);
  CHECK_EQ(bt.lato[0].stadio[STAT_ATTACCO], (int8_t)2);
}

TEST(battle, focalenergia_quadruplica_i_brutti_colpi) {
  BattleRng r;
  r.seme(9);
  int n = 0, nf = 0;
  for (int i = 0; i < 20000; i++) n += battleTiraBruttoColpo(r, 50, false, false);
  for (int i = 0; i < 20000; i++) nf += battleTiraBruttoColpo(r, 50, false, true);
  CHECK_RANGE(n, 1500, 2400);    // 25/256 ~ 9,8%
  CHECK_RANGE(nf, 7300, 8300);   // 100/256 ~ 39%
}

// ---------- fine garantita, coda della narrazione ----------

TEST(battle, con_sole_mosse_di_stato_finisce_entro_il_limite) {
  Combatant a = crea(19, 20, 60, 40, 40, 50, 40, { MV_GROWL, MV_TAIL_WHIP, MV_HARDEN, MV_RECOVER });
  Combatant b = crea(16, 20, 60, 40, 40, 45, 40, { MV_SAND_ATTACK, MV_LEER, MV_MIST, MV_REST });
  for (uint32_t seme = 1; seme <= 20; seme++) {
    Battle bt;
    battleInizia(bt, a, b, seme);
    int turni = 0;
    while (battleTurno(bt, (uint8_t)(turni % 4), (uint8_t)((turni + 1) % 4))) turni++;
    CHECK(bt.turno <= BATTLE_MAX_TURNI);
    // finiti i PP arriva Scontro, che manda KO qualcuno ben prima del limite
    CHECK(bt.vincitore == 0 || bt.vincitore == 1);
  }
}

// battaglia tra due specie a caso con le loro mosse, scelte dall'IA
static void battagliaCasuale(uint32_t seme, bool difficile, Battle &bt, int *maxEventi) {
  BattleRng r;
  r.seme(seme);
  Combatant a, b;
  uint8_t la = (uint8_t)(5 + r.sotto(96)), lb = (uint8_t)(5 + r.sotto(96));
  battleCombattente(a, (int16_t)(1 + r.sotto(151)), la, 20 + la * 3, 10 + la * 2, 10 + la * 2,
                    10 + la * 2, 10 + la * 2, nullptr);
  battleCombattente(b, (int16_t)(1 + r.sotto(151)), lb, 20 + lb * 3, 10 + lb * 2, 10 + lb * 2,
                    10 + lb * 2, 10 + lb * 2, nullptr);
  battleInizia(bt, a, b, seme);
  for (;;) {
    uint8_t sa = battleSceltaIA(bt, 0, difficile), sb = battleSceltaIA(bt, 1, !difficile);
    bool continua = battleTurno(bt, sa, sb);
    if (bt.nEventi > *maxEventi) *maxEventi = bt.nEventi;
    // la UI svuota la coda una riga alla volta: deve trovare tutte le righe del turno
    for (int i = 0; i < bt.nEventi; i++) {
      BattleEvento e = {};
      CHECK(bt.coda.prendi(e));
      CHECK(e.tipo == bt.eventi[i].tipo && e.lato == bt.eventi[i].lato &&
            e.valore == bt.eventi[i].valore);
    }
    CHECK(bt.coda.vuota());
    if (!continua) break;
  }
}

TEST(battle, ogni_battaglia_finisce_e_nessuna_riga_si_perde) {
  int maxEventi = 0;
  for (uint32_t seme = 1; seme <= 300; seme++) {
    Battle bt;
    battagliaCasuale(seme, seme & 1, bt, &maxEventi);
    CHECK(bt.vincitore != BATTLE_IN_CORSO);
    CHECK(bt.turno <= BATTLE_MAX_TURNI);
    CHECK_EQ(bt.eventiPersi, (uint16_t)0);
    CHECK_EQ(bt.coda.persi, (uint16_t)0);
  }
  CHECK(maxEventi < BATTLE_MAX_EVENTI);
}

TEST(battle, la_coda_tiene_due_turni_senza_perdere_righe) {
  Combatant a = crea(19, 50, 999, 50, 999, 100, 999, { MV_TACKLE });
  Battle bt;
  battleInizia(bt, a, a, 6);
  battleTurno(bt, 0, 0);
  uint8_t n1 = bt.nEventi;
  battleTurno(bt, 0, 0);  // la UI è rimasta indietro di un turno
  CHECK_EQ(bt.coda.n, (uint8_t)(n1 + bt.nEventi));
  CHECK_EQ(bt.coda.persi, (uint16_t)0);
  BattleEvento e = {};
  CHECK(bt.coda.prendi(e));
  CHECK_EQ(e.tipo, (uint8_t)EV_MOSSA);
  BattleCoda c;
  for (int i = 0; i < BATTLE_CODA + 3; i++) c.metti(e);
  CHECK_EQ(c.n, (uint8_t)BATTLE_CODA);
  CHECK_EQ(c.persi, (uint16_t)3);
}

// ---------- statistiche dal pet ----------

static Pet petDi(int16_t dex, uint16_t livello, uint8_t gene, uint8_t tr) {
  Pet p;
  p.speciesId = dex;
  p.ageMinutes = (uint32_t)(livello - 1) * MINUTES_PER_LEVEL;
  p.geneAtk = p.geneDef = p.geneSpe = gene;
  p.trAtk = p.trDef = p.trSpe = tr;
  return p;
}

TEST(battle, pet_al_massimo_ha_le_statistiche_massime_di_gen_1) {
  // Tauros al 100 con DV 15 e stat exp al massimo: 353 / 298 / 288 / 318 / 238
  Combatant c;
  CHECK(combattenteDaPet(c, petDi(128, 100, 110, 100)));
  CHECK_EQ(c.livello, (uint8_t)100);
  CHECK_EQ(c.psMax, (uint16_t)353);
  CHECK_EQ(c.stat[STAT_ATTACCO], (uint16_t)298);
  CHECK_EQ(c.stat[STAT_DIFESA], (uint16_t)288);
  CHECK_EQ(c.stat[STAT_VELOCITA], (uint16_t)318);
  CHECK_EQ(c.stat[STAT_SPECIALE], (uint16_t)238);
  CHECK_EQ(c.ps, c.psMax);
}

TEST(battle, pet_a_campione_con_la_formula_di_gen_1) {
  // Tauros al 50, geni 100 -> DV 7 (PS: bit bassi 1111 = 15), allenamento 50 -> bonus 31
  // PS ((75+15)*2+31)*50/100 + 50 + 10 = 165; ATK ((100+7)*2+31)*50/100 + 5 = 127
  Combatant c;
  combattenteDaPet(c, petDi(128, 50, 100, 50));
  CHECK_EQ(c.psMax, (uint16_t)165);
  CHECK_EQ(c.stat[STAT_ATTACCO], (uint16_t)127);
  // geni minimi e niente allenamento: DV 0; Pikachu al 10
  // PS (35*2)*10/100 + 20 = 27; VEL (90*2)*10/100 + 5 = 23
  combattenteDaPet(c, petDi(25, 10, 90, 0));
  CHECK_EQ(c.psMax, (uint16_t)(DEX_TBL[25].bHp * 2 * 10 / 100 + 20));
  CHECK_EQ(c.stat[STAT_VELOCITA], (uint16_t)(DEX_TBL[25].bSpe * 2 * 10 / 100 + 5));
  CHECK_EQ(c.tipo1, (uint8_t)T_ELETTRO);
  CHECK_EQ(battleStatGen1(90, 0, 0, 10, false), (uint16_t)23);
}

TEST(battle, pet_speciale_dalla_media_di_attacco_e_difesa) {
  Pet p = petDi(128, 100, 100, 0);
  p.geneAtk = 110;  // DV 15
  p.geneDef = 90;   // DV 0: media 100 -> DV 7
  p.trAtk = 100;
  p.trDef = 0;      // media 50 -> bonus 31
  p.trSpe = 100;
  Combatant c;
  combattenteDaPet(c, p);
  CHECK_EQ(c.stat[STAT_SPECIALE], (uint16_t)((70 + 7) * 2 + 31 + 5));
  CHECK_EQ(c.stat[STAT_ATTACCO], (uint16_t)((100 + 15) * 2 + 63 + 5));
  CHECK_EQ(c.stat[STAT_DIFESA], (uint16_t)(95 * 2 + 0 + 5));
}

TEST(battle, pet_oltre_il_cento_combatte_al_cento_con_le_ultime_mosse) {
  Combatant c;
  combattenteDaPet(c, petDi(6, 999, 100, 0));
  CHECK_EQ(c.livello, (uint8_t)100);
  uint8_t attese[BATTLE_MOSSE];
  battleMosseApprese(6, 100, attese);
  for (int i = 0; i < BATTLE_MOSSE; i++) CHECK_EQ(c.mosse[i], attese[i]);
  combattenteDaPet(c, petDi(6, 30, 100, 0));
  battleMosseApprese(6, 30, attese);
  for (int i = 0; i < BATTLE_MOSSE; i++) CHECK_EQ(c.mosse[i], attese[i]);
  Pet uovo;
  CHECK(!combattenteDaPet(c, uovo));
}

// ---------- IA ----------

TEST(battle, l_ia_non_sceglie_mai_mosse_senza_pp) {
  Combatant a = crea(6, 50, 200, 100, 100, 100, 100,
                     { MV_FLAMETHROWER, MV_SLASH, MV_GROWL, MV_FLY });
  Combatant b = crea(19, 50, 200, 100, 100, 10, 100, { MV_TACKLE });
  for (bool difficile : { false, true }) {
    Battle bt;
    battleInizia(bt, a, b, 13);
    bt.lato[0].pp[0] = 0;  // la migliore per l'IA difficile
    bt.lato[0].pp[3] = 0;
    for (int i = 0; i < 200; i++) {
      uint8_t s = battleSceltaIA(bt, 0, difficile);
      CHECK(s == 1 || s == 2);
    }
    bt.lato[0].pp[1] = bt.lato[0].pp[2] = 0;
    CHECK_EQ(battleSceltaIA(bt, 0, difficile), (uint8_t)0);  // niente PP: Scontro
    CHECK_EQ(battleMossaScelta(bt.lato[0], 0), (uint8_t)MOVE_STRUGGLE);
  }
}

TEST(battle, l_ia_facile_usa_tutte_le_mosse) {
  Combatant a = crea(6, 50, 200, 100, 100, 100, 100,
                     { MV_FLAMETHROWER, MV_SLASH, MV_GROWL, MV_FLY });
  Battle bt;
  battleInizia(bt, a, a, 4);
  int visto[BATTLE_MOSSE] = { 0, 0, 0, 0 };
  for (int i = 0; i < 400; i++) visto[battleSceltaIA(bt, 0, false)]++;
  for (int i = 0; i < BATTLE_MOSSE; i++) CHECK(visto[i] > 50);
}

TEST(battle, l_ia_difficile_sceglie_bene) {
  // contro un acqua: Fulmine superefficace, non Azione né Ruggito
  Combatant p = crea(25, 50, 200, 100, 100, 100, 100, { MV_GROWL, MV_TACKLE, MV_THUNDERBOLT });
  Combatant s = crea(7, 50, 200, 100, 100, 10, 100, { MV_TACKLE });
  Battle bt;
  battleInizia(bt, p, s, 1);
  CHECK_EQ(battleSceltaIA(bt, 0, true), (uint8_t)2);
  // contro un terra Fulmine non ha effetto
  Combatant t = crea(50, 50, 200, 100, 100, 10, 100, { MV_TACKLE });
  battleInizia(bt, p, t, 1);
  CHECK(battleSceltaIA(bt, 0, true) != 2);
  // e un KO sicuro batte tutto
  bt.lato[1].ps = 5;
  CHECK_EQ(battleSceltaIA(bt, 0, true), (uint8_t)1);
  // non riaddormenta chi dorme, e non usa Splash
  Combatant g = crea(1, 50, 200, 100, 100, 100, 100, { MV_SPLASH, MV_SLEEP_POWDER, MV_TACKLE });
  battleInizia(bt, g, s, 1);
  CHECK_EQ(battleSceltaIA(bt, 0, true), (uint8_t)1);
  bt.lato[1].stato = ST_SONNO;
  CHECK_EQ(battleSceltaIA(bt, 0, true), (uint8_t)2);
  // ferito, si cura
  Combatant c = crea(113, 50, 400, 50, 50, 50, 50, { MV_TACKLE, MV_SOFT_BOILED });
  battleInizia(bt, c, s, 1);
  CHECK_EQ(battleSceltaIA(bt, 0, true), (uint8_t)0);
  bt.lato[0].ps = 80;
  CHECK_EQ(battleSceltaIA(bt, 0, true), (uint8_t)1);
}

// ---------- squadre ----------

// impronta FNV-1a di tutti gli eventi (tipo, lato, mossa, valore) e del vincitore di 300
// battaglie 1 contro 1 casuali: il valore atteso è quello del motore prima delle squadre
static uint32_t impronta1contro1() {
  uint32_t h = 2166136261u;
  auto mescola = [&](uint32_t v) {
    for (int k = 0; k < 4; k++) {
      h ^= (v >> (8 * k)) & 0xFF;
      h *= 16777619u;
    }
  };
  for (uint32_t seme = 1; seme <= 300; seme++) {
    BattleRng r;
    r.seme(seme * 7 + 3);
    Combatant a, b;
    uint8_t la = (uint8_t)(5 + r.sotto(96)), lb = (uint8_t)(5 + r.sotto(96));
    battleCombattente(a, (int16_t)(1 + r.sotto(151)), la, 20 + la * 3, 10 + la * 2,
                      10 + la * 2, 10 + la * 2, 10 + la * 2, nullptr);
    battleCombattente(b, (int16_t)(1 + r.sotto(151)), lb, 20 + lb * 3, 10 + lb * 2,
                      10 + lb * 2, 10 + lb * 2, 10 + lb * 2, nullptr);
    Battle bt;
    battleInizia(bt, a, b, seme);
    for (;;) {
      uint8_t sa = battleSceltaIA(bt, 0, seme & 1), sb = battleSceltaIA(bt, 1, !(seme & 1));
      bool continua = battleTurno(bt, sa, sb);
      for (int i = 0; i < bt.nEventi; i++) {
        const BattleEvento &e = bt.eventi[i];
        mescola((uint32_t)e.tipo << 24 | (uint32_t)e.lato << 16 | e.mossa);
        mescola(e.valore);
      }
      if (!continua) break;
    }
    mescola(bt.vincitore);
    mescola(bt.turno);
  }
  return h;
}

TEST(battle, uno_contro_uno_identico_al_motore_di_prima) {
  CHECK_EQ(impronta1contro1(), 3770933631u);
}

static int posizione(const Battle &b, uint8_t tipo, int lato = -1) {
  for (int i = 0; i < b.nEventi; i++)
    if (b.eventi[i].tipo == tipo && (lato < 0 || b.eventi[i].lato == lato)) return i;
  return -1;
}

TEST(battle, la_squadra_parte_dal_primo_e_tronca_oltre_tre) {
  Combatant sq[4] = { tauros({ MV_TACKLE }), chansey({ MV_TACKLE }), alakazam({ MV_TACKLE }),
                      tauros({ MV_TACKLE }) };
  Battle bt;
  battleIniziaSquadre(bt, sq, 4, sq + 1, 0, 1);
  CHECK_EQ(bt.membri[0], (uint8_t)BATTLE_SQUADRA);
  CHECK_EQ(bt.membri[1], (uint8_t)1);
  CHECK_EQ(bt.lato[0].dex, (int16_t)128);
  CHECK_EQ(bt.lato[1].dex, (int16_t)113);
  CHECK_EQ(battleMembro(bt, 0, 2).dex, (int16_t)65);
  CHECK_EQ(battleRestanti(bt, 0), (uint8_t)3);
  CHECK(!battlePuoCambiare(bt, 0, 0));  // già in campo
  CHECK(battlePuoCambiare(bt, 0, 2));
  CHECK(!battlePuoCambiare(bt, 0, 3));  // non esiste
  CHECK(!battlePuoCambiare(bt, 1, 1));
}

TEST(battle, il_cambio_avviene_prima_delle_mosse_e_chi_entra_subisce) {
  // Alakazam è più veloce e usa Attacco Rapido: il cambio di Chansey passa comunque prima
  Combatant noi[2] = { chansey({ MV_SPLASH }), tauros({ MV_TACKLE }) };
  Combatant loro = alakazam({ MV_QUICK_ATTACK });
  Battle bt;
  battleIniziaSquadre(bt, noi, 2, &loro, 1, 7);
  CHECK(battleTurno(bt, BATTLE_CAMBIA + 1, 0));
  int rit = posizione(bt, EV_RITIRO, 0), ent = posizione(bt, EV_ENTRA, 0);
  int mos = posizione(bt, EV_MOSSA, 1);
  CHECK(rit >= 0 && ent == rit + 1 && mos > ent);
  CHECK_EQ(bt.eventi[rit].valore, (uint16_t)(0 << 8 | 113));
  CHECK_EQ(bt.eventi[ent].valore, (uint16_t)(1 << 8 | 128));
  CHECK_EQ(contaEventi(bt, EV_MOSSA, 0), 0);  // chi cambia non agisce
  CHECK_EQ(bt.inCampo[0], (uint8_t)1);
  CHECK_EQ(bt.lato[0].dex, (int16_t)128);
  CHECK(bt.lato[0].ps < bt.lato[0].psMax);              // il colpo l'ha preso Tauros
  CHECK_EQ(battleMembro(bt, 0, 0).ps, battleMembro(bt, 0, 0).psMax);  // Chansey intatto
  CHECK_EQ(bt.turno, (uint16_t)1);
}

TEST(battle, un_cambio_non_valido_vale_come_mossa) {
  Combatant noi[2] = { tauros({ MV_TACKLE }), chansey({ MV_TACKLE }) };
  Combatant loro = chansey({ MV_SPLASH });
  Battle bt;
  battleIniziaSquadre(bt, noi, 2, &loro, 1, 3);
  battleTurno(bt, BATTLE_CAMBIA + 0, 0);  // è già in campo
  CHECK_EQ(contaEventi(bt, EV_RITIRO), 0);
  CHECK(trovaEvento(bt, EV_MOSSA, 0) && trovaEvento(bt, EV_MOSSA, 0)->mossa == MV_TACKLE);
  battleTurno(bt, 0, BATTLE_CAMBIA + 1);  // l'altro lato non ha riserve
  CHECK_EQ(contaEventi(bt, EV_RITIRO), 0);
  // chi deve ricaricarsi non può uscire
  Combatant iper[2] = { tauros({ MV_HYPER_BEAM }), chansey({ MV_TACKLE }) };
  battleIniziaSquadre(bt, iper, 2, &loro, 1, 3);
  bt.lato[0].ricarica = true;
  CHECK(!battlePuoCambiare(bt, 0, 1));
  battleTurno(bt, BATTLE_CAMBIA + 1, 0);
  CHECK_EQ(contaEventi(bt, EV_RITIRO), 0);
  CHECK_EQ(contaEventi(bt, EV_RICARICA, 0), 1);
}

TEST(battle, cambio_forzato_senza_turno_e_fine_a_squadra_ko) {
  Combatant noi[2] = { crea(19, 5, 1, 5, 5, 5, 5, { MV_SPLASH }), chansey({ MV_SPLASH }) };
  Combatant loro = tauros({ MV_TACKLE });
  Battle bt;
  battleIniziaSquadre(bt, noi, 2, &loro, 1, 11);
  CHECK(battleTurno(bt, 0, 0));  // Rattata va KO, ma c'è ancora Chansey
  CHECK(bt.lato[0].ko());
  CHECK(bt.deveCambiare[0]);
  CHECK(!bt.deveCambiare[1]);
  CHECK_EQ(bt.vincitore, (uint8_t)BATTLE_IN_CORSO);
  CHECK_EQ(contaEventi(bt, EV_FINE), 0);
  CHECK_EQ(battleRestanti(bt, 0), (uint8_t)1);
  CHECK(!battleCambioForzato(bt, 1, 0));  // l'altro lato non deve cambiare
  CHECK(!battleCambioForzato(bt, 0, 0));  // è KO
  uint16_t turno = bt.turno;
  uint32_t rng = bt.rng.s;
  CHECK(battleCambioForzato(bt, 0, 1));
  CHECK_EQ(bt.turno, turno);
  CHECK_EQ(bt.rng.s, rng);
  CHECK_EQ(bt.nEventi, (uint8_t)1);
  CHECK_EQ(bt.eventi[0].tipo, (uint8_t)EV_ENTRA);
  CHECK(!bt.deveCambiare[0]);
  CHECK(battleMembro(bt, 0, 0).ko());
  CHECK_EQ(bt.lato[0].dex, (int16_t)113);
  CHECK(!battleCambioForzato(bt, 0, 1));  // già fatto
  // fino alla fine: vince chi ha ancora qualcuno in piedi
  while (battleTurno(bt, 0, 0)) {
  }
  CHECK_EQ(bt.vincitore, (uint8_t)1);
  CHECK_EQ(battleRestanti(bt, 0), (uint8_t)0);
  CHECK(!bt.deveCambiare[0]);
}

TEST(battle, senza_cambio_forzato_il_turno_manda_il_primo_vivo) {
  Combatant noi[3] = { crea(19, 5, 1, 5, 5, 5, 5, { MV_SPLASH }),
                       crea(19, 5, 1, 5, 5, 5, 5, { MV_SPLASH }), chansey({ MV_SPLASH }) };
  Combatant loro = chansey({ MV_TACKLE });
  Battle bt;
  battleIniziaSquadre(bt, noi, 3, &loro, 1, 5);
  bt.squadra[0][1].ps = 0;  // il secondo è già KO
  CHECK(battleTurno(bt, 0, 0));
  CHECK(bt.deveCambiare[0]);
  CHECK(battleTurno(bt, 0, 0));
  CHECK(posizione(bt, EV_ENTRA, 0) == 0);
  CHECK_EQ(bt.inCampo[0], (uint8_t)2);
}

TEST(battle, al_cambio_restano_ps_pp_e_stato_e_si_annulla_il_resto) {
  Combatant noi[2] = { tauros({ MV_TACKLE, MV_GROWL }), chansey({ MV_SPLASH }) };
  Combatant loro = chansey({ MV_SPLASH });
  Battle bt;
  battleIniziaSquadre(bt, noi, 2, &loro, 1, 9);
  Combatant &c = bt.lato[0];
  c.ps = 100;
  c.pp[0] = 3;
  c.stato = ST_SONNO;
  c.turniSonno = 4;
  c.confusione = 3;
  c.stadio[STAT_ATTACCO] = 2;
  c.stadio[STAT_ELUSIONE] = -1;
  c.riflesso = c.schermoluce = c.nebbia = c.concentrazione = c.parassiseme = true;
  battleTurno(bt, BATTLE_CAMBIA + 1, 0);
  const Combatant &t = battleMembro(bt, 0, 0);
  CHECK_EQ(t.ps, (uint16_t)100);
  CHECK_EQ(t.pp[0], (uint8_t)3);
  CHECK_EQ(t.pp[1], t.ppMax[1]);
  CHECK_EQ(t.stato, (uint8_t)ST_SONNO);
  CHECK_EQ(t.turniSonno, (uint8_t)4);
  CHECK_EQ(t.confusione, (uint8_t)0);
  CHECK_EQ(t.stadio[STAT_ATTACCO], (int8_t)0);
  CHECK_EQ(t.stadio[STAT_ELUSIONE], (int8_t)0);
  CHECK(!t.riflesso && !t.schermoluce && !t.nebbia && !t.concentrazione && !t.parassiseme);
  // e quando torna in campo è così
  battleTurno(bt, BATTLE_CAMBIA + 0, 0);
  CHECK_EQ(bt.lato[0].turniSonno, (uint8_t)4);
  CHECK_EQ(bt.lato[0].ps, (uint16_t)100);
  // Tossina torna veleno normale
  bt.lato[0].stato = ST_TOSSICO;
  bt.lato[0].tossico = 5;
  battleTurno(bt, BATTLE_CAMBIA + 1, 0);
  CHECK_EQ(battleMembro(bt, 0, 0).stato, (uint8_t)ST_VELENO);
  CHECK_EQ(battleMembro(bt, 0, 0).tossico, (uint8_t)0);
}

TEST(battle, al_cambio_la_trasformazione_si_annulla_con_i_pp_originali) {
  Combatant noi[2] = { crea(132, 50, 900, 80, 300, 80, 80, { MV_TRANSFORM }),
                       chansey({ MV_SPLASH }) };
  Combatant loro = tauros({ MV_TACKLE, MV_HYPER_BEAM });
  Battle bt;
  battleIniziaSquadre(bt, noi, 2, &loro, 1, 21);
  battleTurno(bt, 0, 0);
  CHECK_EQ(bt.lato[0].dex, (int16_t)128);
  bt.lato[0].pp[0] = 2;  // ha usato Azione da trasformato
  battleTurno(bt, BATTLE_CAMBIA + 1, 0);
  const Combatant &d = battleMembro(bt, 0, 0);
  CHECK_EQ(d.dex, (int16_t)132);
  CHECK_EQ(d.mosse[0], (uint8_t)MV_TRANSFORM);
  CHECK_EQ(d.pp[0], (uint8_t)(d.ppMax[0] - 1));
  CHECK_EQ(d.stat[STAT_ATTACCO], (uint16_t)80);
  CHECK(!d.trasformato);
}

TEST(battle, chi_esce_scioglie_l_intrappolamento) {
  // l'intrappolato può uscire; il nuovo entrato è libero e la presa finisce
  Combatant noi[2] = { chansey({ MV_SPLASH }), chansey({ MV_SPLASH }) };
  Combatant loro = crea(23, 50, 150, 200, 80, 300, 80, { MV_WRAP });  // Ekans, più veloce
  Battle bt;
  for (uint32_t seme = 1; seme < 50; seme++) {  // un seme in cui Avvolgibotta va a segno
    battleIniziaSquadre(bt, noi, 2, &loro, 1, seme);
    battleTurno(bt, 0, 0);
    if (bt.lato[0].intrappolato && bt.lato[1].bloccata) break;
  }
  CHECK(bt.lato[0].intrappolato);
  CHECK(battlePuoCambiare(bt, 0, 1));
  CHECK(!battlePuoCambiare(bt, 1, 0));
  battleTurno(bt, BATTLE_CAMBIA + 1, 0);
  CHECK_EQ(bt.inCampo[0], (uint8_t)1);
  CHECK(!battleMembro(bt, 0, 0).intrappolato);
  CHECK_EQ(bt.lato[1].bloccata, (uint8_t)0);
}

TEST(battle, ia_del_sostituto_facile_e_difficile) {
  // contro Charizard (fuoco/volante): facile = il primo vivo, difficile = Blastoise
  Combatant noi[3] = { crea(25, 50, 1, 50, 50, 50, 50, { MV_THUNDER_SHOCK }),
                       crea(3, 50, 150, 80, 80, 80, 80, { MV_VINE_WHIP }),
                       crea(9, 50, 150, 80, 80, 80, 80, { MV_WATER_GUN }) };
  Combatant loro = crea(6, 50, 150, 80, 80, 100, 80, { MV_EMBER });
  Battle bt;
  battleIniziaSquadre(bt, noi, 3, &loro, 1, 4);
  CHECK_EQ(battleSceltaSostituto(bt, 0, false), (uint8_t)1);
  CHECK_EQ(battleSceltaSostituto(bt, 0, true), (uint8_t)2);
  CHECK_EQ(battleSceltaSostituto(bt, 1, true), (uint8_t)BATTLE_NESSUNO);
  bt.squadra[0][2].ps = 0;
  CHECK_EQ(battleSceltaSostituto(bt, 0, true), (uint8_t)1);
  bt.squadra[0][1].ps = 0;
  CHECK_EQ(battleSceltaSostituto(bt, 0, false), (uint8_t)BATTLE_NESSUNO);
}

// squadre casuali da 1 a 3, mosse e cambi volontari dell'IA, cambi forzati del sostituto
static void battagliaASquadre(uint32_t seme, Battle &bt) {
  BattleRng r;
  r.seme(seme);
  Combatant sq[2][BATTLE_SQUADRA];
  uint8_t n[2];
  for (int k = 0; k < 2; k++) {
    n[k] = (uint8_t)(1 + r.sotto(3));
    for (int i = 0; i < n[k]; i++) {
      uint8_t lv = (uint8_t)(5 + r.sotto(96));
      battleCombattente(sq[k][i], (int16_t)(1 + r.sotto(151)), lv, 20 + lv * 3, 10 + lv * 2,
                        10 + lv * 2, 10 + lv * 2, 10 + lv * 2, nullptr);
    }
  }
  battleIniziaSquadre(bt, sq[0], n[0], sq[1], n[1], seme);
  for (;;) {
    uint8_t s[2];
    for (uint8_t k = 0; k < 2; k++) {
      s[k] = battleSceltaIA(bt, k, (seme + k) & 1);
      uint8_t sost = battleSceltaSostituto(bt, k, k == 1);
      if (sost != BATTLE_NESSUNO && r.sotto(8) == 0) s[k] = (uint8_t)(BATTLE_CAMBIA + sost);
    }
    bool continua = battleTurno(bt, s[0], s[1]);
    CHECK(bt.nEventi < BATTLE_MAX_EVENTI);
    for (int i = 0; i < bt.nEventi; i++) {
      BattleEvento e = {};
      CHECK(bt.coda.prendi(e));
      CHECK(e.tipo == bt.eventi[i].tipo && e.lato == bt.eventi[i].lato &&
            e.valore == bt.eventi[i].valore);
    }
    CHECK(bt.coda.vuota());
    if (!continua) break;
    for (uint8_t k = 0; k < 2; k++) {
      if (!bt.deveCambiare[k] || r.sotto(4) == 0) continue;  // a volte lo fa il turno
      CHECK(battleCambioForzato(bt, k, battleSceltaSostituto(bt, k, k == 0)));
      BattleEvento e = {};
      CHECK(bt.coda.prendi(e) && e.tipo == EV_ENTRA && e.lato == k);
    }
  }
}

TEST(battle, nessun_evento_perso_su_300_battaglie_a_squadre) {
  for (uint32_t seme = 1; seme <= 300; seme++) {
    Battle bt;
    battagliaASquadre(seme, bt);
    CHECK(bt.vincitore != BATTLE_IN_CORSO);
    CHECK(bt.turno <= BATTLE_MAX_TURNI);
    if (bt.vincitore < 2) {
      CHECK(battleRestanti(bt, bt.vincitore) > 0);
      CHECK_EQ(battleRestanti(bt, 1 - bt.vincitore), (uint8_t)0);
    }
    CHECK_EQ(bt.eventiPersi, (uint16_t)0);
    CHECK_EQ(bt.coda.persi, (uint16_t)0);
  }
}

TEST(battle, tre_contro_tre_fino_alla_fine_si_ripete_col_seme) {
  Battle a, b;
  battagliaASquadre(77, a);
  battagliaASquadre(77, b);
  CHECK_EQ(a.vincitore, b.vincitore);
  CHECK_EQ(a.turno, b.turno);
  CHECK_EQ(a.rng.s, b.rng.s);
}

TEST(battle, il_ritratto_combatte_come_il_pet) {
  Pet p = petDi(6, 47, 104, 61);
  p.geneDef = 93;
  p.trSpe = 12;
  Combatant dp, dr;
  CHECK(combattenteDaPet(dp, p));
  CHECK(combattenteDaRitratto(dr, p.ritratto()));
  CHECK_EQ(dr.dex, dp.dex);
  CHECK_EQ(dr.livello, dp.livello);
  CHECK_EQ(dr.psMax, dp.psMax);
  for (int i = 0; i < 4; i++) {
    CHECK_EQ(dr.stat[i], dp.stat[i]);
    CHECK_EQ(dr.mosse[i], dp.mosse[i]);
  }
  PetRitratto vuoto;
  CHECK(!combattenteDaRitratto(dr, vuoto));
  CHECK_EQ(dr.dex, (int16_t)0);
}
