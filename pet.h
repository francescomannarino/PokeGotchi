#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "ritratto.h"

// 1 tick = 1 minuto di gioco. Abbassa questo valore per provare più in fretta (es.
// 5000UL = le statistiche calano 12 volte più veloci).
#define PET_TICK_MS 60000UL
// Minuti di gioco per livello. Con 60, CHARMANDER evolve dopo ~16 h di gioco con cura
// perfetta. Metti 1 per vedere le evoluzioni subito.
#define MINUTES_PER_LEVEL 60
#define EAT_ANIM_MS 2500UL
#define HEART_MS 1500UL
#define EVOLVE_ANIM_MS 5200UL              // animazione di evoluzione (più lunga = più epica)
#define CEREMONY_MS 10000UL                // durata dell'addio a schermo
#define FAREWELL_AGE_MIN (3UL * 24 * 60)   // dice addio dopo 3 giorni di gioco (in forma finale)
#define RUNAWAY_TICKS 60                   // fugge dopo 1 h con TUTTO a zero

// millisecondi che mancano a `deadline` (0 se è già passato, o se deadline==0: "timer
// inattivo"). Tutti i timer del gioco (dialoghi, animazioni) si salvano come istante
// assoluto "deadline = millis() + durata" e si confrontavano con millis() < deadline /
// millis() > deadline; questo si rompe quando millis() va a capo dopo ~49,7 giorni di
// uptime. La sottrazione senza segno, reinterpretata con segno, è sicura rispetto a
// quel giro finché l'intervallo reale è molto inferiore a ~24,8 giorni (qui il timer
// più lungo è ~12 s), senza cambiare come si salva la deadline.
static inline uint32_t timeLeft(uint32_t deadline) {
  if (!deadline) return 0;
  int32_t left = (int32_t)(deadline - millis());
  return left > 0 ? (uint32_t)left : 0;
}

// livello per l'età in minuti: uint16_t, non uint8_t. Con MINUTES_PER_LEVEL=60 il livello
// sono le ore, quindi un uint8_t torna a 0 al livello 256, dopo ~10,6 giorni. E ci si
// arriva giocando normalmente, perché "restare insieme" permette di rimandare l'addio
// all'infinito. Si ferma a 999 invece di andare in overflow.
static inline uint16_t levelFromAge(uint32_t ageMinutes) {
  uint32_t lv = 1 + ageMinutes / MINUTES_PER_LEVEL;
  return lv > 999 ? 999 : (uint16_t)lv;
}

// cerimonie di fine ciclo
enum : uint8_t { CER_NONE = 0, CER_FAREWELL, CER_RUNAWAY, CER_RELEASE };

enum PetMood : uint8_t { MOOD_HAPPY, MOOD_SAD, MOOD_EATING, MOOD_SLEEPING };

// medaglie dell'individuo (bitmask)
enum : uint16_t {
  MED_LV10 = 1 << 0, MED_LV25 = 1 << 1, MED_LV50 = 1 << 2,
  MED_BERRY = 1 << 3, MED_STREAK7 = 1 << 4, MED_BOND = 1 << 5,
  MED_FINAL = 1 << 6, MED_FIT = 1 << 7,
};
#define MED_COUNT 8

