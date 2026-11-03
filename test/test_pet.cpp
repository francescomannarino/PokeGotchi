// Test della logica del Pokémon (pet.cpp) sugli shim dell'host.
#include "framework.h"
#include "shim/Arduino.h"
#include "../pet.h"
#include "../dex.h"
#include "../savefile.h"
#include <string.h>
#include <stddef.h>
#include <vector>

// ---------------------------------------------------------------- helper
static void advance(Pet &p, uint32_t minutes) {
  for (uint32_t i = 0; i < minutes; i++) {
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
  }
}

// Pokémon appena schiuso della specie richiesta, con la NVS pulita
static void makePet(Pet &p, int16_t dex) {
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(dex);
  p.eggTap();
  p.eggTap();
  p.eggTap();  // 3 tocchi = schiusa immediata
}

static void setStats(Pet &p, uint8_t f, uint8_t j, uint8_t e, uint8_t h) {
  p.fullness = f; p.joy = j; p.energy = e; p.hygiene = h;
}

// ---------------------------------------------------------------- uovo
TEST(egg, la_prima_partita_chiede_l_iniziale) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK(p.isEgg());
  CHECK(p.awaitingStarter());
  CHECK_EQ(p.eggCracks(), (uint8_t)0);
  CHECK_EQ(p.registeredCount(), (uint16_t)0);
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
}

// regressione v1.3: l'uovo si schiudeva da solo dopo 3 min mentre il giocatore stava
// ancora scegliendo l'iniziale, e la scelta andava persa
TEST(egg, non_si_schiude_mentre_si_sceglie_l_iniziale) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK(p.awaitingStarter());
  advance(p, 600);  // 10 ore di gioco
  CHECK_MSG(p.isEgg(), "el huevo NO debe eclosionar durante la eleccion");
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
  p.chooseStarter(7);
  CHECK(!p.awaitingStarter());
  advance(p, 3);
  CHECK(!p.isEgg());
  CHECK_EQ(p.speciesId, (int16_t)7);
}

TEST(egg, si_schiude_da_solo_dopo_3_minuti) {
  mockNvsReset();
  Pet p;
  p.begin();
  p.chooseStarter(1);
  advance(p, 2);
  CHECK(p.isEgg());
  advance(p, 1);
  CHECK(!p.isEgg());
  CHECK_EQ(p.speciesId, (int16_t)1);
}

TEST(egg, tre_tocchi_lo_schiudono) {
  mockNvsReset();
  Pet p;
  p.begin();
  p.chooseStarter(4);
  p.eggTap();
  CHECK_EQ(p.eggCracks(), (uint8_t)1);
  CHECK(p.isEgg());
  p.eggTap();
  CHECK_EQ(p.eggCracks(), (uint8_t)2);
  CHECK(p.isEgg());
  p.eggTap();
  CHECK(!p.isEgg());
  CHECK_EQ(p.speciesId, (int16_t)4);
}

TEST(egg, schiudersi_estrae_i_geni_e_azzera_l_individuo) {
  Pet p;
  makePet(p, 4);
  CHECK_RANGE(p.geneAtk, 90, 110);
  CHECK_RANGE(p.geneDef, 90, 110);
  CHECK_RANGE(p.geneSpe, 90, 110);
  CHECK_EQ(p.trAtk, (uint8_t)0);
  CHECK_EQ(p.trDef, (uint8_t)0);
  CHECK_EQ(p.trSpe, (uint8_t)0);
  CHECK_EQ(p.bond, (uint8_t)0);
  CHECK_EQ(p.berryKnown, false);
  CHECK_STREQ(p.nick, "");
  CHECK_MSG(p.isRegistered(4), "criar = registrar en la pokedex");
  CHECK_EQ(p.registeredCount(), (uint16_t)1);
}

TEST(egg, toccare_l_uovo_non_fa_nulla_se_gia_schiuso) {
  Pet p;
  makePet(p, 4);
  uint8_t cracks = p.eggCracks();
  p.eggTap();
  CHECK_EQ(p.eggCracks(), cracks);
  CHECK_EQ(p.speciesId, (int16_t)4);
}

TEST(egg, la_rarita_dell_uovo_e_valida) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK_RANGE(p.eggRarity(), (uint8_t)R_COMUN, (uint8_t)R_LEGENDARIO);
}

// ---------------------------------------------------------------- cibo
TEST(feed, bacca_normale_alza_di_25_e_si_ferma_a_100) {
  Pet p;
  makePet(p, 4);
  p.fullness = 50;
  uint8_t notLoved = (uint8_t)((p.speciesId + 1) % 3);
  p.feedBerry(notLoved);
  CHECK_EQ(p.fullness, (uint8_t)75);
  p.feedBerry(notLoved);
  CHECK_EQ(p.fullness, (uint8_t)100);  // 100 invece di 105
}

TEST(feed, bacca_preferita_alza_di_piu_e_da_un_cuore) {
  Pet p;
  makePet(p, 4);  // 4 % 3 == 1 -> gli piace la blu
  CHECK(p.lovesBerry(1));
  CHECK(!p.lovesBerry(0));
  CHECK(!p.lovesBerry(2));
  p.fullness = 10;
  p.joy = 10;
  p.feedBerry(1);
  CHECK_EQ(p.fullness, (uint8_t)45);
  CHECK_EQ(p.joy, (uint8_t)20);
  CHECK(p.berryKnown);
  CHECK(p.showHeart());
  CHECK(p.eating());
}

TEST(feed, il_dolcetto_fa_ingrassare) {
  Pet p;
  makePet(p, 4);
  p.fullness = 50;
  p.joy = 50;
  p.feedCandy();
  CHECK_EQ(p.fullness, (uint8_t)60);
  CHECK_EQ(p.joy, (uint8_t)62);
  CHECK_EQ(p.weight, (uint8_t)12);
}

TEST(feed, addormentato_o_uovo_non_mangia) {
  Pet p;
  makePet(p, 4);
  p.fullness = 40;
  p.toggleLight();  // a dormire
  CHECK(p.sleeping);
  p.feedBerry(1);
  p.feedCandy();
  CHECK_EQ(p.fullness, (uint8_t)40);

  Pet e;
  mockNvsReset();
  e.begin();
  e.chooseStarter(4);
  e.fullness = 40;
  e.feedBerry(0);
  CHECK_EQ(e.fullness, (uint8_t)40);
}

// ---------------------------------------------------------------- tick
TEST(tick, sveglio_cala_cibo_2_ed_energia_1_al_minuto) {
  Pet p;
  makePet(p, 4);
  setStats(p, 100, 100, 100, 100);
  p.poops = 0;
  mockForceRandom(99);  // random(100) < 15 falso: niente cacche
  advance(p, 10);
  mockClearForcedRandom();
  CHECK_EQ(p.fullness, (uint8_t)80);
  CHECK_EQ(p.energy, (uint8_t)90);
  CHECK_EQ(p.hygiene, (uint8_t)90);
  CHECK_EQ(p.joy, (uint8_t)90);
  CHECK_EQ(p.ageMinutes, (uint32_t)10);
  CHECK_EQ(p.poops, (uint8_t)0);
}

TEST(tick, le_barre_non_scendono_mai_sotto_zero) {
  Pet p;
  makePet(p, 4);
  setStats(p, 5, 3, 2, 1);
  advance(p, 30);
  CHECK_EQ(p.fullness, (uint8_t)0);
  CHECK_EQ(p.joy, (uint8_t)0);
  CHECK_EQ(p.energy, (uint8_t)0);
  CHECK_EQ(p.hygiene, (uint8_t)0);
}

TEST(tick, dormire_recupera_energia_con_minimi) {
  Pet p;
  makePet(p, 4);
  setStats(p, 100, 100, 10, 100);
  p.toggleLight();
  advance(p, 20);
  CHECK_MSG(p.energy == 100, "durmiendo la energia sube 6/min hasta 100");
  CHECK_MSG(p.fullness > 80, "durmiendo el hambre baja ~4x mas lento");
  advance(p, 2000);  // dorme moltissimo: i minimi proteggono
  CHECK_MSG(p.fullness >= 30, "suelo de comida durmiendo = 30");
  CHECK_MSG(p.joy >= 35, "suelo de felicidad durmiendo = 35");
  CHECK_MSG(p.hygiene >= 45, "suelo de higiene durmiendo = 45");
  CHECK_EQ(p.careMistakes, (uint8_t)0);
}

TEST(tick, il_sovrappeso_si_smaltisce_da_solo) {
  Pet p;
  makePet(p, 4);
  p.weight = 30;
  advance(p, 30);
  CHECK_EQ(p.weight, (uint8_t)20);  // -1 ogni 3 minuti
}

TEST(tick, l_incuria_conta_una_volta_all_ora) {
  Pet p;
  makePet(p, 4);
  setStats(p, 0, 0, 0, 0);
  advance(p, 1);
  CHECK_EQ(p.careMistakes, (uint8_t)1);
  advance(p, 50);
  CHECK_MSG(p.careMistakes == 1, "el enfriamiento evita contar el mismo descuido cada minuto");
  advance(p, 11);
  CHECK_EQ(p.careMistakes, (uint8_t)2);
}

// regressione v1.4: la trascuratezza toglieva 3 di legame ogni 30 min ed era
// impossibile recuperarlo; ora toglie 1 e non lo lascia mai a 0
TEST(tick, l_incuria_raffredda_il_legame_senza_azzerarlo) {
  Pet p;
  makePet(p, 4);
  p.bond = 50;
  setStats(p, 0, 0, 0, 0);
  advance(p, 1);
  CHECK_EQ(p.bond, (uint8_t)49);
  p.bond = 1;
  advance(p, 61);
  CHECK_MSG(p.bond == 1, "el vinculo no cae por debajo de 1");
}

TEST(tick, buona_cura_per_12h_forgia_la_difesa) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.trDef, (uint8_t)0);
  for (int h = 0; h < 720; h++) {  // mantenere tutto >= 40 per 12 h
    setStats(p, 100, 100, 100, 100);
    advance(p, 1);
  }
  CHECK_EQ(p.trDef, (uint8_t)1);
}

// ---------------------------------------------------------------- livelli
TEST(level, un_livello_per_ora_di_gioco) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.level(), (uint8_t)1);
  p.ageMinutes = 59;
  CHECK_EQ(p.level(), (uint8_t)1);
  p.ageMinutes = 60;
  CHECK_EQ(p.level(), (uint8_t)2);
  p.ageMinutes = 24 * 60;
  CHECK_EQ(p.level(), (uint8_t)25);
}

// Corretto in v1.8: level() era uint8_t e tornava a 0 dopo ~10,6 giorni
TEST(level, non_trabocca_con_pet_longevi) {
  Pet p;
  makePet(p, 4);
  p.ageMinutes = 254 * 60;
  uint8_t before = p.level();
  CHECK_EQ(before, (uint8_t)255);
  p.ageMinutes = 255 * 60;  // livello 256 -> dovrebbe continuare a salire (o saturare)
  CHECK_MSG(p.level() >= before, "el nivel nunca deberia bajar al envejecer");
}

// ---------------------------------------------------------------- evoluzione
TEST(evolve, richiede_livello_e_buona_cura) {
  Pet p;
  makePet(p, 4);  // CHARMANDER evolve al livello 16
  p.ageMinutes = 14 * 60;  // livello 15
  CHECK(!p.canEvolveNow());
  p.ageMinutes = 15 * 60;  // livello 16
  CHECK(p.canEvolveNow());
  p.fullness = 39;  // una barra sotto 40 lo blocca
  CHECK(!p.canEvolveNow());
  p.fullness = 40;
  CHECK(p.canEvolveNow());
}

TEST(evolve, ogni_incuria_ritarda_di_un_livello) {
  Pet p;
  makePet(p, 4);
  p.ageMinutes = 15 * 60;  // livello 16
  p.careMistakes = 1;
  CHECK(!p.canEvolveNow());
  p.ageMinutes = 16 * 60;  // livello 17
  CHECK(p.canEvolveNow());
}

