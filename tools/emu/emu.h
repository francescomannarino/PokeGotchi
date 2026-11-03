// Interfaccia interna dell'emulatore: ciò che il ciclo principale (emu.cpp) usa da
// arduino_host.cpp e hal_host.cpp oltre a hal.h.
#pragma once
#include <stdint.h>
#include <stddef.h>

// ---- tempo (arduino_host.cpp) ----
void emuTempoAvanza(uint32_t ms);
uint64_t emuTempoMs();

// ---- console (arduino_host.cpp) ----
void emuSerialeRiceve(const char *dati, size_t n);  // come se arrivasse dalla seriale
void emuSerialeStdin(bool attivo);
bool emuSerialeLeggiStdin(int attesaMs);             // true se è arrivato qualcosa

// ---- piattaforma (hal_host.cpp) ----
// senza --sd/--nvs: tools/sdcard e build/nvs.txt, trovati partendo dall'eseguibile
struct EmuOpzioni {
  const char *sd = nullptr;               // cartella che fa da SD
  const char *nvs = nullptr;              // la NVS su file (più <nvs>.rtc per l'RTC)
  uint32_t epoch = 0;                     // ora iniziale dell'RTC (0 = quella del PC)
  const char *otaUltima = nullptr;        // versione dell'ultima release finta (WiFi finto)
};
extern EmuOpzioni emuOpz;
extern char **emuArgv;                    // per il riavvio (halRestart rilancia sé stesso)

// audio: suono = false (--muto, --headless) prepara il motore senza uscita
void emuAudioApri(bool suono);
void emuAudioChiudi();

const uint16_t *emuFramebuffer();         // l'ultimo fotogramma inviato con halFlush()
uint32_t emuFotogrammi();                 // quanti halFlush() finora
uint8_t emuLuminosita();
bool emuSalvaPng(const char *path);       // l'ultimo fotogramma, con la maschera rotonda
void emuNvsSalvaSeCambiata();
void emuSalvaStato();                     // NVS e RTC, all'uscita

// touch (tocco_host.cpp, anche nella PWA): il dito del mouse, oppure un gesto in coda
void emuDito(bool giu, int16_t x, int16_t y);
void emuGestoTocco(int16_t x, int16_t y);
void emuGestoScorri(int dx, int dy);      // +1/-1: verso destra / verso il basso
bool emuGestoInCorso();                   // il gesto o il dito non sono finiti
void emuPremiPwr();

// movimento (hal_host.cpp): il sensore finto, che esiste dal primo uso
void emuMotoInclina(float x, float y);    // gravità di base negli assi dello schermo, in g
void emuMotoTasti(int dx, int dy);        // I/J/K/L tenuti: -1/0/+1 per asse
void emuMotoScuoti();                     // uno scossone (perso col sensore spento)
void emuMotoColpetti();                   // un doppio colpetto (solo in VEGLIA)
