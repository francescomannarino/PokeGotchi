#pragma once
// Motore di battaglia con le regole di Rosso/Blu, 1 contro 1 o a squadre fino a 3 per lato:
// statistiche ATK/DEF/VEL/SPC, formula del danno di Gen 1, tipi doppi, STAB, brutti colpi,
// PP e Scontro, stati alterati, mosse a due turni e ripetute, Trasformazione, cambi, IA
// della CPU e statistiche ricavate dal pet o da un ritratto.
// Niente UI, niente hardware, niente `random()`: tutto il caso passa da un RNG con seme
// (`BattleRng`), così una battaglia si ripete identica nei test e sulle due schede del
// collegamento, a parità di seme e di scelte.
//
// Derivato da DylanPDao/TamaPoke (MIT).
// Copyright (c) 2026 Quique Tortosa
// Permission is hereby granted, free of charge, to any person obtaining a copy of this
// software and associated documentation files (the "Software"), to deal in the Software
// without restriction, including without limitation the rights to use, copy, modify,
// merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following
// conditions: The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
// CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// USO
//   Combatant a, b;
//   battleCombattente(a, dex, livello, ps, att, dif, vel, spc, nullptr);  // mosse dal learnset
//   Battle bt;
//   battleInizia(bt, a, b, seme);            // copia i due lati, PP e PS pieni
//   while (battleTurno(bt, slotA, slotB)) {  // slot 0..3 della mossa scelta
//     BattleEvento e;
//     while (bt.coda.prendi(e)) narra(e);   // una riga alla volta, al ritmo della UI
//   }
//   bt.vincitore: 0, 1 o BATTLE_PAREGGIO
//   Lo slot della CPU: battleSceltaIA(bt, 1, difficile). Il pet: combattenteDaPet(c, pet).
//
// SQUADRE
//   battleIniziaSquadre(bt, squadraA, nA, squadraB, nB, seme);  // 1..BATTLE_SQUADRA per lato
//   bt.lato[k] è sempre il Pokémon in campo; gli altri sono in bt.squadra[k][…]
//   (battleMembro(bt, k, i) li dà tutti, quello in campo compreso).
//   La scelta di un turno è uno slot 0..3 oppure BATTLE_CAMBIA + i (manda in campo il
//   membro i, se battlePuoCambiare). Dopo un KO, se il lato ha ancora qualcuno,
//   bt.deveCambiare[k] è true: la UI (o battleSceltaSostituto) sceglie e chiama
//   battleCambioForzato(bt, k, i), che non consuma un turno. Finisce quando un lato è tutto
//   KO. battleInizia(bt, a, b, seme) è la squadra da 1: identica al motore 1 contro 1.
//
// Solo aritmetica intera. Il danno segue pokered (core.asm): con un brutto colpo il
// livello raddoppia e si usano le statistiche senza stadi, scottatura, paralisi né
// barriere; con statistiche sopra 255 attacco e difesa si dividono entrambi per 4.
// Fedele a Gen 1 anche il tiro di precisione su 255/256: una mossa al 100% manca 1 volta
// su 256 (Comete esclusa).
#include <stdint.h>
#include "dex.h"
#include "moves.h"

class Pet;

#define BATTLE_MOSSE 4
#define BATTLE_PAREGGIO 2         // valore di `vincitore`: entrambi KO o turni finiti
#define BATTLE_IN_CORSO 0xFF
#define BATTLE_MAX_TURNI 200      // oltre: pareggio, così nessuna battaglia è infinita
#define BATTLE_MAX_EVENTI 48      // eventi di un turno (il caso peggiore ne fa una trentina)
#define BATTLE_CODA 96            // coda della narrazione: due turni interi
#define BATTLE_SQUADRA 3          // combattenti per lato al massimo
#define BATTLE_CAMBIA 0x10        // scelta del turno BATTLE_CAMBIA + i: cambio col membro i
#define BATTLE_NESSUNO 0xFF       // nessun membro (battleSceltaSostituto)