// Corretto in v1.8: il cast a uint8_t di (evolveLevel + careMistakes) andava in
// overflow
TEST(evolve, molte_incurie_non_anticipano_l_evoluzione) {
  Pet p;
  makePet(p, 4);       // CHARMANDER: evolve al livello 16
  p.careMistakes = 250;  // 250 trascuratezze: dovrebbe essere lontanissimo dall'evolvere
  p.ageMinutes = 9 * 60;  // livello 10
  CHECK_MSG(!p.canEvolveNow(), "con 250 descuidos no puede evolucionar a nivel 10");
}

TEST(evolve, non_evolve_da_addormentato_ne_da_uovo_ne_in_cerimonia) {
  Pet p;
  makePet(p, 4);
  p.ageMinutes = 20 * 60;
  CHECK(p.canEvolveNow());
  p.toggleLight();
  CHECK(!p.canEvolveNow());
  p.toggleLight();
  p.startFarewell();
  CHECK(!p.canEvolveNow());

  Pet e;
  mockNvsReset();
  e.begin();
  CHECK(!e.canEvolveNow());
}

TEST(evolve, trasforma_registra_e_anima) {
  Pet p;
  makePet(p, 4);
  p.ageMinutes = 20 * 60;
  p.evolve();
  CHECK_EQ(p.speciesId, (int16_t)5);
  CHECK_EQ(p.prevSpeciesId, (int16_t)4);
  CHECK(p.isRegistered(5));
  CHECK(p.evolving());
  CHECK_RANGE(p.evolveT(), 0.0f, 1.0f);
  mockAdvanceMillis(EVOLVE_ANIM_MS + 1);
  CHECK(!p.evolving());
}

TEST(evolve, la_forma_finale_non_evolve) {
  Pet p;
  makePet(p, 6);  // CHARIZARD
  p.ageMinutes = 200 * 60;
  CHECK(!p.canEvolveNow());
  p.evolve();
  CHECK_EQ(p.speciesId, (int16_t)6);
}

TEST(evolve, eevee_si_ramifica_in_134_136) {
  for (int seed = 1; seed <= 12; seed++) {
    randomSeed(seed);
    Pet p;
    makePet(p, DEX_EEVEE);
    p.ageMinutes = 40 * 60;
    CHECK(p.canEvolveNow());
    p.evolve();
    CHECK_RANGE(p.speciesId, (int16_t)134, (int16_t)136);
    CHECK(p.isRegistered(p.speciesId));
  }
}

TEST(evolve, il_pulsante_di_evoluzione_si_rimanda_se_si_rifiuta) {
  Pet p;
  makePet(p, 4);
  p.ageMinutes = 15 * 60;  // livello 16
  CHECK(p.wantEvolveButton());
  p.declineEvolve();
  CHECK_MSG(!p.wantEvolveButton(), "'mantener forma' quita el boton");
  CHECK_MSG(p.canEvolveNow(), "pero la evolucion sigue disponible");
  p.ageMinutes = 16 * 60;  // sale di livello: si riofferisce
  CHECK(p.wantEvolveButton());
}

// ---------------------------------------------------------------- statistiche
TEST(stats, formula_base_per_geni_piu_livello_e_allenamento) {
  Pet p;
  makePet(p, 4);  // CHARMANDER: atk 52, def 43, spe 65
  p.geneAtk = p.geneDef = p.geneSpe = 100;
  p.trAtk = p.trDef = p.trSpe = 0;
  p.ageMinutes = 0;  // livello 1
  CHECK_EQ(p.atkStat(), (uint16_t)(52 + 1));
  CHECK_EQ(p.defStat(), (uint16_t)(43 + 1));
  CHECK_EQ(p.speStat(), (uint16_t)(65 + 1));
  p.ageMinutes = 9 * 60;  // livello 10
  p.trAtk = 20;
  CHECK_EQ(p.atkStat(), (uint16_t)(52 + 10 + 20));
  p.geneAtk = 110;
  CHECK_EQ(p.atkStat(), (uint16_t)(52 * 110 / 100 + 10 + 20));
}

TEST(stats, la_speciale_usa_la_media_di_geni_e_allenamento_di_att_e_dif) {
  Pet p;
  makePet(p, 4);  // CHARMANDER: Speciale 50
  p.geneAtk = 110;
  p.geneDef = 90;
  p.trAtk = 30;
  p.trDef = 10;
  p.ageMinutes = 9 * 60;  // livello 10
  CHECK_EQ(p.spcStat(), (uint16_t)(50 + 10 + 20));
  p.geneDef = 110;
  CHECK_EQ(p.spcStat(), (uint16_t)(50 * 110 / 100 + 10 + 20));
}

TEST(stats, l_uovo_non_ha_stats) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK_EQ(p.atkStat(), (uint16_t)0);
  CHECK_EQ(p.defStat(), (uint16_t)0);
  CHECK_EQ(p.speStat(), (uint16_t)0);
  CHECK_EQ(p.spcStat(), (uint16_t)0);
}

TEST(train, il_sacco_da_un_punto_ogni_4_colpi_con_tetto) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.trainStrength(0), (uint8_t)0);
  CHECK_EQ(p.trainStrength(40), (uint8_t)10);
  CHECK_EQ(p.trAtk, (uint8_t)10);
  CHECK_MSG(p.trainStrength(1000) == 18, "tope de 18 por sesion");
  CHECK_EQ(p.strHi, (uint16_t)1000);
}

// Corretto portando la batteria: trainStrength() restituiva l'aumento teorico
TEST(train, il_sacco_annuncia_cio_che_sale_davvero) {
  Pet p;
  makePet(p, 4);
  p.trAtk = 95;
  uint8_t before = p.trAtk;
  uint8_t gain = p.trainStrength(72);  // 72/4 = 18, ma ne entrano solo 5
  CHECK_EQ(gain, (uint8_t)(p.trAtk - before));
}

TEST(train, l_allenamento_si_ferma_a_100) {
  Pet p;
  makePet(p, 4);
  for (int i = 0; i < 20; i++) p.trainStrength(80);
  CHECK_EQ(p.trAtk, (uint8_t)100);
}

TEST(train, il_sacco_stanca_e_brucia_peso) {
  Pet p;
  makePet(p, 4);
  p.weight = 40;
  p.energy = 80;
  p.fullness = 80;
  p.trainStrength(30);
  CHECK_EQ(p.energy, (uint8_t)68);
  CHECK_EQ(p.fullness, (uint8_t)75);
  CHECK_EQ(p.weight, (uint8_t)30);
  p.energy = 3;  // il minimo protegge dal lasciarlo a zero
  p.trainStrength(30);
  CHECK_EQ(p.energy, (uint8_t)3);
}

TEST(play, il_minigioco_allena_la_velocita_e_salva_il_record) {
  Pet p;
  makePet(p, 4);
  p.joy = 10;
  p.weight = 50;
  p.playResult(20);
  CHECK_EQ(p.trSpe, (uint8_t)4);
  CHECK_EQ(p.joy, (uint8_t)45);   // +5 +30 per averne segnati più di 15
  CHECK_EQ(p.weight, (uint8_t)10);  // 50 - 20*2
  CHECK_EQ(p.gameHi, (uint16_t)20);
  p.playResult(5);
  CHECK_MSG(p.gameHi == 20, "un resultado peor no baja el record");
}

TEST(play, giocare_alza_la_felicita_e_stanca) {
  Pet p;
  makePet(p, 4);
  setStats(p, 50, 50, 50, 50);
  p.play();
  CHECK_EQ(p.joy, (uint8_t)75);
  CHECK_EQ(p.energy, (uint8_t)40);
  CHECK_EQ(p.fullness, (uint8_t)45);
  CHECK(p.showHeart());
}

// ---------------------------------------------------------------- giochi a inclinazione
TEST(grotta, allena_la_difesa_un_punto_ogni_4_con_tetto) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.grottaResult(0), (uint8_t)0);
  CHECK_EQ(p.grottaResult(41), (uint8_t)10);
  CHECK_EQ(p.trDef, (uint8_t)10);
  CHECK_EQ(p.trAtk, (uint8_t)0);
  CHECK_EQ(p.trSpe, (uint8_t)0);
  CHECK_MSG(p.grottaResult(500) == 18, "tetto di 18 per partita, come il sacco");
  CHECK_EQ(p.trDef, (uint8_t)28);
}

TEST(grotta, la_difesa_si_ferma_a_100_e_annuncia_cio_che_entra) {
  Pet p;
  makePet(p, 4);
  p.trDef = 95;
  CHECK_EQ(p.grottaResult(72), (uint8_t)5);
  CHECK_EQ(p.trDef, (uint8_t)100);
  CHECK_EQ(p.grottaResult(72), (uint8_t)0);
}

TEST(grotta, felicita_fatica_peso_cuore_e_legame) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 10, 80, 80);
  p.weight = 50;
  uint8_t legame = p.bond;
  p.grottaResult(12);
  CHECK_EQ(p.joy, (uint8_t)27);       // +5 +12
  CHECK_EQ(p.energy, (uint8_t)64);    // -(10 + 12/2)
  CHECK_EQ(p.fullness, (uint8_t)75);
  CHECK_EQ(p.weight, (uint8_t)26);    // 50 - 12*2
  CHECK(p.showHeart());
  CHECK(p.bond > legame);
  p.joy = 10;
  p.grottaResult(200);
  CHECK_EQ(p.joy, (uint8_t)45);       // +5 +30 al massimo
  CHECK_EQ(p.weight, (uint8_t)0);
}

TEST(grotta, pochi_punti_niente_cuore) {
  Pet p;
  makePet(p, 4);
  p.grottaResult(9);
  CHECK(!p.showHeart());
}

TEST(grotta, il_record_non_scende) {
  Pet p;
  makePet(p, 4);
  p.grottaResult(30);
  CHECK_EQ(p.grottaHi, (uint16_t)30);
  p.grottaResult(12);
  CHECK_EQ(p.grottaHi, (uint16_t)30);
  CHECK_EQ(p.surfHi, (uint16_t)0);
}

TEST(surf, nessuna_statistica_ma_felicita_e_record) {
  Pet p;
  makePet(p, 25);
  setStats(p, 80, 10, 80, 80);
  p.surfResult(8);
  CHECK_EQ(p.joy, (uint8_t)23);
  CHECK_EQ(p.trAtk, (uint8_t)0);
  CHECK_EQ(p.trDef, (uint8_t)0);
  CHECK_EQ(p.trSpe, (uint8_t)0);
  CHECK_EQ(p.surfHi, (uint16_t)8);
  p.surfResult(3);
  CHECK_EQ(p.surfHi, (uint16_t)8);
  CHECK_EQ(p.grottaHi, (uint16_t)0);
}

TEST(grotta, niente_effetto_da_uovo_ne_in_cerimonia) {
  mockNvsReset();
  Pet e;
  e.begin();
  CHECK(e.isEgg());
  uint8_t gioia = e.joy;
  CHECK_EQ(e.grottaResult(40), (uint8_t)0);
  e.surfResult(40);
  CHECK_EQ(e.joy, gioia);
  CHECK_EQ(e.grottaHi, (uint16_t)0);
  CHECK_EQ(e.surfHi, (uint16_t)0);

  Pet p;
  makePet(p, 4);
  p.startFarewell();
  CHECK_EQ(p.grottaResult(40), (uint8_t)0);
  p.surfResult(40);
  CHECK_EQ(p.trDef, (uint8_t)0);
  CHECK_EQ(p.grottaHi, (uint16_t)0);
  CHECK_EQ(p.surfHi, (uint16_t)0);
}

// ---------------------------------------------------------------- scossone e MOVIMENTO
TEST(spavento, toglie_felicita_poi_pausa_di_60_s) {
  Pet p;
  makePet(p, 4);
  p.joy = 50;
  p.flushSave();
  CHECK_EQ(p.spavento(), (uint8_t)SPAVENTO_GIOIA);
  CHECK_EQ(p.joy, (uint8_t)40);
  CHECK_MSG(p.savePending(), "salvataggio differito, come il tick");
  CHECK(p.spaventoInPausa());
  mockAdvanceMillis(SPAVENTO_PAUSA_MS - 1);
  CHECK_MSG(p.spavento() == 0, "nella pausa niente penalita");
  CHECK_EQ(p.joy, (uint8_t)40);
  mockAdvanceMillis(1);
  CHECK(!p.spaventoInPausa());
  CHECK_EQ(p.spavento(), (uint8_t)SPAVENTO_GIOIA);
  CHECK_EQ(p.joy, (uint8_t)30);
}

