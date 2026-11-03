// Coerenza delle traduzioni (i18n.cpp).
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../i18n.h"
#include "../pet.h"  // MED_COUNT
#include <ctype.h>
#include <string.h>
#include <vector>

static const char *LANG_NAME[LANG_COUNT] = { "ES", "EN", "FR", "DE", "IT", "PT", "JA", "KO" };

// estrae la firma degli specificatori printf di una stringa: "%lud" -> "lu"
static std::vector<std::string> specs(const char *s) {
  std::vector<std::string> out;
  const char *p = s;
  while (*p) {
    if (*p != '%') { p++; continue; }
    p++;
    if (*p == '%') { p++; continue; }   // %% è un letterale
    std::string sig;
    while (*p && strchr("-+ #0", *p)) p++;             // flag
    while (*p && isdigit((unsigned char)*p)) p++;      // larghezza
    if (*p == '.') { p++; while (*p && isdigit((unsigned char)*p)) p++; }
    while (*p && strchr("lhzjt", *p)) { sig += *p; p++; }  // modificatore
    if (!*p) { out.push_back(sig + "<incompleto>"); break; }
    sig += *p++;
    out.push_back(sig);
  }
  return out;
}

static std::string joinSpecs(const std::vector<std::string> &v) {
  std::string s;
  for (size_t i = 0; i < v.size(); i++) { if (i) s += ","; s += "%" + v[i]; }
  return s.empty() ? "(sin formato)" : s;
}

// firma attesa di ogni stringa con formato, ricavata dalle chiamate dello sketch
struct ExpectedFmt { StrId id; const char *sig; };
static const ExpectedFmt EXPECTED[] = {
  { S_POKEDEX_FMT,     "%u" },
  { S_NAME_FMT,        "%s,%s,%u" },
  { S_RELEASE_FMT,     "%s" },
  { S_HITS_FMT,        "%u" },
  { S_STR_GAIN_FMT,    "%u" },
  { S_DEF_GAIN_FMT,    "%u" },
  { S_RECORD_FMT,      "%u" },
  { S_SCORE_FMT,       "%u" },
  { S_STREAK_DAYS_FMT, "%u" },
  { S_STREAK_FMT,      "%u,%u" },
  { S_INFO_FMT,        "%s,%lu" },
  { S_MEDALS_FMT,      "%d,%d" },
  { S_REC_FMT,         "%u" },
  { S_LVL_FMT,         "%u" },
  { S_NEXT_LVL_FMT,    "%u,%u" },
  { S_EVO_IN_FMT,      "%u" },
  { S_MISTAKES_FMT,    "%u" },
  { S_FAREWELL_BTN,    "%s" },
  { S_RUNAWAY_BTN,     "%s" },
  // battaglia (ui_battle.cpp)
  { S_BTL_USES,        "%s,%s" },
  { S_BTL_STAT_UP,     "%s,%s" },
  { S_BTL_STAT_DOWN,   "%s,%s" },
  { S_BTL_TRANSFORM,   "%s,%s" },
  { S_BTL_HITS,        "%u" },
  { S_BTL_GO,          "%s" },
  { S_BTL_APPEARS,     "%s" },
  { S_BTL_STRUGGLE,    "%s" },
  { S_BTL_RECHARGE,    "%s" },
  { S_BTL_RECOIL,      "%s" },
  { S_BTL_HEAL,        "%s" },
  { S_BTL_FAINT,       "%s" },
  { S_BTL_CHARGE,      "%s" },
  { S_BTL_PARA,        "%s" },
  { S_BTL_BURN,        "%s" },
  { S_BTL_POISON,      "%s" },
  { S_BTL_TOXIC,       "%s" },
  { S_BTL_SLEEP,       "%s" },
  { S_BTL_FROZEN,      "%s" },
  { S_BTL_CONFUSED,    "%s" },
  { S_BTL_ASLEEP,      "%s" },
  { S_BTL_WAKE,        "%s" },
  { S_BTL_THAW,        "%s" },
  { S_BTL_CANT_MOVE,   "%s" },
  { S_BTL_UNCONFUSED,  "%s" },
  { S_BTL_FLINCH,      "%s" },
  { S_BTL_PSN_DMG,     "%s" },
  { S_BTL_BRN_DMG,     "%s" },
  { S_BTL_SEED_DMG,    "%s" },
  { S_BTL_SEEDED,      "%s" },
  { S_BTL_RAGE,        "%s" },
  { S_BTL_PROTECTED,   "%s" },
  { S_BTL_MIST,        "%s" },
  { S_BTL_FOCUS,       "%s" },
  { S_BTL_BARRIER,     "%s" },
  { S_BTL_CONVERT,     "%s" },
  { S_BTL_REWARD,      "%s" },
  // collezione (ui_collezione.cpp)
  { S_COL_FMT,         "%u,%u" },
  { S_COL_TAKE_Q,      "%s" },
  { S_COL_FREE_Q,      "%s" },
  { S_COL_KEEP_Q,      "%s" },
  // impostazioni (ui_clock.cpp): la pillola del volume
  { S_VOL_FMT,         "%u" },
  // battaglia in collegamento (ui_link.cpp, ui_battle.cpp)
  { S_LKB_WANTS,       "%s" },
  { S_LKB_PROPOSES,    "%s" },
  { S_LKB_WAITING,     "%s" },
  { S_LKB_SENDS,       "%s,%s" },
  { S_LKB_BACK,        "%s" },
  { S_LKB_WITHDRAW,    "%s,%s" },
  { S_LKB_GAVE_UP,     "%s" },
  { S_LKB_WANTS_REMATCH, "%s" },
  { S_LKB_WINS,        "%u" },
  // scambio in collegamento (ui_link.cpp) e il contatore nella scheda (ui_card.cpp)
  { S_LKS_CONFIRM_Q,   "%s,%s" },
  { S_LKS_THEY_OK,     "%s" },
  { S_LKS_GOT,         "%s" },
  { S_LKS_EVOLVES,     "%s" },
  { S_LKS_SUSPENDED,   "%s" },
  { S_LKS_TRADES,      "%u" },
  { S_AG_VERSIONE,     "%s" },
  { S_AG_RETE,         "%s" },
  { S_AG_NUOVA,        "%s" },
  { S_AG_SCARICO,      "%u" },
};
static const int NUM_EXPECTED = (int)(sizeof(EXPECTED) / sizeof(EXPECTED[0]));

