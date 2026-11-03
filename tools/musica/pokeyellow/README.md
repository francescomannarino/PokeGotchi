# Sorgenti della musica del Surf (pret/pokeyellow)

File copiati senza modifiche da [pret/pokeyellow](https://github.com/pret/pokeyellow), il
disassemblaggio di Pokémon Giallo, al commit
[`ca1e8ba`](https://github.com/pret/pokeyellow/commit/ca1e8ba5c866e02cfa350ec9b3e5ce2e92f9ac39)
(`ca1e8ba5c866e02cfa350ec9b3e5ce2e92f9ac39`, preso il 31/10/2026):

| File | Contenuto |
|------|-----------|
| `audio/music/surfingpikachu.asm` | Surfing Pikachu, la musica della Spiaggia di Pikachu (3 canali) |
| `audio/headers/musicheaders4.asm` | intestazioni del banco di Surfing Pikachu |

Note e forme d'onda sono quelle di `../pokered/` (in Giallo le cinque usate sono uguali).
Stanno nel repository perché `tools/gen_musica.py` (→ `musica_dati.h`) e i suoi test devono
funzionare senza rete. Per aggiornarli si ripete la copia da un commit nuovo e si aggiorna
questa pagina.

## Diritti

La musica è di Junichi Masuda, © 1998 Nintendo / Creatures Inc. / GAME FREAK inc. È qui
solo per uso personale, per il minigioco del Surf del tamagotchi; non è coperta dalla licenza
del resto del progetto. Il disassemblaggio è un lavoro della comunità pret.
