// Console seriale: provisioning della SD, salvataggi e comandi di debug.

#include "ui_common.h"
#include "ota.h"

// ---------- sensore di movimento (MOTO) ----------

static const char *const NOMI_MODO[] = {"SPENTO", "VEGLIA", "GIOCO"};
static uint32_t motoFino = 0, motoRiga = 0;  // millis(): fine delle righe, prossima riga
static int8_t motoRipristina = -1;           // il modo da rimettere alla fine, -1 = nessuno

static void motoStato() {
  HalMotoDiag d;
  halMotoDiag(&d);
  Serial.printf("moto presente=%d indirizzo=0x%02X modo=%s letture=%lu persi=%lu errori=%lu "
                "scossoni=%lu colpetti=%lu\n",
                halMotoPresente() ? 1 : 0, d.indirizzo, NOMI_MODO[d.modo % 3],
                (unsigned long)d.letture, (unsigned long)d.persi, (unsigned long)d.erroriI2c,
                (unsigned long)d.scossoni, (unsigned long)d.colpetti);
}

// Per 5 s dopo MOTO: una riga con la gravità ogni 250 ms e gli eventi appena arrivano (li
// consuma: in quei 5 s non arrivano al gioco). Dal loop, senza bloccarlo.
static void motoRighe() {
  if (!motoFino) return;
  uint32_t ora = millis();
  uint8_t ev = halMotoEventi();
  if (ev & HAL_EV_SCOSSONE) Serial.println("moto evento SCOSSONE");
  if (ev & HAL_EV_COLPETTI) Serial.println("moto evento COLPETTI");
  if ((int32_t)(ora - motoRiga) >= 0) {
    motoRiga = ora + 250;
    float x, y, z;
    if (halMotoGravita(&x, &y, &z))
      Serial.printf("moto g x=%+.2f y=%+.2f z=%+.2f\n", x, y, z);
    else
      Serial.println("moto g -");
  }
  if ((int32_t)(ora - motoFino) >= 0) {
    motoFino = 0;
    if (motoRipristina >= 0) halMotoModo((uint8_t)motoRipristina);
    motoRipristina = -1;
    motoStato();
    Serial.println("DONE");
  }
}

bool motoConsoleAttivo() { return motoFino != 0; }

// MOTO [SPENTO|VEGLIA|GIOCO]: lo stato e 5 s di righe. Senza modo, un sensore spento si
// accende in GIOCO per i 5 s e poi torna spento; con il modo, quello vale per i 5 s. Dopo,
// il loop() rimette il modo della schermata.
static void motoComando(const String &arg) {
  if (motoFino) {
    Serial.println("ERR (MOTO in corso)");
    return;
  }
  int modo = -1;
  for (int i = 0; i < 3; i++)
    if (arg == NOMI_MODO[i]) modo = i;
  if (arg.length() && modo < 0) {
    Serial.println("ERR (MOTO [SPENTO|VEGLIA|GIOCO])");
    return;
  }
  if (modo >= 0) {
    halMotoModo((uint8_t)modo);
  } else if (halMotoPresente() && halMotoModoAttuale() == HAL_MOTO_SPENTO) {
    motoRipristina = HAL_MOTO_SPENTO;
    halMotoModo(HAL_MOTO_GIOCO);
  }
  motoStato();
  halMotoEventi();  // quelli vecchi non contano
  motoFino = millis() + 5000;
  if (!motoFino) motoFino = 1;
  motoRiga = millis();
}

// ---------- console seriale (provisioning SD + debug) ----------