static const char *expectedSig(int id) {
  for (int i = 0; i < NUM_EXPECTED; i++)
    if ((int)EXPECTED[i].id == id) return EXPECTED[i].sig;
  return "(sin formato)";
}

TEST(i18n, nessuna_lingua_ha_buchi) {
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    gLang = (Lang)lang;
    for (int id = 0; id < STR_COUNT; id++) {
      const char *s = T((StrId)id);
      char where[64];
      snprintf(where, sizeof(where), "[%s][id %d]", LANG_NAME[lang], id);
      CHECK_MSG(s != NULL, where);
      if (s) CHECK_MSG(s[0] != 0, std::string("cadena vacia ") + where);
    }
  }
  gLang = LANG_DEFAULT;
}

// Il font del firmware (glcdfont.h di Arduino_GFX) è una tabella CP437 da 256 glifi,
// quindi ammette gli accenti, ma ciascuno deve occupare UN byte: la UI centra con
// strlen(t)*6, e un carattere UTF-8 da due byte scentrerebbe l'etichetta. Questo test
// non vieta i byte alti, vieta l'UTF-8.
TEST(i18n, gli_accenti_stanno_in_un_solo_byte_non_in_utf8) {
  // sequenze di avvio UTF-8 multibyte: 0xC2-0xDF, 0xE0-0xEF, 0xF0-0xF4
  auto arranqueUtf8 = [](unsigned char c) {
    return (c >= 0xC2 && c <= 0xDF) || (c >= 0xE0 && c <= 0xEF) || (c >= 0xF0 && c <= 0xF4);
  };
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    if (LANG_IS_CJK(lang)) continue;  // i CJK vanno in UTF-8 di proposito, con font U8g2
    gLang = (Lang)lang;
    for (int id = 0; id < STR_COUNT; id++) {
      const char *s = T((StrId)id);
      if (!s) continue;
      for (const char *c = s; *c; c++) {
        unsigned char u = (unsigned char)*c;
        if (u < 128) continue;                 // ASCII: va sempre bene
        if (!arranqueUtf8(u)) continue;        // byte alto singolo: è CP437, va bene
        // un avvio UTF-8 è sospetto solo se è seguito da un byte di continuazione
        // (0x80-0xBF): allora sono due glifi dove dovrebbe andarne uno
        unsigned char sig = (unsigned char)*(c + 1);
        char msg[200];
        snprintf(msg, sizeof(msg),
                 "parece UTF-8 (0x%02X 0x%02X) en [%s][id %d]: los acentos van en octal, "
                 "un byte por caracter, o se descuadra el centrado", u, sig, LANG_NAME[lang], id);
        CHECK_MSG(!(sig >= 0x80 && sig <= 0xBF), msg);
      }
    }
  }
  gLang = LANG_DEFAULT;
}

