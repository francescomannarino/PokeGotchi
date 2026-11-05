#!/usr/bin/env python3
"""Genera docs/CREDITI_SPRITE.md: gli autori degli sprite di PMD SpriteCollab che l'installer
e la PWA scaricano e convertono (web/pmd.js), specie per specie, normale e shiny.

Legge tracker.json e credit_names.txt di SpriteCollab allo stesso commit di tools/pack_pmd.py
e di web/pmd.js, scaricati in tools/pmd_cache/ (ignorata da git). Nel file finiscono solo i
nomi pubblici degli autori (la colonna Name di credit_names.txt), mai gli id Discord né i
contatti.

  python3 tools/crediti_sprite.py
"""
import json
import os
import sys
import urllib.request

QUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, QUI)
from dex_data import DEX  # noqa: E402
from pack_pmd import ACTIONS, CACHE, COMMIT, MIN_MS, SLOW  # noqa: E402

USCITA = os.path.join(QUI, '..', 'docs', 'CREDITI_SPRITE.md')
RAW = f'https://raw.githubusercontent.com/PMDCollab/SpriteCollab/{COMMIT}'
REPO = 'https://github.com/PMDCollab/SpriteCollab'
LICENZA = 'https://creativecommons.org/licenses/by-nc/4.0/deed.it'


def scarica(nome):
    dest = os.path.join(CACHE, nome)
    if not os.path.exists(dest):
        os.makedirs(CACHE, exist_ok=True)
        req = urllib.request.Request(f'{RAW}/{nome}', headers={'User-Agent': 'PokeGotchi crediti_sprite.py'})
        dati = urllib.request.urlopen(req, timeout=60).read()
        with open(dest + '.parziale', 'wb') as f:
            f.write(dati)
        os.rename(dest + '.parziale', dest)
    with open(dest, encoding='utf-8') as f:
        return f.read()


ANONIMO = 'anonimo'  # in credit_names.txt senza nome pubblico


def nomi_pubblici(testo):
    """credit_names.txt (Name, Discord, Contact separati da tab) -> {id o nome: nome}. Chi
    non ha un nome pubblico resta anonimo: il suo id Discord non si pubblica."""
    righe = testo.splitlines()
    testa = righe[0].split('\t')
    if testa[:2] != ['Name', 'Discord']:
        raise SystemExit(f'credit_names.txt: intestazione inattesa {testa}')
    nomi = {}
    for r in righe[1:]:
        p = r.split('\t')
        if len(p) >= 2 and p[1]:
            nomi[p[1]] = p[0].strip() or ANONIMO
        if p[0].strip():
            nomi.setdefault(p[0].strip(), p[0].strip())
    return nomi


def autori(voce, nomi, dove):
    """sprite_credit di una voce del tracker -> elenco di nomi (il principale per primo)"""
    if 'sprite_credit' not in voce:
        raise SystemExit(f'{dove}: manca sprite_credit (è cambiato il formato di tracker.json?)')
    c = voce['sprite_credit']
    if not isinstance(c, dict) or 'primary' not in c or 'secondary' not in c:
        raise SystemExit(f'{dove}: sprite_credit senza primary/secondary: {c!r}')
    elenco = []
    for x in [c['primary']] + list(c['secondary']):
        if not x:
            continue
        if x not in nomi:
            raise SystemExit(f'{dove}: autore {x!r} senza nome in credit_names.txt')
        if nomi[x] not in elenco:
            elenco.append(nomi[x])
    return elenco


def main():
    tracker = json.loads(scarica('tracker.json'))
    nomi = nomi_pubblici(scarica('credit_names.txt'))
    azioni = ', '.join(dict.fromkeys(n for _, n, _ in ACTIONS))
    righe = []
    tutti = set()
    for num, _slug, nome, *_ in DEX:
        voce = tracker[f'{num:04d}']
        normale = autori(voce, nomi, f'{num:04d}')
        try:
            shiny_voce = voce['subgroups']['0000']['subgroups']['0001']
        except KeyError:
            raise SystemExit(f'{num:04d}: manca lo shiny (subgroups 0000/0001)')
        shiny = autori(shiny_voce, nomi, f'{num:04d}/0000/0001')
        tutti.update(normale + shiny)
        cella = lambda a: ', '.join(a) if a else '—'  # noqa: E731
        righe.append(f'| {num} | {nome.capitalize()} | {cella(normale)} | {cella(shiny)} |')

    testo = f"""# Crediti degli sprite

<!-- GENERATO da tools/crediti_sprite.py: non modificare a mano -->

Gli sprite animati dei Pokémon vengono da [PMD SpriteCollab]({REPO}), al commit
[`{COMMIT[:7]}`]({REPO}/tree/{COMMIT}), e sono distribuiti dai loro autori con licenza
[Creative Commons Attribuzione - Non commerciale 4.0 Internazionale (CC BY-NC 4.0)]({LICENZA}).
Gli autori di ogni sprite sono quelli indicati da SpriteCollab (`tracker.json` e
`credit_names.txt` dello stesso commit); per primo l'autore principale.

PokeGotchi non contiene né pubblica gli sprite: l'installer e la PWA li scaricano da
SpriteCollab e li convertono **sul dispositivo dell'utente**; chi sviluppa li può convertire in
locale con `tools/pack_pmd.py`. I personaggi Pokémon sono © Nintendo / Creatures /
GAME FREAK; SpriteCollab e PokeGotchi sono progetti non commerciali di fan.

## Modifiche

La conversione (`web/pmd.js`, uguale a `tools/pack_pmd.py` e `tools/make_thumbs.py`) cambia
gli sprite così:

- **ritaglio**: si tengono solo le azioni {azioni}, una sola direzione per azione (di fronte;
  la camminata verso destra e verso sinistra) e al massimo 24 fotogrammi;
- **palette indicizzata**: al massimo 255 colori in RGB565, e i pixel con alfa sotto 128
  diventano trasparenti;
- **ritmo rallentato**: ogni fotogramma dura {str(SLOW).replace('.', ',')} volte l'originale,
  almeno {MIN_MS} ms;
- **formato**: il file PGK2 della scheda (`p[s]NNN.bin`) e, dal primo fotogramma di Idle,
  una miniatura di al massimo 40×40 pixel per il Pokédex (`thumbs.bin`).

## Autori per specie

{len(tutti - {ANONIMO})} autori con nome{', più chi ha scelto di restare anonimo' if ANONIMO in tutti else ''}. «Shiny» è la
variante a colori alternativi (`0000/0001` in SpriteCollab).

| N. | Specie | Sprite | Shiny |
|---:|---|---|---|
""" + '\n'.join(righe) + '\n'
    with open(USCITA, 'w', encoding='utf-8') as f:
        f.write(testo)
    print(f'{os.path.normpath(USCITA)}: {len(righe)} specie, {len(tutti)} autori')


if __name__ == '__main__':
    main()
