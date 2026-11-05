#pragma once
#include <stdint.h>
#include "dex.h"  // DEX_COUNT, tipi T_*, gLang

// GENERATO da tools/gen_dex.py da tools/dex_moves.py, dex_types.py e
// dex_move_names.py - non modificare

// Mosse e learnset di Rosso/Blu (pret/pokered), nomi da PokeAPI. Il significato
// di p1/p2 per ogni effetto è il contratto con il motore di battaglia.
// Modello degli effetti derivato da DylanPDao/TamaPoke (MIT).

// categoria per tipo, come in Gen 1: fisica sotto T_FUOCO, speciale da lì in poi
#define TYPE_IS_SPECIAL(t) ((t) >= T_FUOCO)

// efficacia x2, così 1/2 è un intero: 0 immune, 1 = 1/2, 2 = 1x, 4 = 2x
// TYPE_CHART[attaccante][difensore]; con due tipi si moltiplica e si divide per 4
static const uint8_t TYPE_CHART[TYPE_COUNT][TYPE_COUNT] = {
  { 2, 2, 2, 2, 2, 1, 2, 0, 2, 2, 2, 2, 2, 2, 2 },  // normale
  { 4, 2, 1, 1, 2, 4, 1, 0, 2, 2, 2, 2, 1, 4, 2 },  // lotta
  { 2, 4, 2, 2, 2, 1, 4, 2, 2, 2, 4, 1, 2, 2, 2 },  // volante
  { 2, 2, 2, 1, 1, 1, 4, 1, 2, 2, 4, 2, 2, 2, 2 },  // veleno
  { 2, 2, 0, 4, 2, 4, 1, 2, 4, 2, 1, 4, 2, 2, 2 },  // terra
  { 2, 1, 4, 2, 1, 2, 4, 2, 4, 2, 2, 2, 2, 4, 2 },  // roccia
  { 2, 1, 1, 4, 2, 2, 2, 1, 1, 2, 4, 2, 4, 2, 2 },  // coleottero
  { 0, 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 0, 2, 2 },  // spettro
  { 2, 2, 2, 2, 2, 1, 4, 2, 1, 1, 4, 2, 2, 4, 1 },  // fuoco
  { 2, 2, 2, 2, 4, 4, 2, 2, 4, 1, 1, 2, 2, 2, 1 },  // acqua
  { 2, 2, 1, 1, 4, 4, 1, 2, 1, 4, 1, 2, 2, 2, 1 },  // erba
  { 2, 2, 4, 2, 0, 2, 2, 2, 2, 4, 1, 1, 2, 2, 1 },  // elettro
  { 2, 4, 2, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 2 },  // psico
  { 2, 2, 4, 2, 4, 2, 2, 2, 2, 1, 4, 2, 2, 1, 4 },  // ghiaccio
  { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4 },  // drago
};

// effetti (EFFETTI di tools/dex_moves.py): valore = indice, i nuovi vanno in fondo
enum : uint8_t {
  EF_NESSUNO = 0,  // danno semplice
  EF_STADIO = 1,  // mossa di stato: p1 = statistica (STATISTICHE), p2 = stadi; positivi su chi la usa, negativi sul bersaglio
  EF_RINCULO = 2,  // p1 = denominatore: chi la usa subisce 1/p1 del danno inflitto
  EF_ASSORBI = 3,  // p1 = % del danno inflitto recuperata; p2 = 1: fallisce se il bersaglio non dorme (Mangiasogni)
  EF_DANNO_LIVELLO = 4,  // danno pari al livello di chi la usa, ignora statistiche e tipi; p1 = 100 esatto, 150 = casuale tra 1 e 1,5 volte il livello
  EF_DANNO_FISSO = 5,  // p1 = danno fisso, ignora statistiche
  EF_PRIORITA = 6,  // p1 = priorità: agisce prima delle mosse con priorità minore
  EF_MAI_FALLISCE = 7,  // salta il tiro di precisione
  EF_MULTIPLO = 8,  // p1 = colpi (0 = da 2 a 5), potenza per colpo; p2 = % di avvelenare (Doppio Ago)
  EF_CURA = 9,  // mossa di stato: p1 = % dei PS massimi recuperata
  EF_RICARICA = 10,  // chi la usa perde il turno seguente (non se manda KO, come in Gen 1)
  EF_CARICA = 11,  // il primo turno carica, il secondo colpisce; p1 = 1: invulnerabile mentre carica
  EF_STADIO_SECONDARIO = 12,  // danno + p2 % di abbassare di 1 stadio la statistica p1 del bersaglio
  EF_STATO = 13,  // mossa di stato: infligge lo stato p1 (STATI) se va a segno
  EF_STATO_SECONDARIO = 14,  // danno + p2 % di infliggere lo stato p1 (STATI); paralisi, scottatura e congelamento non colpiscono un bersaglio del tipo della mossa, il veleno non colpisce i tipi veleno
  EF_RIPOSO = 15,  // mossa di stato: recupera tutti i PS e chi la usa dorme 2 turni
  EF_PARASSISEME = 16,  // mossa di stato: ogni turno 1/16 dei PS massimi passa dal bersaglio a chi la usa; non sul tipo erba
  EF_TRASFORMAZIONE = 17,  // copia specie, tipi, statistiche (non i PS) e mosse del bersaglio, con 5 PP ciascuna
  EF_META_PS = 18,  // danno pari a metà dei PS attuali del bersaglio, minimo 1
  EF_KO = 19,  // manda KO in un colpo; fallisce se chi la usa è più lento
  EF_ESPLOSIONE = 20,  // chi la usa va KO; la difesa del bersaglio conta la metà
  EF_CALCIO_SALTO = 21,  // se manca, chi la usa subisce 1 PS di danno
  EF_INTRAPPOLA = 22,  // colpisce per 2-5 turni e intanto il bersaglio non può agire
  EF_FURIA = 23,  // si ripete per 2-3 turni, poi chi la usa resta confuso
  EF_IRA = 24,  // si ripete fino alla fine; ogni colpo subito alza di 1 stadio l'attacco
  EF_BRUTTO_COLPO = 25,  // danno con probabilità di brutto colpo molto più alta
  EF_NEBBIA = 26,  // mossa di stato: protegge chi la usa dagli abbassamenti di stadio
  EF_CONCENTRAZIONE = 27,  // mossa di stato: aumenta i brutti colpi (in Gen 1 per un bug li riduce: decide il motore)
  EF_BARRIERA = 28,  // mossa di stato: p1 = 0 raddoppia la difesa (Riflesso), 1 la speciale (Schermoluce), finché chi la usa resta in campo
  EF_AZZERA = 29,  // mossa di stato: azzera gli stadi di entrambi e gli stati volatili
  EF_CONVERSIONE = 30,  // mossa di stato: chi la usa prende i tipi del bersaglio
  EF_NULLA = 31,  // non succede niente (Splash): la mossa si può comunque scegliere
};

// stati (p1 di EF_STATO / EF_STATO_SECONDARIO)
enum : uint8_t {
  ST_NESSUNO = 0,
  ST_PARALISI = 1,
  ST_SCOTTATURA = 2,
  ST_VELENO = 3,
  ST_TOSSICO = 4,
  ST_SONNO = 5,
  ST_CONGELAMENTO = 6,
  ST_CONFUSIONE = 7,
  ST_TENTENNAMENTO = 8,
};

// statistiche (p1 di EF_STADIO / EF_STADIO_SECONDARIO)
enum : uint8_t {
  STAT_ATTACCO = 0,
  STAT_DIFESA = 1,
  STAT_VELOCITA = 2,
  STAT_SPECIALE = 3,
  STAT_PRECISIONE = 4,
  STAT_ELUSIONE = 5,
};

// id Gen 1 delle mosse: è il valore da persistere; gli id mancanti sono le mosse
// escluse (ESCLUSE di tools/dex_moves.py), con pp = 0 in MOVE_TBL
enum : uint8_t {
  MV_POUND = 1,
  MV_KARATE_CHOP = 2,
  MV_DOUBLE_SLAP = 3,
  MV_COMET_PUNCH = 4,
  MV_MEGA_PUNCH = 5,
  MV_PAY_DAY = 6,
  MV_FIRE_PUNCH = 7,
  MV_ICE_PUNCH = 8,
  MV_THUNDER_PUNCH = 9,
  MV_SCRATCH = 10,
  MV_VICE_GRIP = 11,
  MV_GUILLOTINE = 12,
  MV_RAZOR_WIND = 13,
  MV_SWORDS_DANCE = 14,
  MV_CUT = 15,
  MV_GUST = 16,
  MV_WING_ATTACK = 17,
  MV_FLY = 19,
  MV_BIND = 20,
  MV_SLAM = 21,
  MV_VINE_WHIP = 22,
  MV_STOMP = 23,
  MV_DOUBLE_KICK = 24,
  MV_MEGA_KICK = 25,
  MV_JUMP_KICK = 26,
  MV_ROLLING_KICK = 27,
  MV_SAND_ATTACK = 28,
  MV_HEADBUTT = 29,
  MV_HORN_ATTACK = 30,
  MV_FURY_ATTACK = 31,
  MV_HORN_DRILL = 32,
  MV_TACKLE = 33,
  MV_BODY_SLAM = 34,
  MV_WRAP = 35,
  MV_TAKE_DOWN = 36,
  MV_THRASH = 37,
  MV_DOUBLE_EDGE = 38,
  MV_TAIL_WHIP = 39,
  MV_POISON_STING = 40,
  MV_TWINEEDLE = 41,
  MV_PIN_MISSILE = 42,
  MV_LEER = 43,
  MV_BITE = 44,
  MV_GROWL = 45,
  MV_SING = 47,
  MV_SUPERSONIC = 48,
  MV_SONIC_BOOM = 49,
  MV_ACID = 51,
  MV_EMBER = 52,
  MV_FLAMETHROWER = 53,
  MV_MIST = 54,
  MV_WATER_GUN = 55,
  MV_HYDRO_PUMP = 56,
  MV_SURF = 57,
  MV_ICE_BEAM = 58,
  MV_BLIZZARD = 59,
  MV_PSYBEAM = 60,
  MV_BUBBLE_BEAM = 61,
  MV_AURORA_BEAM = 62,
  MV_HYPER_BEAM = 63,
  MV_PECK = 64,
  MV_DRILL_PECK = 65,
  MV_SUBMISSION = 66,
  MV_LOW_KICK = 67,
  MV_SEISMIC_TOSS = 69,
  MV_STRENGTH = 70,
  MV_ABSORB = 71,
  MV_MEGA_DRAIN = 72,
  MV_LEECH_SEED = 73,
  MV_GROWTH = 74,
  MV_RAZOR_LEAF = 75,
  MV_SOLAR_BEAM = 76,
  MV_POISON_POWDER = 77,
  MV_STUN_SPORE = 78,
  MV_SLEEP_POWDER = 79,
  MV_PETAL_DANCE = 80,
  MV_STRING_SHOT = 81,
  MV_DRAGON_RAGE = 82,
  MV_FIRE_SPIN = 83,
  MV_THUNDER_SHOCK = 84,
  MV_THUNDERBOLT = 85,
  MV_THUNDER_WAVE = 86,
  MV_THUNDER = 87,
  MV_ROCK_THROW = 88,
  MV_EARTHQUAKE = 89,
  MV_FISSURE = 90,
  MV_DIG = 91,
  MV_TOXIC = 92,
  MV_CONFUSION = 93,
  MV_PSYCHIC = 94,
  MV_HYPNOSIS = 95,
  MV_MEDITATE = 96,
  MV_AGILITY = 97,
  MV_QUICK_ATTACK = 98,
  MV_RAGE = 99,
  MV_NIGHT_SHADE = 101,
  MV_SCREECH = 103,
  MV_DOUBLE_TEAM = 104,
  MV_RECOVER = 105,
  MV_HARDEN = 106,
  MV_MINIMIZE = 107,
  MV_SMOKESCREEN = 108,
  MV_CONFUSE_RAY = 109,
  MV_WITHDRAW = 110,
  MV_DEFENSE_CURL = 111,
  MV_BARRIER = 112,
  MV_LIGHT_SCREEN = 113,
  MV_HAZE = 114,
  MV_REFLECT = 115,
  MV_FOCUS_ENERGY = 116,
  MV_SELF_DESTRUCT = 120,
  MV_EGG_BOMB = 121,
  MV_LICK = 122,
  MV_SMOG = 123,
  MV_SLUDGE = 124,
  MV_BONE_CLUB = 125,
  MV_FIRE_BLAST = 126,
  MV_WATERFALL = 127,
  MV_CLAMP = 128,
  MV_SWIFT = 129,
  MV_SKULL_BASH = 130,
  MV_SPIKE_CANNON = 131,
  MV_CONSTRICT = 132,
  MV_AMNESIA = 133,
  MV_KINESIS = 134,
  MV_SOFT_BOILED = 135,
  MV_HIGH_JUMP_KICK = 136,
  MV_GLARE = 137,
  MV_DREAM_EATER = 138,
  MV_POISON_GAS = 139,
  MV_BARRAGE = 140,
  MV_LEECH_LIFE = 141,
  MV_LOVELY_KISS = 142,
  MV_SKY_ATTACK = 143,
  MV_TRANSFORM = 144,
  MV_BUBBLE = 145,
  MV_DIZZY_PUNCH = 146,
  MV_SPORE = 147,
  MV_FLASH = 148,
  MV_PSYWAVE = 149,
  MV_SPLASH = 150,
  MV_ACID_ARMOR = 151,
  MV_CRABHAMMER = 152,
  MV_EXPLOSION = 153,
  MV_FURY_SWIPES = 154,
  MV_BONEMERANG = 155,
  MV_REST = 156,
  MV_ROCK_SLIDE = 157,
  MV_HYPER_FANG = 158,
  MV_SHARPEN = 159,
  MV_CONVERSION = 160,
  MV_TRI_ATTACK = 161,
  MV_SUPER_FANG = 162,
  MV_SLASH = 163,
  MV_STRUGGLE = 165,
};
#define MOVE_MAX_ID 165
#define MOVE_STRUGGLE MV_STRUGGLE  // ripiego di chi non ha mosse (Abra)

