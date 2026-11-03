// Schermata di battaglia (SCR_BATTLE): il pet contro un Pokémon selvatico, 1 contro 1, con
// il motore di battle.h, oppure la battaglia in collegamento a squadre fino a 3.
// Il layout segue quello di DylanPDao, già pensato per questo display rotondo:
// l'avversario in alto a destra, il nostro in basso a sinistra e specchiato, nella fascia
// in basso il riquadro di testo e i menu.
//
// Il motore gioca un turno intero in una volta e accoda gli eventi nella coda; la
// schermata li racconta uno alla volta. Ciò che si vede (PS, stato, specie, KO, chi è in
// campo) segue la narrazione, non il motore, e si riallinea a fine turno. Una riga di
// testo aspetta un tocco prima di lasciare il posto alla successiva; il danno non ha testo
// e si vede con l'animazione e con la barra che scende.
//
// Sprite PMD in PSRAM: contro la CPU il nostro è `pmd`, già caricato per la schermata
// principale, e l'avversario si carica in `foePmd`; nel collegamento ogni lato ha il suo
// (`ourPmd`, `foePmd`), ricaricato quando entra un altro membro della squadra. Si liberano
// all'uscita.
//
// Dal pulsante LOTTA della scheda (startCpuBattle) la battaglia segue le regole del
// tamagotchi di Pet::battleResult: a fine battaglia costi e premi, e la riga del premio.
// Quella del comando seriale BATTLE è solo una prova e non tocca il pet.
//
// Collegamento (startLinkBattle da ui_link.cpp): la battaglia è quella di
// LinkBattaglia (linkLb()), dove il lato 0 è l'host; qui il nostro lato si disegna sempre
// in basso e latoM() traduce. La narrazione viene da linkLb().coda(), uguale sulle due
// schede. Menu LOTTA / POKEMON / FUGA: la fuga è una resa, con conferma; POKEMON elenca la
// squadra con PS e stati, e dopo un KO la stessa lista sceglie il sostituto. Dopo la nostra
// scelta, finché l'altro non ha scelto, il riquadro dice «In attesa di …». Nessun timer,
// né per leggere né per scegliere: il collegamento resta vivo con i PING di Link (linkLoop
// gira anche a schermo spento). Alla fine RIVINCITA (si torna a SCR_LINK per la squadra) o
// ESCI; le ricompense le dà ui_link.cpp alla conferma di END.
//
// Musica: `wildbattle` contro la CPU e `trainerbattle` nel collegamento,
// dall'ingresso; `defeatedwildmon` / `defeatedtrainer` quando si narra la vittoria;
// sconfitta, pareggio, fuga e uscita la fermano.

#include "ui_common.h"
#include "battle.h"
#include "linkbattaglia.h"
#include "musica_dati.h"

// eventi della UI, accodati nella coda accanto a quelli del motore (dopo l'ultimo EV_*)
enum : uint8_t { EVU_APPARE = 200, EVU_VAI, EVU_FUGA, EVU_PREMIO, EVU_NO_PREMIO, EVU_MANDA,
                 EVU_RESA };

// BF_NARRA: riquadro di testo; BF_MENU: LOTTA / POKEMON / FUGA; BF_MOSSE: griglia 2x2;
// BF_SQUADRA: la squadra (anche per il cambio forzato); BF_RESA: conferma della resa nel
// collegamento; BF_FINE: fine della battaglia in collegamento (RIVINCITA / ESCI)
enum : uint8_t { BF_NARRA, BF_MENU, BF_MOSSE, BF_SQUADRA, BF_RESA, BF_FINE };

#define BTL_ENTRA_MS 450    // il Pokémon scivola in campo
#define BTL_ATTACCO_MS 520  // slancio verso l'avversario
#define BTL_COLPO_MS 480    // scossa del colpo subito
#define BTL_KO_MS 900       // affonda sotto la pedana

// geometria (schermo rotondo: ogni angolo resta entro il raggio di 233)
#define FOE_X 336    // centro e suolo dello sprite avversario
#define FOE_GY 190
#define OUR_X 140    // centro e suolo del nostro
#define OUR_GY 262
#define BAND_Y 280   // la fascia piatta dei menu e del testo
#define BOX_X 60     // riquadro di testo
#define BOX_Y 288
#define BOX_W 346
#define BOX_H 96
#define MENU_Y 290   // LOTTA / POKEMON
#define MENU_W 158
#define MENU_H 62
#define RUN_Y 360    // FUGA, sotto e al centro
#define RUN_H 48
#define CELL_W 162   // griglia delle mosse
#define CELL_H 52
#define CELL_X(i) (67 + ((i) % 2) * (CELL_W + 6))
#define CELL_Y(i) (284 + ((i) / 2) * (CELL_H + 6))
#define BACK_X 163   // pulsante per tornare al menu
#define BACK_Y 404
#define BACK_W 140
#define BACK_H 44
#define SQ_X 63      // righe della squadra (BF_SQUADRA, a schermo intero)
#define SQ_W 340
#define SQ_Y0 112
#define SQ_H 70
#define SQ_GAP 8
#define DUE_W 150    // due pulsanti affiancati nella fascia (resa, fine)
#define DUE_Y 340
#define DUE_H 52

// nomi delle statistiche per gli stadi, indici STAT_*
static const StrId STAT_NOME[6] = { S_BTL_ST_ATK, S_BTL_ST_DEF, S_BTL_ST_SPE,
                                     S_BTL_ST_SPC, S_BTL_ST_ACC, S_BTL_ST_EVA };
// abbreviazioni e colori degli stati maggiori, indici ST_PARALISI..ST_CONGELAMENTO - 1
static const StrId AB[] = { S_BTL_AB_PAR, S_BTL_AB_BRN, S_BTL_AB_PSN, S_BTL_AB_PSN,
                            S_BTL_AB_SLP, S_BTL_AB_FRZ };
static const uint16_t AB_COL[] = { C565(0xd8, 0xb0, 0x20), C565(0xe0, 0x60, 0x30),
                                   C565(0xa0, 0x40, 0xa0), C565(0x80, 0x20, 0x80),
                                   C565(0x80, 0x80, 0x90), C565(0x50, 0xa8, 0xd8) };

static Battle *gBt = nullptr;  // la battaglia contro la CPU, in PSRAM (inPsram)
static Battle &bt() { return inPsram(gBt); }
static bool gLinkMode = false;  // battaglia in collegamento: quella di linkLb()
static uint8_t gMio = 0;   // il nostro lato nel motore (collegamento: 0 host, 1 ospite)
static PmdMon foePmd;      // sprite dell'avversario
static PmdMon ourPmd;      // sprite nostro nel collegamento (contro la CPU è `pmd`)
static int16_t spriteDex[2] = { -1, -1 };  // specie caricata in ourPmd / foePmd
static bool spriteShiny[2] = { false, false };
static int16_t foeDex = 0;
static uint8_t fase = BF_NARRA;
static bool fuggito = false;
static bool conRegole = false;   // battaglia dalla scheda: a fine battaglia battleResult()
static bool esitoFatto = false;  // battleResult() già chiamato per questa battaglia
static bool cambioForzato = false;  // BF_SQUADRA dopo un KO: niente indietro
static bool resaNarrata = false;    // la riga della resa è già in coda
static char riga[112];     // riga della narrazione in corso
static bool rigaDaLeggere = false;  // la riga aspetta un tocco prima della successiva
static uint32_t occupatoFino = 0;   // animazione in corso: la narrazione aspetta

