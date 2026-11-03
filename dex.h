#pragma once
#include <stdint.h>
#include "i18n.h"  // gLang

// GENERATO da tools/gen_dex.py da tools/dex_data.py - non modificare

#define DEX_COUNT 151
#define DEX_EEVEE 133  // ramo casuale: 134/135/136

// tipi di battaglia di Rosso/Blu (tools/dex_types.py): l'ordine è l'indice di
// TYPE_CHART in moves.h; i fisici vengono prima degli speciali
#define TYPE_COUNT 15
enum : uint8_t {
  T_NORMALE = 0,
  T_LOTTA = 1,
  T_VOLANTE = 2,
  T_VELENO = 3,
  T_TERRA = 4,
  T_ROCCIA = 5,
  T_COLEOTTERO = 6,
  T_SPETTRO = 7,
  T_FUOCO = 8,
  T_ACQUA = 9,
  T_ERBA = 10,
  T_ELETTRO = 11,
  T_PSICO = 12,
  T_GHIACCIO = 13,
  T_DRAGO = 14,
  T_NESSUNO = 0xFF,  // type2 di chi ha un solo tipo
};

// colore RGB565 di ogni tipo T_* (TYPE_ACCENTS di dex_data.py), per la UI
static const uint16_t TYPE_COLOR[TYPE_COUNT] = {
  0x8C4D, 0xA2A5, 0x7CDA, 0x8A73, 0xB447, 0x9407, 0x7CC4, 0x6AD3, 0xEA87, 0x4C98, 0x3C49, 0xBCA1, 0xD28F, 0x4DB8, 0x5A98,
};

// rarità: 0 = solo per evoluzione, 1 = comune, 2 = raro, 3 = leggendario
enum : uint8_t { R_EVO = 0, R_COMUN, R_RARO, R_LEGENDARIO };

struct DexEntry {
  const char *name;
  uint8_t evolvesTo;    // numero di dex, 0 = forma finale
  uint8_t evolveLevel;
  uint8_t rarity;       // esce dall'uovo se > 0
  uint16_t accent;      // colore RGB565 del tipo per la UI
  uint8_t bHp, bAtk, bDef, bSpe;  // statistiche base reali della gen 1
  uint8_t biome;        // 0 prateria 1 spiaggia 2 bosco 3 vulcano 4 montagna 5 neve
  uint8_t type1, type2; // tipi di battaglia T_*; type2 = T_NESSUNO se è uno solo
  uint8_t bSpc;         // Speciale base della gen 1
  uint8_t tradeEvo;     // 1 = evolve in evolvesTo anche alla ricezione di uno scambio
};