struct MoveEntry {
  uint8_t type;      // T_*
  uint8_t power;     // 0 = niente formula del danno
  uint8_t accuracy;  // %
  uint8_t pp;        // 0 = id senza mossa
  uint8_t effect;    // EF_*
  uint8_t p1;        // vedi EF_*
  int8_t p2;         // vedi EF_*; gli stadi sono con segno
};

static const MoveEntry MOVE_TBL[MOVE_MAX_ID + 1] = {
  { T_NORMALE, 0, 0, 0, EF_NESSUNO, 0, 0 },  // 0: esclusa o inutilizzato
  { T_NORMALE, 40, 100, 35, EF_NESSUNO, 0, 0 },  // 1 pound
  { T_NORMALE, 50, 100, 25, EF_BRUTTO_COLPO, 0, 0 },  // 2 karate-chop
  { T_NORMALE, 15, 85, 10, EF_MULTIPLO, 0, 0 },  // 3 double-slap
  { T_NORMALE, 18, 85, 15, EF_MULTIPLO, 0, 0 },  // 4 comet-punch
  { T_NORMALE, 80, 85, 20, EF_NESSUNO, 0, 0 },  // 5 mega-punch
  { T_NORMALE, 40, 100, 20, EF_NESSUNO, 0, 0 },  // 6 pay-day
  { T_FUOCO, 75, 100, 15, EF_STATO_SECONDARIO, 2, 10 },  // 7 fire-punch
  { T_GHIACCIO, 75, 100, 15, EF_STATO_SECONDARIO, 6, 10 },  // 8 ice-punch
  { T_ELETTRO, 75, 100, 15, EF_STATO_SECONDARIO, 1, 10 },  // 9 thunder-punch
  { T_NORMALE, 40, 100, 35, EF_NESSUNO, 0, 0 },  // 10 scratch
  { T_NORMALE, 55, 100, 30, EF_NESSUNO, 0, 0 },  // 11 vice-grip
  { T_NORMALE, 0, 30, 5, EF_KO, 0, 0 },  // 12 guillotine
  { T_NORMALE, 80, 75, 10, EF_CARICA, 0, 0 },  // 13 razor-wind
  { T_NORMALE, 0, 100, 30, EF_STADIO, 0, 2 },  // 14 swords-dance
  { T_NORMALE, 50, 95, 30, EF_NESSUNO, 0, 0 },  // 15 cut
  { T_NORMALE, 40, 100, 35, EF_NESSUNO, 0, 0 },  // 16 gust
  { T_VOLANTE, 35, 100, 35, EF_NESSUNO, 0, 0 },  // 17 wing-attack
  { T_NORMALE, 0, 0, 0, EF_NESSUNO, 0, 0 },  // 18: esclusa o inutilizzato
  { T_VOLANTE, 70, 95, 15, EF_CARICA, 1, 0 },  // 19 fly
  { T_NORMALE, 15, 75, 20, EF_INTRAPPOLA, 0, 0 },  // 20 bind
  { T_NORMALE, 80, 75, 20, EF_NESSUNO, 0, 0 },  // 21 slam
  { T_ERBA, 35, 100, 10, EF_NESSUNO, 0, 0 },  // 22 vine-whip
  { T_NORMALE, 65, 100, 20, EF_STATO_SECONDARIO, 8, 30 },  // 23 stomp
  { T_LOTTA, 30, 100, 30, EF_MULTIPLO, 2, 0 },  // 24 double-kick
  { T_NORMALE, 120, 75, 5, EF_NESSUNO, 0, 0 },  // 25 mega-kick
  { T_LOTTA, 70, 95, 25, EF_CALCIO_SALTO, 0, 0 },  // 26 jump-kick
  { T_LOTTA, 60, 85, 15, EF_STATO_SECONDARIO, 8, 30 },  // 27 rolling-kick
  { T_NORMALE, 0, 100, 15, EF_STADIO, 4, -1 },  // 28 sand-attack
  { T_NORMALE, 70, 100, 15, EF_STATO_SECONDARIO, 8, 30 },  // 29 headbutt
  { T_NORMALE, 65, 100, 25, EF_NESSUNO, 0, 0 },  // 30 horn-attack
  { T_NORMALE, 15, 85, 20, EF_MULTIPLO, 0, 0 },  // 31 fury-attack
  { T_NORMALE, 0, 30, 5, EF_KO, 0, 0 },  // 32 horn-drill
  { T_NORMALE, 35, 95, 35, EF_NESSUNO, 0, 0 },  // 33 tackle
  { T_NORMALE, 85, 100, 15, EF_STATO_SECONDARIO, 1, 30 },  // 34 body-slam
  { T_NORMALE, 15, 85, 20, EF_INTRAPPOLA, 0, 0 },  // 35 wrap
  { T_NORMALE, 90, 85, 20, EF_RINCULO, 4, 0 },  // 36 take-down
  { T_NORMALE, 90, 100, 20, EF_FURIA, 0, 0 },  // 37 thrash
  { T_NORMALE, 100, 100, 15, EF_RINCULO, 4, 0 },  // 38 double-edge
  { T_NORMALE, 0, 100, 30, EF_STADIO, 1, -1 },  // 39 tail-whip
  { T_VELENO, 15, 100, 35, EF_STATO_SECONDARIO, 3, 20 },  // 40 poison-sting
  { T_COLEOTTERO, 25, 100, 20, EF_MULTIPLO, 2, 20 },  // 41 twineedle
  { T_COLEOTTERO, 14, 85, 20, EF_MULTIPLO, 0, 0 },  // 42 pin-missile
  { T_NORMALE, 0, 100, 30, EF_STADIO, 1, -1 },  // 43 leer
  { T_NORMALE, 60, 100, 25, EF_STATO_SECONDARIO, 8, 10 },  // 44 bite
  { T_NORMALE, 0, 100, 40, EF_STADIO, 0, -1 },  // 45 growl
  { T_NORMALE, 0, 0, 0, EF_NESSUNO, 0, 0 },  // 46: esclusa o inutilizzato
  { T_NORMALE, 0, 55, 15, EF_STATO, 5, 0 },  // 47 sing
  { T_NORMALE, 0, 55, 20, EF_STATO, 7, 0 },  // 48 supersonic
  { T_NORMALE, 0, 90, 20, EF_DANNO_FISSO, 20, 0 },  // 49 sonic-boom
  { T_NORMALE, 0, 0, 0, EF_NESSUNO, 0, 0 },  // 50: esclusa o inutilizzato
  { T_VELENO, 40, 100, 30, EF_STADIO_SECONDARIO, 1, 33 },  // 51 acid
  { T_FUOCO, 40, 100, 25, EF_STATO_SECONDARIO, 2, 10 },  // 52 ember
  { T_FUOCO, 95, 100, 15, EF_STATO_SECONDARIO, 2, 10 },  // 53 flamethrower
  { T_GHIACCIO, 0, 100, 30, EF_NEBBIA, 0, 0 },  // 54 mist
  { T_ACQUA, 40, 100, 25, EF_NESSUNO, 0, 0 },  // 55 water-gun
  { T_ACQUA, 120, 80, 5, EF_NESSUNO, 0, 0 },  // 56 hydro-pump
  { T_ACQUA, 95, 100, 15, EF_NESSUNO, 0, 0 },  // 57 surf
  { T_GHIACCIO, 95, 100, 10, EF_STATO_SECONDARIO, 6, 10 },  // 58 ice-beam
  { T_GHIACCIO, 120, 90, 5, EF_STATO_SECONDARIO, 6, 10 },  // 59 blizzard
  { T_PSICO, 65, 100, 20, EF_STATO_SECONDARIO, 7, 10 },  // 60 psybeam
  { T_ACQUA, 65, 100, 20, EF_STADIO_SECONDARIO, 2, 33 },  // 61 bubble-beam
  { T_GHIACCIO, 65, 100, 20, EF_STADIO_SECONDARIO, 0, 33 },  // 62 aurora-beam
  { T_NORMALE, 150, 90, 5, EF_RICARICA, 0, 0 },  // 63 hyper-beam
  { T_VOLANTE, 35, 100, 35, EF_NESSUNO, 0, 0 },  // 64 peck
  { T_VOLANTE, 80, 100, 20, EF_NESSUNO, 0, 0 },  // 65 drill-peck
  { T_LOTTA, 80, 80, 25, EF_RINCULO, 4, 0 },  // 66 submission
  { T_LOTTA, 50, 90, 20, EF_STATO_SECONDARIO, 8, 30 },  // 67 low-kick
  { T_NORMALE, 0, 0, 0, EF_NESSUNO, 0, 0 },  // 68: esclusa o inutilizzato
  { T_LOTTA, 0, 100, 20, EF_DANNO_LIVELLO, 100, 0 },  // 69 seismic-toss
  { T_NORMALE, 80, 100, 15, EF_NESSUNO, 0, 0 },  // 70 strength
  { T_ERBA, 20, 100, 20, EF_ASSORBI, 50, 0 },  // 71 absorb
  { T_ERBA, 40, 100, 10, EF_ASSORBI, 50, 0 },  // 72 mega-drain
  { T_ERBA, 0, 90, 10, EF_PARASSISEME, 0, 0 },  // 73 leech-seed
  { T_NORMALE, 0, 100, 40, EF_STADIO, 3, 1 },  // 74 growth
  { T_ERBA, 55, 95, 25, EF_BRUTTO_COLPO, 0, 0 },  // 75 razor-leaf
  { T_ERBA, 120, 100, 10, EF_CARICA, 0, 0 },  // 76 solar-beam
  { T_VELENO, 0, 75, 35, EF_STATO, 3, 0 },  // 77 poison-powder
  { T_ERBA, 0, 75, 30, EF_STATO, 1, 0 },  // 78 stun-spore
  { T_ERBA, 0, 75, 15, EF_STATO, 5, 0 },  // 79 sleep-powder
  { T_ERBA, 70, 100, 20, EF_FURIA, 0, 0 },  // 80 petal-dance
  { T_COLEOTTERO, 0, 95, 40, EF_STADIO, 2, -1 },  // 81 string-shot
  { T_DRAGO, 0, 100, 10, EF_DANNO_FISSO, 40, 0 },  // 82 dragon-rage
  { T_FUOCO, 15, 70, 15, EF_INTRAPPOLA, 0, 0 },  // 83 fire-spin
  { T_ELETTRO, 40, 100, 30, EF_STATO_SECONDARIO, 1, 10 },  // 84 thunder-shock
  { T_ELETTRO, 95, 100, 15, EF_STATO_SECONDARIO, 1, 10 },  // 85 thunderbolt
  { T_ELETTRO, 0, 100, 20, EF_STATO, 1, 0 },  // 86 thunder-wave
  { T_ELETTRO, 120, 70, 10, EF_STATO_SECONDARIO, 1, 10 },  // 87 thunder
  { T_ROCCIA, 50, 65, 15, EF_NESSUNO, 0, 0 },  // 88 rock-throw
  { T_TERRA, 100, 100, 10, EF_NESSUNO, 0, 0 },  // 89 earthquake
  { T_TERRA, 0, 30, 5, EF_KO, 0, 0 },  // 90 fissure
  { T_TERRA, 100, 100, 10, EF_CARICA, 1, 0 },  // 91 dig
  { T_VELENO, 0, 85, 10, EF_STATO, 4, 0 },  // 92 toxic
  { T_PSICO, 50, 100, 25, EF_STATO_SECONDARIO, 7, 10 },  // 93 confusion
  { T_PSICO, 90, 100, 10, EF_STADIO_SECONDARIO, 3, 33 },  // 94 psychic
  { T_PSICO, 0, 60, 20, EF_STATO, 5, 0 },  // 95 hypnosis
  { T_PSICO, 0, 100, 40, EF_STADIO, 0, 1 },  // 96 meditate
  { T_PSICO, 0, 100, 30, EF_STADIO, 2, 2 },  // 97 agility
  { T_NORMALE, 40, 100, 30, EF_PRIORITA, 1, 0 },  // 98 quick-attack
  { T_NORMALE, 20, 100, 20, EF_IRA, 0, 0 },  // 99 rage
  { T_NORMALE, 0, 0, 0, EF_NESSUNO, 0, 0 },  // 100: esclusa o inutilizzato
  { T_SPETTRO, 0, 100, 15, EF_DANNO_LIVELLO, 100, 0 },  // 101 night-shade
  { T_NORMALE, 0, 0, 0, EF_NESSUNO, 0, 0 },  // 102: esclusa o inutilizzato
  { T_NORMALE, 0, 85, 40, EF_STADIO, 1, -2 },  // 103 screech
  { T_NORMALE, 0, 100, 15, EF_STADIO, 5, 1 },  // 104 double-team
  { T_NORMALE, 0, 100, 20, EF_CURA, 50, 0 },  // 105 recover
  { T_NORMALE, 0, 100, 30, EF_STADIO, 1, 1 },  // 106 harden
  { T_NORMALE, 0, 100, 20, EF_STADIO, 5, 1 },  // 107 minimize
  { T_NORMALE, 0, 100, 20, EF_STADIO, 4, -1 },  // 108 smokescreen
  { T_SPETTRO, 0, 100, 10, EF_STATO, 7, 0 },  // 109 confuse-ray
  { T_ACQUA, 0, 100, 40, EF_STADIO, 1, 1 },  // 110 withdraw
  { T_NORMALE, 0, 100, 40, EF_STADIO, 1, 1 },  // 111 defense-curl
  { T_PSICO, 0, 100, 30, EF_STADIO, 1, 2 },  // 112 barrier
  { T_PSICO, 0, 100, 30, EF_BARRIERA, 1, 0 },  // 113 light-screen
  { T_GHIACCIO, 0, 100, 30, EF_AZZERA, 0, 0 },  // 114 haze
  { T_PSICO, 0, 100, 20, EF_BARRIERA, 0, 0 },  // 115 reflect
  { T_NORMALE, 0, 100, 30, EF_CONCENTRAZIONE, 0, 0 },  // 116 focus-energy
  { T_NORMALE, 0, 0, 0, EF_NESSUNO, 0, 0 },  // 117: esclusa o inutilizzato
  { T_NORMALE, 0, 0, 0, EF_NESSUNO, 0, 0 },  // 118: esclusa o inutilizzato
  { T_NORMALE, 0, 0, 0, EF_NESSUNO, 0, 0 },  // 119: esclusa o inutilizzato
  { T_NORMALE, 130, 100, 5, EF_ESPLOSIONE, 0, 0 },  // 120 self-destruct
  { T_NORMALE, 100, 75, 10, EF_NESSUNO, 0, 0 },  // 121 egg-bomb
  { T_SPETTRO, 20, 100, 30, EF_STATO_SECONDARIO, 1, 30 },  // 122 lick
  { T_VELENO, 20, 70, 20, EF_STATO_SECONDARIO, 3, 40 },  // 123 smog
  { T_VELENO, 65, 100, 20, EF_STATO_SECONDARIO, 3, 40 },  // 124 sludge
  { T_TERRA, 65, 85, 20, EF_STATO_SECONDARIO, 8, 10 },  // 125 bone-club
  { T_FUOCO, 120, 85, 5, EF_STATO_SECONDARIO, 2, 30 },  // 126 fire-blast
  { T_ACQUA, 80, 100, 15, EF_NESSUNO, 0, 0 },  // 127 waterfall
  { T_ACQUA, 35, 75, 10, EF_INTRAPPOLA, 0, 0 },  // 128 clamp
  { T_NORMALE, 60, 100, 20, EF_MAI_FALLISCE, 0, 0 },  // 129 swift
  { T_NORMALE, 100, 100, 15, EF_CARICA, 0, 0 },  // 130 skull-bash
  { T_NORMALE, 20, 100, 15, EF_MULTIPLO, 0, 0 },  // 131 spike-cannon
  { T_NORMALE, 10, 100, 35, EF_STADIO_SECONDARIO, 2, 33 },  // 132 constrict
  { T_PSICO, 0, 100, 20, EF_STADIO, 3, 2 },  // 133 amnesia
  { T_PSICO, 0, 80, 15, EF_STADIO, 4, -1 },  // 134 kinesis
  { T_NORMALE, 0, 100, 10, EF_CURA, 50, 0 },  // 135 soft-boiled
  { T_LOTTA, 85, 90, 20, EF_CALCIO_SALTO, 0, 0 },  // 136 high-jump-kick
  { T_NORMALE, 0, 75, 30, EF_STATO, 1, 0 },  // 137 glare
  { T_PSICO, 100, 100, 15, EF_ASSORBI, 50, 1 },  // 138 dream-eater
  { T_VELENO, 0, 55, 40, EF_STATO, 3, 0 },  // 139 poison-gas
  { T_NORMALE, 15, 85, 20, EF_MULTIPLO, 0, 0 },  // 140 barrage
  { T_COLEOTTERO, 20, 100, 15, EF_ASSORBI, 50, 0 },  // 141 leech-life
  { T_NORMALE, 0, 75, 10, EF_STATO, 5, 0 },  // 142 lovely-kiss
  { T_VOLANTE, 140, 90, 5, EF_CARICA, 0, 0 },  // 143 sky-attack
  { T_NORMALE, 0, 100, 10, EF_TRASFORMAZIONE, 0, 0 },  // 144 transform
  { T_ACQUA, 20, 100, 30, EF_STADIO_SECONDARIO, 2, 33 },  // 145 bubble
  { T_NORMALE, 70, 100, 10, EF_NESSUNO, 0, 0 },  // 146 dizzy-punch
  { T_ERBA, 0, 100, 15, EF_STATO, 5, 0 },  // 147 spore
  { T_NORMALE, 0, 70, 20, EF_STADIO, 4, -1 },  // 148 flash
  { T_PSICO, 0, 80, 15, EF_DANNO_LIVELLO, 150, 0 },  // 149 psywave
  { T_NORMALE, 0, 100, 40, EF_NULLA, 0, 0 },  // 150 splash
  { T_VELENO, 0, 100, 40, EF_STADIO, 1, 2 },  // 151 acid-armor
  { T_ACQUA, 90, 85, 10, EF_BRUTTO_COLPO, 0, 0 },  // 152 crabhammer
  { T_NORMALE, 170, 100, 5, EF_ESPLOSIONE, 0, 0 },  // 153 explosion
  { T_NORMALE, 18, 80, 15, EF_MULTIPLO, 0, 0 },  // 154 fury-swipes
  { T_TERRA, 50, 90, 10, EF_MULTIPLO, 2, 0 },  // 155 bonemerang
  { T_PSICO, 0, 100, 10, EF_RIPOSO, 0, 0 },  // 156 rest
  { T_ROCCIA, 75, 90, 10, EF_NESSUNO, 0, 0 },  // 157 rock-slide
  { T_NORMALE, 80, 90, 15, EF_STATO_SECONDARIO, 8, 10 },  // 158 hyper-fang
  { T_NORMALE, 0, 100, 30, EF_STADIO, 0, 1 },  // 159 sharpen
  { T_NORMALE, 0, 100, 30, EF_CONVERSIONE, 0, 0 },  // 160 conversion
  { T_NORMALE, 80, 100, 10, EF_NESSUNO, 0, 0 },  // 161 tri-attack
  { T_NORMALE, 0, 90, 10, EF_META_PS, 0, 0 },  // 162 super-fang
  { T_NORMALE, 70, 100, 20, EF_BRUTTO_COLPO, 0, 0 },  // 163 slash
  { T_NORMALE, 0, 0, 0, EF_NESSUNO, 0, 0 },  // 164: esclusa o inutilizzato
  { T_NORMALE, 50, 100, 10, EF_RINCULO, 2, 0 },  // 165 struggle
};