// ciò che lo schermo mostra di ciascun lato (0 = noi, 1 = l'altro), al passo della narrazione
static uint16_t psMostrati[2], psNarrati[2];  // la barra scende verso psNarrati
static uint8_t statoNarrato[2];
static int16_t dexNarrato[2];  // cambia con la Trasformazione
static bool koNarrato[2], inCampo[2];
static uint8_t membroNarrato[2];  // chi è in campo, secondo la narrazione
static uint16_t psMembro[2][BATTLE_SQUADRA];  // PS e stato di ogni membro, narrati
static uint8_t statoMembro[2][BATTLE_SQUADRA];
static uint32_t entraFino[2], attaccoFino[2], colpoFino[2], koFino[2];

// ---------- la battaglia mostrata ----------

static const Battle &B() { return gLinkMode ? linkLb().battaglia() : bt(); }
static BattleCoda &Q() { return gLinkMode ? linkLb().coda() : bt().coda; }
// lato della schermata (0 = noi) -> lato del motore
static uint8_t latoM(uint8_t u) { return (u ^ gMio) & 1; }
// il membro in campo secondo la narrazione: livello e PS massimi
static const Combatant &membro(uint8_t u) {
  return battleMembro(B(), latoM(u), membroNarrato[u]);
}

static bool finita() { return fuggito || bt().vincitore != BATTLE_IN_CORSO; }

// nome del membro i del lato u: il soprannome o la specie. Dalla radio solo ASCII
// stampabile, come in ui_link.cpp
static const char *nomeMembro(uint8_t u, uint8_t i) {
  static char buf[2][RIT_NOME];
  if (!gLinkMode) {
    if (u) return dexName(foeDex);
    return pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  }
  const PetRitratto &r = linkLb().ritratto(latoM(u), i);
  if (!r.nick[0]) return dexName(r.specie);
  char *o = buf[u & 1];
  size_t k = 0;
  for (; k + 1 < RIT_NOME && r.nick[k]; k++)
    o[k] = (r.nick[k] >= 0x20 && r.nick[k] < 0x7F) ? r.nick[k] : '?';
  o[k] = 0;
  return o;
}

static const char *nomeLato(uint8_t u) { return nomeMembro(u, membroNarrato[u]); }

// l'allenatore del lato u (collegamento)
static const char *allenatore(uint8_t u) {
  static char io[LINK_NOME_LEN + 1];
  if (u) return linkNomeAltro();
  pet.trainerName(io, sizeof(io));
  return io;
}

// nel collegamento lo sprite del membro in campo: si carica dalla SD quando entra
static void caricaSprite(uint8_t u) {
  if (!gLinkMode) return;
  const PetRitratto &r = linkLb().ritratto(latoM(u), membroNarrato[u]);
  if (spriteDex[u] == r.specie && spriteShiny[u] == r.shiny) return;
  (u ? foePmd : ourPmd).load((uint8_t)r.specie, r.shiny);
  spriteDex[u] = r.specie;
  spriteShiny[u] = r.shiny;
}

// Avvia una battaglia del pet contro un selvatico `dex` di livello `lvl` (1..100). false
// se il pet non può combattere: uovo, scelta dell'iniziale o cerimonia in corso.
static bool avvia(int16_t dex, uint8_t lvl, bool regole) {
  if (dex < 1 || dex > DEX_COUNT || pet.awaitingStarter() || pet.ceremony || gScreen == SCR_BATTLE)
    return false;
  Combatant noi, loro;
  if (!combattenteDaPet(noi, pet)) return false;
  if (lvl < 1) lvl = 1;
  if (lvl > 100) lvl = 100;
  // DV casuali come i selvatici di Gen 1 (quello dei PS dai bit bassi), nessun bonus
  const DexEntry &d = DEX_TBL[dex];
  uint8_t dvA = random(16), dvD = random(16), dvV = random(16), dvS = random(16);
  uint8_t dvP = ((dvA & 1) << 3) | ((dvD & 1) << 2) | ((dvV & 1) << 1) | (dvS & 1);
  battleCombattente(loro, dex, lvl, battleStatGen1(d.bHp, dvP, 0, lvl, true),
                    battleStatGen1(d.bAtk, dvA, 0, lvl, false),
                    battleStatGen1(d.bDef, dvD, 0, lvl, false),
                    battleStatGen1(d.bSpe, dvV, 0, lvl, false),
                    battleStatGen1(d.bSpc, dvS, 0, lvl, false), nullptr);
  battleInizia(bt(), noi, loro, (uint32_t)random(1, 0x7FFFFFFF));
  foeDex = dex;
  conRegole = regole;
  gLinkMode = false;
  goScreen(SCR_BATTLE);
  return true;
}

// comando seriale BATTLE: una prova, senza costi né premi
bool startBattle(int16_t dex, uint8_t lvl) { return avvia(dex, lvl, false); }

// console PP: i PP del nostro Pokémon in campo nella battaglia contro la CPU (mossa 1-4,
// 0 = tutte), per provare celle esaurite e Scontro. false fuori da una battaglia con la CPU
bool battleDbgPp(uint8_t mossa, uint8_t n) {
  if (gScreen != SCR_BATTLE || gLinkMode || mossa > BATTLE_MOSSE) return false;
  Combatant &c = bt().lato[0];
  for (uint8_t i = 0; i < BATTLE_MOSSE; i++)
    if (c.mosse[i] && (!mossa || i == mossa - 1)) c.pp[i] = n < c.ppMax[i] ? n : c.ppMax[i];
  return true;
}

// pulsante LOTTA della scheda: un selvatico del bioma, di livello vicino al pet, con le
// regole del tamagotchi. false se il pet ora non può lottare (Pet::canBattle).
bool startCpuBattle() {
  if (!pet.canBattle()) return false;
  uint8_t lvl = 1;
  int16_t dex = pet.pickWild(lvl);
  return avvia(dex, lvl, true);
}

// da SCR_LINK, quando LinkBattaglia è passata a SCELTA (INIZIO): la battaglia è già lì
bool startLinkBattle() {
  if (gScreen == SCR_BATTLE) return false;
  gLinkMode = true;
  goScreen(SCR_BATTLE);
  return true;
}