// xorshift32. Il seme si moltiplica per una costante dispari: da un seme piccolo (1, 2…,
// o millis()) lo xorshift darebbe primi valori minuscoli, tutti uguali tra loro. Il seme 0
// bloccherebbe la sequenza: lo si sostituisce con una costante.
struct BattleRng {
  uint32_t s = 0x9E3779B9u;
  void seme(uint32_t v) { s = v ? v * 2654435761u : 0x9E3779B9u; }
  uint32_t prossimo() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  uint8_t byte() { return (uint8_t)(prossimo() >> 24); }
  // intero uniforme in [0, n), n > 0
  uint16_t sotto(uint16_t n) { return (uint16_t)(((uint64_t)prossimo() * n) >> 32); }
};

// Un Pokémon in battaglia. È una copia: il motore non scrive mai nel pet.
// stat[] e stadio[] usano gli indici STAT_* di moves.h (ATK, DEF, VEL, SPC; poi
// precisione ed elusione, che hanno solo lo stadio).
struct Combatant {
  int16_t dex = 0;
  uint8_t livello = 1;
  uint8_t tipo1 = T_NORMALE, tipo2 = T_NESSUNO;
  uint8_t velBase = 0;            // VEL base della specie: probabilità di brutto colpo
  uint16_t psMax = 1, ps = 1;
  uint16_t stat[4] = { 1, 1, 1, 1 };
  uint8_t mosse[BATTLE_MOSSE] = { 0, 0, 0, 0 };  // id Gen 1 (MV_*), 0 = slot vuoto
  uint8_t pp[BATTLE_MOSSE] = { 0, 0, 0, 0 };
  uint8_t ppMax[BATTLE_MOSSE] = { 0, 0, 0, 0 };
  int8_t stadio[6] = { 0, 0, 0, 0, 0, 0 };      // -6..+6
  uint8_t stato = ST_NESSUNO;     // stato non volatile (paralisi … congelamento)
  bool riflesso = false, schermoluce = false;  // raddoppiano DEF / SPC in difesa
  bool ricarica = false;          // Iper Raggio: salta il prossimo turno

  // stati volatili: durano finché il Pokémon resta in campo; al cambio, come in Gen 1,
  // restano solo PS, PP e lo stato non volatile (col suo turniSonno; Tossina torna veleno)
  uint8_t turniSonno = 0;         // turni di sonno rimasti (1..7; Riposo 2)
  uint8_t tossico = 0;            // Tossina: il danno del veleno è tossico/16 e cresce
  uint8_t confusione = 0;         // turni di confusione rimasti
  bool tentenna = false;          // colpito da un tentennamento: perde l'azione del turno
  uint8_t carica = 0;             // mossa a due turni in caricamento (Solarraggio, Fossa…)
  bool invulnerabile = false;     // Fossa / Volo in caricamento: nessuna mossa lo colpisce
  uint8_t bloccata = 0;           // mossa che si ripete da sola: Colpo, Ira, Avvolgibotta…
  uint8_t turniBloccata = 0;      // ripetizioni rimaste (Ira: fino alla fine)
  uint16_t dannoBloccato = 0;     // danno che le mosse che intrappolano ripetono
  bool intrappolato = false;      // l'altro lo stringe: non può agire
  bool nebbia = false;            // Nebbia: nessuno gli abbassa gli stadi
  bool concentrazione = false;    // Focalenergia: brutti colpi ×4
  bool parassiseme = false;       // Parassiseme: perde 1/16 dei PS a ogni turno
  // Trasformazione e Conversione cambiano specie, tipi, statistiche e mosse: gli originali
  // restano qui e battleRipristina li rimette
  bool trasformato = false;
  struct {
    int16_t dex;
    uint8_t tipo1, tipo2, velBase;
    uint16_t stat[4];
    uint8_t mosse[BATTLE_MOSSE], ppMax[BATTLE_MOSSE];
    uint8_t pp[BATTLE_MOSSE];     // PP delle mosse originali, che tornano al cambio
  } orig = {};

  bool ko() const { return ps == 0; }
};

// Riempie `c` con specie, livello e statistiche già calcolate (l'adattatore dal pet le
// ricava da geni e allenamento). `mosse` = 4 id, oppure nullptr per le ultime 4 imparate
// fino a quel livello. PP e PS partono pieni.
void battleCombattente(Combatant &c, int16_t dex, uint8_t livello, uint16_t ps, uint16_t att,
                       uint16_t dif, uint16_t vel, uint16_t spc, const uint8_t *mosse);

