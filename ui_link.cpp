// Collegamento (SCR_LINK): accende la radio, cerca un altro PokeGotchi con il
// protocollo di link.h, mostra il codice dell'abbinamento e la specie dell'altro e, dopo
// le due conferme, la scelta LOTTA / SCAMBIA.
// La radio è accesa solo qui e nella battaglia che parte da qui: onExit la spegne, tranne
// quando si va in campo, e linkLoop la spegne se si finisce altrove. Con la radio accesa
// niente scritture sulla NVS (bloccano entrambi i core per ~1 s, e il keepalive
// salterebbe): il loop non fa il salvataggio periodico e lo si fa all'uscita.
//
// LOTTA (LinkBattaglia di linkbattaglia.h): l'ospite invita; l'host sceglie la
// regola Coppa (anche senza invito) e la propone; l'ospite la vede e accetta. Poi ognuno
// sceglie la squadra, fino a 3 tra il pet attivo (se Pet::canBattle) e la collezione, e
// con le due squadre si va in SCR_BATTLE (ui_battle.cpp), che torna qui alla fine o per la
// squadra della rivincita. Ogni attesa ha ANNULLA (esci()). Le ricompense (Pet::
// linkBattleResult) si danno qui, alla conferma di END.
//
// SCAMBIA (LinkScambio di linkscambio.h): si sceglie il pet da offrire, l'attivo o
// uno della collezione (Pet::scambioPerche dice perché no: uovo, cerimonia...). La trattativa
// mostra le due offerte (miniatura, livello, shiny, allenatore originale); SCAMBIA chiede
// conferma e poi accetta, e lo scambio parte quando hanno accettato tutti e due: copia sulla
// SD con motivo «scambio», giornale, COMMIT. L'offerta dell'altro, la ripresa di uno scambio
// sospeso e l'esito aprono la trattativa da soli. Un'evoluzione da scambio si vede qui per un
// pet della collezione (miniature), nella cerimonia della principale per il pet attivo. Uno
// scambio sospeso con un altro partner (o senza collegamento) si può annullare a mano, con
// un avviso. Il giornale si scrive sulla NVS anche con la radio accesa: è l'unica eccezione.

#include "ui_common.h"
#include "link.h"
#include "linkbattaglia.h"
#include "linkscambio.h"
#include "savesd.h"

#define LK_BTN_Y 300    // pulsanti (alti 44, come nella scheda)
#define LK_BTN_H 44
#define LK_BTN_W 160
#define LK_SX (CX - 3 - LK_BTN_W)   // sinistro: SI, LOTTA, RIPROVA da solo al centro
#define LK_DX (CX + 3)              // destro: NO, SCAMBIA
#define LK_SOLO_X (CX - LK_BTN_W / 2)
#define REG_X (CX - 150)    // scelta della regola: due pulsanti larghi, uno sotto l'altro
#define REG_W 300
#define REG_H 50
#define REG_Y1 168
#define REG_Y2 230
#define SQ_CW 110           // scelta della squadra: caselle 3 + 3
#define SQ_CH 120
#define SQ_GAP 6
#define SQ_Y0 84
#define SQ_BTN_Y 340
#define SC_CW 160           // trattativa: le due carte, la mia a sinistra
#define SC_CH 184
#define SC_Y 80
#define SC_SX (CX - 3 - SC_CW)
#define SC_DX (CX + 3)
#define SC_RIGA 272         // riga di stato, sopra i pulsanti

static Link *gLinkP = nullptr;  // in PSRAM (inPsram), come gLb e gLs
static Link &lk() { return inPsram(gLinkP); }
static bool gRadio = false;    // halRadioBegin() riuscita: si resta qui con la radio accesa
static uint8_t gMac[6];

static LinkBattaglia *gLbP = nullptr;
static LinkBattaglia &lb() { return inPsram(gLbP); }
static bool gLbAvviata = false;      // lb().avvia() fatto per questo collegamento
static bool gVersoBattaglia = false; // si va in SCR_BATTLE: onExit lascia la radio accesa
static bool gScegliRegola = false;   // host: la scelta della regola è a schermo
// squadra: i candidati (il pet attivo, se può lottare, poi la collezione) e i scelti in ordine
static PetRitratto gCand[1 + COL_POSTI];
static uint8_t gNCand = 0;
static bool gCandAttivo = false;     // gCand[0] è il pet attivo
static bool gCandSpento = false;     // ...che ora non lotta: grigio, con il motivo
static bool gCandFatti = false;      // candidati preparati per questa scelta
static uint8_t gScelti[BATTLE_SQUADRA];
static uint8_t gNScelti = 0;
static bool gAttivoInSquadra = false;
// fine della lotta, seguita a ogni pacchetto (seguiFine)
static uint8_t gFineVinc = BATTLE_IN_CORSO, gFineMotivo = LBM_NESSUNO;
static bool gEsitoFatto = false;

static LinkScambio *gLsP = nullptr;
static LinkScambio &ls() { return inPsram(gLsP); }
static bool gLsAvviato = false;      // ls().avvia() fatto per questo collegamento
enum : uint8_t { SC_NO = 0, SC_SCELTA, SC_TRATTA };  // vista: LOTTA/SCAMBIA, scelta, trattativa
static uint8_t gSc = SC_NO;
static int8_t gScPosto[1 + COL_POSTI];  // scelta: il posto (SCAMBIO_ATTIVO o 0..4) di gCand[i]
static bool gScConferma = false;     // SI/NO prima di accetta()
static bool gScForza = false;        // avviso prima dell'annullamento manuale
static StrId gScMsg = STR_COUNT;     // scelta: perché quel pet non si offre
static LsFase gScFase = LSF_LIBERO;  // fase vista l'ultima volta (seguiScambio)
static uint32_t gEvoDa = 0;          // inizio dell'evoluzione in collezione (FATTO)

LinkBattaglia &linkLb() { return lb(); }
uint8_t linkFineVincitore() { return gFineVinc; }
uint8_t linkFineMotivo() { return gFineMotivo; }

// all'avvio, subito dopo pet.begin(): il giornale di uno scambio interrotto
void linkScambioCarica() {
  ls().carica(pet);
  ls().copiaPrima(saveSdCopiaPrimaScambio, &pet);
  gScFase = ls().fase();
}

// il trasporto di Link: LINK_BROADCAST è il broadcast dell'HAL (mac nullo)
static void inviaRadio(void *, const uint8_t mac[6], const uint8_t *dati, uint8_t len) {
  halRadioSend(memcmp(mac, LINK_BROADCAST, 6) ? mac : nullptr, dati, len);
}

static void avviaLink() {
  LinkConfig c;
  memcpy(c.mac, gMac, 6);
  c.trainerId = pet.trainerId();
  c.specie = pet.isEgg() ? 0 : (uint16_t)pet.speciesId;
  pet.trainerName(c.nome, sizeof(c.nome));
  c.fw = FW_VERSION;
  c.seme = (uint32_t)random(1, 0x7FFFFFFF);
  LinkTrasporto tr;
  tr.invia = inviaRadio;
  lk().avvia(c, tr, millis());
  lb().ferma();
  gLbAvviata = false;
  gScegliRegola = false;
  gFineVinc = BATTLE_IN_CORSO;
  ls().ferma();
  gLsAvviato = false;
  gSc = SC_NO;
  gScForza = false;
}

