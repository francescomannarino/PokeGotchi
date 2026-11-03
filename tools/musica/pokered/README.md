# Sorgenti delle musiche di Rosso/Blu (pret/pokered)

File copiati senza modifiche da [pret/pokered](https://github.com/pret/pokered), il
disassemblaggio di Pokémon Rosso/Blu. Le musiche di battaglia vengono dal commit
[`a536711`](https://github.com/pret/pokered/commit/a53671100ec22e5394ff26ec028d0812a238dae2)
(`a53671100ec22e5394ff26ec028d0812a238dae2`, preso il 23/10/2026):

| File | Contenuto |
|------|-----------|
| `audio/music/wildbattle.asm` | lotta contro un selvatico |
| `audio/music/trainerbattle.asm` | lotta contro un allenatore |
| `audio/music/gymleaderbattle.asm` | lotta contro un capopalestra |
| `audio/music/finalbattle.asm` | lotta finale contro il rivale |
| `audio/music/defeatedwildmon.asm` | vittoria su un selvatico |
| `audio/music/defeatedtrainer.asm` | vittoria su un allenatore |
| `audio/music/defeatedgymleader.asm` | vittoria su un capopalestra |
| `audio/notes.asm` | tabella delle frequenze delle 12 note (ottava più alta) |
| `audio/wave_samples.asm` | forme d'onda a 4 bit del canale 3 |
| `audio/headers/musicheaders2.asm` | intestazioni dei brani (canali e punti d'ingresso) |

La musica della Grotta Diglett e la sua batteria vengono dal commit
[`0dbf95c`](https://github.com/pret/pokered/commit/0dbf95cd12f5b1511c62ce69efe283000f780a8f)
(`0dbf95cd12f5b1511c62ce69efe283000f780a8f`, preso il 31/10/2026; i file della tabella sopra
sono uguali anche lì):

| File | Contenuto |
|------|-----------|
| `audio/music/dungeon2.asm` | Grotta Diglett, Bosco Smeraldo, Isole Spumarine (`data/maps/songs.asm`); 4 canali, il quarto è la batteria |
| `audio/headers/musicheaders3.asm` | intestazioni del banco di Dungeon2 |
| `audio/sfx/noise_instrument01_3.asm` … `noise_instrument19_3.asm` | gli strumenti di rumore che `drum_note` suona sul canale 8 (uguali nei tre banchi) |

Stanno nel repository perché `tools/gen_musica.py` (→ `musica_dati.h`) e i suoi test
devono funzionare senza rete. Per aggiornarli si ripete la copia da un commit nuovo e si
aggiorna questa pagina.

## Diritti

Le musiche sono di Junichi Masuda, © 1995–1996 Nintendo / Creatures Inc. / GAME FREAK inc.
Sono qui solo per uso personale, per far suonare al tamagotchi le musiche di battaglia e
della Grotta Diglett di Rosso/Blu; non sono coperte dalla licenza del resto del progetto. Il
disassemblaggio è un lavoro della comunità pret.
