// «PokeGotchi finto» per le prove del collegamento.
//
// Con una sola Waveshare, l'altro capo del collegamento è una seconda scheda ESP32 qualsiasi
// senza schermo né PSRAM: qui una ESP32-2432S028 («Cheap Yellow Display»). Fa girare lo
// stesso protocollo del gioco (link.cpp) sullo stesso trasporto (linknow.cpp), si abbina da
// sola e risponde da copione; la si guida dalla seriale. Lo schermo della CYD non si usa.
// Lotta (linkbattaglia.cpp, lo stesso del gioco) e scambia (linkscambio.cpp e
// pet.cpp del gioco, con un pet e una collezione veri nella NVS della CYD).
//
// COMPILARE E CARICARE (dalla radice del repository; il -I serve a trovare link.cpp e
// linknow.cpp, che restano quelli del gioco)
//   FQBN=esp32:esp32:esp32
//   arduino-cli compile --fqbn $FQBN --build-property "compiler.cpp.extra_flags=-I$PWD" tools/peer_link
//   arduino-cli upload -p /dev/ttyUSB0 --fqbn $FQBN tools/peer_link
//   arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
// Per tornare alla prova radio si ricarica tools/prova_radio.
//
// COMPORTAMENTO
//   All'avvio cerca (HELLO in broadcast sul canale LINK_CANALE). Al primo PokeGotchi
//   compatibile si aggancia, stampa codice e specie dell'altro e, con AUTO acceso, conferma
//   da sé dopo AUTO_MS. Ogni cambio di stato si stampa. Da PERSO o INCOMPATIBILE torna a
//   cercare dopo RIPARTI_MS, salvo con FERMO.
//
// COMANDI (seriale, 115200, una riga ciascuno; maiuscole o minuscole)
//   STATO           stato, peer, codice, contatori, RAM
//   CONF            conferma il codice (con AUTO spento)
//   AUTO ON|OFF     conferma da sé (predefinito ON)
//   SPEC <n>        specie annunciata (predefinita 25)       NOME <testo>  nome (CYD)
//   FW <testo>      versione annunciata (predefinita 2.0, come FW_VERSION)
//   ALTRO           simula un firmware incompatibile (FW 9.99 e hash diverso); NORMALE torna
//   CHIUDI          BYE all'altro e silenzio; RIPARTI ricomincia a cercare
//   MUTO            smette di rispondere (l'altro va in PERSO per silenzio); RIPARTI torna
//   FERMO           dopo PERSO/INCOMPATIBILE non riparte da solo; RIPARTI lo toglie
//
// BATTAGLIA (con LOTTA ON, il predefinito, risponde da sé)
//   Da host, all'invito propone la regola REGOLA (predefinita 1 = tutti al 50); da ospite
//   accetta ogni proposta. Manda la squadra SQUADRA, sceglie a caso una mossa con PP e ogni
//   tanto (1 volta su CAMBI) un cambio; dopo un KO il primo sostituto vivo a caso. Prima di
//   ogni scelta aspetta PENSA ms (pause lunghe: PENSA 60000). Alla fine stampa l'esito e, con
//   RIVINCITA ON, chiede la rivincita dopo 3 s. Il caso è esp_random(): mai l'RNG della
//   battaglia, che deve restare uguale sulle due schede.
//   LOTTA ON|OFF    risponde in battaglia (OFF: i pacchetti di battaglia si stampano e basta)
//   INVITA          da ospite: invita l'host (la Waveshare propone la regola)
//   REGOLA 0|1      da host: 0 livello libero, 1 tutti al 50
//   SQUADRA a [b [c]] [Lnn]  specie della squadra (1..3) e livello (predefinita 25 4 7 L30)
//   PENSA <ms>      attesa prima di ogni scelta (predefinita 1500)
//   CAMBI <n>       un cambio volontario ogni n scelte circa (0 = mai; predefinito 6)
//   RIVINCITA ON|OFF  chiede la rivincita a fine battaglia (predefinito ON)
//   ARRENDITI       resa; ESCI esce dalla lotta (l'altro legge «L'altro è uscito»)
//   STATO stampa anche la riga LOTTA: fase, regola, passo, desync (deve restare 0),
//   scartati, persi e il conto vinte/perse/pari.
//
// SCAMBIO (con SCAMBIO ON, il predefinito, risponde da sé)
//   La CYD ha un Pet vero (pet.cpp) nella sua NVS: allenatore (trainerId fisso, nome «CYD»),
//   pet attivo e collezione. Al primo avvio la collezione riceve HAUNTER Lv.40 («HAUNT»),
//   che evolve in GENGAR alla ricezione. All'offerta dell'altro offre il posto OFFRI (0 =
//   il primo della collezione) dopo PENSA ms, e accetta appena vede le due offerte. Il
//   giornale dello scambio sopravvive al riavvio: spegnere la CYD a metà e riaccenderla
//   riprende lo scambio con lo stesso partner.
//   SCAMBIO ON|OFF  risponde da sé (OFF: si guida con OFFRI e ACCETTA)
//   OFFRI A|n       il posto da offrire: A = il pet attivo, n = collezione; collegati, offre
//   ACCETTA         accetta le due offerte          RIFIUTA  annulla la trattativa
//   FORZA           annullamento manuale di uno scambio sospeso (ultima risorsa)
//   PET             pet attivo e collezione         DEPOSITA dex [Lnn] [S] [nome]  in collezione
//   STATO stampa anche la riga SCAMBIO: fase, motivo, offerte, accettazioni, giornale.
//
// Derivato da DylanPDao/TamaPoke (MIT): avviso completo in link.h.
#include "link.h"
#include "linknow.h"
#include "linkbattaglia.h"
#include "linkscambio.h"
#include "pet.h"
#include <esp_heap_caps.h>

