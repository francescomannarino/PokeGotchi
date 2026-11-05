# Emulatore su PC

Il firmware **intero** compilato con g++ sul PC: `setup()` e `loop()` di `PokeGotchi.ino`
(trattato come `.cpp`), la UI, il gioco e la console, sopra la **vera** Arduino_GFX
(`Arduino_GFX` + `Arduino_Canvas`, font 5×7 CP437 e i sottoinsiemi unifont di U8g2). Al
posto dei driver della scheda c'è `hal_host.cpp`, l'implementazione PC di `hal.h`.

```bash
cd tools/emu
make                  # -> build/pokegotchi (serve SDL2)
./build/pokegotchi      # finestra 466×466 rotonda; console seriale sul terminale
make smoke            # 48 h accelerate senza finestra né suono, ogni schermata fotografata
make musica           # i 7 brani di battaglia in WAV (build/musica/), da ascoltare
make prova            # la prova di fattibilità della grafica (build/prova_gfx.png)
make web ...          # la PWA con em++: si lancia con bash tools/build_pwa.sh (-> web/gioca/)
```

Servono `g++`, `gcc`, `awk`, SDL2 (`sudo apt install libsdl2-dev`, `brew install sdl2`)
e le librerie installate da `arduino-cli` (le stesse del firmware: GFX Library for
Arduino e U8g2). Il Makefile le trova con `arduino-cli config get directories.user`;
altrimenti si passano `GFX_DIR=` e `U8G2_DIR=`.

## Comandi

Nella finestra: **mouse** = dito sul touch (tocco, trascinamento, pressione lunga),
**frecce** = scorrimenti, **P** = pulsante PWR (schermo on/off), **S** = screenshot
(`pokegotchi-NNN.png` nella cartella corrente), **Esc** = esci. Sensore di movimento (finto):
**I/J/K/L** tenuti = scheda inclinata di ~25° verso l'alto, sinistra, basso, destra, **Z** =
scossone, **C** = doppio colpetto. Il sensore c'è solo dal primo uso di questi tasti (o dei
comandi del copione): prima, come sul PC senza sensore, `halMotoPresente()` è falso.

La console seriale è il terminale: si scrivono gli stessi comandi della scheda (`STATS`,
`DEBUG ON`, `LVL 20`, `HATCH`, `EXPORT`, `BATTLE 25`…, vedi [COMANDI.md](../../COMANDI.md)).

| Opzione | Effetto |
|---|---|
| `--fast N` | il tempo scorre N volte più veloce (millis e RTC; `PET_TICK_MS` non cambia). Mentre il dito è giù o un gesto è in corso scorre normale |
| `--sd DIR` | la cartella che fa da SD (predefinita `tools/sdcard`; gli sprite di `mons/` li produce `python3 tools/pack_pmd.py && python3 tools/make_thumbs.py`, fuori dal repository; senza, il gioco usa gli sprite di riserva; i salvataggi vanno in `save/`, ignorata da git) |
| `--nvs FILE` | la NVS su file (predefinita `build/nvs.txt`); accanto, `FILE.rtc` tiene l'ora dell'RTC |
| `--epoch E` | l'ora iniziale dell'RTC (secondi unix) |
| `--seed S` | il seme di `random()`, per partite riproducibili |
| `--zoom Z` | la finestra Z volte più grande |
| `--muto` | niente audio (senza finestra è sempre muto) |
| `--headless --screenshot F.png [--ms T]` | senza finestra: gioca T ms (predefiniti 2000) e salva il fotogramma |
| `--headless --copione F` | senza finestra: esegue un copione (sotto) |

Tra un avvio e l'altro l'RTC fa come quello della scheda con la batteria tampone: riparte
dall'ultima ora vista (anche accelerata) più il tempo trascorso sul PC, quindi la
progressione offline si applica come sulla scheda. Per ricominciare da zero: `WIPE YES`
dalla console (con `DEBUG ON`), oppure si cancellano `build/nvs.txt*`. Il riavvio
(`WIPE YES`, importazione) rilancia l'emulatore con gli stessi argomenti.

## Audio

Musica ed effetti escono dalla scheda audio del PC come usciranno dall'altoparlante della
scheda: `hal_host.cpp` apre un'uscita SDL2 a **32 kHz mono** (SDL converte alla frequenza del
PC) e la sua callback chiama `musicaRiempi()` di [`musica.h`](../../musica.h), l'interprete
del motore audio di pokered sull'APU emulata di [`apu.h`](../../apu.h). Gli
effetti (`halSfx`) passano dallo stesso motore; `halMusica(MUS_…)` suona un brano. Il
volume 0–10 e l'interruttore MUSICA stanno nelle chiavi `vol` e `mus` della NVS (alla prima
lettura `vol` viene da `snd`: spento → 0, acceso → 7); si cambiano dal pannello Audio
dell'orologio. Con il pet addormentato o lo schermo spento (`P`) la musica va avanti in
silenzio, come sulla scheda, e `AUDIO` in console mostra il carico della callback. Con
`--muto` o `--headless` non si apre l'uscita: i comandi restano nella coda del motore e si
perdono.

