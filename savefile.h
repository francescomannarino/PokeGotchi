#pragma once
// Salvataggi `.pgsave`: copia di tutte le chiavi NVS del gioco in un file unico per scheda
// e PWA, con intestazione e riepilogo leggibili senza decodificare il resto.
// Dipende solo da `Preferences` (e da `Pet` per il riepilogo): niente hardware. L'elenco
// delle chiavi arriva da `SaveElencoChiavi`, perché `Preferences` non sa enumerarle:
// sulla scheda lo implementa l'iteratore NVS dell'ESP-IDF, nei test lo shim.
//
// FORMATO (tutti gli interi sono little-endian, scritti byte per byte)
//
//   Intestazione (SAVE_HDR_MIN = 52 byte; un formato futuro può allungarla, chi legge
//   salta i byte in più grazie al campo a offset 10)
//   off  dim  campo
//    0    4   magic "PGSV" (si legge anche "TPSV" dei file delle versioni fino alla 2.1)
//    4    1   versione del formato (SAVE_FORMATO)
//    5    1   piattaforma di origine (SAVE_SCHEDA, SAVE_WEB)
//    6    1   motivo della copia (SAVE_MANUALE … SAVE_PRIMA_IMPORT)
//    7    1   riservato, 0
//    8    2   versione dello schema dei dati (SAVE_SCHEMA)
//   10    2   lunghezza dell'intestazione (≥ 52): il corpo comincia qui
//   12    4   lunghezza totale del file in byte, CRC finale compreso
//   16    4   epoch di creazione (secondi UTC, 0 = ora sconosciuta)
//   20   12   FW_VERSION di origine, ASCII, completato con NUL
//   -- riepilogo --
//   32    2   specie (int16, numero di Pokédex; -1 = uovo)
//   34    2   livello
//   36    1   shiny (0/1)
//   37    1   riservato, 0
//   38    2   specie registrate nel Pokédex
//   40   12   soprannome, completato con NUL ("" = nome della specie)
//
//   Corpo: un blocco per namespace, nell'ordine di SAVE_NAMESPACE (in lettura il nome
//   SAVE_NS_VECCHIO vale SAVE_NS_PET)
//     1   lunghezza del nome del namespace (1..15)
//     n   nome del namespace (senza NUL)
//     2   numero di voci
//     per ogni voce:
//       1   tipo (codici di `nvs_type_t`: SAVE_T_U8 … SAVE_T_BLOB)
//       1   lunghezza della chiave (1..15)
//       n   chiave (senza NUL)
//       2   lunghezza del valore
//       n   valore: interi LE della loro dimensione, stringhe senza NUL, blob grezzi
//
//   Coda: 4 byte, CRC32 (IEEE, lo stesso di zlib.crc32) di tutti i byte precedenti.
//
//   Forma testuale: base64 standard (con `=`), su una riga.
//
// Versioni: un file con formato o schema più recenti di quelli del firmware viene
// rifiutato ("aggiorna il firmware"); i più vecchi si importano sempre, perché il corpo
// copia le chiavi così come sono e `Pet::load()` migra quelle legacy.
#include <Arduino.h>
#include <Preferences.h>

class Pet;

// versione dello schema dei dati salvati: si alza quando cambiano chiavi o significato,
// insieme a una migrazione in `Pet::load()`. Unico punto in cui è definita.
// 1: chiavi singole (1.17-1.18). 2: stato nel blob PET_SAVE_KEY (pet.h); i file di schema 1
// si importano ancora e `load()` li migra.
#define SAVE_SCHEMA 2
// versione del formato del contenitore descritto sopra
#define SAVE_FORMATO 1
#define SAVE_HDR_MIN 52
#define SAVE_MAX_CHIAVE 15     // limite di chiavi e namespace della NVS
#define SAVE_MAX_STRINGA 255   // stringhe più lunghe non si esportano
// buffer consigliato per un salvataggio intero (oggi ne occupa ~1 KB)
#define SAVE_MAX_BYTES 8192
// caratteri base64 per n byte, senza il NUL finale
#define SAVE_B64_LEN(n) ((((n) + 2) / 3) * 4)

// Namespace NVS esportati e ripristinati. Ogni nuova chiave NVS deve stare in uno di
// questi, e un nuovo namespace va aggiunto qui. Unica eccezione voluta: il giornale degli
// scambi (namespace LS_NS "scambio" di linkscambio.h), che descrive un'operazione in
// corso e non va ripristinato.
// SAVE_NS_PET: pet e giocatore (pet.cpp), lingua (i18n.cpp), audio (audio.cpp), stato
// delle copie sulla SD (savesd.cpp: "fwver", "bkday").
#define SAVE_NS_PET "pokegotchi"
// Namespace delle versioni fino alla 2.1, prima del cambio di nome: saveMigraNamespace() lo
// copia in SAVE_NS_PET al primo avvio, e i salvataggi che lo contengono si importano lo
// stesso. Non si scrive mai.
#define SAVE_NS_VECCHIO "tamapoke"
static const char *const SAVE_NAMESPACE[] = { SAVE_NS_PET };
static const uint8_t SAVE_N_NAMESPACE = sizeof(SAVE_NAMESPACE) / sizeof(SAVE_NAMESPACE[0]);