#define AUTO_MS 1500      // conferma automatica: il tempo di leggere il codice sull'altra
#define RIPARTI_MS 3000

static Link lk;
static LinkConfig cfg;
static uint8_t mac[6];
static bool autoConf = true, muto = false, fermo = false;
static LinkStato statoPrima = LINK_SPENTO;
static uint32_t daStato = 0;
static char fw[LINK_FW_LEN + 1] = "2.1";

// battaglia
static LinkBattaglia lb;
static bool lbAvviata = false, lotta = true, rivAuto = true;
static uint8_t regola = LB_TUTTI_AL_50;
static int16_t sqSpecie[BATTLE_SQUADRA] = { 25, 4, 7 };
static uint8_t sqN = 3, sqLivello = 30;
static uint32_t pensaMs = 1500, cambiOgni = 6;
static LbFase fasePrima = LBF_LIBERO;
static uint32_t daFase = 0, pensaDa = 0;
static uint16_t vinte = 0, perse = 0, pari = 0;
static const char *const FASI[] = { "LIBERO", "INVITO", "PROPOSTA", "SQUADRA", "SCELTA",
                                    "CAMBIO", "FINE" };
static const char *const LB_MOTIVI[] = { "-", "RIFIUTO", "KO", "RESA", "USCITA", "ERRORE" };

// scambio
static Pet pet;
static LinkScambio ls;
static bool lsAvviato = false, scambioAuto = true;
static int8_t postoOfferta = 0;
static LsFase faseSPrima = LSF_LIBERO;
static uint32_t daFaseS = 0;
static const char *const LS_FASI[] = { "LIBERO", "OFFERTA", "PREPARATO", "SOSPESO", "FATTO" };
static const char *const LS_MOTIVI[] = { "-", "ANNULLATO", "RITIRATO", "COPIA", "ERRORE", "MANUALE" };

// il pet.cpp del gioco suona gli effetti dall'HAL: qui non c'è audio
void halSfx(uint8_t) {}

static const char *const NOMI[] = { "SPENTO", "CERCA", "AGGANCIO", "CONFERMA", "PRONTO",
                                    "INCOMPATIBILE", "PERSO" };