// Le ultime 4 mosse distinte imparate da `dex` fino a `livello`, la più vecchia per prima;
// gli slot avanzati restano 0. Senza mosse (Abra): solo MOVE_STRUGGLE.
void battleMosseApprese(int16_t dex, uint8_t livello, uint8_t out[BATTLE_MOSSE]);

// Il pet come combattente: livello min(100, level()), statistiche con la formula di Gen 1
// (DV dai geni, bonus dall'allenamento, Speciale dalla media di ATK e DEF), le ultime 4
// mosse imparate. false (e `c` vuoto) se è ancora un uovo.
bool combattenteDaPet(Combatant &c, const Pet &p);

// Lo stesso da un ritratto (collezione, squadre del collegamento), con la stessa formula:
// a parità di specie, età, geni e allenamento il combattente è identico a quello del pet.
// false (e `c` vuoto) se la specie non è valida.
struct PetRitratto;
bool combattenteDaRitratto(Combatant &c, const PetRitratto &r);

// Statistica di Gen 1: ((base + dv)·2 + bonus)·L/100 + 5; per i PS + L + 10.
// dv 0..15, bonus 0..63 (la radice delle "stat exp" / 4).
uint16_t battleStatGen1(uint8_t base, uint8_t dv, uint8_t bonus, uint8_t livello, bool ps);

// Fine battaglia: PS, PP, stadi, stati e barriere tornano come all'inizio, e la specie
// originale torna dopo una Trasformazione.
void battleRipristina(Combatant &c);

// true se nessuno slot ha PP: la mossa sarà Scontro.
bool battleDeveScontro(const Combatant &c);

// La mossa che `c` userà scegliendo `slot`: Scontro se i PP sono finiti; il primo slot
// usabile se quello scelto è vuoto o senza PP.
uint8_t battleMossaScelta(const Combatant &c, uint8_t slot);

// false se questo turno `c` non sceglie: ricarica, caricamento, mossa che si ripete, o
// intrappolato. La UI può saltare il menu e passare uno slot qualsiasi.
bool battleDeveScegliere(const Combatant &c);

// Efficacia di un tipo d'attacco contro i tipi del difensore, ×4:
// 0 immune, 1 = ¼, 2 = ½, 4 = 1×, 8 = 2×, 16 = 4×.
uint8_t battleEfficacia(uint8_t tipoMossa, const Combatant &dif);

// Statistica con lo stadio applicato (rapporti di Gen 1: -6 = 25%, +1 = 150%, +6 = 400%),
// tra 1 e 999.
uint16_t battleStadio(uint16_t valore, int8_t stadio);

// Danno della formula di Gen 1, prima del limite dei PS del bersaglio: STAB, tipi e
// fattore casuale `r` (217..255; sotto i 2 PS non si applica). 0 = nessun danno (immune,
// oppure ridotto a 0 dai tipi: in Gen 1 conta come mancato). Solo per le mosse con
// potenza; i danni fissi li calcola il turno.
uint16_t battleDanno(const Combatant &att, const Combatant &dif, uint8_t mossa, bool brutto,
                     uint8_t r);

// Tiro del brutto colpo di Gen 1: VEL base / 512, ×8 per le mosse ad alto tasso
// (EF_BRUTTO_COLPO), ×4 con Focalenergia, fino a 255/256. Focalenergia fa ciò che
// promette, come in Stadium, non il ¼ del bug di Rosso/Blu.
bool battleTiraBruttoColpo(BattleRng &rng, uint8_t velBase, bool altoTasso,
                           bool concentrazione = false);