static const DexEntry DEX_TBL[DEX_COUNT + 1] = {
  { "?", 0, 0, 0, 0x2946, 50, 50, 50, 50, 0, T_NORMALE, T_NESSUNO, 50, 0 },  // 0: inutilizzato
  { "BULBASAUR", 2, 16, R_COMUN, 0x3C49, 45, 49, 49, 45, 2, T_ERBA, T_VELENO, 65, 0 },  // 1 erba
  { "IVYSAUR", 3, 32, R_EVO, 0x3C49, 60, 62, 63, 60, 2, T_ERBA, T_VELENO, 80, 0 },  // 2 erba
  { "VENUSAUR", 0, 0, R_EVO, 0x3C49, 80, 82, 83, 80, 2, T_ERBA, T_VELENO, 100, 0 },  // 3 erba
  { "CHARMANDER", 5, 16, R_COMUN, 0xEA87, 39, 52, 43, 65, 3, T_FUOCO, T_NESSUNO, 50, 0 },  // 4 fuoco
  { "CHARMELEON", 6, 36, R_EVO, 0xEA87, 58, 64, 58, 80, 3, T_FUOCO, T_NESSUNO, 65, 0 },  // 5 fuoco
  { "CHARIZARD", 0, 0, R_EVO, 0xEA87, 78, 84, 78, 100, 3, T_FUOCO, T_VOLANTE, 85, 0 },  // 6 fuoco
  { "SQUIRTLE", 8, 16, R_COMUN, 0x4C98, 44, 48, 65, 43, 1, T_ACQUA, T_NESSUNO, 50, 0 },  // 7 acqua
  { "WARTORTLE", 9, 36, R_EVO, 0x4C98, 59, 63, 80, 58, 1, T_ACQUA, T_NESSUNO, 65, 0 },  // 8 acqua
  { "BLASTOISE", 0, 0, R_EVO, 0x4C98, 79, 83, 100, 78, 1, T_ACQUA, T_NESSUNO, 85, 0 },  // 9 acqua
  { "CATERPIE", 11, 7, R_COMUN, 0x7CC4, 45, 30, 35, 45, 2, T_COLEOTTERO, T_NESSUNO, 20, 0 },  // 10 coleottero
  { "METAPOD", 12, 10, R_EVO, 0x7CC4, 50, 20, 55, 30, 2, T_COLEOTTERO, T_NESSUNO, 25, 0 },  // 11 coleottero
  { "BUTTERFREE", 0, 0, R_EVO, 0x7CC4, 60, 45, 50, 70, 2, T_COLEOTTERO, T_VOLANTE, 80, 0 },  // 12 coleottero
  { "WEEDLE", 14, 7, R_COMUN, 0x7CC4, 40, 35, 30, 50, 2, T_COLEOTTERO, T_VELENO, 20, 0 },  // 13 coleottero
  { "KAKUNA", 15, 10, R_EVO, 0x7CC4, 45, 25, 50, 35, 2, T_COLEOTTERO, T_VELENO, 25, 0 },  // 14 coleottero
  { "BEEDRILL", 0, 0, R_EVO, 0x7CC4, 65, 90, 40, 75, 2, T_COLEOTTERO, T_VELENO, 45, 0 },  // 15 coleottero
  { "PIDGEY", 17, 18, R_COMUN, 0x8C4D, 40, 45, 40, 56, 0, T_NORMALE, T_VOLANTE, 35, 0 },  // 16 normale
  { "PIDGEOTTO", 18, 36, R_EVO, 0x8C4D, 63, 60, 55, 71, 0, T_NORMALE, T_VOLANTE, 50, 0 },  // 17 normale
  { "PIDGEOT", 0, 0, R_EVO, 0x8C4D, 83, 80, 75, 101, 0, T_NORMALE, T_VOLANTE, 70, 0 },  // 18 normale
  { "RATTATA", 20, 20, R_COMUN, 0x8C4D, 30, 56, 35, 72, 0, T_NORMALE, T_NESSUNO, 25, 0 },  // 19 normale
  { "RATICATE", 0, 0, R_EVO, 0x8C4D, 55, 81, 60, 97, 0, T_NORMALE, T_NESSUNO, 50, 0 },  // 20 normale
  { "SPEAROW", 22, 20, R_COMUN, 0x8C4D, 40, 60, 30, 70, 0, T_NORMALE, T_VOLANTE, 31, 0 },  // 21 normale
  { "FEAROW", 0, 0, R_EVO, 0x8C4D, 65, 90, 65, 100, 0, T_NORMALE, T_VOLANTE, 61, 0 },  // 22 normale
  { "EKANS", 24, 22, R_COMUN, 0x8A73, 35, 60, 44, 55, 0, T_VELENO, T_NESSUNO, 40, 0 },  // 23 veleno
  { "ARBOK", 0, 0, R_EVO, 0x8A73, 60, 95, 69, 80, 0, T_VELENO, T_NESSUNO, 65, 0 },  // 24 veleno
  { "PIKACHU", 26, 30, R_COMUN, 0xBCA1, 35, 55, 40, 90, 0, T_ELETTRO, T_NESSUNO, 50, 0 },  // 25 elettro
  { "RAICHU", 0, 0, R_EVO, 0xBCA1, 60, 90, 55, 110, 0, T_ELETTRO, T_NESSUNO, 90, 0 },  // 26 elettro
  { "SANDSHREW", 28, 22, R_COMUN, 0xB447, 50, 75, 85, 40, 4, T_TERRA, T_NESSUNO, 30, 0 },  // 27 terra
  { "SANDSLASH", 0, 0, R_EVO, 0xB447, 75, 100, 110, 65, 4, T_TERRA, T_NESSUNO, 55, 0 },  // 28 terra
  { "NIDORAN H", 30, 16, R_COMUN, 0x8A73, 55, 47, 52, 41, 0, T_VELENO, T_NESSUNO, 40, 0 },  // 29 veleno
  { "NIDORINA", 31, 30, R_EVO, 0x8A73, 70, 62, 67, 56, 0, T_VELENO, T_NESSUNO, 55, 0 },  // 30 veleno
  { "NIDOQUEEN", 0, 0, R_EVO, 0x8A73, 90, 92, 87, 76, 0, T_VELENO, T_TERRA, 75, 0 },  // 31 veleno
  { "NIDORAN M", 33, 16, R_COMUN, 0x8A73, 46, 57, 40, 50, 0, T_VELENO, T_NESSUNO, 40, 0 },  // 32 veleno
  { "NIDORINO", 34, 30, R_EVO, 0x8A73, 61, 72, 57, 65, 0, T_VELENO, T_NESSUNO, 55, 0 },  // 33 veleno
  { "NIDOKING", 0, 0, R_EVO, 0x8A73, 81, 102, 77, 85, 0, T_VELENO, T_TERRA, 75, 0 },  // 34 veleno
  { "CLEFAIRY", 36, 30, R_COMUN, 0x8C4D, 70, 45, 48, 35, 0, T_NORMALE, T_NESSUNO, 60, 0 },  // 35 normale
  { "CLEFABLE", 0, 0, R_EVO, 0x8C4D, 95, 70, 73, 60, 0, T_NORMALE, T_NESSUNO, 85, 0 },  // 36 normale
  { "VULPIX", 38, 30, R_COMUN, 0xEA87, 38, 41, 40, 65, 3, T_FUOCO, T_NESSUNO, 65, 0 },  // 37 fuoco
  { "NINETALES", 0, 0, R_EVO, 0xEA87, 73, 76, 75, 100, 3, T_FUOCO, T_NESSUNO, 100, 0 },  // 38 fuoco
  { "JIGGLYPUFF", 40, 30, R_COMUN, 0x8C4D, 115, 45, 20, 20, 0, T_NORMALE, T_NESSUNO, 25, 0 },  // 39 normale
  { "WIGGLYTUFF", 0, 0, R_EVO, 0x8C4D, 140, 70, 45, 45, 0, T_NORMALE, T_NESSUNO, 50, 0 },  // 40 normale
  { "ZUBAT", 42, 22, R_COMUN, 0x8A73, 40, 45, 35, 55, 0, T_VELENO, T_VOLANTE, 40, 0 },  // 41 veleno
  { "GOLBAT", 0, 0, R_EVO, 0x8A73, 75, 80, 70, 90, 0, T_VELENO, T_VOLANTE, 75, 0 },  // 42 veleno
  { "ODDISH", 44, 21, R_COMUN, 0x3C49, 45, 50, 55, 30, 2, T_ERBA, T_VELENO, 75, 0 },  // 43 erba
  { "GLOOM", 45, 36, R_EVO, 0x3C49, 60, 65, 70, 40, 2, T_ERBA, T_VELENO, 85, 0 },  // 44 erba
  { "VILEPLUME", 0, 0, R_EVO, 0x3C49, 75, 80, 85, 50, 2, T_ERBA, T_VELENO, 100, 0 },  // 45 erba
  { "PARAS", 47, 24, R_COMUN, 0x7CC4, 35, 70, 55, 25, 2, T_COLEOTTERO, T_ERBA, 55, 0 },  // 46 coleottero
  { "PARASECT", 0, 0, R_EVO, 0x7CC4, 60, 95, 80, 30, 2, T_COLEOTTERO, T_ERBA, 80, 0 },  // 47 coleottero
  { "VENONAT", 49, 31, R_COMUN, 0x7CC4, 60, 55, 50, 45, 2, T_COLEOTTERO, T_VELENO, 40, 0 },  // 48 coleottero
  { "VENOMOTH", 0, 0, R_EVO, 0x7CC4, 70, 65, 60, 90, 2, T_COLEOTTERO, T_VELENO, 90, 0 },  // 49 coleottero
  { "DIGLETT", 51, 26, R_COMUN, 0xB447, 10, 55, 25, 95, 4, T_TERRA, T_NESSUNO, 45, 0 },  // 50 terra
  { "DUGTRIO", 0, 0, R_EVO, 0xB447, 35, 100, 50, 120, 4, T_TERRA, T_NESSUNO, 70, 0 },  // 51 terra
  { "MEOWTH", 53, 28, R_COMUN, 0x8C4D, 40, 45, 35, 90, 0, T_NORMALE, T_NESSUNO, 40, 0 },  // 52 normale
  { "PERSIAN", 0, 0, R_EVO, 0x8C4D, 65, 70, 60, 115, 0, T_NORMALE, T_NESSUNO, 65, 0 },  // 53 normale
  { "PSYDUCK", 55, 33, R_COMUN, 0x4C98, 50, 52, 48, 55, 1, T_ACQUA, T_NESSUNO, 50, 0 },  // 54 acqua
  { "GOLDUCK", 0, 0, R_EVO, 0x4C98, 80, 82, 78, 85, 1, T_ACQUA, T_NESSUNO, 80, 0 },  // 55 acqua
  { "MANKEY", 57, 28, R_COMUN, 0xA2A5, 40, 80, 35, 70, 0, T_LOTTA, T_NESSUNO, 35, 0 },  // 56 lotta
  { "PRIMEAPE", 0, 0, R_EVO, 0xA2A5, 65, 105, 60, 95, 0, T_LOTTA, T_NESSUNO, 60, 0 },  // 57 lotta
  { "GROWLITHE", 59, 30, R_RARO, 0xEA87, 55, 70, 45, 60, 3, T_FUOCO, T_NESSUNO, 50, 0 },  // 58 fuoco
  { "ARCANINE", 0, 0, R_EVO, 0xEA87, 90, 110, 80, 95, 3, T_FUOCO, T_NESSUNO, 80, 0 },  // 59 fuoco
  { "POLIWAG", 61, 25, R_COMUN, 0x4C98, 40, 50, 40, 90, 1, T_ACQUA, T_NESSUNO, 40, 0 },  // 60 acqua
  { "POLIWHIRL", 62, 36, R_EVO, 0x4C98, 65, 65, 65, 90, 1, T_ACQUA, T_NESSUNO, 50, 0 },  // 61 acqua
  { "POLIWRATH", 0, 0, R_EVO, 0x4C98, 90, 95, 95, 70, 1, T_ACQUA, T_LOTTA, 70, 0 },  // 62 acqua
  { "ABRA", 64, 16, R_COMUN, 0xD28F, 25, 20, 15, 90, 0, T_PSICO, T_NESSUNO, 105, 0 },  // 63 psico
  { "KADABRA", 65, 40, R_EVO, 0xD28F, 40, 35, 30, 105, 0, T_PSICO, T_NESSUNO, 120, 1 },  // 64 psico
  { "ALAKAZAM", 0, 0, R_EVO, 0xD28F, 55, 50, 45, 120, 0, T_PSICO, T_NESSUNO, 135, 0 },  // 65 psico
  { "MACHOP", 67, 28, R_COMUN, 0xA2A5, 70, 80, 50, 35, 0, T_LOTTA, T_NESSUNO, 35, 0 },  // 66 lotta
  { "MACHOKE", 68, 40, R_EVO, 0xA2A5, 80, 100, 70, 45, 0, T_LOTTA, T_NESSUNO, 50, 1 },  // 67 lotta
  { "MACHAMP", 0, 0, R_EVO, 0xA2A5, 90, 130, 80, 55, 0, T_LOTTA, T_NESSUNO, 65, 0 },  // 68 lotta
  { "BELLSPROUT", 70, 21, R_COMUN, 0x3C49, 50, 75, 35, 40, 2, T_ERBA, T_VELENO, 70, 0 },  // 69 erba
  { "WEEPINBELL", 71, 36, R_EVO, 0x3C49, 65, 90, 50, 55, 2, T_ERBA, T_VELENO, 85, 0 },  // 70 erba
  { "VICTREEBEL", 0, 0, R_EVO, 0x3C49, 80, 105, 65, 70, 2, T_ERBA, T_VELENO, 100, 0 },  // 71 erba
  { "TENTACOOL", 73, 30, R_COMUN, 0x4C98, 40, 40, 35, 70, 1, T_ACQUA, T_VELENO, 100, 0 },  // 72 acqua
  { "TENTACRUEL", 0, 0, R_EVO, 0x4C98, 80, 70, 65, 100, 1, T_ACQUA, T_VELENO, 120, 0 },  // 73 acqua
  { "GEODUDE", 75, 25, R_COMUN, 0x9407, 40, 80, 100, 20, 4, T_ROCCIA, T_TERRA, 30, 0 },  // 74 roccia
  { "GRAVELER", 76, 40, R_EVO, 0x9407, 55, 95, 115, 35, 4, T_ROCCIA, T_TERRA, 45, 1 },  // 75 roccia
  { "GOLEM", 0, 0, R_EVO, 0x9407, 80, 120, 130, 45, 4, T_ROCCIA, T_TERRA, 55, 0 },  // 76 roccia
  { "PONYTA", 78, 40, R_RARO, 0xEA87, 50, 85, 55, 90, 3, T_FUOCO, T_NESSUNO, 65, 0 },  // 77 fuoco
  { "RAPIDASH", 0, 0, R_EVO, 0xEA87, 65, 100, 70, 105, 3, T_FUOCO, T_NESSUNO, 80, 0 },  // 78 fuoco
  { "SLOWPOKE", 80, 37, R_COMUN, 0x4C98, 90, 65, 65, 15, 1, T_ACQUA, T_PSICO, 40, 0 },  // 79 acqua
  { "SLOWBRO", 0, 0, R_EVO, 0x4C98, 95, 75, 110, 30, 1, T_ACQUA, T_PSICO, 80, 0 },  // 80 acqua
  { "MAGNEMITE", 82, 30, R_COMUN, 0xBCA1, 25, 35, 70, 45, 0, T_ELETTRO, T_NESSUNO, 95, 0 },  // 81 elettro
  { "MAGNETON", 0, 0, R_EVO, 0xBCA1, 50, 60, 95, 70, 0, T_ELETTRO, T_NESSUNO, 120, 0 },  // 82 elettro
  { "FARFETCHD", 0, 0, R_RARO, 0x8C4D, 52, 90, 55, 60, 0, T_NORMALE, T_VOLANTE, 58, 0 },  // 83 normale
  { "DODUO", 85, 31, R_COMUN, 0x8C4D, 35, 85, 45, 75, 0, T_NORMALE, T_VOLANTE, 35, 0 },  // 84 normale
  { "DODRIO", 0, 0, R_EVO, 0x8C4D, 60, 110, 70, 110, 0, T_NORMALE, T_VOLANTE, 60, 0 },  // 85 normale
  { "SEEL", 87, 34, R_COMUN, 0x4C98, 65, 45, 55, 45, 1, T_ACQUA, T_NESSUNO, 70, 0 },  // 86 acqua
  { "DEWGONG", 0, 0, R_EVO, 0x4C98, 90, 70, 80, 70, 1, T_ACQUA, T_GHIACCIO, 95, 0 },  // 87 acqua
  { "GRIMER", 89, 38, R_RARO, 0x8A73, 80, 80, 50, 25, 0, T_VELENO, T_NESSUNO, 40, 0 },  // 88 veleno
  { "MUK", 0, 0, R_EVO, 0x8A73, 105, 105, 75, 50, 0, T_VELENO, T_NESSUNO, 65, 0 },  // 89 veleno
  { "SHELLDER", 91, 30, R_COMUN, 0x4C98, 30, 65, 100, 40, 1, T_ACQUA, T_NESSUNO, 45, 0 },  // 90 acqua
  { "CLOYSTER", 0, 0, R_EVO, 0x4C98, 50, 95, 180, 70, 1, T_ACQUA, T_GHIACCIO, 85, 0 },  // 91 acqua
  { "GASTLY", 93, 25, R_COMUN, 0x6AD3, 30, 35, 30, 80, 0, T_SPETTRO, T_VELENO, 100, 0 },  // 92 spettro
  { "HAUNTER", 94, 40, R_EVO, 0x6AD3, 45, 50, 45, 95, 0, T_SPETTRO, T_VELENO, 115, 1 },  // 93 spettro
  { "GENGAR", 0, 0, R_EVO, 0x6AD3, 60, 65, 60, 110, 0, T_SPETTRO, T_VELENO, 130, 0 },  // 94 spettro
  { "ONIX", 0, 0, R_RARO, 0x9407, 35, 45, 160, 70, 4, T_ROCCIA, T_TERRA, 30, 0 },  // 95 roccia
  { "DROWZEE", 97, 26, R_COMUN, 0xD28F, 60, 48, 45, 42, 0, T_PSICO, T_NESSUNO, 90, 0 },  // 96 psico
  { "HYPNO", 0, 0, R_EVO, 0xD28F, 85, 73, 70, 67, 0, T_PSICO, T_NESSUNO, 115, 0 },  // 97 psico
  { "KRABBY", 99, 28, R_COMUN, 0x4C98, 30, 105, 90, 50, 1, T_ACQUA, T_NESSUNO, 25, 0 },  // 98 acqua
  { "KINGLER", 0, 0, R_EVO, 0x4C98, 55, 130, 115, 75, 1, T_ACQUA, T_NESSUNO, 50, 0 },  // 99 acqua
  { "VOLTORB", 101, 30, R_COMUN, 0xBCA1, 40, 30, 50, 100, 0, T_ELETTRO, T_NESSUNO, 55, 0 },  // 100 elettro
  { "ELECTRODE", 0, 0, R_EVO, 0xBCA1, 60, 50, 70, 150, 0, T_ELETTRO, T_NESSUNO, 80, 0 },  // 101 elettro
  { "EXEGGCUTE", 103, 30, R_COMUN, 0x3C49, 60, 40, 80, 40, 2, T_ERBA, T_PSICO, 60, 0 },  // 102 erba
  { "EXEGGUTOR", 0, 0, R_EVO, 0x3C49, 95, 95, 85, 55, 2, T_ERBA, T_PSICO, 125, 0 },  // 103 erba
  { "CUBONE", 105, 28, R_COMUN, 0xB447, 50, 50, 95, 35, 4, T_TERRA, T_NESSUNO, 40, 0 },  // 104 terra
  { "MAROWAK", 0, 0, R_EVO, 0xB447, 60, 80, 110, 45, 4, T_TERRA, T_NESSUNO, 50, 0 },  // 105 terra
  { "HITMONLEE", 0, 0, R_RARO, 0xA2A5, 50, 120, 53, 87, 0, T_LOTTA, T_NESSUNO, 35, 0 },  // 106 lotta
  { "HITMONCHAN", 0, 0, R_RARO, 0xA2A5, 50, 105, 79, 76, 0, T_LOTTA, T_NESSUNO, 35, 0 },  // 107 lotta
  { "LICKITUNG", 0, 0, R_RARO, 0x8C4D, 90, 55, 75, 30, 0, T_NORMALE, T_NESSUNO, 60, 0 },  // 108 normale
  { "KOFFING", 110, 35, R_COMUN, 0x8A73, 40, 65, 95, 35, 0, T_VELENO, T_NESSUNO, 60, 0 },  // 109 veleno
  { "WEEZING", 0, 0, R_EVO, 0x8A73, 65, 90, 120, 60, 0, T_VELENO, T_NESSUNO, 85, 0 },  // 110 veleno
  { "RHYHORN", 112, 42, R_RARO, 0xB447, 80, 85, 95, 25, 4, T_TERRA, T_ROCCIA, 30, 0 },  // 111 terra
  { "RHYDON", 0, 0, R_EVO, 0xB447, 105, 130, 120, 40, 4, T_TERRA, T_ROCCIA, 45, 0 },  // 112 terra
  { "CHANSEY", 0, 0, R_RARO, 0x8C4D, 250, 5, 5, 50, 0, T_NORMALE, T_NESSUNO, 105, 0 },  // 113 normale
  { "TANGELA", 0, 0, R_RARO, 0x3C49, 65, 55, 115, 60, 2, T_ERBA, T_NESSUNO, 100, 0 },  // 114 erba
  { "KANGASKHAN", 0, 0, R_RARO, 0x8C4D, 105, 95, 80, 90, 0, T_NORMALE, T_NESSUNO, 40, 0 },  // 115 normale
  { "HORSEA", 117, 32, R_COMUN, 0x4C98, 30, 40, 70, 60, 1, T_ACQUA, T_NESSUNO, 70, 0 },  // 116 acqua
  { "SEADRA", 0, 0, R_EVO, 0x4C98, 55, 65, 95, 85, 1, T_ACQUA, T_NESSUNO, 95, 0 },  // 117 acqua
  { "GOLDEEN", 119, 33, R_COMUN, 0x4C98, 45, 67, 60, 63, 1, T_ACQUA, T_NESSUNO, 50, 0 },  // 118 acqua
  { "SEAKING", 0, 0, R_EVO, 0x4C98, 80, 92, 65, 68, 1, T_ACQUA, T_NESSUNO, 80, 0 },  // 119 acqua
  { "STARYU", 121, 30, R_COMUN, 0x4C98, 30, 45, 55, 85, 1, T_ACQUA, T_NESSUNO, 70, 0 },  // 120 acqua
  { "STARMIE", 0, 0, R_EVO, 0x4C98, 60, 75, 85, 115, 1, T_ACQUA, T_PSICO, 100, 0 },  // 121 acqua
  { "MR. MIME", 0, 0, R_RARO, 0xD28F, 40, 45, 65, 90, 0, T_PSICO, T_NESSUNO, 100, 0 },  // 122 psico
  { "SCYTHER", 0, 0, R_RARO, 0x7CC4, 70, 110, 80, 105, 2, T_COLEOTTERO, T_VOLANTE, 55, 0 },  // 123 coleottero
  { "JYNX", 0, 0, R_RARO, 0x4DB8, 65, 50, 35, 95, 5, T_GHIACCIO, T_PSICO, 95, 0 },  // 124 ghiaccio
  { "ELECTABUZZ", 0, 0, R_RARO, 0xBCA1, 65, 83, 57, 105, 0, T_ELETTRO, T_NESSUNO, 85, 0 },  // 125 elettro
  { "MAGMAR", 0, 0, R_RARO, 0xEA87, 65, 95, 57, 93, 3, T_FUOCO, T_NESSUNO, 85, 0 },  // 126 fuoco
  { "PINSIR", 0, 0, R_RARO, 0x7CC4, 65, 125, 100, 85, 2, T_COLEOTTERO, T_NESSUNO, 55, 0 },  // 127 coleottero
  { "TAUROS", 0, 0, R_RARO, 0x8C4D, 75, 100, 95, 110, 0, T_NORMALE, T_NESSUNO, 70, 0 },  // 128 normale
  { "MAGIKARP", 130, 20, R_COMUN, 0x4C98, 20, 10, 55, 80, 1, T_ACQUA, T_NESSUNO, 20, 0 },  // 129 acqua
  { "GYARADOS", 0, 0, R_EVO, 0x4C98, 95, 125, 79, 81, 1, T_ACQUA, T_VOLANTE, 100, 0 },  // 130 acqua
  { "LAPRAS", 0, 0, R_RARO, 0x4C98, 130, 85, 80, 60, 1, T_ACQUA, T_GHIACCIO, 95, 0 },  // 131 acqua
  { "DITTO", 0, 0, R_RARO, 0x8C4D, 48, 48, 48, 48, 0, T_NORMALE, T_NESSUNO, 48, 0 },  // 132 normale
  { "EEVEE", 134, 30, R_COMUN, 0x8C4D, 55, 55, 50, 55, 0, T_NORMALE, T_NESSUNO, 65, 0 },  // 133 normale
  { "VAPOREON", 0, 0, R_EVO, 0x4C98, 130, 65, 60, 65, 1, T_ACQUA, T_NESSUNO, 110, 0 },  // 134 acqua
  { "JOLTEON", 0, 0, R_EVO, 0xBCA1, 65, 65, 60, 130, 0, T_ELETTRO, T_NESSUNO, 110, 0 },  // 135 elettro
  { "FLAREON", 0, 0, R_EVO, 0xEA87, 65, 130, 60, 65, 3, T_FUOCO, T_NESSUNO, 110, 0 },  // 136 fuoco
  { "PORYGON", 0, 0, R_RARO, 0x8C4D, 65, 60, 70, 40, 0, T_NORMALE, T_NESSUNO, 75, 0 },  // 137 normale
  { "OMANYTE", 139, 40, R_RARO, 0x9407, 35, 40, 100, 35, 1, T_ROCCIA, T_ACQUA, 90, 0 },  // 138 roccia
  { "OMASTAR", 0, 0, R_EVO, 0x9407, 70, 60, 125, 55, 1, T_ROCCIA, T_ACQUA, 115, 0 },  // 139 roccia
  { "KABUTO", 141, 40, R_RARO, 0x9407, 30, 80, 90, 55, 1, T_ROCCIA, T_ACQUA, 45, 0 },  // 140 roccia
  { "KABUTOPS", 0, 0, R_EVO, 0x9407, 60, 115, 105, 80, 1, T_ROCCIA, T_ACQUA, 70, 0 },  // 141 roccia
  { "AERODACTYL", 0, 0, R_RARO, 0x9407, 80, 105, 65, 130, 4, T_ROCCIA, T_VOLANTE, 60, 0 },  // 142 roccia
  { "SNORLAX", 0, 0, R_RARO, 0x8C4D, 160, 110, 65, 30, 0, T_NORMALE, T_NESSUNO, 65, 0 },  // 143 normale
  { "ARTICUNO", 0, 0, R_LEGENDARIO, 0x4DB8, 90, 85, 100, 85, 5, T_GHIACCIO, T_VOLANTE, 125, 0 },  // 144 ghiaccio
  { "ZAPDOS", 0, 0, R_LEGENDARIO, 0xBCA1, 90, 90, 85, 100, 0, T_ELETTRO, T_VOLANTE, 125, 0 },  // 145 elettro
  { "MOLTRES", 0, 0, R_LEGENDARIO, 0xEA87, 90, 100, 90, 90, 3, T_FUOCO, T_VOLANTE, 125, 0 },  // 146 fuoco
  { "DRATINI", 148, 30, R_RARO, 0x5A98, 41, 64, 45, 50, 1, T_DRAGO, T_NESSUNO, 50, 0 },  // 147 drago
  { "DRAGONAIR", 149, 55, R_EVO, 0x5A98, 61, 84, 65, 70, 1, T_DRAGO, T_NESSUNO, 70, 0 },  // 148 drago
  { "DRAGONITE", 0, 0, R_EVO, 0x5A98, 91, 134, 95, 80, 1, T_DRAGO, T_VOLANTE, 100, 0 },  // 149 drago
  { "MEWTWO", 0, 0, R_LEGENDARIO, 0xD28F, 106, 110, 90, 130, 0, T_PSICO, T_NESSUNO, 154, 0 },  // 150 psico
  { "MEW", 0, 0, R_LEGENDARIO, 0xD28F, 100, 100, 100, 100, 0, T_PSICO, T_NESSUNO, 100, 0 },  // 151 psico
};