// tipi dei valori: gli stessi codici di `nvs_type_t`, così l'iteratore NVS li passa tali e
// quali
enum : uint8_t {
  SAVE_T_U8 = 0x01, SAVE_T_I8 = 0x11, SAVE_T_U16 = 0x02, SAVE_T_I16 = 0x12,
  SAVE_T_U32 = 0x04, SAVE_T_I32 = 0x14, SAVE_T_U64 = 0x08, SAVE_T_I64 = 0x18,
  SAVE_T_STR = 0x21, SAVE_T_BLOB = 0x42,
};

enum : uint8_t { SAVE_SCHEDA = 0, SAVE_WEB = 1 };
enum : uint8_t {
  SAVE_MANUALE = 0, SAVE_GIORNALIERA, SAVE_AGGIORNAMENTO, SAVE_PRIMA_SCAMBIO,
  SAVE_PRIMA_IMPORT,
};

enum SaveEsito : uint8_t {
  SAVE_OK = 0,
  SAVE_ERR_MAGIC,      // non è un .pgsave
  SAVE_ERR_FORMATO,    // contenitore più recente del firmware
  SAVE_ERR_LUNGHEZZA,  // troncato o con byte in più
  SAVE_ERR_CRC,        // corrotto
  SAVE_ERR_SCHEMA,     // dati più recenti del firmware: aggiornarlo
  SAVE_ERR_DATI,       // corpo malformato (tipo sconosciuto, lunghezze incoerenti)
  SAVE_ERR_COPIA,      // la copia "prima dell'importazione" non è riuscita
  SAVE_ERR_SCRITTURA,  // la NVS ha rifiutato una scrittura (può restare a metà)
};
const char *saveEsitoNome(SaveEsito e);  // nome ASCII breve, per la console

struct SaveRiepilogo {
  int16_t specie = -1;   // numero di Pokédex, -1 = uovo
  uint16_t livello = 0;
  bool shiny = false;
  uint16_t pokedex = 0;  // specie registrate
  char nome[12] = "";    // soprannome ("" = nome della specie)
};

struct SaveIntestazione {
  uint8_t formato = SAVE_FORMATO;  // in scrittura vale sempre SAVE_FORMATO
  uint16_t schema = SAVE_SCHEMA;   // in scrittura vale sempre SAVE_SCHEMA
  uint8_t piattaforma = SAVE_SCHEDA;
  uint8_t motivo = SAVE_MANUALE;
  uint32_t epoch = 0;
  char fw[13] = "";
  SaveRiepilogo riepilogo;
  uint16_t lunghezzaHdr = SAVE_HDR_MIN;  // solo in lettura
  uint32_t lunghezza = 0;                // solo in lettura: byte totali, CRC compreso
};

// Elenca le chiavi di un namespace NVS. Sulla scheda: `nvs_entry_find`/`nvs_entry_next`.
class SaveElencoChiavi {
public:
  virtual ~SaveElencoChiavi() {}
  // chiama `voce` per ogni chiave di `ns`, con il suo tipo (SAVE_T_*)
  virtual void elenca(const char *ns,
                      void (*voce)(const char *chiave, uint8_t tipo, void *ctx),
                      void *ctx) = 0;
};

// Esporta lo stato attuale. Prima allinea l'orologio del pet (`setClock(oraEpoch)`, che
// salva subito), così il file contiene i minuti fino a ora; con oraEpoch = 0 contiene
// l'ultimo salvataggio. Restituisce i byte scritti in `out`, 0 se non bastano.
size_t saveEsporta(Pet &pet, uint32_t oraEpoch, const char *fw, uint8_t piattaforma,
                   uint8_t motivo, SaveElencoChiavi &chiavi, uint8_t *out, size_t max);
// Livello basso: serializza i namespace con l'intestazione data (riepilogo compreso).
size_t saveScrivi(const SaveIntestazione &h, SaveElencoChiavi &chiavi, uint8_t *out,
                  size_t max);

// Legge solo l'intestazione (servono i primi SAVE_HDR_MIN byte): magic, formato e
// riepilogo, senza controllare lunghezza e CRC. Per gli elenchi di salvataggi.
SaveEsito saveLeggiIntestazione(const uint8_t *dati, size_t len, SaveIntestazione *h);
// Controlla tutto senza scrivere: magic, formato, lunghezza, CRC, schema, corpo.
SaveEsito saveVerifica(const uint8_t *dati, size_t len, SaveIntestazione *h = 0);