// un %u in più o in meno in una traduzione è una lettura di spazzatura dallo stack
TEST(i18n, i_formati_coincidono_in_tutte_le_lingue) {
  for (int id = 0; id < STR_COUNT; id++) {
    gLang = LANG_EN;
    std::vector<std::string> ref = specs(T((StrId)id));
    for (int lang = 0; lang < LANG_COUNT; lang++) {
      gLang = (Lang)lang;
      std::vector<std::string> got = specs(T((StrId)id));
      char msg[256];
      snprintf(msg, sizeof(msg), "[%s][id %d] \"%s\": %s en vez de %s (EN)", LANG_NAME[lang], id,
               T((StrId)id), joinSpecs(got).c_str(), joinSpecs(ref).c_str());
      CHECK_MSG(got == ref, msg);
    }
  }
  gLang = LANG_DEFAULT;
}

// ...e inoltre devono coincidere con ciò che passa lo sketch a ogni chiamata
TEST(i18n, i_formati_tornano_con_le_chiamate_dello_sketch) {
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    gLang = (Lang)lang;
    for (int id = 0; id < STR_COUNT; id++) {
      std::string got = joinSpecs(specs(T((StrId)id)));
      const char *want = expectedSig(id);
      char msg[256];
      snprintf(msg, sizeof(msg), "[%s][id %d] \"%s\": %s, se esperaba %s", LANG_NAME[lang], id,
               T((StrId)id), got.c_str(), want);
      CHECK_MSG(got == want, msg);
    }
  }
  gLang = LANG_DEFAULT;
}

TEST(i18n, le_medaglie_ci_sono_in_tutte_le_lingue) {
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    gLang = (Lang)lang;
    // i limiti di lunghezza sono in BYTE, che equivalgono a caratteri solo con il font
    // CP437. In CJK ogni kana occupa 3 byte e il limite reale è di larghezza a schermo,
    // che qui non si può misurare: questo si verifica sulla scheda.
    const bool cjk = LANG_IS_CJK(lang);
    for (int m = 0; m < MED_COUNT; m++) {
      const char *n = medalName(m), *l = medalLabel(m), *d = medalDesc(m);
      char where[64];
      snprintf(where, sizeof(where), "[%s][medalla %d]", LANG_NAME[lang], m);
      CHECK_MSG(n && n[0], std::string("medalName vacio ") + where);
      CHECK_MSG(l && l[0], std::string("medalLabel vacio ") + where);
      CHECK_MSG(d && d[0], std::string("medalDesc vacio ") + where);
      // l'etichetta breve va in una casella della griglia delle medaglie
      if (!cjk && l) CHECK_MSG(strlen(l) <= 6, std::string("etiqueta demasiado larga ") + where + ": " + l);
      if (!cjk && n) CHECK_MSG(strlen(n) <= 12, std::string("nombre demasiado largo ") + where + ": " + n);
      if (!cjk && d) CHECK_MSG(strlen(d) <= 16, std::string("descripcion demasiado larga ") + where + ": " + d);
    }
  }
  gLang = LANG_DEFAULT;
}

TEST(i18n, le_medaglie_non_hanno_accenti) {
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    if (LANG_IS_CJK(lang)) continue;  // i CJK vanno in UTF-8 con font U8g2
    gLang = (Lang)lang;
    for (int m = 0; m < MED_COUNT; m++) {
      const char *tab[3] = { medalName(m), medalLabel(m), medalDesc(m) };
      for (int t = 0; t < 3; t++)
        for (const char *c = tab[t]; c && *c; c++)
          CHECK_MSG((unsigned char)*c < 128, std::string("no ASCII: ") + tab[t]);
    }
  }
  gLang = LANG_DEFAULT;
}

TEST(i18n, la_lingua_si_salva_e_si_recupera) {
  mockNvsReset();
  setLang(LANG_DE);
  CHECK_EQ((int)gLang, (int)LANG_DE);
  gLang = LANG_ES;
  loadLang();
  CHECK_EQ((int)gLang, (int)LANG_DE);
  gLang = LANG_DEFAULT;
}

TEST(i18n, senza_impostazione_salvata_parte_nella_lingua_predefinita) {
  mockNvsReset();
  gLang = LANG_IT;
  loadLang();
  CHECK_EQ((int)gLang, (int)LANG_DEFAULT);
  gLang = LANG_DEFAULT;
}

TEST(i18n, una_lingua_non_valida_nella_nvs_non_rompe_nulla) {
  mockNvsReset();
  Preferences p;
  p.begin("pokegotchi", false);
  p.putUChar("lang", 200);  // valore corrotto
  p.end();
  gLang = LANG_ES;
  loadLang();
  CHECK_EQ((int)gLang, (int)LANG_DEFAULT);
  gLang = LANG_DEFAULT;
}

TEST(i18n, setLang_rifiuta_valori_fuori_intervallo) {
  mockNvsReset();
  setLang(LANG_FR);
  setLang((Lang)LANG_COUNT);
  CHECK_EQ((int)gLang, (int)LANG_FR);
  setLang((Lang)250);
  CHECK_EQ((int)gLang, (int)LANG_FR);
  gLang = LANG_DEFAULT;
}