// Learnset di Rosso/Blu senza MT, in layout CSR: le mosse della specie d sono
// LEARN_TBL[LEARN_OFS[d]] .. LEARN_TBL[LEARN_OFS[d + 1] - 1], per livello crescente;
// il livello 1 sono le mosse iniziali. Nessuna voce (Abra) = MOVE_STRUGGLE.
struct LearnEntry {
  uint8_t level;
  uint8_t move;  // MV_*
};

#define LEARN_COUNT 952
static const LearnEntry LEARN_TBL[LEARN_COUNT] = {
  // 1 bulbasaur
  { 1, MV_TACKLE },
  { 1, MV_GROWL },
  { 7, MV_LEECH_SEED },
  { 13, MV_VINE_WHIP },
  { 20, MV_POISON_POWDER },
  { 27, MV_RAZOR_LEAF },
  { 34, MV_GROWTH },
  { 41, MV_SLEEP_POWDER },
  { 48, MV_SOLAR_BEAM },
  // 2 ivysaur
  { 1, MV_TACKLE },
  { 1, MV_GROWL },
  { 1, MV_LEECH_SEED },
  { 13, MV_VINE_WHIP },
  { 22, MV_POISON_POWDER },
  { 30, MV_RAZOR_LEAF },
  { 38, MV_GROWTH },
  { 46, MV_SLEEP_POWDER },
  { 54, MV_SOLAR_BEAM },
  // 3 venusaur
  { 1, MV_TACKLE },
  { 1, MV_GROWL },
  { 1, MV_LEECH_SEED },
  { 1, MV_VINE_WHIP },
  { 22, MV_POISON_POWDER },
  { 30, MV_RAZOR_LEAF },
  { 43, MV_GROWTH },
  { 55, MV_SLEEP_POWDER },
  { 65, MV_SOLAR_BEAM },
  // 4 charmander
  { 1, MV_SCRATCH },
  { 1, MV_GROWL },
  { 9, MV_EMBER },
  { 15, MV_LEER },
  { 22, MV_RAGE },
  { 30, MV_SLASH },
  { 38, MV_FLAMETHROWER },
  { 46, MV_FIRE_SPIN },
  // 5 charmeleon
  { 1, MV_SCRATCH },
  { 1, MV_GROWL },
  { 1, MV_EMBER },
  { 15, MV_LEER },
  { 24, MV_RAGE },
  { 33, MV_SLASH },
  { 42, MV_FLAMETHROWER },
  { 56, MV_FIRE_SPIN },
  // 6 charizard
  { 1, MV_SCRATCH },
  { 1, MV_GROWL },
  { 1, MV_EMBER },
  { 1, MV_LEER },
  { 24, MV_RAGE },
  { 36, MV_SLASH },
  { 46, MV_FLAMETHROWER },
  { 55, MV_FIRE_SPIN },
  // 7 squirtle
  { 1, MV_TACKLE },
  { 1, MV_TAIL_WHIP },
  { 8, MV_BUBBLE },
  { 15, MV_WATER_GUN },
  { 22, MV_BITE },
  { 28, MV_WITHDRAW },
  { 35, MV_SKULL_BASH },
  { 42, MV_HYDRO_PUMP },
  // 8 wartortle
  { 1, MV_TACKLE },
  { 1, MV_TAIL_WHIP },
  { 1, MV_BUBBLE },
  { 15, MV_WATER_GUN },
  { 24, MV_BITE },
  { 31, MV_WITHDRAW },
  { 39, MV_SKULL_BASH },
  { 47, MV_HYDRO_PUMP },
  // 9 blastoise
  { 1, MV_TACKLE },
  { 1, MV_TAIL_WHIP },
  { 1, MV_BUBBLE },
  { 1, MV_WATER_GUN },
  { 24, MV_BITE },
  { 31, MV_WITHDRAW },
  { 42, MV_SKULL_BASH },
  { 52, MV_HYDRO_PUMP },
  // 10 caterpie
  { 1, MV_TACKLE },
  { 1, MV_STRING_SHOT },
  // 11 metapod
  { 1, MV_HARDEN },
  // 12 butterfree
  { 1, MV_CONFUSION },
  { 15, MV_POISON_POWDER },
  { 16, MV_STUN_SPORE },
  { 17, MV_SLEEP_POWDER },
  { 21, MV_SUPERSONIC },
  { 32, MV_PSYBEAM },
  // 13 weedle
  { 1, MV_POISON_STING },
  { 1, MV_STRING_SHOT },
  // 14 kakuna
  { 1, MV_HARDEN },
  // 15 beedrill
  { 1, MV_FURY_ATTACK },
  { 16, MV_FOCUS_ENERGY },
  { 20, MV_TWINEEDLE },
  { 25, MV_RAGE },
  { 30, MV_PIN_MISSILE },
  { 35, MV_AGILITY },
  // 16 pidgey
  { 1, MV_GUST },
  { 5, MV_SAND_ATTACK },
  { 12, MV_QUICK_ATTACK },
  { 28, MV_WING_ATTACK },
  { 36, MV_AGILITY },
  // 17 pidgeotto
  { 1, MV_GUST },
  { 1, MV_SAND_ATTACK },
  { 12, MV_QUICK_ATTACK },
  { 31, MV_WING_ATTACK },
  { 40, MV_AGILITY },
  // 18 pidgeot
  { 1, MV_GUST },
  { 1, MV_SAND_ATTACK },
  { 1, MV_QUICK_ATTACK },
  { 31, MV_WING_ATTACK },
  { 44, MV_AGILITY },
  // 19 rattata
  { 1, MV_TACKLE },
  { 1, MV_TAIL_WHIP },
  { 7, MV_QUICK_ATTACK },
  { 14, MV_HYPER_FANG },
  { 23, MV_FOCUS_ENERGY },
  { 34, MV_SUPER_FANG },
  // 20 raticate
  { 1, MV_TACKLE },
  { 1, MV_TAIL_WHIP },
  { 1, MV_QUICK_ATTACK },
  { 14, MV_HYPER_FANG },
  { 27, MV_FOCUS_ENERGY },
  { 41, MV_SUPER_FANG },
  // 21 spearow
  { 1, MV_PECK },
  { 1, MV_GROWL },
  { 9, MV_LEER },
  { 15, MV_FURY_ATTACK },
  { 29, MV_DRILL_PECK },
  { 36, MV_AGILITY },
  // 22 fearow
  { 1, MV_PECK },
  { 1, MV_GROWL },
  { 1, MV_LEER },
  { 15, MV_FURY_ATTACK },
  { 34, MV_DRILL_PECK },
  { 43, MV_AGILITY },
  // 23 ekans
  { 1, MV_WRAP },
  { 1, MV_LEER },
  { 10, MV_POISON_STING },
  { 17, MV_BITE },
  { 24, MV_GLARE },
  { 31, MV_SCREECH },
  { 38, MV_ACID },
  // 24 arbok
  { 1, MV_WRAP },
  { 1, MV_LEER },
  { 1, MV_POISON_STING },
  { 17, MV_BITE },
  { 27, MV_GLARE },
  { 36, MV_SCREECH },
  { 47, MV_ACID },
  // 25 pikachu
  { 1, MV_THUNDER_SHOCK },
  { 1, MV_GROWL },
  { 9, MV_THUNDER_WAVE },
  { 16, MV_QUICK_ATTACK },
  { 26, MV_SWIFT },
  { 33, MV_AGILITY },
  { 43, MV_THUNDER },
  // 26 raichu
  { 1, MV_THUNDER_SHOCK },
  { 1, MV_GROWL },
  { 1, MV_THUNDER_WAVE },
  // 27 sandshrew
  { 1, MV_SCRATCH },
  { 10, MV_SAND_ATTACK },
  { 17, MV_SLASH },
  { 24, MV_POISON_STING },
  { 31, MV_SWIFT },
  { 38, MV_FURY_SWIPES },
  // 28 sandslash
  { 1, MV_SCRATCH },
  { 1, MV_SAND_ATTACK },
  { 17, MV_SLASH },
  { 27, MV_POISON_STING },
  { 36, MV_SWIFT },
  { 47, MV_FURY_SWIPES },
  // 29 nidoranf
  { 1, MV_GROWL },
  { 1, MV_TACKLE },
  { 8, MV_SCRATCH },
  { 14, MV_POISON_STING },
  { 21, MV_TAIL_WHIP },
  { 29, MV_BITE },
  { 36, MV_FURY_SWIPES },
  { 43, MV_DOUBLE_KICK },
  // 30 nidorina
  { 1, MV_GROWL },
  { 1, MV_TACKLE },
  { 1, MV_SCRATCH },
  { 14, MV_POISON_STING },
  { 23, MV_TAIL_WHIP },
  { 32, MV_BITE },
  { 41, MV_FURY_SWIPES },
  { 50, MV_DOUBLE_KICK },
  // 31 nidoqueen
  { 1, MV_TACKLE },
  { 1, MV_SCRATCH },
  { 1, MV_TAIL_WHIP },
  { 1, MV_BODY_SLAM },
  { 14, MV_POISON_STING },
  // 32 nidoranm
  { 1, MV_LEER },
  { 1, MV_TACKLE },
  { 8, MV_HORN_ATTACK },
  { 14, MV_POISON_STING },
  { 21, MV_FOCUS_ENERGY },
  { 29, MV_FURY_ATTACK },
  { 36, MV_HORN_DRILL },
  { 43, MV_DOUBLE_KICK },
  // 33 nidorino
  { 1, MV_LEER },
  { 1, MV_TACKLE },
  { 1, MV_HORN_ATTACK },
  { 14, MV_POISON_STING },
  { 23, MV_FOCUS_ENERGY },
  { 32, MV_FURY_ATTACK },
  { 41, MV_HORN_DRILL },
  { 50, MV_DOUBLE_KICK },
  // 34 nidoking
  { 1, MV_TACKLE },
  { 1, MV_HORN_ATTACK },
  { 1, MV_POISON_STING },
  { 1, MV_THRASH },
  // 35 clefairy
  { 1, MV_POUND },
  { 1, MV_GROWL },
  { 13, MV_SING },
  { 18, MV_DOUBLE_SLAP },
  { 24, MV_MINIMIZE },
  { 39, MV_DEFENSE_CURL },
  { 48, MV_LIGHT_SCREEN },
  // 36 clefable
  { 1, MV_SING },
  { 1, MV_DOUBLE_SLAP },
  { 1, MV_MINIMIZE },
  // 37 vulpix
  { 1, MV_EMBER },
  { 1, MV_TAIL_WHIP },
  { 16, MV_QUICK_ATTACK },
  { 28, MV_CONFUSE_RAY },
  { 35, MV_FLAMETHROWER },
  { 42, MV_FIRE_SPIN },
  // 38 ninetales
  { 1, MV_EMBER },
  { 1, MV_TAIL_WHIP },
  { 1, MV_QUICK_ATTACK },
  // 39 jigglypuff
  { 1, MV_SING },
  { 9, MV_POUND },
  { 19, MV_DEFENSE_CURL },
  { 24, MV_DOUBLE_SLAP },
  { 29, MV_REST },
  { 34, MV_BODY_SLAM },
  { 39, MV_DOUBLE_EDGE },
  // 40 wigglytuff
  { 1, MV_SING },
  { 1, MV_DEFENSE_CURL },
  { 1, MV_DOUBLE_SLAP },
  // 41 zubat
  { 1, MV_LEECH_LIFE },
  { 10, MV_SUPERSONIC },
  { 15, MV_BITE },
  { 21, MV_CONFUSE_RAY },
  { 28, MV_WING_ATTACK },
  { 36, MV_HAZE },
  // 42 golbat
  { 1, MV_LEECH_LIFE },
  { 1, MV_SCREECH },
  { 1, MV_BITE },
  { 10, MV_SUPERSONIC },
  { 21, MV_CONFUSE_RAY },
  { 32, MV_WING_ATTACK },
  { 43, MV_HAZE },
  // 43 oddish
  { 1, MV_ABSORB },
  { 15, MV_POISON_POWDER },
  { 17, MV_STUN_SPORE },
  { 19, MV_SLEEP_POWDER },
  { 24, MV_ACID },
  { 33, MV_PETAL_DANCE },
  { 46, MV_SOLAR_BEAM },
  // 44 gloom
  { 1, MV_ABSORB },
  { 1, MV_POISON_POWDER },
  { 1, MV_STUN_SPORE },
  { 19, MV_SLEEP_POWDER },
  { 28, MV_ACID },
  { 38, MV_PETAL_DANCE },
  { 52, MV_SOLAR_BEAM },
  // 45 vileplume
  { 1, MV_STUN_SPORE },
  { 1, MV_SLEEP_POWDER },
  { 1, MV_ACID },
  { 1, MV_PETAL_DANCE },
  { 15, MV_POISON_POWDER },
  // 46 paras
  { 1, MV_SCRATCH },
  { 13, MV_STUN_SPORE },
  { 20, MV_LEECH_LIFE },
  { 27, MV_SPORE },
  { 34, MV_SLASH },
  { 41, MV_GROWTH },
  // 47 parasect
  { 1, MV_SCRATCH },
  { 1, MV_STUN_SPORE },
  { 1, MV_LEECH_LIFE },
  { 30, MV_SPORE },
  { 39, MV_SLASH },
  { 48, MV_GROWTH },
  // 48 venonat
  { 1, MV_TACKLE },
  { 24, MV_POISON_POWDER },
  { 27, MV_LEECH_LIFE },
  { 30, MV_STUN_SPORE },
  { 35, MV_PSYBEAM },
  { 38, MV_SLEEP_POWDER },
  { 43, MV_PSYCHIC },
  // 49 venomoth
  { 1, MV_TACKLE },
  { 1, MV_POISON_POWDER },
  { 1, MV_LEECH_LIFE },
  { 30, MV_STUN_SPORE },
  { 38, MV_PSYBEAM },
  { 43, MV_SLEEP_POWDER },
  { 50, MV_PSYCHIC },
  // 50 diglett
  { 1, MV_SCRATCH },
  { 15, MV_GROWL },
  { 19, MV_DIG },
  { 24, MV_SAND_ATTACK },
  { 31, MV_SLASH },
  { 40, MV_EARTHQUAKE },
  // 51 dugtrio
  { 1, MV_SCRATCH },
  { 1, MV_GROWL },
  { 1, MV_DIG },
  { 24, MV_SAND_ATTACK },
  { 35, MV_SLASH },
  { 47, MV_EARTHQUAKE },
  // 52 meowth
  { 1, MV_SCRATCH },
  { 1, MV_GROWL },
  { 12, MV_BITE },
  { 17, MV_PAY_DAY },
  { 24, MV_SCREECH },
  { 33, MV_FURY_SWIPES },
  { 44, MV_SLASH },
  // 53 persian
  { 1, MV_SCRATCH },
  { 1, MV_GROWL },
  { 1, MV_BITE },
  { 1, MV_SCREECH },
  { 17, MV_PAY_DAY },
  { 37, MV_FURY_SWIPES },
  { 51, MV_SLASH },
  // 54 psyduck
  { 1, MV_SCRATCH },
  { 28, MV_TAIL_WHIP },
  { 36, MV_CONFUSION },
  { 43, MV_FURY_SWIPES },
  { 52, MV_HYDRO_PUMP },
  // 55 golduck
  { 1, MV_SCRATCH },
  { 1, MV_TAIL_WHIP },
  { 39, MV_CONFUSION },
  { 48, MV_FURY_SWIPES },
  { 59, MV_HYDRO_PUMP },
  // 56 mankey
  { 1, MV_SCRATCH },
  { 1, MV_LEER },
  { 15, MV_KARATE_CHOP },
  { 21, MV_FURY_SWIPES },
  { 27, MV_FOCUS_ENERGY },
  { 33, MV_SEISMIC_TOSS },
  { 39, MV_THRASH },
  // 57 primeape
  { 1, MV_SCRATCH },
  { 1, MV_LEER },
  { 1, MV_KARATE_CHOP },
  { 1, MV_FURY_SWIPES },
  { 27, MV_FOCUS_ENERGY },
  { 37, MV_SEISMIC_TOSS },
  { 46, MV_THRASH },
  // 58 growlithe
  { 1, MV_BITE },
  { 18, MV_EMBER },
  { 23, MV_LEER },
  { 30, MV_TAKE_DOWN },
  { 39, MV_AGILITY },
  { 50, MV_FLAMETHROWER },
  // 59 arcanine
  { 1, MV_EMBER },
  { 1, MV_LEER },
  { 1, MV_TAKE_DOWN },
  // 60 poliwag
  { 1, MV_BUBBLE },
  { 16, MV_HYPNOSIS },
  { 19, MV_WATER_GUN },
  { 25, MV_DOUBLE_SLAP },
  { 31, MV_BODY_SLAM },
  { 38, MV_AMNESIA },
  { 45, MV_HYDRO_PUMP },
  // 61 poliwhirl
  { 1, MV_BUBBLE },
  { 1, MV_HYPNOSIS },
  { 1, MV_WATER_GUN },
  { 26, MV_DOUBLE_SLAP },
  { 33, MV_BODY_SLAM },
  { 41, MV_AMNESIA },
  { 49, MV_HYDRO_PUMP },
  // 62 poliwrath
  { 1, MV_HYPNOSIS },
  { 1, MV_WATER_GUN },
  { 1, MV_DOUBLE_SLAP },
  { 1, MV_BODY_SLAM },
  // 64 kadabra
  { 1, MV_CONFUSION },
  { 27, MV_PSYBEAM },
  { 31, MV_RECOVER },
  { 38, MV_PSYCHIC },
  { 42, MV_REFLECT },
  // 65 alakazam
  { 1, MV_CONFUSION },
  { 27, MV_PSYBEAM },
  { 31, MV_RECOVER },
  { 38, MV_PSYCHIC },
  { 42, MV_REFLECT },
  // 66 machop
  { 1, MV_KARATE_CHOP },
  { 20, MV_LOW_KICK },
  { 25, MV_LEER },
  { 32, MV_FOCUS_ENERGY },
  { 39, MV_SEISMIC_TOSS },
  { 46, MV_SUBMISSION },
  // 67 machoke
  { 1, MV_KARATE_CHOP },
  { 1, MV_LOW_KICK },
  { 1, MV_LEER },
  { 36, MV_FOCUS_ENERGY },
  { 44, MV_SEISMIC_TOSS },
  { 52, MV_SUBMISSION },
  // 68 machamp
  { 1, MV_KARATE_CHOP },
  { 1, MV_LOW_KICK },
  { 1, MV_LEER },
  { 36, MV_FOCUS_ENERGY },
  { 44, MV_SEISMIC_TOSS },
  { 52, MV_SUBMISSION },
  // 69 bellsprout
  { 1, MV_VINE_WHIP },
  { 1, MV_GROWTH },
  { 13, MV_WRAP },
  { 15, MV_POISON_POWDER },
  { 18, MV_SLEEP_POWDER },
  { 21, MV_STUN_SPORE },
  { 26, MV_ACID },
  { 33, MV_RAZOR_LEAF },
  { 42, MV_SLAM },
  // 70 weepinbell
  { 1, MV_VINE_WHIP },
  { 1, MV_GROWTH },
  { 1, MV_WRAP },
  { 15, MV_POISON_POWDER },
  { 18, MV_SLEEP_POWDER },
  { 23, MV_STUN_SPORE },
  { 29, MV_ACID },
  { 38, MV_RAZOR_LEAF },
  { 49, MV_SLAM },
  // 71 victreebel
  { 1, MV_SLEEP_POWDER },
  { 1, MV_STUN_SPORE },
  { 1, MV_ACID },
  { 1, MV_RAZOR_LEAF },
  { 13, MV_WRAP },
  { 15, MV_POISON_POWDER },
  // 72 tentacool
  { 1, MV_ACID },
  { 7, MV_SUPERSONIC },
  { 13, MV_WRAP },
  { 18, MV_POISON_STING },
  { 22, MV_WATER_GUN },
  { 27, MV_CONSTRICT },
  { 33, MV_BARRIER },
  { 40, MV_SCREECH },
  { 48, MV_HYDRO_PUMP },
  // 73 tentacruel
  { 1, MV_ACID },
  { 1, MV_SUPERSONIC },
  { 1, MV_WRAP },
  { 18, MV_POISON_STING },
  { 22, MV_WATER_GUN },
  { 27, MV_CONSTRICT },
  { 35, MV_BARRIER },
  { 43, MV_SCREECH },
  { 50, MV_HYDRO_PUMP },
  // 74 geodude
  { 1, MV_TACKLE },
  { 11, MV_DEFENSE_CURL },
  { 16, MV_ROCK_THROW },
  { 21, MV_SELF_DESTRUCT },
  { 26, MV_HARDEN },
  { 31, MV_EARTHQUAKE },
  { 36, MV_EXPLOSION },
  // 75 graveler
  { 1, MV_TACKLE },
  { 1, MV_DEFENSE_CURL },
  { 16, MV_ROCK_THROW },
  { 21, MV_SELF_DESTRUCT },
  { 29, MV_HARDEN },
  { 36, MV_EARTHQUAKE },
  { 43, MV_EXPLOSION },
  // 76 golem
  { 1, MV_TACKLE },
  { 1, MV_DEFENSE_CURL },
  { 16, MV_ROCK_THROW },
  { 21, MV_SELF_DESTRUCT },
  { 29, MV_HARDEN },
  { 36, MV_EARTHQUAKE },
  { 43, MV_EXPLOSION },
  // 77 ponyta
  { 1, MV_EMBER },
  { 30, MV_TAIL_WHIP },
  { 32, MV_STOMP },
  { 35, MV_GROWL },
  { 39, MV_FIRE_SPIN },
  { 43, MV_TAKE_DOWN },
  { 48, MV_AGILITY },
  // 78 rapidash
  { 1, MV_EMBER },
  { 1, MV_TAIL_WHIP },
  { 1, MV_STOMP },
  { 1, MV_GROWL },
  { 39, MV_FIRE_SPIN },
  { 47, MV_TAKE_DOWN },
  { 55, MV_AGILITY },
  // 79 slowpoke
  { 1, MV_CONFUSION },
  { 22, MV_HEADBUTT },
  { 27, MV_GROWL },
  { 33, MV_WATER_GUN },
  { 40, MV_AMNESIA },
  { 48, MV_PSYCHIC },
  // 80 slowbro
  { 1, MV_CONFUSION },
  { 1, MV_HEADBUTT },
  { 27, MV_GROWL },
  { 33, MV_WATER_GUN },
  { 37, MV_WITHDRAW },
  { 44, MV_AMNESIA },
  { 55, MV_PSYCHIC },
  // 81 magnemite
  { 1, MV_TACKLE },
  { 21, MV_SONIC_BOOM },
  { 25, MV_THUNDER_SHOCK },
  { 29, MV_SUPERSONIC },
  { 35, MV_THUNDER_WAVE },
  { 41, MV_SWIFT },
  { 47, MV_SCREECH },
  // 82 magneton
  { 1, MV_TACKLE },
  { 1, MV_SONIC_BOOM },
  { 1, MV_THUNDER_SHOCK },
  { 29, MV_SUPERSONIC },
  { 38, MV_THUNDER_WAVE },
  { 46, MV_SWIFT },
  { 54, MV_SCREECH },
  // 83 farfetchd
  { 1, MV_PECK },
  { 1, MV_SAND_ATTACK },
  { 7, MV_LEER },
  { 15, MV_FURY_ATTACK },
  { 23, MV_SWORDS_DANCE },
  { 31, MV_AGILITY },
  { 39, MV_SLASH },
  // 84 doduo
  { 1, MV_PECK },
  { 20, MV_GROWL },
  { 24, MV_FURY_ATTACK },
  { 30, MV_DRILL_PECK },
  { 36, MV_RAGE },
  { 40, MV_TRI_ATTACK },
  { 44, MV_AGILITY },
  // 85 dodrio
  { 1, MV_PECK },
  { 1, MV_GROWL },
  { 1, MV_FURY_ATTACK },
  { 30, MV_DRILL_PECK },
  { 39, MV_RAGE },
  { 45, MV_TRI_ATTACK },
  { 51, MV_AGILITY },
  // 86 seel
  { 1, MV_HEADBUTT },
  { 30, MV_GROWL },
  { 35, MV_AURORA_BEAM },
  { 40, MV_REST },
  { 45, MV_TAKE_DOWN },
  { 50, MV_ICE_BEAM },
  // 87 dewgong
  { 1, MV_HEADBUTT },
  { 1, MV_GROWL },
  { 1, MV_AURORA_BEAM },
  { 44, MV_REST },
  { 50, MV_TAKE_DOWN },
  { 56, MV_ICE_BEAM },
  // 88 grimer
  { 1, MV_POUND },
  { 30, MV_POISON_GAS },
  { 33, MV_MINIMIZE },
  { 37, MV_SLUDGE },
  { 42, MV_HARDEN },
  { 48, MV_SCREECH },
  { 55, MV_ACID_ARMOR },
  // 89 muk
  { 1, MV_POUND },
  { 1, MV_POISON_GAS },
  { 33, MV_MINIMIZE },
  { 37, MV_SLUDGE },
  { 45, MV_HARDEN },
  { 53, MV_SCREECH },
  { 60, MV_ACID_ARMOR },
  // 90 shellder
  { 1, MV_TACKLE },
  { 1, MV_WITHDRAW },
  { 18, MV_SUPERSONIC },
  { 23, MV_CLAMP },
  { 30, MV_AURORA_BEAM },
  { 39, MV_LEER },
  { 50, MV_ICE_BEAM },
  // 91 cloyster
  { 1, MV_WITHDRAW },
  { 1, MV_SUPERSONIC },
  { 1, MV_CLAMP },
  { 1, MV_AURORA_BEAM },
  { 50, MV_SPIKE_CANNON },
  // 92 gastly
  { 1, MV_LICK },
  { 1, MV_CONFUSE_RAY },
  { 1, MV_NIGHT_SHADE },
  { 27, MV_HYPNOSIS },
  { 35, MV_DREAM_EATER },
  // 93 haunter
  { 1, MV_LICK },
  { 1, MV_CONFUSE_RAY },
  { 1, MV_NIGHT_SHADE },
  { 29, MV_HYPNOSIS },
  { 38, MV_DREAM_EATER },
  // 94 gengar
  { 1, MV_LICK },
  { 1, MV_CONFUSE_RAY },
  { 1, MV_NIGHT_SHADE },
  { 29, MV_HYPNOSIS },
  { 38, MV_DREAM_EATER },
  // 95 onix
  { 1, MV_TACKLE },
  { 1, MV_SCREECH },
  { 15, MV_BIND },
  { 19, MV_ROCK_THROW },
  { 25, MV_RAGE },
  { 33, MV_SLAM },
  { 43, MV_HARDEN },
  // 96 drowzee
  { 1, MV_POUND },
  { 1, MV_HYPNOSIS },
  { 17, MV_CONFUSION },
  { 24, MV_HEADBUTT },
  { 29, MV_POISON_GAS },
  { 32, MV_PSYCHIC },
  { 37, MV_MEDITATE },
  // 97 hypno
  { 1, MV_POUND },
  { 1, MV_HYPNOSIS },
  { 1, MV_CONFUSION },
  { 24, MV_HEADBUTT },
  { 33, MV_POISON_GAS },
  { 37, MV_PSYCHIC },
  { 43, MV_MEDITATE },
  // 98 krabby
  { 1, MV_BUBBLE },
  { 1, MV_LEER },
  { 20, MV_VICE_GRIP },
  { 25, MV_GUILLOTINE },
  { 30, MV_STOMP },
  { 35, MV_CRABHAMMER },
  { 40, MV_HARDEN },
  // 99 kingler
  { 1, MV_BUBBLE },
  { 1, MV_LEER },
  { 1, MV_VICE_GRIP },
  { 25, MV_GUILLOTINE },
  { 34, MV_STOMP },
  { 42, MV_CRABHAMMER },
  { 49, MV_HARDEN },
  // 100 voltorb
  { 1, MV_TACKLE },
  { 1, MV_SCREECH },
  { 17, MV_SONIC_BOOM },
  { 22, MV_SELF_DESTRUCT },
  { 29, MV_LIGHT_SCREEN },
  { 36, MV_SWIFT },
  { 43, MV_EXPLOSION },
  // 101 electrode
  { 1, MV_TACKLE },
  { 1, MV_SCREECH },
  { 1, MV_SONIC_BOOM },
  { 22, MV_SELF_DESTRUCT },
  { 29, MV_LIGHT_SCREEN },
  { 40, MV_SWIFT },
  { 50, MV_EXPLOSION },
  // 102 exeggcute
  { 1, MV_BARRAGE },
  { 1, MV_HYPNOSIS },
  { 25, MV_REFLECT },
  { 28, MV_LEECH_SEED },
  { 32, MV_STUN_SPORE },
  { 37, MV_POISON_POWDER },
  { 42, MV_SOLAR_BEAM },
  { 48, MV_SLEEP_POWDER },
  // 103 exeggutor
  { 1, MV_BARRAGE },
  { 1, MV_HYPNOSIS },
  { 28, MV_STOMP },
  // 104 cubone
  { 1, MV_BONE_CLUB },
  { 1, MV_GROWL },
  { 25, MV_LEER },
  { 31, MV_FOCUS_ENERGY },
  { 38, MV_THRASH },
  { 43, MV_BONEMERANG },
  { 46, MV_RAGE },
  // 105 marowak
  { 1, MV_BONE_CLUB },
  { 1, MV_GROWL },
  { 1, MV_LEER },
  { 1, MV_FOCUS_ENERGY },
  { 41, MV_THRASH },
  { 48, MV_BONEMERANG },
  { 55, MV_RAGE },
  // 106 hitmonlee
  { 1, MV_DOUBLE_KICK },
  { 1, MV_MEDITATE },
  { 33, MV_ROLLING_KICK },
  { 38, MV_JUMP_KICK },
  { 43, MV_FOCUS_ENERGY },
  { 48, MV_HIGH_JUMP_KICK },
  { 53, MV_MEGA_KICK },
  // 107 hitmonchan
  { 1, MV_COMET_PUNCH },
  { 1, MV_AGILITY },
  { 33, MV_FIRE_PUNCH },
  { 38, MV_ICE_PUNCH },
  { 43, MV_THUNDER_PUNCH },
  { 48, MV_MEGA_PUNCH },
  // 108 lickitung
  { 1, MV_WRAP },
  { 1, MV_SUPERSONIC },
  { 7, MV_STOMP },
  { 23, MV_DEFENSE_CURL },
  { 31, MV_SLAM },
  { 39, MV_SCREECH },
  // 109 koffing
  { 1, MV_TACKLE },
  { 1, MV_SMOG },
  { 32, MV_SLUDGE },
  { 37, MV_SMOKESCREEN },
  { 40, MV_SELF_DESTRUCT },
  { 45, MV_HAZE },
  { 48, MV_EXPLOSION },
  // 110 weezing
  { 1, MV_TACKLE },
  { 1, MV_SMOG },
  { 1, MV_SLUDGE },
  { 39, MV_SMOKESCREEN },
  { 43, MV_SELF_DESTRUCT },
  { 49, MV_HAZE },
  { 53, MV_EXPLOSION },
  // 111 rhyhorn
  { 1, MV_HORN_ATTACK },
  { 30, MV_STOMP },
  { 35, MV_TAIL_WHIP },
  { 40, MV_FURY_ATTACK },
  { 45, MV_HORN_DRILL },
  { 50, MV_LEER },
  { 55, MV_TAKE_DOWN },
  // 112 rhydon
  { 1, MV_HORN_ATTACK },
  { 1, MV_STOMP },
  { 1, MV_TAIL_WHIP },
  { 1, MV_FURY_ATTACK },
  { 48, MV_HORN_DRILL },
  { 55, MV_LEER },
  { 64, MV_TAKE_DOWN },
  // 113 chansey
  { 1, MV_POUND },
  { 1, MV_DOUBLE_SLAP },
  { 24, MV_SING },
  { 30, MV_GROWL },
  { 38, MV_MINIMIZE },
  { 44, MV_DEFENSE_CURL },
  { 48, MV_LIGHT_SCREEN },
  { 54, MV_DOUBLE_EDGE },
  // 114 tangela
  { 1, MV_CONSTRICT },
  { 1, MV_BIND },
  { 29, MV_ABSORB },
  { 32, MV_POISON_POWDER },
  { 36, MV_STUN_SPORE },
  { 39, MV_SLEEP_POWDER },
  { 45, MV_SLAM },
  { 49, MV_GROWTH },
  // 115 kangaskhan
  { 1, MV_COMET_PUNCH },
  { 1, MV_RAGE },
  { 26, MV_BITE },
  { 31, MV_TAIL_WHIP },
  { 36, MV_MEGA_PUNCH },
  { 41, MV_LEER },
  { 46, MV_DIZZY_PUNCH },
  // 116 horsea
  { 1, MV_BUBBLE },
  { 19, MV_SMOKESCREEN },
  { 24, MV_LEER },
  { 30, MV_WATER_GUN },
  { 37, MV_AGILITY },
  { 45, MV_HYDRO_PUMP },
  // 117 seadra
  { 1, MV_BUBBLE },
  { 1, MV_SMOKESCREEN },
  { 24, MV_LEER },
  { 30, MV_WATER_GUN },
  { 41, MV_AGILITY },
  { 52, MV_HYDRO_PUMP },
  // 118 goldeen
  { 1, MV_PECK },
  { 1, MV_TAIL_WHIP },
  { 19, MV_SUPERSONIC },
  { 24, MV_HORN_ATTACK },
  { 30, MV_FURY_ATTACK },
  { 37, MV_WATERFALL },
  { 45, MV_HORN_DRILL },
  { 54, MV_AGILITY },
  // 119 seaking
  { 1, MV_PECK },
  { 1, MV_TAIL_WHIP },
  { 1, MV_SUPERSONIC },
  { 24, MV_HORN_ATTACK },
  { 30, MV_FURY_ATTACK },
  { 39, MV_WATERFALL },
  { 48, MV_HORN_DRILL },
  { 54, MV_AGILITY },
  // 120 staryu
  { 1, MV_TACKLE },
  { 17, MV_WATER_GUN },
  { 22, MV_HARDEN },
  { 27, MV_RECOVER },
  { 32, MV_SWIFT },
  { 37, MV_MINIMIZE },
  { 42, MV_LIGHT_SCREEN },
  { 47, MV_HYDRO_PUMP },
  // 121 starmie
  { 1, MV_TACKLE },
  { 1, MV_WATER_GUN },
  { 1, MV_HARDEN },
  // 122 mrmime
  { 1, MV_CONFUSION },
  { 1, MV_BARRIER },
  { 23, MV_LIGHT_SCREEN },
  { 31, MV_DOUBLE_SLAP },
  { 39, MV_MEDITATE },
  // 123 scyther
  { 1, MV_QUICK_ATTACK },
  { 17, MV_LEER },
  { 20, MV_FOCUS_ENERGY },
  { 24, MV_DOUBLE_TEAM },
  { 29, MV_SLASH },
  { 35, MV_SWORDS_DANCE },
  { 42, MV_AGILITY },
  // 124 jynx
  { 1, MV_POUND },
  { 1, MV_LOVELY_KISS },
  { 18, MV_LICK },
  { 23, MV_DOUBLE_SLAP },
  { 31, MV_ICE_PUNCH },
  { 39, MV_BODY_SLAM },
  { 47, MV_THRASH },
  { 58, MV_BLIZZARD },
  // 125 electabuzz
  { 1, MV_QUICK_ATTACK },
  { 1, MV_LEER },
  { 34, MV_THUNDER_SHOCK },
  { 37, MV_SCREECH },
  { 42, MV_THUNDER_PUNCH },
  { 49, MV_LIGHT_SCREEN },
  { 54, MV_THUNDER },
  // 126 magmar
  { 1, MV_EMBER },
  { 36, MV_LEER },
  { 39, MV_CONFUSE_RAY },
  { 43, MV_FIRE_PUNCH },
  { 48, MV_SMOKESCREEN },
  { 52, MV_SMOG },
  { 55, MV_FLAMETHROWER },
  // 127 pinsir
  { 1, MV_VICE_GRIP },
  { 25, MV_SEISMIC_TOSS },
  { 30, MV_GUILLOTINE },
  { 36, MV_FOCUS_ENERGY },
  { 43, MV_HARDEN },
  { 49, MV_SLASH },
  { 54, MV_SWORDS_DANCE },
  // 128 tauros
  { 1, MV_TACKLE },
  { 21, MV_STOMP },
  { 28, MV_TAIL_WHIP },
  { 35, MV_LEER },
  { 44, MV_RAGE },
  { 51, MV_TAKE_DOWN },
  // 129 magikarp
  { 1, MV_SPLASH },
  { 15, MV_TACKLE },
  // 130 gyarados
  { 1, MV_BITE },
  { 1, MV_DRAGON_RAGE },
  { 1, MV_LEER },
  { 1, MV_HYDRO_PUMP },
  { 52, MV_HYPER_BEAM },
  // 131 lapras
  { 1, MV_WATER_GUN },
  { 1, MV_GROWL },
  { 16, MV_SING },
  { 20, MV_MIST },
  { 25, MV_BODY_SLAM },
  { 31, MV_CONFUSE_RAY },
  { 38, MV_ICE_BEAM },
  { 46, MV_HYDRO_PUMP },
  // 132 ditto
  { 1, MV_TRANSFORM },
  // 133 eevee
  { 1, MV_TACKLE },
  { 1, MV_SAND_ATTACK },
  { 27, MV_QUICK_ATTACK },
  { 31, MV_TAIL_WHIP },
  { 37, MV_BITE },
  { 45, MV_TAKE_DOWN },
  // 134 vaporeon
  { 1, MV_TACKLE },
  { 1, MV_SAND_ATTACK },
  { 1, MV_QUICK_ATTACK },
  { 1, MV_WATER_GUN },
  { 37, MV_TAIL_WHIP },
  { 40, MV_BITE },
  { 42, MV_ACID_ARMOR },
  { 44, MV_HAZE },
  { 48, MV_MIST },
  { 54, MV_HYDRO_PUMP },
  // 135 jolteon
  { 1, MV_TACKLE },
  { 1, MV_SAND_ATTACK },
  { 1, MV_QUICK_ATTACK },
  { 1, MV_THUNDER_SHOCK },
  { 37, MV_TAIL_WHIP },
  { 40, MV_THUNDER_WAVE },
  { 42, MV_DOUBLE_KICK },
  { 44, MV_AGILITY },
  { 48, MV_PIN_MISSILE },
  { 54, MV_THUNDER },
  // 136 flareon
  { 1, MV_TACKLE },
  { 1, MV_SAND_ATTACK },
  { 1, MV_QUICK_ATTACK },
  { 1, MV_EMBER },
  { 37, MV_TAIL_WHIP },
  { 40, MV_BITE },
  { 42, MV_LEER },
  { 44, MV_FIRE_SPIN },
  { 48, MV_RAGE },
  { 54, MV_FLAMETHROWER },
  // 137 porygon
  { 1, MV_TACKLE },
  { 1, MV_SHARPEN },
  { 1, MV_CONVERSION },
  { 23, MV_PSYBEAM },
  { 28, MV_RECOVER },
  { 35, MV_AGILITY },
  { 42, MV_TRI_ATTACK },
  // 138 omanyte
  { 1, MV_WATER_GUN },
  { 1, MV_WITHDRAW },
  { 34, MV_HORN_ATTACK },
  { 39, MV_LEER },
  { 46, MV_SPIKE_CANNON },
  { 53, MV_HYDRO_PUMP },
  // 139 omastar
  { 1, MV_WATER_GUN },
  { 1, MV_WITHDRAW },
  { 1, MV_HORN_ATTACK },
  { 39, MV_LEER },
  { 44, MV_SPIKE_CANNON },
  { 49, MV_HYDRO_PUMP },
  // 140 kabuto
  { 1, MV_SCRATCH },
  { 1, MV_HARDEN },
  { 34, MV_ABSORB },
  { 39, MV_SLASH },
  { 44, MV_LEER },
  { 49, MV_HYDRO_PUMP },
  // 141 kabutops
  { 1, MV_SCRATCH },
  { 1, MV_HARDEN },
  { 1, MV_ABSORB },
  { 39, MV_SLASH },
  { 46, MV_LEER },
  { 53, MV_HYDRO_PUMP },
  // 142 aerodactyl
  { 1, MV_WING_ATTACK },
  { 1, MV_AGILITY },
  { 33, MV_SUPERSONIC },
  { 38, MV_BITE },
  { 45, MV_TAKE_DOWN },
  { 54, MV_HYPER_BEAM },
  // 143 snorlax
  { 1, MV_HEADBUTT },
  { 1, MV_AMNESIA },
  { 1, MV_REST },
  { 35, MV_BODY_SLAM },
  { 41, MV_HARDEN },
  { 48, MV_DOUBLE_EDGE },
  { 56, MV_HYPER_BEAM },
  // 144 articuno
  { 1, MV_PECK },
  { 1, MV_ICE_BEAM },
  { 51, MV_BLIZZARD },
  { 55, MV_AGILITY },
  { 60, MV_MIST },
  // 145 zapdos
  { 1, MV_THUNDER_SHOCK },
  { 1, MV_DRILL_PECK },
  { 51, MV_THUNDER },
  { 55, MV_AGILITY },
  { 60, MV_LIGHT_SCREEN },
  // 146 moltres
  { 1, MV_PECK },
  { 1, MV_FIRE_SPIN },
  { 51, MV_LEER },
  { 55, MV_AGILITY },
  { 60, MV_SKY_ATTACK },
  // 147 dratini
  { 1, MV_WRAP },
  { 1, MV_LEER },
  { 10, MV_THUNDER_WAVE },
  { 20, MV_AGILITY },
  { 30, MV_SLAM },
  { 40, MV_DRAGON_RAGE },
  { 50, MV_HYPER_BEAM },
  // 148 dragonair
  { 1, MV_WRAP },
  { 1, MV_LEER },
  { 1, MV_THUNDER_WAVE },
  { 20, MV_AGILITY },
  { 35, MV_SLAM },
  { 45, MV_DRAGON_RAGE },
  { 55, MV_HYPER_BEAM },
  // 149 dragonite
  { 1, MV_WRAP },
  { 1, MV_LEER },
  { 1, MV_THUNDER_WAVE },
  { 1, MV_AGILITY },
  { 35, MV_SLAM },
  { 45, MV_DRAGON_RAGE },
  { 60, MV_HYPER_BEAM },
  // 150 mewtwo
  { 1, MV_CONFUSION },
  { 1, MV_SWIFT },
  { 1, MV_PSYCHIC },
  { 63, MV_BARRIER },
  { 70, MV_RECOVER },
  { 75, MV_MIST },
  { 81, MV_AMNESIA },
  // 151 mew
  { 1, MV_POUND },
  { 10, MV_TRANSFORM },
  { 20, MV_MEGA_PUNCH },
  { 40, MV_PSYCHIC },
};

