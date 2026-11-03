#pragma once
// Copie dei salvataggi sulla SD (cartella /save/), ripristino e comandi seriali
// EXPORT/BACKUP/IMPORT.
// Parte della scheda di `savefile`: la SD e l'elenco delle chiavi NVS passano dall'HAL
// (`halSd*`, `halNvsElenca`); il file resta fuori dalla build dei test.
//
// Ogni copia si scrive come `<nome>.tmp`, si rilegge controllando il CRC e solo allora si
// rinomina in `/save/AAAAMMGG-HHMMSS-<motivo>.pgsave` (vedi `saveNomeFile`). Dopo ogni copia
// la rotazione tiene le 10 più recenti più l'ultima di ogni motivo (`saveRotazione`).
// Copie automatiche:
//   - "aggiornamento": al primo avvio di un firmware diverso da quello annotato in NVS
//     ("fwver"), con le chiavi fotografate PRIMA di `pet.begin()`;
//   - "giornaliera": alla prima attenuazione dello schermo dopo il cambio di data ("bkday");
//   - "importazione": prima di sovrascrivere la NVS (`saveSdCopiaPrimaImport`).
//   - "scambio": prima di uno scambio in collegamento (`saveSdCopiaPrimaScambio`).
// Senza SD le copie si saltano e `saveSdSaltata` lo segnala.
#include <Arduino.h>
#include "savefile.h"

#define SAVE_DIR "/save"

class Pet;

// true se l'ultima copia automatica è stata saltata per mancanza della SD; torna false
// alla prima copia riuscita. Le impostazioni lo mostrano.
extern bool saveSdSaltata;

// elenco delle chiavi dalla NVS della piattaforma (`halNvsElenca`)
SaveElencoChiavi &nvsChiavi();

// Da chiamare con la SD già montata e PRIMA di `pet.begin()`: se il firmware è cambiato e
// la NVS ha dei dati, li fotografa in RAM prima che `load()` li migri. `fw` deve restare
// valido (FW_VERSION).
void saveSdAvvio(const char *fw);
// Da chiamare dopo `pet.begin()` e con l'RTC pronto, prima di `syncClock()`: completa la
// fotografia con il riepilogo, la scrive sulla SD e annota la versione del firmware.
// Senza SD (o se la scrittura fallisce) la versione non si annota: si riprova al prossimo
// avvio.
void saveSdAvvioFine(Pet &pet);

// Copia dello stato attuale (allinea l'orologio del pet: salva la NVS). `nome`, se dato,
// riceve il nome del file senza cartella. false senza SD o se la scrittura fallisce.
bool saveSdCopia(Pet &pet, uint8_t motivo, char *nome = 0, size_t max = 0);
// Copia giornaliera: da chiamare dal loop SOLO a schermo attenuato o spento. Al più un
// tentativo al giorno per avvio.
void saveSdGiornaliera(Pet &pet);
// Callback `copiaPrima` di `saveImporta`, con ctx = Pet*. Senza SD l'importazione procede
// (segnala `saveSdSaltata`); con la SD, false se la copia non riesce.
bool saveSdCopiaPrimaImport(void *ctx);
// Callback `copiaPrima` di `LinkScambio`, con ctx = Pet*: copia con motivo
// «scambio». Come per l'importazione, senza SD lo scambio procede (segnala `saveSdSaltata`);
// con la SD, false (lo scambio si annulla) se la copia non riesce.
bool saveSdCopiaPrimaScambio(void *ctx);

// ---------------------------------------------------------------- elenco e ripristino

// file che un lettore di schede (o la PWA) lascia sulla SD: proposto all'avvio, poi
// rinominato in `importato-AAAAMMGG-HHMMSS.pgsave` (resta nell'elenco, fuori rotazione)
#define SAVE_FILE_IMPORTA "importa.pgsave"
#define SAVE_ELENCO_MAX 24

struct SaveSdVoce {
  char nome[SAVE_NOME_MAX];  // senza cartella
  SaveIntestazione h;        // solo intestazione e riepilogo, CRC non controllato
};
// Copie in /save/ con un'intestazione leggibile, dalla più recente (epoch di creazione).
// Restituisce quante ne ha scritte in `out`, 0 senza SD.
uint8_t saveSdElenco(SaveSdVoce *out, uint8_t max);
bool saveSdImportaPresente();
// Rinomina /save/importa.pgsave (vedi SAVE_FILE_IMPORTA); false se non c'è o non riesce.
bool saveSdArchiviaImporta();

// Un ripristino alla volta aspetta la conferma sullo schermo: il file intero sta in RAM,
// già verificato da `saveVerifica`. L'origine decide il testo del dialogo, se fare la copia
// "prima dell'importazione" e se rispondere sulla seriale.
enum : uint8_t {
  SAVE_DA_NESSUNO = 0,
  SAVE_DA_ELENCO,       // "Ripristina" nelle impostazioni
  SAVE_DA_PRIMO_AVVIO,  // NVS vuota: niente copia prima, non c'è nulla da conservare
  SAVE_DA_FILE,         // /save/importa.pgsave
  SAVE_DA_SERIALE,      // IMPORT <base64>
};
// Carica e verifica /save/<nome>. Se non è SAVE_OK non resta nulla in attesa.
SaveEsito saveSdPreparaFile(const char *nome, uint8_t origine);
// Origine dell'importazione in attesa (SAVE_DA_NESSUNO se non ce n'è); `h` ne riceve
// l'intestazione.
uint8_t saveSdAttesa(SaveIntestazione *h = 0);
// Importa quella in attesa (`saveImporta`, con la copia prima tranne al primo avvio) e la
// libera. Dopo SAVE_OK il chiamante riavvia SUBITO, senza salvare il `Pet` in RAM.
SaveEsito saveSdConferma(Pet &pet);
void saveSdAnnulla();
// IMPORT dalla seriale senza conferma entro SAVE_ATTESA_SERIALE_MS: la libera, risponde
// `ERR SCADUTO` e restituisce true (una volta). Prima restava in attesa per sempre, anche
// dopo che salvataggi.py o l'installer avevano smesso di aspettare.
#define SAVE_ATTESA_SERIALE_MS 150000UL
bool saveSdScaduta();

// Comandi seriali, sempre attivi; true se `line` era uno di questi:
//   EXPORT           base64 su una riga, poi DONE
//   BACKUP           copia manuale sulla SD: stampa /save/<nome>, poi DONE
//   IMPORT <base64>  decodifica e verifica subito; risponde `ERR <esito>` (saveEsitoNome)
//                    oppure `CONFERMA <specie> <livello>` e non scrive nulla. Il resto
//                    arriva quando si tocca lo schermo: `DONE` e riavvio, oppure
//                    `ERR ANNULLATO` / `ERR <esito>`, oppure `ERR SCADUTO` dopo
//                    SAVE_ATTESA_SERIALE_MS. `ERR OCCUPATO` se un'altra
//                    importazione aspetta già la conferma.
bool saveSerialCommand(const String &line, Pet &pet);