// Nomi ufficiali per lingua. FR e DE sono le uniche lingue latine che differiscono
// dall'inglese in gen 1 (ES/IT/PT usano quello di DEX_TBL); JA e KO vanno in UTF-8,
// che si disegna con il font U8g2. nullptr = senza nome proprio.
static const char *const DEX_NAME_FR[DEX_COUNT + 1] = {
  nullptr, "BULBIZARRE", "HERBIZARRE", "FLORIZARRE",
  "SALAMECHE", "REPTINCEL", "DRACAUFEU", "CARAPUCE",
  "CARABAFFE", "TORTANK", "CHENIPAN", "CHRYSACIER",
  "PAPILUSION", "ASPICOT", "COCONFORT", "DARDARGNAN",
  "ROUCOOL", "ROUCOUPS", "ROUCARNAGE", nullptr,
  "RATTATAC", "PIAFABEC", "RAPASDEPIC", "ABO",
  nullptr, nullptr, nullptr, "SABELETTE",
  "SABLAIREAU", nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, "MELOFEE",
  "MELODELFE", "GOUPIX", "FEUNARD", "RONDOUDOU",
  "GRODOUDOU", "NOSFERAPTI", "NOSFERALTO", "MYSTHERBE",
  "ORTIDE", "RAFFLESIA", nullptr, nullptr,
  "MIMITOSS", "AEROMITE", "TAUPIQUEUR", "TRIOPIKEUR",
  "MIAOUSS", nullptr, "PSYKOKWAK", "AKWAKWAK",
  "FEROSINGE", "COLOSSINGE", "CANINOS", "ARCANIN",
  "PTITARD", "TETARTE", "TARTARD", nullptr,
  nullptr, nullptr, "MACHOC", "MACHOPEUR",
  "MACKOGNEUR", "CHETIFLOR", "BOUSTIFLOR", "EMPIFLOR",
  nullptr, nullptr, "RACAILLOU", "GRAVALANCH",
  "GROLEM", nullptr, "GALOPA", "RAMOLOSS",
  "FLAGADOSS", "MAGNETI", nullptr, "CANARTICHO",
  nullptr, nullptr, "OTARIA", "LAMANTINE",
  "TADMORV", "GROTADMORV", "KOKIYAS", "CRUSTABRI",
  "FANTOMINUS", "SPECTRUM", "ECTOPLASMA", nullptr,
  "SOPORIFIK", "HYPNOMADE", nullptr, "KRABBOSS",
  "VOLTORBE", nullptr, "NOEUNOEUF", "NOADKOKO",
  "OSSELAIT", "OSSATUEUR", "KICKLEE", "TYGNON",
  "EXCELANGUE", "SMOGO", "SMOGOGO", "RHINOCORNE",
  "RHINOFEROS", "LEVEINARD", "SAQUEDENEU", "KANGOUREX",
  "HYPOTREMPE", "HYPOCEAN", "POISSIRENE", "POISSOROY",
  "STARI", "STAROSS", "M. MIME", "INSECATEUR",
  "LIPPOUTOU", "ELEKTEK", nullptr, "SCARABRUTE",
  nullptr, "MAGICARPE", "LEVIATOR", "LOKHLASS",
  "METAMORPH", "EVOLI", "AQUALI", "VOLTALI",
  "PYROLI", nullptr, "AMONITA", "AMONISTAR",
  nullptr, nullptr, "PTERA", "RONFLEX",
  "ARTIKODIN", "ELECTHOR", "SULFURA", "MINIDRACO",
  "DRACO", "DRACOLOSSE", nullptr, nullptr,
};