static const uint16_t LEARN_OFS[DEX_COUNT + 2] = {
  0, 0, 9, 18, 27, 35, 43, 51,
  59, 67, 75, 77, 78, 84, 86, 87,
  93, 98, 103, 108, 114, 120, 126, 132,
  139, 146, 153, 156, 162, 168, 176, 184,
  189, 197, 205, 209, 216, 219, 225, 228,
  235, 238, 244, 251, 258, 265, 270, 276,
  282, 289, 296, 302, 308, 315, 322, 327,
  332, 339, 346, 352, 355, 362, 369, 373,
  373, 378, 383, 389, 395, 401, 410, 419,
  425, 434, 443, 450, 457, 464, 471, 478,
  484, 491, 498, 505, 512, 519, 526, 532,
  538, 545, 552, 559, 564, 569, 574, 579,
  586, 593, 600, 607, 614, 621, 628, 636,
  639, 646, 653, 660, 666, 672, 679, 686,
  693, 700, 708, 716, 723, 729, 735, 743,
  751, 759, 762, 767, 774, 782, 789, 796,
  803, 809, 811, 816, 824, 825, 831, 841,
  851, 861, 868, 874, 880, 886, 892, 898,
  905, 910, 915, 920, 927, 934, 941, 948,
  952,
};