// onEnter: sprite, stato mostrato e le prime due righe
void battleEnter() {
  fase = BF_NARRA;
  fuggito = false;
  esitoFatto = false;
  cambioForzato = false;
  resaNarrata = false;
  riga[0] = 0;
  rigaDaLeggere = false;
  occupatoFino = 0;
  gMio = gLinkMode ? linkLb().mioLato() : 0;
  const Battle &b = B();
  for (uint8_t u = 0; u < 2; u++) {
    uint8_t k = latoM(u);
    membroNarrato[u] = b.inCampo[k];
    for (uint8_t i = 0; i < BATTLE_SQUADRA; i++) {
      psMembro[u][i] = battleMembro(b, k, i).psMax;
      statoMembro[u][i] = ST_NESSUNO;
    }
    psMostrati[u] = psNarrati[u] = b.lato[k].psMax;
    statoNarrato[u] = ST_NESSUNO;
    dexNarrato[u] = b.lato[k].dex;
    koNarrato[u] = false;
    entraFino[u] = attaccoFino[u] = colpoFino[u] = koFino[u] = 0;
  }
  inCampo[0] = false;  // entra con "Vai!"
  spriteDex[0] = spriteDex[1] = -1;
  if (gLinkMode) {
    caricaSprite(0);
    caricaSprite(1);
    inCampo[1] = false;  // l'altro allenatore lo manda in campo
    Q().metti({ EVU_MANDA, 1, 0, 0 });
    Q().metti({ EVU_VAI, 0, 0, 0 });
    halMusica(MUS_TRAINERBATTLE);  // contro un allenatore
    return;
  }
  foePmd.load(foeDex, false);
  spriteDex[1] = foeDex;
  inCampo[1] = true;
  entraFino[1] = millis() + BTL_ENTRA_MS;
  occupatoFino = entraFino[1];
  bt().coda.metti({ EVU_APPARE, 1, 0, 0 });
  bt().coda.metti({ EVU_VAI, 0, 0, 0 });
  halMusica(MUS_WILDBATTLE);  // contro la CPU, cioè un selvatico
}

// costi e premi della battaglia dalla scheda, una volta sola; la riga del premio va in
// coda dopo quella della vittoria
static void applicaEsito(uint8_t esito) {
  esitoFatto = true;
  if (!conRegole) return;
  bool premio = pet.battleResult(esito);
  if (esito == BTL_VINTA) bt().coda.metti({ premio ? EVU_PREMIO : EVU_NO_PREMIO, 0, 0, 0 });
}

// onExit: libera gli sprite della battaglia (il nostro `pmd` resta alla principale).
// Uscire a battaglia non finita (una cerimonia che parte) conta come una fuga; nel
// collegamento lo si dice all'altro.
void battleExit() {
  if (gLinkMode) {
    LinkBattaglia &lb = linkLb();
    if (lb.fase() == LBF_SCELTA || lb.fase() == LBF_CAMBIO) lb.esci();
    ourPmd.unload();
  } else if (!esitoFatto) {
    applicaEsito(BTL_FUGA);
  }
  foePmd.unload();
  spriteDex[0] = spriteDex[1] = -1;
  gLinkMode = false;
  halMusica(MUS_NESSUNA);
}

// ---------- narrazione ----------

static void colpisci(uint8_t l, uint16_t v) {
  psNarrati[l] = v >= psNarrati[l] ? 0 : psNarrati[l] - v;
  colpoFino[l] = millis() + BTL_COLPO_MS;
  if (timeLeft(occupatoFino) < BTL_COLPO_MS) occupatoFino = colpoFino[l];
  halSfx(SFX_HIT);
}

static void cura(uint8_t l, uint16_t v) {
  uint32_t ps = (uint32_t)psNarrati[l] + v;
  uint16_t max = membro(l).psMax;
  psNarrati[l] = ps > max ? max : (uint16_t)ps;
}

// il danno non ha testo: resta la riga di prima mentre la barra scende
static bool conTesto(uint8_t tipo) { return tipo != EV_DANNO; }

