#include "pet.h"
#include "dex.h"
#include "hal.h"
#include "savefile.h"
#include <stddef.h>
#include <string.h>

void Pet::begin() {
  prefs.begin(SAVE_NS_PET, false);
  if (!prefs.getBool("init", false)) {
    prefs.putBool("init", true);
    newEgg();
  } else {
    load();
  }
  movimentoOn = prefs.getBool(MOVIMENTO_KEY, true);
  lastTick = millis();
}

void Pet::newEgg() {
  ceremony = CER_NONE;
  colAsk = false;
  neglectTicks = 0;
  weight = 0;
  speciesId = -1;
  prevSpeciesId = -1;
  eggTarget = pickEggSpecies();  // specie nascosta secondo rarità e pokedex
  starterPick = (registeredCount() == 0);  // prima partita: il giocatore sceglie l'iniziale
  // tiro shiny: 1/48 di base, migliore dopo un addio e con serie/legame alti
  int shinyBase = (lastEnd == CER_FAREWELL ? 24 : 48) - careBonus();
  if (shinyBase < 8) shinyBase = 8;
  eggShiny = (random(shinyBase) == 0);
  eggTaps = 0;
  fullness = 80;
  joy = 80;
  energy = 80;
  hygiene = 100;
  poops = 0;
  ageMinutes = 0;
  careMistakes = 0;
  mistakeCooldown = 0;
  sleeping = false;
  save();
}

// progressione offline: il tempo è passato anche da spento, ma con pietà: le barre
// scendono con un minimo a 15 (torna affamato, non morto), senza trascuratezze né fughe
// in assenza
static uint8_t dropTo(uint8_t v, uint8_t d, uint8_t fl) {
  if (v <= fl) return v;
  return (v - fl > d) ? v - d : fl;
}

void Pet::setClock(uint32_t nowEpoch) {
  lastSeenEpoch = nowEpoch;
  if (nowEpoch) save();  // lo persiste subito: un blackout non fa perdere il riferimento
}

void Pet::syncClock(uint32_t nowEpoch) {
  uint32_t seen = prefs.getUInt("seen", 0);
  lastSeenEpoch = nowEpoch;
  if (nowEpoch == 0) return;
  uint32_t mins = (seen && nowEpoch > seen) ? (nowEpoch - seen) / 60 : 0;
  if (mins < 2 || ceremony != CER_NONE || starterPick) {
    save();  // prima volta, senza tempo da applicare o ancora in scelta dell'iniziale
    return;
  }
  if (mins > 14UL * 24 * 60) mins = 14UL * 24 * 60;  // limite: 2 settimane

  for (uint32_t i = 0; i < mins; i++) {
    ageMinutes++;
    if (isEgg()) {
      if (ageMinutes >= 3) hatch();  // si schiude in tua assenza
      continue;
    }
    if (sleeping) {  // riposo: scende lento e con un minimo, come dal vivo
      energy = clamp100(energy + 6);
      if (ageMinutes % 2 == 0) {
        fullness = dropTo(fullness, 1, 30);
        joy = dropTo(joy, 1, 35);
      }
      if (ageMinutes % 3 == 0) hygiene = dropTo(hygiene, 1, 45);
      continue;
    }
    fullness = dropTo(fullness, 2, 15);
    energy = dropTo(energy, 1, 15);
    hygiene = dropTo(hygiene, 1, 15);
    joy = dropTo(joy, 1, 15);
  }
  if (!isEgg()) {
    if (!sleeping) {  // se dorme non sporca
      uint8_t p = poops + mins / 240;
      poops = p > 3 ? 3 : p;
    }
    // l'evoluzione NON si applica offline: resta pronta e la attiva l'utente toccando
    // il Pokémon al ritorno (così vede la trasformazione)
  }
  Serial.printf("offline: %u min applicati (lv.%u)\n", mins, level());
  save();
}

void Pet::update(uint32_t nowMs) {
  // fine cerimonia: la creatura se ne va e resta un uovo nuovo. Dopo un addio o il
  // rilascio di una forma finale prima si chiede se conservarla nella collezione: la
  // cerimonia resta ferma alla fine (e con lei il tempo) finché non si risponde
  if (ceremony != CER_NONE && !colAsk && !timeLeft(ceremonyUntil)) {
    if (!colEntra()) {
      newEgg();
      return;
    }
    colAsk = true;
    save();
  }
  while (nowMs - lastTick >= PET_TICK_MS) {
    lastTick += PET_TICK_MS;
    tick();
  }
}

void Pet::tick() {
  if (ceremony != CER_NONE) return;  // il tempo si ferma durante l'addio
  if (starterPick) return;  // la partita non parte finché non si sceglie l'iniziale: se il tempo
                            // scorresse qui, l'uovo si schiuderebbe da solo dopo 3 min con la specie
                            // sorteggiata e si perderebbe la scelta del giocatore
  ageMinutes++;

  if (isEgg()) {
    if (ageMinutes >= 3) hatch();  // se non lo tocchi, si schiude da solo dopo 3 min
    return;
  }

  // il sonno è riposo: l'energia si recupera e i bisogni scendono MOLTO più lentamente
  // che da sveglio, con un minimo (si sveglia chiedendo un po' di coccole, non a zero,
  // senza trascuratezze né fughe). Da sveglio: cibo -2/min, igiene/gioia -1/min. Il
  // peso si brucia ancora; la serie di buona cura (goodTicks) resta in pausa.
  if (sleeping) {
    energy = clamp100(energy + 6);
    if (weight > 0 && ageMinutes % 3 == 0) weight--;
    if (ageMinutes % 2 == 0) {                 // ~4 volte più lento che da sveglio
      fullness = dropTo(fullness, 1, 30);
      joy = dropTo(joy, 1, 35);
    }
    if (ageMinutes % 3 == 0) hygiene = dropTo(hygiene, 1, 45);
    checkMedals();  // può ancora superare un livello per età mentre dorme
    if (++ticksSinceSave >= 5) pendingSave = true;
    return;
  }

  if (ageMinutes % MINUTES_PER_LEVEL == 0) halSfx(SFX_LEVEL);  // è salito di livello (da sveglio)

  fullness = clamp100(fullness - 2);
  energy = clamp100(energy - 1);
  if (fullness > 40 && poops < 3 && random(100) < 15) poops++;

  hygiene = clamp100(hygiene - 1 - 4 * poops);
  // il sovrappeso dà pigrizia: l'energia cala il doppio
  if (weight > 50) energy = clamp100(energy - 1);
  if (weight > 0 && ageMinutes % 3 == 0) weight--;

  // la disciplina forgia la difesa: 12 h di fila ben curato = +1 DEF
  if (lowestStat() >= 40) {
    if (++goodTicks >= 720) {
      goodTicks = 0;
      if (trDef < 100) trDef++;
    }
  } else {
    goodTicks = 0;
  }

  int dJoy = -1;
  if (fullness < 30) dJoy -= 2;
  if (hygiene < 30) dJoy -= 2;
  joy = clamp100(joy + dJoy);

  // Trascuratezza: lasciare una statistica a terra conta come errore di cura (con tempo
  // di recupero per non contare la stessa trascuratezza ogni minuto)
  if (mistakeCooldown > 0) mistakeCooldown--;
  if (lowestStat() <= 10 && mistakeCooldown == 0) {
    careMistakes++;
    mistakeCooldown = 60;
    if (bond > 1) bond--;  // la trascuratezza raffredda il legame, ma senza azzerarlo:
                           // a -3 ogni 30 min si perdeva molto più di quanto si potesse
                           // guadagnare in un giorno intero e il legame si bloccava
  }

  checkMedals();  // l'evoluzione la attiva l'utente (canEvolveNow + tap), non il tick

  // abbandono totale: con TUTTO a zero per un'ora è pronto a fuggire; NON se ne va da
  // solo, lo attiva l'utente con il pulsante (finale triste, ne è testimone)
  if (fullness == 0 && joy == 0 && energy == 0 && hygiene == 0) {
    if (neglectTicks < RUNAWAY_TICKS) neglectTicks++;
  } else {
    neglectTicks = 0;  // basta una sola cura per salvarlo
  }

  // ciclo completo (forma finale + 7 giorni): l'addio NON scatta da solo; resta pronto
  // (canFarewellNow) e lo attiva l'utente con il pulsante, perché lo veda

  // salvataggio automatico periodico: NON scrivere in flash qui (gira dentro il loop,
  // mentre si anima); basta segnare e lasciare che il loop lo esegua quando si attenua
  // lo schermo
  if (++ticksSinceSave >= 5) pendingSave = true;
}