`make musica` compila `musica_wav.cpp` (solo `apu.cpp` e `musica.cpp`, senza SDL) e scrive in
`build/musica/` un WAV per brano a 32 kHz, al volume 10: per le battaglie intro, un giro e
5 s della ripresa del ciclo; per le vittorie il brano intero più 1 s. Stampa il picco di
ogni brano e il costo del motore sul PC (campioni al secondo). Si ascoltano contro
l'originale (un emulatore Game Boy o un video del gioco): melodia, basso, tempo, vibrato.

## Collegamento tra due emulatori

La radio della scheda (ESP-NOW, accesa solo nella schermata COLLEGA) sul PC è UDP su
`127.0.0.1`: ogni emulatore che entra in COLLEGA prende la prima porta libera da 47470 (fino a
8 istanze) e un MAC finto `02:54:50:45:4D:0N`; il broadcast va alle altre porte. Basta
lanciare due emulatori con NVS diverse:

```bash
./build/pokegotchi --nvs build/a.txt &
./build/pokegotchi --nvs build/b.txt
```

e aprire COLLEGA (scheda, pagina Lotta) su entrambi: stesso codice, conferma, collegati.
`LINK` in console mostra lo stato. Su localhost non si perde nulla: perdite, duplicati e
riordino li prova `test/test_link.cpp`. Senza finestra due copioni vanno di pari passo con
`PAUSA` (sotto) invece di `ATTENDI`. La PWA non ha radio: COLLEGA dice «Radio non disponibile».

## Copione

Una riga per comando, `#` = commento; le coordinate sono quelle dello schermo 466×466.

| Comando | Effetto |
|---|---|
| `TOCCA x y` | un tocco |
| `SCORRI SU\|GIU\|SINISTRA\|DESTRA` | uno scorrimento di 200 px dal centro, nella direzione del dito |
| `PWR` | pressione breve del pulsante PWR |
| `ATTENDI ms` / `ORE h` | tempo di gioco, a passi di 20 ms × `--fast` |
| `PAUSA ms` | tempo reale, a passi di 20 ms: due emulatori collegati vanno di pari passo |
| `SERIALE riga` | una riga sulla console |
| `INCLINA x y` | gravità di base negli assi dello schermo, in g (x a destra, y in basso; `0 0` = in piano) |
| `SCUOTI` / `COLPETTI` | uno scossone (perso a sensore spento) / un doppio colpetto (solo con il sensore in VEGLIA) |
| `FOTO file.png` | il fotogramma attuale, con la maschera rotonda |
| `SCHERMATA NOME` | verifica la schermata attiva (`gScreen` senza `SCR_`: `MAIN`, `CARD`…); se è un'altra il copione fallisce |

`smoke.txt` è lo smoke test: sceglie l'iniziale, apre e fotografa tutte le schermate
(scheda e tastiera, impostazioni e salvataggi, Pokédex, minigiochi, battaglia), lascia
passare 48 h a schermo spento e chiude con la cerimonia d'addio. `make smoke` lo esegue
con `--fast 500` su una SD e una NVS nuove in `build/smoke/` (foto in `build/smoke/foto/`);
`test/run_tests.sh` lo lancia se SDL2 è installato.

## Cosa c'è

- `emu.cpp`: `main()`, la finestra SDL2, i tasti e il copione.
- `hal_host.cpp`: `hal.h` sul PC. Lo schermo è il framebuffer che `Arduino_Canvas::flush()`
  passa a un pannello finto; la SD è una cartella; la NVS è lo shim `Preferences` dei
  test (`test/shim/preferences.cpp`) salvato su file; batteria sempre carica e cavo USB
  collegato; l'audio SDL2 (sopra); la radio su UDP (sopra).
- `tocco_host.cpp`: il touch, condiviso con la PWA (`pwa/hal_web.cpp`): il dito del mouse
  e la coda dei gesti (frecce, copione). Un clic più breve di un fotogramma diventa un
  tocco in coda, così non si perde.
- `arduino_host.cpp`: il core Arduino del PC: `millis()` virtuale (avanza col ciclo
  principale, per `--fast` e per il copione), `random()`, `Serial` su stdin/stdout.
- `shim/`: `Arduino.h`, `pgmspace.h` (PROGMEM e `pgm_read_*` vuoti), `Print.h`,
  `WString.h` (la `String` che usano console e salvataggi) e un `Arduino_GFX_Library.h`
  ridotto a GFX + Canvas, per non trascinare bus e pannelli.
- `png.cpp`: scrittura di PNG senza dipendenze (deflate «stored»).
- `musica_wav.cpp`: `make musica` (WAV dei brani e costo del motore).
- `prova_gfx.cpp`: la prova di fattibilità della grafica.
- I font U8g2 si estraggono da `u8g2_fonts.c` (40 MB) nella cartella `build/`; l'elenco è
  `FONTS` nel Makefile.