// Stato di gioco persistito in un'unica voce NVS, il blob PET_SAVE_KEY, scritto con una
// sola `putBytes` (la NVS dell'ESP-IDF scrive i blob in modo atomico): un'interruzione di
// corrente lascia lo stato vecchio o quello nuovo, mai un misto.
// Layout: la struct così com'è in memoria, impacchettata e little-endian (scheda, test e
// PWA). In testa `version` e `size` (byte totali, CRC compreso), in coda il CRC32 IEEE
// (`saveCrc32` di savefile.h) di tutti i byte che lo precedono.
// Versioni: i campi nuovi si aggiungono SOLO in coda, prima di `crc`, alzando
// PET_SAVE_VERSION. Chi legge copia il prefisso che conosce: da un blob più vecchio i campi
// mancanti restano ai default qui sotto, da uno più nuovo (downgrade) si ignora la coda.
// Una migrazione di significato va in `Pet::load()`, guardando `version`.
// Fuori dal blob, chiavi separate dello stesso namespace: "init", "seen" (lastSeenEpoch,
// letta da syncClock/savedEpoch e riscritta da saveImporta per congelare il tempo), le
// impostazioni (lingua, audio), lo stato delle copie sulla SD e la collezione (COL_KEY).
#define PET_SAVE_KEY "st"
// 2: vittorie premiate del giorno (battaglie contro la CPU). 3: collezione (domanda in
// sospeso, giorno dell'ultimo RIPRENDI, età minima del nuovo addio) e allenatore originale.
// 4: ultimo scambio applicato (tradeFatto), scritto insieme al pet ricevuto. 5: record dei
// giochi a inclinazione (grottaHi, surfHi)
#define PET_SAVE_VERSION 5
#define PET_SAVE_MAX 512  // blob più lunghi non sono nostri: si trattano come rovinati
struct __attribute__((packed)) PetSave {
  uint16_t version = PET_SAVE_VERSION;
  uint16_t size = sizeof(PetSave);
  uint8_t fullness = 80, joy = 80, energy = 80, hygiene = 100;
  uint8_t poops = 0, weight = 0;
  uint8_t geneAtk = 0, geneDef = 0, geneSpe = 0;  // 0 = precedente ai geni: li tira load()
  uint8_t trAtk = 0, trDef = 0, trSpe = 0;
  uint8_t berryKnown = 0, shiny = 0, eggShiny = 0, starterPick = 0;
  uint32_t ageMinutes = 0;
  int16_t speciesId = -1, eggTarget = 4;
  uint8_t eggTaps = 0, careMistakes = 0, sleeping = 0, lastEnd = CER_NONE;
  uint8_t dexReg[19] = { 0 };
  uint8_t dexShinyReg[19] = { 0 };
  uint16_t streak = 0, bestStreak = 0;
  uint32_t lastCareDay = 0;
  uint8_t bond = 0;
  uint16_t medals = 0, totalMedals = 0, lastMilestone = 0, gameHi = 0, strHi = 0;
  char nick[12] = "";
  uint8_t winsToday = 0;   // versione 2
  uint32_t winsDay = 0;
  uint8_t colAsk = 0;          // versione 3
  uint32_t colDay = 0;
  uint32_t farewellMinAge = 0;
  uint32_t otId = 0;
  char otName[12] = "";
  uint32_t tradeFatto = 0;     // versione 4
  uint16_t grottaHi = 0, surfHi = 0;  // versione 5
  uint32_t crc = 0;
};
// tools/salvataggi.py decodifica lo stesso layout (TestSalvataggi lo confronta con questo)
static_assert(sizeof(PetSave) == 143, "PetSave: aggiorna anche tools/salvataggi.py");

// battaglie contro la CPU: ui_battle.cpp le gioca, Pet ne tiene le regole
#define BATTLE_MIN_ENERGY 20     // sotto questa energia è troppo stanco per lottare
#define BATTLE_WINS_PER_DAY 5    // vittorie premiate al giorno: oltre si lotta senza premi
enum : uint8_t { BTL_VINTA = 0, BTL_PERSA, BTL_PARI, BTL_FUGA };  // esito per battleResult()

// battaglie in collegamento: il contatore delle vittorie è del giocatore (come la
// serie) e vive nella chiave LINK_WINS_KEY del namespace esportato, fuori dal blob; il pet
// attivo, se era in squadra, prende un po' di legame. Niente allenamento né costi: due
// schede vicine non devono potersi farmare a vicenda.
#define LINK_WINS_KEY "lkvit"
#define LINK_BOND_WIN 2   // legame per una vittoria
#define LINK_BOND_PLAY 1  // per una sconfitta o un pareggio (la resa non dà niente)
// scambi effettuati: anche questo del giocatore, nella chiave LINK_TRADES_KEY dello
// stesso namespace; lo conta riceviScambio() una volta per scambio
#define LINK_TRADES_KEY "lkscb"