// esegue il salvataggio periodico in sospeso (lo chiama il loop in un momento senza
// animazione, così il blocco della scrittura in flash non si nota)
void Pet::flushSave() {
  if (pendingSave) save();
}

// restano membri non registrati nella linea evolutiva di questa base?
bool Pet::lineHasUnregistered(int16_t base) const {
  int16_t cur = base;
  for (int guard = 0; cur >= 1 && cur <= 151 && guard < 6; guard++) {
    if (!isRegistered(cur)) return true;
    if (cur == DEX_EEVEE) {
      for (int16_t b = 134; b <= 136; b++)
        if (!isRegistered(b)) return true;
      return false;
    }
    cur = DEX_TBL[cur].evolvesTo;
  }
  return false;
}

uint8_t Pet::eggRarity() const {
  return (eggTarget >= 1 && eggTarget <= 151) ? DEX_TBL[eggTarget].rarity : R_COMUN;
}

// sceglie la specie dell'uovo: tiro di rarità (migliorato da un addio completo,
// penalizzato da una fuga) e preferenza per le linee incomplete
int16_t Pet::pickEggSpecies() {
  // prima partita: iniziale classico
  if (registeredCount() == 0) {
    return CLASSIC_DEX[random(NUM_CLASSIC_DEX)];
  }

  uint8_t tier = R_COMUN;
  if (lastEnd != CER_RUNAWAY) {
    bool blessed = (lastEnd == CER_FAREWELL);
    int rare = (blessed ? 45 : 27) + careBonus();
    int leg = (registeredCount() >= 25) ? (blessed ? 10 : 3) + careBonus() / 3 : 0;
    int r = random(100);
    if (r < leg) tier = R_LEGENDARIO;
    else if (r < leg + rare) tier = R_RARO;
  }

  // candidati del livello con linea incompleta; se non ce ne sono, scende di livello;
  // se il pokedex del livello è completo, va bene uno qualsiasi del livello
  for (int pass = 0; pass < 2; pass++) {
    for (int t = tier; t >= R_COMUN; t--) {
      int16_t cand[80];
      int n = 0;
      for (int16_t d = 1; d <= 151 && n < 80; d++) {
        if (DEX_TBL[d].rarity != t) continue;
        if (pass == 0 && !lineHasUnregistered(d)) continue;
        cand[n++] = d;
      }
      if (n > 0) return cand[random(n)];
    }
  }
  return CLASSIC_DEX[random(NUM_CLASSIC_DEX)];  // irraggiungibile, per sicurezza
}