// spegne la radio (BYE all'altro, così non aspetta il timeout) e fa il salvataggio rimandato
static void spegniRadio() {
  if (gRadio) {
    lb().ferma();
    gLbAvviata = false;
    ls().ferma();
    gLsAvviato = false;
    gSc = SC_NO;
    lk().chiudi();
    delay(20);       // il tempo di far partire il BYE prima di spegnere
    halRadioEnd();
    gRadio = false;
  }
  // l'evoluzione in collezione si vede solo qui: uscendo prima della fine non resta in
  // sospeso (bloccherebbe i prossimi scambi). Quella del pet attivo la mostra la principale.
  if (pet.evoScambioDa() && pet.evoScambioPosto() != SCAMBIO_ATTIVO) pet.avviaEvoScambio();
  if (pet.savePending()) pet.flushSave();
}

// COLLEGA della scheda: la prima volta chiede il nome dell'allenatore
void openLink() {
  char nome[LINK_NOME_LEN + 1];
  if (pet.trainerName(nome, sizeof(nome))) goScreen(SCR_LINK);
  else keyboardTrainer();
}

void linkEnter() {
  gScegliRegola = false;
  gCandFatti = false;
  gSc = SC_NO;
  gScConferma = gScForza = false;
  if (gRadio) return;  // ritorno dalla lotta: la radio e il collegamento sono ancora vivi
  gRadio = halRadioBegin(LINK_CANALE);
  if (!gRadio) return;
  halRadioMac(gMac);
  avviaLink();
}

void linkExit() {
  if (gVersoBattaglia) {  // in campo: la radio resta accesa
    gVersoBattaglia = false;
    return;
  }
  spegniRadio();
}

bool linkRadioAccesa() { return gRadio; }

// Fine della lotta: l'esito per ui_battle.cpp e le ricompense, una volta sola alla
// conferma di END. Si segue a ogni pacchetto, così un ESCI dell'altro che arriva subito
// dopo il suo END non la fa perdere.
static void seguiFine() {
  LbFase f = lb().fase();
  if (f == LBF_SQUADRA) {  // una lotta nuova (o la rivincita)
    gFineVinc = BATTLE_IN_CORSO;
    gEsitoFatto = false;
  }
  if (f != LBF_FINE) return;
  gFineVinc = lb().vincitore();  // vale l'host se i due END differiscono
  gFineMotivo = lb().motivo();
  if (!lb().fineConfermata() || gEsitoFatto) return;
  gEsitoFatto = true;
  uint8_t esito = gFineVinc == BATTLE_PAREGGIO ? BTL_PARI
                  : gFineVinc == lb().mioLato() ? BTL_VINTA
                  : gFineMotivo == LBM_RESA ? BTL_FUGA : BTL_PERSA;
  pet.linkBattleResult(esito, gAttivoInSquadra);
}

// Lo scambio a ogni giro: la trattativa si apre da sola per l'offerta dell'altro, per la
// ripresa di uno scambio sospeso (PREPARATO), per l'esito e per un annullamento dell'altro.
// Non durante la lotta né sopra la scelta del mio pet.
static void seguiScambio() {
  LsFase f = ls().fase();
  if (f == gScFase) return;
  gScFase = f;
  gScConferma = gScForza = false;
  gEvoDa = 0;
  if (gScreen != SCR_LINK || lb().fase() != LBF_LIBERO || gScegliRegola) return;
  LsMotivo m = ls().motivo();
  if (f == LSF_PREPARATO || f == LSF_FATTO ||
      (f == LSF_OFFERTA && gSc != SC_SCELTA) ||
      (f == LSF_LIBERO && (m == LSM_ANNULLATO || m == LSM_COPIA))) {
    gSc = SC_TRATTA;
    lastInteract = millis();
  }
}

// dal loop(), anche a schermo spento: i pacchetti arrivati, poi i tempi del protocollo
void linkLoop() {
  if (!gRadio) return;
  // la radio vive in SCR_LINK e nella lotta partita da lì: una cerimonia o un ripristino
  // che portano altrove chiudono il collegamento
  if (gScreen != SCR_LINK && gScreen != SCR_BATTLE) {
    spegniRadio();
    return;
  }
  uint8_t mac[6], buf[HAL_RADIO_MAX];
  int n;
  uint32_t ora = millis();
  while ((n = halRadioRicevi(mac, buf, sizeof(buf))) >= 0) lk().ricevi(mac, buf, (size_t)n, ora);
  lk().tick(ora);
  if (lk().stato() == LINK_PRONTO) {
    if (!gLbAvviata) {
      lb().avvia(lk(), (uint32_t)random(1, 0x7FFFFFFF));
      gLbAvviata = true;
    }
    if (!gLsAvviato) {  // con un giornale dello stesso partner rimanda COMMIT: si riprende
      ls().avvia(lk(), (uint32_t)random(1, 0x7FFFFFFF));
      gLsAvviato = true;
    }
    // i messaggi che lb() non riconosce sono dello scambio
    LinkMessaggio m;
    while (lk().prendi(m)) {
      if (!lb().ricevi(m)) ls().ricevi(m);
      seguiFine();
    }
    lb().tick();
    ls().tick();
    seguiFine();
  } else {
    if (gLbAvviata) {  // collegamento perso: la lotta si ferma senza dire niente
      lb().ferma();
      gLbAvviata = false;
    }
    if (gLsAvviato) {  // la trattativa si perde; uno scambio preparato resta SOSPESO
      ls().ferma();
      gLsAvviato = false;
      if (ls().fase() != LSF_FATTO) gSc = SC_NO;  // l'esito si guarda anche se l'altro è uscito
    }
  }
  seguiScambio();
  if (lb().fase() != LBF_SQUADRA) gCandFatti = false;
  // le due squadre ci sono (INIZIO): in campo
  if (gScreen == SCR_LINK && (lb().fase() == LBF_SCELTA || lb().fase() == LBF_CAMBIO)) {
    gVersoBattaglia = true;
    if (!startLinkBattle()) gVersoBattaglia = false;
  }
}

// comando LINK della console
void linkStato() {
  static const char *const NOMI[] = { "SPENTO", "CERCA", "AGGANCIO", "CONFERMA", "PRONTO",
                                      "INCOMPATIBILE", "PERSO" };
  const LinkPeer &p = lk().peer();
  Serial.printf("LINK radio=%d stato=%s motivo=%u mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
                gRadio ? 1 : 0, NOMI[lk().stato()], (unsigned)lk().motivo(), gMac[0], gMac[1],
                gMac[2], gMac[3], gMac[4], gMac[5]);
  if (lk().stato() >= LINK_AGGANCIO)
    Serial.printf("LINK peer=%02X:%02X:%02X:%02X:%02X:%02X fw=%s proto=%u hash=%08X id=%08X "
                  "specie=%u nome=%s codice=%04u conf=%d/%d\n",
                  p.mac[0], p.mac[1], p.mac[2], p.mac[3], p.mac[4], p.mac[5], p.fw,
                  (unsigned)p.proto, (unsigned)p.hash, (unsigned)p.trainerId, (unsigned)p.specie,
                  p.nome, (unsigned)lk().codice(), lk().confermatoIo() ? 1 : 0,
                  lk().confermatoLoro() ? 1 : 0);
  Serial.printf("LINK fw=%s hash=%08X id=%08X scartati=%u ritrasmessi=%u heap=%u\n",
                FW_VERSION, (unsigned)lk().hashMio(), (unsigned)pet.trainerId(),
                (unsigned)lk().scartati(), (unsigned)lk().ritrasmessi(), halFreeHeap());
  static const char *const FASI[] = { "LIBERO", "INVITO", "PROPOSTA", "SQUADRA", "SCELTA",
                                      "CAMBIO", "FINE" };
  Serial.printf("LINKB fase=%s host=%d regola=%u passo=%u motivo=%u desync=%u scartati=%u "
                "persi=%u vittorie=%u\n",
                FASI[lb().fase() % 7], lb().host() ? 1 : 0, (unsigned)lb().regola(),
                (unsigned)lb().passo(), (unsigned)lb().motivo(), (unsigned)lb().desync(),
                (unsigned)lb().scartati(), (unsigned)lb().persi(), (unsigned)pet.linkWins());
  static const char *const LSF[] = { "LIBERO", "OFFERTA", "PREPARATO", "SOSPESO", "FATTO" };
  Serial.printf("LINKS fase=%s motivo=%u mia=%d(%d) loro=%d acc=%d/%d id=%08X giornale=%d "
                "bloccato=%d scartati=%u persi=%u scambi=%u\n",
                LSF[ls().fase() % 5], (unsigned)ls().motivo(), ls().offertaMia() ? 1 : 0,
                (int)ls().postoMio(), ls().offertaLoro() ? 1 : 0, ls().accettatoIo() ? 1 : 0,
                ls().accettatoLoro() ? 1 : 0, (unsigned)ls().tradeId(), ls().giornale() ? 1 : 0,
                (int)pet.scambioBloccato(), (unsigned)ls().scartati(), (unsigned)ls().persi(),
                (unsigned)pet.linkTrades());
  if (ls().giornale())
    Serial.printf("LINKS giornale partner=%08X nome=%s posto=%d offerto=%u ricevuto=%u\n",
                  (unsigned)ls().giornaleDati().partner, ls().giornaleDati().nome,
                  (int)ls().giornaleDati().posto, (unsigned)ls().giornaleDati().offerto.specie,
                  (unsigned)ls().giornaleDati().ricevuto.specie);
}