TEST(spavento, nel_sonno_meno_e_resta_addormentato) {
  Pet p;
  makePet(p, 4);
  p.joy = 50;
  p.sleeping = true;
  CHECK_EQ(p.spavento(), (uint8_t)SPAVENTO_GIOIA_SONNO);
  CHECK_EQ(p.joy, (uint8_t)45);
  CHECK(p.sleeping);
}

TEST(spavento, si_ferma_a_zero) {
  Pet p;
  makePet(p, 4);
  p.joy = 4;
  CHECK_EQ(p.spavento(), (uint8_t)4);
  CHECK_EQ(p.joy, (uint8_t)0);
}

TEST(spavento, niente_su_uovo_ne_in_cerimonia) {
  mockNvsReset();
  mockSetMillis(0);
  Pet e;
  e.begin();
  CHECK(e.isEgg());
  uint8_t gioia = e.joy;
  CHECK_EQ(e.spavento(), (uint8_t)0);
  CHECK_EQ(e.joy, gioia);
  CHECK(!e.spaventoInPausa());

  Pet p;
  makePet(p, 4);
  p.joy = 50;
  p.startFarewell();
  uint8_t prima = p.joy;
  CHECK_EQ(p.spavento(), (uint8_t)0);
  CHECK_EQ(p.joy, prima);
  CHECK_MSG(p.ceremony == CER_FAREWELL, "nessun effetto sul ciclo di vita");
}

TEST(movimento, acceso_di_default_e_persistito) {
  Pet p;
  makePet(p, 4);
  CHECK(p.movimento());
  p.setMovimento(false);
  CHECK(!p.movimento());
  Pet q;  // stessa NVS: la chiave sopravvive al riavvio
  q.begin();
  CHECK(!q.movimento());
  q.setMovimento(true);
  Pet r;
  r.begin();
  CHECK(r.movimento());
}

// ---------------------------------------------------------------- battaglie (CPU)
TEST(battle, non_si_lotta_da_uovo_nel_sonno_in_cerimonia_ne_stanchi) {
  Pet p;
  mockNvsReset();
  p.begin();
  CHECK(!p.canBattle());  // uovo in attesa dell'iniziale
  makePet(p, 4);
  setStats(p, 50, 50, 50, 50);
  CHECK(p.canBattle());
  p.energy = BATTLE_MIN_ENERGY - 1;
  CHECK(!p.canBattle());
  p.energy = BATTLE_MIN_ENERGY;
  CHECK(p.canBattle());
  p.toggleLight();
  CHECK(!p.canBattle());
  p.toggleLight();
  p.startRunaway();
  CHECK(!p.canBattle());
}

// il motivo per la squadra della lotta in collegamento: il pet resta, grigio
TEST(battle, il_motivo_per_cui_non_lotta) {
  Pet p;
  mockNvsReset();
  p.begin();
  CHECK_EQ((int)p.lottaPerche(), (int)LOT_UOVO);  // uovo in attesa dell'iniziale
  makePet(p, 4);
  setStats(p, 50, 50, 50, 50);
  CHECK_EQ((int)p.lottaPerche(), (int)LOT_OK);
  p.energy = BATTLE_MIN_ENERGY - 1;
  CHECK_EQ((int)p.lottaPerche(), (int)LOT_STANCO);
  p.toggleLight();  // il sonno conta prima della stanchezza
  CHECK_EQ((int)p.lottaPerche(), (int)LOT_DORME);
}

TEST(battle, non_si_lotta_mentre_evolve) {
  Pet p;
  makePet(p, 4);
  setStats(p, 90, 90, 90, 90);
  p.ageMinutes = 16 * MINUTES_PER_LEVEL;
  CHECK(p.canEvolveNow());
  p.evolve();
  CHECK(p.evolving());
  CHECK(!p.canBattle());
}

TEST(battle, la_lotta_costa_energia_e_sazieta_fuggire_meno) {
  Pet p;
  makePet(p, 4);
  setStats(p, 50, 50, 50, 50);
  p.battleResult(BTL_PARI);
  CHECK_EQ(p.energy, (uint8_t)38);
  CHECK_EQ(p.fullness, (uint8_t)44);
  CHECK_EQ(p.joy, (uint8_t)50);
  setStats(p, 50, 50, 50, 50);
  p.battleResult(BTL_FUGA);
  CHECK_EQ(p.energy, (uint8_t)46);
  CHECK_EQ(p.fullness, (uint8_t)48);
  setStats(p, 6, 50, 6, 50);
  p.battleResult(BTL_PERSA);  // i costi si fermano a 5, come il minigioco
  CHECK_EQ(p.energy, (uint8_t)5);
  CHECK_EQ(p.fullness, (uint8_t)5);
}

TEST(battle, la_vittoria_premia_gioia_legame_e_allenamento) {
  Pet p;
  makePet(p, 4);
  p.setClock(100u * 86400);
  setStats(p, 50, 50, 50, 50);
  uint8_t atk = p.trAtk, def = p.trDef, spe = p.trSpe, bond = p.bond;
  CHECK(p.battleResult(BTL_VINTA));
  CHECK_EQ(p.joy, (uint8_t)65);
  CHECK_EQ(p.trAtk, (uint8_t)(atk + 2));
  CHECK_EQ(p.trDef, (uint8_t)(def + 2));
  CHECK_EQ(p.trSpe, spe);
  CHECK_EQ(p.bond, (uint8_t)(bond + 3));
  CHECK(p.showHeart());
  CHECK_EQ(p.battleWinsLeft(), (uint8_t)(BATTLE_WINS_PER_DAY - 1));
}

TEST(battle, la_sconfitta_toglie_un_po_di_gioia_e_nient_altro) {
  Pet p;
  makePet(p, 4);
  setStats(p, 50, 50, 50, 50);
  uint8_t atk = p.trAtk, bond = p.bond;
  CHECK(!p.battleResult(BTL_PERSA));
  CHECK_EQ(p.joy, (uint8_t)42);
  CHECK_EQ(p.trAtk, atk);
  CHECK_EQ(p.bond, bond);
}

TEST(battle, al_massimo_5_vittorie_premiate_al_giorno) {
  Pet p;
  makePet(p, 4);
  p.setClock(100u * 86400);
  for (int i = 0; i < BATTLE_WINS_PER_DAY; i++) {
    p.energy = 100;
    CHECK(p.battleResult(BTL_VINTA));
  }
  CHECK_EQ(p.battleWinsLeft(), (uint8_t)0);
  uint8_t atk = p.trAtk;
  p.joy = 50;
  CHECK_MSG(!p.battleResult(BTL_VINTA), "la sesta vittoria del giorno non e' premiata");
  CHECK_EQ(p.trAtk, atk);
  CHECK_EQ(p.joy, (uint8_t)53);
  // il giorno dopo si ricomincia
  p.setClock(101u * 86400);
  CHECK_EQ(p.battleWinsLeft(), (uint8_t)BATTLE_WINS_PER_DAY);
  CHECK(p.battleResult(BTL_VINTA));
}

TEST(battle, il_limite_giornaliero_sopravvive_al_riavvio) {
  Pet a;
  makePet(a, 4);
  a.setClock(100u * 86400);
  for (int i = 0; i < BATTLE_WINS_PER_DAY; i++) a.battleResult(BTL_VINTA);
  a.flushSave();
  Pet b;
  b.begin();
  b.setClock(100u * 86400 + 3600);
  CHECK_EQ(b.battleWinsLeft(), (uint8_t)0);
  CHECK(!b.battleResult(BTL_VINTA));
}

TEST(battle, nessun_effetto_sul_ciclo_di_vita_e_salvataggio_differito) {
  Pet p;
  makePet(p, 4);
  p.setClock(100u * 86400);
  setStats(p, 21, 21, 21, 21);
  p.flushSave();
  CHECK(!p.savePending());
  uint32_t age = p.ageMinutes;
  uint8_t mist = p.careMistakes;
  for (uint8_t esito = BTL_VINTA; esito <= BTL_FUGA; esito++)
    for (int i = 0; i < 10; i++) p.battleResult(esito);
  CHECK_EQ(p.ageMinutes, age);
  CHECK_EQ(p.careMistakes, mist);
  CHECK_EQ(p.speciesId, (int16_t)4);
  CHECK_EQ(p.ceremony, (uint8_t)CER_NONE);
  CHECK(!p.isEgg() && !p.sleeping);
  CHECK(p.energy >= 5 && p.fullness >= 5 && p.joy >= 5);
  CHECK_MSG(p.savePending(), "la battaglia salva con savePending(), non subito");
}

TEST(battle, da_uovo_o_in_cerimonia_il_risultato_non_fa_nulla) {
  Pet p;
  makePet(p, 4);
  setStats(p, 50, 50, 50, 50);
  p.startFarewell();
  CHECK(!p.battleResult(BTL_VINTA));
  CHECK_EQ(p.energy, (uint8_t)50);
  CHECK_EQ(p.joy, (uint8_t)50);
}

// ---------------------------------------------------------------- battaglie in collegamento
TEST(linkbattle, la_vittoria_conta_e_da_legame_senza_allenamento_ne_costi) {
  Pet p;
  makePet(p, 4);
  p.setClock(100u * 86400);
  setStats(p, 50, 50, 50, 50);
  p.flushSave();
  uint8_t atk = p.trAtk, def = p.trDef, spe = p.trSpe, bond = p.bond;
  p.linkBattleResult(BTL_VINTA, true);
  CHECK_EQ(p.linkWins(), (uint16_t)1);
  CHECK_EQ(p.bond, (uint8_t)(bond + LINK_BOND_WIN));
  CHECK_EQ(p.trAtk, atk);
  CHECK_EQ(p.trDef, def);
  CHECK_EQ(p.trSpe, spe);
  CHECK_EQ(p.energy, (uint8_t)50);
  CHECK_EQ(p.fullness, (uint8_t)50);
  CHECK_EQ(p.joy, (uint8_t)50);
  CHECK_EQ(p.battleWinsLeft(), (uint8_t)BATTLE_WINS_PER_DAY);
  CHECK_MSG(p.savePending(), "salvataggio differito: con la radio accesa si salva all'uscita");
}

TEST(linkbattle, il_legame_solo_se_il_pet_attivo_era_in_squadra) {
  Pet p;
  makePet(p, 4);
  uint8_t bond = p.bond;
  p.linkBattleResult(BTL_VINTA, false);
  CHECK_EQ(p.linkWins(), (uint16_t)1);
  CHECK_EQ(p.bond, bond);
  p.linkBattleResult(BTL_PERSA, true);
  CHECK_EQ(p.bond, (uint8_t)(bond + LINK_BOND_PLAY));
  p.linkBattleResult(BTL_PARI, true);
  CHECK_EQ(p.bond, (uint8_t)(bond + 2 * LINK_BOND_PLAY));
  p.linkBattleResult(BTL_FUGA, true);  // la resa non dà niente
  CHECK_EQ(p.bond, (uint8_t)(bond + 2 * LINK_BOND_PLAY));
  CHECK_EQ(p.linkWins(), (uint16_t)1);
}

TEST(linkbattle, il_contatore_e_una_chiave_nvs_che_sopravvive_al_riavvio) {
  Pet a;
  makePet(a, 4);
  for (int i = 0; i < 7; i++) a.linkBattleResult(BTL_VINTA, true);
  a.flushSave();
  Pet b;
  b.begin();
  CHECK_EQ(b.linkWins(), (uint16_t)7);
  // fuori dal blob: una chiave sua del namespace esportato
  Preferences pr;
  pr.begin("pokegotchi", true);
  CHECK(pr.isKey(LINK_WINS_KEY));
  CHECK_EQ(pr.getUShort(LINK_WINS_KEY, 0), (uint16_t)7);
  pr.end();
}

