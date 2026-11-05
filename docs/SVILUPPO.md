# Sviluppare PokeGotchi

Guida per chi vuole compilare, modificare o contribuire. Il gioco, dal punto di vista di chi
gioca, è nel [libretto del giocatore](https://francescomannarino.github.io/PokeGotchi/libretto-del-giocatore.pdf);
gesti, coordinate e console in [`COMANDI.md`](../COMANDI.md).

Commenti, documentazione e testi predefiniti sono in **italiano**, e lo sono anche gli
identificatori degli strumenti e i nomi dei test (restano i nomi tecnici inglesi esistenti:
`DEX`, `BASE_STATS`, `TestGeneratedFiles`…). Scrivi in italiano anche il codice nuovo.

## Cosa c'è nel repository

- Il **firmware**: uno sketch Arduino nella radice (`PokeGotchi.ino` e i `.cpp/.h` accanto),
  per la Waveshare ESP32-S3-Touch-AMOLED-1.75 (AMOLED rotondo 466×466, CO5300 via QSPI, touch
  CST9217, PMU AXP2101, RTC PCF85063, codec ES8311, accelerometro QMI8658, microSD).
- Gli **strumenti** in Python (`tools/`): dati delle specie, sprite, musiche, salvataggi,
  diagrammi, libretto.
- L'**emulatore** per PC (`tools/emu/`, SDL2) e la **PWA** (`pwa/`, WebAssembly con
  Emscripten), che compilano lo stesso firmware.
- L'**installer** da browser (`web/`), il **sito** (`site/`) e il **libretto** (`docs/libretto/`).
- I **test** (`test/`): C++ per la logica, Python per gli strumenti.

## Comandi

```bash
# core ESP32 e librerie alle versioni provate (le stesse della CI)
bash tools/prepara_arduino.sh            # tutto; "librerie" = solo le librerie (emulatore, PWA)

# firmware: la cartella del repository deve chiamarsi PokeGotchi (regola di arduino-cli).
# PSRAM=opi è obbligatorio: il framebuffer da ~434 KB vive in PSRAM
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
arduino-cli compile --fqbn "$FQBN" .
arduino-cli upload -p <porta> --fqbn "$FQBN" .
bash tools/build_firmware.sh             # compila e prepara dist/ come una release (PG_REPO=owner/nome)

# test (senza scheda né compilazione del firmware)
bash test/run_tests.sh [--asan]          # logica C++, strumenti Python, smoke test dell'emulatore
python3 test/test_tools.py               # solo gli strumenti; una classe: ... TestBufferDiTesto
make -C test run FILTER=ota              # solo i test C++ il cui nome contiene "ota"
python3 tools/test_i18n_formats.py       # coerenza delle tabelle di stringhe

# file generati
python3 tools/gen_dex.py                 # -> dex.h, moves.h
python3 tools/gen_musica.py              # -> musica_dati.h (da tools/musica/originali/)
python3 tools/gen_musica.py durate       # intro e giro di ogni canale, in fotogrammi
python3 tools/confronta_musica.py        # i brani contro le musiche di pret (a mano, scarica)
python3 tools/crediti_sprite.py          # -> docs/CREDITI_SPRITE.md (scarica il tracker)
python3 tools/sprites.py emit            # -> species.h
python3 tools/fetch_gen1.py              # dati di battaglia da pret/pokered -> tools/dex_types.py, dex_moves.py

# emulatore (SDL2): mouse = touch, frecce, P = PWR, S = foto, I/J/K/L inclinano, Z scuote
cd tools/emu && make && ./build/pokegotchi --fast 60
cd tools/emu && make smoke               # 48 h accelerate senza finestra, ogni schermata
cd tools/emu && make musica              # i brani in WAV (build/musica/)
./build/pokegotchi --ota-ultima 3.1.0    # il WiFi finto annuncia una release più nuova

# PWA (Emscripten; carica ~/emsdk se emcc non è nel PATH) -> web/gioca/
bash tools/build_pwa.sh

# documentazione, libretto e sito
python3 tools/foto_doc.py                # smoke test -> docs/img/schermate/ e docs/libretto/img/
python3 tools/make_diagrams.py           # -> docs/img/comandi/*.svg
python3 tools/libretto.py                # -> dist/libretto-del-giocatore.pdf (serve Chrome)
bash tools/build_web.sh                  # web/ completo per le prove locali
bash tools/build_sito.sh                 # -> _site/, come su GitHub Pages
python3 tools/serve_web.py [--sito] [--locale]   # con le richieste Range; --locale = solo 127.0.0.1

# salvataggi dal terminale (scheda via seriale, o file senza scheda)
python3 tools/salvataggi.py esporta -o mio.pgsave | importa mio.pgsave | info mio.pgsave
```