static const char *const MOTIVI[] = { "-", "NESSUNO", "SILENZIO", "ADDIO" };

static void inviaRadio(void *, const uint8_t m[6], const uint8_t *dati, uint8_t len) {
  if (!muto) linknowSend(m, dati, len);
}

static void avvia() {
  cfg.fw = fw;
  cfg.seme = esp_random() | 1;
  LinkTrasporto tr;
  tr.invia = inviaRadio;
  muto = false;
  lk.avvia(cfg, tr, millis());
  Serial.printf("cerco: fw=%s hash=%08X specie=%u nome=%s\n", fw,
                (unsigned)(cfg.hash ? cfg.hash : linkHashDati()), cfg.specie, cfg.nome);
}

static void stampaMac(const uint8_t *m) {
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static void stato() {
  const LinkPeer &p = lk.peer();
  Serial.printf("stato=%s motivo=%s io=", NOMI[lk.stato()], MOTIVI[lk.motivo()]);
  stampaMac(mac);
  if (lk.stato() >= LINK_AGGANCIO) {
    Serial.print(" peer=");
    stampaMac(p.mac);
    Serial.printf(" fw=%s proto=%u specie=%u nome=%s id=%08X codice=%04u conf=%d/%d",
                  p.fw, (unsigned)p.proto, p.specie, p.nome, (unsigned)p.trainerId,
                  (unsigned)lk.codice(), lk.confermatoIo() ? 1 : 0, lk.confermatoLoro() ? 1 : 0);
  }
  Serial.printf(" scartati=%u ritrasmessi=%u coda_persi=%u txko=%u int=%u\n",
                (unsigned)lk.scartati(), (unsigned)lk.ritrasmessi(), (unsigned)linknowPersi(),
                (unsigned)linknowTxKo(), heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  Serial.printf("LOTTA %s fase=%s host=%d regola=%u passo=%u motivo=%s desync=%u scartati=%u "
                "persi=%u vinte=%u perse=%u pari=%u pensa=%u\n",
                lotta ? "ON" : "OFF", FASI[lb.fase() % 7], lb.host() ? 1 : 0,
                (unsigned)lb.regola(), (unsigned)lb.passo(), LB_MOTIVI[lb.motivo() % 6],
                (unsigned)lb.desync(), (unsigned)lb.scartati(), (unsigned)lb.persi(),
                vinte, perse, pari, (unsigned)pensaMs);
  Serial.printf("SCAMBIO %s fase=%s motivo=%s offro=%d mia=%d loro=%d acc=%d/%d id=%08X "
                "giornale=%d scartati=%u persi=%u\n",
                scambioAuto ? "ON" : "OFF", LS_FASI[ls.fase() % 5], LS_MOTIVI[ls.motivo() % 6],
                (int)postoOfferta, ls.offertaMia() ? 1 : 0, ls.offertaLoro() ? 1 : 0,
                ls.accettatoIo() ? 1 : 0, ls.accettatoLoro() ? 1 : 0, (unsigned)ls.tradeId(),
                ls.giornale() ? 1 : 0, (unsigned)ls.scartati(), (unsigned)ls.persi());
}

static void stampaRitratto(const char *chi, const PetRitratto &r) {
  Serial.printf("%s: #%d %s Lv.%u%s nick=%s ao=%08X %s\n", chi, r.specie,
                r.specie >= 1 && r.specie <= DEX_COUNT ? DEX_TBL[r.specie].name : "?",
                levelFromAge(r.ageMinutes), r.shiny ? " shiny" : "", r.nick,
                (unsigned)r.otId, r.otNome);
}

// PET: il pet attivo e la collezione
static void stampaPet() {
  if (pet.isEgg()) Serial.println("attivo: uovo");
  else stampaRitratto("attivo", pet.ritratto());
  for (uint8_t i = 0; i < pet.colNumero(); i++) {
    char chi[8];
    snprintf(chi, sizeof(chi), "col %u", i);
    stampaRitratto(chi, pet.colRitratto(i));
  }
  Serial.printf("collezione %u/%u, scambi=%u, bloccato=%d\n", pet.colNumero(), COL_POSTI,
                pet.linkTrades(), (int)pet.scambioBloccato());
}

static bool deposita(int16_t dex, uint8_t lv, bool shiny, const char *nick) {
  PetRitratto r;
  r.specie = dex;
  r.shiny = shiny;
  r.ageMinutes = (uint32_t)(lv - 1) * MINUTES_PER_LEVEL;
  strncpy(r.nick, nick, RIT_NOME - 1);
  return pet.colDeposita(r);
}

static void offri() {
  ScambioMotivo m = pet.scambioPerche(postoOfferta);
  bool ok = ls.offri(postoOfferta);
  Serial.printf("offro il posto %d -> %s (perche=%u)\n", (int)postoOfferta, ok ? "ok" : "ERR",
                (unsigned)m);
}

// risponde nello scambio: offre all'offerta dell'altro, accetta le due offerte
static void pilotaScambio(uint32_t ora) {
  LsFase f = ls.fase();
  if (f != faseSPrima) {
    faseSPrima = f;
    daFaseS = ora;
    Serial.printf("scambio: fase=%s motivo=%s\n", LS_FASI[f % 5], LS_MOTIVI[ls.motivo() % 6]);
    if (f == LSF_FATTO) {
      stampaRitratto("ricevuto", ls.ricevuto());
      if (pet.evoScambioDa()) {
        Serial.printf("evoluzione per scambio da #%d (posto %d)\n", pet.evoScambioDa(),
                      (int)pet.evoScambioPosto());
        pet.avviaEvoScambio();
      }
      Serial.printf("scambi=%u\n", pet.linkTrades());
    }
  }
  static bool loroPrima = false;
  if (ls.offertaLoro() && !loroPrima) stampaRitratto("offerta dell'altro", ls.loro());
  loroPrima = ls.offertaLoro();
  if (!scambioAuto || f != LSF_OFFERTA || ora - daFaseS < pensaMs) return;
  if (ls.offertaLoro() && !ls.offertaMia()) offri();
  else if (ls.offertaMia() && ls.offertaLoro() && !ls.accettatoIo())
    Serial.printf("accetto -> %s\n", ls.accetta() ? "ok" : "ERR");
}

// la squadra di SQUADRA: ritratti di specie e livello dati, geni medi, senza allenamento
static void mandaSquadra() {
  PetRitratto r[BATTLE_SQUADRA];
  for (uint8_t i = 0; i < sqN; i++) {
    r[i].specie = sqSpecie[i];
    r[i].ageMinutes = (uint32_t)(sqLivello - 1) * MINUTES_PER_LEVEL;
  }
  Serial.printf("squadra: %d %d %d L%u -> %s\n", sqSpecie[0], sqN > 1 ? sqSpecie[1] : 0,
                sqN > 2 ? sqSpecie[2] : 0, sqLivello, lb.squadra(r, sqN) ? "ok" : "ERR");
}

// una scelta a caso: dopo un KO un sostituto vivo; nel turno una mossa con PP o, ogni
// tanto, un cambio. Mai l'RNG della battaglia (bt.rng): deve restare quello dell'host.
static void scegliACaso() {
  const Battle &b = lb.battaglia();
  uint8_t io = lb.mioLato(), validi[BATTLE_SQUADRA], n = 0;
  for (uint8_t i = 0; i < b.membri[io]; i++)
    if (battlePuoCambiare(b, io, i)) validi[n++] = i;
  if (lb.fase() == LBF_CAMBIO || (n && cambiOgni && esp_random() % cambiOgni == 0)) {
    uint8_t i = n ? validi[esp_random() % n] : 0;
    Serial.printf("passo %u: cambio con %u -> %s\n", (unsigned)lb.passo(), i,
                  lb.scegli(BATTLE_CAMBIA + i) ? "ok" : "ERR");
    return;
  }
  const Combatant &c = b.lato[io];
  uint8_t slot = 0, m = 0;
  if (battleDeveScegliere(c) && !battleDeveScontro(c)) {
    for (uint8_t i = 0; i < BATTLE_MOSSE; i++)
      if (c.mosse[i] && c.pp[i] && esp_random() % ++m == 0) slot = i;
  }
  Serial.printf("passo %u: mossa %u -> %s\n", (unsigned)lb.passo(), slot,
                lb.scegli(slot) ? "ok" : "ERR");
}

// risponde in battaglia, con le pause di PENSA
static void pilotaLotta(uint32_t ora) {
  LbFase f = lb.fase();
  if (f != fasePrima) {
    fasePrima = f;
    daFase = ora;
    pensaDa = 0;
    Serial.printf("lotta: fase=%s motivo=%s\n", FASI[f % 7], LB_MOTIVI[lb.motivo() % 6]);
    if (f == LBF_FINE) {
      uint8_t v = lb.vincitore();
      if (v == BATTLE_PAREGGIO) pari++;
      else if (v == lb.mioLato()) vinte++;
      else perse++;
      Serial.printf("FINE: %s (%s) passi=%u desync=%u vinte=%u perse=%u pari=%u\n",
                    v == BATTLE_PAREGGIO ? "pareggio" : v == lb.mioLato() ? "vinta" : "persa",
                    LB_MOTIVI[lb.motivo() % 6], (unsigned)lb.passo(), (unsigned)lb.desync(),
                    vinte, perse, pari);
    }
  }
  if (!lotta) return;
  switch (f) {
    case LBF_LIBERO:
      if (lb.host() && lb.invitato()) {
        Serial.printf("invitato: propongo la regola %u\n", regola);
        lb.proponi(regola);
      }
      break;
    case LBF_PROPOSTA:
      if (!lb.host() && ora - daFase >= pensaMs) {
        Serial.printf("proposta: regola %u, accetto\n", (unsigned)lb.regola());
        lb.rispondi(true);
      }
      break;
    case LBF_SQUADRA:
      if (!lb.squadraMandata() && ora - daFase >= pensaMs) mandaSquadra();
      break;
    case LBF_SCELTA:
    case LBF_CAMBIO:
      if (!lb.devoScegliere()) {
        pensaDa = 0;
      } else if (!pensaDa) {
        pensaDa = ora | 1;
      } else if (ora - pensaDa >= pensaMs) {
        pensaDa = 0;
        scegliACaso();
      }
      break;
    case LBF_FINE:
      if (rivAuto && !lb.rivincitaMia() && ora - daFase >= 3000) {
        Serial.println("chiedo la rivincita");
        lb.rivincita();
      }
      break;
    default:
      break;
  }
}

static void comando(String riga) {
  riga.trim();
  String su = riga;
  su.toUpperCase();
  String arg = su.indexOf(' ') > 0 ? riga.substring(su.indexOf(' ') + 1) : "";
  if (su == "STATO" || su == "LINK") {
    stato();
  } else if (su == "CONF") {
    if (lk.stato() == LINK_CONFERMA) lk.conferma();
    else Serial.println("ERR non in CONFERMA");
  } else if (su == "AUTO ON" || su == "AUTO OFF") {
    autoConf = su == "AUTO ON";
  } else if (su.startsWith("SPEC ")) {
    cfg.specie = (uint16_t)arg.toInt();
    avvia();
  } else if (su.startsWith("NOME ")) {
    arg.toUpperCase();
    strncpy(cfg.nome, arg.c_str(), LINK_NOME_LEN);
    cfg.nome[LINK_NOME_LEN] = 0;
    avvia();
  } else if (su.startsWith("FW ")) {
    strncpy(fw, arg.c_str(), LINK_FW_LEN);
    fw[LINK_FW_LEN] = 0;
    avvia();
  } else if (su == "ALTRO") {
    strcpy(fw, "9.99");
    cfg.hash = linkHashDati() ^ 0x5A5A5A5A;
    avvia();
  } else if (su == "NORMALE") {
    strcpy(fw, "2.1");
    cfg.hash = 0;
    avvia();
  } else if (su == "CHIUDI") {
    lk.chiudi();
    fermo = true;
  } else if (su == "MUTO") {
    muto = true;
    Serial.println("muto: non rispondo piu' (RIPARTI per tornare)");
  } else if (su == "FERMO") {
    fermo = true;
  } else if (su == "RIPARTI") {
    fermo = false;
    avvia();
  } else if (su == "LOTTA ON" || su == "LOTTA OFF") {
    lotta = su == "LOTTA ON";
  } else if (su == "INVITA") {
    if (!lb.invita()) Serial.println("ERR (da ospite, collegati e senza lotta in corso)");
  } else if (su == "REGOLA 0" || su == "REGOLA 1") {
    regola = su == "REGOLA 1" ? LB_TUTTI_AL_50 : LB_LIVELLO_LIBERO;
  } else if (su.startsWith("SQUADRA ")) {
    uint8_t n = 0;
    int da = 8;
    while (da < (int)su.length() && n < BATTLE_SQUADRA + 1) {
      int fine = su.indexOf(' ', da);
      if (fine < 0) fine = su.length();
      String t = su.substring(da, fine);
      da = fine + 1;
      if (t.startsWith("L")) {
        int lv = t.substring(1).toInt();
        sqLivello = lv < 1 ? 1 : lv > 100 ? 100 : lv;
      } else if (t.length() && n < BATTLE_SQUADRA) {
        int d = t.toInt();
        if (d >= 1 && d <= DEX_COUNT) sqSpecie[n++] = d;
      }
    }
    if (n) sqN = n;
  } else if (su.startsWith("PENSA ")) {
    pensaMs = (uint32_t)arg.toInt();
  } else if (su.startsWith("CAMBI ")) {
    cambiOgni = (uint32_t)arg.toInt();
  } else if (su == "RIVINCITA ON" || su == "RIVINCITA OFF") {
    rivAuto = su == "RIVINCITA ON";
  } else if (su == "ARRENDITI") {
    if (!lb.arrenditi()) Serial.println("ERR (nessuna battaglia in corso)");
  } else if (su == "ESCI") {
    lb.esci();
  } else if (su == "SCAMBIO ON" || su == "SCAMBIO OFF") {
    scambioAuto = su == "SCAMBIO ON";
  } else if (su.startsWith("OFFRI ")) {
    postoOfferta = arg.startsWith("A") || arg.startsWith("a") ? SCAMBIO_ATTIVO : (int8_t)arg.toInt();
    if (lsAvviato) offri();
  } else if (su == "ACCETTA") {
    if (!ls.accetta()) Serial.println("ERR (servono le due offerte)");
  } else if (su == "RIFIUTA") {
    ls.annulla();
  } else if (su == "FORZA") {
    if (!ls.annullaGiornale()) Serial.println("ERR (nessuno scambio sospeso)");
  } else if (su == "PET") {
    stampaPet();
  } else if (su.startsWith("DEPOSITA ")) {
    int dex = 0, lv = 30;
    bool shiny = false;
    String nick = "";
    int da = 9;
    while (da < (int)riga.length()) {
      int fine = riga.indexOf(' ', da);
      if (fine < 0) fine = riga.length();
      String t = riga.substring(da, fine), tu = t;
      tu.toUpperCase();
      da = fine + 1;
      if (!t.length()) continue;
      if (!dex) dex = t.toInt();
      else if (tu.startsWith("L") && t.length() > 1 && isDigit(t[1])) lv = t.substring(1).toInt();
      else if (tu == "S") shiny = true;
      else nick = tu;
    }
    if (dex < 1 || dex > DEX_COUNT || lv < 1 || lv > 100 || !deposita(dex, lv, shiny, nick.c_str()))
      Serial.println("ERR (DEPOSITA dex [Lnn] [S] [nome]; collezione piena?)");
  } else {
    Serial.println("comandi: STATO CONF AUTO ON|OFF SPEC n NOME x FW x ALTRO NORMALE CHIUDI "
                   "MUTO FERMO RIPARTI LOTTA ON|OFF INVITA REGOLA 0|1 SQUADRA a b c Lnn "
                   "PENSA ms CAMBI n RIVINCITA ON|OFF ARRENDITI ESCI SCAMBIO ON|OFF OFFRI A|n "
                   "ACCETTA RIFIUTA FORZA PET DEPOSITA dex Lnn S nome");
    return;
  }
  Serial.println("ok");
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n== PokeGotchi finto (peer_link) ==");
  if (!linknowBegin(LINK_CANALE)) {
    Serial.println("ERR ESP-NOW non parte");
    return;
  }
  linknowMac(mac);
  memcpy(cfg.mac, mac, 6);
  // il pet e lo scambio prima del collegamento: un giornale rimasto blocca il pet offerto
  pet.begin();
  ls.carica(pet);
  if (!pet.trainerName(cfg.nome, sizeof(cfg.nome))) {
    pet.setTrainerName("CYD");
    strcpy(cfg.nome, "CYD");
  }
  if (pet.colNumero() == 0) deposita(93, 40, false, "HAUNT");
  cfg.trainerId = pet.trainerId();  // fisso: la ripresa di uno scambio riconosce il partner
  cfg.specie = 25;
  stampaPet();
  if (ls.giornale()) Serial.printf("scambio sospeso con %s (%08X)\n", ls.giornaleDati().nome,
                                   (unsigned)ls.giornaleDati().partner);
  Serial.print("MAC ");
  stampaMac(mac);
  Serial.printf(", canale %u, int=%u\n", LINK_CANALE, heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  avvia();
}

void loop() {
  static String riga;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (riga.length()) comando(riga);
      riga = "";
    } else if (riga.length() < 40) {
      riga += c;
    }
  }

  uint32_t ora = millis();
  uint8_t da[6], buf[LINK_FRAME_MAX];
  int n;
  while ((n = linknowRicevi(da, buf, sizeof(buf))) >= 0)
    if (!muto) lk.ricevi(da, buf, (size_t)n, ora);
  if (!muto) lk.tick(ora);

  // i messaggi dell'applicazione: la battaglia li prende (con LOTTA ON), poi lo scambio
  if (lk.stato() == LINK_PRONTO) {
    if (!lbAvviata) {
      lb.avvia(lk, esp_random() | 1);
      lbAvviata = true;
      fasePrima = LBF_LIBERO;
    }
    if (!lsAvviato) {  // con un giornale dello stesso partner rimanda COMMIT: si riprende
      ls.avvia(lk, esp_random() | 1);
      lsAvviato = true;
    }
    LinkMessaggio m;
    while (lk.prendi(m))
      if ((!lotta || !lb.ricevi(m)) && !ls.ricevi(m))
        Serial.printf("messaggio tipo=0x%02X len=%u\n", m.tipo, m.len);
    lb.tick();
    ls.tick();
    pilotaLotta(ora);
  } else {
    if (lbAvviata) {
      lb.ferma();
      lbAvviata = false;
    }
    if (lsAvviato) {
      ls.ferma();
      lsAvviato = false;
    }
  }
  pilotaScambio(ora);

  LinkStato st = lk.stato();
  if (st != statoPrima) {
    statoPrima = st;
    daStato = ora;
    stato();
  }
  if (st == LINK_CONFERMA && autoConf && !lk.confermatoIo() && ora - daStato >= AUTO_MS) {
    Serial.println("confermo il codice");
    lk.conferma();
  }
  if ((st == LINK_PERSO || st == LINK_INCOMPATIBILE) && !fermo && ora - daStato >= RIPARTI_MS)
    avvia();
}