// Nomi delle mosse per lingua (tools/dex_move_names.py): latino in maiuscolo ASCII,
// JA e KO in UTF-8. nullptr = come l'inglese (sempre per il PT).
static const char *const MOVE_NAME_EN[MOVE_MAX_ID + 1] = {
  nullptr, "POUND", "KARATE CHOP", "DOUBLE SLAP",
  "COMET PUNCH", "MEGA PUNCH", "PAY DAY", "FIRE PUNCH",
  "ICE PUNCH", "THUNDERPUNCH", "SCRATCH", "VISE GRIP",
  "GUILLOTINE", "RAZOR WIND", "SWORDS DANCE", "CUT",
  "GUST", "WING ATTACK", nullptr, "FLY",
  "BIND", "SLAM", "VINE WHIP", "STOMP",
  "DOUBLE KICK", "MEGA KICK", "JUMP KICK", "ROLLING KICK",
  "SAND ATTACK", "HEADBUTT", "HORN ATTACK", "FURY ATTACK",
  "HORN DRILL", "TACKLE", "BODY SLAM", "WRAP",
  "TAKE DOWN", "THRASH", "DOUBLE-EDGE", "TAIL WHIP",
  "POISON STING", "TWINEEDLE", "PIN MISSILE", "LEER",
  "BITE", "GROWL", nullptr, "SING",
  "SUPERSONIC", "SONIC BOOM", nullptr, "ACID",
  "EMBER", "FLAMETHROWER", "MIST", "WATER GUN",
  "HYDRO PUMP", "SURF", "ICE BEAM", "BLIZZARD",
  "PSYBEAM", "BUBBLE BEAM", "AURORA BEAM", "HYPER BEAM",
  "PECK", "DRILL PECK", "SUBMISSION", "LOW KICK",
  nullptr, "SEISMIC TOSS", "STRENGTH", "ABSORB",
  "MEGA DRAIN", "LEECH SEED", "GROWTH", "RAZOR LEAF",
  "SOLAR BEAM", "POISONPOWDER", "STUN SPORE", "SLEEP POWDER",
  "PETAL DANCE", "STRING SHOT", "DRAGON RAGE", "FIRE SPIN",
  "THUNDERSHOCK", "THUNDERBOLT", "THUNDER WAVE", "THUNDER",
  "ROCK THROW", "EARTHQUAKE", "FISSURE", "DIG",
  "TOXIC", "CONFUSION", "PSYCHIC", "HYPNOSIS",
  "MEDITATE", "AGILITY", "QUICK ATTACK", "RAGE",
  nullptr, "NIGHT SHADE", nullptr, "SCREECH",
  "DOUBLE TEAM", "RECOVER", "HARDEN", "MINIMIZE",
  "SMOKESCREEN", "CONFUSE RAY", "WITHDRAW", "DEFENSE CURL",
  "BARRIER", "LIGHT SCREEN", "HAZE", "REFLECT",
  "FOCUS ENERGY", nullptr, nullptr, nullptr,
  "SELFDESTRUCT", "EGG BOMB", "LICK", "SMOG",
  "SLUDGE", "BONE CLUB", "FIRE BLAST", "WATERFALL",
  "CLAMP", "SWIFT", "SKULL BASH", "SPIKE CANNON",
  "CONSTRICT", "AMNESIA", "KINESIS", "SOFT-BOILED",
  "HI JUMP KICK", "GLARE", "DREAM EATER", "POISON GAS",
  "BARRAGE", "LEECH LIFE", "LOVELY KISS", "SKY ATTACK",
  "TRANSFORM", "BUBBLE", "DIZZY PUNCH", "SPORE",
  "FLASH", "PSYWAVE", "SPLASH", "ACID ARMOR",
  "CRABHAMMER", "EXPLOSION", "FURY SWIPES", "BONEMERANG",
  "REST", "ROCK SLIDE", "HYPER FANG", "SHARPEN",
  "CONVERSION", "TRI ATTACK", "SUPER FANG", "SLASH",
  nullptr, "STRUGGLE",
};

