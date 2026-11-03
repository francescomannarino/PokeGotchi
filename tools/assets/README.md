# Asset di PokeGotchi — da dove arrivano gli sprite

Gli sprite NON si modificano a mano né si conservano qui: si **scaricano dalle loro
fonti e si impacchettano** con gli script di `tools/`. Questa cartella è solo
documentazione del flusso (prima conteneva una proposta di importazione da PNG
che è diventata obsoleta).

## Il flusso reale

Due formati convivono nella microSD (`/mons/`), entrambi derivati da PMD SpriteCollab:

| Formato | Script | Fonte | Cos'è |
|---|---|---|---|
| **PGK2** `pNNN.bin` / `psNNN.bin` | `pack_pmd.py` | [PMD SpriteCollab](https://github.com/PMDCollab/SpriteCollab) (CC BY-NC) | Animazioni multi-azione (idle, walk, sleep, eat, hurt, attack, gesti) — usate in **tutto**: schermata principale e **Pokédex / galleria** |
| **PGTH** `thumbs.bin` | `make_thumbs.py` | (deriva dai PGK2) | Miniature 40×40 della galleria |

```bash
python3 tools/pack_pmd.py     # i 151 + shiny -> tools/sdcard/mons/p[s]NNN.bin
python3 tools/make_thumbs.py  # -> tools/sdcard/mons/thumbs.bin
python3 tools/send_sd.py      # invia tutto alla SD della scheda via USB
```

(`s` = variante shiny. `pack_pmd.py` accetta anche numeri di Pokédex singoli,
es. `python3 tools/pack_pmd.py 7 25`.)

## Formati binari

Definiti nelle intestazioni di ogni impacchettatore e analizzati in `sdmon.cpp`:

- **PGK1** (`SdMon`): `"PGK1"`, `u16 w,h,frames,frameMs`, `u16 palCount`,
  `u16 pal[]` (RGB565), `u8 data[frames*w*h]` (indice di palette, `0xFF`
  trasparente).
- **PGK2** (`PmdMon`): `"PGK2"`, `u8 nActs`, `u16 palCount`, `u16 pal[]`, e per
  azione `u8 id,w,h,nFrames` + `u16 ms[nFrames]` + `u8 data[w*h*nFrames]`.
- **PGTH** (`SdThumbs`): `"PGTH"`, `u16 count`, `u32 offset[count]`, e per
  miniatura `u8 w,h,palCount` + `u16 pal[]` + `u8 data[w*h]`.

Il firmware valida le dimensioni al caricamento, quindi un `.bin` troncato viene rifiutato senza
rompere nulla.

## Cache dei download

`pack_pmd.py` mette in cache i PNG originali di SpriteCollab in `tools/pmd_cache/`
(ignorata da git, rigenerabile). I `.bin` finali invece si versionano in
`tools/sdcard/mons/` come riserva.

> Vedi [CREDITS.md](../../CREDITS.md) per la provenienza e i termini degli
> sprite. Sono di terzi: non ridistribuire a fini commerciali.