// scossone: felicità tolta (meno nel sonno, che non si interrompe) e pausa prima
// della penalità successiva
#define SPAVENTO_GIOIA 10
#define SPAVENTO_GIOIA_SONNO 5
#define SPAVENTO_PAUSA_MS 60000UL
// interruttore MOVIMENTO: chiave del namespace esportato, fuori dal blob come
// "vol" e "mus" di audio.cpp
#define MOVIMENTO_KEY "mot"

// Collezione personale: fino a COL_POSTI ritratti (ritratto.h) congelati, nel
// blob NVS COL_KEY dello stesso namespace del pet (quindi esportato nei .pgsave).
// Layout: 1 byte di versione (COL_VERSIONE), 1 byte con il numero di ritratti, poi i
// ritratti uno dopo l'altro, ognuno con la sua lunghezza al byte 1. Senza la chiave la
// collezione è vuota (anche nei .pgsave che non la hanno); un ritratto rovinato si perde,
// gli altri restano.
#define COL_KEY "col"
#define COL_POSTI 5
#define COL_VERSIONE 1
// perché RIPRENDI non è possibile (colPerche), per la UI
enum ColMotivo : uint8_t {
  COL_OK = 0,
  COL_POSTO,       // posto vuoto o fuori intervallo
  COL_OCCUPATO,    // il pet attivo dorme, è in cerimonia (o nella domanda) o evolve
  COL_NON_FINALE,  // il pet attivo non è né un uovo né una forma finale
  COL_NON_IN_FORMA,  // una barra sotto COL_MIN_BARRA, o abbandono in corso
  COL_OGGI,        // RIPRENDI già usato oggi
};
#define COL_MIN_BARRA 30
// statistiche di combattimento di un ritratto (ATK, DEF, VEL, SPC), con la formula di
// atkStat() e compagne; tutte 0 se la specie non è valida. Per il dettaglio della collezione.
void statDaRitratto(const PetRitratto &r, uint16_t out[4]);

// Scambio (il protocollo è in linkscambio.h): si offre il pet attivo
// (SCAMBIO_ATTIVO) o un posto 0..COL_POSTI-1 della collezione, e il ricevuto prende il
// posto di quello offerto.
#define SCAMBIO_ATTIVO (-1)
#define SCAMBIO_NESSUNO (-2)
// perché un pet non si può offrire (scambioPerche), per la UI
enum ScambioMotivo : uint8_t {
  SCB_OK = 0,
  SCB_POSTO,      // posto della collezione vuoto o fuori intervallo
  SCB_UOVO,       // il pet attivo è un uovo
  SCB_OCCUPATO,   // cerimonia, domanda della collezione o evoluzione da scambio da mostrare;
                  // per il pet attivo anche sonno ed evoluzione in corso
  SCB_BLOCCATO,   // c'è già uno scambio in corso (scambioBlocca)
};
// perché il pet attivo ora non lotta (lottaPerche), per la squadra della lotta in collegamento
enum LottaMotivo : uint8_t {
  LOT_OK = 0,
  LOT_UOVO,
  LOT_DORME,
  LOT_STANCO,     // energia sotto BATTLE_MIN_ENERGY
  LOT_OCCUPATO,   // scelta dell'iniziale, cerimonia o evoluzione
};