void Pet::registerSpecies(int16_t dex, bool sh) {
  if (dex < 1 || dex > 151) return;
  dexReg[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
  if (sh) dexShinyReg[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
}

// la serie e il legame migliorano il sorteggio dell'uovo (0..~14)
int Pet::careBonus() const {
  int s = streak > 30 ? 30 : streak;
  return s / 3 + bond / 25;
}

// prima cura del giorno: avanza la serie e rafforza il legame
void Pet::registerCare() {
  if (isEgg() || ceremony != CER_NONE) return;
  uint32_t d = today();
  if (d == 0 || d == lastCareDay) return;  // senza orologio, o già contato oggi
  if (lastCareDay == 0 || d == lastCareDay + 1) {
    streak++;
  } else {
    streak = 1;        // c'è stato un buco di giorni
    lastMilestone = 0;
  }
  lastCareDay = d;
  bondToday = 0;
  if (streak > bestStreak) bestStreak = streak;
  bond = clamp100(bond + 4);
  uint16_t ms = (streak >= 100) ? 100 : (streak >= 30) ? 30
              : (streak >= 7)   ? 7   : (streak >= 3)  ? 3 : 0;
  if (ms > lastMilestone) {
    lastMilestone = ms;
    milestoneUntil = millis() + 4500;
  }
  checkMedals();
  save();
}

void Pet::addBond(uint8_t amt) {
  if (bondToday >= 20) return;  // limite giornaliero: il legame non si farma
  bond = clamp100(bond + amt);
  bondToday += amt;
}

void Pet::checkMedals() {
  if (isEgg()) return;
  uint16_t before = medals;
  if (level() >= 10) medals |= MED_LV10;
  if (level() >= 25) medals |= MED_LV25;
  if (level() >= 50) medals |= MED_LV50;
  if (berryKnown) medals |= MED_BERRY;
  if (streak >= 7) medals |= MED_STREAK7;
  if (bond >= 100) medals |= MED_BOND;
  if (DEX_TBL[speciesId].evolvesTo == 0) medals |= MED_FINAL;
  if (weight == 0 && level() >= 5 && careMistakes == 0) medals |= MED_FIT;
  uint16_t gained = medals & ~before;
  if (gained) {
    for (uint16_t m = gained; m; m &= (m - 1)) totalMedals++;
    newMedal = gained;
    medalUntil = millis() + 4000;
    if (!sleeping) halSfx(SFX_MEDAL);
    save();
  }
}

void Pet::rename(const char *name) {
  strncpy(nick, name, sizeof(nick) - 1);
  nick[sizeof(nick) - 1] = 0;
  save();
}

uint32_t Pet::trainerId() {
  uint32_t id = prefs.getUInt("tid", 0);
  while (!id) id = ((uint32_t)random(0x10000) << 16) | (uint32_t)random(0x10000);
  if (!prefs.isKey("tid")) prefs.putUInt("tid", id);
  return id;
}

bool Pet::trainerName(char *out, size_t n) {
  if (!n) return false;
  out[0] = 0;
  if (prefs.isKey("tnome")) prefs.getString("tnome", out, n);
  return out[0] != 0;
}

void Pet::setTrainerName(const char *name) { prefs.putString("tnome", name); }

static uint16_t calcStat(uint8_t base, uint8_t gene, uint16_t lvl, uint8_t tr) {
  return (uint16_t)base * gene / 100 + lvl + tr;
}

uint16_t Pet::atkStat() const {
  return isEgg() ? 0 : calcStat(DEX_TBL[speciesId].bAtk, geneAtk, level(), trAtk);
}
uint16_t Pet::defStat() const {
  return isEgg() ? 0 : calcStat(DEX_TBL[speciesId].bDef, geneDef, level(), trDef);
}
uint16_t Pet::speStat() const {
  return isEgg() ? 0 : calcStat(DEX_TBL[speciesId].bSpe, geneSpe, level(), trSpe);
}
uint16_t Pet::spcStat() const {
  return isEgg() ? 0 : calcStat(DEX_TBL[speciesId].bSpc, (geneAtk + geneDef) / 2, level(),
                                (trAtk + trDef) / 2);
}

void statDaRitratto(const PetRitratto &r, uint16_t out[4]) {
  out[0] = out[1] = out[2] = out[3] = 0;
  if (r.specie < 1 || r.specie > 151) return;
  const DexEntry &d = DEX_TBL[r.specie];
  uint16_t lv = levelFromAge(r.ageMinutes);
  out[0] = calcStat(d.bAtk, r.geneAtk, lv, r.trAtk);
  out[1] = calcStat(d.bDef, r.geneDef, lv, r.trDef);
  out[2] = calcStat(d.bSpe, r.geneSpe, lv, r.trSpe);
  out[3] = calcStat(d.bSpc, (r.geneAtk + r.geneDef) / 2, lv, (r.trAtk + r.trDef) / 2);
}

uint16_t Pet::registeredCount() const {
  uint16_t n = 0;
  for (int i = 1; i <= 151; i++)
    if (isRegistered(i)) n++;
  return n;
}

// forma finale che ha già compiuto il suo ciclo (7 giorni): pronto a dire addio.
// L'addio lo attiva l'utente con il pulsante (non scatta da solo, perché lo veda)
bool Pet::canFarewellNow() const {
  return !isEgg() && !sleeping && ceremony == CER_NONE && scambioPosto != SCAMBIO_ATTIVO &&
         DEX_TBL[speciesId].evolvesTo == 0 && ageMinutes >= FAREWELL_AGE_MIN &&
         ageMinutes >= farewellMinAge;
}

// abbandono totale per 1 h: pronto a fuggire. Lo attiva l'utente con il pulsante
// (finale triste); curarlo anche per un solo tick lo salva (neglectTicks si azzera)
bool Pet::canRunawayNow() const {
  return !isEgg() && !sleeping && ceremony == CER_NONE && scambioPosto != SCAMBIO_ATTIVO &&
         neglectTicks >= RUNAWAY_TICKS;
}

void Pet::startFarewell() {
  if (isEgg() || ceremony != CER_NONE || scambioPosto == SCAMBIO_ATTIVO) return;
  lastEnd = CER_FAREWELL;
  ceremony = CER_FAREWELL;
  ceremonyUntil = millis() + CEREMONY_MS;
  heartUntil = ceremonyUntil;  // cuori per tutta la durata dell'addio
  halSfx(SFX_BYE);
  save();
}

void Pet::startRunaway() {
  if (isEgg() || ceremony != CER_NONE || scambioPosto == SCAMBIO_ATTIVO) return;
  lastEnd = CER_RUNAWAY;
  ceremony = CER_RUNAWAY;
  ceremonyUntil = millis() + CEREMONY_MS;
  halSfx(SFX_BYE);
  save();
}

void Pet::release() {
  if (isEgg() || ceremony != CER_NONE || scambioPosto == SCAMBIO_ATTIVO) return;
  lastEnd = CER_RELEASE;
  ceremony = CER_RELEASE;
  ceremonyUntil = millis() + CEREMONY_MS;
  heartUntil = ceremonyUntil;
  halSfx(SFX_BYE);
  save();
}

void Pet::hatch() {
  speciesId = eggTarget;
  shiny = eggShiny;
  // geni dell'individuo: 90-110% per statistica (ogni allevamento è unico)
  geneAtk = 90 + random(21);
  geneDef = 90 + random(21);
  geneSpe = 90 + random(21);
  trAtk = trDef = trSpe = 0;
  berryKnown = false;
  bond = 0;          // legame, medaglie e nome sono dell'individuo
  bondToday = 0;
  medals = 0;
  newMedal = 0;
  medalUntil = 0;    // il banner di una medaglia del pet precedente non sopravvive alla schiusa
  nick[0] = 0;
  otId = 0;
  otName[0] = 0;
  farewellMinAge = 0;
  registerSpecies(speciesId);  // allevato = registrato nel pokedex
  checkMedals();     // nel caso nasca già in forma finale (leggendario)
  halSfx(SFX_HATCH);
  save();
}

// si verificano già le condizioni per evolvere? Ogni trascuratezza ritarda l'evoluzione
// di 1 livello, e inoltre deve essere ben curato in quel momento (nessuna statistica
// sotto 40). NON evolve da solo: lo attiva l'utente toccando il Pokémon (evolve()),
// perché veda la trasformazione.
bool Pet::canEvolveNow() const {
  if (isEgg() || sleeping || ceremony != CER_NONE || scambioPosto == SCAMBIO_ATTIVO) return false;
  const DexEntry &d = DEX_TBL[speciesId];
  if (d.evolvesTo == 0) return false;
  return level() >= (uint16_t)d.evolveLevel + careMistakes && lowestStat() >= 40;
}

void Pet::evolve() {
  if (!canEvolveNow()) return;
  const DexEntry &d = DEX_TBL[speciesId];
  prevSpeciesId = speciesId;
  int16_t next = d.evolvesTo;
  if (speciesId == DEX_EEVEE) {
    // ramo di Eevee: preferisce l'evoluzione che manca nel pokedex
    int16_t opts[3];
    int n = 0;
    for (int16_t b = 134; b <= 136; b++)
      if (!isRegistered(b)) opts[n++] = b;
    next = n > 0 ? opts[random(n)] : (int16_t)(134 + random(3));
  }
  speciesId = next;
  registerSpecies(speciesId);
  halSfx(SFX_EVOLVE);
  evolveUntil = millis() + EVOLVE_ANIM_MS;
  save();
}

void Pet::feed() {
  feedBerry(0);
}

void Pet::feedBerry(uint8_t color) {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  if (lovesBerry(color)) {
    fullness = clamp100(fullness + 35);
    joy = clamp100(joy + 10);
    heartUntil = millis() + HEART_MS;  // "le piace tantissimo!"
    berryKnown = true;                 // scoperto: si mostra nella scheda
    addBond(2);
  } else {
    fullness = clamp100(fullness + 25);
  }
  eatUntil = millis() + EAT_ANIM_MS;
  registerCare();
  save();
}

void Pet::feedCandy() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  fullness = clamp100(fullness + 10);
  joy = clamp100(joy + 12);
  weight = clamp100(weight + 12);  // i dolcetti presentano il conto
  eatUntil = millis() + EAT_ANIM_MS;
  registerCare();
  save();
}

void Pet::playResult(uint8_t score) {
  if (ceremony != CER_NONE || isEgg()) return;
  uint8_t v = trSpe + score / 5;  // giocare allena la velocità
  trSpe = v > 100 ? 100 : v;
  joy = clamp100(joy + 5 + (score > 15 ? 30 : score * 2));
  energy = dropTo(energy, 10 + score / 2, 5);
  fullness = dropTo(fullness, 5, 5);
  int burn = (int)weight - score * 2;  // l'esercizio brucia peso
  weight = burn > 0 ? burn : 0;
  if (score >= 5) heartUntil = millis() + HEART_MS;
  if (score > gameHi) gameHi = score;  // nuovo record
  addBond(2);
  registerCare();
  save();
}

// sacco da allenamento: i colpi allenano la forza. Restituisce l'aumento.
uint8_t Pet::trainStrength(uint16_t hits) {
  if (ceremony != CER_NONE || isEgg()) return 0;
  uint8_t gain = hits / 4;          // ~4 colpi = 1 punto di allenamento
  if (gain > 18) gain = 18;         // limite per sessione: la FUE si forgia a fuoco lento
  uint8_t antes = trAtk;
  uint16_t v = (uint16_t)trAtk + gain;
  trAtk = v > 100 ? 100 : (uint8_t)v;
  gain = trAtk - antes;             // quanto è entrato davvero: arrivati al 100 lo
                                    // schermo annunciava +18 anche se ne entravano meno
  energy = dropTo(energy, 12, 5);   // stanca
  fullness = dropTo(fullness, 5, 5);
  int burn = (int)weight - hits / 3;  // brucia anche peso
  weight = burn > 0 ? burn : 0;
  joy = clamp100(joy + 6);
  if (hits >= 20) heartUntil = millis() + HEART_MS;
  if (hits > strHi) strHi = hits;   // record di colpi
  addBond(2);
  registerCare();
  save();
  return gain;
}

// Ricompensa comune dei giochi a inclinazione: felicità, fatica, peso bruciato,
// cuore, legame e cura del giorno; il chiamante ha già escluso uovo e cerimonia.
void Pet::giocoInclinato(uint16_t punti) {
  uint16_t p = punti > 60 ? 60 : punti;  // oltre, fatica e felicità non crescono più
  joy = clamp100(joy + 5 + (p > 30 ? 30 : p));
  energy = dropTo(energy, 10 + p / 2, 5);
  fullness = dropTo(fullness, 5, 5);
  int burn = (int)weight - p * 2;
  weight = burn > 0 ? burn : 0;
  if (punti >= 10) heartUntil = millis() + HEART_MS;
  addBond(2);
  registerCare();
}

// Grotta Diglett: 4 punti = 1 di allenamento della DIFESA, al massimo 18 per partita come il
// sacco. Restituisce quanto è entrato davvero (fermo a 100).
uint8_t Pet::grottaResult(uint16_t punti) {
  if (ceremony != CER_NONE || isEgg()) return 0;
  uint16_t gain = punti / 4;
  if (gain > 18) gain = 18;
  uint8_t prima = trDef;
  uint16_t v = trDef + gain;
  trDef = v > 100 ? 100 : (uint8_t)v;
  if (punti > grottaHi) grottaHi = punti;
  giocoInclinato(punti);
  save();
  return trDef - prima;
}

void Pet::surfResult(uint16_t punti) {
  if (ceremony != CER_NONE || isEgg()) return;
  if (punti > surfHi) surfHi = punti;
  giocoInclinato(punti);
  save();
}

uint8_t Pet::spavento() {
  if (isEgg() || ceremony != CER_NONE || evolving() || spaventoInPausa()) return 0;
  uint8_t tolta = sleeping ? SPAVENTO_GIOIA_SONNO : SPAVENTO_GIOIA;
  if (tolta > joy) tolta = joy;
  joy -= tolta;
  spaventoFino = millis() + SPAVENTO_PAUSA_MS;
  if (!spaventoFino) spaventoFino = 1;  // 0 vuol dire "nessuna pausa"
  pendingSave = true;
  return tolta;
}

void Pet::setMovimento(bool on) {
  if (on == movimentoOn) return;
  movimentoOn = on;
  prefs.putBool(MOVIMENTO_KEY, on);
}

LottaMotivo Pet::lottaPerche() const {
  if (isEgg()) return LOT_UOVO;
  if (starterPick || ceremony != CER_NONE || evolving()) return LOT_OCCUPATO;
  if (sleeping) return LOT_DORME;
  return energy >= BATTLE_MIN_ENERGY ? LOT_OK : LOT_STANCO;
}

int16_t Pet::pickWild(uint8_t &lvl) const {
  int base = battleLevel();
  int l = base - 2 + (int)random(4);  // da 2 sotto a 1 sopra il pet
  lvl = (uint8_t)(l < 1 ? 1 : (l > 100 ? 100 : l));
  uint8_t biome = isEgg() ? 0 : DEX_TBL[speciesId].biome;
  // a questo livello la specie esiste già in natura? Non se la sua forma precedente
  // evolve più in alto (niente Venusaur al livello 10)
  auto ammessa = [&](int16_t d) {
    if (DEX_TBL[d].rarity == R_LEGENDARIO) return false;
    for (int16_t p = 1; p <= DEX_COUNT; p++)
      if (DEX_TBL[p].evolvesTo == d && DEX_TBL[p].evolveLevel > lvl) return false;
    return true;
  };
  uint8_t n = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (DEX_TBL[d].biome == biome && ammessa(d)) n++;
  if (n == 0) return 19;  // ogni bioma ne ha almeno una (la neve solo Jynx); 19 Rattata
  uint8_t k = (uint8_t)random(n);
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (DEX_TBL[d].biome == biome && ammessa(d) && k-- == 0) return d;
  return 19;
}

uint8_t Pet::battleWinsLeft() const {
  if (winsDay != today()) return BATTLE_WINS_PER_DAY;
  return winsToday >= BATTLE_WINS_PER_DAY ? 0 : BATTLE_WINS_PER_DAY - winsToday;
}

// La lotta stanca e fa venire fame (fuggire costa meno), con un minimo come il
// minigioco: da sola non porta mai a una trascuratezza. Età, trascuratezze, specie e
// cerimonie non si toccano. Non conta come cura del giorno: registerCare() salverebbe
// subito, e qui il salvataggio resta differito.
bool Pet::battleResult(uint8_t esito) {
  if (isEgg() || ceremony != CER_NONE) return false;
  bool fuga = esito == BTL_FUGA;
  energy = dropTo(energy, fuga ? 4 : 12, 5);
  fullness = dropTo(fullness, fuga ? 2 : 6, 5);
  bool premio = false;
  if (esito == BTL_VINTA) {
    uint32_t d = today();  // senza orologio è sempre il giorno 0: 5 premi in tutto
    if (winsDay != d) {
      winsDay = d;
      winsToday = 0;
    }
    if (winsToday < BATTLE_WINS_PER_DAY) {
      winsToday++;
      premio = true;
      joy = clamp100(joy + 15);
      trAtk = clamp100(trAtk + 2);
      trDef = clamp100(trDef + 2);
      addBond(3);
      heartUntil = millis() + HEART_MS;
    } else {
      joy = clamp100(joy + 3);  // vince lo stesso, ma senza premi
    }
  } else if (esito == BTL_PERSA) {
    joy = dropTo(joy, 8, 5);
  }
  pendingSave = true;
  return premio;
}

void Pet::linkBattleResult(uint8_t esito, bool attivoInSquadra) {
  if (esito == BTL_VINTA && linkWinsN < 0xFFFF) {
    linkWinsN++;
    linkWinsDirty = true;
  }
  if (attivoInSquadra && !isEgg() && esito != BTL_FUGA)
    addBond(esito == BTL_VINTA ? LINK_BOND_WIN : LINK_BOND_PLAY);
  pendingSave = true;
}

void Pet::play() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  joy = clamp100(joy + 25);
  energy = clamp100(energy - 10);
  fullness = clamp100(fullness - 5);
  heartUntil = millis() + HEART_MS;
  addBond(2);
  registerCare();
  save();
}