// Narrazione: un turno produce una lista di eventi, nell'ordine in cui vanno raccontati.
// `lato` = 0/1, il Pokémon a cui succede la cosa; `mossa` = la mossa in corso.
enum : uint8_t {
  EV_MOSSA = 0,         // lato usa mossa
  EV_SCONTRO,           // lato non ha più PP: userà Scontro
  EV_RICARICA,          // lato deve ricaricarsi e salta il turno
  EV_MANCATA,           // la mossa di lato non va a segno
  EV_FALLITA,           // la mossa di lato fallisce (KO su chi è più veloce, Mangiasogni)
  EV_NON_HA_EFFETTO,    // lato (il bersaglio) è immune al tipo della mossa
  EV_NESSUN_EFFETTO,    // la mossa di lato non fa niente (Splash, effetti non gestiti)
  EV_BRUTTO_COLPO,      // lato ha messo a segno un brutto colpo
  EV_EFFICACIA,         // valore = efficacia ×4 (solo se diversa da 4), lato = bersaglio
  EV_DANNO,             // lato perde valore PS (totale dei colpi)
  EV_COLPI,             // la mossa di lato ha colpito valore volte
  EV_RINCULO,           // lato perde valore PS per il contraccolpo
  EV_ASSORBE,           // lato recupera valore PS
  EV_SCHIANTO,          // lato ha mancato un calcio e perde valore PS
  EV_KO,                // lato va KO
  EV_FINE,              // la battaglia è finita: valore = vincitore
  EV_CARICA,            // lato carica la mossa (Fossa e Volo: intanto è invulnerabile)
  EV_STADIO,            // lato: valore >> 8 = STAT_*, (int8_t)(valore & 0xFF) = stadi
  EV_STATO,             // lato prende lo stato valore (ST_*, confusione compresa)
  EV_DORME,             // lato dorme e non agisce
  EV_SVEGLIA,           // lato si sveglia (in Gen 1 perde comunque il turno)
  EV_CONGELATO,         // lato è congelato e non agisce
  EV_SCONGELA,          // lato si scongela: colpito da una mossa di fuoco
  EV_PARALIZZATO,       // lato è paralizzato e non agisce
  EV_CONFUSO,           // lato è confuso (prima di agire)
  EV_SI_COLPISCE,       // lato si colpisce da solo nella confusione: valore = PS persi
  EV_NON_PIU_CONFUSO,   // lato non è più confuso
  EV_TENTENNA,          // lato tentenna e non agisce
  EV_INTRAPPOLATO,      // lato è stretto e non può agire
  EV_VELENO,            // lato perde valore PS per il veleno
  EV_SCOTTATURA,        // lato perde valore PS per la scottatura
  EV_PARASSISEME,       // lato perde valore PS per il Parassiseme (l'altro: EV_ASSORBE)
  EV_SEME,              // lato è stato seminato dal Parassiseme
  EV_CURA,              // lato recupera valore PS (Ripresa, Riposo)
  EV_IRA,               // l'ira di lato cresce: ATK +1
  EV_PROTETTO,          // lato è protetto dalla Nebbia
  EV_NEBBIA,            // lato si avvolge nella Nebbia
  EV_CONCENTRAZIONE,    // lato si concentra (Focalenergia)
  EV_BARRIERA,          // lato alza una barriera: valore 0 Riflesso, 1 Schermoluce
  EV_AZZERA,            // stadi e stati volatili azzerati per entrambi (Nube)
  EV_CONVERSIONE,       // lato prende i tipi: valore = tipo1 | tipo2 << 8
  EV_TRASFORMA,         // lato si trasforma nella specie valore
  EV_RITIRO,            // lato richiama il suo Pokémon: valore = indice << 8 | dex
  EV_ENTRA,             // lato manda in campo: valore = indice << 8 | dex
};

struct BattleEvento {
  uint8_t tipo;
  uint8_t lato;
  uint8_t mossa;
  uint16_t valore;
};

// Coda della narrazione: il motore accoda, la UI toglie una riga alla volta al suo ritmo,
// così nessuna riga si perde anche se la UI ne mostra poche per volta. Tiene due turni
// interi: basta svuotarla prima di ogni battleTurno. `persi` conta gli eventi scartati a
// coda piena e deve restare 0.
struct BattleCoda {
  BattleEvento buf[BATTLE_CODA];
  uint8_t testa = 0, n = 0;
  uint16_t persi = 0;
  bool vuota() const { return n == 0; }
  const BattleEvento &prossimo() const { return buf[testa]; }  // solo se !vuota()
  bool prendi(BattleEvento &e) {
    if (!n) return false;
    e = buf[testa];
    testa = (uint8_t)((testa + 1) % BATTLE_CODA);
    n--;
    return true;
  }
  void metti(const BattleEvento &e) {
    if (n >= BATTLE_CODA) {
      persi++;
      return;
    }
    buf[(testa + n) % BATTLE_CODA] = e;
    n++;
  }
};

