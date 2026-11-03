// PokeGotchi - tamagotchi pixel art ispirato alla gen 1
// per Waveshare ESP32-S3-Touch-AMOLED-1.75
//
// Librerie (Library Manager o repo di Waveshare):
// - "GFX Library for Arduino" (moononournation), con supporto CO5300 QSPI
// - "SensorLib" (Lewis He), driver touch CST9217
//
// Scheda: ESP32S3 Dev Module | Flash 16MB | PSRAM: OPI PSRAM | USB CDC On Boot: Enabled
//
// Gli sprite e la tabella delle specie si generano con tools/sprites.py (emit).

#include "ui_common.h"

uint32_t lastRender = 0;

void setup() {
  halBegin();  // seriale, I2C, alimentazione del pannello, schermo e touch
  Serial.printf("PokeGotchi fw v%s\n", FW_VERSION);
  // partita delle versioni fino alla 2.1 (namespace NVS col nome vecchio): prima di ogni lettura
  if (saveMigraNamespace(nvsChiavi())) Serial.println("NVS: partita migrata al nome nuovo");
  loadLang();  // lingua salvata (IT di default)
  applyLangFont();  // font della lingua salvata (classico tranne CJK)

  // la SD prima del pet: al cambio di firmware la copia "aggiornamento" fotografa la NVS
  // prima che load() migri le chiavi
  sdBegin();
  saveSdAvvio(FW_VERSION);
  pet.begin();
  linkScambioCarica();  // uno scambio interrotto blocca il pet offerto finché non si chiude
  thumbs.load();

  // orologio reale: applica il tempo in cui è rimasto spento
  halRtcBegin();
  halPowerBegin();  // PMU e pulsante PWR
  uint32_t e = halRtcEpoch();
  if (e == 0) {
    // Senza batteria tampone, il PCF85063 perde l'ora quando manca l'alimentazione.
    // Seminare sempre la data fissa farebbe TORNARE INDIETRO il tempo di gioco rispetto
    // a quanto già salvato, e questo rompe la serie, l'età e l'ora della scena. Si
    // semina con l'ultima ora vista, se è successiva, così il tempo non va mai
    // all'indietro. I minuti da spento si perdono comunque (senza RTC non c'è modo di
    // saperli), ma nulla si sfasa.
    uint32_t seed = 1767225600UL;             // RTC davvero vergine
    uint32_t seen = pet.savedEpoch();         // usa la NVS che pet ha già aperto
    if (seen > seed) seed = seen;
    halRtcSetEpoch(seed);
    e = halRtcEpoch();
    Serial.printf("RTC senza ora: impostato a %u%s\n", seed,
                  seen > 1767225600UL ? " (dall'ultima ora salvata)" : "");
  }
  saveSdAvvioFine(pet);  // scrive la copia "aggiornamento" prima dei minuti da spento
  pet.syncClock(e);

  halAudioBegin();  // ES8311 + I2S + amplificatore (suona un jingle di avvio)

  proponiAvvio();  // importa.pgsave, oppure il ripristino di una partita nuova
  lastInteract = millis();
}

