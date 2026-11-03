// Integrità della tabella del Pokédex (dex.h, generata da tools/gen_dex.py).
#include "framework.h"
#include "shim/Arduino.h"
#include "../dex.h"
#include "../moves.h"
#include "../i18n.h"
#include "../pet.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <set>

static const int N = DEX_COUNT;

TEST(dex, la_tabella_ha_152_voci) {
  CHECK_EQ((int)(sizeof(DEX_TBL) / sizeof(DEX_TBL[0])), N + 1);
  CHECK_EQ((int)(sizeof(DEX_NAME_FR) / sizeof(DEX_NAME_FR[0])), N + 1);
  CHECK_EQ((int)(sizeof(DEX_NAME_DE) / sizeof(DEX_NAME_DE[0])), N + 1);
}

TEST(dex, tutti_i_nomi_sono_ascii_maiuscoli) {
  for (int d = 1; d <= N; d++) {
    const char *n = DEX_TBL[d].name;
    CHECK_MSG(n != NULL && n[0] != 0, "entrada sin nombre");
    if (!n || !n[0]) continue;
    for (const char *c = n; *c; c++) {
      // il font GFX del firmware è ASCII e senza minuscole
      CHECK_MSG((unsigned char)*c < 128, std::string("caracter no ASCII en ") + n);
      CHECK_MSG(!islower((unsigned char)*c), std::string("minuscula en ") + n);
    }
  }
}

TEST(dex, anche_i_nomi_tradotti_sono_ascii) {
  const char *const *tables[2] = { DEX_NAME_FR, DEX_NAME_DE };
  for (int t = 0; t < 2; t++) {
    for (int d = 1; d <= N; d++) {
      const char *n = tables[t][d];
      if (!n) continue;  // senza nome proprio: ricade su quello di DEX_TBL
      CHECK_MSG(n[0] != 0, "nombre traducido vacio");
      for (const char *c = n; *c; c++) {
        CHECK_MSG((unsigned char)*c < 128, std::string("caracter no ASCII en ") + n);
        CHECK_MSG(!islower((unsigned char)*c), std::string("minuscula en ") + n);
      }
    }
  }
}

// l'intestazione della scheda è char[26] con "%s%s Lv.%u": nessun nome in nessuna
// lingua può farla traboccare (il nome verrebbe tagliato a schermo)
TEST(dex, nessun_nome_trabocca_dall_intestazione) {
  char head[26];
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    gLang = (Lang)lang;
    for (int d = 1; d <= N; d++) {
      int need = snprintf(head, sizeof(head), T(S_NAME_FMT), "*", dexName(d), 255);
      CHECK_MSG(need >= 0 && need < (int)sizeof(head),
                std::string("cabecera cortada: ") + dexName(d));
    }
  }
  gLang = LANG_DEFAULT;
}

TEST(dex, le_evoluzioni_puntano_a_voci_valide) {
  for (int d = 1; d <= N; d++) {
    uint8_t to = DEX_TBL[d].evolvesTo;
    CHECK_MSG(to <= N, std::string("evolvesTo fuera de rango en ") + DEX_TBL[d].name);
    CHECK_MSG(to != d, std::string("se evoluciona a si mismo: ") + DEX_TBL[d].name);
  }
}

TEST(dex, evoluzione_e_livello_vanno_di_pari_passo) {
  for (int d = 1; d <= N; d++) {
    bool evolves = DEX_TBL[d].evolvesTo != 0;
    bool hasLevel = DEX_TBL[d].evolveLevel > 0;
    CHECK_MSG(evolves == hasLevel,
              std::string("evolvesTo y evolveLevel no cuadran en ") + DEX_TBL[d].name);
  }
}

TEST(dex, nessuna_catena_evolutiva_fa_un_ciclo) {
  for (int d = 1; d <= N; d++) {
    std::set<int> seen;
    int cur = d;
    int guard = 0;
    while (cur >= 1 && cur <= N && DEX_TBL[cur].evolvesTo != 0 && guard++ < N) {
      CHECK_MSG(seen.insert(cur).second, std::string("bucle evolutivo en ") + DEX_TBL[d].name);
      if (!seen.count(cur)) break;
      cur = DEX_TBL[cur].evolvesTo;
    }
    CHECK_MSG(guard < 6, std::string("cadena demasiado larga desde ") + DEX_TBL[d].name);
  }
}

