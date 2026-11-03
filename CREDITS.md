# Crediti

PokeGotchi è un progetto **non commerciale, per uso personale**. Non vende né
ridistribuisce commercialmente alcun materiale protetto da copyright. Pokémon e tutti i
nomi, i design e i personaggi correlati sono marchi e © di **Nintendo / Game Freak /
The Pokémon Company**.

Questo progetto non è affiliato né approvato da nessuna di queste aziende.

## Sprite e dati

| Risorsa | Fonte | Uso nel progetto |
|---|---|---|
| **Tutti gli sprite** (idle, camminata, sonno, mangiare, colpito, attacco…) | [PMD Sprite Collaboration (PMDCollab/SpriteCollab)](https://github.com/PMDCollab/SpriteCollab) | Sprite animati in stile Mystery Dungeon usati ovunque: schermata principale, scheda delle statistiche, minigioco, e griglia del Pokédex + vista dettaglio |
| **Statistiche base della gen 1** | [PokéAPI](https://pokeapi.co) | ATK/DEF/SPD/HP reali di ogni specie |
| **Nomi localizzati** di specie e mosse | [PokéAPI](https://pokeapi.co) | Scaricati da `tools/gen_names.py` in `tools/dex_names.py` e `tools/dex_move_names.py`, poi in `dex.h` e `moves.h` |
| **Dati di battaglia della gen 1** (tipi, Speciale, tabella dei tipi, mosse, learnset) | [pret/pokered](https://github.com/pret/pokered) | Valori di Rosso/Blu letti dal disassembly da `tools/fetch_gen1.py`, verificati a campione con PokéAPI, generati in `moves.h` |
| **Musiche di battaglia di Rosso/Blu** (7 brani, note, forme d'onda) | [pret/pokered](https://github.com/pret/pokered) (commit `a536711`) | Sorgenti copiati in `tools/musica/pokered/`, tradotti da `tools/gen_musica.py` nel bytecode del motore audio originale (`musica_dati.h`). Musiche di Junichi Masuda, © Nintendo / Creatures / GAME FREAK, solo per uso personale |
| **Musiche dei minigiochi**: Dungeon2 (Grotta Diglett) con i 19 strumenti di rumore della batteria, Surfing Pikachu (Surf) | [pret/pokered](https://github.com/pret/pokered) (commit `0dbf95c`), [pret/pokeyellow](https://github.com/pret/pokeyellow) (commit `ca1e8ba`) | Sorgenti copiati in `tools/musica/pokered/` e `tools/musica/pokeyellow/`, tradotti da `tools/gen_musica.py` in `musica_dati.h`. Musiche di Junichi Masuda, © Nintendo / Creatures / GAME FREAK, solo per uso personale. L'idea del Surf viene dalla Spiaggia di Pikachu di Pokémon Giallo |

Gli sprite di **SpriteCollab** sono opera della sua comunità di artisti secondo i
loro termini (Creative Commons Attribution-NonCommercial 4.0). I crediti per specie/per autore
sono nel [tracker.json](https://github.com/PMDCollab/SpriteCollab/blob/master/tracker.json)
del repository originale.
Un enorme grazie a tutta quella comunità per l'enorme quantità di lavoro.

> **Importante se riutilizzi questo repository:** i file di sprite impacchettati
> (`tools/sdcard/mons/*.bin`) derivano dalle fonti sopra. Non ridistribuirli
> commercialmente. Se pubblichi il progetto, l'approccio pulito è distribuire
> **solo il codice e gli script**, e far scaricare a ogni utente gli sprite dalle fonti
> originali e impacchettarli con `tools/pack_*.py` (o con l'installer
> web).

## Codice derivato e ispirazioni

| Progetto | Autore | Cosa se ne è preso | Licenza / tipo d'uso |
|---|---|---|---|
| [socquique/TamaPoke](https://github.com/socquique/TamaPoke) | Quique Tortosa | Progetto originale: firmware, strumenti, installer web, scocca 3D | MIT (codice), CC BY-NC-SA (scocca) — base del progetto |
| [DylanPDao/TamaPoke](https://github.com/DylanPDao/TamaPoke) | DylanPDao | Motore di battaglia e IA della CPU (`aiChooseMove`), modello degli effetti delle mosse (`EFFETTI` in `tools/dex_moves.py`), layout della schermata di battaglia (`ui_battle.cpp`), protocollo di collegamento ESP-NOW (`link.h/.cpp`: trasporto come puntatore a funzione, ritrasmissione con jitter, prova su un canale che perde in `test/test_link.cpp`), il modello host-autoritativo della battaglia in collegamento con l'azione dell'ospite numerata e il risultato completo del turno (`linkbattaglia.h/.cpp`), la coda tra il callback di ricezione ESP-NOW e il loop (`linknow.h/.cpp`) e, per la prova radio (`tools/prova_radio/`), l'idea dell'anello tra il callback di ricezione e il loop; l'idea della musica per tutta la battaglia (solo l'idea: le note sono tradotte dai sorgenti di pokered, non dalle sue tabelle) | MIT — codice derivato, con avviso «Derivato da DylanPDao/TamaPoke (MIT)» |
| [allenst486db/iTamaPoke](https://github.com/allenst486db/iTamaPoke) | allenst486db | Versione smartphone/PWA e salvataggi esportabili | Solo idee: la licenza non permette di riusarne il codice |
| [ShadowEnemyx/TamaPoke — Expanded](https://github.com/ShadowEnemyx/TamaPoke/tree/tamapoke-expanded-update) | ShadowEnemy | Idee per le battaglie (relazioni tra tipi), se verranno usate | Solo idee |
| [pret/pokered](https://github.com/pret/pokered), `audio/engine_2.asm` | comunità pret | L'algoritmo del motore audio (durata delle note con il resto frazionario, vibrato, `toggle_perfect_pitch`, chiamate e cicli, scrittura dei registri, batteria con gli strumenti di rumore sul canale 8), riscritto in C++ in `musica.cpp` | Disassemblaggio — solo l'algoritmo, nessun codice copiato |
| [Pan Docs](https://gbdev.io/pandocs/Audio.html) | comunità gbdev | Il comportamento dell'APU del Game Boy (canali, inviluppi, sweep, LFSR, sequencer, DAC, passa-alto del DMG) emulato in `apu.cpp` | Documentazione — solo idee |
| [Blip_Buffer](http://www.slack.net/~ant/libs/audio.html) | Shay Green (blargg) | L'idea della sintesi a banda limitata per gradini (BLEP in un buffer di impulsi che si integra), in `apu.cpp` | LGPL — solo l'idea, codice scritto da zero |

## Software / hardware

| Componente | Autore / fonte |
|---|---|
| GFX Library for Arduino | [moononournation](https://github.com/moononournation/Arduino_GFX) |
| SensorLib (touch CST9217, RTC PCF85063) | [Lewis He / lewisxhe](https://github.com/lewisxhe/SensorLib) |
| XPowersLib (PMU AXP2101) | [Lewis He / lewisxhe](https://github.com/lewisxhe/XPowersLib) |
| U8g2 (solo dati dei font CJK) | [olikraus](https://github.com/olikraus/u8g2) |
| Scheda e piedinatura | [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75) |
| Installer web | [ESP Web Tools](https://esphome.github.io/esp-web-tools/) (Nabu Casa) |
| Emulatore desktop (finestra, tasti, audio) | [SDL2](https://www.libsdl.org/) |
| PWA (firmware compilato in WebAssembly) | [Emscripten](https://emscripten.org/) |
| Core Arduino per ESP32 (WiFi, HTTPS con il bundle dei certificati, aggiornamento OTA) | [Espressif](https://github.com/espressif/arduino-esp32) |
| Release e changelog automatici | [release-please](https://github.com/googleapis/release-please) (Google) |
| Font 5×7 dei titoli del libretto e del sito (`tools/font5x7.py`) | glcdfont di [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library), ripreso da GFX Library for Arduino — licenza BSD, avviso sotto |

Il codice di PokeGotchi (firmware e strumenti) è opera originale, salvo le parti indicate in
«Codice derivato e ispirazioni».

### Licenza del font 5×7 (Adafruit GFX)

```
Software License Agreement (BSD License)

Copyright (c) 2012 Adafruit Industries.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
```