static const char *const DEX_NAME_DE[DEX_COUNT + 1] = {
  nullptr, "BISASAM", "BISAKNOSP", "BISAFLOR",
  "GLUMANDA", "GLUTEXO", "GLURAK", "SCHIGGY",
  "SCHILLOK", "TURTOK", "RAUPY", "SAFCON",
  "SMETTBO", "HORNLIU", "KOKUNA", "BIBOR",
  "TAUBSI", "TAUBOGA", "TAUBOSS", "RATTFRATZ",
  "RATTIKARL", "HABITAK", "IBITAK", "RETTAN",
  nullptr, nullptr, nullptr, "SANDAN",
  "SANDAMER", nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, "PIEPI",
  "PIXI", nullptr, "VULNONA", "PUMMELUFF",
  "KNUDDELUFF", nullptr, nullptr, "MYRAPLA",
  "DUFLOR", "GIFLOR", nullptr, "PARASEK",
  "BLUZUK", "OMOT", "DIGDA", "DIGDRI",
  "MAUZI", "SNOBILIKAT", "ENTON", "ENTORON",
  "MENKI", "RASAFF", "FUKANO", "ARKANI",
  "QUAPSEL", "QUAPUTZI", "QUAPPO", nullptr,
  nullptr, "SIMSALA", "MACHOLLO", "MASCHOCK",
  "MACHOMEI", "KNOFENSA", "ULTRIGARIA", "SARZENIA",
  "TENTACHA", "TENTOXA", "KLEINSTEIN", "GEOROK",
  "GEOWAZ", "PONITA", "GALLOPA", "FLEGMON",
  "LAHMUS", "MAGNETILO", nullptr, "PORENTA",
  "DODU", "DODRI", "JUROB", "JUGONG",
  "SLEIMA", "SLEIMOK", "MUSCHAS", "AUSTOS",
  "NEBULAK", "ALPOLLO", nullptr, nullptr,
  "TRAUMATO", nullptr, nullptr, nullptr,
  "VOLTOBAL", "LEKTROBAL", "OWEI", "KOKOWEI",
  "TRAGOSSO", "KNOGGA", "KICKLEE", "NOCKCHAN",
  "SCHLURP", "SMOGON", "SMOGMOG", "RIHORN",
  "RIZEROS", "CHANEIRA", nullptr, "KANGAMA",
  "SEEPER", "SEEMON", "GOLDINI", "GOLKING",
  "STERNDU", nullptr, "PANTIMOS", "SICHLOR",
  "ROSSANA", "ELEKTEK", nullptr, nullptr,
  nullptr, "KARPADOR", "GARADOS", nullptr,
  nullptr, "EVOLI", "AQUANA", "BLITZA",
  "FLAMARA", nullptr, "AMONITAS", "AMOROSO",
  nullptr, nullptr, nullptr, "RELAXO",
  "ARKTOS", nullptr, "LAVADOS", nullptr,
  "DRAGONIR", "DRAGORAN", "MEWTU", nullptr,
};