void Pet::toggleLight() {
  if (ceremony != CER_NONE) return;
  if (isEgg()) return;
  sleeping = !sleeping;
  save();
}

void Pet::clean() {
  if (ceremony != CER_NONE) return;
  poops = 0;
  hygiene = 100;
  addBond(1);
  registerCare();
  save();
}

void Pet::caress() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  joy = clamp100(joy + 5);
  heartUntil = millis() + HEART_MS;
  addBond(1);
  registerCare();
}

void Pet::eggTap() {
  if (!isEgg()) return;
  if (++eggTaps >= 3) hatch();
  else save();
}

PetMood Pet::mood() const {
  if (sleeping) return MOOD_SLEEPING;
  if (eating()) return MOOD_EATING;
  if (lowestStat() < 25) return MOOD_SAD;
  return MOOD_HAPPY;
}

void Pet::toSave(PetSave &s) const {
  s.fullness = fullness;
  s.joy = joy;
  s.energy = energy;
  s.hygiene = hygiene;
  s.poops = poops;
  s.weight = weight;
  s.geneAtk = geneAtk;
  s.geneDef = geneDef;
  s.geneSpe = geneSpe;
  s.trAtk = trAtk;
  s.trDef = trDef;
  s.trSpe = trSpe;
  s.berryKnown = berryKnown;
  s.shiny = shiny;
  s.eggShiny = eggShiny;
  s.starterPick = starterPick;
  s.ageMinutes = ageMinutes;
  s.speciesId = speciesId;
  s.eggTarget = eggTarget;
  s.eggTaps = eggTaps;
  s.careMistakes = careMistakes;
  s.sleeping = sleeping;
  s.lastEnd = lastEnd;
  memcpy(s.dexReg, dexReg, sizeof(s.dexReg));
  memcpy(s.dexShinyReg, dexShinyReg, sizeof(s.dexShinyReg));
  s.streak = streak;
  s.bestStreak = bestStreak;
  s.lastCareDay = lastCareDay;
  s.bond = bond;
  s.medals = medals;
  s.totalMedals = totalMedals;
  s.lastMilestone = lastMilestone;
  s.gameHi = gameHi;
  s.strHi = strHi;
  memcpy(s.nick, nick, sizeof(s.nick));
  s.winsToday = winsToday;
  s.winsDay = winsDay;
  s.colAsk = colAsk;
  s.colDay = colDay;
  s.farewellMinAge = farewellMinAge;
  s.otId = otId;
  memcpy(s.otName, otName, sizeof(s.otName));
  s.tradeFatto = tradeFatto;
  s.grottaHi = grottaHi;
  s.surfHi = surfHi;
}