class Pet {
public:
  // Statistiche 0..100
  uint8_t fullness = 80;  // cibo
  uint8_t joy = 80;       // felicità
  uint8_t energy = 80;    // energia
  uint8_t hygiene = 100;  // pulizia
  uint8_t poops = 0;      // cacche a schermo (max 3)
  uint8_t weight = 0;     // 0-100: i dolcetti ingrassano, il minigioco brucia
  // geni (90-110%, si tirano alla schiusa) e allenamento (0-100)
  uint8_t geneAtk = 100, geneDef = 100, geneSpe = 100;
  uint8_t trAtk = 0, trDef = 0, trSpe = 0;
  bool berryKnown = false;  // ha già scoperto la sua bacca preferita
  bool shiny = false;       // variante di colore rara (si sorteggia nell'uovo)
  uint32_t ageMinutes = 0;
  int16_t speciesId = -1;      // numero di Pokédex (1-151), -1 = uovo
  int16_t prevSpeciesId = -1;  // per l'animazione di evoluzione
  uint8_t careMistakes = 0;   // trascuratezze: ognuna ritarda l'evoluzione di 1 livello
  bool sleeping = false;
  uint32_t lastSeenEpoch = 0;   // ultima ora RTC vista (per la progressione offline)
  uint8_t ceremony = CER_NONE;  // addio/fuga/rilascio in corso
  uint8_t lastEnd = CER_NONE;   // come è finita la precedente (influisce sull'uovo)
  uint8_t dexReg[19] = { 0 };       // pokédex degli allevati (bitmap da 151 bit)
  uint8_t dexShinyReg[19] = { 0 };  // allevati in versione shiny
  // serie di cura giornaliera (del giocatore: persiste tra un allevamento e l'altro)
  uint16_t streak = 0, bestStreak = 0;
  uint32_t lastCareDay = 0;
  // legame (del Pokémon: sale lentamente con la cura, si azzera quando ne nasce un
  // altro)
  uint8_t bond = 0;
  char nick[12] = "";    // soprannome (vuoto = nome della specie)
  // medaglie: dell'individuo + contatore cumulativo tra tutti gli allevamenti
  uint16_t medals = 0, totalMedals = 0;
  uint16_t newMedal = 0;   // appena ottenute, da festeggiare
  uint16_t lastMilestone = 0;  // traguardo di serie già festeggiato
  uint16_t gameHi = 0;     // record del minigioco (del giocatore)
  uint16_t strHi = 0;      // record di colpi al sacco
  uint16_t grottaHi = 0;   // record della Grotta Diglett e del Surf (del giocatore)
  uint16_t surfHi = 0;
  uint8_t winsToday = 0;   // vittorie premiate nel giorno winsDay (today())
  uint32_t winsDay = 0;
  // allenatore originale del pet attivo (0 = io): lo porta il ritratto
  uint32_t otId = 0;
  char otName[12] = "";

  void begin();                 // carica lo stato da NVS (o crea il primo uovo)
  void update(uint32_t nowMs);  // chiamare a ogni loop()
  // riporta a valori sicuri uno stato letto da fuori (NVS corrotta, importazione): una
  // specie fuori intervallo diventa un uovo nuovo, le barre si fermano a 100
  void sanitize();

  // Azioni (pulsanti touch)
  void feed();              // bacca rossa (compatibilità)
  void feedBerry(uint8_t color);  // 0 rossa, 1 blu, 2 verde
  void feedCandy();
  bool lovesBerry(uint8_t color) const {
    return !isEgg() && (speciesId % 3) == color;  // gusto nascosto per specie
  }
  void playResult(uint8_t score);  // ricompensa del minigioco (allena VEL)
  uint8_t trainStrength(uint16_t hits);  // sacco da allenamento (allena FUE)
  // giochi a inclinazione: la Grotta allena la DIF e restituisce l'aumento, il Surf
  // non allena nulla; entrambi aggiornano il proprio record
  uint8_t grottaResult(uint16_t punti);
  void surfResult(uint16_t punti);
  // scossone sulla principale: -SPAVENTO_GIOIA di felicità, -SPAVENTO_GIOIA_SONNO
  // nel sonno (resta addormentato), poi SPAVENTO_PAUSA_MS senza penalità. Niente da uovo, in
  // cerimonia o in evoluzione. Restituisce la felicità tolta (0 anche nella pausa); il
  // salvataggio è differito, come quello del tick.
  uint8_t spavento();
  bool spaventoInPausa() const { return timeLeft(spaventoFino) > 0; }
  // MOVIMENTO: scossone e colpetti sulla principale; acceso se la chiave manca.
  // I giochi a inclinazione usano il sensore comunque.
  bool movimento() const { return movimentoOn; }
  void setMovimento(bool on);