// applica un evento allo schermo e ne scrive la riga
static void narra(const BattleEvento &e) {
  uint32_t now = millis();
  uint8_t l = e.tipo >= EVU_APPARE ? (e.lato & 1) : latoM(e.lato);  // gli EVU_ sono già della schermata
  // PS e stato di chi è in campo, per quando rientra dopo un cambio
  for (uint8_t u = 0; u < 2; u++) {
    psMembro[u][membroNarrato[u] % BATTLE_SQUADRA] = psNarrati[u];
    statoMembro[u][membroNarrato[u] % BATTLE_SQUADRA] = statoNarrato[u];
  }
  const char *n = nomeLato(l);
  switch (e.tipo) {
    case EVU_APPARE: snprintf(riga, sizeof(riga), T(S_BTL_APPEARS), n); break;
    case EVU_VAI:
      inCampo[0] = true;
      entraFino[0] = occupatoFino = now + BTL_ENTRA_MS;
      halSfx(SFX_TAP);
      snprintf(riga, sizeof(riga), T(S_BTL_GO), n);
      break;
    case EVU_MANDA:
      inCampo[1] = true;
      entraFino[1] = occupatoFino = now + BTL_ENTRA_MS;
      halSfx(SFX_TAP);
      snprintf(riga, sizeof(riga), T(S_LKB_SENDS), allenatore(1), n);
      break;
    case EVU_FUGA:
      halMusica(MUS_NESSUNA);  // come in Gen 1: la fuga ferma la musica
      snprintf(riga, sizeof(riga), T(S_BTL_FLED));
      break;
    case EVU_RESA: snprintf(riga, sizeof(riga), T(S_LKB_GAVE_UP), allenatore(l)); break;
    case EVU_PREMIO: snprintf(riga, sizeof(riga), T(S_BTL_REWARD), n); break;
    case EVU_NO_PREMIO: snprintf(riga, sizeof(riga), T(S_BTL_NO_REWARD)); break;
    case EV_MOSSA:
      attaccoFino[l] = occupatoFino = now + BTL_ATTACCO_MS;
      halSfx(SFX_ATTACK);
      snprintf(riga, sizeof(riga), T(S_BTL_USES), n, moveName(e.mossa));
      break;
    case EV_SCONTRO: snprintf(riga, sizeof(riga), T(S_BTL_STRUGGLE), n); break;
    case EV_RICARICA: snprintf(riga, sizeof(riga), T(S_BTL_RECHARGE), n); break;
    case EV_MANCATA: snprintf(riga, sizeof(riga), T(S_BTL_MISS)); break;
    case EV_FALLITA: snprintf(riga, sizeof(riga), T(S_BTL_FAIL)); break;
    case EV_NON_HA_EFFETTO: snprintf(riga, sizeof(riga), T(S_BTL_IMMUNE)); break;
    case EV_NESSUN_EFFETTO: snprintf(riga, sizeof(riga), T(S_BTL_NOTHING)); break;
    case EV_BRUTTO_COLPO: snprintf(riga, sizeof(riga), T(S_BTL_CRIT)); break;
    case EV_EFFICACIA:
      if (e.valore > 4) snprintf(riga, sizeof(riga), T(S_BTL_SUPER));
      else snprintf(riga, sizeof(riga), T(S_BTL_WEAK));
      break;
    case EV_DANNO: colpisci(l, e.valore); return;
    case EV_COLPI: snprintf(riga, sizeof(riga), T(S_BTL_HITS), e.valore); break;
    case EV_RINCULO:
    case EV_SCHIANTO:
      colpisci(l, e.valore);
      snprintf(riga, sizeof(riga), T(S_BTL_RECOIL), n);
      break;
    case EV_ASSORBE:
    case EV_CURA:
      cura(l, e.valore);
      snprintf(riga, sizeof(riga), T(S_BTL_HEAL), n);
      break;
    case EV_KO:
      koNarrato[l] = true;
      koFino[l] = occupatoFino = now + BTL_KO_MS;
      halSfx(SFX_FAINT);
      snprintf(riga, sizeof(riga), T(S_BTL_FAINT), n);
      break;
    case EV_FINE: {
      // il vincitore come lato della schermata: 0 noi, 1 l'altro, 2 pareggio
      uint8_t v = e.valore == BATTLE_PAREGGIO ? 2 : latoM((uint8_t)e.valore);
      // vittoria: la fanfara di Rosso/Blu, o la medaglia con la MUSICA spenta; sconfitta e
      // pareggio fermano la musica
      if (v == 0 && halMusicaAccesa()) {
        halMusica(gLinkMode ? MUS_DEFEATEDTRAINER : MUS_DEFEATEDWILDMON);
      } else {
        halMusica(MUS_NESSUNA);
        halSfx(v == 0 ? SFX_MEDAL : SFX_BYE);
      }
      if (v == 0) snprintf(riga, sizeof(riga), T(S_BTL_WIN));
      else if (v == 1) snprintf(riga, sizeof(riga), T(S_BTL_LOSE));
      else snprintf(riga, sizeof(riga), T(S_BTL_DRAW));
      break;
    }
    case EV_CARICA: snprintf(riga, sizeof(riga), T(S_BTL_CHARGE), n); break;
    case EV_STADIO: {
      const char *st = T(STAT_NOME[(e.valore >> 8) % 6]);
      if ((int8_t)(e.valore & 0xFF) > 0) snprintf(riga, sizeof(riga), T(S_BTL_STAT_UP), n, st);
      else snprintf(riga, sizeof(riga), T(S_BTL_STAT_DOWN), n, st);
      break;
    }
    case EV_STATO:
      if (e.valore < ST_CONFUSIONE) statoNarrato[l] = (uint8_t)e.valore;
      switch (e.valore) {
        case ST_PARALISI: snprintf(riga, sizeof(riga), T(S_BTL_PARA), n); break;
        case ST_SCOTTATURA: snprintf(riga, sizeof(riga), T(S_BTL_BURN), n); break;
        case ST_VELENO: snprintf(riga, sizeof(riga), T(S_BTL_POISON), n); break;
        case ST_TOSSICO: snprintf(riga, sizeof(riga), T(S_BTL_TOXIC), n); break;
        case ST_SONNO: snprintf(riga, sizeof(riga), T(S_BTL_SLEEP), n); break;
        case ST_CONGELAMENTO: snprintf(riga, sizeof(riga), T(S_BTL_FROZEN), n); break;
        case ST_CONFUSIONE: snprintf(riga, sizeof(riga), T(S_BTL_CONFUSED), n); break;
        default: return;
      }
      break;
    case EV_DORME: snprintf(riga, sizeof(riga), T(S_BTL_ASLEEP), n); break;
    case EV_SVEGLIA:
      statoNarrato[l] = ST_NESSUNO;
      snprintf(riga, sizeof(riga), T(S_BTL_WAKE), n);
      break;
    case EV_CONGELATO: snprintf(riga, sizeof(riga), T(S_BTL_FROZEN), n); break;
    case EV_SCONGELA:
      statoNarrato[l] = ST_NESSUNO;
      snprintf(riga, sizeof(riga), T(S_BTL_THAW), n);
      break;
    case EV_PARALIZZATO:
    case EV_INTRAPPOLATO: snprintf(riga, sizeof(riga), T(S_BTL_CANT_MOVE), n); break;
    case EV_CONFUSO: snprintf(riga, sizeof(riga), T(S_BTL_CONFUSED), n); break;
    case EV_SI_COLPISCE:
      colpisci(l, e.valore);
      snprintf(riga, sizeof(riga), T(S_BTL_HURTSELF));
      break;
    case EV_NON_PIU_CONFUSO: snprintf(riga, sizeof(riga), T(S_BTL_UNCONFUSED), n); break;
    case EV_TENTENNA: snprintf(riga, sizeof(riga), T(S_BTL_FLINCH), n); break;
    case EV_VELENO:
      colpisci(l, e.valore);
      snprintf(riga, sizeof(riga), T(S_BTL_PSN_DMG), n);
      break;
    case EV_SCOTTATURA:
      colpisci(l, e.valore);
      snprintf(riga, sizeof(riga), T(S_BTL_BRN_DMG), n);
      break;
    case EV_PARASSISEME:
      colpisci(l, e.valore);
      snprintf(riga, sizeof(riga), T(S_BTL_SEED_DMG), n);
      break;
    case EV_SEME: snprintf(riga, sizeof(riga), T(S_BTL_SEEDED), n); break;
    case EV_IRA: snprintf(riga, sizeof(riga), T(S_BTL_RAGE), n); break;
    case EV_PROTETTO: snprintf(riga, sizeof(riga), T(S_BTL_PROTECTED), n); break;
    case EV_NEBBIA: snprintf(riga, sizeof(riga), T(S_BTL_MIST), n); break;
    case EV_CONCENTRAZIONE: snprintf(riga, sizeof(riga), T(S_BTL_FOCUS), n); break;
    case EV_BARRIERA: snprintf(riga, sizeof(riga), T(S_BTL_BARRIER), n); break;
    case EV_AZZERA: snprintf(riga, sizeof(riga), T(S_BTL_RESET)); break;
    case EV_CONVERSIONE: snprintf(riga, sizeof(riga), T(S_BTL_CONVERT), n); break;
    case EV_TRASFORMA:
      dexNarrato[l] = (int16_t)e.valore;
      snprintf(riga, sizeof(riga), T(S_BTL_TRANSFORM), n, dexName((int16_t)e.valore));
      break;
    case EV_RITIRO:  // esce di scena; entra l'altro con EV_ENTRA
      inCampo[l] = false;
      if (l == 0) snprintf(riga, sizeof(riga), T(S_LKB_BACK), n);
      else snprintf(riga, sizeof(riga), T(S_LKB_WITHDRAW), allenatore(1), n);
      break;
    case EV_ENTRA: {  // valore = indice << 8 | dex
      uint8_t i = (uint8_t)(e.valore >> 8) % BATTLE_SQUADRA;
      membroNarrato[l] = i;
      dexNarrato[l] = (int16_t)(e.valore & 0xFF);
      psNarrati[l] = psMostrati[l] = psMembro[l][i];
      statoNarrato[l] = statoMembro[l][i];
      koNarrato[l] = false;
      inCampo[l] = true;
      entraFino[l] = occupatoFino = now + BTL_ENTRA_MS;
      caricaSprite(l);
      halSfx(SFX_TAP);
      n = nomeLato(l);
      if (l == 0) snprintf(riga, sizeof(riga), T(S_BTL_GO), n);
      else snprintf(riga, sizeof(riga), T(S_LKB_SENDS), allenatore(1), n);
      break;
    }
    default: return;
  }
  rigaDaLeggere = true;
}

// la barra non è ancora arrivata al valore narrato
static bool barreInMoto() {
  return psMostrati[0] != psNarrati[0] || psMostrati[1] != psNarrati[1];
}

// la riga è stata mostrata, le animazioni sono finite e la prossima vuole il suo posto
static bool aspettaTocco() {
  if (fase != BF_NARRA || !rigaDaLeggere || timeLeft(occupatoFino) || barreInMoto()) return false;
  return Q().vuota() || conTesto(Q().prossimo().tipo);
}