void handleSerial() {
  motoRighe();
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;
  if (sdSerialCommand(line)) return;
  if (saveSerialCommand(line, pet)) {  // EXPORT, BACKUP, IMPORT
    // sostituisce anche un messaggio d'errore rimasto aperto (non una conferma: lì IMPORT
    // risponde OCCUPATO)
    if (saveDlg != 1 && saveSdAttesa() == SAVE_DA_SERIALE) saveDlgApri(SAVE_OK, SAVE_DA_SERIALE);
    return;
  }

  // i comandi che alterano la partita restano bloccati finche' non arriva DEBUG ON, che
  // vale fino al riavvio: la porta e' la stessa che apre l'installer web. Sempre attivi:
  // il protocollo SD e i salvataggi (sopra) e le letture HEALTH, STATS, REG, TIME, EGGS,
  // BEEP, AUDIO, MOTO, COL, LINK, OTA. Anche TIME <epoch>: fa quello che fa l'OK dell'orologio (data e ora,
  // senza tempo offline) e lo usa l'installer web per impostare la data.
  static bool debugOn = false;
  if (line == "DEBUG ON") {
    debugOn = true;
    Serial.println("DONE");
    return;
  }
  bool alwaysOn = line == "HEALTH" || line == "STATS" || line == "REG" || line == "TIME" ||
                  line == "EGGS" || line == "BEEP" || line == "AUDIO" || line == "COL" ||
                  line == "MOTO" || line.startsWith("MOTO ") ||
                  line == "LINK" || line == "PWR" || line == "TOCCHI" || line.startsWith("TIME ") ||
                  line == "OTA";
  if (!debugOn && !alwaysOn) {
    Serial.println("ERR (comando di debug: prima DEBUG ON)");
    return;
  }

  if (line == "HATCH") {
    pet.eggTap(); pet.eggTap(); pet.eggTap();
    Serial.println("DONE");
  } else if (line.startsWith("SPEC ")) {
    int n = line.substring(5).toInt();
    if (n >= 1 && n <= DEX_COUNT) {
      pet.prevSpeciesId = pet.speciesId;
      pet.speciesId = n;
      Serial.printf("specie #%d %s\n", n, DEX_TBL[n].name);
    }
    Serial.println("DONE");
  } else if (line.startsWith("LVL ")) {
    pet.ageMinutes = (uint32_t)line.substring(4).toInt() * MINUTES_PER_LEVEL;
    Serial.println("DONE");
  } else if (line.startsWith("TIME ")) {
    uint32_t e = (uint32_t)line.substring(5).toInt();
    if (e < 1735689600UL) {  // prima del 2025: riga sbagliata, non un orologio da azzerare
      Serial.println("ERR (TIME <epoch locale>, dal 2025)");
      return;
    }
    halRtcSetEpoch(e);
    pet.setClock(e);
    Serial.printf("rtc=%u\n", halRtcEpoch());
    Serial.println("DONE");
  } else if (line.startsWith("RTCSET ")) {  // solo RTC (simulare spegnimenti nei test)
    halRtcSetEpoch((uint32_t)line.substring(7).toInt());
    Serial.printf("rtc=%u\n", halRtcEpoch());
    Serial.println("DONE");
  } else if (line == "TIME") {
    Serial.printf("rtc=%u\n", halRtcEpoch());
    Serial.println("DONE");
  } else if (line == "GAL") {
    goScreen(gScreen == SCR_GALLERY ? SCR_MAIN : SCR_GALLERY);
    Serial.println("DONE");
  } else if (line.startsWith("BATTLE ")) {  // BATTLE <dex> [lvl]: lvl di default = il pet
    String arg = line.substring(7);
    arg.trim();
    int sp = arg.indexOf(' ');
    int dex = arg.toInt();
    int lv = sp > 0 ? arg.substring(sp + 1).toInt() : (pet.level() < 100 ? pet.level() : 100);
    if (lv < 1 || lv > 100 || !startBattle(dex, (uint8_t)lv)) {
      Serial.println("ERR (dex 1-151, lvl 1-100; non da uovo ne' in cerimonia)");
      return;
    }
    Serial.printf("battaglia contro #%d %s Nv.%d\n", dex, DEX_TBL[dex].name, lv);
    Serial.println("DONE");
  } else if (line.startsWith("PP ")) {  // PP <mossa 1-4, 0 = tutte> <n>: in battaglia con la CPU
    String arg = line.substring(3);
    arg.trim();
    int sp = arg.indexOf(' ');
    int m = arg.toInt(), n = sp > 0 ? arg.substring(sp + 1).toInt() : -1;
    if (m < 0 || m > 4 || n < 0 || n > 255 || !battleDbgPp((uint8_t)m, (uint8_t)n)) {
      Serial.println("ERR (PP <mossa 0-4> <n>, in una battaglia contro la CPU)");
      return;
    }
    Serial.println("DONE");
  } else if (line == "EGGS") {
    // simula 20 tiri di uovo (non cambia lo stato del gioco)
    for (int i = 0; i < 20; i++) {
      int16_t d = pet.pickEggSpecies();
      Serial.printf("%d:%s(r%u) ", d, DEX_TBL[d].name, DEX_TBL[d].rarity);
    }
    Serial.println();
    Serial.println("DONE");
  } else if (line == "SHINY") {  // alterna shiny dell'attuale (test)
    pet.shiny = !pet.shiny;
    Serial.printf("shiny=%d\n", pet.shiny);
    Serial.println("DONE");
  } else if (line.startsWith("NICK ")) {
    pet.rename(line.substring(5).c_str());
    Serial.printf("nick=%s\n", pet.nick);
    Serial.println("DONE");
  } else if (line.startsWith("TOCCA ")) {  // TOCCA x y: un tocco, come il dito (copione delle prove)
    int x = -1, y = -1;
    sscanf(line.c_str() + 6, "%d %d", &x, &y);
    if (x < 0 || y < 0 || x >= 466 || y >= 466) {
      Serial.println("ERR (TOCCA x y, 0-465)");
      return;
    }
    screenOff = false;
    lastInteract = millis();
    onTap((int16_t)x, (int16_t)y);
    Serial.println("DONE");
  } else if (line == "CASA") {  // torna alla schermata principale (copione delle prove)
    Serial.printf("schermata=%d\n", (int)gScreen);
    screenOff = false;
    lastInteract = millis();
    goScreen(SCR_MAIN);
    syncScreen();  // la scelta dell'iniziale o la cerimonia prendono il posto della principale
    Serial.println("DONE");
  } else if (line.startsWith("SCORRI ")) {  // SCORRI SU|GIU|SINISTRA|DESTRA, nel verso del dito
    String d = line.substring(7);
    d.trim();
    screenOff = false;
    lastInteract = millis();
    if (d == "SU") onSwipeV(-1);
    else if (d == "GIU") onSwipeV(1);
    else if (d == "SINISTRA") onSwipe(-1);
    else if (d == "DESTRA") onSwipe(1);
    else {
      Serial.println("ERR (SCORRI SU|GIU|SINISTRA|DESTRA)");
      return;
    }
    Serial.println("DONE");
  } else if (line.startsWith("BARRE ")) {  // BARRE <cibo> <gioia> <energia> <igiene>, 0-100
    int v[4] = { -1, -1, -1, -1 };
    sscanf(line.c_str() + 6, "%d %d %d %d", &v[0], &v[1], &v[2], &v[3]);
    if (v[0] < 0 || v[1] < 0 || v[2] < 0 || v[3] < 0) {
      Serial.println("ERR (BARRE cibo gioia energia igiene, 0-100)");
      return;
    }
    pet.dbgBarre((uint8_t)min(v[0], 100), (uint8_t)min(v[1], 100), (uint8_t)min(v[2], 100),
                 (uint8_t)min(v[3], 100));
    Serial.printf("com=%u fel=%u ene=%u lim=%u\n", pet.fullness, pet.joy, pet.energy, pet.hygiene);
    Serial.println("DONE");
  } else if (line == "CAREDAY") {  // simula un nuovo giorno curato (test)
    pet.setClock(pet.lastSeenEpoch + 86400);
    pet.caress();
    Serial.printf("streak=%u bond=%u medals=0x%X\n", pet.streak, pet.bond, pet.medals);
    Serial.println("DONE");
  } else if (line == "BYE") {
    pet.startFarewell();
    Serial.println("DONE");
  } else if (line == "RUN") {
    pet.startRunaway();
    Serial.println("DONE");
  } else if (line == "TOCCHI") {  // diagnostica: ogni gesto riconosciuto sulla seriale
    gLogTocchi = !gLogTocchi;
    Serial.printf("tocchi=%d\n", gLogTocchi ? 1 : 0);
    Serial.println("DONE");
  } else if (line == "PWR") {  // come la pressione breve del pulsante PWR: schermo on/off
    screenOff = !screenOff;
    if (!screenOff) lastInteract = millis();
    Serial.printf("schermo=%s\n", screenOff ? "spento" : "acceso");
    Serial.println("DONE");
  } else if (line == "BEEP") {
    halSfx(SFX_HATCH);  // test audio
    Serial.println("DONE");
  } else if (line == "AUDIO") {  // uscita audio; carico e blocco più lento dall'ultimo AUDIO
    HalAudioDiag d;
    halAudioDiag(&d);
    Serial.printf("fs=%lu%s vol=%u mus=%d amp=%d blocco=%u carico=%u.%u%% max=%luus "
                  "blocchi=%lu ritardi=%lu sfx=%lu ultimo=%d\n",
                  (unsigned long)d.fs, d.riserva ? "(riserva)" : "", (unsigned)halVolume(),
                  halMusicaAccesa() ? 1 : 0, d.amplificatore ? 1 : 0, (unsigned)d.bloccoCampioni,
                  (unsigned)(d.caricoPm / 10), (unsigned)(d.caricoPm % 10),
                  (unsigned long)d.maxUs, (unsigned long)d.blocchi, (unsigned long)d.ritardi,
                  (unsigned long)d.effetti, d.ultimoEffetto == 0xFF ? -1 : (int)d.ultimoEffetto);
    Serial.println("DONE");
  } else if (line == "MOTO" || line.startsWith("MOTO ")) {
    motoComando(line.length() > 5 ? line.substring(5) : String());  // DONE dopo i 5 s
  } else if (line == "ABANDON") {
    pet.dbgRunawayReady();  // forza lo stato "pronto a fuggire" (test del pulsante)
    Serial.println("DONE");
  } else if (line == "WIPE") {
    Serial.println("ERR (conferma con WIPE YES)");
  } else if (line == "WIPE YES") {
    pet.factoryReset();     // cancella NVS e riavvia -> partita nuova (scelta dell'iniziale)
    Serial.println("DONE");
    delay(100);
    halRestart();
  } else if (line == "REG") {
    Serial.printf("pokedex %u/151:", pet.registeredCount());
    for (int i = 1; i <= 151; i++)
      if (pet.isRegistered(i)) Serial.printf(" %d", i);
    Serial.println();
    Serial.println("DONE");
  } else if (line == "HEALTH") {
    Serial.printf("up=%lus heap=%u min=%u psram=%u sd=%d mon=%d rst=%s\n",
                  (unsigned long)(millis() / 1000), halFreeHeap(),
                  halMinFreeHeap(), halFreePsram(), sdReady, pmd.loaded || mon.loaded,
                  halMotivoRiavvio());
    Serial.println("DONE");
  } else if (line == "LINK") {  // collegamento: stato, peer, contatori
    linkStato();
    Serial.println("DONE");
  } else if (line == "COL") {  // collezione: un posto per riga
    for (uint8_t i = 0; i < pet.colNumero(); i++) {
      const PetRitratto &r = pet.colRitratto(i);
      Serial.printf("%u: #%d %s nv=%u%s nick=%s ot=%u riprendi=%u\n", i, r.specie,
                    DEX_TBL[r.specie].name, levelFromAge(r.ageMinutes),
                    r.shiny ? " shiny" : "", r.nick, (unsigned)r.otId, (unsigned)pet.colPerche(i));
    }
    Serial.printf("col %u/%u domanda=%d\n", pet.colNumero(), COL_POSTI, pet.colChiede());
    Serial.println("DONE");
  } else if (line == "COLADD") {  // deposita il pet attivo senza cerimonia (prove)
    if (pet.isEgg() || pet.ceremony || !pet.colDeposita(pet.ritratto())) {
      Serial.println("ERR (niente uovo ne' cerimonia; collezione piena?)");
      return;
    }
    pet.newEgg();
    Serial.println("DONE");
  } else if (line == "COLSI" || line.startsWith("COLSI ")) {  // COLSI [posto da liberare]
    int8_t libera = line.length() > 6 ? (int8_t)line.substring(6).toInt() : -1;
    if (!pet.colConserva(libera)) {
      Serial.println("ERR (nessuna domanda; con la collezione piena: COLSI <posto>)");
      return;
    }
    Serial.println("DONE");
  } else if (line == "COLNO") {
    if (!pet.colChiede()) {
      Serial.println("ERR (nessuna domanda)");
      return;
    }
    pet.colRinuncia();
    Serial.println("DONE");
  } else if (line == "OTA") {  // aggiornamento: versione, repository, manifest, WiFi
    char ssid[OTA_SSID_MAX + 1], pass[OTA_PASS_MAX + 1];
    bool rete = otaCredenziali(ssid, sizeof(ssid), pass, sizeof(pass));
    Serial.printf("ota fw=%s repo=%s manifest=%s prova=%d wifi=%u rete=%s\n", FW_VERSION,
                  PG_REPO, otaUrlManifest(), otaUrlProva() ? 1 : 0, halWifiStato(),
                  rete ? ssid : "-");
    Serial.println("DONE");
  } else if (line.startsWith("OTA URL")) {  // OTA URL [url]: manifest di prova fino al riavvio
    otaImpostaUrl(line.length() > 8 ? line.substring(8).c_str() : "");
    Serial.printf("manifest=%s\n", otaUrlManifest());
    Serial.println("DONE");
  } else if (line == "WIFI OFF") {  // dimentica la rete
    otaDimentica();
    Serial.println("DONE");
  } else if (line.startsWith("WIFI ")) {  // WIFI <ssid senza spazi> [password, anche con spazi]
    String resto = line.substring(5);
    int sp = resto.indexOf(' ');
    String ssid = sp < 0 ? resto : resto.substring(0, sp);
    String pass = sp < 0 ? String("") : resto.substring(sp + 1);
    if (!otaSalvaCredenziali(ssid.c_str(), pass.c_str())) {
      Serial.println("ERR (WIFI <ssid> [password]: ssid 1-32, password 0-63 caratteri)");
      return;
    }
    Serial.println("DONE");
  } else if (line == "AGGIORNA") {  // apre la schermata dell'aggiornamento (prove dalla seriale)
    goScreen(SCR_AGGIORNA);
    Serial.println("DONE");
  } else if (line == "STATS") {
    Serial.printf("spec=%d nv=%u com=%u fel=%u ene=%u lim=%u desc=%u sd=%d mon=%d bat=%d usb=%d rtc=%u\n",
                  pet.speciesId, pet.level(), pet.fullness, pet.joy, pet.energy,
                  pet.hygiene, pet.careMistakes, sdReady, mon.loaded,
                  halBatPercent(), halUsbPresent(), halRtcEpoch());
    Serial.printf("peso=%u fue=%u def=%u vel=%u genes=%u/%u/%u tr=%u/%u/%u baya=%d\n",
                  pet.weight, pet.atkStat(), pet.defStat(), pet.speStat(),
                  pet.geneAtk, pet.geneDef, pet.geneSpe,
                  pet.trAtk, pet.trDef, pet.trSpe, pet.berryKnown);
    Serial.printf("shiny=%d streak=%u/%u bond=%u medals=0x%X(%u) nick=%s\n",
                  pet.shiny, pet.streak, pet.bestStreak, pet.bond, pet.medals,
                  pet.totalMedals, pet.nick);
    Serial.println("DONE");
  }
}
