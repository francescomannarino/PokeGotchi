#pragma once
#include <Arduino.h>

// Lingue supportate. Il font 5x7 di Arduino_GFX è una tabella CP437 completa (256 glifi), quindi gli accenti, le dieresi e le ñ ci SONO: vanno come byte singolo in ottale (\240 a, \244 n~, \202 e, \204 a"...), mai in UTF-8, perché la UI centra con strlen(t)*6 e un carattere da due byte scentrerebbe le etichette.
// Non sono in CP437: le maiuscole accentate tranne E (\220), né a~/o~ del portoghese.
enum Lang : uint8_t { LANG_ES = 0, LANG_EN, LANG_FR, LANG_DE, LANG_IT, LANG_PT,
                     LANG_JA, LANG_KO, LANG_COUNT };
// lingue che richiedono un font U8g2 e testo UTF-8 invece della tabella CP437
#define LANG_IS_CJK(l) ((l) == LANG_JA || (l) == LANG_KO)
#define LANG_DEFAULT LANG_IT  // lingua predefinita: italiano

extern Lang gLang;  // lingua attiva (definita in i18n.cpp)
// lingua usata da loadLang() se in NVS non ce n'è una salvata: LANG_DEFAULT, salvo la PWA
// che prima di setup() mette quella del browser
extern Lang gLangDefault;