  // statistiche di combattimento: base reale della gen 1 x geni + livello + allenamento
  uint16_t atkStat() const;
  uint16_t defStat() const;
  uint16_t speStat() const;
  // Speciale: non ha geni né allenamento suoi, prende la media di quelli di ATK e DEF
  // (come combattenteDaPet in battle.h)
  uint16_t spcStat() const;
  // battaglie contro la CPU: niente da uovo, nel sonno, in cerimonia o in evoluzione, né
  // con l'energia sotto BATTLE_MIN_ENERGY
  bool canBattle() const { return lottaPerche() == LOT_OK; }
  LottaMotivo lottaPerche() const;
  // livello in battaglia: quello del pet, fino a 100 come in Gen 1
  uint8_t battleLevel() const { return level() > 100 ? 100 : (uint8_t)level(); }
  // avversario casuale: una specie del bioma del pet (né leggendaria né di uno stadio
  // evolutivo che a quel livello non si è ancora raggiunto) e il suo livello in `lvl`
  int16_t pickWild(uint8_t &lvl) const;
  // fine battaglia: costi, e per le prime BATTLE_WINS_PER_DAY vittorie del giorno gioia,
  // legame e allenamento. Mai effetti sul ciclo di vita; salvataggio differito.
  // true se la vittoria è stata premiata.
  bool battleResult(uint8_t esito);
  uint8_t battleWinsLeft() const;  // vittorie premiate che restano oggi
  // fine di una battaglia in collegamento, alla conferma di END: esito come battleResult
  // (BTL_FUGA = ci siamo arresi). Conta la vittoria e dà il legame al pet attivo se era in
  // squadra. Salvataggio differito: con la radio accesa si salva all'uscita.
  void linkBattleResult(uint8_t esito, bool attivoInSquadra);
  uint16_t linkWins() const { return linkWinsN; }
  uint16_t linkTrades() const { return linkTradesN; }
  void play();
  void toggleLight();  // dormire / svegliarsi
  void clean();
  void caress();  // toccare il Pokémon
  void eggTap();  // toccare l'uovo: 3 tocchi e si schiude
  void newEgg();   // ricominciare da zero con un iniziale casuale
  void release();  // rilasciare (pressione lunga + conferma)
  void syncClock(uint32_t nowEpoch);  // applica il tempo trascorso da spento
  void setClock(uint32_t nowEpoch);   // imposta l'ora senza applicare progressione
  void startFarewell();  // usabile anche dalla console seriale (BYE)
  void startRunaway();   // usabile anche dalla console seriale (RUN)