static const char *const MOVE_NAME_ES[MOVE_MAX_ID + 1] = {
  nullptr, "DESTRUCTOR", "GOLPE KARATE", "DOBLEBOFETON",
  "PUNO COMETA", "MEGAPUNO", "DIA DE PAGO", "PUNO FUEGO",
  "PUNO HIELO", "PUNO TRUENO", "ARANAZO", "AGARRE",
  "GUILLOTINA", "V. CORTANTE", "DANZA ESPADA", "CORTE",
  "TORNADO", "ATAQUE ALA", nullptr, "VUELO",
  "ATADURA", "ATIZAR", "LATIGO CEPA", "PISOTON",
  "DOBLE PATADA", "MEGAPATADA", "PATADA SALTO", "PATADA GIRO",
  "ATAQUE ARENA", "GOLPE CABEZA", "CORNADA", "ATAQUE FURIA",
  "PERFORADOR", "PLACAJE", "GOLPE CUERPO", "CONSTRICCION",
  "DERRIBO", "SANA", "DOBLE FILO", "AGITACOLA",
  "PICOT.VENENO", "DOBLE ATAQUE", "PIN MISIL", "MALICIOSO",
  "MORDISCO", "GRUNIDO", nullptr, "CANTO",
  "SUPERSONICO", "BOMBA SONICA", nullptr, "ACIDO",
  "ASCUAS", "LANZALLAMAS", "NEBLINA", "PISTOLA AGUA",
  "HIDROBOMBA", nullptr, "RAYO HIELO", "VENTISCA",
  "PSICORRAYO", "RAYO BURBUJA", "RAYO AURORA", "HIPERRAYO",
  "PICOTAZO", "PICO TALADRO", "SUMISION", "PATADA BAJA",
  nullptr, "SISMICO", "FUERZA", "ABSORBER",
  "MEGAAGOTAR", "DRENADORAS", "DESARROLLO", "HOJA AFILADA",
  "RAYO SOLAR", "POLVO VENENO", "PARALIZADOR", "SOMNIFERO",
  "DANZA PETALO", "DISP. DEMORA", "FURIA DRAGON", "GIRO FUEGO",
  "IMPACTRUENO", "RAYO", "ONDA TRUENO", "TRUENO",
  "LANZARROCAS", "TERREMOTO", "FISURA", "EXCAVAR",
  "TOXICO", nullptr, "PSIQUICO", "HIPNOSIS",
  "MEDITACION", "AGILIDAD", "ATAQ. RAPIDO", "FURIA",
  nullptr, "TINIEBLAS", nullptr, "CHIRRIDO",
  "DOBLE EQUIPO", "RECUPERACION", "FORTALEZA", "REDUCCION",
  "PANT. HUMO", "RAYO CONFUSO", "REFUGIO", "RIZO DEFENSA",
  "BARRERA", "PANT. LUZ", "NIEBLA", "REFLEJO",
  "FOCO ENERGIA", nullptr, nullptr, nullptr,
  "AUTODESTRUC.", "BOMBA HUEVO", "LENGUETAZO", "POLUCION",
  "RESIDUOS", "HUESO PALO", "LLAMARADA", "CASCADA",
  "TENAZA", "METEOROS", "CABEZAZO", "CLAVO CANON",
  "RESTRICCION", nullptr, "KINETICO", "OVOCURACION",
  "P.SALTO ALTA", "DESLUMBRAR", "COMESUENOS", "GAS VENENOSO",
  "BOMBARDEO", "CHUPAVIDAS", "BESO AMOROSO", "ATAQUE AEREO",
  "TRANSFORMAC.", "BURBUJA", "PUNO MAREO", "ESPORA",
  "DESTELLO", "PSICOONDA", "SALPICADURA", "ARMAD. ACIDA",
  "MARTILLAZO", nullptr, "GOLPES FURIA", "HUESOMERANG",
  "DESCANSO", "AVALANCHA", "HIPERCOLMIL.", "AFILAR",
  nullptr, "TRIATAQUE", "SUPERDIENTE", "CUCHILLADA",
  nullptr, "FORCEJEO",
};