// ID delle stringhe. L'ordine deve coincidere con la tabella STRINGS di i18n.cpp. Sono più
// di 256, quindi uint16_t.
enum StrId : uint16_t {
  // stato dell'animale (statusMsg)
  S_EVOLVING, S_EATING, S_LIKES, S_HUNGRY, S_NEEDS_BATH,
  S_EXHAUSTED, S_SAD, S_CHUBBY, S_IS_SHINY, S_HAPPY,
  // cerimonie di addio
  S_FAREWELL, S_RUNAWAY, S_GOODBYE,
  // uovo
  S_EGG_HDR, S_EGG_LEGEND, S_EGG_RARE, S_EGG_TOUCH, S_EGG_MOVES, S_EGG_ALMOST,
  // formati condivisi
  S_POKEDEX_FMT,   // "POKEDEX %u/151"
  S_NAME_FMT,      // "%s%s Nv.%u"
  // dialogo di rilascio
  S_RELEASE_FMT, S_YES, S_NO,
  // minigioco e sacco
  S_HITS_FMT, S_STR_GAIN_FMT, S_NEW_RECORD, S_RECORD_FMT, S_HIT_FAST,
  S_SCORE_FMT, S_GREAT_JOY, S_PLUS_JOY,
  // orologio / impostazioni
  S_CLK_TIME, S_HOUR, S_MIN, S_CLOCK_CANCEL, S_LANG_LABEL,
  // festeggiamento
  S_MEDAL_BANNER, S_GREAT, S_STREAK_DAYS_FMT,
  // scheda: profilo
  S_STREAK_FMT, S_VIN, S_BERRY_UNK, S_BERRY_RED, S_BERRY_BLUE, S_BERRY_GREEN,
  S_INFO_FMT, S_RENAME_HINT,
  // scheda: combattimento
  S_BATTLE, S_STAT_ATK, S_STAT_DEF, S_STAT_SPE, S_STAT_WGT, S_TRAIN_STR,
  // scheda: medaglie
  S_MEDALS_FMT, S_BACK,
  // tastiera e galleria
  S_NAME, S_DETAIL_BACK,
  // barre
  S_BAR_FOOD, S_BAR_JOY, S_BAR_ENE, S_BAR_HYG,
  // punteggio in tempo reale del minigioco
  S_REC_FMT,
  // scheda: pagina dei progressi
  S_PROGRESS, S_LVL_FMT, S_NEXT_LVL_FMT, S_EVO_LABEL, S_FINAL_FORM,
  S_EVO_READY, S_EVO_BLOCKED, S_EVO_IN_FMT, S_MISTAKES_FMT,
  // interruttore del suono (impostazioni)
  S_VOL_FMT, S_MUTE,  // pillola del suono nell'orologio: volume, oppure muto a 0
  S_EVO_TAP,        // testo del pulsante di evoluzione
  S_FAREWELL_BTN,   // testo del pulsante di addio (contiene il nome: "%s ...")
  S_RUNAWAY_BTN,    // testo del pulsante di fuga per abbandono (finale triste)
  // dialoghi di decisione (evolvere/mantenere, dire addio/restare insieme)
  S_EVO_Q, S_EVO_KEEP, S_FAR_Q, S_FAR_GO, S_FAR_STAY,
  S_CHOOSE_STARTER,  // titolo della scelta dell'iniziale (prima volta)
  S_NO_SPRITES, S_LOAD_SPRITES,  // avviso quando manca lo sprite nella SD
  // salvataggi (impostazioni, ripristino, importazione)
  S_SAVES, S_SAVE_NOW, S_SAVE_NO_SD, S_SAVE_DONE, S_SAVE_FAIL, S_SAVE_EMPTY,
  S_SAVE_FOUND, S_SAVE_RESTORE_Q, S_SAVE_IMPORT_Q, S_SAVE_RESTORING,
  S_SAVE_ERR_FILE, S_SAVE_ERR_FW, S_SAVE_ERR_COPY,
  // motivo della copia (SAVE_MANUALE ... SAVE_PRIMA_IMPORT, stesso ordine)
  S_MOT_MANUAL, S_MOT_DAILY, S_MOT_UPDATE, S_MOT_TRADE, S_MOT_IMPORT,
  // schermata di battaglia (ui_battle.cpp)
  S_BTL_FIGHT, S_BTL_RUN, S_BTL_TAP, S_BTL_HP, S_BTL_APPEARS, S_BTL_GO, S_BTL_USES,
  S_BTL_STRUGGLE, S_BTL_RECHARGE, S_BTL_MISS, S_BTL_FAIL, S_BTL_IMMUNE, S_BTL_NOTHING,
  S_BTL_CRIT, S_BTL_SUPER, S_BTL_WEAK, S_BTL_HITS, S_BTL_RECOIL, S_BTL_HEAL, S_BTL_FAINT,
  S_BTL_WIN, S_BTL_LOSE, S_BTL_DRAW, S_BTL_FLED, S_BTL_CHARGE, S_BTL_STAT_UP,
  S_BTL_STAT_DOWN, S_BTL_ST_ATK, S_BTL_ST_DEF, S_BTL_ST_SPE, S_BTL_ST_SPC, S_BTL_ST_ACC,
  S_BTL_ST_EVA, S_BTL_PARA, S_BTL_BURN, S_BTL_POISON, S_BTL_TOXIC, S_BTL_SLEEP,
  S_BTL_FROZEN, S_BTL_CONFUSED, S_BTL_ASLEEP, S_BTL_WAKE, S_BTL_THAW, S_BTL_CANT_MOVE,
  S_BTL_HURTSELF, S_BTL_UNCONFUSED, S_BTL_FLINCH, S_BTL_PSN_DMG, S_BTL_BRN_DMG,
  S_BTL_SEED_DMG, S_BTL_SEEDED, S_BTL_RAGE, S_BTL_PROTECTED, S_BTL_MIST, S_BTL_FOCUS,
  S_BTL_BARRIER, S_BTL_RESET, S_BTL_CONVERT, S_BTL_TRANSFORM, S_BTL_AB_PAR, S_BTL_AB_BRN,
  S_BTL_AB_PSN, S_BTL_AB_SLP, S_BTL_AB_FRZ,
  // battaglie contro la CPU: Speciale nella scheda, premio della vittoria
  S_STAT_SPC, S_BTL_REWARD, S_BTL_NO_REWARD,
  // nomi dei tipi, nell'ordine dei T_* di dex.h (typeName() in ui_common.cpp)
  S_TYPE_NORMAL, S_TYPE_FIGHTING, S_TYPE_FLYING, S_TYPE_POISON, S_TYPE_GROUND, S_TYPE_ROCK,
  S_TYPE_BUG, S_TYPE_GHOST, S_TYPE_FIRE, S_TYPE_WATER, S_TYPE_GRASS, S_TYPE_ELECTRIC,
  S_TYPE_PSYCHIC, S_TYPE_ICE, S_TYPE_DRAGON,
  // collezione personale (E11, ui_collezione.cpp): titolo, caselle, pulsanti, domande,
  // motivi per cui RIPRENDI non si può (ColMotivo), allenatore originale
  S_COL_TITLE, S_COL_FMT, S_COL_EMPTY, S_COL_TAKE, S_COL_FREE, S_COL_TAKE_Q,
  S_COL_FREE_Q, S_COL_KEEP_Q, S_COL_FULL, S_COL_GIVE_UP, S_COL_EGG_LOST, S_COL_SWAP,
  S_COL_WHY_BUSY, S_COL_WHY_FINAL, S_COL_WHY_FIT, S_COL_WHY_TODAY, S_COL_OT, S_COL_ME,
  // pannello Audio dell'orologio: titolo e interruttore della musica
  S_AUDIO, S_MUSIC_ON, S_MUSIC_OFF,
  // orologio: scheda DATA ed etichette dei suoi pulsanti (anno, mese, giorno)
  S_CLK_DATE, S_YEAR, S_MONTH, S_DAY,
  // collegamento (E5, ui_link.cpp): pulsante della scheda, titolo, tastiera del nome,
  // ricerca, codice, attesa, pronto, versione diversa (con TU/ALTRO), fine, pulsanti
  S_LINK_BTN, S_LINK_TITLE, S_LINK_TRAINER, S_LINK_NO_RADIO, S_LINK_SEARCH, S_LINK_NEAR,
  S_LINK_CODE_Q, S_LINK_WAIT, S_LINK_READY, S_LINK_UPDATE, S_LINK_YOU, S_LINK_OTHER,
  S_LINK_NOBODY, S_LINK_LOST, S_LINK_LEFT, S_LINK_RETRY, S_LINK_TRADE,
  // battaglia in collegamento (E6, ui_link.cpp e ui_battle.cpp): menu POKEMON, regola
  // Coppa, invito e proposta, attesa, scelta della squadra, pulsanti, narrazione dei cambi
  // e della resa, cambio forzato, fine, rivincita, vittorie, rifiuto ed errore
  S_BTL_PKMN, S_LKB_RULE, S_LKB_FREE, S_LKB_L50, S_LKB_WANTS, S_LKB_PROPOSES,
  S_LKB_ACCEPT_Q, S_LKB_WAITING, S_LKB_TEAM, S_LKB_NONE, S_LKB_OK, S_LKB_CANCEL,
  S_LKB_SENDS, S_LKB_BACK, S_LKB_WITHDRAW, S_LKB_GAVE_UP, S_LKB_NEXT, S_LKB_GIVE_UP_Q,
  S_LKB_REMATCH, S_LKB_EXIT, S_LKB_WANTS_REMATCH, S_LKB_WINS, S_LKB_REFUSED, S_LKB_ERROR,
  // scambio in collegamento (E7, ui_link.cpp): scelta del pet da offrire e divieti, la
  // trattativa con la conferma, l'esito, l'evoluzione, lo scambio sospeso con l'annullamento
  // manuale (avviso e domanda) e il contatore della scheda
  S_LKS_OFFER_Q, S_LKS_NONE, S_LKS_EGG, S_LKS_TAP_CHANGE, S_LKS_CONFIRM_Q, S_LKS_THEY_OK,
  S_LKS_BUSY, S_LKS_GOT, S_LKS_EVOLVES, S_LKS_CANCELLED, S_LKS_COPY_FAIL, S_LKS_ERROR,
  S_LKS_SUSPENDED, S_LKS_RECONNECT, S_LKS_FORCE, S_LKS_FORCE_WARN, S_LKS_FORCE_Q, S_LKS_TRADES,
  // squadra della lotta in collegamento: perché il pet attivo è grigio (Pet::lottaPerche)
  S_LKB_WHY_SLEEP, S_LKB_WHY_TIRED, S_LKB_WHY_BUSY,
  // conferma della fuga per abbandono (ui_main.cpp, OVL_RUNAWAY): domanda, fuga, cura
  S_RUN_Q, S_RUN_GO, S_RUN_STAY,
  // giochi a inclinazione (E13, ui_grotta.cpp): DIFESA allenata, zero del sensore, joystick
  S_DEF_GAIN_FMT, S_TILT_HOLD, S_TILT_JOY,
  // interruttore MOVIMENTO nel pannello Audio: scossone e colpetti
  S_MOTION_ON, S_MOTION_OFF,
  // aggiornamento via WiFi (ui_aggiorna.cpp): titolo, versione e rete, scelta e connessione,
  // controllo ed esito, pulsanti, scaricamento, batteria, errori, PWA, tastiera della password
  S_AG_TITOLO, S_AG_VERSIONE, S_AG_RETE, S_AG_SCEGLI, S_AG_CERCO_RETI, S_AG_NESSUNA_RETE,
  S_AG_CONNETTO, S_AG_CONN_FALLITA, S_AG_CONTROLLO, S_AG_AGGIORNATO, S_AG_NUOVA,
  S_AG_INSTALLA, S_AG_RIPROVA, S_AG_ALTRA_RETE, S_AG_DIMENTICA, S_AG_SCARICO,
  S_AG_NON_SPEGNERE, S_AG_FATTO, S_AG_BATT1, S_AG_BATT2, S_AG_ERR_RETE, S_AG_ERR_SERVER,
  S_AG_ERR_FILE, S_AG_NO_WIFI, S_AG_PASSWORD,
  STR_COUNT
};

const char *T(StrId id);       // testo nella lingua attiva
const char *medalName(int i);  // banner della medaglia (MED_COUNT)
const char *medalLabel(int i); // etichetta breve della medaglia
const char *medalDesc(int i);  // descrizione lunga della medaglia

void loadLang();             // legge la lingua da NVS (chiamare nel setup)
void setLang(Lang l);        // cambia e persiste la lingua