// passo raccontato: lo schermo si riallinea al motore
static void riallinea() {
  const Battle &b = B();
  for (uint8_t u = 0; u < 2; u++) {
    uint8_t k = latoM(u);
    const Combatant &c = b.lato[k];
    if (membroNarrato[u] != b.inCampo[k]) {  // solo se l'host ha corretto qualcosa
      membroNarrato[u] = b.inCampo[k];
      inCampo[u] = true;
      koNarrato[u] = c.ko();
      caricaSprite(u);
    }
    psNarrati[u] = c.ps;
    statoNarrato[u] = c.stato;
    dexNarrato[u] = c.dex;
    for (uint8_t i = 0; i < b.membri[k] && i < BATTLE_SQUADRA; i++) {
      psMembro[u][i] = battleMembro(b, k, i).ps;
      statoMembro[u][i] = battleMembro(b, k, i).stato;
    }
  }
}

// la scelta di un turno: slot 0..3 o BATTLE_CAMBIA + i. Contro la CPU si gioca subito
// (la CPU sceglie la sua); nel collegamento va all'altra scheda. false se non è valida.
static bool gioca(uint8_t scelta) {
  if (gLinkMode) {
    if (!linkLb().scegli(scelta)) return false;
    riga[0] = 0;
  } else {
    uint8_t ia = battleDeveScegliere(bt().lato[1]) ? battleSceltaIA(bt(), 1, false) : 0;
    battleTurno(bt(), scelta, ia);
  }
  fase = BF_NARRA;
  cambioForzato = false;
  return true;
}

// tocca all'altro: la nostra scelta è fatta (o non tocca a noi) e non c'è niente da narrare
static bool attesaLink() {
  if (!gLinkMode || fase != BF_NARRA || rigaDaLeggere || !Q().vuota() || barreInMoto() ||
      timeLeft(occupatoFino))
    return false;
  LinkBattaglia &lb = linkLb();
  return (lb.fase() == LBF_SCELTA || lb.fase() == LBF_CAMBIO) && !lb.devoScegliere();
}

// collegamento, narrazione finita: il prossimo passo, l'attesa o la fine
static bool avanzaLink() {
  LinkBattaglia &lb = linkLb();
  LbFase f = lb.fase();
  if (f != LBF_SCELTA && f != LBF_CAMBIO) {
    uint8_t v = linkFineVincitore();
    if (v == BATTLE_IN_CORSO) {  // l'altro è uscito, errore o collegamento perso: lo dice SCR_LINK
      goScreen(SCR_LINK);
      return false;
    }
    if (linkFineMotivo() == LBM_RESA && !resaNarrata) {  // si è arreso chi ha perso
      resaNarrata = true;
      Q().metti({ EVU_RESA, (uint8_t)(v == gMio ? 1 : 0), 0, 0 });
      Q().metti({ EV_FINE, 0, 0, v });
      return true;
    }
    fase = BF_FINE;
    return true;
  }
  riallinea();
  if (barreInMoto() || !lb.devoScegliere()) return true;  // le barre, o l'attesa dell'altro
  if (f == LBF_CAMBIO) {  // il nostro è KO: si sceglie il sostituto
    fase = BF_SQUADRA;
    cambioForzato = true;
    return true;
  }
  if (battleDeveScegliere(B().lato[gMio])) {
    fase = BF_MENU;
    riga[0] = 0;
  } else {
    gioca(0);  // ricarica, caricamento, mossa che si ripete: niente menu
  }
  return true;
}

// fa avanzare la narrazione; false se la battaglia è finita e si è usciti
static bool avanza() {
  if (fase != BF_NARRA || timeLeft(occupatoFino) || barreInMoto()) return true;
  if (!Q().vuota()) {
    if (rigaDaLeggere && conTesto(Q().prossimo().tipo)) return true;
    BattleEvento e = {};
    Q().prendi(e);
    narra(e);
    return true;
  }
  if (rigaDaLeggere) return true;
  if (gLinkMode) return avanzaLink();
  if (finita()) {
    if (!esitoFatto) {  // la riga del premio, se c'è, si legge prima di uscire
      applicaEsito(fuggito ? BTL_FUGA
                   : bt().vincitore == 0 ? BTL_VINTA
                   : bt().vincitore == 1 ? BTL_PERSA : BTL_PARI);
      return true;
    }
    goScreen(SCR_MAIN);
    return false;
  }
  riallinea();
  if (barreInMoto()) return true;
  if (battleDeveScegliere(bt().lato[0])) {
    fase = BF_MENU;
    riga[0] = 0;
  } else {
    gioca(0);  // ricarica, caricamento, mossa che si ripete: niente menu
  }
  return true;
}

// ---------- disegno ----------

// lo sprite PMD con cui disegnare il lato: la specie mostrata può essere quella
// dell'altro dopo una Trasformazione, e allora si usa il suo sprite
static PmdMon *spriteDi(uint8_t l) {
  int16_t d = dexNarrato[l];
  PmdMon *proprio = l ? &foePmd : &ourPmd, *altro = l ? &ourPmd : &foePmd;
  if (proprio->loaded && d == spriteDex[l]) return proprio;
  if (!gLinkMode && d == pet.speciesId && pmd.loaded) return &pmd;
  if (altro->loaded && d == spriteDex[l ^ 1]) return altro;
  return nullptr;
}

static void drawLato(uint8_t l) {
  if (!inCampo[l]) return;
  uint32_t now = millis();
  int cx = l ? FOE_X : OUR_X, gy = l ? FOE_GY : OUR_GY;
  int ox = 0, oy = 0, clip = 0x7FFF;
  if (koNarrato[l]) {  // affonda sotto la pedana, poi sparisce
    uint32_t left = timeLeft(koFino[l]);
    if (!left) return;
    oy = (int)((BTL_KO_MS - left) * 110 / BTL_KO_MS);
    clip = gy;
  }
  if (uint32_t left = timeLeft(entraFino[l]))  // scivola dentro dal bordo
    ox = (l ? 1 : -1) * (int)(left * 170 / BTL_ENTRA_MS);
  uint8_t act = PMD_IDLE;
  bool loop = true;
  uint32_t t = now;
  bool colpito = timeLeft(colpoFino[l]) > 0;
  if (colpito) {
    act = PMD_HURT;
    loop = false;
    t = BTL_COLPO_MS - timeLeft(colpoFino[l]);
    ox += ((t / 60) % 2) ? 5 : -5;
  } else if (uint32_t left = timeLeft(attaccoFino[l])) {
    act = PMD_ATTACK;
    loop = false;
    t = BTL_ATTACCO_MS - left;
    // slancio verso l'altro e ritorno
    int amt = (int)(t < BTL_ATTACCO_MS / 2 ? t : BTL_ATTACCO_MS - t) * 26 / (BTL_ATTACCO_MS / 2);
    ox += l ? -amt : amt;
    oy += l ? amt / 2 : -amt / 2;
  }
  if (PmdMon *m = spriteDi(l)) {
    if (!m->has(act)) {
      act = PMD_IDLE;
      loop = true;
      t = now;
    }
    // il nostro è specchiato: guarda verso l'avversario
    drawPmdActM(*m, act, cx + ox, gy + oy, t, loop, false, 3, l == 0, clip);
    return;
  }
  // senza SD: la miniatura del Pokédex, che lampeggia quando è colpita
  if (koNarrato[l]) return;
  const uint8_t *th = thumbs.get(dexNarrato[l]);
  if (!th) return;
  int h = th[1] * 3;
  drawThumb(th, cx + ox - GAL_CELL / 2, gy + oy - h - (GAL_CELL - h) / 2, 3,
            colpito && ((t / 60) % 2 == 0));
}