TEST(dex, i_livelli_di_evoluzione_salgono_lungo_la_catena) {
  for (int d = 1; d <= N; d++) {
    int cur = d, prev = 0, guard = 0;
    while (DEX_TBL[cur].evolvesTo != 0 && guard++ < 6) {
      CHECK_MSG(DEX_TBL[cur].evolveLevel > prev,
                std::string("nivel de evolucion no creciente en ") + DEX_TBL[cur].name);
      prev = DEX_TBL[cur].evolveLevel;
      cur = DEX_TBL[cur].evolvesTo;
    }
  }
}

// una specie che esce solo per evoluzione deve essere raggiungibile da qualche uovo,
// altrimenti sarebbe impossibile registrarla nel pokédex
TEST(dex, ogni_specie_e_raggiungibile) {
  bool reach[DEX_COUNT + 1] = { false };
  for (int d = 1; d <= N; d++) {
    if (DEX_TBL[d].rarity == R_EVO) continue;  // non esce dall'uovo
    int cur = d, guard = 0;
    while (cur >= 1 && cur <= N && guard++ < 6) {
      reach[cur] = true;
      if (cur == DEX_EEVEE) {  // ramo speciale del codice del gioco
        reach[134] = reach[135] = reach[136] = true;
        break;
      }
      if (DEX_TBL[cur].evolvesTo == 0) break;
      cur = DEX_TBL[cur].evolvesTo;
    }
  }
  for (int d = 1; d <= N; d++)
    CHECK_MSG(reach[d], std::string("inalcanzable: ") + DEX_TBL[d].name);
}

// con l'addio a 3 giorni (livello 72), ogni linea deve poter arrivare alla sua forma
// finale prima che il Pokémon compia il ciclo
TEST(dex, ogni_linea_si_completa_prima_del_congedo) {
  const uint32_t maxLevel = FAREWELL_AGE_MIN / MINUTES_PER_LEVEL;
  for (int d = 1; d <= N; d++) {
    if (DEX_TBL[d].rarity == R_EVO) continue;
    int cur = d, guard = 0;
    uint32_t need = 0;
    while (DEX_TBL[cur].evolvesTo != 0 && guard++ < 6) {
      if (DEX_TBL[cur].evolveLevel > need) need = DEX_TBL[cur].evolveLevel;
      cur = DEX_TBL[cur].evolvesTo;
    }
    CHECK_MSG(need <= maxLevel,
              std::string("la linea de ") + DEX_TBL[d].name + " no llega a forma final a tiempo");
  }
}

TEST(dex, le_stats_base_sono_plausibili) {
  for (int d = 1; d <= N; d++) {
    const DexEntry &e = DEX_TBL[d];
    CHECK_MSG(e.bHp > 0 && e.bAtk > 0 && e.bDef > 0 && e.bSpe > 0,
              std::string("stat base a cero en ") + e.name);
    CHECK_MSG(e.biome <= 5, std::string("bioma desconocido en ") + e.name);
    CHECK_MSG(e.rarity <= R_LEGENDARIO, std::string("rareza desconocida en ") + e.name);
  }
}

TEST(dex, il_ramo_di_eevee_torna_con_il_codice) {
  CHECK_EQ((int)DEX_TBL[DEX_EEVEE].rarity, (int)R_COMUN);
  CHECK_RANGE((int)DEX_TBL[DEX_EEVEE].evolvesTo, 134, 136);
  for (int d = 134; d <= 136; d++) {
    CHECK_EQ((int)DEX_TBL[d].rarity, (int)R_EVO);
    CHECK_EQ((int)DEX_TBL[d].evolvesTo, 0);
  }
}