// ---------- disegno ----------

// testo arrivato dalla radio: solo ASCII stampabile (il font e il centraggio vogliono
// un byte per carattere, e i byte di un altro firmware non sono affidabili)
static void ascii(char *out, const char *in, size_t n) {
  size_t i = 0;
  for (; i + 1 < n && in[i]; i++) out[i] = (in[i] >= 0x20 && in[i] < 0x7F) ? in[i] : '?';
  out[i] = 0;
}

// l'altro allenatore, per le righe della lotta
const char *linkNomeAltro() {
  static char nome[LINK_NOME_LEN + 1];
  ascii(nome, lk().peer().nome, sizeof(nome));
  if (!nome[0]) strcpy(nome, "?");
  return nome;
}

// centrato a dimensione 2, oppure 1 se non ci sta in `maxW`
static void testo(const char *s, int y, int maxW = 380) {
  uint8_t sz = textW(s, 2) <= maxW ? 2 : 1;
  setSize(sz);
  setCur(centerX(s, sz), sz == 2 ? y : y + 4);
  printT(s);
}

static void pulsante(int x, uint16_t col, uint16_t ink, const char *txt, int y = LK_BTN_Y,
                     int w = LK_BTN_W, int h = LK_BTN_H) {
  gfx->fillRoundRect(x, y, w, h, 12, col);
  gfx->setTextColor(ink);
  uint8_t sz = textW(txt, 2) <= w - 12 ? 2 : 1;
  setSize(sz);
  setCur(x + (w - textW(txt, sz)) / 2, y + h / 2 - (sz == 2 ? 7 : 4));
  printT(txt);
}

// zona di tocco di un pulsante: il disegno più un margine, perché il dito cade di solito
// 10-20 px sotto il punto mirato (registro TOCCHI); di lato solo metà dello spazio tra
// due pulsanti affiancati, così non si rubano i tocchi
#define LK_TOCCO_SU 12
#define LK_TOCCO_GIU 20
#define LK_TOCCO_LATO 3
static bool suPulsante(int16_t x, int16_t y, int bx, int by = LK_BTN_Y, int bw = LK_BTN_W,
                       int bh = LK_BTN_H) {
  return x >= bx - LK_TOCCO_LATO && x < bx + bw + LK_TOCCO_LATO && y >= by - LK_TOCCO_SU &&
         y < by + bh + LK_TOCCO_GIU;
}

// caselle, carte e i due pulsanti della regola, vicini da tutti i lati: metà dello spazio
// che li separa (`m`) tutt'intorno
static bool suCella(int16_t x, int16_t y, int bx, int by, int bw, int bh, int m = SQ_GAP / 2) {
  return x >= bx - m && x < bx + bw + m && y >= by - m && y < by + bh + m;
}

// la fascia in basso con «tocca: indietro»: l'unico posto, con lo scorrimento verso l'alto,
// da cui si chiude il collegamento (prima chiudeva ogni tocco fuori dai pulsanti)
#define LK_INDIETRO_Y 372
static bool suIndietro(int16_t y) { return y >= LK_INDIETRO_Y; }

// l'altro: miniatura (o niente se manca thumbs.bin), specie e nome dell'allenatore
static void disegnaPeer(int y) {
  const LinkPeer &p = lk().peer();
  const uint8_t *th = p.specie >= 1 && p.specie <= DEX_COUNT ? thumbs.get(p.specie) : nullptr;
  if (th) drawThumb(th, CX - GAL_CELL / 2, y, 2, false);
  char nome[LINK_NOME_LEN + 1], riga[64];
  ascii(nome, p.nome, sizeof(nome));
  const char *specie = p.specie >= 1 && p.specie <= DEX_COUNT ? dexName(p.specie) : T(S_EGG_HDR);
  snprintf(riga, sizeof(riga), "%s - %s", nome[0] ? nome : "?", specie);
  gfx->setTextColor(UI_INK);
  testo(riga, y + GAL_CELL + 2);
}

// onde che si allargano dal centro: si sta cercando
static void disegnaRicerca(uint32_t now) {
  for (int i = 0; i < 3; i++) {
    int r = (int)((now / 12 + i * 40) % 120);
    gfx->drawCircle(CX, 190, 20 + r, lerp565(UI_INK, UI_BG_DAY, r, 120));
    gfx->drawCircle(CX, 190, 21 + r, lerp565(UI_INK, UI_BG_DAY, r, 120));
  }
  gfx->fillCircle(CX, 190, 14, UI_INK);
  gfx->setTextColor(UI_INK);
  char riga[64];
  snprintf(riga, sizeof(riga), "%s%.*s", T(S_LINK_SEARCH), (int)(now / 400 % 4), "...");
  testo(riga, 328);
  gfx->setTextColor(UI_HINT);
  testo(T(S_LINK_NEAR), 356);
}

// ---------- LOTTA ----------

static uint8_t contaCandidati() { return (pet.canBattle() ? 1 : 0) + pet.colNumero(); }

// i candidati della squadra, ripresi a ogni scelta (anche per la rivincita)
static void preparaCandidati() {
  gNCand = 0;
  gNScelti = 0;
  gCandAttivo = !pet.isEgg();  // anche se ora non lotta: resta, grigio e con il motivo
  gCandSpento = gCandAttivo && !pet.canBattle();
  if (gCandAttivo) gCand[gNCand++] = pet.ritratto();
  for (uint8_t i = 0; i < pet.colNumero() && gNCand < 1 + COL_POSTI; i++)
    gCand[gNCand++] = pet.colRitratto(i);
  gCandFatti = true;
}

static const char *nomeRegola(uint8_t r) { return T(r == LB_TUTTI_AL_50 ? S_LKB_L50 : S_LKB_FREE); }

// casella i della squadra: 3 per riga, l'ultima riga centrata
static void cellaSquadra(uint8_t i, int &x, int &y) {
  uint8_t riga = i / 3, col = i % 3;
  uint8_t n = gNCand - riga * 3 < 3 ? gNCand - riga * 3 : 3;
  x = CX - (n * SQ_CW + (n - 1) * SQ_GAP) / 2 + col * (SQ_CW + SQ_GAP);
  y = SQ_Y0 + riga * (SQ_CH + SQ_GAP);
}