// Importa un salvataggio. Prima verifica tutto (`saveVerifica`): se qualcosa non torna non
// scrive nulla. Poi chiama `copiaPrima(ctx)` per mettere da parte lo stato attuale
// (false = annulla, niente scritture). Poi svuota i namespace di SAVE_NAMESPACE, scrive le
// chiavi del file e congela il tempo: "seen" (lastSeenEpoch) diventa oraEpoch, o viene
// tolto se è 0, così `syncClock()` non applica il tempo passato da quando il file è stato
// creato. Dopo SAVE_OK il chiamante riavvia senza salvare il `Pet` in memoria, che
// sovrascriverebbe lo stato importato: è `load()` a validarlo e migrarlo.
SaveEsito saveImporta(const uint8_t *dati, size_t len, uint32_t oraEpoch,
                      bool (*copiaPrima)(void *ctx), void *ctx);

// CRC32 IEEE (zlib); `crc` permette di calcolarlo a pezzi
uint32_t saveCrc32(const uint8_t *dati, size_t len, uint32_t crc = 0);
// Riepilogo dell'intestazione preso dal pet (specie, livello, shiny, Pokédex, nome).
SaveRiepilogo saveRiepilogo(Pet &pet);
// Riscrive epoch di creazione e riepilogo di un file già serializzato e ricalcola il CRC.
// Serve alla copia "aggiornamento", fotografata prima di `Pet::load()` e completata dopo.
// false se non è un .pgsave intero (magic o lunghezza).
bool saveCompletaIntestazione(uint8_t *dati, size_t len, uint32_t epoch,
                              const SaveRiepilogo &r);

// Migrazione dal namespace delle versioni fino alla 2.1 (SAVE_NS_VECCHIO): se SAVE_NS_PET è
// vuoto e quello vecchio no, ne copia tutte le chiavi (stessi tipi e valori) e poi svuota il
// vecchio. Va chiamata all'avvio prima di chiunque legga SAVE_NS_PET. true se ha copiato.
bool saveMigraNamespace(SaveElencoChiavi &chiavi);

// Copie in una cartella (la SD della scheda). Nome: `AAAAMMGG-HHMMSS-<motivo>.pgsave`, con
// data e ora dell'epoch, così l'ordine alfabetico è quello cronologico. I secondi (il piano
// diceva HHMM) evitano che due copie nello stesso minuto si sovrascrivano.
#define SAVE_ESTENSIONE ".pgsave"
// le copie delle versioni fino alla 2.1: si elencano, si ripristinano e ruotano come le altre
#define SAVE_ESTENSIONE_VECCHIA ".tpsave"
// il nome finisce con una delle due estensioni
bool saveNomeHaEstensione(const char *nome);
#define SAVE_NOME_MAX 40  // buffer per il nome, NUL compreso
#define SAVE_TIENI 10     // la rotazione tiene le 10 copie più recenti...
const char *saveMotivoNome(uint8_t motivo);  // "manuale", "giornaliera", ...; "?" se ignoto
// Scrive in `out` il nome della copia (senza cartella). Restituisce la lunghezza, 0 se
// `max` non basta.
size_t saveNomeFile(char *out, size_t max, uint32_t epoch, uint8_t motivo);
// Motivo di un nome prodotto da `saveNomeFile`, -1 se il nome non ha quella forma (per
// esempio `importa.pgsave` o un `.tmp` rimasto a metà): la rotazione non lo tocca.
int saveMotivoDaNome(const char *nome);
// Rotazione: `nomi` in ordine crescente (alfabetico = cronologico). Segna in `tieni` le
// SAVE_TIENI copie più recenti più l'ultima di ogni motivo; i nomi di forma estranea si
// tengono sempre. Con `adesso` (epoch locale, 0 = nessun controllo) anche le copie datate
// più di SAVE_FUTURO_S dopo: non contano tra le più recenti e non si tolgono (prove con
// TIME avanti, oppure un RTC rimasto indietro: le copie vere non devono sparire).
#define SAVE_FUTURO_S 60
void saveRotazione(const char *const *nomi, uint16_t n, bool *tieni, uint32_t adesso = 0);
// La copia `nome` è datata più di SAVE_FUTURO_S dopo `adesso` (stessa regola).
bool saveDalFuturo(const char *nome, uint32_t adesso);

// base64 standard su una riga; `out` riceve anche il NUL. Restituiscono la lunghezza
// prodotta, 0 se il buffer non basta o (decodifica) il testo non è base64 valido.
// La decodifica salta spazi e a capo.
size_t saveBase64Codifica(const uint8_t *dati, size_t len, char *out, size_t max);
size_t saveBase64Decodifica(const char *testo, size_t len, uint8_t *out, size_t max);
