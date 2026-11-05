# Test

Batteria di test che gira **sul PC, senza scheda**: compila la logica pura del
firmware (`pet.cpp`, `i18n.cpp`, `dex.h`) contro alcuni shim di Arduino e la
esegue con un orologio e un `random()` finti, quindi i risultati sono
deterministici e richiedono meno di un secondo.

```bash
./test/run_tests.sh           # tutto
./test/run_tests.sh --asan    # in più con AddressSanitizer + UBSan
```

Oppure a pezzi:

```bash
cd test && make               # solo la logica del firmware
cd test && make FILTER=evolve # solo i test con "evolve" nel nome
cd test && make asan          # con i sanitizer
python3 test/test_tools.py    # solo gli strumenti di tools/
```

## Cosa copre

| File | Cosa verifica |
|---|---|
| `test_pet.cpp` | uovo e scelta dell'iniziale, cibo, tick delle statistiche, sonno, trascuratezze, livelli, evoluzione (compreso il ramo di Eevee), statistiche di combattimento, allenamento e minigioco, serie e legame, medaglie, cerimonie, progressione offline e salvataggio in NVS |
| `test_dex.cpp` | integrità del Pokédex: nomi ASCII, catene evolutive senza cicli, ogni specie raggiungibile, nessuna linea impossibile da completare, limiti dei buffer della UI |
| `test_i18n.cpp` | tutte le lingue complete, con gli accenti come byte singolo CP437 (mai UTF-8 nelle lingue latine), e con gli stessi `%u`/`%s` che lo sketch passa a ogni chiamata |
| `test_save.cpp` | il formato `.pgsave` (esportazione, verifica, importazione, rotazione delle copie) e la migrazione delle partite e dei file delle versioni fino alla 2.1 |
| `test_battle.cpp` | il motore di battaglia della prima generazione: danni, stati, mosse speciali, IA, squadre |
| `test_musica.cpp` | l'APU del Game Boy emulata e l'interprete del motore audio di pokered |
| `test_link.cpp`, `test_linkbattaglia.cpp`, `test_linkscambio.cpp` | il protocollo del collegamento, la battaglia host-autoritativa e lo scambio atomico, su un canale simulato che perde, duplica e riordina i pacchetti (`canale_link.h`) |
| `test_moto.cpp` | il filtro dell'inclinazione, il joystick, l'atterraggio del Surf e lo scossone |
| `test_ota.cpp` | versioni, manifest della release, credenziali del WiFi fuori dai salvataggi, regola della batteria |
| `test_tools.py` | gli script di `tools/` compilano, i dati del Pokédex sono coerenti, i file generati coincidono con i generatori, i buffer di testo bastano in ogni lingua, la versione è la stessa ovunque, il nome vecchio resta solo nei crediti |

## Come è organizzato

- `shim/Arduino.h` + `shim/shim.cpp` + `shim/Preferences.h` + `shim/preferences.cpp`:
  il minimo di Arduino che usano `pet.cpp` e `i18n.cpp`. L'orologio (`millis()`) e
  `random()` sono finti e controllabili; `Preferences` è una mappa in memoria che imita
  la NVS, quindi si può simulare un riavvio creando un altro `Pet` e chiamando `begin()`.
  `preferences.cpp` sta da solo perché lo usa anche l'emulatore (`tools/emu/`), che lo
  salva su file con `mockNvsSalva()`/`mockNvsCarica()`.
- `run_tests.sh` lancia in più lo smoke test dell'emulatore (`make -C ../tools/emu smoke`)
  se SDL2 è installato.
- `framework.h` + `main.cpp`: micro framework proprio, senza dipendenze.
- I test marcati con `TEST_KNOWN_ISSUE` descrivono il comportamento
  **corretto** di qualcosa che oggi fallisce: non rompono la batteria, vengono elencati
  a parte, e avvisano se un giorno iniziano a passare (per poter togliere il marcatore).

## Cosa NON copre

Tutto ciò che richiede la scheda: l'HAL (`hal_esp32*.cpp`), l'audio (`audio.cpp`), la
scheda SD (`sdmon.cpp`) e RTC/batteria (`rtcbat.cpp`). L'`.ino` e le schermate le prova lo
smoke test dell'emulatore; il firmware vero lo compila la CI con `arduino-cli`.