  bool isEgg() const { return speciesId < 0; }
  uint8_t eggCracks() const { return eggTaps; }
  bool eating() const { return timeLeft(eatUntil) > 0; }
  bool showHeart() const { return timeLeft(heartUntil) > 0; }
  bool evolving() const { return timeLeft(evolveUntil) > 0; }
  float evolveT() const {     // progresso dell'animazione di evoluzione 0..1
    return 1.0f - (float)timeLeft(evolveUntil) / (float)EVOLVE_ANIM_MS;
  }
  bool canEvolveNow() const;  // condizioni di evoluzione soddisfatte (elenco)
  void evolve();              // avvia la trasformazione (la chiama un tocco dell'utente)
  // forma finale + FAREWELL_AGE_MIN (e, per un pet ripreso dalla collezione, altri
  // FAREWELL_AGE_MIN dal rientro): pronto a dire addio (pulsante)
  bool canFarewellNow() const;
  bool canRunawayNow() const;   // abbandono totale 1h: pronto a fuggire (pulsante triste)
  // l'utente decide in un dialogo; "mantieni/restare insieme" rimanda e ripropone più
  // tardi
  bool wantEvolveButton() const { return canEvolveNow() && level() > evoDeclinedLv; }
  bool wantFarewellButton() const { return canFarewellNow() && ageMinutes >= farDeclinedAge; }
  void declineEvolve() { evoDeclinedLv = level(); }              // ripropone al salire di livello
  void declineFarewell() { farDeclinedAge = ageMinutes + 1440; } // ripropone tra 1 giorno
  // prima partita: il giocatore sceglie l'iniziale (Bulbasaur/Charmander/Squirtle)
  bool awaitingStarter() const { return starterPick; }
  void chooseStarter(int16_t dex) { eggTarget = dex; starterPick = false; save(); }
  void factoryReset() { prefs.clear(); }  // cancella la NVS (test: comando seriale WIPE YES)
  void dbgRunawayReady() { fullness = joy = energy = hygiene = 0; neglectTicks = RUNAWAY_TICKS; }  // test
  // console BARRE: le quattro barre a mano (prove), senza incuria in corso
  void dbgBarre(uint8_t c, uint8_t g, uint8_t e, uint8_t i) {
    fullness = c > 100 ? 100 : c; joy = g > 100 ? 100 : g;
    energy = e > 100 ? 100 : e; hygiene = i > 100 ? 100 : i;
    neglectTicks = 0; pendingSave = true;
  }
  uint16_t level() const { return levelFromAge(ageMinutes); }
  bool isRegistered(int16_t dex) const {
    return dex >= 1 && dex <= 151 && (dexReg[(dex - 1) >> 3] & (1 << ((dex - 1) & 7)));
  }
  bool isShinyRegistered(int16_t dex) const {
    return dex >= 1 && dex <= 151 && (dexShinyReg[(dex - 1) >> 3] & (1 << ((dex - 1) & 7)));
  }
  uint16_t registeredCount() const;
  bool lineHasUnregistered(int16_t base) const;
  uint8_t eggRarity() const;       // rarità dell'uovo attuale (senza rivelare la specie)
  int16_t pickEggSpecies();        // pubblica per poter simulare i tiri (EGGS)
  uint8_t lowestStat() const { return min(min(fullness, joy), min(energy, hygiene)); }
  PetMood mood() const;
  // progresso della cerimonia di addio/fuga, 0..1 (per animarla)
  float ceremonyT() const {
    if (ceremony == CER_NONE) return 0.0f;
    return 1.0f - (float)timeLeft(ceremonyUntil) / (float)CEREMONY_MS;
  }

  // ---- collezione
  // ritratto del pet attivo (non da uovo)
  PetRitratto ritratto() const;
  // il ritratto diventa il pet attivo: tiene specie, shiny, età, geni, allenamento,
  // legame, medaglie, nome e allenatore originale; bisogni, timer, trascuratezze e
  // abbandono ripartono da valori neutri (come per un pet ricevuto con lo scambio).
  // Registra la specie nel Pokédex. Non salva: lo fa chi la chiama.
  void daRitratto(const PetRitratto &r);
  uint8_t colNumero() const { return colN; }
  bool colPiena() const { return colN >= COL_POSTI; }
  const PetRitratto &colRitratto(uint8_t i) const { return col[i < COL_POSTI ? i : 0]; }
  // aggiunge in fondo e salva; false se la collezione è piena
  bool colDeposita(const PetRitratto &r);
  // libera il posto i (i successivi scalano) e salva; false se il posto è vuoto
  bool colLibera(uint8_t i);
  // LIBERA è possibile: il posto c'è e nessun posto della collezione è offerto in uno scambio
  // (anche sospeso), che non deve scalare. Il motivo, se no, è COL_OCCUPATO di colPerche
  bool colLiberabile(uint8_t i) const { return i < colN && scambioPosto < 0; }
  // Domanda di fine ciclo: dopo un addio o il rilascio di una forma finale, a cerimonia
  // finita il pet resta (cerimonia ferma all'ultimo fotogramma, tempo fermo) finché non si
  // risponde. Il flag è persistente: dopo un riavvio la cerimonia riparte già finita.
  bool colChiede() const { return colAsk; }
  // conserva il pet nella collezione e fa nascere l'uovo. Con la collezione piena serve
  // `libera` (posto da liberare prima): false se manca o non è valido, e non cambia nulla
  bool colConserva(int8_t libera = -1);
  void colRinuncia();  // il pet non entra: nasce l'uovo
  // RIPRENDI il posto i: COL_OK se si può. Con un uovo attivo l'uovo si scarta, con una
  // forma finale attiva è uno scambio (l'attivo prende il posto del ripreso). Una volta
  // al giorno. Non vede le battaglie in corso: la UI non lo offre da lì.
  ColMotivo colPerche(uint8_t i) const;
  bool colRiprendi(uint8_t i);