static const char *const MOVE_NAME_FR[MOVE_MAX_ID + 1] = {
  nullptr, "ECRAS'FACE", "POING KARATE", "TORGNOLES",
  "POING COMETE", "ULTIMAPOING", "JACKPOT", "POING FEU",
  "POING GLACE", "POING ECLAIR", "GRIFFE", "FORCE POIGNE",
  nullptr, "COUPE-VENT", "DANSE LAMES", "COUPE",
  "TORNADE", "CRU-AILES", nullptr, "VOL",
  "ETREINTE", "SOUPLESSE", "FOUET LIANES", "ECRASEMENT",
  "DOUBLE PIED", "ULTIMAWASHI", "PIED SAUTE", "MAWASHI GERI",
  "JET DE SABLE", "COUP D'BOULE", "KOUD'KORNE", "FURIE",
  "EMPAL'KORNE", "CHARGE", "PLAQUAGE", "LIGOTAGE",
  "BELIER", "MANIA", "DAMOCLES", "MIMI-QUEUE",
  "DARD-VENIN", "DOUBLE DARD", "DARD-NUEE", "GROZ'YEUX",
  "MORSURE", "RUGISSEMENT", nullptr, "BERCEUSE",
  "ULTRASON", nullptr, nullptr, "ACIDE",
  "FLAMMECHE", "LANCE-FLAMME", "BRUME", "PISTOLET A O",
  "HYDROCANON", nullptr, "LASER GLACE", nullptr,
  "RAFALE PSY", "BULLES D'O", "ONDE BOREALE", "ULTRALASER",
  "PICPIC", "BEC VRILLE", "SACRIFICE", "BALAYAGE",
  nullptr, "FRAPPE ATLAS", "FORCE", "VOLE-VIE",
  "MEGA-SANGSUE", "VAMPIGRAINE", "CROISSANCE", "TRANCH'HERBE",
  "LANCE-SOLEIL", "POUDRE TOXIK", "PARA-SPORE", "POUDRE DODO",
  "DANSE FLEURS", "SECRETION", "DRACO-RAGE", "DANSEFLAMME",
  "ECLAIR", "TONNERRE", "CAGE ECLAIR", "FATAL-FOUDRE",
  "JET-PIERRES", "SEISME", "ABIME", "TUNNEL",
  "TOXIK", "CHOC MENTAL", "PSYKO", "HYPNOSE",
  "YOGA", "HATE", "VIVE-ATTAQUE", "FRENESIE",
  nullptr, "OMBRE NOCT.", nullptr, "GRINCEMENT",
  "REFLET", "SOIN", "ARMURE", "LILLIPUT",
  "BROUILLARD", "ONDE FOLIE", "REPLI", "BOUL'ARMURE",
  "BOUCLIER", "MUR LUMIERE", "BUEE NOIRE", "PROTECTION",
  "PUISSANCE", nullptr, nullptr, nullptr,
  "DESTRUCTION", "BOMBE OEUF", "LECHOUILLE", "PUREDPOIS",
  "DETRITUS", "MASSD'OS", "DEFLAGRATION", "CASCADE",
  "CLAQUOIR", "METEORES", "COUD'KRANE", "PICANON",
  "CONSTRICTION", "AMNESIE", "TELEKINESIE", "E-COQUE",
  "PIED VOLTIGE", "REGARD MEDU.", "DEVOREVE", "GAZ TOXIK",
  "PILONNAGE", "VAMPIRISME", "GROBISOU", "PIQUE",
  "MORPHING", "ECUME", "UPPERCUT", nullptr,
  nullptr, "VAGUE PSY", "TREMPETTE", "ACIDARMURE",
  "PINCE-MASSE", nullptr, "COMBO-GRIFFE", "OSMERANG",
  "REPOS", "EBOULEMENT", "CROC DE MORT", "AFFUTAGE",
  nullptr, "TRIPLATTAQUE", "CROC FATAL", "TRANCHE",
  nullptr, "LUTTE",
};

static const char *const MOVE_NAME_DE[MOVE_MAX_ID + 1] = {
  nullptr, "KLAPS", "KARATESCHLAG", "DUPLEXHIEB",
  "KOMETENHIEB", "MEGAHIEB", "ZAHLTAG", "FEUERSCHLAG",
  "EISHIEB", "DONNERSCHLAG", "KRATZER", "KLAMMER",
  nullptr, "KLINGENSTURM", "SCHWERTTANZ", "ZERSCHNEIDER",
  "WINDSTOSS", "FLUGELSCHLAG", nullptr, "FLIEGEN",
  "KLAMMERGRIFF", nullptr, "RANKENHIEB", "STAMPFER",
  "DOPPELKICK", "MEGAKICK", "SPRUNGKICK", "FEGEKICK",
  "SANDWIRBEL", "KOPFNUSS", "HORNATTACKE", "FURIENSCHLAG",
  "HORNBOHRER", nullptr, "BODYSLAM", "WICKEL",
  "BODYCHECK", "FUCHTLER", "RISIKOTACKLE", "RUTENSCHLAG",
  "GIFTSTACHEL", "DUONADEL", "NADELRAKETE", "SILBERBLICK",
  "BISS", "HEULER", nullptr, "GESANG",
  "SUPERSCHALL", "ULTRASCHALL", nullptr, "SAURE",
  "GLUT", "FLAMMENWURF", "WEISSNEBEL", "AQUAKNARRE",
  "HYDROPUMPE", "SURFER", "EISSTRAHL", nullptr,
  "PSYSTRAHL", "BLUBBSTRAHL", "AURORASTRAHL", "HYPERSTRAHL",
  "PIKSER", "BOHRSCHNABEL", "UBERROLLER", "FUSSKICK",
  nullptr, "GEOWURF", "STARKE", "ABSORBER",
  "MEGASAUGER", "EGELSAMEN", "WACHSTUM", "RASIERBLATT",
  "SOLARSTRAHL", "GIFTPUDER", "STACHELSPORE", "SCHLAFPUDER",
  "BLATTERTANZ", "FADENSCHUSS", "DRACHENWUT", "FEUERWIRBEL",
  "DONNERSCHOCK", "DONNERBLITZ", "DONNERWELLE", "DONNER",
  "STEINWURF", "ERDBEBEN", "GEOFISSUR", "SCHAUFLER",
  "TOXIN", "KONFUSION", "PSYCHOKINESE", "HYPNOSE",
  "MEDITATION", "AGILITAT", "RUCKZUCKHIEB", "RASEREI",
  nullptr, "NACHTNEBEL", nullptr, "KREIDESCHREI",
  "DOPPELTEAM", "GENESUNG", "HARTNER", "KOMPRIMATOR",
  "RAUCHWOLKE", "KONFUSSTRAHL", "PANZERSCHUTZ", "EINIGLER",
  "BARRIERE", "LICHTSCHILD", "DUNKELNEBEL", "REFLEKTOR",
  "ENERGIEFOKUS", nullptr, nullptr, nullptr,
  "FINALE", "EIERBOMBE", "SCHLECKER", nullptr,
  "SCHLAMMBAD", "KNOCHENKEULE", "FEUERSTURM", "KASKADE",
  "SCHNAPPER", "STERNSCHAUER", "SCHADELWUMME", "DORNKANONE",
  "UMKLAMMERUNG", "AMNESIE", "PSYKRAFT", "WEICHEI",
  "TURMKICK", "SCHLANGENBL.", "TRAUMFRESSER", "GIFTWOLKE",
  "STAKKATO", "BLUTSAUGER", "TODESKUSS", "HIMMELSFEGER",
  "WANDLER", "BLUBBER", "IRRSCHLAG", "PILZSPORE",
  "BLITZ", "PSYWELLE", "PLATSCHER", "SAUREPANZER",
  "KRABBHAMMER", nullptr, "KRATZFURIE", "KNOCHMERANG",
  "ERHOLUNG", "STEINHAGEL", "HYPERZAHN", "SCHARFER",
  "UMWANDLUNG", "TRIPLETTE", "SUPERZAHN", "SCHLITZER",
  nullptr, "VERZWEIFLER",
};