static const char *const DEX_NAME_JA[DEX_COUNT + 1] = {
  nullptr, "フシギダネ", "フシギソウ", "フシギバナ",
  "ヒトカゲ", "リザード", "リザードン", "ゼニガメ",
  "カメール", "カメックス", "キャタピー", "トランセル",
  "バタフリー", "ビードル", "コクーン", "スピアー",
  "ポッポ", "ピジョン", "ピジョット", "コラッタ",
  "ラッタ", "オニスズメ", "オニドリル", "アーボ",
  "アーボック", "ピカチュウ", "ライチュウ", "サンド",
  "サンドパン", "ニドランF", "ニドリーナ", "ニドクイン",
  "ニドランM", "ニドリーノ", "ニドキング", "ピッピ",
  "ピクシー", "ロコン", "キュウコン", "プリン",
  "プクリン", "ズバット", "ゴルバット", "ナゾノクサ",
  "クサイハナ", "ラフレシア", "パラス", "パラセクト",
  "コンパン", "モルフォン", "ディグダ", "ダグトリオ",
  "ニャース", "ペルシアン", "コダック", "ゴルダック",
  "マンキー", "オコリザル", "ガーディ", "ウインディ",
  "ニョロモ", "ニョロゾ", "ニョロボン", "ケーシィ",
  "ユンゲラー", "フーディン", "ワンリキー", "ゴーリキー",
  "カイリキー", "マダツボミ", "ウツドン", "ウツボット",
  "メノクラゲ", "ドククラゲ", "イシツブテ", "ゴローン",
  "ゴローニャ", "ポニータ", "ギャロップ", "ヤドン",
  "ヤドラン", "コイル", "レアコイル", "カモネギ",
  "ドードー", "ドードリオ", "パウワウ", "ジュゴン",
  "ベトベター", "ベトベトン", "シェルダー", "パルシェン",
  "ゴース", "ゴースト", "ゲンガー", "イワーク",
  "スリープ", "スリーパー", "クラブ", "キングラー",
  "ビリリダマ", "マルマイン", "タマタマ", "ナッシー",
  "カラカラ", "ガラガラ", "サワムラー", "エビワラー",
  "ベロリンガ", "ドガース", "マタドガス", "サイホーン",
  "サイドン", "ラッキー", "モンジャラ", "ガルーラ",
  "タッツー", "シードラ", "トサキント", "アズマオウ",
  "ヒトデマン", "スターミー", "バリヤード", "ストライク",
  "ルージュラ", "エレブー", "ブーバー", "カイロス",
  "ケンタロス", "コイキング", "ギャラドス", "ラプラス",
  "メタモン", "イーブイ", "シャワーズ", "サンダース",
  "ブースター", "ポリゴン", "オムナイト", "オムスター",
  "カブト", "カブトプス", "プテラ", "カビゴン",
  "フリーザー", "サンダー", "ファイヤー", "ミニリュウ",
  "ハクリュー", "カイリュー", "ミュウツー", "ミュウ",
};