TEST(dex, gli_iniziali_classici_sono_validi) {
  CHECK_EQ((int)(sizeof(CLASSIC_DEX) / sizeof(CLASSIC_DEX[0])), NUM_CLASSIC_DEX);
  for (int i = 0; i < NUM_CLASSIC_DEX; i++) {
    int16_t d = CLASSIC_DEX[i];
    CHECK_RANGE(d, (int16_t)1, (int16_t)151);
    CHECK_MSG(DEX_TBL[d].rarity != R_EVO, "un inicial no puede ser una forma evolucionada");
  }
}

TEST(dex, c_e_almeno_una_specie_per_ogni_rarita) {
  int count[4] = { 0, 0, 0, 0 };
  for (int d = 1; d <= N; d++) count[DEX_TBL[d].rarity]++;
  for (int r = 0; r <= R_LEGENDARIO; r++)
    CHECK_MSG(count[r] > 0, "hay una rareza sin especies: el sorteo del huevo se quedaria sin candidatos");
}

// pickEggSpecies mette i candidati di un livello in un array da 80: se qualche livello
// ne avesse di più, si sorteggerebbe solo tra i primi 80
TEST(dex, nessun_tier_trabocca_dall_array_dei_candidati) {
  for (int r = R_COMUN; r <= R_LEGENDARIO; r++) {
    int n = 0;
    for (int d = 1; d <= N; d++)
      if (DEX_TBL[d].rarity == r) n++;
    CHECK_MSG(n <= 80, "mas de 80 candidatos en un tier: pickEggSpecies los recorta");
  }
}

TEST(dex, dexName_ripiega_sul_nome_base_senza_traduzione) {
  gLang = LANG_ES;
  CHECK_STREQ(dexName(1), DEX_TBL[1].name);
  gLang = LANG_FR;
  for (int d = 1; d <= N; d++) {
    const char *want = DEX_NAME_FR[d] ? DEX_NAME_FR[d] : DEX_TBL[d].name;
    CHECK_STREQ(dexName(d), want);
  }
  gLang = LANG_DE;
  for (int d = 1; d <= N; d++) {
    const char *want = DEX_NAME_DE[d] ? DEX_NAME_DE[d] : DEX_TBL[d].name;
    CHECK_STREQ(dexName(d), want);
  }
  gLang = LANG_DEFAULT;
}

TEST(dex, dexName_fuori_intervallo_non_crasha) {
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    gLang = (Lang)lang;
    CHECK_STREQ(dexName(0), DEX_TBL[0].name);
    CHECK_STREQ(dexName(-1), DEX_TBL[0].name);
    CHECK_STREQ(dexName(152), DEX_TBL[0].name);
    CHECK_STREQ(dexName(30000), DEX_TBL[0].name);
  }
  gLang = LANG_DEFAULT;
}

// --- dati di battaglia (moves.h e i campi nuovi di DexEntry, generati da gen_dex.py) ---

TEST(dex, i_tipi_di_battaglia_sono_validi) {
  for (int d = 1; d <= N; d++) {
    const DexEntry &e = DEX_TBL[d];
    CHECK_MSG(e.type1 < TYPE_COUNT, std::string("tipo 1 non valido in ") + e.name);
    CHECK_MSG(e.type2 < TYPE_COUNT || e.type2 == T_NESSUNO, std::string("tipo 2 non valido in ") + e.name);
    CHECK_MSG(e.type1 != e.type2, std::string("tipo ripetuto in ") + e.name);
    CHECK_MSG(e.bSpc > 0, std::string("Speciale a zero in ") + e.name);
  }
  CHECK_EQ((int)DEX_TBL[6].type2, (int)T_VOLANTE);   // Charizard
  CHECK_EQ((int)DEX_TBL[81].type2, (int)T_NESSUNO);  // Magnemite, solo elettro
  CHECK_EQ((int)DEX_TBL[150].bSpc, 154);             // Mewtwo
}