static const char *const MOVE_NAME_IT[MOVE_MAX_ID + 1] = {
  nullptr, "BOTTA", "COLPOKARATE", "DOPPIASBERLA",
  "COMETAPUGNO", "MEGAPUGNO", "GIORNOPAGA", "FUOCOPUGNO",
  "GELOPUGNO", "TUONOPUGNO", "GRAFFIO", "PRESA",
  "GHIGLIOTTINA", "VENTAGLIENTE", "DANZASPADA", "TAGLIO",
  "RAFFICA", "ATT. D'ALA", nullptr, "VOLO",
  "LEGATUTTO", "SCHIANTO", "FRUSTATA", "PESTONE",
  "DOPPIOCALCIO", "MEGACALCIO", "CALCIOSALTO", "CALCIORULLO",
  "TURBOSABBIA", "BOTTINTESTA", "INCORNATA", "FURIA",
  "PERFORCORNO", "AZIONE", "CORPOSCONTRO", "AVVOLGIBOTTA",
  "RIDUTTORE", "COLPO", "SDOPPIATORE", "COLPOCODA",
  "VELENOSPINA", "DOPPIO AGO", "MISSILSPILLO", "FULMISGUARDO",
  "MORSO", "RUGGITO", nullptr, "CANTO",
  "SUPERSUONO", "SONICBOOM", nullptr, "ACIDO",
  "BRACIERE", "LANCIAFIAMME", "NEBBIA", "PISTOLACQUA",
  "IDROPOMPA", nullptr, "GELORAGGIO", "BORA",
  "PSICORAGGIO", "BOLLARAGGIO", "RAGGIAURORA", "IPER RAGGIO",
  "BECCATA", "PERFORBECCO", "SOTTOMISS.", "COLPO BASSO",
  nullptr, "MOV. SISMICO", "FORZA", "ASSORBIMENTO",
  "MEGASSORBIM.", "PARASSISEME", "CRESCITA", "FOGLIELAMA",
  "SOLARRAGGIO", "VELENPOLVERE", "PARALIZZANTE", "SONNIFERO",
  "PETALODANZA", "MILLEBAVE", "IRA DI DRAGO", "TURBOFUOCO",
  "TUONOSHOCK", "FULMINE", "TUONONDA", "TUONO",
  "SASSATA", "TERREMOTO", "ABISSO", "FOSSA",
  "TOSSINA", "CONFUSIONE", "PSICHICO", "IPNOSI",
  "MEDITAZIONE", "AGILITA", "ATT. RAPIDO", "IRA",
  nullptr, "OMBRA NOTT.", nullptr, "STRIDIO",
  "DOPPIOTEAM", "RIPRESA", "RAFFORZATORE", "MINIMIZZATO",
  "MURO DI FUMO", "STORDIRAGGIO", "RITIRATA", "RICCIOLSCUDO",
  "BARRIERA", "SCHERMOLUCE", "NUBE", "RIFLESSO",
  "FOCALENERGIA", nullptr, nullptr, nullptr,
  "AUTODISTRUZ.", "UOVOBOMBA", "LECCATA", nullptr,
  "FANGO", "OSSOCLAVA", "FUOCOBOMBA", "CASCATA",
  "TENAGLIA", "COMETE", "CAPOCCIATA", "SPARALANCE",
  "LIMITAZIONE", nullptr, "CINESI", "COVAUOVA",
  "CALCINVOLO", "SGUARDO FER.", "MANGIASOGNI", "VELENOGAS",
  "ATT. PIOGGIA", "SANGUISUGA", "DEMONBACIO", "AEROATTACCO",
  "TRASFORMAZ.", "BOLLA", "STORDIPUGNO", "SPORA",
  nullptr, "PSICONDA", nullptr, "SCUDO ACIDO",
  "MARTELLATA", "ESPLOSIONE", "SFURIATE", "OSSOMERANG",
  "RIPOSO", "FRANA", "IPERZANNA", "AFFILATORE",
  "CONVERSIONE", "TRIPLETTA", "SUPERZANNA", "LACERAZIONE",
  nullptr, "SCONTRO",
};

static const char *const MOVE_NAME_JA[MOVE_MAX_ID + 1] = {
  nullptr, "はたく", "からてチョップ", "おうふくビンタ",
  "れんぞくパンチ", "メガトンパンチ", "ネコにこばん", "ほのおのパンチ",
  "れいとうパンチ", "かみなりパンチ", "ひっかく", "はさむ",
  "ハサミギロチン", "かまいたち", "つるぎのまい", "いあいぎり",
  "かぜおこし", "つばさでうつ", nullptr, "そらをとぶ",
  "しめつける", "たたきつける", "つるのムチ", "ふみつけ",
  "にどげり", "メガトンキック", "とびげり", "まわしげり",
  "すなかけ", "ずつき", "つのでつく", "みだれづき",
  "つのドリル", "たいあたり", "のしかかり", "まきつく",
  "とっしん", "あばれる", "すてみタックル", "しっぽをふる",
  "どくばり", "ダブルニードル", "ミサイルばり", "にらみつける",
  "かみつく", "なきごえ", nullptr, "うたう",
  "ちょうおんぱ", "ソニックブーム", nullptr, "ようかいえき",
  "ひのこ", "かえんほうしゃ", "しろいきり", "みずでっぽう",
  "ハイドロポンプ", "なみのり", "れいとうビーム", "ふぶき",
  "サイケこうせん", "バブルこうせん", "オーロラビーム", "はかいこうせん",
  "つつく", "ドリルくちばし", "じごくぐるま", "けたぐり",
  nullptr, "ちきゅうなげ", "かいりき", "すいとる",
  "メガドレイン", "やどりぎのタネ", "せいちょう", "はっぱカッター",
  "ソーラービーム", "どくのこな", "しびれごな", "ねむりごな",
  "はなびらのまい", "いとをはく", "りゅうのいかり", "ほのおのうず",
  "でんきショック", "10まんボルト", "でんじは", "かみなり",
  "いわおとし", "じしん", "じわれ", "あなをほる",
  "どくどく", "ねんりき", "サイコキネシス", "さいみんじゅつ",
  "ヨガのポーズ", "こうそくいどう", "でんこうせっか", "いかり",
  nullptr, "ナイトヘッド", nullptr, "いやなおと",
  "かげぶんしん", "じこさいせい", "かたくなる", "ちいさくなる",
  "えんまく", "あやしいひかり", "からにこもる", "まるくなる",
  "バリアー", "ひかりのかべ", "くろいきり", "リフレクター",
  "きあいだめ", nullptr, nullptr, nullptr,
  "じばく", "タマゴばくだん", "したでなめる", "スモッグ",
  "ヘドロこうげき", "ホネこんぼう", "だいもんじ", "たきのぼり",
  "からではさむ", "スピードスター", "ロケットずつき", "とげキャノン",
  "からみつく", "ドわすれ", "スプーンまげ", "タマゴうみ",
  "とびひざげり", "へびにらみ", "ゆめくい", "どくガス",
  "たまなげ", "きゅうけつ", "あくまのキッス", "ゴッドバード",
  "へんしん", "あわ", "ピヨピヨパンチ", "キノコのほうし",
  "フラッシュ", "サイコウェーブ", "はねる", "とける",
  "クラブハンマー", "だいばくはつ", "みだれひっかき", "ホネブーメラン",
  "ねむる", "いわなだれ", "ひっさつまえば", "かくばる",
  "テクスチャー", "トライアタック", "いかりのまえば", "きりさく",
  nullptr, "わるあがき",
};

static const char *const MOVE_NAME_KO[MOVE_MAX_ID + 1] = {
  nullptr, "막치기", "태권당수", "연속뺨치기",
  "연속펀치", "메가톤펀치", "고양이돈받기", "불꽃펀치",
  "냉동펀치", "번개펀치", "할퀴기", "찝기",
  "가위자르기", "칼바람", "칼춤", "풀베기",
  "바람일으키기", "날개치기", nullptr, "공중날기",
  "조이기", "힘껏치기", "덩굴채찍", "짓밟기",
  "두번차기", "메가톤킥", "점프킥", "돌려차기",
  "모래뿌리기", "박치기", "뿔찌르기", "마구찌르기",
  "뿔드릴", "몸통박치기", "누르기", "김밥말이",
  "돌진", "난동부리기", "이판사판태클", "꼬리흔들기",
  "독침", "더블니들", "바늘미사일", "째려보기",
  "물기", "울음소리", nullptr, "노래하기",
  "초음파", "소닉붐", nullptr, "용해액",
  "불꽃세례", "화염방사", "흰안개", "물대포",
  "하이드로펌프", "파도타기", "냉동빔", "눈보라",
  "환상빔", "거품광선", "오로라빔", "파괴광선",
  "쪼기", "회전부리", "지옥의바퀴", "안다리걸기",
  nullptr, "지구던지기", "괴력", "흡수",
  "메가드레인", "씨뿌리기", "성장", "잎날가르기",
  "솔라빔", "독가루", "저리가루", "수면가루",
  "꽃잎댄스", "실뿜기", "용의분노", "회오리불꽃",
  "전기쇼크", "10만볼트", "전기자석파", "번개",
  "돌떨구기", "지진", "땅가르기", "구멍파기",
  "맹독", "염동력", "사이코키네시스", "최면술",
  "요가포즈", "고속이동", "전광석화", "분노",
  nullptr, "나이트헤드", nullptr, "싫은소리",
  "그림자분신", "HP회복", "단단해지기", "작아지기",
  "연막", "이상한빛", "껍질에숨기", "웅크리기",
  "배리어", "빛의장막", "흑안개", "리플렉터",
  "기충전", nullptr, nullptr, nullptr,
  "자폭", "알폭탄", "핥기", "스모그",
  "오물공격", "뼈다귀치기", "불대문자", "폭포오르기",
  "껍질끼우기", "스피드스타", "로켓박치기", "가시대포",
  "휘감기", "망각술", "숟가락휘기", "알낳기",
  "무릎차기", "뱀눈초리", "꿈먹기", "독가스",
  "구슬던지기", "흡혈", "악마의키스", "불새",
  "변신", "거품", "잼잼펀치", "버섯포자",
  "플래시", "사이코웨이브", "튀어오르기", "녹기",
  "집게해머", "대폭발", "마구할퀴기", "뼈다귀부메랑",
  "잠자기", "스톤샤워", "필살앞니", "각지기",
  "텍스처", "트라이어택", "분노의앞니", "베어가르기",
  nullptr, "발버둥",
};

// Nome della mossa nella lingua attiva; "?" per un id senza mossa.
static inline const char *moveName(uint8_t id) {
  if (id > MOVE_MAX_ID || !MOVE_NAME_EN[id]) return "?";
  const char *n = (gLang == LANG_ES)   ? MOVE_NAME_ES[id]
                  : (gLang == LANG_FR) ? MOVE_NAME_FR[id]
                  : (gLang == LANG_DE) ? MOVE_NAME_DE[id]
                  : (gLang == LANG_IT) ? MOVE_NAME_IT[id]
                  : (gLang == LANG_JA) ? MOVE_NAME_JA[id]
                  : (gLang == LANG_KO) ? MOVE_NAME_KO[id]
                                       : nullptr;
  return n ? n : MOVE_NAME_EN[id];
}