Per provare in fretta abbassa `PET_TICK_MS`, `MINUTES_PER_LEVEL` e `FAREWELL_AGE_MIN` in
`pet.h`. La console seriale (115200) ha comandi di prova che rispondono `ERR` finché non si
invia `DEBUG ON`; l'elenco è in [`COMANDI.md`](../COMANDI.md#console-seriale).

## Architettura

- **`PokeGotchi.ino`**: `setup()`, `loop()` e luminosità.
- **HAL** (`hal.h`, `hal_esp32.cpp`, `hal_esp32_rete.cpp`): l'unico accesso del gioco
  all'hardware: schermo, luminosità, touch, RTC, batteria e PWR, audio, file della SD, NVS,
  radio del collegamento, sensore di movimento, WiFi e aggiornamento. Ogni accesso al bus I2C
  passa dal mutex ricorsivo `BusI2C`: il sensore si legge da un task sul core 0 ogni 8 ms. I
  driver (`rtcbat`, `audio`, SensorLib, SD_MMC) li include solo l'HAL della scheda.
  `millis()`, `random()`, `delay()`, `Serial` e `Preferences` restano quelli del core (shim su
  PC). Sul PC l'HAL è `tools/emu/hal_host.cpp`, nel browser `pwa/hal_web.cpp`.
- **UI** (`ui_*.cpp`): `ui_common.h` dichiara i globali condivisi, la geometria e i prototipi;
  ogni schermata ha il suo file. Una schermata attiva alla volta (`enum Screen`, `gScreen`):
  `render()` e i gesti passano per la tabella `SCREENS` (`render`, `onTap`, `onSwipe`,
  `onSwipeV`, `onEnter`, `onExit`). Si cambia schermata solo con `goScreen()`. Per aggiungerne
  una: valore dell'enum, riga in `SCREENS` nello stesso ordine, gestori in `ui_common.h`, il
  nome nei `NOMI` di `tools/emu/emu.cpp` e una tappa in `tools/emu/smoke.txt`.
- **`pet.h/.cpp`**: stato e regole del gioco (tick, evoluzione, uova, finali, serie, legame,
  medaglie, collezione), salvati in NVS. 1 tick = 1 minuto; livello = ore di età; il tempo da
  spento si applica dall'RTC con `syncClock()`.
- **`savefile.h/.cpp`** (formato `.pgsave`, layout byte per byte in testa all'header, uguale per
  scheda, PWA ed emulatore) e **`savesd.h/.cpp`** (copie sulla microSD, rotazione,
  `EXPORT`/`IMPORT`/`BACKUP`). `saveMigraNamespace()` porta le partite delle versioni fino alla
  2.1 nel namespace attuale.
- **`battle.h/.cpp`**: motore di battaglia della prima generazione, senza hardware;
  **`link*.h/.cpp`**: protocollo del collegamento, battaglia host-autoritativa e scambio atomico
  con giornale, provati su un canale simulato che perde (`test/canale_link.h`);
  **`linknow.h/.cpp`**: il trasporto ESP-NOW.
- **`apu.h/.cpp`** e **`musica.h/.cpp`**: APU del Game Boy a banda limitata e interprete con
  l'algoritmo del motore audio di pokered, senza hardware; sulla scheda li alimenta il task
  audio di `audio.cpp`.
- **`moto.h/.cpp`**: filtro dell'inclinazione, joystick sul touch, atterraggio del Surf,
  riconoscimento dello scossone.
- **`ota.h/.cpp`**: versioni, manifest della release, credenziali del WiFi (namespace `wifi`),
  regole prima di installare; la schermata è `ui_aggiorna.cpp`, la tastiera `ui_tastiera.cpp`.
- **`sdmon.h/.cpp`**: sprite PMD (`PGK2`) e miniature (`PGTH`) dalla SD in PSRAM, e il
  protocollo `PUT`/`LS` usato da `tools/send_sd.py` e dall'installer.