static int8_t posScelto(uint8_t i) {
  for (uint8_t k = 0; k < gNScelti; k++)
    if (gScelti[k] == i) return (int8_t)k;
  return -1;
}

// la casella i dei candidati (squadra o scambio): miniatura, nome e livello. `k` >= 0 è
// scelto (bordo verde e numero d'ordine), `spento` non si può scegliere (fondo grigio);
// `motivo` (se c'è) prende il posto del livello
static void cella(uint8_t i, int8_t k, bool spento = false, StrId motivo = STR_COUNT) {
  const PetRitratto &r = gCand[i];
  int x, y;
  cellaSquadra(i, x, y);
  gfx->fillRoundRect(x, y, SQ_CW, SQ_CH, 12, spento ? UI_TRACK : UI_WHITE);
  uint16_t bordo = k >= 0 ? UI_BAR_OK : DEX_TBL[r.specie].accent;
  gfx->drawRoundRect(x, y, SQ_CW, SQ_CH, 12, bordo);
  if (k >= 0) gfx->drawRoundRect(x + 1, y + 1, SQ_CW - 2, SQ_CH - 2, 11, bordo);
  if (const uint8_t *t = thumbs.get(r.specie)) drawThumb(t, x + (SQ_CW - GAL_CELL) / 2, y, 2, false);
  if (k >= 0) {
    char num[2] = { (char)('1' + k), 0 };
    gfx->fillCircle(x + 16, y + 16, 11, UI_BAR_OK);
    gfx->setTextColor(UI_WHITE);
    setSize(2);
    setCur(x + 11, y + 9);
    printT(num);
  }
  if (r.shiny) {
    gfx->setTextColor(UI_BAR_WARN);
    setSize(2);
    setCur(x + SQ_CW - 18, y + 6);
    printT("*");
  }
  const char *nm = r.nick[0] ? r.nick : dexName(r.specie);
  uint8_t sz = textW(nm, 2) <= SQ_CW - 2 ? 2 : 1;
  gfx->setTextColor(UI_INK);
  setSize(sz);
  setCur(x + (SQ_CW - textW(nm, sz)) / 2, y + 80 + (sz == 2 ? 0 : 4));
  printT(nm);
  char lv[12];
  snprintf(lv, sizeof(lv), T(S_LVL_FMT), levelFromAge(r.ageMinutes));
  const char *sotto = motivo != STR_COUNT ? T(motivo) : lv;
  sz = textW(sotto, 2) <= SQ_CW - 4 ? 2 : 1;
  gfx->setTextColor(motivo != STR_COUNT ? UI_BAR_BAD : UI_HINT);
  setSize(sz);
  setCur(x + (SQ_CW - textW(sotto, sz)) / 2, y + 100 + (sz == 2 ? 0 : 4));
  printT(sotto);
}

// perché il pet attivo è grigio nella squadra
static StrId motivoLotta() {
  switch (pet.lottaPerche()) {
    case LOT_DORME: return S_LKB_WHY_SLEEP;
    case LOT_STANCO: return S_LKB_WHY_TIRED;
    default: return S_LKB_WHY_BUSY;
  }
}

// la scelta della squadra: i candidati con miniatura, nome e livello; i scelti hanno il
// bordo verde e il loro numero d'ordine (il primo va in campo per primo). Il pet attivo che
// ora non lotta è grigio, con il motivo al posto del livello
static void renderSquadra() {
  if (!gCandFatti) preparaCandidati();
  gfx->setTextColor(UI_INK);
  testo(T(S_LKB_TEAM), 40, 270);
  gfx->setTextColor(UI_HINT);
  setSize(1);
  setCur(centerX(nomeRegola(lb().regola()), 1), 64);
  printT(nomeRegola(lb().regola()));
  for (uint8_t i = 0; i < gNCand; i++) {
    bool spento = gCandSpento && i == 0;
    cella(i, posScelto(i), spento, spento ? motivoLotta() : STR_COUNT);
  }
  pulsante(LK_SX, gNScelti ? UI_BAR_OK : UI_TRACK, gNScelti ? UI_WHITE : UI_HINT, T(S_LKB_OK), SQ_BTN_Y);
  pulsante(LK_DX, UI_TRACK, UI_INK, T(S_LKB_CANCEL), SQ_BTN_Y);
}

// un'attesa dell'altro, con ANNULLA
static void renderAttesa() {
  disegnaPeer(96);
  char riga[64];
  snprintf(riga, sizeof(riga), T(S_LKB_WAITING), linkNomeAltro());
  gfx->setTextColor(UI_HINT);
  testo(riga, 236);
  pulsante(LK_SOLO_X, UI_TRACK, UI_INK, T(S_LKB_CANCEL));
}

// collegati: LOTTA / SCAMBIA, la regola, la proposta, la squadra o un'attesa
static void renderLotta() {
  LbFase f = gLbAvviata ? lb().fase() : LBF_LIBERO;
  char riga[64];
  if (f == LBF_LIBERO && gScegliRegola) {
    gfx->setTextColor(UI_INK);
    testo(T(S_LKB_RULE), 126);
    pulsante(REG_X, C565(0xf4, 0xa8, 0x98), UI_INK, T(S_LKB_FREE), REG_Y1, REG_W, REG_H);
    pulsante(REG_X, C565(0xa8, 0xc8, 0xf0), UI_INK, T(S_LKB_L50), REG_Y2, REG_W, REG_H);
    pulsante(LK_SOLO_X, UI_TRACK, UI_INK, T(S_LKB_CANCEL));
  } else if (f == LBF_LIBERO) {
    disegnaPeer(96);
    uint16_t col = UI_INK;
    if (lb().host() && lb().invitato()) {
      snprintf(riga, sizeof(riga), T(S_LKB_WANTS), linkNomeAltro());
      col = UI_BAR_OK;
    } else {
      LbMotivo m = lb().motivo();
      snprintf(riga, sizeof(riga), "%s", T(m == LBM_RIFIUTO ? S_LKB_REFUSED
                                          : m == LBM_USCITA ? S_LINK_LEFT
                                          : m == LBM_ERRORE ? S_LKB_ERROR : S_LINK_READY));
    }
    gfx->setTextColor(col);
    testo(riga, 222);
    if (ls().giornale()) {  // uno scambio sospeso con un altro partner: si vede da SCAMBIA
      char nome[LINK_NOME_LEN + 1];
      ascii(nome, ls().giornaleDati().nome, sizeof(nome));
      snprintf(riga, sizeof(riga), T(S_LKS_SUSPENDED), nome[0] ? nome : "?");
      gfx->setTextColor(UI_BAR_BAD);
    } else {
      snprintf(riga, sizeof(riga), T(S_LKB_WINS), pet.linkWins());
      gfx->setTextColor(UI_HINT);
    }
    testo(riga, 254);
    bool puo = contaCandidati() > 0;
    pulsante(LK_SX, puo ? C565(0xf4, 0xa8, 0x98) : UI_TRACK, puo ? UI_INK : UI_HINT, T(S_BTL_FIGHT));
    pulsante(LK_DX, gLsAvviato ? C565(0xa8, 0xc8, 0xf0) : UI_TRACK, gLsAvviato ? UI_INK : UI_HINT,
             T(S_LINK_TRADE));
  } else if (f == LBF_PROPOSTA && !lb().host()) {
    disegnaPeer(96);
    snprintf(riga, sizeof(riga), T(S_LKB_PROPOSES), linkNomeAltro());
    gfx->setTextColor(UI_HINT);
    testo(riga, 204);
    gfx->setTextColor(UI_INK);
    testo(nomeRegola(lb().regola()), 230);
    bool puo = contaCandidati() > 0;
    gfx->setTextColor(puo ? UI_INK : UI_BAR_BAD);
    testo(T(puo ? S_LKB_ACCEPT_Q : S_LKB_NONE), 258);
    pulsante(LK_SX, puo ? UI_BAR_OK : UI_TRACK, puo ? UI_WHITE : UI_HINT, T(S_YES));
    pulsante(LK_DX, UI_TRACK, UI_INK, T(S_NO));
  } else {  // INVITO, la proposta dell'host, la squadra mandata, l'attimo prima del campo
    renderAttesa();
  }
}