// barra dei PS in stile Game Boy: verde sopra metà, gialla sopra un quarto, poi rossa
static void drawBarraPs(int x, int y, int w, uint16_t ps, uint16_t max) {
  int fw = max ? (int)((uint32_t)(w - 4) * ps / max) : 0;
  if (ps && fw < 1) fw = 1;
  uint16_t col = (ps * 2 > max) ? UI_BAR_OK : (ps * 4 > max) ? UI_BAR_WARN : UI_BAR_BAD;
  gfx->fillRoundRect(x, y, w, 12, 4, UI_TRACK);
  if (fw > 0) gfx->fillRoundRect(x + 2, y + 2, fw, 8, 3, col);
  gfx->drawRoundRect(x, y, w, 12, 4, UI_INK);
}

// larghezza della pillola dello stato maggiore; 0 se sano o confuso (niente pillola)
static int larghezzaStato(uint8_t st) {
  if (st < ST_PARALISI || st > ST_CONGELAMENTO) return 0;
  return textW(T(AB[st - 1]), 1) + 10;
}

// pillola dello stato maggiore (niente se sano o confuso); restituisce la larghezza
static int drawStato(int x, int y, uint8_t st) {
  int w = larghezzaStato(st);
  if (!w) return 0;
  const char *ab = T(AB[st - 1]);
  gfx->fillRoundRect(x, y, w, 18, 5, AB_COL[st - 1]);
  gfx->setTextColor(UI_WHITE);
  setSize(1);
  setCur(x + 5, y + 1 + (LANG_IS_CJK(gLang) ? 0 : 5));
  printT(ab);
  return w;
}

// targa di un lato: nome; livello (e i nostri PS in cifre); barra dei PS. Lo stato è una
// pillola sul bordo in alto a destra, così le tre righe restano a scala 2 e leggibili
static void drawTarga(uint8_t l) {
  int px = l ? 92 : 220, py = l ? 56 : 190, pw = l ? 168 : 190, ph = l ? 74 : 78;
  const Combatant &c = membro(l);
  gfx->fillRoundRect(px, py, pw, ph, 10, UI_BG_DAY);
  gfx->drawRoundRect(px, py, pw, ph, 10, UI_INK);
  gfx->drawRoundRect(px + 1, py + 1, pw - 2, ph - 2, 9, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(px + 10, py + 8);
  printT(nomeLato(l));

  char lv[16];
  snprintf(lv, sizeof(lv), T(S_LVL_FMT), c.livello);
  setCur(px + 10, py + 30);
  printT(lv);
  if (l == 0) {  // i nostri PS in cifre, come nei giochi
    char ps[16];
    snprintf(ps, sizeof(ps), "%u/%u", psMostrati[0], c.psMax);
    setCur(px + pw - 12 - textW(ps, 2), py + 30);
    printT(ps);
  }
  const char *hp = T(S_BTL_HP);
  int bx = px + 10;
  gfx->setTextColor(UI_BAR_WARN);
  setCur(bx, py + 51);
  printT(hp);
  bx += textW(hp, 2) + 6;
  drawBarraPs(bx, py + 53, px + pw - 12 - bx, psMostrati[l], c.psMax);

  int sw = larghezzaStato(statoNarrato[l]);
  if (sw) drawStato(px + pw - 14 - sw, py - 9, statoNarrato[l]);
}

// spezza `s` in righe larghe al massimo w px a scala 2, sugli spazi; l'ultima prende il resto
static uint8_t aCapo(const char *s, char out[][sizeof(riga)], uint8_t max, int w) {
  uint8_t n = 0;
  setSize(2);
  while (*s && n < max) {
    while (*s == ' ') s++;
    size_t len = strlen(s), taglio = 0;
    for (size_t i = 0; i <= len; i++) {
      if (s[i] != ' ' && s[i]) continue;
      memcpy(out[n], s, i);
      out[n][i] = 0;
      if (taglio && textW(out[n], 2) > w) break;
      taglio = i;
    }
    if (n == max - 1) taglio = len;
    memcpy(out[n], s, taglio);
    out[n][taglio] = 0;
    s += taglio;
    n++;
  }
  return n;
}

static void drawTesto() {
  static char righe[3][sizeof(riga)];
  bool attesa = attesaLink();
  if (attesa) snprintf(riga, sizeof(riga), T(S_LKB_WAITING), linkNomeAltro());
  gfx->fillRoundRect(BOX_X, BOX_Y, BOX_W, BOX_H, 12, UI_WHITE);
  gfx->drawRoundRect(BOX_X, BOX_Y, BOX_W, BOX_H, 12, UI_INK);
  gfx->drawRoundRect(BOX_X + 1, BOX_Y + 1, BOX_W - 2, BOX_H - 2, 11, UI_INK);
  uint8_t n = aCapo(riga, righe, 3, BOX_W - 28);
  gfx->setTextColor(attesa ? UI_HINT : UI_INK);
  for (uint8_t i = 0; i < n; i++) {
    setCur(BOX_X + 14, BOX_Y + 12 + i * 24);
    printT(righe[i]);
  }
  if (attesa) {  // tre puntini che si accendono a turno: l'altra scheda sta scegliendo
    uint8_t acceso = (millis() / 300) % 3;
    for (uint8_t i = 0; i < 3; i++)
      gfx->fillCircle(BOX_X + BOX_W - 50 + i * 14, BOX_Y + BOX_H - 16, 4,
                      i == acceso ? UI_INK : UI_TRACK);
  }
  if (aspettaTocco()) {
    // sotto il riquadro, sul fondo chiaro della fascia: dentro, a dimensione 1, non si leggeva
    const char *tap = T(S_BTL_TAP);
    setSize(2);
    gfx->setTextColor(UI_HINT);
    setCur(centerX(tap, 2), BOX_Y + BOX_H + 10);
    printT(tap);
    if ((millis() / 400) % 2) {  // triangolino che lampeggia, come nei giochi
      int tx = BOX_X + BOX_W - 30, ty = BOX_Y + BOX_H - 18;
      gfx->fillTriangle(tx, ty, tx + 12, ty, tx + 6, ty + 8, UI_INK);
    }
  }
}

// pulsante del menu: testo a dimensione 3, o 2 se non ci sta
static void drawPulsante(int x, int y, int w, int h, uint16_t col, const char *txt,
                         uint16_t ink = UI_INK) {
  gfx->fillRoundRect(x, y, w, h, 12, col);
  gfx->drawRoundRect(x, y, w, h, 12, UI_INK);
  gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, 11, UI_INK);
  gfx->setTextColor(ink);
  uint8_t sz = textW(txt, 3) <= w - 12 ? 3 : 2;
  setSize(sz);
  int dy = sz == 3 ? (LANG_IS_CJK(gLang) ? 8 : 12) : (LANG_IS_CJK(gLang) ? 8 : 7);
  setCur(x + (w - textW(txt, sz)) / 2, y + h / 2 - dy);
  printT(txt);
}