void Pet::fromSave(const PetSave &s) {
  fullness = s.fullness;
  joy = s.joy;
  energy = s.energy;
  hygiene = s.hygiene;
  poops = s.poops;
  weight = s.weight;
  geneAtk = s.geneAtk;
  geneDef = s.geneDef;
  geneSpe = s.geneSpe;
  trAtk = s.trAtk;
  trDef = s.trDef;
  trSpe = s.trSpe;
  berryKnown = s.berryKnown;
  shiny = s.shiny;
  eggShiny = s.eggShiny;
  starterPick = s.starterPick;
  ageMinutes = s.ageMinutes;
  speciesId = s.speciesId;
  eggTarget = s.eggTarget;
  eggTaps = s.eggTaps;
  careMistakes = s.careMistakes;
  sleeping = s.sleeping;
  lastEnd = s.lastEnd;
  memcpy(dexReg, s.dexReg, sizeof(dexReg));
  memcpy(dexShinyReg, s.dexShinyReg, sizeof(dexShinyReg));
  streak = s.streak;
  bestStreak = s.bestStreak;
  lastCareDay = s.lastCareDay;
  bond = s.bond;
  medals = s.medals;
  totalMedals = s.totalMedals;
  lastMilestone = s.lastMilestone;
  gameHi = s.gameHi;
  strHi = s.strHi;
  memcpy(nick, s.nick, sizeof(nick));
  winsToday = s.winsToday;
  winsDay = s.winsDay;
  colAsk = s.colAsk;
  colDay = s.colDay;
  farewellMinAge = s.farewellMinAge;
  otId = s.otId;
  memcpy(otName, s.otName, sizeof(otName));
  tradeFatto = s.tradeFatto;
  grottaHi = s.grottaHi;
  surfHi = s.surfHi;
}

void Pet::save() {
  ticksSinceSave = 0;
  pendingSave = false;
  // "seen" prima del blob: se la corrente salta in mezzo si perdono i minuti non ancora
  // salvati, come in ogni blackout, ma non si contano due volte
  if (lastSeenEpoch) prefs.putUInt("seen", lastSeenEpoch);
  PetSave s;
  toSave(s);
  s.crc = saveCrc32((const uint8_t *)&s, sizeof(s) - sizeof(s.crc));
  prefs.putBytes(PET_SAVE_KEY, &s, sizeof(s));
  if (linkWinsDirty) {
    prefs.putUShort(LINK_WINS_KEY, linkWinsN);
    linkWinsDirty = false;
  }
  if (linkTradesDirty) {
    prefs.putUShort(LINK_TRADES_KEY, linkTradesN);
    linkTradesDirty = false;
  }
}