// ---------- SCAMBIA ----------

// perché un pet non si offre, per la riga sopra la scelta
static StrId perche(ScambioMotivo m) { return m == SCB_UOVO ? S_LKS_EGG : S_COL_WHY_BUSY; }

// Pet::scambioPerche() per la scelta. La mia offerta in corso blocca il pet offerto (e con
// lui ogni altro scambio), ma cambiarla si può: offri() sblocca prima di controllare, e
// qui si fa lo stesso per un attimo.
static ScambioMotivo puoOffrire(int8_t posto) {
  ScambioMotivo m = pet.scambioPerche(posto);
  if (m != SCB_BLOCCATO || !ls().offertaMia() || ls().giornale()) return m;
  int8_t b = pet.scambioBloccato();
  pet.scambioSblocca();
  m = pet.scambioPerche(posto);
  pet.scambioBlocca(b);
  return m;
}

// i pet che si possono offrire, nelle caselle della squadra: l'attivo (se non è un uovo)
// e la collezione, ognuno con il suo posto
static void apriScelta() {
  gNCand = 0;
  if (!pet.isEgg()) {
    gScPosto[gNCand] = SCAMBIO_ATTIVO;
    gCand[gNCand++] = pet.ritratto();
  }
  for (uint8_t i = 0; i < pet.colNumero() && gNCand < 1 + COL_POSTI; i++) {
    gScPosto[gNCand] = (int8_t)i;
    gCand[gNCand++] = pet.colRitratto(i);
  }
  gCandFatti = false;  // gCand non è più la squadra
  gScMsg = gNCand ? (pet.isEgg() ? S_LKS_EGG : STR_COUNT) : S_LKS_NONE;
  gScConferma = false;
  gSc = SC_SCELTA;
}

// lo scambio occupa lo schermo: collegati e senza lotta, oppure l'esito (FATTO), che resta
// anche se l'altro chiude il collegamento
static bool scambioAschermo() {
  if (!gRadio || gSc == SC_NO) return false;
  if (gSc == SC_TRATTA && ls().fase() == LSF_FATTO) return true;
  return lk().stato() == LINK_PRONTO && lb().fase() == LBF_LIBERO && !gScegliRegola;
}

// la scelta del pet da offrire: quelli che non si possono offrire sono grigi (il tocco dice
// perché), quello già offerto ha il bordo verde
static void renderScelta() {
  gfx->setTextColor(UI_INK);
  testo(T(S_LKS_OFFER_Q), 40, 270);
  if (gScMsg != STR_COUNT) {
    gfx->setTextColor(UI_BAR_BAD);
    setSize(1);
    setCur(centerX(T(gScMsg), 1), 64);
    printT(T(gScMsg));
  }
  for (uint8_t i = 0; i < gNCand; i++) {
    bool offerto = ls().offertaMia() && ls().postoMio() == gScPosto[i];
    cella(i, offerto ? 0 : -1, !offerto && puoOffrire(gScPosto[i]) != SCB_OK);
  }
  pulsante(LK_SOLO_X, UI_TRACK, UI_INK, T(S_LKB_CANCEL), SQ_BTN_Y);
}

// il nome a schermo di un ritratto: il soprannome (solo ASCII stampabile: può arrivare
// dalla radio) o il nome di `specie` (0 = quella del ritratto)
static const char *nomeDi(const PetRitratto &r, char *buf, size_t n, int16_t specie = 0) {
  if (!r.nick[0]) return dexName(specie ? specie : r.specie);
  ascii(buf, r.nick, n);
  return buf;
}

// una carta della trattativa: chi offre, miniatura, nome, livello (con * se shiny) e
// allenatore originale, che con otId 0 è chi offre (`suo`). Senza ritratto un «?»; `ok`
// (bordo verde) = ha accettato.
static void carta(int x, const char *chi, const PetRitratto *r, const char *suo, bool ok) {
  gfx->fillRoundRect(x, SC_Y, SC_CW, SC_CH, 12, UI_WHITE);
  uint16_t bordo = ok ? UI_BAR_OK : r ? DEX_TBL[r->specie].accent : UI_TRACK;
  gfx->drawRoundRect(x, SC_Y, SC_CW, SC_CH, 12, bordo);
  if (ok) gfx->drawRoundRect(x + 1, SC_Y + 1, SC_CW - 2, SC_CH - 2, 11, bordo);
  gfx->setTextColor(UI_HINT);
  uint8_t sz = textW(chi, 2) <= SC_CW - 12 ? 2 : 1;
  setSize(sz);
  setCur(x + (SC_CW - textW(chi, sz)) / 2, SC_Y + 8);
  printT(chi);
  if (!r) {
    gfx->setTextColor(UI_TRACK);
    setSize(6);
    setCur(x + (SC_CW - textW("?", 6)) / 2, SC_Y + 72);
    printT("?");
    return;
  }
  if (const uint8_t *t = thumbs.get(r->specie)) drawThumb(t, x + (SC_CW - GAL_CELL) / 2, SC_Y + 26, 2, false);
  char buf[RIT_NOME + 1], lv[12], riga[48];
  const char *nm = nomeDi(*r, buf, sizeof(buf));
  sz = textW(nm, 2) <= SC_CW - 8 ? 2 : 1;
  gfx->setTextColor(UI_INK);
  setSize(sz);
  setCur(x + (SC_CW - textW(nm, sz)) / 2, SC_Y + 112 + (sz == 2 ? 0 : 4));
  printT(nm);
  snprintf(lv, sizeof(lv), T(S_LVL_FMT), levelFromAge(r->ageMinutes));
  snprintf(riga, sizeof(riga), "%s%s", r->shiny ? "* " : "", lv);
  gfx->setTextColor(r->shiny ? UI_BAR_WARN : UI_HINT);
  setSize(2);
  setCur(x + (SC_CW - textW(riga, 2)) / 2, SC_Y + 134);
  printT(riga);
  if (r->otId) ascii(buf, r->otNome, sizeof(buf));
  snprintf(riga, sizeof(riga), "%s: %s", T(S_COL_OT), r->otId ? (buf[0] ? buf : "?") : suo);
  sz = textW(riga, 2) <= SC_CW - 8 ? 2 : 1;
  gfx->setTextColor(UI_HINT);
  setSize(sz);
  setCur(x + (SC_CW - textW(riga, sz)) / 2, SC_Y + 158 + (sz == 2 ? 0 : 4));
  printT(riga);
}