TEST(battle, l_avversario_e_del_bioma_e_di_livello_vicino) {
  Pet p;
  makePet(p, 4);  // Charmander: vulcano
  p.ageMinutes = 29 * MINUTES_PER_LEVEL;  // livello 30
  for (uint32_t seed = 1; seed <= 300; seed++) {
    randomSeed(seed);
    uint8_t lvl = 0;
    int16_t d = p.pickWild(lvl);
    CHECK(d >= 1 && d <= DEX_COUNT);
    CHECK_EQ(DEX_TBL[d].biome, DEX_TBL[4].biome);
    CHECK(DEX_TBL[d].rarity != R_LEGENDARIO);
    CHECK(lvl >= 28 && lvl <= 31);
  }
}

TEST(battle, niente_forme_evolute_prima_del_loro_livello) {
  Pet p;
  makePet(p, 1);  // Bulbasaur, bosco, livello 1
  bool visto[DEX_COUNT + 1] = { false };
  for (uint32_t seed = 1; seed <= 500; seed++) {
    randomSeed(seed);
    uint8_t lvl = 0;
    int16_t d = p.pickWild(lvl);
    CHECK(lvl >= 1 && lvl <= 2);
    visto[d] = true;
  }
  CHECK(visto[1] || visto[10] || visto[13]);
  CHECK_MSG(!visto[2] && !visto[3], "niente Ivysaur ne' Venusaur al livello 1");
  CHECK_MSG(!visto[11] && !visto[12], "niente Metapod ne' Butterfree al livello 1");
}

TEST(battle, il_livello_di_battaglia_si_ferma_a_100) {
  Pet p;
  makePet(p, 4);
  p.ageMinutes = 500UL * MINUTES_PER_LEVEL;
  CHECK_EQ(p.battleLevel(), (uint8_t)100);
  uint8_t lvl = 0;
  for (uint32_t seed = 1; seed <= 50; seed++) {
    randomSeed(seed);
    p.pickWild(lvl);
    CHECK(lvl >= 98 && lvl <= 100);
  }
}

TEST(care, pulire_toglie_le_cacche_e_porta_l_igiene_al_massimo) {
  Pet p;
  makePet(p, 4);
  p.poops = 3;
  p.hygiene = 10;
  p.clean();
  CHECK_EQ(p.poops, (uint8_t)0);
  CHECK_EQ(p.hygiene, (uint8_t)100);
}

TEST(care, coccolare_alza_felicita_e_legame) {
  Pet p;
  makePet(p, 4);
  p.joy = 50;
  p.caress();
  CHECK_EQ(p.joy, (uint8_t)55);
  CHECK_EQ(p.bond, (uint8_t)1);
  CHECK(p.showHeart());
}

TEST(care, il_legame_ha_un_tetto_giornaliero) {
  Pet p;
  makePet(p, 4);
  for (int i = 0; i < 50; i++) p.play();  // +2 di legame ogni volta
  CHECK_MSG(p.bond <= 20, "tope de 20 puntos de vinculo al dia");
  CHECK_EQ(p.bond, (uint8_t)20);
}

TEST(mood, riflette_lo_stato) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 80, 80, 80);
  CHECK_EQ((int)p.mood(), (int)MOOD_HAPPY);
  p.joy = 20;
  CHECK_EQ((int)p.mood(), (int)MOOD_SAD);
  p.joy = 80;
  p.feedBerry(0);
  CHECK_EQ((int)p.mood(), (int)MOOD_EATING);
  mockAdvanceMillis(EAT_ANIM_MS + 1);
  p.toggleLight();
  CHECK_EQ((int)p.mood(), (int)MOOD_SLEEPING);
}

// ---------------------------------------------------------------- pokédex
TEST(dexreg, la_bitmap_copre_da_1_a_151) {
  Pet p;
  makePet(p, 4);
  memset(p.dexReg, 0, sizeof(p.dexReg));
  CHECK_EQ(p.registeredCount(), (uint16_t)0);
  for (int16_t d = 1; d <= 151; d++) {
    p.dexReg[(d - 1) >> 3] |= (uint8_t)(1 << ((d - 1) & 7));
    CHECK(p.isRegistered(d));
  }
  CHECK_EQ(p.registeredCount(), (uint16_t)151);
}

TEST(dexreg, fuori_intervallo_non_e_registrato) {
  Pet p;
  makePet(p, 4);
  memset(p.dexReg, 0xFF, sizeof(p.dexReg));
  CHECK(!p.isRegistered(0));
  CHECK(!p.isRegistered(-1));
  CHECK(!p.isRegistered(152));
  CHECK(!p.isRegistered(32767));
  CHECK_EQ(p.registeredCount(), (uint16_t)151);
}

TEST(dexreg, la_linea_incompleta_si_rileva) {
  Pet p;
  makePet(p, 1);  // BULBASAUR -> IVYSAUR -> VENUSAUR
  CHECK(p.lineHasUnregistered(1));
  p.dexReg[(2 - 1) >> 3] |= (uint8_t)(1 << ((2 - 1) & 7));
  p.dexReg[(3 - 1) >> 3] |= (uint8_t)(1 << ((3 - 1) & 7));
  CHECK_MSG(!p.lineHasUnregistered(1), "linea 1-2-3 completa");
}

TEST(dexreg, il_ramo_di_eevee_conta_tutte_e_tre) {
  Pet p;
  makePet(p, DEX_EEVEE);
  CHECK(p.lineHasUnregistered(DEX_EEVEE));
  for (int16_t d = 134; d <= 136; d++)
    p.dexReg[(d - 1) >> 3] |= (uint8_t)(1 << ((d - 1) & 7));
  CHECK(!p.lineHasUnregistered(DEX_EEVEE));
}

TEST(egg, l_estrazione_da_sempre_una_specie_valida) {
  Pet p;
  makePet(p, 4);
  for (int i = 0; i < 400; i++) {
    randomSeed(i * 7919 + 1);
    int16_t d = p.pickEggSpecies();
    CHECK_RANGE(d, (int16_t)1, (int16_t)151);
    CHECK_MSG(DEX_TBL[d].rarity != R_EVO, "de un huevo nunca sale una forma evolucionada");
  }
}

TEST(egg, il_primo_uovo_e_un_iniziale_classico) {
  for (int seed = 1; seed <= 20; seed++) {
    randomSeed(seed);
    mockNvsReset();
    Pet p;
    p.begin();
    int16_t d = p.pickEggSpecies();
    bool classic = false;
    for (int i = 0; i < NUM_CLASSIC_DEX; i++)
      if (CLASSIC_DEX[i] == d) classic = true;
    CHECK_MSG(classic, "sin pokedex, solo salen iniciales clasicos");
  }
}

TEST(egg, una_fuga_penalizza_la_rarita) {
  Pet p;
  makePet(p, 4);
  p.lastEnd = CER_RUNAWAY;
  for (int seed = 1; seed <= 60; seed++) {
    randomSeed(seed);
    int16_t d = p.pickEggSpecies();
    CHECK_MSG(DEX_TBL[d].rarity == R_COMUN, "tras una escapada solo salen comunes");
  }
}

// ---------------------------------------------------------------- serie
TEST(streak, la_prima_cura_del_giorno_conta) {
  Pet p;
  makePet(p, 4);
  p.setClock(100u * 86400);
  p.caress();
  CHECK_EQ(p.streak, (uint16_t)1);
  CHECK_EQ(p.bestStreak, (uint16_t)1);
  p.caress();
  CHECK_MSG(p.streak == 1, "el segundo cuidado del mismo dia no suma");
  p.setClock(101u * 86400);
  p.caress();
  CHECK_EQ(p.streak, (uint16_t)2);
}

TEST(streak, un_giorno_perso_azzera) {
  Pet p;
  makePet(p, 4);
  p.setClock(100u * 86400);
  p.caress();
  p.setClock(101u * 86400);
  p.caress();
  p.setClock(105u * 86400);  // buco di giorni
  p.caress();
  CHECK_EQ(p.streak, (uint16_t)1);
  CHECK_MSG(p.bestStreak == 2, "el record de racha se conserva");
}

TEST(streak, senza_orologio_non_c_e_serie) {
  Pet p;
  makePet(p, 4);
  p.caress();
  CHECK_EQ(p.streak, (uint16_t)0);
}

TEST(streak, festeggia_i_traguardi) {
  Pet p;
  makePet(p, 4);
  for (uint32_t d = 100; d < 103; d++) {
    p.setClock(d * 86400);
    p.caress();
  }
  CHECK_EQ(p.streak, (uint16_t)3);
  CHECK_MSG(p.showMilestone(), "hito de 3 dias celebrado");
  CHECK_EQ(p.lastMilestone, (uint16_t)3);
}

TEST(bonus, serie_e_legame_migliorano_l_uovo) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.careBonus(), 0);
  p.streak = 30;
  p.bond = 100;
  CHECK_EQ(p.careBonus(), 14);
  p.streak = 300;  // la serie è limitata a 30
  CHECK_EQ(p.careBonus(), 14);
}

// ---------------------------------------------------------------- medaglie
TEST(medals, il_livello_10_da_una_medaglia) {
  Pet p;
  makePet(p, 4);
  CHECK(!p.hasMedal(MED_LV10));
  p.ageMinutes = 9 * 60 - 1;
  advance(p, 1);
  CHECK(p.hasMedal(MED_LV10));
  CHECK(p.showMedal());
}

// senza sovrappeso, livello 5 e zero trascuratezze
TEST(medals, in_forma_arriva_al_livello_5) {
  Pet p;
  makePet(p, 4);
  p.weight = 0;
  p.ageMinutes = 4 * 60 - 1;
  advance(p, 1);
  CHECK(p.hasMedal(MED_FIT));
  CHECK_EQ(p.totalMedals, (uint16_t)1);

  Pet q;
  makePet(q, 4);
  q.careMistakes = 1;   // una trascuratezza la lascia fuori
  q.ageMinutes = 4 * 60 - 1;
  advance(q, 1);
  CHECK(!q.hasMedal(MED_FIT));
}

TEST(medals, ogni_medaglia_nuova_si_somma_al_totale) {
  Pet p;
  makePet(p, 4);
  p.weight = 0;
  p.ageMinutes = 9 * 60 - 1;
  advance(p, 1);  // supera il livello 10: MED_LV10 + MED_FIT insieme
  CHECK(p.hasMedal(MED_LV10));
  CHECK(p.hasMedal(MED_FIT));
  CHECK_MSG(p.totalMedals == 2, "el total cuenta cada bit ganado, no cada evento");
}

TEST(medals, forma_finale_alla_nascita) {
  Pet p;
  makePet(p, 143);  // SNORLAX è già in forma finale
  CHECK(p.hasMedal(MED_FINAL));
  CHECK_EQ(p.totalMedals, (uint16_t)1);
}

TEST(medals, la_bacca_preferita_da_una_medaglia) {
  Pet p;
  makePet(p, 4);
  p.feedBerry(1);  // la sua preferita
  CHECK(p.berryKnown);
  advance(p, 1);
  CHECK(p.hasMedal(MED_BERRY));
}

TEST(medals, il_totale_si_accumula_tra_gli_allevamenti) {
  Pet p;
  makePet(p, 143);
  CHECK_EQ(p.totalMedals, (uint16_t)1);
  p.newEgg();
  p.chooseStarter(144);  // ARTICUNO, un'altra forma finale
  p.eggTap(); p.eggTap(); p.eggTap();
  CHECK_MSG(p.medals == MED_FINAL, "las medallas del individuo se reinician");
  CHECK_MSG(p.totalMedals == 2, "el total del jugador se conserva");
}

// ---------------------------------------------------------------- cerimonie
TEST(ceremony, il_congedo_solo_in_forma_finale_e_dopo_3_giorni) {
  Pet p;
  makePet(p, 6);  // CHARIZARD, forma finale
  p.ageMinutes = FAREWELL_AGE_MIN - 1;
  CHECK(!p.canFarewellNow());
  p.ageMinutes = FAREWELL_AGE_MIN;
  CHECK(p.canFarewellNow());

  Pet q;
  makePet(q, 4);  // CHARMANDER non è in forma finale
  q.ageMinutes = FAREWELL_AGE_MIN * 3;
  CHECK(!q.canFarewellNow());
}