static void drawMenu() {
  drawPulsante(CX - 5 - MENU_W, MENU_Y, MENU_W, MENU_H, C565(0xf4, 0xa8, 0x98), T(S_BTL_FIGHT));
  drawPulsante(CX + 5, MENU_Y, MENU_W, MENU_H, C565(0xa8, 0xd8, 0xa0), T(S_BTL_PKMN));
  drawPulsante(CX - MENU_W / 2, RUN_Y, MENU_W, RUN_H, C565(0xa8, 0xc8, 0xf0), T(S_BTL_RUN));
}

// indietro: freccia verso sinistra
static void drawIndietro() {
  gfx->fillRoundRect(BACK_X, BACK_Y, BACK_W, BACK_H, 12, UI_TRACK);
  gfx->drawRoundRect(BACK_X, BACK_Y, BACK_W, BACK_H, 12, UI_INK);
  int ax = CX, ay = BACK_Y + BACK_H / 2;
  gfx->fillTriangle(ax - 22, ay, ax - 6, ay - 12, ax - 6, ay + 12, UI_INK);
  gfx->fillRect(ax - 6, ay - 4, 28, 8, UI_INK);
}

static void drawMosse() {
  const Combatant &c = B().lato[gMio];
  for (uint8_t i = 0; i < BATTLE_MOSSE; i++) {
    int x = CELL_X(i), y = CELL_Y(i);
    uint8_t mv = c.mosse[i];
    if (!mv) {  // slot vuoto
      gfx->fillRoundRect(x, y, CELL_W, CELL_H, 10, UI_TRACK);
      gfx->drawRoundRect(x, y, CELL_W, CELL_H, 10, lerp565(UI_INK, UI_WHITE, 8, 16));
      continue;
    }
    uint16_t tc = TYPE_COLOR[MOVE_TBL[mv].type % TYPE_COUNT];
    bool vuota = c.pp[i] == 0;
    gfx->fillRoundRect(x, y, CELL_W, CELL_H, 10, vuota ? UI_TRACK : lerp565(tc, UI_WHITE, 10, 16));
    gfx->fillRoundRect(x, y, 10, CELL_H, 5, tc);  // la fascia del tipo
    gfx->drawRoundRect(x, y, CELL_W, CELL_H, 10, UI_INK);
    // esaurita: il nome sbiadito e barrato, perché il fondo grigio da solo si confonde con
    // il colore schiarito del tipo Normale
    gfx->setTextColor(vuota ? UI_HINT : UI_INK);
    setSize(2);
    setCur(x + 16, y + 7);
    printT(moveName(mv));
    if (vuota) gfx->fillRect(x + 14, y + 13, textW(moveName(mv), 2) + 4, 2, UI_HINT);
    char pp[16];
    snprintf(pp, sizeof(pp), "PP %u/%u", c.pp[i], c.ppMax[i]);
    gfx->setTextColor(vuota ? UI_BAR_BAD : UI_INK);
    setCur(x + CELL_W - 10 - textW(pp, 2), y + CELL_H - 22);
    printT(pp);
  }
  drawIndietro();
}

// POKEMON: la nostra squadra con PS e stati, a schermo intero. Chi è in campo ha il bordo
// verde, chi è KO è grigio. Dopo un KO (cambioForzato) è la scelta del sostituto.
static void drawSquadra() {
  const Battle &b = B();
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  const char *tit = T(cambioForzato ? S_LKB_NEXT : S_BTL_PKMN);
  uint8_t ts = textW(tit, 3) <= 260 ? 3 : 2;
  gfx->setTextColor(UI_INK);
  setSize(ts);
  setCur(centerX(tit, ts), 60);
  printT(tit);
  for (uint8_t i = 0; i < b.membri[gMio] && i < BATTLE_SQUADRA; i++) {
    const Combatant &c = battleMembro(b, gMio, i);
    int y = SQ_Y0 + i * (SQ_H + SQ_GAP);
    bool campo = i == b.inCampo[gMio];
    gfx->fillRoundRect(SQ_X, y, SQ_W, SQ_H, 12, c.ko() ? UI_TRACK : UI_WHITE);
    gfx->drawRoundRect(SQ_X, y, SQ_W, SQ_H, 12, campo ? UI_BAR_OK : UI_INK);
    if (campo) gfx->drawRoundRect(SQ_X + 1, y + 1, SQ_W - 2, SQ_H - 2, 11, UI_BAR_OK);
    if (const uint8_t *th = thumbs.get(c.dex)) drawThumb(th, SQ_X + 10, y + 15, 1, c.ko());
    gfx->setTextColor(c.ko() ? UI_HINT : UI_INK);
    setSize(2);
    setCur(SQ_X + 60, y + 10);
    printT(nomeMembro(0, i));
    char lv[16];
    snprintf(lv, sizeof(lv), T(S_LVL_FMT), c.livello);
    setSize(1);
    setCur(SQ_X + SQ_W - 14 - textW(lv, 1), y + 14);
    printT(lv);
    drawBarraPs(SQ_X + 60, y + 40, 150, c.ps, c.psMax);
    char ps[16];
    snprintf(ps, sizeof(ps), "%u/%u", c.ps, c.psMax);
    gfx->setTextColor(UI_INK);
    setCur(SQ_X + 218, y + 42);
    printT(ps);
    drawStato(SQ_X + SQ_W - 14 - 40, y + 38, c.stato);
  }
  if (!cambioForzato) drawIndietro();
}

// la domanda della resa (collegamento), nella fascia
static void drawResa() {
  gfx->setTextColor(UI_INK);
  const char *q = T(S_LKB_GIVE_UP_Q);
  setSize(2);
  setCur(centerX(q, 2), BAND_Y + 18);
  printT(q);
  drawPulsante(CX - 5 - DUE_W, DUE_Y, DUE_W, DUE_H, UI_BAR_BAD, T(S_YES), UI_WHITE);
  drawPulsante(CX + 5, DUE_Y, DUE_W, DUE_H, UI_TRACK, T(S_NO));
}

// fine della battaglia in collegamento: l'esito, una riga (vittorie, rivincita, attesa,
// l'altro è uscito), RIVINCITA ed ESCI
static void drawFine() {
  LinkBattaglia &lb = linkLb();
  uint8_t v = linkFineVincitore();
  const char *esito = T(v == BATTLE_PAREGGIO ? S_BTL_DRAW : v == gMio ? S_BTL_WIN : S_BTL_LOSE);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(esito, 2), BAND_Y + 12);
  printT(esito);
  char t[80];
  bool puo = lb.fase() == LBF_FINE && !lb.rivincitaMia();
  if (lb.fase() != LBF_FINE) snprintf(t, sizeof(t), "%s", T(S_LINK_LEFT));
  else if (lb.rivincitaMia()) snprintf(t, sizeof(t), T(S_LKB_WAITING), linkNomeAltro());
  else if (lb.rivincitaAltro()) snprintf(t, sizeof(t), T(S_LKB_WANTS_REMATCH), linkNomeAltro());
  else snprintf(t, sizeof(t), T(S_LKB_WINS), pet.linkWins());
  gfx->setTextColor(lb.rivincitaAltro() && puo ? UI_BAR_OK : UI_HINT);
  uint8_t sz = textW(t, 2) <= 360 ? 2 : 1;
  setSize(sz);
  setCur(centerX(t, sz), BAND_Y + 38 + (sz == 2 ? 0 : 4));
  printT(t);
  drawPulsante(CX - 5 - DUE_W, DUE_Y, DUE_W, DUE_H, puo ? UI_BAR_OK : UI_TRACK,
               T(S_LKB_REMATCH), puo ? UI_WHITE : UI_HINT);
  drawPulsante(CX + 5, DUE_Y, DUE_W, DUE_H, UI_TRACK, T(S_LKB_EXIT));
}