void loop() {
  uint32_t now = millis();
  pet.update(now);

  // l'amplificatore segue il sonno e lo schermo spento dal PWR (la chiamata esce subito se
  // non cambia): niente suono, e la musica di una battaglia va avanti in silenzio. Qui
  // copre tutte le vie: il pulsante della luce, il sonno notturno e quello che arriva
  // applicato dalla progressione offline.
  halAudioSetSleeping(pet.sleeping || screenOff);

  // avvisa con un suono quando il Pokémon diventa pronto a evolvere (include il caso in
  // cui le condizioni si soddisfano al risveglio). canEvolveNow è false mentre dorme.
  static bool wasEvoReady = false;
  bool evoReady = pet.wantEvolveButton();
  if (evoReady && !wasEvoReady) halSfx(SFX_MEDAL);
  wasEvoReady = evoReady;
  // avviso cupo quando il Pokémon sta per fuggire per abbandono
  static bool wasRunReady = false;
  bool runReady = pet.canRunawayNow();
  if (runReady && !wasRunReady) halSfx(SFX_DENY);
  wasRunReady = runReady;

  syncScreen();  // l'iniziale e la cerimonia cambiano anche fuori dalla UI
  handleTouch();
  handleSerial();
  saveDlgScadenza();  // IMPORT dalla seriale senza conferma: si annulla da solo
  linkLoop();    // collegamento (solo in SCR_LINK): pacchetti e keepalive, anche a schermo spento
  // sensore di movimento: i giochi a inclinazione lo tengono in GIOCO da sé
  // (inclInizia/inclFine); sulla principale VEGLIA, per scossone e colpetti, con lo schermo
  // acceso o attenuato e MOVIMENTO acceso; spento altrove e con lo schermo spento dal PWR
  // (in tasca prende colpi). Per i 5 s di MOTO decide la console.
  if (!motoConsoleAttivo() && gScreen != SCR_GROTTA && gScreen != SCR_SURF) {
    bool veglia = gScreen == SCR_MAIN && !screenOff && pet.movimento();
    halMotoModo(veglia ? HAL_MOTO_VEGLIA : HAL_MOTO_SPENTO);
  }
  motoPrincipale();
  ensureMon();

  // pressione breve del PWR: schermo on/off
  static uint32_t lastPwr = 0;
  if (now - lastPwr > 250) {
    lastPwr = now;
    if (halPwrShortPressed()) {
      screenOff = !screenOff;
      if (!screenOff) lastInteract = now;
    }
  }

  updateBrightness(now);

  // esegue il salvataggio automatico periodico SOLO con lo schermo attenuato/spento o
  // durante il sonno: la scrittura su NVS congela ~1s entrambi i core (cache di flash
  // off), e qui non c'è animazione da tagliare né dito che aspetta risposta. Con 90s di
  // inattività lo schermo si attenua già, quindi si esegue subito; l'uso attivo
  // persiste comunque con i salvataggi di ogni azione (mangiare/giocare/...).
  // Con la radio accesa (SCR_LINK) si aspetta: un secondo fermo farebbe saltare il
  // keepalive del collegamento, e si salva all'uscita da lì.
  // Lo stesso durante lo scaricamento di un aggiornamento (le letture dalla rete si fermerebbero).
  bool nvsLibera = !linkRadioAccesa() && !aggiornaOccupato();
  if (nvsLibera && pet.savePending() && (screenOff || dimStage >= 1 || pet.sleeping)) {
    pet.flushSave();
  }
  // la copia giornaliera sulla SD, con lo stesso criterio (ma non col solo sonno: lo
  // schermo resta acceso)
  if (nvsLibera && (screenOff || dimStage >= 1)) saveSdGiornaliera(pet);

  // annota l'ora reale ogni 30 s (si persiste a ogni save del gioco)
  static uint32_t lastClock = 0;
  if (now - lastClock > 30000) {
    lastClock = now;
    uint32_t e = halRtcEpoch();
    if (e) pet.lastSeenEpoch = e;
  }

  // battito di salute ogni 5 min (per il soak test; si scarta se non c'è monitor)
  static uint32_t lastHealth = 0;
  if (now - lastHealth > 300000) {
    lastHealth = now;
    Serial.printf("HEALTH up=%lus heap=%u min=%u bat=%d%% mv=%d chg=%d usb=%d dim=%u off=%d\n",
                  (unsigned long)(now / 1000), halFreeHeap(), halMinFreeHeap(),
                  halBatPercent(), halBatMillivolts(), halBatCharging() ? 1 : 0,
                  halUsbPresent() ? 1 : 0, dimStage, screenOff ? 1 : 0);
  }

  // 85 ms nei giochi e nel sacco: margine sicuro perché il ridisegno non pesti l'invio DMA del
  // frame precedente (a 40-65 ms si sovrapponeva e causava lampi neri; con sprite
  // grandi il disegno richiede di più, quindi si lascia un cuscinetto).
  // Con lo schermo spento non si disegna: a luminosità 0 non si vede nulla, ma il
  // ridisegno e il flush DMA del framebuffer 466x466 continuavano a girare a 10 fps, e
  // misurati sono 70 ms di lavoro ogni 100 (46 solo per lo scarico). È il più grande
  // carico evitabile della scheda. Ogni render ridipinge la scena intera, quindi non
  // resta nulla a metà; e poiché lastRender resta congelato, il primo frame dopo il
  // risveglio esce subito.
  if (!screenOff && now - lastRender >= (uint32_t)((gScreen == SCR_GAME || gScreen == SCR_SACK || gScreen == SCR_GROTTA || gScreen == SCR_SURF) ? 85 : 100)) {
    lastRender = now;
    render();
  }
}

// luminosità secondo sonno + inattività (protezione dell'AMOLED)
void updateBrightness(uint32_t now) {
  // gli eventi visibili svegliano lo schermo da soli
  // (la domanda della collezione a fine cerimonia no: aspetta una risposta, come un menu)
  if (pet.evolving() || (pet.ceremony && !pet.colChiede()) || pet.eating() || pet.showHeart()) {
    lastInteract = now;
  }
  uint32_t idle = now - lastInteract;
  dimStage = (idle > 300000) ? 2 : (idle > 90000) ? 1 : 0;
  uint8_t target = pet.sleeping ? 25 : (halUsbPresent() ? 180 : 145);
  if (dimStage == 1) target = pet.sleeping ? 10 : 60;
  else if (dimStage == 2) target = 8;
  if (screenOff) target = 0;
  static uint8_t current = 255;
  if (target != current) {
    current = target;
    halSetBrightness(target);
  }
}