TEST(ceremony, il_congedo_lascia_un_uovo_nuovo) {
  Pet p;
  makePet(p, 6);
  p.ageMinutes = FAREWELL_AGE_MIN;
  p.startFarewell();
  CHECK_EQ(p.ceremony, (uint8_t)CER_FAREWELL);
  CHECK_RANGE(p.ceremonyT(), 0.0f, 1.0f);
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.colChiede());  // forma finale: prima la domanda della collezione
  p.colRinuncia();
  CHECK(p.isEgg());
  CHECK_EQ(p.ceremony, (uint8_t)CER_NONE);
  CHECK_EQ(p.lastEnd, (uint8_t)CER_FAREWELL);
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
}

TEST(ceremony, la_fuga_richiede_un_ora_di_abbandono_totale) {
  Pet p;
  makePet(p, 4);
  setStats(p, 0, 0, 0, 0);
  CHECK(!p.canRunawayNow());
  advance(p, RUNAWAY_TICKS - 1);
  CHECK(!p.canRunawayNow());
  advance(p, 1);
  CHECK(p.canRunawayNow());
}

TEST(ceremony, una_sola_cura_salva_dall_abbandono) {
  Pet p;
  makePet(p, 4);
  p.dbgRunawayReady();
  CHECK(p.canRunawayNow());
  p.feedBerry(0);
  advance(p, 1);
  CHECK_MSG(!p.canRunawayNow(), "comer resetea el contador de abandono");
}

TEST(ceremony, durante_la_cerimonia_non_si_puo_interagire) {
  Pet p;
  makePet(p, 4);
  setStats(p, 50, 50, 50, 50);
  p.poops = 2;
  p.startRunaway();
  p.feedBerry(0);
  p.feedCandy();
  p.play();
  p.clean();
  p.caress();
  p.toggleLight();
  CHECK_EQ(p.fullness, (uint8_t)50);
  CHECK_EQ(p.joy, (uint8_t)50);
  CHECK_EQ(p.poops, (uint8_t)2);
  CHECK(!p.sleeping);
}

TEST(ceremony, liberare_segna_il_finale_come_liberazione) {
  Pet p;
  makePet(p, 4);
  p.release();
  CHECK_EQ(p.ceremony, (uint8_t)CER_RELEASE);
  CHECK_EQ(p.lastEnd, (uint8_t)CER_RELEASE);
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.isEgg());
}

TEST(ceremony, un_uovo_non_si_congeda_ne_si_libera) {
  mockNvsReset();
  Pet p;
  p.begin();
  p.startFarewell();
  p.release();
  p.startRunaway();
  CHECK_EQ(p.ceremony, (uint8_t)CER_NONE);
}

// ---------------------------------------------------------------- offline
TEST(offline, applica_il_tempo_da_spento_con_minimo_di_15) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 80, 80, 100);
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 600 * 60);  // 600 minuti da spento
  CHECK_EQ(p.ageMinutes, (uint32_t)600);
  CHECK_MSG(p.fullness == 15, "suelo de comida offline = 15");
  CHECK_MSG(p.energy == 15, "suelo de energia offline = 15");
  CHECK_MSG(p.joy == 15, "suelo de felicidad offline = 15");
  CHECK_MSG(p.hygiene == 15, "suelo de higiene offline = 15");
  CHECK_MSG(p.careMistakes == 0, "en ausencia no se acumulan descuidos");
  CHECK_EQ(p.poops, (uint8_t)2);  // una cacca ogni 4 h
}

TEST(offline, tetto_di_due_settimane) {
  Pet p;
  makePet(p, 4);
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 90u * 86400);  // 90 giorni
  CHECK_EQ(p.ageMinutes, (uint32_t)(14u * 24 * 60));
}

TEST(offline, meno_di_due_minuti_non_fa_nulla) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 80, 80, 100);
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 90);
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
  CHECK_EQ(p.fullness, (uint8_t)80);
}

TEST(offline, l_uovo_si_schiude_in_tua_assenza) {
  Pet p;
  makePet(p, 4);
  p.newEgg();  // c'è già il pokédex: non chiede l'iniziale
  CHECK(!p.awaitingStarter());
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 600);  // 10 minuti
  CHECK(!p.isEgg());
  CHECK_RANGE(p.speciesId, (int16_t)1, (int16_t)151);
}

TEST(offline, dormendo_si_riposa_anche_da_spento) {
  Pet p;
  makePet(p, 4);
  setStats(p, 90, 90, 20, 90);
  p.toggleLight();
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 120 * 60);
  CHECK_EQ(p.energy, (uint8_t)100);
  CHECK_MSG(p.fullness >= 30, "suelos de sueno tambien offline");
  CHECK_MSG(p.poops == 0, "durmiendo no ensucia");
}

TEST(offline, senza_orologio_non_si_applica_nulla) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 80, 80, 100);
  p.syncClock(0);
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
  CHECK_EQ(p.fullness, (uint8_t)80);
}

// ---------------------------------------------------------------- salvataggio
TEST(save, lo_stato_sopravvive_a_un_riavvio) {
  Pet a;
  makePet(a, 25);  // PIKACHU
  a.ageMinutes = 1234;
  a.streak = 9;
  a.bestStreak = 11;
  a.bond = 44;
  a.trAtk = 17;
  a.weight = 33;
  a.gameHi = 21;
  a.rename("SPARKY");  // rename persiste tutto lo stato

  Pet b;
  b.begin();  // stessa "NVS": simula un riavvio
  CHECK_EQ(b.speciesId, (int16_t)25);
  CHECK_EQ(b.ageMinutes, (uint32_t)1234);
  CHECK_EQ(b.streak, (uint16_t)9);
  CHECK_EQ(b.bestStreak, (uint16_t)11);
  CHECK_EQ(b.bond, (uint8_t)44);
  CHECK_EQ(b.trAtk, (uint8_t)17);
  CHECK_EQ(b.weight, (uint8_t)33);
  CHECK_EQ(b.gameHi, (uint16_t)21);
  CHECK_STREQ(b.nick, "SPARKY");
  CHECK(b.isRegistered(25));
  CHECK(!b.awaitingStarter());
}

TEST(save, il_soprannome_si_tronca_a_11_caratteri) {
  Pet p;
  makePet(p, 4);
  p.rename("NOMBRE-LARGUISIMO-QUE-NO-CABE");
  CHECK_EQ((int)strlen(p.nick), 11);
  CHECK_STREQ(p.nick, "NOMBRE-LARG");
}

TEST(save, il_ripristino_di_fabbrica_lascia_tutto_come_nuovo) {
  Pet a;
  makePet(a, 4);
  a.factoryReset();
  Pet b;
  b.begin();
  CHECK(b.isEgg());
  CHECK(b.awaitingStarter());
  CHECK_EQ(b.registeredCount(), (uint16_t)0);
}

TEST(save, l_autosalvataggio_si_segna_e_si_scrive) {
  Pet p;
  makePet(p, 4);
  CHECK(!p.savePending());
  advance(p, 5);
  CHECK_MSG(p.savePending(), "cada 5 ticks queda un guardado pendiente");
  p.flushSave();
  CHECK(!p.savePending());
}

// riscrive a mano il blob dello stato nella NVS finta, con un CRC valido: come un
// salvataggio con valori assurdi (bug, importazione) che load() deve raddrizzare
static void corruptSave(void (*edit)(PetSave &)) {
  Preferences raw;
  raw.begin("pokegotchi", false);
  PetSave s;
  CHECK_EQ(raw.getBytes(PET_SAVE_KEY, &s, sizeof(s)), sizeof(s));
  edit(s);
  s.crc = saveCrc32((const uint8_t *)&s, sizeof(s) - sizeof(s.crc));
  raw.putBytes(PET_SAVE_KEY, &s, sizeof(s));
  raw.end();
}

TEST(save, una_specie_fuori_intervallo_diventa_un_uovo) {
  Pet a;
  makePet(a, 25);
  a.ageMinutes = 600;
  a.rename("SPARKY");
  corruptSave([](PetSave &s) { s.speciesId = 500; });
  Pet b;
  b.begin();
  CHECK(b.isEgg());
  CHECK(b.isRegistered(25));  // il pokédex già allevato resta
  CHECK_EQ(b.ageMinutes, (uint32_t)0);
  corruptSave([](PetSave &s) { s.speciesId = -7; });
  Pet c;
  c.begin();
  CHECK(c.isEgg());
}

TEST(save, un_uovo_con_specie_nascosta_non_valida_si_schiude_bene) {
  Pet a;
  makePet(a, 4);
  corruptSave([](PetSave &s) {
    s.speciesId = -1;
    s.eggTarget = 999;
    s.eggTaps = 40;
  });
  Pet b;
  b.begin();
  CHECK(b.isEgg());
  CHECK(b.eggCracks() < 3);
  b.eggTap(); b.eggTap(); b.eggTap();
  CHECK(!b.isEgg());
  CHECK(b.speciesId >= 1 && b.speciesId <= DEX_COUNT);
}

TEST(save, i_campi_fuori_intervallo_tornano_validi) {
  Pet a;
  makePet(a, 4);
  corruptSave([](PetSave &s) {
    s.fullness = 250;
    s.joy = 101;
    s.energy = 200;
    s.hygiene = 255;
    s.poops = 9;
    s.weight = 180;
    s.geneAtk = 255;
    s.geneDef = 3;
    s.trSpe = 222;
    s.bond = 255;
    s.lastEnd = 77;
    s.streak = 30;
    s.bestStreak = 2;
    s.medals = 0xFFFF;
  });
  Pet b;
  b.begin();
  CHECK_EQ(b.speciesId, (int16_t)4);  // specie valida: resta com'è
  CHECK_EQ(b.fullness, (uint8_t)100);
  CHECK_EQ(b.joy, (uint8_t)100);
  CHECK_EQ(b.energy, (uint8_t)100);
  CHECK_EQ(b.hygiene, (uint8_t)100);
  CHECK_EQ(b.weight, (uint8_t)100);
  CHECK_EQ(b.trSpe, (uint8_t)100);
  CHECK_EQ(b.bond, (uint8_t)100);
  CHECK_EQ(b.poops, (uint8_t)3);
  CHECK_EQ(b.geneAtk, (uint8_t)110);
  CHECK_EQ(b.geneDef, (uint8_t)90);
  CHECK_EQ(b.lastEnd, (uint8_t)CER_NONE);
  CHECK_EQ(b.bestStreak, (uint16_t)30);
  CHECK_EQ(b.medals, (uint16_t)((1 << MED_COUNT) - 1));
  advance(b, 5);  // e il gioco va avanti senza leggere fuori dalle tabelle
  CHECK(b.fullness <= 100);
}

// ---------------------------------------------------------------- blob dello stato
static const uint32_t T0 = 1760000000;

static std::vector<uint8_t> readBlob() {
  Preferences raw;
  raw.begin("pokegotchi", true);
  std::vector<uint8_t> v(raw.getBytesLength(PET_SAVE_KEY));
  if (!v.empty()) raw.getBytes(PET_SAVE_KEY, &v[0], v.size());
  raw.end();
  return v;
}

// scrive `v` come blob dello stato; con `fixCrc` riscrive prima size e CRC in coda
static void writeBlob(std::vector<uint8_t> v, bool fixCrc) {
  if (fixCrc) {
    v[2] = (uint8_t)v.size();
    v[3] = (uint8_t)(v.size() >> 8);
    uint32_t c = saveCrc32(&v[0], v.size() - 4);
    for (int i = 0; i < 4; i++) v[v.size() - 4 + i] = (uint8_t)(c >> (8 * i));
  }
  Preferences raw;
  raw.begin("pokegotchi", false);
  raw.putBytes(PET_SAVE_KEY, &v[0], v.size());
  raw.end();
}