static const char *const DEX_NAME_KO[DEX_COUNT + 1] = {
  nullptr, "이상해씨", "이상해풀", "이상해꽃",
  "파이리", "리자드", "리자몽", "꼬부기",
  "어니부기", "거북왕", "캐터피", "단데기",
  "버터플", "뿔충이", "딱충이", "독침붕",
  "구구", "피죤", "피죤투", "꼬렛",
  "레트라", "깨비참", "깨비드릴조", "아보",
  "아보크", "피카츄", "라이츄", "모래두지",
  "고지", "니드런 암", "니드리나", "니드퀸",
  "니드런 수", "니드리노", "니드킹", "삐삐",
  "픽시", "식스테일", "나인테일", "푸린",
  "푸크린", "주뱃", "골뱃", "뚜벅쵸",
  "냄새꼬", "라플레시아", "파라스", "파라섹트",
  "콘팡", "도나리", "디그다", "닥트리오",
  "나옹", "페르시온", "고라파덕", "골덕",
  "망키", "성원숭", "가디", "윈디",
  "발챙이", "슈륙챙이", "강챙이", "캐이시",
  "윤겔라", "후딘", "알통몬", "근육몬",
  "괴력몬", "모다피", "우츠동", "우츠보트",
  "왕눈해", "독파리", "꼬마돌", "데구리",
  "딱구리", "포니타", "날쌩마", "야돈",
  "야도란", "코일", "레어코일", "파오리",
  "두두", "두트리오", "쥬쥬", "쥬레곤",
  "질퍽이", "질뻐기", "셀러", "파르셀",
  "고오스", "고우스트", "팬텀", "롱스톤",
  "슬리프", "슬리퍼", "크랩", "킹크랩",
  "찌리리공", "붐볼", "아라리", "나시",
  "탕구리", "텅구리", "시라소몬", "홍수몬",
  "내루미", "또가스", "또도가스", "뿔카노",
  "코뿌리", "럭키", "덩쿠리", "캥카",
  "쏘드라", "시드라", "콘치", "왕콘치",
  "별가사리", "아쿠스타", "마임맨", "스라크",
  "루주라", "에레브", "마그마", "쁘사이저",
  "켄타로스", "잉어킹", "갸라도스", "라프라스",
  "메타몽", "이브이", "샤미드", "쥬피썬더",
  "부스터", "폴리곤", "암나이트", "암스타",
  "투구", "투구푸스", "프테라", "잠만보",
  "프리져", "썬더", "파이어", "미뇽",
  "신뇽", "망나뇽", "뮤츠", "뮤",
};

// Nome della specie nella lingua attiva (ricade su quello di DEX_TBL se quella
// lingua non ha un nome proprio per essa).
static inline const char *dexName(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return DEX_TBL[0].name;
  const char *n = (gLang == LANG_FR)   ? DEX_NAME_FR[dex]
                  : (gLang == LANG_DE) ? DEX_NAME_DE[dex]
                  : (gLang == LANG_JA) ? DEX_NAME_JA[dex]
                  : (gLang == LANG_KO) ? DEX_NAME_KO[dex]
                                       : nullptr;
  return n ? n : DEX_TBL[dex].name;
}

// il primo uovo della partita: iniziali classici
static const int16_t CLASSIC_DEX[] = { 1, 4, 7, 25, 133 };
#define NUM_CLASSIC_DEX 5