// Uno scambio sospeso: con chi (riga y1) e, sotto (y2), `sotto`; i pulsanti RIPROVA o OK
// (`sx`) e ANNULLA SCAMBIO. Prima dell'annullamento manuale l'avviso e SI (a sinistra,
// lontano da ANNULLA SCAMBIO) / NO.
static void disegnaSospeso(int y1, int y2, StrId sx, StrId sotto) {
  char nome[LINK_NOME_LEN + 1], riga[64];
  if (gScForza) {
    gfx->setTextColor(UI_INK);
    testo(T(S_LKS_FORCE_Q), y1);
    gfx->setTextColor(UI_BAR_BAD);
    testo(T(S_LKS_FORCE_WARN), y2);
    pulsante(LK_SX, UI_BAR_BAD, UI_WHITE, T(S_YES));
    pulsante(LK_DX, UI_TRACK, UI_INK, T(S_NO));
    return;
  }
  ascii(nome, ls().giornaleDati().nome, sizeof(nome));
  snprintf(riga, sizeof(riga), T(S_LKS_SUSPENDED), nome[0] ? nome : "?");
  gfx->setTextColor(UI_INK);
  testo(riga, y1);
  gfx->setTextColor(ls().motivo() == LSM_ERRORE ? UI_BAR_BAD : UI_HINT);
  testo(T(ls().motivo() == LSM_ERRORE ? S_LKS_ERROR : sotto), y2);
  pulsante(LK_SX, UI_BAR_WARN, UI_INK, T(sx));
  pulsante(LK_DX, UI_TRACK, UI_INK, T(S_LKS_FORCE));
}

// l'evoluzione di un pet della collezione: come drawEvolveFX, con le miniature (raggi, le
// due sagome che si alternano sempre più in fretta, il lampo finale)
static void evoMiniature(int16_t da, int16_t a, float t, uint32_t now) {
  const int cx = CX, cy = 158;
  for (int i = 0; i < 12; i++) {
    float ang = now * 0.004f + i * (float)(PI / 6);
    int len = 70 + (int)(40 * (0.5f + 0.5f * sinf(now * 0.012f + i)));
    gfx->drawLine(cx, cy, cx + (int)(cosf(ang) * len), cy + (int)(sinf(ang) * len), DEX_TBL[a].accent);
  }
  int periodo = 60 + (int)(220 * (1.0f - t));
  bool vecchia = t < 0.9f && (now / periodo) % 2 == 0;
  if (const uint8_t *th = thumbs.get(vecchia ? da : a)) drawThumb(th, cx - GAL_CELL / 2, cy - GAL_CELL / 2, 3, true);
  if (t > 0.9f) gfx->fillCircle(cx, cy, (int)(120 * (t - 0.9f) / 0.1f), UI_WHITE);
}

// FATTO si chiude quando i pacchetti dello scambio sono partiti (il DONE: senza, l'altro
// resterebbe sospeso fino al prossimo collegamento) e l'evoluzione in collezione è finita
static bool fattoChiudibile() {
  bool partiti = lk().stato() != LINK_PRONTO || (!lk().inUscita() && !ls().inUscita());
  return partiti && !(pet.evoScambioDa() && pet.evoScambioPosto() != SCAMBIO_ATTIVO);
}

// l'esito: il pet ricevuto, grande, ed eventualmente la sua evoluzione
static void renderFatto(uint32_t now) {
  const PetRitratto &r = ls().ricevuto();
  int16_t da = pet.evoScambioDa();
  bool inCol = da && pet.evoScambioPosto() != SCAMBIO_ATTIVO;
  if (inCol && !gEvoDa) {
    gEvoDa = now ? now : 1;
    halSfx(SFX_EVOLVE);
  }
  float t = inCol ? (float)(now - gEvoDa) / (float)EVOLVE_ANIM_MS : 1.0f;
  if (inCol && t >= 1.0f) {  // finita: la collezione ha già la specie nuova
    pet.avviaEvoScambio();
    inCol = false;
  }
  if (inCol) {
    evoMiniature(da, r.specie, t, now);
  } else if (const uint8_t *th = thumbs.get(pet.evoScambioDa() ? pet.evoScambioDa() : r.specie)) {
    drawThumb(th, CX - GAL_CELL / 2, 158 - GAL_CELL / 2, 3, false);
  }
  char buf[RIT_NOME + 1], riga[64];
  // prima dell'evoluzione il nome è quello della specie di prima
  const char *nm = nomeDi(r, buf, sizeof(buf), pet.evoScambioDa());
  snprintf(riga, sizeof(riga), T(S_LKS_GOT), nm);
  gfx->setTextColor(UI_INK);
  testo(riga, SC_RIGA - 28);
  if (pet.evoScambioDa()) {  // in collezione si vede qui, il pet attivo nella principale
    snprintf(riga, sizeof(riga), T(S_LKS_EVOLVES), nm);
    gfx->setTextColor(UI_BAR_OK);
    testo(riga, SC_RIGA);
  }
  bool ok = fattoChiudibile();
  pulsante(LK_SOLO_X, ok ? UI_BAR_OK : UI_TRACK, ok ? UI_WHITE : UI_HINT, T(S_LKB_OK));
}

// la trattativa e le sue fasi, a tutto schermo
static void renderTrattativa(uint32_t now) {
  uint8_t ts = textW(T(S_LINK_TRADE), 3) <= 230 ? 3 : 2;
  gfx->setTextColor(UI_INK);
  setSize(ts);
  setCur(centerX(T(S_LINK_TRADE), ts), 36);
  printT(T(S_LINK_TRADE));
  LsFase f = ls().fase();
  char riga[96];
  if (f == LSF_FATTO) {
    renderFatto(now);
    return;
  }
  if (ls().giornale()) {  // PREPARATO o SOSPESO: i due pet del giornale
    const LsGiornale &g = ls().giornaleDati();
    char nome[LINK_NOME_LEN + 1];
    ascii(nome, g.nome, sizeof(nome));
    carta(SC_SX, T(S_LINK_YOU), &g.offerto, T(S_COL_ME), true);
    carta(SC_DX, nome[0] ? nome : "?", &g.ricevuto, nome, true);
    if (f == LSF_PREPARATO) {
      gfx->setTextColor(UI_INK);
      testo(T(S_LKS_BUSY), SC_RIGA);
    } else {  // con un altro partner: si riprende solo con quello giusto
      disegnaSospeso(SC_RIGA, 356, S_LKB_OK, S_LKS_RECONNECT);
    }
    return;
  }
  if (f == LSF_LIBERO) {  // l'altro ha annullato, o la copia non è riuscita
    disegnaPeer(96);
    gfx->setTextColor(UI_BAR_BAD);
    testo(T(ls().motivo() == LSM_COPIA ? S_LKS_COPY_FAIL : S_LKS_CANCELLED), 236);
    pulsante(LK_SOLO_X, UI_TRACK, UI_INK, T(S_LKB_OK));
    return;
  }
  // OFFERTA
  const bool mia = ls().offertaMia(), loro = ls().offertaLoro();
  carta(SC_SX, T(S_LINK_YOU), mia ? &ls().mia() : nullptr, T(S_COL_ME), ls().accettatoIo());
  carta(SC_DX, linkNomeAltro(), loro ? &ls().loro() : nullptr, linkNomeAltro(), ls().accettatoLoro());
  if (gScConferma) {
    char a[RIT_NOME + 1], b[RIT_NOME + 1];
    snprintf(riga, sizeof(riga), T(S_LKS_CONFIRM_Q), nomeDi(ls().mia(), a, sizeof(a)),
             nomeDi(ls().loro(), b, sizeof(b)));
    gfx->setTextColor(UI_INK);
    testo(riga, SC_RIGA);
    pulsante(LK_SX, UI_BAR_OK, UI_WHITE, T(S_YES));
    pulsante(LK_DX, UI_TRACK, UI_INK, T(S_NO));
    return;
  }
  uint16_t col = UI_HINT;
  if (!mia) {
    snprintf(riga, sizeof(riga), "%s", T(S_LKS_OFFER_Q));
    col = UI_INK;
  } else if (!loro || ls().accettatoIo()) {
    snprintf(riga, sizeof(riga), T(S_LKB_WAITING), linkNomeAltro());
  } else if (ls().accettatoLoro()) {
    snprintf(riga, sizeof(riga), T(S_LKS_THEY_OK), linkNomeAltro());
    col = UI_BAR_OK;
  } else {
    snprintf(riga, sizeof(riga), "%s", T(S_LKS_TAP_CHANGE));
  }
  gfx->setTextColor(col);
  testo(riga, SC_RIGA);
  bool puo = mia && loro && !ls().accettatoIo();
  pulsante(LK_SX, puo ? C565(0xa8, 0xc8, 0xf0) : UI_TRACK, puo ? UI_INK : UI_HINT, T(S_LINK_TRADE));
  pulsante(LK_DX, UI_TRACK, UI_INK, T(S_LKB_CANCEL));
}