// Chiavi singole come le scriveva Pet::save() fino alla 1.18. Usata anche da test_save.cpp.
void writeLegacyKeys() {
  Preferences raw;
  raw.begin("pokegotchi", false);
  raw.putBool("init", true);
  raw.putUChar("full", 61);
  raw.putUChar("joy", 62);
  raw.putUChar("ene", 63);
  raw.putUChar("hyg", 64);
  raw.putUChar("poop", 1);
  raw.putUChar("wgt", 20);
  raw.putUChar("gatk", 101);
  raw.putUChar("gdef", 102);
  raw.putUChar("gspe", 103);
  raw.putUChar("tatk", 5);
  raw.putUChar("tdef", 6);
  raw.putUChar("tspe", 7);
  raw.putBool("bk", true);
  raw.putBool("shy", true);
  raw.putBool("eshy", false);
  raw.putBool("stpk", false);
  uint8_t dex[19] = { 0x01 };  // BULBASAUR
  dex[3] = 0x01;               // 25 PIKACHU
  raw.putBytes("dexreg", dex, sizeof(dex));
  raw.putBytes("dexsh", dex, sizeof(dex));
  raw.putUInt("age", 2500);
  raw.putShort("dexn", 25);
  raw.putShort("eggT2", 7);
  raw.putUChar("crack", 0);
  raw.putUChar("mist", 2);
  raw.putBool("sleep", false);
  raw.putUChar("lend", CER_RUNAWAY);
  raw.putUInt("seen", T0);
  raw.putUShort("strk", 4);
  raw.putUShort("bstrk", 8);
  raw.putUInt("cday", 20300);
  raw.putUChar("bond", 50);
  raw.putUShort("medal", MED_LV10);
  raw.putUShort("tmedal", 3);
  raw.putUShort("mstone", 0);
  raw.putUShort("ghi", 12);
  raw.putUShort("shi", 34);
  raw.putString("nick", "PIKA");
  raw.end();
}

// lo stato scritto da writeLegacyKeys(), letto da un Pet
void checkLegacyPet(const Pet &p) {
  CHECK_EQ(p.speciesId, (int16_t)25);
  CHECK_EQ(p.ageMinutes, (uint32_t)2500);
  CHECK_EQ(p.fullness, (uint8_t)61);
  CHECK_EQ(p.hygiene, (uint8_t)64);
  CHECK_EQ(p.weight, (uint8_t)20);
  CHECK_EQ(p.geneAtk, (uint8_t)101);
  CHECK_EQ(p.geneSpe, (uint8_t)103);
  CHECK_EQ(p.trSpe, (uint8_t)7);
  CHECK(p.berryKnown);
  CHECK(p.shiny);
  CHECK_EQ(p.careMistakes, (uint8_t)2);
  CHECK_EQ(p.lastEnd, (uint8_t)CER_RUNAWAY);
  CHECK(p.isRegistered(1) && p.isRegistered(25) && p.isShinyRegistered(25));
  CHECK_EQ(p.registeredCount(), (uint16_t)2);
  CHECK_EQ(p.bestStreak, (uint16_t)8);
  CHECK_EQ(p.lastCareDay, (uint32_t)20300);
  CHECK_EQ(p.bond, (uint8_t)50);
  CHECK_EQ(p.medals, (uint16_t)MED_LV10);
  CHECK_EQ(p.totalMedals, (uint16_t)3);
  CHECK_EQ(p.strHi, (uint16_t)34);
  CHECK_STREQ(p.nick, "PIKA");
  CHECK(!p.awaitingStarter());
}

TEST(save, il_blob_conserva_tutti_i_campi) {
  Pet a;
  makePet(a, 133);
  setStats(a, 11, 22, 33, 44);
  a.poops = 2;
  a.weight = 55;
  a.trAtk = 1;
  a.trDef = 2;
  a.trSpe = 3;
  a.berryKnown = true;
  a.shiny = true;
  a.ageMinutes = 4321;
  a.careMistakes = 3;
  a.sleeping = true;
  a.lastEnd = CER_FAREWELL;
  a.dexReg[5] = 0xA5;
  a.dexShinyReg[2] = 0x11;
  a.dexShinyReg[(133 - 1) >> 3] |= 1 << ((133 - 1) & 7);  // shiny: load() lo registrerebbe
  a.streak = 7;
  a.bestStreak = 9;
  a.lastCareDay = 20000;
  a.bond = 66;
  a.medals = MED_LV10 | MED_BERRY;
  a.totalMedals = 12;
  a.lastMilestone = 7;
  a.gameHi = 99;
  a.strHi = 77;
  a.winsToday = 3;
  a.winsDay = 20001;
  a.rename("EEVEE-1");  // salva
  std::vector<uint8_t> prima = readBlob();
  CHECK_EQ(prima.size(), sizeof(PetSave));
  CHECK_EQ(mockNvsKeyCount("pokegotchi"), (size_t)2);  // "init" e il blob: niente chiavi singole

  Pet b;
  b.begin();
  CHECK_EQ(b.speciesId, (int16_t)133);
  CHECK_EQ(b.fullness, (uint8_t)11);
  CHECK_EQ(b.hygiene, (uint8_t)44);
  CHECK_EQ(b.poops, (uint8_t)2);
  CHECK_EQ(b.trDef, (uint8_t)2);
  CHECK(b.berryKnown && b.shiny && b.sleeping);
  CHECK_EQ(b.ageMinutes, (uint32_t)4321);
  CHECK_EQ(b.geneAtk, a.geneAtk);
  CHECK_EQ(b.lastEnd, (uint8_t)CER_FAREWELL);
  CHECK_EQ(b.dexReg[5], (uint8_t)0xA5);
  CHECK_EQ(b.dexShinyReg[2], (uint8_t)0x11);
  CHECK_EQ(b.lastCareDay, (uint32_t)20000);
  CHECK_EQ(b.medals, (uint16_t)(MED_LV10 | MED_BERRY));
  CHECK_EQ(b.lastMilestone, (uint16_t)7);
  CHECK_EQ(b.strHi, (uint16_t)77);
  CHECK_STREQ(b.nick, "EEVEE-1");
  CHECK_EQ(b.winsToday, (uint8_t)3);
  CHECK_EQ(b.winsDay, (uint32_t)20001);
  // risalvato, il blob è lo stesso byte per byte, anche nei campi privati (uovo, iniziale);
  // tranne i tocchi dell'uovo, che dopo la schiusa restano 3 e sanitize() porta a 2
  b.setClock(T0);
  prima[offsetof(PetSave, eggTaps)] = 2;
  uint32_t c = saveCrc32(&prima[0], prima.size() - 4);
  for (int i = 0; i < 4; i++) prima[prima.size() - 4 + i] = (uint8_t)(c >> (8 * i));
  CHECK(readBlob() == prima);
}

TEST(save, le_chiavi_singole_della_1_18_migrano_nel_blob) {
  mockNvsReset();
  mockSetMillis(0);
  writeLegacyKeys();
  Pet b;
  b.begin();
  checkLegacyPet(b);
  // restano "init", "seen" e il blob; le chiavi singole sono state tolte
  Preferences raw;
  raw.begin("pokegotchi", true);
  CHECK_EQ(readBlob().size(), sizeof(PetSave));
  CHECK(!raw.isKey("dexn") && !raw.isKey("full") && !raw.isKey("nick"));
  CHECK_EQ(raw.getUInt("seen", 0), T0);
  raw.end();
  CHECK_EQ(mockNvsKeyCount("pokegotchi"), (size_t)3);
  Pet c;  // e al riavvio successivo si legge il blob
  c.begin();
  checkLegacyPet(c);
}

TEST(save, un_blob_con_crc_errato_ripiega_sulle_chiavi_legacy) {
  mockNvsReset();
  mockSetMillis(0);
  writeLegacyKeys();
  PetSave s;
  s.speciesId = 150;
  std::vector<uint8_t> v((uint8_t *)&s, (uint8_t *)&s + sizeof(s));  // CRC 0: sbagliato
  writeBlob(v, false);
  Pet b;
  b.begin();
  checkLegacyPet(b);
  Pet c;  // il blob è stato riscritto, valido
  c.begin();
  checkLegacyPet(c);
}

TEST(save, un_blob_con_crc_errato_senza_legacy_diventa_un_uovo_col_pokedex) {
  Pet a;
  makePet(a, 25);
  a.ageMinutes = 600;
  a.rename("SPARKY");
  std::vector<uint8_t> v = readBlob();
  v[offsetof(PetSave, ageMinutes)] ^= 0x01;  // un bit cambiato, CRC non più valido
  writeBlob(v, false);
  Pet b;
  b.begin();
  CHECK(b.isEgg());
  CHECK_EQ(b.ageMinutes, (uint32_t)0);
  CHECK_STREQ(b.nick, "");
  CHECK(b.isRegistered(25));  // il pokédex già allevato resta
  CHECK(!b.awaitingStarter());
  Pet c;  // il nuovo uovo è stato salvato
  c.begin();
  CHECK(c.isEgg());
  CHECK(c.isRegistered(25));
}

TEST(save, un_blob_di_versione_futura_carica_il_prefisso_noto) {
  Pet a;
  makePet(a, 25);
  a.ageMinutes = 777;
  a.rename("SPARKY");
  std::vector<uint8_t> v = readBlob();
  // firmware più nuovo (downgrade): 8 byte di campi sconosciuti prima del CRC
  v.resize(v.size() - 4);
  v[0] = PET_SAVE_VERSION + 1;
  for (int i = 0; i < 8 + 4; i++) v.push_back(0xEE);
  writeBlob(v, true);
  Pet b;
  b.begin();
  CHECK_EQ(b.speciesId, (int16_t)25);
  CHECK_EQ(b.ageMinutes, (uint32_t)777);
  CHECK_STREQ(b.nick, "SPARKY");
  CHECK_EQ(b.geneAtk, a.geneAtk);
}

TEST(save, un_blob_di_versione_precedente_lascia_ai_default_i_campi_nuovi) {
  Pet a;
  makePet(a, 25);
  a.strHi = 55;
  a.rename("SPARKY");
  std::vector<uint8_t> v = readBlob();
  // un blob che finiva prima di `nick`: il campo nuovo prende il default
  v.resize(offsetof(PetSave, nick) + 4);
  writeBlob(v, true);
  Pet b;
  b.begin();
  CHECK_EQ(b.speciesId, (int16_t)25);
  CHECK_EQ(b.strHi, (uint16_t)55);
  CHECK_STREQ(b.nick, "");
}

TEST(save, un_blob_di_lunghezza_incoerente_non_si_legge) {
  Pet a;
  makePet(a, 25);
  a.rename("SPARKY");
  std::vector<uint8_t> v = readBlob();
  v[2] ^= 0x10;  // size non coincide con la lunghezza, anche con un CRC valido
  uint32_t c = saveCrc32(&v[0], v.size() - 4);
  for (int i = 0; i < 4; i++) v[v.size() - 4 + i] = (uint8_t)(c >> (8 * i));
  writeBlob(v, false);
  Pet b;
  b.begin();
  CHECK(b.isEgg());
  CHECK_STREQ(b.nick, "");
}

TEST(save, un_blob_della_versione_1_si_carica_senza_vittorie_del_giorno) {
  Pet a;
  makePet(a, 25);
  a.strHi = 55;
  a.winsToday = 4;
  a.winsDay = 123;
  a.rename("SPARKY");
  std::vector<uint8_t> v = readBlob();
  // il blob della 1.18 (versione 1, 105 byte): finiva con `nick`
  v.resize(offsetof(PetSave, winsToday) + 4);
  v[0] = 1;
  writeBlob(v, true);
  Pet b;
  b.begin();
  CHECK_EQ(b.speciesId, (int16_t)25);
  CHECK_EQ(b.strHi, (uint16_t)55);
  CHECK_STREQ(b.nick, "SPARKY");
  CHECK_EQ(b.winsToday, (uint8_t)0);
  CHECK_EQ(b.winsDay, (uint32_t)0);
}

// ---------------------------------------------------------------- collezione
// forma finale pronta all'addio, con l'orologio (per il limite giornaliero di RIPRENDI)
static void makeFinale(Pet &p, int16_t dex) {
  makePet(p, dex);
  p.setClock(T0);
  p.ageMinutes = FAREWELL_AGE_MIN;
}

// fine della cerimonia in corso
static void fineCerimonia(Pet &p) {
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
}

// un ritratto qualsiasi, valido
static PetRitratto ritrattoDi(int16_t dex, const char *nick) {
  PetRitratto r;
  r.specie = dex;
  r.ageMinutes = 5000;
  r.geneAtk = 95;
  r.trSpe = 12;
  r.bond = 40;
  r.medals = MED_FINAL;
  strncpy(r.nick, nick, sizeof(r.nick) - 1);
  return r;
}

