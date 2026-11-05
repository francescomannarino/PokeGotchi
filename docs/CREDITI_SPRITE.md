# Crediti degli sprite

<!-- GENERATO da tools/crediti_sprite.py: non modificare a mano -->

Gli sprite animati dei Pokémon vengono da [PMD SpriteCollab](https://github.com/PMDCollab/SpriteCollab), al commit
[`c163410`](https://github.com/PMDCollab/SpriteCollab/tree/c163410723fbc42460f86d63f6390ad90632082d), e sono distribuiti dai loro autori con licenza
[Creative Commons Attribuzione - Non commerciale 4.0 Internazionale (CC BY-NC 4.0)](https://creativecommons.org/licenses/by-nc/4.0/deed.it).
Gli autori di ogni sprite sono quelli indicati da SpriteCollab (`tracker.json` e
`credit_names.txt` dello stesso commit); per primo l'autore principale.

PokeGotchi non contiene né pubblica gli sprite: l'installer e la PWA li scaricano da
SpriteCollab e li convertono **sul dispositivo dell'utente**; chi sviluppa li può convertire in
locale con `tools/pack_pmd.py`. I personaggi Pokémon sono © Nintendo / Creatures /
GAME FREAK; SpriteCollab e PokeGotchi sono progetti non commerciali di fan.

## Modifiche

La conversione (`web/pmd.js`, uguale a `tools/pack_pmd.py` e `tools/make_thumbs.py`) cambia
gli sprite così:

- **ritaglio**: si tengono solo le azioni Idle, Walk, Sleep, Eat, Hurt, Attack, Pose, Hop, Nod, DeepBreath, Sit, una sola direzione per azione (di fronte;
  la camminata verso destra e verso sinistra) e al massimo 24 fotogrammi;
- **palette indicizzata**: al massimo 255 colori in RGB565, e i pixel con alfa sotto 128
  diventano trasparenti;
- **ritmo rallentato**: ogni fotogramma dura 1,4 volte l'originale,
  almeno 70 ms;
- **formato**: il file PGK2 della scheda (`p[s]NNN.bin`) e, dal primo fotogramma di Idle,
  una miniatura di al massimo 40×40 pixel per il Pokédex (`thumbs.bin`).

## Autori per specie

39 autori con nome, più chi ha scelto di restare anonimo. «Shiny» è la
variante a colori alternativi (`0000/0001` in SpriteCollab).

| N. | Specie | Sprite | Shiny |
|---:|---|---|---|
| 1 | Bulbasaur | CHUNSOFT | ShyStarryRain |
| 2 | Ivysaur | CHUNSOFT | SilverDeoxys563 |
| 3 | Venusaur | CHUNSOFT | SilverDeoxys563 |
| 4 | Charmander | CHUNSOFT | ShyStarryRain |
| 5 | Charmeleon | CHUNSOFT, bwappi, Grimlin | ShyStarryRain, Emmuffin |
| 6 | Charizard | CHUNSOFT, Emboarger | SilverDeoxys563, Emboarger, Emmuffin |
| 7 | Squirtle | CHUNSOFT | SilverDeoxys563, ShyStarryRain |
| 8 | Wartortle | CHUNSOFT | Emmuffin |
| 9 | Blastoise | CHUNSOFT | SilverDeoxys563 |
| 10 | Caterpie | CHUNSOFT | SilverDeoxys563 |
| 11 | Metapod | CHUNSOFT | SilverDeoxys563 |
| 12 | Butterfree | CHUNSOFT | Emmuffin |
| 13 | Weedle | CHUNSOFT | ShyStarryRain |
| 14 | Kakuna | CHUNSOFT | SilverDeoxys563 |
| 15 | Beedrill | CHUNSOFT | SilverDeoxys563 |
| 16 | Pidgey | CHUNSOFT, JFain | SilverDeoxys563 |
| 17 | Pidgeotto | CHUNSOFT | Moo, bwappi |
| 18 | Pidgeot | CHUNSOFT | SilverDeoxys563 |
| 19 | Rattata | CHUNSOFT | ShyStarryRain |
| 20 | Raticate | CHUNSOFT | SilverDeoxys563, 3Monika4 |
| 21 | Spearow | CHUNSOFT | SilverDeoxys563 |
| 22 | Fearow | CHUNSOFT | SilverDeoxys563 |
| 23 | Ekans | CHUNSOFT, Emmuffin | Emmuffin |
| 24 | Arbok | CHUNSOFT | Moo, Morei |
| 25 | Pikachu | CHUNSOFT | SilverDeoxys563 |
| 26 | Raichu | CHUNSOFT, Grimlin, Top_Kec | G〜 |
| 27 | Sandshrew | CHUNSOFT, Emboarger, 3P1C | ShyStarryRain |
| 28 | Sandslash | CHUNSOFT, Emmuffin | SilverDeoxys563, XModxGodX, Emmuffin |
| 29 | Nidoran h | CHUNSOFT | lokatts |
| 30 | Nidorina | CHUNSOFT | Audino, baronessfaron |
| 31 | Nidoqueen | CHUNSOFT | Emmuffin |
| 32 | Nidoran m | CHUNSOFT | Tacocoa |
| 33 | Nidorino | CHUNSOFT | Anonymous |
| 34 | Nidoking | CHUNSOFT | Tacocoa |
| 35 | Clefairy | CHUNSOFT | Tacocoa |
| 36 | Clefable | CHUNSOFT | Tacocoa |
| 37 | Vulpix | CHUNSOFT | Lovi |
| 38 | Ninetales | CHUNSOFT, baronessfaron | Emmuffin |
| 39 | Jigglypuff | CHUNSOFT, JFain | Emmuffin |
| 40 | Wigglytuff | CHUNSOFT | Lovi, Morei |
| 41 | Zubat | CHUNSOFT | Tacocoa |
| 42 | Golbat | CHUNSOFT | Tacocoa |
| 43 | Oddish | CHUNSOFT | Tacocoa |
| 44 | Gloom | CHUNSOFT | Tacocoa |
| 45 | Vileplume | CHUNSOFT | Emmuffin |
| 46 | Paras | CHUNSOFT | Tacocoa |
| 47 | Parasect | CHUNSOFT | ShyStarryRain |
| 48 | Venonat | CHUNSOFT, anomalocaris | Tacocoa, Emmuffin |
| 49 | Venomoth | CHUNSOFT | Anonymous |
| 50 | Diglett | CHUNSOFT | El_Pangoro_Parse |
| 51 | Dugtrio | CHUNSOFT | El_Pangoro_Parse |
| 52 | Meowth | CHUNSOFT | El_Pangoro_Parse |
| 53 | Persian | CHUNSOFT | El_Pangoro_Parse |
| 54 | Psyduck | CHUNSOFT | anonimo, ShyStarryRain |
| 55 | Golduck | CHUNSOFT | El_Pangoro_Parse |
| 56 | Mankey | CHUNSOFT | El_Pangoro_Parse |
| 57 | Primeape | CHUNSOFT | G〜 |
| 58 | Growlithe | CHUNSOFT, Emmuffin, JustAGunk | Audino, Emmuffin |
| 59 | Arcanine | CHUNSOFT | Tacocoa, baronessfaron |
| 60 | Poliwag | CHUNSOFT | El_Pangoro_Parse |
| 61 | Poliwhirl | CHUNSOFT | El_Pangoro_Parse, 3Monika4 |
| 62 | Poliwrath | CHUNSOFT | El_Pangoro_Parse |
| 63 | Abra | CHUNSOFT, Emmuffin | Tacocoa, 3Monika4, Emmuffin |
| 64 | Kadabra | CHUNSOFT | El_Pangoro_Parse |
| 65 | Alakazam | CHUNSOFT | Morei, Emmuffin |
| 66 | Machop | CHUNSOFT, Emboarger | Morei |
| 67 | Machoke | CHUNSOFT | Tacocoa |
| 68 | Machamp | CHUNSOFT | Tacocoa |
| 69 | Bellsprout | CHUNSOFT | Audino |
| 70 | Weepinbell | CHUNSOFT | Anonymous |
| 71 | Victreebel | CHUNSOFT | Emmuffin |
| 72 | Tentacool | CHUNSOFT | Tacocoa |
| 73 | Tentacruel | CHUNSOFT | Tacocoa |
| 74 | Geodude | CHUNSOFT | El_Pangoro_Parse |
| 75 | Graveler | CHUNSOFT, Emmuffin | Emmuffin |
| 76 | Golem | CHUNSOFT, Emmuffin | Emmuffin, El_Pangoro_Parse |
| 77 | Ponyta | CHUNSOFT, JFain | Tacocoa, Emmuffin |
| 78 | Rapidash | CHUNSOFT | El_Pangoro_Parse, Noo |
| 79 | Slowpoke | CHUNSOFT | ShyStarryRain |
| 80 | Slowbro | CHUNSOFT | Tacocoa |
| 81 | Magnemite | CHUNSOFT | El_Pangoro_Parse |
| 82 | Magneton | CHUNSOFT | El_Pangoro_Parse |
| 83 | Farfetchd | CHUNSOFT | Lovi |
| 84 | Doduo | CHUNSOFT | Anonymous |
| 85 | Dodrio | CHUNSOFT | Anonymous |
| 86 | Seel | CHUNSOFT | Tacocoa |
| 87 | Dewgong | CHUNSOFT | Tacocoa |
| 88 | Grimer | CHUNSOFT | Emmuffin |
| 89 | Muk | CHUNSOFT | G〜 |
| 90 | Shellder | CHUNSOFT | El_Pangoro_Parse |
| 91 | Cloyster | CHUNSOFT | El_Pangoro_Parse, Emmuffin |
| 92 | Gastly | CHUNSOFT | El_Pangoro_Parse, ShyStarryRain |
| 93 | Haunter | CHUNSOFT | Tacocoa |
| 94 | Gengar | CHUNSOFT, Emboarger | Tacocoa, Emboarger |
| 95 | Onix | CHUNSOFT | El_Pangoro_Parse |
| 96 | Drowzee | CHUNSOFT | El_Pangoro_Parse |
| 97 | Hypno | CHUNSOFT | Lovi |
| 98 | Krabby | CHUNSOFT | Tacocoa, G〜 |
| 99 | Kingler | CHUNSOFT | Tacocoa |
| 100 | Voltorb | CHUNSOFT, Emboarger | Emmuffin |
| 101 | Electrode | CHUNSOFT | Tacocoa, Noo |
| 102 | Exeggcute | CHUNSOFT | Prismatic, Noo |
| 103 | Exeggutor | CHUNSOFT | Audino, Noo |
| 104 | Cubone | CHUNSOFT, Sceptile, Some_Shinx | Emmuffin, Some_Shinx |
| 105 | Marowak | CHUNSOFT | El_Pangoro_Parse |
| 106 | Hitmonlee | CHUNSOFT | Tacocoa |
| 107 | Hitmonchan | CHUNSOFT | Tacocoa |
| 108 | Lickitung | CHUNSOFT | Audino |
| 109 | Koffing | CHUNSOFT | Tacocoa |
| 110 | Weezing | CHUNSOFT | El_Pangoro_Parse |
| 111 | Rhyhorn | CHUNSOFT | Tacocoa |
| 112 | Rhydon | CHUNSOFT | Tacocoa |
| 113 | Chansey | CHUNSOFT | Tacocoa |
| 114 | Tangela | CHUNSOFT | Anonymous |
| 115 | Kangaskhan | CHUNSOFT | Emmuffin |
| 116 | Horsea | CHUNSOFT | Reppamon |
| 117 | Seadra | CHUNSOFT | Tacocoa |
| 118 | Goldeen | CHUNSOFT | Tacocoa |
| 119 | Seaking | CHUNSOFT | Tacocoa |
| 120 | Staryu | CHUNSOFT | Adrian |
| 121 | Starmie | CHUNSOFT | Anonymous, Noo |
| 122 | Mr. mime | CHUNSOFT | Lovi |
| 123 | Scyther | CHUNSOFT | Tacocoa |
| 124 | Jynx | CHUNSOFT | Audino, ShyStarryRain |
| 125 | Electabuzz | CHUNSOFT | Scizivire |
| 126 | Magmar | CHUNSOFT | Lovi |
| 127 | Pinsir | CHUNSOFT | El_Pangoro_Parse |
| 128 | Tauros | CHUNSOFT | El_Pangoro_Parse |
| 129 | Magikarp | CHUNSOFT | Scizivire, Noo |
| 130 | Gyarados | CHUNSOFT | Noo, Emmuffin |
| 131 | Lapras | CHUNSOFT | RaoKurai |
| 132 | Ditto | CHUNSOFT, Caitemis | ShyStarryRain |
| 133 | Eevee | CHUNSOFT | baronessfaron |
| 134 | Vaporeon | CHUNSOFT | Emmuffin |
| 135 | Jolteon | CHUNSOFT, dmDash | Lovi, Morei |
| 136 | Flareon | CHUNSOFT, Ginnie, JFain | Tacocoa, ShyStarryRain, FlowerSnek |
| 137 | Porygon | CHUNSOFT | Tacocoa |
| 138 | Omanyte | CHUNSOFT | Tacocoa |
| 139 | Omastar | CHUNSOFT | Tacocoa |
| 140 | Kabuto | CHUNSOFT, anomalocaris | Emmuffin |
| 141 | Kabutops | CHUNSOFT | Tacocoa |
| 142 | Aerodactyl | CHUNSOFT | El_Pangoro_Parse |
| 143 | Snorlax | CHUNSOFT | El_Pangoro_Parse |
| 144 | Articuno | CHUNSOFT | El_Pangoro_Parse, Emmuffin |
| 145 | Zapdos | CHUNSOFT | Emmuffin |
| 146 | Moltres | CHUNSOFT | Emmuffin |
| 147 | Dratini | CHUNSOFT, JFain | Pencil |
| 148 | Dragonair | CHUNSOFT | Lovi |
| 149 | Dragonite | CHUNSOFT, SethY, Deeshura | Deeshura |
| 150 | Mewtwo | CHUNSOFT, JFain | El_Pangoro_Parse, JFain |
| 151 | Mew | CHUNSOFT, JFain | Anonymous, Emboarger |