  // ---- scambio
  ScambioMotivo scambioPerche(int8_t posto) const;
  PetRitratto scambioRitratto(int8_t posto) const;  // da chiamare con scambioPerche() == SCB_OK
  // Il pet offerto resta fermo finché lo scambio non si chiude: se è il pet attivo niente
  // evoluzione, addio, fuga né rilascio; in ogni caso niente RIPRENDI né LIBERA (gli indici
  // della collezione non devono cambiare). Solo in RAM: dopo un riavvio lo rimette
  // LinkScambio::carica() leggendo il giornale.
  void scambioBlocca(int8_t posto) { scambioPosto = posto; }
  void scambioSblocca() { scambioPosto = SCAMBIO_NESSUNO; }
  int8_t scambioBloccato() const { return scambioPosto; }
  // Il ritratto ricevuto come entra nel posto: legame a 0; allenatore originale `partner`
  // (trainerId e nome) se per chi lo manda era 0 = «io», e 0 se torna a noi; se la specie
  // ha tradeEvo, già evoluta. Deterministico, così la ricezione si può rifare dopo un riavvio.
  PetRitratto scambioDaRicevere(const PetRitratto &ricevuto, uint32_t partner, const char *nome);
  // Ricezione: il ricevuto (scambioDaRicevere) prende il posto di `offerto` (che deve essere ancora lì:
  // stessa specie, shiny, geni e allenatore originale se è il pet attivo, stessi byte se è
  // in collezione), con legame 0, bisogni e timer neutri e un nuovo addio solo dopo altri
  // FAREWELL_AGE_MIN; la specie (e quella prima dell'evoluzione) entra nel Pokédex, anche
  // shiny. Salva con tradeFatto = tradeId nello stesso blob del pet: è il segno, atomico,
  // che lo scambio è applicato. Idempotente: con lo stesso tradeId non rifà nulla, e una
  // collezione già scritta da una ricezione interrotta non si riscrive. false se il posto
  // non contiene più il pet offerto (o il ricevuto non è valido): non cambia nulla.
  bool riceviScambio(int8_t posto, const PetRitratto &offerto, const PetRitratto &ricevuto,
                     uint32_t tradeId, uint32_t partner, const char *nome);
  bool scambioApplicato(uint32_t tradeId) const { return tradeId && tradeFatto == tradeId; }
  // annullamento manuale di uno scambio non applicato: se una ricezione interrotta aveva
  // già scritto il ricevuto nella collezione, rimette `offerto` al suo posto
  void annullaScambio(int8_t posto, const PetRitratto &offerto, const PetRitratto &ricevuto,
                      uint32_t partner, const char *nome);
  // Evoluzione per scambio da mostrare: la specie di prima (0 = nessuna) e il posto
  // (SCAMBIO_ATTIVO o della collezione). I dati sono già evoluti e salvati; solo in RAM.
  int16_t evoScambioDa() const { return evoScDa; }
  int8_t evoScambioPosto() const { return evoScPosto; }
  // la cerimonia: per il pet attivo avvia l'animazione di evoluzione esistente (evolving(),
  // prevSpeciesId = specie di prima); in ogni caso abbassa il flag
  void avviaEvoScambio();

  // serie / legame / medaglie / nome
  void rename(const char *name);
  // identità dell'allenatore per il collegamento: chiavi "tid" e "tnome" del
  // namespace esportato, così entrano nei salvataggi e seguono il ripristino su un'altra
  // scheda. Il MAC serve solo al trasporto.
  uint32_t trainerId();                         // casuale a 32 bit, mai 0: creato al primo uso
  bool trainerName(char *out, size_t n);        // false (e "") se non è ancora stato scelto
  void setTrainerName(const char *name);
  bool hasMedal(uint16_t m) const { return medals & m; }
  bool showMedal() const { return timeLeft(medalUntil) > 0; }
  bool showMilestone() const { return timeLeft(milestoneUntil) > 0; }
  int careBonus() const;  // miglioramento dell'uovo per serie + legame