// chiavi dello stato fino alla 1.18 ("spec"/"eggT": indici di flash ancora più vecchi),
// tolte dopo la migrazione al blob
static const char *const LEGACY_KEYS[] = {
  "full", "joy", "ene", "hyg", "poop", "wgt", "gatk", "gdef", "gspe", "tatk", "tdef",
  "tspe", "bk", "shy", "eshy", "stpk", "dexsh", "age", "dexn", "eggT2", "crack", "mist",
  "sleep", "lend", "dexreg", "strk", "bstrk", "cday", "bond", "medal", "tmedal", "mstone",
  "ghi", "shi", "nick", "spec", "eggT",
};

static bool hasLegacyKeys(Preferences &prefs) {
  return prefs.isKey("dexn") || prefs.isKey("spec");
}

// Legge il blob dello stato in `s`: il prefisso noto, il resto ai default di PetSave
// (`version` e `size` restano quelli del blob, per le migrazioni di significato).
// 1 = valido, 0 = assente, -1 = illeggibile (lunghezza), -2 = CRC errato: in questo caso
// `s` è riempita lo stesso, ma i suoi valori non sono affidabili.
static int readSave(Preferences &prefs, PetSave &s) {
  if (!prefs.isKey(PET_SAVE_KEY)) return 0;
  uint8_t raw[PET_SAVE_MAX];
  size_t len = prefs.getBytesLength(PET_SAVE_KEY);
  const size_t head = offsetof(PetSave, fullness), tail = sizeof(s.crc);
  if (len < head + tail || len > sizeof(raw)) return -1;
  if (prefs.getBytes(PET_SAVE_KEY, raw, len) != len) return -1;
  if ((size_t)(raw[2] | (raw[3] << 8)) != len) return -1;
  size_t known = sizeof(PetSave) - tail;
  memcpy(&s, raw, len - tail < known ? len - tail : known);
  uint32_t crc = 0;
  for (size_t i = 0; i < tail; i++) crc |= (uint32_t)raw[len - tail + i] << (8 * i);
  return saveCrc32(raw, len - tail) == crc ? 1 : -2;
}

// Unico punto delle migrazioni: blob (di qualunque versione) o chiavi legacy, poi
// sanitize() in ogni caso.
void Pet::load() {
  PetSave s;
  int st = readSave(prefs, s);
  bool migrate = false, lost = false;
  if (st > 0) {
    fromSave(s);
  } else if (st == 0 || hasLegacyKeys(prefs)) {
    if (st < 0) Serial.println("save: blob rovinato, recupero le chiavi legacy");
    loadLegacy();
    migrate = true;
  } else {
    Serial.printf("save: blob rovinato (%d), uovo nuovo\n", st);
    PetSave fresh;  // tutto ai default: un uovo
    if (st == -2) {  // la lunghezza torna: si prova a tenere il pokédex, sanitize lo ripulisce
      memcpy(fresh.dexReg, s.dexReg, sizeof(fresh.dexReg));
      memcpy(fresh.dexShinyReg, s.dexShinyReg, sizeof(fresh.dexShinyReg));
    }
    fromSave(fresh);
    lost = true;
  }
  if (geneAtk == 0) {  // Pokémon precedente ai geni: tiro unico adesso
    geneAtk = 90 + random(21);
    geneDef = 90 + random(21);
    geneSpe = 90 + random(21);
  }
  sanitize();  // prima di registerSpecies: un dexn fuori intervallo scriverebbe oltre dexReg
  colCarica();
  linkWinsN = prefs.getUShort(LINK_WINS_KEY, 0);
  linkWinsDirty = false;
  linkTradesN = prefs.getUShort(LINK_TRADES_KEY, 0);
  linkTradesDirty = false;
  // domanda della collezione in sospeso: la cerimonia riparte già finita, senza tempo
  if (colAsk) {
    ceremony = lastEnd == CER_RELEASE ? CER_RELEASE : CER_FAREWELL;
    ceremonyUntil = 0;
  }
  // semina: il Pokémon attuale conta come allevato (salvataggi vecchi)
  if (speciesId >= 1) registerSpecies(speciesId);
  if (lost) {
    newEgg();  // salva
  } else if (migrate) {
    save();
    PetSave check;
    if (readSave(prefs, check) == 1) {  // blob scritto e rileggibile: le chiavi vecchie vanno
      for (const char *k : LEGACY_KEYS) prefs.remove(k);
      Serial.println("save: chiavi legacy migrate nel blob");
    }
  }
}

void Pet::loadLegacy() {
  fullness = prefs.getUChar("full", 80);
  joy = prefs.getUChar("joy", 80);
  energy = prefs.getUChar("ene", 80);
  hygiene = prefs.getUChar("hyg", 100);
  poops = prefs.getUChar("poop", 0);
  weight = prefs.getUChar("wgt", 0);
  geneAtk = prefs.getUChar("gatk", 0);
  geneDef = prefs.getUChar("gdef", 0);
  geneSpe = prefs.getUChar("gspe", 0);
  trAtk = prefs.getUChar("tatk", 0);
  trDef = prefs.getUChar("tdef", 0);
  trSpe = prefs.getUChar("tspe", 0);
  berryKnown = prefs.getBool("bk", false);
  shiny = prefs.getBool("shy", false);
  eggShiny = prefs.getBool("eshy", false);
  starterPick = prefs.getBool("stpk", false);
  prefs.getBytes("dexsh", dexShinyReg, sizeof(dexShinyReg));
  ageMinutes = prefs.getUInt("age", 0);
  if (prefs.isKey("dexn")) {
    speciesId = prefs.getShort("dexn", -1);
    eggTarget = prefs.getShort("eggT2", 4);
  } else {
    // migrazione dalla versione con indici di flash (0-8)
    static const uint8_t OLD2DEX[9] = { 4, 5, 6, 1, 2, 3, 7, 8, 9 };
    int8_t old = prefs.getChar("spec", -1);
    speciesId = (old >= 0 && old < 9) ? OLD2DEX[old] : -1;
    int8_t oldT = prefs.getChar("eggT", 0);
    eggTarget = (oldT >= 0 && oldT < 9) ? OLD2DEX[oldT] : 4;
  }
  eggTaps = prefs.getUChar("crack", 0);
  careMistakes = prefs.getUChar("mist", 0);
  sleeping = prefs.getBool("sleep", false);
  lastEnd = prefs.getUChar("lend", CER_NONE);
  prefs.getBytes("dexreg", dexReg, sizeof(dexReg));
  streak = prefs.getUShort("strk", 0);
  bestStreak = prefs.getUShort("bstrk", 0);
  lastCareDay = prefs.getUInt("cday", 0);
  bond = prefs.getUChar("bond", 0);
  medals = prefs.getUShort("medal", 0);
  totalMedals = prefs.getUShort("tmedal", 0);
  lastMilestone = prefs.getUShort("mstone", 0);
  gameHi = prefs.getUShort("ghi", 0);
  strHi = prefs.getUShort("shi", 0);
  prefs.getString("nick", nick, sizeof(nick));
}