TEST(dex, solo_le_quattro_evoluzioni_per_scambio) {
  int n = 0;
  for (int d = 1; d <= N; d++) {
    if (!DEX_TBL[d].tradeEvo) continue;
    n++;
    CHECK_MSG(DEX_TBL[d].evolvesTo == d + 1, std::string("tradeEvo senza evoluzione in ") + DEX_TBL[d].name);
  }
  CHECK_EQ(n, 4);
  CHECK(DEX_TBL[64].tradeEvo && DEX_TBL[67].tradeEvo && DEX_TBL[75].tradeEvo && DEX_TBL[93].tradeEvo);
}

TEST(dex, la_tabella_dei_tipi_e_raddoppiata) {
  for (int a = 0; a < TYPE_COUNT; a++)
    for (int b = 0; b < TYPE_COUNT; b++) {
      uint8_t v = TYPE_CHART[a][b];
      CHECK_MSG(v == 0 || v == 1 || v == 2 || v == 4, "efficacia fuori da {0, 1/2, 1, 2}");
    }
  CHECK_EQ((int)TYPE_CHART[T_ACQUA][T_FUOCO], 4);
  CHECK_EQ((int)TYPE_CHART[T_FUOCO][T_ACQUA], 1);
  CHECK_EQ((int)TYPE_CHART[T_SPETTRO][T_PSICO], 0);  // bug di Rosso/Blu, per fedeltà
  CHECK(TYPE_IS_SPECIAL(T_FUOCO) && TYPE_IS_SPECIAL(T_DRAGO));
  CHECK(!TYPE_IS_SPECIAL(T_NORMALE) && !TYPE_IS_SPECIAL(T_SPETTRO));
}

TEST(dex, i_learnset_csr_puntano_a_mosse_vere) {
  CHECK_EQ((int)LEARN_OFS[0], 0);
  CHECK_EQ((int)LEARN_OFS[1], 0);
  CHECK_EQ((int)LEARN_OFS[DEX_COUNT + 1], LEARN_COUNT);
  for (int d = 1; d <= N; d++) {
    CHECK_MSG(LEARN_OFS[d] <= LEARN_OFS[d + 1], "LEARN_OFS non crescente");
    uint8_t lv = 1;
    for (int i = LEARN_OFS[d]; i < LEARN_OFS[d + 1]; i++) {
      const LearnEntry &l = LEARN_TBL[i];
      CHECK_MSG(l.move >= 1 && l.move <= MOVE_MAX_ID && MOVE_TBL[l.move].pp > 0,
                std::string("mossa inesistente nel learnset di ") + DEX_TBL[d].name);
      CHECK_MSG(l.move != MOVE_STRUGGLE, "Scontro non si impara");
      CHECK_MSG(l.level >= lv, std::string("livelli in disordine in ") + DEX_TBL[d].name);
      lv = l.level;
    }
  }
  CHECK_EQ((int)(LEARN_OFS[64] - LEARN_OFS[63]), 0);  // Abra: ripiega su Scontro
  CHECK_EQ((int)MOVE_TBL[MOVE_STRUGGLE].effect, (int)EF_RINCULO);
  CHECK_EQ((int)MOVE_TBL[MV_TRANSFORM].effect, (int)EF_TRASFORMAZIONE);
  CHECK_EQ((int)MOVE_TBL[MV_PSYCHIC].p1, (int)STAT_SPECIALE);
}

TEST(dex, moveName_ripiega_sull_inglese) {
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    gLang = (Lang)lang;
    for (int id = 0; id <= MOVE_MAX_ID; id++) {
      const char *n = moveName(id);
      CHECK_MSG(n && n[0], "nome di mossa vuoto");
      if (MOVE_TBL[id].pp == 0) CHECK_STREQ(n, "?");
    }
    CHECK_STREQ(moveName(MOVE_MAX_ID + 1), "?");
    CHECK_STREQ(moveName(255), "?");
  }
  gLang = LANG_PT;
  CHECK_STREQ(moveName(MV_THUNDER_SHOCK), "THUNDERSHOCK");
  gLang = LANG_IT;
  CHECK_STREQ(moveName(MV_THUNDER_SHOCK), "TUONOSHOCK");
  CHECK_STREQ(moveName(MV_TACKLE), "AZIONE");
  gLang = LANG_DEFAULT;
}