  // salvataggio periodico differito: tick() segna il pendente e il loop lo esegue
  // quando lo schermo è attenuato/spento (la scrittura in flash congela ~1s entrambi i
  // core: così non si vede né taglia il touch)
  bool savePending() const { return pendingSave; }
  // ultima ora reale persistita; serve a riseminare un RTC che ha perso l'ora
  uint32_t savedEpoch() { return prefs.getUInt("seen", 0); }
  void flushSave();

private:
  Preferences prefs;
  uint32_t lastTick = 0;
  uint32_t eatUntil = 0;
  uint32_t heartUntil = 0;
  uint32_t evolveUntil = 0;
  int16_t eggTarget = 1;       // dex nascosto che uscirà dall'uovo
  bool eggShiny = false;       // sorpresa sorteggiata alla creazione dell'uovo
  uint8_t eggTaps = 0;
  uint8_t mistakeCooldown = 0;
  uint8_t ticksSinceSave = 0;
  bool pendingSave = false;     // salvataggio periodico in sospeso da eseguire
  uint16_t evoDeclinedLv = 0;   // "mantieni forma": non offrire l'evoluzione finché non sale di livello
  uint32_t farDeclinedAge = 0;  // "restare insieme": non offrire l'addio fino a questa età
  bool starterPick = false;     // prima partita: in attesa che il giocatore scelga l'iniziale
  uint8_t neglectTicks = 0;
  uint16_t goodTicks = 0;  // serie di buona cura: forgia la DEF
  uint32_t ceremonyUntil = 0;
  uint8_t bondToday = 0;       // limite giornaliero di aumento del legame
  uint32_t medalUntil = 0;     // festeggiamento della medaglia a schermo
  uint32_t milestoneUntil = 0; // festeggiamento del traguardo di serie
  bool colAsk = false;         // domanda della collezione in sospeso
  uint32_t colDay = 0;         // giorno (today() + 1) dell'ultimo RIPRENDI, 0 = mai
  uint32_t farewellMinAge = 0; // pet ripreso: niente addio prima di questa età
  PetRitratto col[COL_POSTI];
  uint8_t colN = 0;
  int8_t scambioPosto = SCAMBIO_NESSUNO;  // pet bloccato da uno scambio in corso
  uint32_t tradeFatto = 0;     // tradeId dell'ultimo scambio applicato (nel blob)
  int16_t evoScDa = 0;         // evoluzione per scambio da mostrare
  int8_t evoScPosto = SCAMBIO_NESSUNO;
  uint16_t linkWinsN = 0;       // vittorie in collegamento (LINK_WINS_KEY)
  bool linkWinsDirty = false;   // da scrivere al prossimo save()
  uint16_t linkTradesN = 0;     // scambi effettuati (LINK_TRADES_KEY)
  bool linkTradesDirty = false;
  uint32_t spaventoFino = 0;    // pausa dopo uno scossone (solo in RAM)
  bool movimentoOn = true;      // MOVIMENTO_KEY

  uint32_t today() const { return lastSeenEpoch ? lastSeenEpoch / 86400 : 0; }
  void registerCare();   // prima cura del giorno: serie + legame
  void addBond(uint8_t amt);
  void giocoInclinato(uint16_t punti);  // la parte comune di grottaResult e surfResult
  void checkMedals();
  void tick();
  void hatch();
  void registerSpecies(int16_t dex) { registerSpecies(dex, shiny); }
  void registerSpecies(int16_t dex, bool sh);
  void save();
  void load();
  void loadLegacy();  // chiavi singole (fino alla 1.18)
  void colCarica();
  void colSalva();
  bool colEntra() const;  // la cerimonia in corso porta alla domanda della collezione
  void toSave(PetSave &s) const;
  void fromSave(const PetSave &s);
  static uint8_t clamp100(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
};