struct Battle {
  Combatant lato[2];                // i due Pokémon in campo
  // le squadre: squadra[k][inCampo[k]] è aggiornato solo quando il Pokémon esce, quindi
  // per leggere un membro qualsiasi c'è battleMembro()
  Combatant squadra[2][BATTLE_SQUADRA];
  uint8_t membri[2] = { 1, 1 };
  uint8_t inCampo[2] = { 0, 0 };
  bool deveCambiare[2] = { false, false };  // in campo è KO e il lato ha ancora qualcuno
  BattleRng rng;
  uint16_t turno = 0;               // turni giocati
  uint8_t vincitore = BATTLE_IN_CORSO;
  uint8_t nEventi = 0;              // eventi dell'ultimo turno
  BattleEvento eventi[BATTLE_MAX_EVENTI];
  uint16_t eventiPersi = 0;         // oltre BATTLE_MAX_EVENTI in un turno: deve restare 0
  BattleCoda coda;                  // gli stessi eventi, per la UI
};

// Copia i due lati (con battleRipristina) e semina l'RNG.
void battleInizia(Battle &b, const Combatant &a, const Combatant &c, uint32_t seme);

// Squadre da 1..BATTLE_SQUADRA (oltre si tronca, 0 vale 1): in campo va il primo di
// ciascuna. Con 1 per lato è battleInizia.
void battleIniziaSquadre(Battle &b, const Combatant *a, uint8_t na, const Combatant *c,
                         uint8_t nc, uint32_t seme);

// Il membro `i` del lato `lato`: quello in campo è bt.lato[lato].
const Combatant &battleMembro(const Battle &b, uint8_t lato, uint8_t i);

// Membri non KO del lato, quello in campo compreso.
uint8_t battleRestanti(const Battle &b, uint8_t lato);

// true se il lato può mandare in campo il membro `i`: esiste, non è KO, non è già in
// campo, e chi è in campo non è legato a una mossa (ricarica, caricamento, Furia, Ira,
// intrappolare: come in Gen 1; chi è intrappolato invece può uscire). Con deveCambiare
// basta la prima parte.
bool battlePuoCambiare(const Battle &b, uint8_t lato, uint8_t i);

// Cambio forzato dopo un KO: manda in campo `i` senza consumare un turno né l'RNG
// (accoda EV_ENTRA; `eventi` ha solo quello). false se il lato non deve cambiare o `i` non
// va bene. Se la UI non lo chiama, il turno seguente manda in campo il primo vivo.
bool battleCambioForzato(Battle &b, uint8_t lato, uint8_t i);

// Il sostituto che la CPU sceglie per `lato` (cambio forzato, o un cambio volontario):
// facile = il primo vivo non in campo; difficile = il miglior rapporto di tipo contro chi
// sta in campo dall'altra parte (efficacia delle sue mosse di danno diviso l'efficacia dei
// tipi dell'altro su di lui), a parità i PS. BATTLE_NESSUNO se non c'è nessuno. Non usa l'RNG.
uint8_t battleSceltaSostituto(const Battle &b, uint8_t lato, bool difficile);

// Un turno: ognuno sceglie uno slot 0..3 oppure BATTLE_CAMBIA + i. I cambi avvengono
// prima delle mosse (prima il lato 0) e chi entra subisce la mossa dell'avversario; chi
// cambia non agisce in quel turno. Ordine delle mosse: priorità, poi VEL con stadi e
// paralisi, poi moneta. Un cambio non valido usa il primo slot con PP. Riempie `eventi`
// e accoda in `coda`; restituisce false a battaglia finita.
// Come in pokered, prima di agire contano sonno, congelamento (in Gen 1 non passa da solo:
// solo una mossa di fuoco o Nube), intrappolamento, tentennamento, ricarica, confusione
// (50% di colpirsi) e paralisi (25% di restare fermo); veleno, scottatura e Parassiseme
// tolgono 1/16 dei PS dopo l'azione di ciascuno.
bool battleTurno(Battle &b, uint8_t sceltaA, uint8_t sceltaB);

// Lo slot che la CPU sceglie per `lato`, mai una mossa con 0 PP (senza PP: 0, e sarà
// Scontro). Facile: a caso tra gli slot usabili. Difficile: il punteggio migliore, che
// premia il danno atteso e il KO, sconta precisione, ricarica e caricamento, e dà valore
// alle mosse di stato solo quando servono. Usa l'RNG della battaglia: riproducibile.
uint8_t battleSceltaIA(Battle &b, uint8_t lato, bool difficile);