TEST(ritratto, andata_e_ritorno_con_tutti_i_campi) {
  PetRitratto a;
  a.specie = 150;
  a.shiny = true;
  a.ageMinutes = 0x01020304;
  a.geneAtk = 90;
  a.geneDef = 101;
  a.geneSpe = 110;
  a.trAtk = 1;
  a.trDef = 2;
  a.trSpe = 100;
  a.bond = 77;
  a.medals = MED_LV10 | MED_FINAL;
  strcpy(a.nick, "MEWTWO-1234");
  a.otId = 0xA1B2C3D4;
  strcpy(a.otNome, "ROSSO");
  uint8_t buf[RIT_BYTES];
  CHECK_EQ(ritrattoScrivi(a, buf, sizeof(buf) - 1), (size_t)0);
  CHECK_EQ(ritrattoScrivi(a, buf, sizeof(buf)), (size_t)RIT_BYTES);
  // little-endian esplicito, agli offset di ritratto.h
  CHECK_EQ(buf[0], (uint8_t)RIT_VERSIONE);
  CHECK_EQ(buf[1], (uint8_t)RIT_BYTES);
  CHECK_EQ(buf[2], (uint8_t)150);
  CHECK_EQ(buf[5], (uint8_t)0x04);
  CHECK_EQ(buf[8], (uint8_t)0x01);
  CHECK_EQ(buf[30], (uint8_t)0xD4);
  CHECK_EQ(saveCrc32(buf, 46), (uint32_t)(buf[46] | buf[47] << 8 | buf[48] << 16 | (uint32_t)buf[49] << 24));
  PetRitratto b;
  CHECK_EQ(ritrattoLeggi(buf, sizeof(buf), &b), (size_t)RIT_BYTES);
  CHECK_EQ(b.specie, (int16_t)150);
  CHECK(b.shiny);
  CHECK_EQ(b.ageMinutes, (uint32_t)0x01020304);
  CHECK_EQ(b.geneAtk, (uint8_t)90);
  CHECK_EQ(b.geneDef, (uint8_t)101);
  CHECK_EQ(b.geneSpe, (uint8_t)110);
  CHECK_EQ(b.trSpe, (uint8_t)100);
  CHECK_EQ(b.bond, (uint8_t)77);
  CHECK_EQ(b.medals, (uint16_t)(MED_LV10 | MED_FINAL));
  CHECK_STREQ(b.nick, "MEWTWO-1234");
  CHECK_EQ(b.otId, (uint32_t)0xA1B2C3D4);
  CHECK_STREQ(b.otNome, "ROSSO");
}

TEST(ritratto, crc_rovinato_troncato_o_specie_assurda_si_rifiutano) {
  uint8_t buf[RIT_BYTES];
  ritrattoScrivi(ritrattoDi(6, "ZARD"), buf, sizeof(buf));
  PetRitratto r;
  CHECK_EQ(ritrattoLeggi(buf, RIT_BYTES - 1, &r), (size_t)0);  // troncato
  buf[20] ^= 0x01;
  CHECK_EQ(ritrattoLeggi(buf, sizeof(buf), &r), (size_t)0);    // CRC errato
  CHECK_EQ(r.specie, (int16_t)-1);  // e non tocca l'uscita
  PetRitratto x = ritrattoDi(6, "ZARD");
  x.specie = 152;
  ritrattoScrivi(x, buf, sizeof(buf));
  CHECK_EQ(ritrattoLeggi(buf, sizeof(buf), &r), (size_t)0);
  buf[0] = 0;  // versione 0: non è un ritratto
  CHECK_EQ(ritrattoLeggi(buf, sizeof(buf), &r), (size_t)0);
}

TEST(ritratto, una_versione_futura_si_legge_per_il_prefisso) {
  uint8_t buf[RIT_BYTES + 6];
  ritrattoScrivi(ritrattoDi(143, "SNORLAX"), buf, sizeof(buf));
  // 6 byte di campi nuovi prima del CRC, lunghezza e CRC aggiornati
  memset(buf + 46, 0xEE, 6);
  buf[0] = RIT_VERSIONE + 1;
  buf[1] = RIT_BYTES + 6;
  uint32_t c = saveCrc32(buf, RIT_BYTES + 2);
  for (int i = 0; i < 4; i++) buf[RIT_BYTES + 2 + i] = (uint8_t)(c >> (8 * i));
  PetRitratto r;
  CHECK_EQ(ritrattoLeggi(buf, sizeof(buf), &r), (size_t)(RIT_BYTES + 6));
  CHECK_EQ(r.specie, (int16_t)143);
  CHECK_STREQ(r.nick, "SNORLAX");
}

TEST(ritratto, dal_pet_attivo_e_ritorno) {
  Pet p;
  makePet(p, 25);
  p.ageMinutes = 999;
  p.trAtk = 30;
  p.bond = 55;
  p.shiny = true;
  p.rename("SPARKY");
  PetRitratto r = p.ritratto();
  CHECK_EQ(r.specie, (int16_t)25);
  CHECK_EQ(r.ageMinutes, (uint32_t)999);
  CHECK_EQ(r.geneAtk, p.geneAtk);
  CHECK_EQ(r.otId, (uint32_t)0);  // allevato qui: l'allenatore sono io

  Pet q;
  makePet(q, 4);
  setStats(q, 5, 5, 5, 5);
  q.poops = 3;
  q.careMistakes = 4;
  q.sleeping = true;
  q.daRitratto(r);
  CHECK_EQ(q.speciesId, (int16_t)25);
  CHECK(q.shiny);
  CHECK_EQ(q.level(), p.level());
  CHECK_EQ(q.geneDef, p.geneDef);
  CHECK_EQ(q.trAtk, (uint8_t)30);
  CHECK_EQ(q.bond, (uint8_t)55);
  CHECK_STREQ(q.nick, "SPARKY");
  // bisogni e trascuratezze ripartono neutri
  CHECK_EQ(q.fullness, (uint8_t)80);
  CHECK_EQ(q.hygiene, (uint8_t)100);
  CHECK_EQ(q.poops, (uint8_t)0);
  CHECK_EQ(q.careMistakes, (uint8_t)0);
  CHECK(!q.sleeping);
  CHECK(q.isRegistered(25) && q.isShinyRegistered(25));
}

TEST(collezione, l_addio_di_una_forma_finale_chiede_e_conserva) {
  Pet p;
  makeFinale(p, 6);
  p.rename("ZARD");
  p.startFarewell();
  CHECK(!p.colChiede());  // non durante la cerimonia
  fineCerimonia(p);
  CHECK(p.colChiede());
  CHECK(!p.isEgg());
  CHECK_EQ(p.ceremony, (uint8_t)CER_FAREWELL);  // ferma all'ultimo fotogramma
  CHECK_EQ(p.ceremonyT(), 1.0f);
  CHECK(p.colConserva());
  CHECK(p.isEgg());
  CHECK(!p.colChiede());
  CHECK_EQ(p.ceremony, (uint8_t)CER_NONE);
  CHECK_EQ(p.lastEnd, (uint8_t)CER_FAREWELL);  // l'uovo ha la benedizione dell'addio
  CHECK_EQ(p.colNumero(), (uint8_t)1);
  CHECK_EQ(p.colRitratto(0).specie, (int16_t)6);
  CHECK_STREQ(p.colRitratto(0).nick, "ZARD");
  CHECK_EQ(p.colRitratto(0).ageMinutes, (uint32_t)FAREWELL_AGE_MIN);
}

TEST(collezione, entra_anche_la_forma_finale_rilasciata) {
  Pet p;
  makePet(p, 143);
  p.release();
  fineCerimonia(p);
  CHECK(p.colChiede());
  CHECK_EQ(p.ceremony, (uint8_t)CER_RELEASE);
  p.colRinuncia();
  CHECK(p.isEgg());
  CHECK_EQ(p.colNumero(), (uint8_t)0);
}

TEST(collezione, fuga_e_rilascio_di_forme_non_finali_non_entrano) {
  Pet p;
  makePet(p, 4);
  p.release();
  fineCerimonia(p);
  CHECK(!p.colChiede());
  CHECK(p.isEgg());

  Pet q;
  makePet(q, 6);
  q.dbgRunawayReady();
  q.startRunaway();
  fineCerimonia(q);
  CHECK(!q.colChiede());
  CHECK(q.isEgg());
}

TEST(collezione, durante_la_domanda_il_tempo_e_fermo) {
  Pet p;
  makeFinale(p, 6);
  setStats(p, 50, 50, 50, 50);
  p.startFarewell();
  fineCerimonia(p);
  CHECK(p.colChiede());
  advance(p, 600);
  CHECK(p.colChiede());
  CHECK_EQ(p.ageMinutes, (uint32_t)FAREWELL_AGE_MIN);
  CHECK_EQ(p.fullness, (uint8_t)50);
  p.feedBerry(0);
  p.clean();
  CHECK_EQ(p.fullness, (uint8_t)50);
  CHECK(!p.canBattle());
  CHECK(!p.canFarewellNow());
  // la progressione offline non si applica
  p.syncClock(T0 + 6 * 3600);
  CHECK_EQ(p.ageMinutes, (uint32_t)FAREWELL_AGE_MIN);
}

TEST(collezione, la_domanda_sopravvive_a_un_riavvio) {
  Pet a;
  makeFinale(a, 6);
  a.release();
  fineCerimonia(a);
  CHECK(a.colChiede());

  mockSetMillis(0);
  Pet b;
  b.begin();
  b.syncClock(T0 + 3600);
  CHECK(b.colChiede());
  CHECK_EQ(b.speciesId, (int16_t)6);
  CHECK_EQ(b.ceremony, (uint8_t)CER_RELEASE);
  CHECK_EQ(b.ceremonyT(), 1.0f);
  advance(b, 30);
  CHECK(b.colChiede());
  CHECK_EQ(b.ageMinutes, (uint32_t)FAREWELL_AGE_MIN);
  CHECK(b.colConserva());
  CHECK(b.isEgg());

  Pet c;  // risposta salvata: niente più domanda, il pet è in collezione
  c.begin();
  CHECK(!c.colChiede());
  CHECK(c.isEgg());
  CHECK_EQ(c.ceremony, (uint8_t)CER_NONE);
  CHECK_EQ(c.colNumero(), (uint8_t)1);
}

TEST(collezione, piena_si_sceglie_chi_liberare_o_si_rinuncia) {
  Pet p;
  makeFinale(p, 6);
  const int16_t dex[COL_POSTI] = { 3, 9, 65, 68, 94 };
  for (uint8_t i = 0; i < COL_POSTI; i++) CHECK(p.colDeposita(ritrattoDi(dex[i], "")));
  CHECK(p.colPiena());
  CHECK(!p.colDeposita(ritrattoDi(130, "")));
  p.startFarewell();
  fineCerimonia(p);
  CHECK(!p.colConserva());     // piena: serve il posto da liberare
  CHECK(!p.colConserva(COL_POSTI));
  CHECK(p.colChiede());        // non cambia nulla
  CHECK(!p.isEgg());
  CHECK(p.colConserva(1));     // libera BLASTOISE
  CHECK(p.isEgg());
  CHECK_EQ(p.colNumero(), (uint8_t)COL_POSTI);
  CHECK_EQ(p.colRitratto(0).specie, (int16_t)3);
  CHECK_EQ(p.colRitratto(1).specie, (int16_t)65);
  CHECK_EQ(p.colRitratto(4).specie, (int16_t)6);

  Pet q;
  makeFinale(q, 6);
  for (uint8_t i = 0; i < COL_POSTI; i++) q.colDeposita(ritrattoDi(dex[i], ""));
  q.startFarewell();
  fineCerimonia(q);
  q.colRinuncia();
  CHECK(q.isEgg());
  CHECK_EQ(q.colRitratto(1).specie, (int16_t)9);
}

TEST(collezione, liberare_scala_i_posti_e_persiste) {
  Pet p;
  makePet(p, 4);
  p.colDeposita(ritrattoDi(3, "A"));
  p.colDeposita(ritrattoDi(9, "B"));
  p.colDeposita(ritrattoDi(65, "C"));
  CHECK(!p.colLibera(3));
  CHECK(p.colLibera(0));
  Pet b;
  b.begin();
  CHECK_EQ(b.colNumero(), (uint8_t)2);
  CHECK_STREQ(b.colRitratto(0).nick, "B");
  CHECK_STREQ(b.colRitratto(1).nick, "C");
  CHECK(b.colLibera(1));
  CHECK(b.colLibera(0));
  Preferences raw;  // vuota: la chiave sparisce
  raw.begin("pokegotchi", true);
  CHECK(!raw.isKey(COL_KEY));
  raw.end();
}