- **`i18n.h/.cpp`**: le stringhe in 8 lingue; **`ritratto.h/.cpp`**: il «ritratto» di 50 byte di
  un Pokémon (collezione, squadre, scambi).

### Dati e file generati

`tools/dex_data.py` è l'unica fonte dei dati delle specie; `dex_stats.py`, `dex_names.py`,
`dex_types.py`, `dex_moves.py` e `dex_move_names.py` completano statistiche, nomi e dati di
battaglia; `gen_dex.py` ne fa **`dex.h`** e **`moves.h`**. **Non modificare mai a mano
`dex.h`, `moves.h`, `musica_dati.h` né `species.h`**: cambia la sorgente e rigenera
(`TestGeneratedFiles` controlla che coincidano).

## Musica

I nove brani (battaglie, vittorie, Surf, Grotta) sono originali, scritti per PokeGotchi e
coperti dalla licenza del progetto. Stanno in `tools/musica/originali/audio/music/*.asm`, nella
sintassi dei comandi audio dei disassemblaggi di pret (`note`, `rest`, `note_type`, `octave`,
`duty_cycle`, `vibrato`, `sound_call`, `sound_loop`, `drum_note`…), con le intestazioni dei
canali in `audio/headers/musicheaders.asm`. `gen_musica.py` li traduce nel bytecode del motore
di Rosso e Blu, che `musica.cpp` interpreta; nello stesso script ci sono la tabella delle note
(calcolata dalla formula dell'APU), le quattro forme d'onda del canale 3 e i cinque strumenti
della batteria. I nomi `MUS_*` e il loro ordine non cambiano: li usa il firmware.

Per cambiare un brano: si modifica il suo `.asm`, si rigenera `musica_dati.h`, si ascolta con
`make musica` (in `tools/emu/`) e si controlla con `python3 tools/confronta_musica.py`, che
segnala ogni frammento di 5 note con gli stessi intervalli e lo stesso ritmo di una musica di
Rosso, Blu o Giallo (scarica i sorgenti di pret per commit in `tools/musica/.cache_pret/`,
ignorata da git). Le durate di intro e giro sono anche nei test C++ (`test/test_musica.cpp`).

Solo per uso personale, `python3 tools/gen_musica.py --pret` scrive un `musica_dati.h` con le
musiche originali di Rosso, Blu e Giallo, scaricate nella stessa cache: quel file non va mai
committato (`TestGeneratedFiles` lo segnala), e `python3 tools/gen_musica.py` torna ai brani
del progetto.

## Sprite

Tutti gli sprite vengono da [PMD SpriteCollab](https://github.com/PMDCollab/SpriteCollab)
(CC BY-NC 4.0; gli autori sono in [`CREDITI_SPRITE.md`](CREDITI_SPRITE.md)). **Il repository e
il sito non li contengono**: l'installer e la PWA scaricano i PNG da SpriteCollab, a un commit
fissato, e li convertono nel browser con `web/pmd.js`; i file convertiti restano nella Cache
Storage, così un'interruzione riprende da lì.

`web/pmd.js` è un porting fedele di `tools/pack_pmd.py` e `tools/make_thumbs.py`: a parità di
PNG i file sono identici byte per byte (`TestPmdJs` lo prova con sprite disegnati dal test). Il
commit di SpriteCollab è lo stesso nei due (`COMMIT`): per aggiornarlo si cambia in entrambi e
si rigenera `docs/CREDITI_SPRITE.md` con `tools/crediti_sprite.py`.

In locale, per l'emulatore e per caricarli sulla scheda via USB senza browser:

```bash
python3 tools/pack_pmd.py [dex...]   # scarica e impacchetta -> tools/sdcard/mons/p[s]NNN.bin (PGK2)
python3 tools/make_thumbs.py         # miniature del Pokédex -> thumbs.bin (PGTH)
python3 tools/send_sd.py             # invia tools/sdcard/mons/* alla SD della scheda
```

`tools/sdcard/mons/` è ignorata da git. Senza sprite l'emulatore usa quelli di riserva (`make
smoke` funziona lo stesso), ma `tools/foto_doc.py` si ferma, perché le schermate della
documentazione li mostrano. La microSD deve essere in FAT32: il firmware non la formatta mai.

## Invarianti (li controllano i test)

- **Testo**: le lingue latine usano il font 5×7 CP437 di Arduino_GFX, quindi in `i18n.cpp` le
  lettere accentate sono **escape ottali** (`\240`, `\202`…), mai UTF-8; giapponese e coreano
  usano UTF-8 e i font unifont di U8g2. Misura con `textW()`/`centerX()`, non con `strlen()`.
  I letterali in `PokeGotchi.ino`, `ui_*` e `console.cpp` sono ASCII.
- **Tabelle i18n**: l'ordine di `StrId` in `i18n.h` coincide con le righe di `STRINGS`; ogni
  lingua ha le stesse voci e gli stessi `%` nello stesso ordine. Un formato nuovo va anche in
  `EXPECTED` di `test/test_i18n.cpp`.
- **Buffer**: ogni `snprintf(buf, sizeof(buf), T(S_x), ...)` è verificato nel caso peggiore in
  tutte le lingue; un nuovo `%s` va dichiarato in `TestBufferDiTesto.ARGS_S`.
- **Nomi delle specie** a schermo: ASCII maiuscolo, al massimo 12 caratteri (8 in CJK).
- **Versione**: una sola, in semver, in `ui_common.h`, `web/manifest.json` e `version.txt`
  (`TestVersione`). Non si cambia a mano: la aggiorna release-please.
- **Nome**: il nome del progetto originale compare solo nei crediti e nel codice di migrazione
  (`TestNomeProgetto`).

## Insidie

- I timer sono scadenze assolute di `millis()`: confrontale solo con `timeLeft()` (`pet.h`).
- Una scrittura della NVS blocca entrambi i core per ~1 s: i salvataggi periodici aspettano lo
  schermo attenuato (`savePending()`/`flushSave()`), e non avvengono con la radio accesa né
  durante lo scaricamento di un aggiornamento.
- I valori di `enum Lang` sono salvati: le lingue nuove vanno in fondo. `LANG_DEFAULT`
  (italiano) vale solo senza una lingua salvata.
- Ogni chiave NVS nuova sta nel namespace esportato (`SAVE_NS_PET`); le uniche eccezioni sono
  il giornale degli scambi (`scambio`) e le credenziali del WiFi (`wifi`), che non devono finire
  nei salvataggi.
- Il WiFi dell'aggiornamento (libreria WiFi di Arduino) e la radio del collegamento (solo il
  driver, `linknow.cpp`) non sono mai accesi insieme: `halRadioBegin()` rifiuta col WiFi acceso.
- L'installer flasha due pezzi (`boot.bin` a `0x0`, `pokegotchi.bin` a `0xE000`): un'immagine
  unica coprirebbe di `0xFF` la NVS a `0x9000` e cancellerebbe la partita.

## Release

Le release sono automatiche, con [release-please](https://github.com/googleapis/release-please)
(`.github/workflows/release.yml`):

1. I commit su `main` seguono i *conventional commits*: `feat: ...` per una funzione nuova,
   `fix: ...` per una correzione, `feat!: ...` o `BREAKING CHANGE:` per un cambiamento
   incompatibile; `docs:`, `test:`, `ci:`, `chore:` non fanno salire la versione.
2. A ogni push la CI tiene aperto un PR di release che alza la versione in `ui_common.h`,
   `web/manifest.json` e `version.txt` e scrive la voce di `CHANGELOG.md`.
3. Unire quel PR crea il tag e la release; la CI compila e allega
   `pokegotchi-<ver>-ota.bin`, il manifest `pokegotchi.json` (che le schede leggono per
   aggiornarsi), le due parti dell'installer e il libretto in PDF, poi ripubblica il sito.

Il repository delle release è quello della CI (`github.repository`, passato come `PG_REPO`):
un fork si aggiorna dalle proprie release.

### Provare un aggiornamento prima di pubblicarlo

Con `DEBUG ON`, il comando `OTA URL http://<ip del pc>:8000/pokegotchi.json` fa leggere alla
scheda un manifest servito sulla rete di casa (anche in `http`), fino al riavvio. Prepara
`dist/` con `PG_REPO` qualsiasi, correggi l'`url` del manifest verso il server locale e servi
la cartella con `python3 tools/serve_web.py`.

## CI

`.github/workflows/ci.yml` esegue a ogni push e pull request i test (con AddressSanitizer e lo
smoke test), compila il firmware, la PWA, il libretto e il sito, e ne conserva gli artefatti.
`pages.yml` pubblica il sito su GitHub Pages (sorgente «GitHub Actions» nelle impostazioni del
repository).