void renderLink() {
  uint32_t now = millis();
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  // la scelta della squadra occupa tutto lo schermo
  if (gRadio && lk().stato() == LINK_PRONTO && gLbAvviata && lb().fase() == LBF_SQUADRA &&
      !lb().squadraMandata()) {
    renderSquadra();
    halFlush();
    return;
  }
  // anche lo scambio (non durante la lotta); l'esito anche a collegamento chiuso
  if (scambioAschermo()) {
    if (gSc == SC_SCELTA) renderScelta();
    else renderTrattativa(now);
    halFlush();
    return;
  }
  gfx->setTextColor(UI_INK);
  uint8_t ts = textW(T(S_LINK_TITLE), 3) <= 230 ? 3 : 2;
  setSize(ts);
  setCur(centerX(T(S_LINK_TITLE), ts), 36);
  printT(T(S_LINK_TITLE));
  // chi sono: il nome dell'allenatore
  char io[LINK_NOME_LEN + 1];
  pet.trainerName(io, sizeof(io));
  gfx->setTextColor(UI_HINT);
  testo(io, 70);

  LinkStato st = lk().stato();
  if (!gRadio) {
    gfx->setTextColor(UI_INK);
    testo(T(S_LINK_NO_RADIO), 220);
  } else if (st == LINK_CERCA || st == LINK_AGGANCIO) {
    disegnaRicerca(now);
  } else if (st == LINK_CONFERMA) {
    disegnaPeer(96);
    char cod[8];
    snprintf(cod, sizeof(cod), "%04u", (unsigned)lk().codice());
    gfx->setTextColor(UI_INK);
    setSize(6);
    setCur(centerX(cod, 6), 206);
    printT(cod);
    if (lk().confermatoIo()) {
      gfx->setTextColor(UI_HINT);
      testo(T(S_LINK_WAIT), 316);
    } else {
      testo(T(S_LINK_CODE_Q), 264);
      pulsante(LK_SX, UI_BAR_OK, UI_WHITE, T(S_YES));
      pulsante(LK_DX, UI_TRACK, UI_INK, T(S_NO));
    }
  } else if (st == LINK_PRONTO) {
    renderLotta();
  } else if (st == LINK_INCOMPATIBILE) {
    const LinkPeer &p = lk().peer();
    char fw[LINK_FW_LEN + 1], riga[64];
    ascii(fw, p.fw, sizeof(fw));
    gfx->setTextColor(UI_BAR_BAD);
    testo(T(S_LINK_UPDATE), 150);
    gfx->setTextColor(UI_INK);
    snprintf(riga, sizeof(riga), "%s: %s (%u)", T(S_LINK_YOU), lk().versioneMia(), LINK_PROTO);
    testo(riga, 196);
    snprintf(riga, sizeof(riga), "%s: %s (%u)", T(S_LINK_OTHER), fw[0] ? fw : "?", (unsigned)p.proto);
    testo(riga, 226);
    pulsante(LK_SOLO_X, UI_BAR_WARN, UI_INK, T(S_LINK_RETRY));
  } else {  // LINK_PERSO (o spento)
    LinkMotivo m = lk().motivo();
    gfx->setTextColor(UI_INK);
    testo(T(m == LINK_NESSUNO ? S_LINK_NOBODY : m == LINK_ADDIO ? S_LINK_LEFT : S_LINK_LOST), 200);
    if (gRadio && ls().giornale()) disegnaSospeso(234, 262, S_LINK_RETRY, S_LKS_RECONNECT);
    else pulsante(LK_SOLO_X, UI_BAR_WARN, UI_INK, T(S_LINK_RETRY));
  }

  // «tocca: indietro», la fascia che chiude (suIndietro): non durante la lotta
  if (!gScForza && !(st == LINK_PRONTO && gLbAvviata && (gScegliRegola || lb().fase() != LBF_LIBERO))) {
    int w = textW(T(S_BACK), 2) + 32;
    gfx->drawRoundRect(CX - w / 2, 388, w, 34, 12, UI_HINT);
    gfx->setTextColor(UI_HINT);
    setSize(2);
    setCur(centerX(T(S_BACK), 2), 398);
    printT(T(S_BACK));
  }
  halFlush();
}

// ---------- gesti ----------

static void squadraTap(int16_t x, int16_t y) {
  for (uint8_t i = 0; i < gNCand; i++) {
    int cx, cy;
    cellaSquadra(i, cx, cy);
    if (!suCella(x, y, cx, cy, SQ_CW, SQ_CH)) continue;
    if (gCandSpento && i == 0) {  // il motivo è scritto nella casella
      halSfx(SFX_DENY);
      return;
    }
    int8_t k = posScelto(i);
    if (k >= 0) {  // tolto: i seguenti salgono di un posto
      for (uint8_t j = (uint8_t)k; j + 1 < gNScelti; j++) gScelti[j] = gScelti[j + 1];
      gNScelti--;
    } else if (gNScelti < BATTLE_SQUADRA) {
      gScelti[gNScelti++] = i;
    } else {
      halSfx(SFX_DENY);
      return;
    }
    halSfx(SFX_TAP);
    return;
  }
  if (suPulsante(x, y, LK_SX, SQ_BTN_Y)) {
    PetRitratto sq[BATTLE_SQUADRA];
    bool attivo = false;
    for (uint8_t k = 0; k < gNScelti; k++) {
      sq[k] = gCand[gScelti[k]];
      attivo |= gCandAttivo && gScelti[k] == 0;
    }
    if (gNScelti && lb().squadra(sq, gNScelti)) {
      gAttivoInSquadra = attivo;  // solo lui può prendere il legame
      halSfx(SFX_TAP);
    } else {
      halSfx(SFX_DENY);
    }
  } else if (suPulsante(x, y, LK_DX, SQ_BTN_Y)) {
    halSfx(SFX_TAP);
    lb().esci();
  }
}