void Pet::sanitize() {
  fullness = clamp100(fullness);
  joy = clamp100(joy);
  energy = clamp100(energy);
  hygiene = clamp100(hygiene);
  weight = clamp100(weight);
  trAtk = clamp100(trAtk);
  trDef = clamp100(trDef);
  trSpe = clamp100(trSpe);
  bond = clamp100(bond);
  if (poops > 3) poops = 3;
  auto gene = [](uint8_t g) -> uint8_t { return g < 90 ? 90 : (g > 110 ? 110 : g); };
  geneAtk = gene(geneAtk);
  geneDef = gene(geneDef);
  geneSpe = gene(geneSpe);
  if (lastEnd > CER_RELEASE) lastEnd = CER_NONE;
  if (ceremony > CER_RELEASE) ceremony = CER_NONE;
  if (eggTaps > 2) eggTaps = 2;
  if (bestStreak < streak) bestStreak = streak;
  medals &= (1 << MED_COUNT) - 1;
  nick[sizeof(nick) - 1] = '\0';
  otName[sizeof(otName) - 1] = '\0';
  if (colAsk && isEgg()) colAsk = false;
  // la bitmap ha 152 bit per 151 specie: l'ultimo non deve contare
  dexReg[sizeof(dexReg) - 1] &= 0x7F;
  dexShinyReg[sizeof(dexShinyReg) - 1] &= 0x7F;
  if (prevSpeciesId < -1 || prevSpeciesId == 0 || prevSpeciesId > DEX_COUNT) prevSpeciesId = -1;
  // dopo i campi sopra: newEgg() e pickEggSpecies() leggono lastEnd, serie, legame e dexReg
  if (eggTarget < 1 || eggTarget > DEX_COUNT) eggTarget = pickEggSpecies();
  if (speciesId < -1 || speciesId == 0 || speciesId > DEX_COUNT) newEgg();
}

// ---------------------------------------------------------------- collezione

PetRitratto Pet::ritratto() const {
  PetRitratto r;
  r.specie = speciesId;
  r.shiny = shiny;
  r.ageMinutes = ageMinutes;
  r.geneAtk = geneAtk;
  r.geneDef = geneDef;
  r.geneSpe = geneSpe;
  r.trAtk = trAtk;
  r.trDef = trDef;
  r.trSpe = trSpe;
  r.bond = bond;
  r.medals = medals;
  memcpy(r.nick, nick, sizeof(r.nick));
  r.otId = otId;
  memcpy(r.otNome, otName, sizeof(r.otNome));
  return r;
}

void Pet::daRitratto(const PetRitratto &r) {
  speciesId = r.specie;
  prevSpeciesId = -1;
  shiny = r.shiny;
  ageMinutes = r.ageMinutes;
  geneAtk = r.geneAtk;
  geneDef = r.geneDef;
  geneSpe = r.geneSpe;
  trAtk = r.trAtk;
  trDef = r.trDef;
  trSpe = r.trSpe;
  bond = r.bond;
  medals = r.medals;
  memcpy(nick, r.nick, sizeof(nick));
  otId = r.otId;
  memcpy(otName, r.otNome, sizeof(otName));
  // valori neutri, come un uovo appena schiuso
  fullness = 80;
  joy = 80;
  energy = 80;
  hygiene = 100;
  poops = 0;
  weight = 0;
  berryKnown = false;
  careMistakes = 0;
  mistakeCooldown = 0;
  neglectTicks = 0;
  goodTicks = 0;
  sleeping = false;
  ceremony = CER_NONE;
  colAsk = false;
  starterPick = false;
  eggTaps = 0;
  eatUntil = heartUntil = evolveUntil = ceremonyUntil = 0;
  medalUntil = milestoneUntil = 0;
  newMedal = 0;
  evoDeclinedLv = 0;
  farDeclinedAge = 0;
  farewellMinAge = 0;
  sanitize();
  registerSpecies(speciesId);  // anche shiny
}

bool Pet::colEntra() const {
  return (ceremony == CER_FAREWELL || ceremony == CER_RELEASE) && !isEgg() &&
         DEX_TBL[speciesId].evolvesTo == 0;
}

void Pet::colCarica() {
  colN = 0;
  if (!prefs.isKey(COL_KEY)) return;
  uint8_t raw[2 + COL_POSTI * 100];  // spazio anche per ritratti di versioni future
  size_t len = prefs.getBytesLength(COL_KEY);
  if (len < 2 || len > sizeof(raw) || prefs.getBytes(COL_KEY, raw, len) != len || raw[0] == 0) {
    Serial.println("col: blob illeggibile, collezione vuota");
    return;
  }
  size_t pos = 2;
  for (uint8_t i = 0; i < raw[1] && colN < COL_POSTI && pos < len; i++) {
    size_t n = ritrattoLeggi(raw + pos, len - pos, &col[colN]);
    if (n) {
      colN++;
      pos += n;
      continue;
    }
    // rovinato: se la sua lunghezza è credibile si salta solo lui
    if (pos + 1 >= len || raw[pos + 1] < RIT_BYTES) break;
    Serial.printf("col: ritratto %u rovinato, perso\n", i);
    pos += raw[pos + 1];
  }
}

void Pet::colSalva() {
  if (colN == 0) {
    prefs.remove(COL_KEY);
    return;
  }
  uint8_t raw[2 + COL_POSTI * RIT_BYTES];
  raw[0] = COL_VERSIONE;
  raw[1] = colN;
  size_t pos = 2;
  for (uint8_t i = 0; i < colN; i++) pos += ritrattoScrivi(col[i], raw + pos, sizeof(raw) - pos);
  prefs.putBytes(COL_KEY, raw, pos);
}

bool Pet::colDeposita(const PetRitratto &r) {
  if (colPiena() || r.specie < 1 || r.specie > DEX_COUNT) return false;
  col[colN++] = r;
  colSalva();
  return true;
}

bool Pet::colLibera(uint8_t i) {
  if (!colLiberabile(i)) return false;
  for (uint8_t k = i; k + 1 < colN; k++) col[k] = col[k + 1];
  col[--colN] = PetRitratto();
  colSalva();
  return true;
}

bool Pet::colConserva(int8_t libera) {
  if (!colAsk) return false;
  if (colPiena()) {
    if (libera < 0 || libera >= colN || scambioPosto >= 0) return false;
    // senza scrivere due volte: il posto liberato lo prende il pet che entra in fondo
    for (uint8_t k = libera; k + 1 < colN; k++) col[k] = col[k + 1];
    colN--;
  }
  col[colN++] = ritratto();
  colSalva();
  newEgg();  // salva
  return true;
}