TEST(collezione, riprendere_con_un_uovo_scarta_l_uovo) {
  Pet p;
  makePet(p, 4);
  p.setClock(T0);
  p.newEgg();
  p.colDeposita(ritrattoDi(3, "A"));
  p.colDeposita(ritrattoDi(9, "B"));
  CHECK_EQ(p.colPerche(1), COL_OK);
  CHECK(p.colRiprendi(1));
  CHECK(!p.isEgg());
  CHECK_EQ(p.speciesId, (int16_t)9);
  CHECK_STREQ(p.nick, "B");
  CHECK_EQ(p.ageMinutes, (uint32_t)5000);
  CHECK_EQ(p.trSpe, (uint8_t)12);
  CHECK_EQ(p.bond, (uint8_t)40);
  CHECK_EQ(p.fullness, (uint8_t)80);
  CHECK(p.isRegistered(9));
  CHECK_EQ(p.colNumero(), (uint8_t)1);  // l'uovo non entra
  CHECK_EQ(p.colRitratto(0).specie, (int16_t)3);
  Pet b;  // tutto salvato
  b.begin();
  CHECK_EQ(b.speciesId, (int16_t)9);
  CHECK_EQ(b.colNumero(), (uint8_t)1);
}

TEST(collezione, riprendere_con_una_forma_finale_e_uno_scambio) {
  Pet p;
  makeFinale(p, 6);
  p.rename("ZARD");
  p.colDeposita(ritrattoDi(3, "SAURO"));
  CHECK(p.colRiprendi(0));
  CHECK_EQ(p.speciesId, (int16_t)3);
  CHECK_STREQ(p.nick, "SAURO");
  CHECK_EQ(p.colNumero(), (uint8_t)1);
  CHECK_EQ(p.colRitratto(0).specie, (int16_t)6);
  CHECK_STREQ(p.colRitratto(0).nick, "ZARD");
}

TEST(collezione, le_statistiche_del_ritratto_sono_quelle_del_pet) {
  Pet p;
  makeFinale(p, 6);
  p.trAtk = 30;
  p.geneDef = 95;
  uint16_t st[4];
  statDaRitratto(p.ritratto(), st);
  CHECK_EQ(st[0], p.atkStat());
  CHECK_EQ(st[1], p.defStat());
  CHECK_EQ(st[2], p.speStat());
  CHECK_EQ(st[3], p.spcStat());
  statDaRitratto(PetRitratto(), st);  // specie -1
  CHECK_EQ(st[0], (uint16_t)0);
}

TEST(collezione, condizioni_di_riprendi) {
  Pet p;
  makeFinale(p, 6);
  CHECK_EQ(p.colPerche(0), COL_POSTO);
  p.colDeposita(ritrattoDi(3, ""));
  CHECK_EQ(p.colPerche(0), COL_OK);
  p.fullness = COL_MIN_BARRA - 1;
  CHECK_EQ(p.colPerche(0), COL_NON_IN_FORMA);
  CHECK(!p.colRiprendi(0));
  p.fullness = COL_MIN_BARRA;
  CHECK_EQ(p.colPerche(0), COL_OK);
  p.toggleLight();
  CHECK_EQ(p.colPerche(0), COL_OCCUPATO);
  p.toggleLight();
  p.startFarewell();
  CHECK_EQ(p.colPerche(0), COL_OCCUPATO);

  Pet q;
  makePet(q, 4);  // non è una forma finale
  q.colDeposita(ritrattoDi(3, ""));
  CHECK_EQ(q.colPerche(0), COL_NON_FINALE);
  q.ageMinutes = 20 * MINUTES_PER_LEVEL;
  q.evolve();
  CHECK_EQ(q.colPerche(0), COL_OCCUPATO);  // mentre evolve
}

TEST(collezione, riprendi_una_volta_al_giorno) {
  Pet p;
  makeFinale(p, 6);
  p.colDeposita(ritrattoDi(3, ""));
  CHECK(p.colRiprendi(0));
  CHECK_EQ(p.colPerche(0), COL_OGGI);
  p.setClock(T0 + 3600);  // stesso giorno, anche dopo un riavvio
  Pet b;
  b.begin();
  b.syncClock(T0 + 3600);
  CHECK_EQ(b.colPerche(0), COL_OGGI);
  b.setClock(T0 + 86400);
  CHECK_EQ(b.colPerche(0), COL_OK);
}

TEST(collezione, il_nuovo_addio_aspetta_un_ciclo_dal_rientro) {
  Pet p;
  makeFinale(p, 6);
  PetRitratto vecchio = ritrattoDi(143, "");
  vecchio.ageMinutes = FAREWELL_AGE_MIN * 4;  // già vecchio: senza la regola potrebbe
  p.colDeposita(vecchio);                     // dire addio subito
  CHECK(p.colRiprendi(0));
  CHECK(!p.canFarewellNow());
  advance(p, 60);
  setStats(p, 100, 100, 100, 100);
  p.ageMinutes = FAREWELL_AGE_MIN * 5 - 1;
  CHECK(!p.canFarewellNow());
  p.ageMinutes = FAREWELL_AGE_MIN * 5;
  CHECK(p.canFarewellNow());
  Pet b;  // il campo è persistente
  p.rename("X");
  b.begin();
  b.ageMinutes = FAREWELL_AGE_MIN * 5 - 1;
  CHECK(!b.canFarewellNow());
  // e un pet nato da un uovo non ha vincoli in più
  Pet q;
  makeFinale(q, 6);
  CHECK(q.canFarewellNow());
}

TEST(collezione, un_ritratto_rovinato_si_perde_da_solo) {
  Pet p;
  makePet(p, 4);
  p.colDeposita(ritrattoDi(3, "A"));
  p.colDeposita(ritrattoDi(9, "B"));
  p.colDeposita(ritrattoDi(65, "C"));
  Preferences raw;
  raw.begin("pokegotchi", false);
  std::vector<uint8_t> v(raw.getBytesLength(COL_KEY));
  CHECK_EQ(v.size(), (size_t)(2 + 3 * RIT_BYTES));
  raw.getBytes(COL_KEY, &v[0], v.size());
  CHECK_EQ(v[0], (uint8_t)COL_VERSIONE);
  CHECK_EQ(v[1], (uint8_t)3);
  v[2 + RIT_BYTES + 20] ^= 0x01;  // il secondo
  raw.putBytes(COL_KEY, &v[0], v.size());
  raw.end();
  Pet b;
  b.begin();
  CHECK_EQ(b.colNumero(), (uint8_t)2);
  CHECK_STREQ(b.colRitratto(0).nick, "A");
  CHECK_STREQ(b.colRitratto(1).nick, "C");
}

TEST(save, i_campi_della_versione_3_sopravvivono_al_riavvio) {
  Pet a;
  makeFinale(a, 6);
  a.otId = 0x12345678;
  strcpy(a.otName, "BLU");
  a.colDeposita(ritrattoDi(3, ""));
  a.colRiprendi(0);  // colDay e farewellMinAge
  a.otId = 0x12345678;
  strcpy(a.otName, "BLU");
  a.rename("X");
  std::vector<uint8_t> v = readBlob();
  CHECK_EQ(v.size(), sizeof(PetSave));
  CHECK_EQ(v[0], (uint8_t)PET_SAVE_VERSION);
  Pet b;
  b.begin();
  CHECK_EQ(b.otId, (uint32_t)0x12345678);
  CHECK_STREQ(b.otName, "BLU");
  b.syncClock(T0 + 60);
  CHECK_EQ(b.colPerche(0), COL_OGGI);
  b.ageMinutes = 5000 + FAREWELL_AGE_MIN - 1;
  CHECK(!b.canFarewellNow());
}

TEST(save, un_blob_della_versione_2_si_carica_senza_i_campi_della_collezione) {
  Pet a;
  makeFinale(a, 6);
  a.colDeposita(ritrattoDi(3, ""));
  a.colRiprendi(0);
  a.rename("SPARKY");
  std::vector<uint8_t> v = readBlob();
  // il blob della 1.19 (versione 2, 110 byte): finiva con `winsDay`
  v.resize(offsetof(PetSave, colAsk) + 4);
  CHECK_EQ(v.size(), (size_t)110);
  v[0] = 2;
  writeBlob(v, true);
  Pet b;
  b.begin();
  CHECK_EQ(b.speciesId, (int16_t)3);
  CHECK_STREQ(b.nick, "SPARKY");
  CHECK(!b.colChiede());
  CHECK_EQ(b.otId, (uint32_t)0);
  CHECK_EQ(b.colPerche(0), COL_OK);  // nessun RIPRENDI registrato
  CHECK(b.canFarewellNow());          // nessuna età minima
}

TEST(save, un_blob_della_versione_3_si_carica_senza_scambi_applicati) {
  Pet a;
  makeFinale(a, 6);
  a.rename("SPARKY");
  std::vector<uint8_t> v = readBlob();
  // il blob della versione 3 (135 byte): finiva con `otName`
  v.resize(offsetof(PetSave, tradeFatto) + 4);
  CHECK_EQ(v.size(), (size_t)135);
  v[0] = 3;
  writeBlob(v, true);
  Pet b;
  b.begin();
  CHECK_EQ(b.speciesId, (int16_t)6);
  CHECK_STREQ(b.nick, "SPARKY");
  CHECK(!b.scambioApplicato(1));
  CHECK(!b.scambioApplicato(0));
}

TEST(save, i_record_dei_giochi_restano_dopo_il_riavvio) {
  Pet a;
  makePet(a, 4);
  a.grottaResult(33);
  a.surfResult(11);
  std::vector<uint8_t> v = readBlob();
  CHECK_EQ(v.size(), sizeof(PetSave));
  CHECK_EQ(v[0], (uint8_t)5);
  Pet b;
  b.begin();
  CHECK_EQ(b.grottaHi, (uint16_t)33);
  CHECK_EQ(b.surfHi, (uint16_t)11);
}

TEST(save, un_blob_della_versione_4_si_carica_senza_i_record_dei_giochi) {
  Pet a;
  makeFinale(a, 6);
  a.rename("SPARKY");
  a.grottaHi = 33;
  a.surfHi = 11;
  a.trDef = 9;
  a.gameHi = 21;
  a.grottaResult(0);  // salva
  std::vector<uint8_t> v = readBlob();
  // il blob della 2.0 (versione 4, 139 byte): finiva con `tradeFatto`
  v.resize(offsetof(PetSave, grottaHi) + 4);
  CHECK_EQ(v.size(), (size_t)139);
  v[0] = 4;
  writeBlob(v, true);
  Pet b;
  b.begin();
  CHECK_EQ(b.speciesId, (int16_t)6);
  CHECK_STREQ(b.nick, "SPARKY");
  CHECK_EQ(b.trDef, (uint8_t)9);
  CHECK_EQ(b.gameHi, (uint16_t)21);
  CHECK_EQ(b.grottaHi, (uint16_t)0);
  CHECK_EQ(b.surfHi, (uint16_t)0);
}

// ---------------------------------------------------------------- allenatore
TEST(allenatore, l_id_e_casuale_non_nullo_e_resta_dopo_il_riavvio) {
  Pet p;
  makePet(p, 4);
  uint32_t id = p.trainerId();
  CHECK(id != 0);
  CHECK_EQ(p.trainerId(), id);
  Pet q;
  q.begin();
  CHECK_EQ(q.trainerId(), id);
  mockNvsReset();  // WIPE: un allenatore nuovo
  Pet r;
  r.begin();
  CHECK(r.trainerId() != 0);
}

TEST(allenatore, il_nome_manca_finche_non_si_sceglie) {
  Pet p;
  makePet(p, 4);
  char nome[13] = "X";
  CHECK(!p.trainerName(nome, sizeof(nome)));
  CHECK_STREQ(nome, "");
  p.setTrainerName("ASH");
  Pet q;
  q.begin();
  CHECK(q.trainerName(nome, sizeof(nome)));
  CHECK_STREQ(nome, "ASH");
}