// collegati: i pulsanti della fase di LinkBattaglia
static void lottaTap(int16_t x, int16_t y) {
  LbFase f = gLbAvviata ? lb().fase() : LBF_LIBERO;
  if (f == LBF_LIBERO && gScegliRegola) {
    const int m = (REG_Y2 - REG_Y1 - REG_H) / 2;
    if (suCella(x, y, REG_X, REG_Y1, REG_W, REG_H, m) || suCella(x, y, REG_X, REG_Y2, REG_W, REG_H, m)) {
      uint8_t r = suCella(x, y, REG_X, REG_Y1, REG_W, REG_H, m) ? LB_LIVELLO_LIBERO : LB_TUTTI_AL_50;
      halSfx(lb().proponi(r) ? SFX_TAP : SFX_DENY);
      gScegliRegola = false;
    } else if (suPulsante(x, y, LK_SOLO_X)) {
      halSfx(SFX_TAP);
      gScegliRegola = false;
    }
    return;
  }
  if (f == LBF_LIBERO) {
    if (suPulsante(x, y, LK_SX)) {  // LOTTA: l'host sceglie la regola, l'ospite invita
      if (!gLbAvviata || !contaCandidati()) {
        halSfx(SFX_DENY);
        return;
      }
      halSfx(SFX_TAP);
      if (lb().host()) gScegliRegola = true;
      else lb().invita();
    } else if (suPulsante(x, y, LK_DX)) {  // SCAMBIA: la scelta, o la trattativa in corso
      if (!gLsAvviato) {
        halSfx(SFX_DENY);
        return;
      }
      halSfx(SFX_TAP);
      if (ls().fase() == LSF_LIBERO || ls().fase() == LSF_FATTO) apriScelta();
      else gSc = SC_TRATTA;
    } else if (suIndietro(y)) {
      goScreen(SCR_CARD);
    }
    return;
  }
  if (f == LBF_PROPOSTA && !lb().host()) {
    if (suPulsante(x, y, LK_SX)) {
      bool ok = contaCandidati() > 0 && lb().rispondi(true);
      halSfx(ok ? SFX_TAP : SFX_DENY);
    } else if (suPulsante(x, y, LK_DX)) {
      halSfx(SFX_TAP);
      lb().rispondi(false);
    }
    return;
  }
  if (f == LBF_SQUADRA && !lb().squadraMandata()) {
    squadraTap(x, y);
    return;
  }
  if (suPulsante(x, y, LK_SOLO_X)) {  // ANNULLA dell'attesa
    halSfx(SFX_TAP);
    lb().esci();
  }
}

// la scelta del pet da offrire: un pet grigio dice perché no; ANNULLA torna alla trattativa
// se ce n'è una
static void sceltaTap(int16_t x, int16_t y) {
  for (uint8_t i = 0; i < gNCand; i++) {
    int cx, cy;
    cellaSquadra(i, cx, cy);
    if (!suCella(x, y, cx, cy, SQ_CW, SQ_CH)) continue;
    int8_t posto = gScPosto[i];
    if (ls().offertaMia() && ls().postoMio() == posto) {  // già offerto: si torna a guardare
      halSfx(SFX_TAP);
      gSc = SC_TRATTA;
      return;
    }
    ScambioMotivo m = puoOffrire(posto);
    if (m == SCB_OK && ls().offri(posto)) {
      halSfx(SFX_TAP);
      gSc = SC_TRATTA;
    } else {
      halSfx(SFX_DENY);
      gScMsg = perche(m == SCB_OK ? SCB_OCCUPATO : m);
    }
    return;
  }
  if (suPulsante(x, y, LK_SOLO_X, SQ_BTN_Y)) {
    halSfx(SFX_TAP);
    gSc = ls().fase() == LSF_OFFERTA ? SC_TRATTA : SC_NO;
  }
}

// i pulsanti di disegnaSospeso: a sinistra RIPROVA (senza collegamento, `riprova`) o OK,
// a destra ANNULLA SCAMBIO; nell'avviso SI / NO
static void sospesoTap(int16_t x, int16_t y, bool riprova) {
  if (gScForza) {
    if (suPulsante(x, y, LK_SX)) {
      halSfx(SFX_TAP);
      gScForza = false;
      ls().annullaGiornale();  // ultima risorsa: il pet offerto resta qui
    } else if (suPulsante(x, y, LK_DX)) {
      halSfx(SFX_TAP);
      gScForza = false;
    }
  } else if (suPulsante(x, y, LK_SX)) {
    halSfx(SFX_TAP);
    if (riprova) avviaLink();
    else gSc = SC_NO;
  } else if (suPulsante(x, y, LK_DX)) {
    halSfx(SFX_TAP);
    gScForza = true;
  }
}

// la trattativa: tocco sulla mia carta = cambia offerta, SCAMBIA = conferma e accetta,
// ANNULLA = annulla per tutti e due. Durante PREPARATO non si tocca niente.
static void trattativaTap(int16_t x, int16_t y) {
  LsFase f = ls().fase();
  if (f == LSF_SOSPESO) {
    sospesoTap(x, y, false);
  } else if (f == LSF_FATTO) {
    if (!suPulsante(x, y, LK_SOLO_X)) return;
    if (!fattoChiudibile()) {
      halSfx(SFX_DENY);
      return;
    }
    halSfx(SFX_TAP);
    // l'evoluzione del pet attivo è la cerimonia della principale (e chiude il collegamento)
    if (pet.evoScambioDa() && pet.evoScambioPosto() == SCAMBIO_ATTIVO) goScreen(SCR_MAIN);
    else gSc = SC_NO;
  } else if (f == LSF_LIBERO) {
    if (suPulsante(x, y, LK_SOLO_X)) {
      halSfx(SFX_TAP);
      gSc = SC_NO;
    }
  } else if (f == LSF_OFFERTA && gScConferma) {
    if (suPulsante(x, y, LK_SX)) {
      halSfx(ls().accetta() ? SFX_TAP : SFX_DENY);
      gScConferma = false;
    } else if (suPulsante(x, y, LK_DX)) {
      halSfx(SFX_TAP);
      gScConferma = false;
    }
  } else if (f == LSF_OFFERTA) {
    if (suCella(x, y, SC_SX, SC_Y, SC_CW, SC_CH)) {
      halSfx(SFX_TAP);
      apriScelta();
    } else if (suPulsante(x, y, LK_SX)) {
      bool puo = ls().offertaMia() && ls().offertaLoro() && !ls().accettatoIo();
      halSfx(puo ? SFX_TAP : SFX_DENY);
      gScConferma = puo;  // la seconda conferma, prima di accetta()
    } else if (suPulsante(x, y, LK_DX)) {
      halSfx(SFX_TAP);
      ls().annulla();
      gSc = SC_NO;
    }
  }
}

// SCR_LINK: i pulsanti dello stato; si chiude solo dalla fascia «indietro» in basso (non
// durante la lotta: lì serve ANNULLA), un tocco altrove non fa niente
void linkTap(int16_t x, int16_t y) {
  LinkStato st = lk().stato();
  if (gLogTocchi)
    Serial.printf("LINKTAP st=%d radio=%d lb=%d avviata=%d regola=%d host=%d sc=%d\n", (int)st,
                  (int)gRadio, gLbAvviata ? (int)lb().fase() : -1, (int)gLbAvviata,
                  (int)gScegliRegola, (int)lb().host(), (int)gSc);
  if (gRadio && st == LINK_CONFERMA && !lk().confermatoIo() &&
      (suPulsante(x, y, LK_SX) || suPulsante(x, y, LK_DX))) {
    if (suPulsante(x, y, LK_SX)) {
      halSfx(SFX_TAP);
      lk().conferma();
    } else {
      goScreen(SCR_CARD);  // codice diverso: si chiude (BYE all'altro)
    }
  } else if (scambioAschermo()) {
    if (gSc == SC_SCELTA) sceltaTap(x, y);
    else trattativaTap(x, y);
  } else if (gRadio && st == LINK_PRONTO) {
    lottaTap(x, y);
  } else if (gRadio && st == LINK_PERSO && ls().giornale() &&
             (gScForza || suPulsante(x, y, LK_SX) || suPulsante(x, y, LK_DX))) {
    sospesoTap(x, y, true);
  } else if (gRadio && (st == LINK_PERSO || st == LINK_INCOMPATIBILE) &&
             suPulsante(x, y, LK_SOLO_X)) {
    halSfx(SFX_TAP);
    avviaLink();  // RIPROVA: la radio è ancora accesa
  } else if (suIndietro(y)) {
    goScreen(SCR_CARD);
  }
}

// SCR_LINK, scorrimento verticale: in alto chiude
void linkSwipeV(int dir) {
  // non mentre lo scambio si chiude (si riprenderebbe solo al prossimo collegamento)
  if (dir < 0 && ls().fase() != LSF_PREPARATO) goScreen(SCR_CARD);
}