void Pet::colRinuncia() {
  if (colAsk) newEgg();
}

ColMotivo Pet::colPerche(uint8_t i) const {
  if (i >= colN) return COL_POSTO;
  if (sleeping || ceremony != CER_NONE || colAsk || evolving() || scambioPosto != SCAMBIO_NESSUNO)
    return COL_OCCUPATO;
  if (colDay && colDay == today() + 1) return COL_OGGI;
  if (isEgg()) return COL_OK;
  if (DEX_TBL[speciesId].evolvesTo != 0) return COL_NON_FINALE;
  if (lowestStat() < COL_MIN_BARRA || neglectTicks > 0) return COL_NON_IN_FORMA;
  return COL_OK;
}

bool Pet::colRiprendi(uint8_t i) {
  if (colPerche(i) != COL_OK) return false;
  PetRitratto r = col[i];
  if (isEgg()) {  // l'uovo non è ancora un Pokémon: si scarta
    for (uint8_t k = i; k + 1 < colN; k++) col[k] = col[k + 1];
    col[--colN] = PetRitratto();
  } else {        // scambio: l'attivo prende il posto del ripreso
    col[i] = ritratto();
  }
  daRitratto(r);
  // il nuovo addio aspetta un altro ciclo intero: senza, un pet già vecchio potrebbe dire
  // addio subito e la benedizione dell'uovo si ricaricherebbe a ripetizione
  farewellMinAge = ageMinutes + FAREWELL_AGE_MIN;
  colDay = today() + 1;  // senza orologio è sempre il giorno 0: un RIPRENDI in tutto
  colSalva();
  save();
  return true;
}

// ---------------------------------------------------------------- scambio

ScambioMotivo Pet::scambioPerche(int8_t posto) const {
  if (scambioPosto != SCAMBIO_NESSUNO) return SCB_BLOCCATO;
  if (ceremony != CER_NONE || colAsk || evoScDa) return SCB_OCCUPATO;
  if (posto == SCAMBIO_ATTIVO) {
    if (isEgg()) return SCB_UOVO;
    return sleeping || evolving() ? SCB_OCCUPATO : SCB_OK;
  }
  return posto >= 0 && posto < colN ? SCB_OK : SCB_POSTO;
}

PetRitratto Pet::scambioRitratto(int8_t posto) const {
  if (posto == SCAMBIO_ATTIVO) return ritratto();
  return colRitratto(posto >= 0 ? (uint8_t)posto : 0);
}

PetRitratto Pet::scambioDaRicevere(const PetRitratto &ricevuto, uint32_t partner,
                                   const char *nome) {
  PetRitratto r = ricevuto;
  r.bond = 0;  // il legame riparte da 0
  if (!r.otId) {  // l'ha allevato chi lo manda: "AO: <nome>"
    r.otId = partner;
    memset(r.otNome, 0, sizeof(r.otNome));
    if (nome) strncpy(r.otNome, nome, sizeof(r.otNome) - 1);
  } else if (r.otId == trainerId()) {  // torna a casa
    r.otId = 0;
    memset(r.otNome, 0, sizeof(r.otNome));
  }
  if (r.specie >= 1 && r.specie <= DEX_COUNT && DEX_TBL[r.specie].tradeEvo)
    r.specie = DEX_TBL[r.specie].evolvesTo;
  return r;
}

// stessi byte: per i pet congelati della collezione
static bool stessoRitratto(const PetRitratto &a, const PetRitratto &b) {
  uint8_t x[RIT_BYTES], y[RIT_BYTES];
  return ritrattoScrivi(a, x, sizeof(x)) && ritrattoScrivi(b, y, sizeof(y)) &&
         !memcmp(x, y, sizeof(x));
}

// lo stesso individuo, per il pet attivo: invecchia e si allena anche durante lo scambio
static bool stessoIndividuo(const PetRitratto &a, const PetRitratto &b) {
  return a.specie == b.specie && a.shiny == b.shiny && a.geneAtk == b.geneAtk &&
         a.geneDef == b.geneDef && a.geneSpe == b.geneSpe && a.otId == b.otId &&
         !memcmp(a.otNome, b.otNome, sizeof(a.otNome));
}

bool Pet::riceviScambio(int8_t posto, const PetRitratto &offerto, const PetRitratto &ricevuto,
                        uint32_t tradeId, uint32_t partner, const char *nome) {
  if (scambioApplicato(tradeId)) return true;
  if (!tradeId || ricevuto.specie < 1 || ricevuto.specie > DEX_COUNT) return false;
  PetRitratto r = scambioDaRicevere(ricevuto, partner, nome);
  if (posto == SCAMBIO_ATTIVO) {
    if (isEgg() || !stessoIndividuo(ritratto(), offerto)) return false;
    daRitratto(r);
    // come per un pet ripreso dalla collezione: un pet vecchio ricevuto non dice addio
    // subito, così due schede non si ricaricano a vicenda la benedizione dell'uovo
    farewellMinAge = ageMinutes + FAREWELL_AGE_MIN;
  } else if (posto >= 0 && posto < colN) {
    // dopo una ricezione interrotta tra le due scritture il ricevuto è già lì
    if (!stessoRitratto(col[posto], r)) {
      if (!stessoRitratto(col[posto], offerto)) return false;
      col[posto] = r;
      colSalva();
    }
  } else {
    return false;
  }
  registerSpecies(ricevuto.specie, r.shiny);  // come le esclusive di versione
  registerSpecies(r.specie, r.shiny);
  tradeFatto = tradeId;
  if (linkTradesN < 0xFFFF) {  // scritto da save() subito dopo il blob
    linkTradesN++;
    linkTradesDirty = true;
  }
  save();
  evoScDa = r.specie != ricevuto.specie ? ricevuto.specie : 0;
  evoScPosto = evoScDa ? posto : SCAMBIO_NESSUNO;
  return true;
}

void Pet::annullaScambio(int8_t posto, const PetRitratto &offerto, const PetRitratto &ricevuto,
                         uint32_t partner, const char *nome) {
  if (posto < 0 || posto >= colN) return;
  if (stessoRitratto(col[posto], scambioDaRicevere(ricevuto, partner, nome)) &&
      !stessoRitratto(col[posto], offerto)) {
    col[posto] = offerto;
    colSalva();
  }
}

void Pet::avviaEvoScambio() {
  if (!evoScDa) return;
  if (evoScPosto == SCAMBIO_ATTIVO && !isEgg()) {
    prevSpeciesId = evoScDa;
    halSfx(SFX_EVOLVE);
    evolveUntil = millis() + EVOLVE_ANIM_MS;
  }
  evoScDa = 0;
  evoScPosto = SCAMBIO_NESSUNO;
}