void renderBattle() {
  if (gLinkMode) {
    LbFase f = linkLb().fase();
    bool inLotta = f == LBF_SCELTA || f == LBF_CAMBIO;
    if (fase == BF_FINE) {
      if (f == LBF_SQUADRA) {  // rivincita accettata da tutti e due: si sceglie la squadra
        goScreen(SCR_LINK);
        return;
      }
    } else if (fase != BF_NARRA && !inLotta) {
      fase = BF_NARRA;  // l'altro si è arreso o è uscito mentre sceglievamo: si racconta
    }
  }
  if (!avanza()) return;
  // le barre scendono un po' a ogni fotogramma invece di saltare al valore nuovo
  for (uint8_t l = 0; l < 2; l++) {
    int diff = (int)psNarrati[l] - (int)psMostrati[l];
    if (!diff) continue;
    int passo = membro(l).psMax / 24;
    if (passo < 1) passo = 1;
    if (abs(diff) < passo) passo = abs(diff);
    psMostrati[l] = (uint16_t)((int)psMostrati[l] + (diff > 0 ? passo : -passo));
  }
  if (fase == BF_SQUADRA) {
    drawSquadra();
    halFlush();
    return;
  }

  uint32_t now = millis();
  int h = sceneHour();
  bool notte = h < 6 || h >= 20;
  uint8_t bioma = pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome;
  drawScene(bioma, now, notte);
  // le pedane sotto i due Pokémon
  uint16_t suolo = BIOME_SOIL[bioma < 6 ? bioma : 0];
  uint16_t pedana = lerp565(suolo, INK_K, notte ? 9 : 5, 16);
  gfx->fillEllipse(FOE_X, FOE_GY, 74, 14, pedana);
  gfx->fillEllipse(OUR_X, OUR_GY, 84, 12, pedana);
  drawLato(1);
  drawLato(0);
  drawTarga(1);
  drawTarga(0);

  gfx->fillRect(0, BAND_Y, 466, 466 - BAND_Y, UI_BG_DAY);
  gfx->fillRect(0, BAND_Y, 466, 3, UI_INK);
  if (fase == BF_MENU) drawMenu();
  else if (fase == BF_MOSSE) drawMosse();
  else if (fase == BF_RESA) drawResa();
  else if (fase == BF_FINE) drawFine();
  else drawTesto();
  halFlush();
}

// ---------- tocchi ----------

static bool dentro(int16_t x, int16_t y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static void squadraTap(int16_t x, int16_t y) {
  if (!cambioForzato && (y < SQ_Y0 - 40 || dentro(x, y, BACK_X, BACK_Y, BACK_W, BACK_H))) {
    halSfx(SFX_TAP);
    fase = BF_MENU;
    return;
  }
  const Battle &b = B();
  for (uint8_t i = 0; i < b.membri[gMio] && i < BATTLE_SQUADRA; i++) {
    if (!dentro(x, y, SQ_X, SQ_Y0 + i * (SQ_H + SQ_GAP), SQ_W, SQ_H)) continue;
    // contro la CPU la squadra è il solo pet: la lista si guarda e basta
    if (gLinkMode && battlePuoCambiare(b, gMio, i) && gioca(BATTLE_CAMBIA + i)) halSfx(SFX_TAP);
    else halSfx(SFX_DENY);
    return;
  }
}

static void fineTap(int16_t x, int16_t y) {
  LinkBattaglia &lb = linkLb();
  if (dentro(x, y, CX - 5 - DUE_W, DUE_Y, DUE_W, DUE_H)) {
    halSfx(lb.rivincita() ? SFX_TAP : SFX_DENY);
  } else if (dentro(x, y, CX + 5, DUE_Y, DUE_W, DUE_H)) {
    halSfx(SFX_TAP);
    if (lb.fase() != LBF_LIBERO) lb.esci();
    goScreen(SCR_LINK);
  }
}

void battleTap(int16_t x, int16_t y) {
  if (fase == BF_NARRA) {  // un tocco ovunque fa leggere la riga successiva
    if (aspettaTocco()) rigaDaLeggere = false;
    return;
  }
  if (fase == BF_SQUADRA) {
    squadraTap(x, y);
    return;
  }
  if (fase == BF_FINE) {
    fineTap(x, y);
    return;
  }
  if (fase == BF_RESA) {  // SI: resa; NO o fuori: si torna al menu
    if (dentro(x, y, CX - 5 - DUE_W, DUE_Y, DUE_W, DUE_H) && linkLb().arrenditi()) {
      halSfx(SFX_TAP);
      fase = BF_NARRA;
    } else {
      halSfx(SFX_TAP);
      fase = BF_MENU;
    }
    return;
  }
  if (fase == BF_MENU) {
    if (dentro(x, y, CX - 5 - MENU_W, MENU_Y, MENU_W, MENU_H)) {
      // senza PP non c'è niente da scegliere: Scontro
      if (!battleDeveScontro(B().lato[gMio])) {
        halSfx(SFX_TAP);
        fase = BF_MOSSE;
      } else {
        halSfx(gioca(0) ? SFX_TAP : SFX_DENY);
      }
    } else if (dentro(x, y, CX + 5, MENU_Y, MENU_W, MENU_H)) {
      halSfx(SFX_TAP);
      fase = BF_SQUADRA;
      cambioForzato = false;
    } else if (dentro(x, y, CX - MENU_W / 2, RUN_Y, MENU_W, RUN_H)) {
      halSfx(SFX_TAP);
      if (gLinkMode) {  // nel collegamento la fuga è una resa: prima si chiede
        fase = BF_RESA;
        return;
      }
      fuggito = true;
      fase = BF_NARRA;
      bt().coda.metti({ EVU_FUGA, 0, 0, 0 });
    }
    return;
  }
  // BF_MOSSE: una mossa, oppure indietro (freccia o la parte alta dello schermo)
  if (y < BAND_Y || dentro(x, y, BACK_X, BACK_Y, BACK_W, BACK_H)) {
    halSfx(SFX_TAP);
    fase = BF_MENU;
    return;
  }
  const Combatant &c = B().lato[gMio];
  for (uint8_t i = 0; i < BATTLE_MOSSE; i++) {
    if (!c.mosse[i] || !dentro(x, y, CELL_X(i), CELL_Y(i), CELL_W, CELL_H)) continue;
    if (!c.pp[i]) {
      halSfx(SFX_DENY);
      return;
    }
    halSfx(gioca(i) ? SFX_TAP : SFX_DENY);
    return;
  }
}
