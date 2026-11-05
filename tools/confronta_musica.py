#!/usr/bin/env python3
"""Confronta i brani originali (tools/musica/originali/) con tutte le musiche di Rosso/Blu e
Giallo di pret, scaricate per commit in tools/musica/.cache_pret/ (vedi musica_pret.py).

  python3 tools/confronta_musica.py                # codice 1 se trova una corrispondenza
  python3 tools/confronta_musica.py --note 6       # sequenze di 6 note invece di 5
  python3 tools/confronta_musica.py --intervalli 8 # in più, solo gli intervalli (avvisi)

Per ogni canale melodico (1-3) si legge la successione delle note com'è scritta nel file
(senza seguire salti e chiamate); una pausa la interrompe. Due frammenti di N note sono uguali
se hanno gli stessi intervalli (in semitoni, quindi a meno di una trasposizione) e lo stesso
ritmo (le durate in rapporto alla prima, quindi a meno del tempo). Si ignorano le figure
troppo comuni per dire qualcosa:
  - i frammenti con meno di tre altezze diverse (note ribattute, oscillazioni tra due note);
  - gli accordi spezzati a valori uguali: tutte le durate uguali e le note di una sola triade
    maggiore o minore (l'accompagnamento ad arpeggio di qualunque brano).

È un'euristica, non una garanzia legale: serve a non riprendere per sbaglio un frammento
riconoscibile. Si esegue a mano, non nella CI (scarica da GitHub).
"""
import argparse
import os
import re
import sys
from fractions import Fraction

QUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, QUI)
import musica_pret  # noqa: E402

ORIGINALI = os.path.join(QUI, 'musica', 'originali')
NOTE = ['C_', 'C#', 'D_', 'D#', 'E_', 'F_', 'F#', 'G_', 'G#', 'A_', 'A#', 'B_']


def frasi(path):
    """Un file .asm -> [(canale, [(altezza, durata, riga), ...]), ...]: le note di fila
    tra una pausa e l'altra, per sezione (etichetta globale). Salta la batteria."""
    out = []
    canale, ottava, vel, frase, batteria = None, 4, 12, [], False

    def chiudi():
        nonlocal frase
        if canale and not batteria and len(frase) > 1:
            out.append((canale, frase))
        frase = []

    with open(path, encoding='utf-8', errors='replace') as fh:
        for n, riga in enumerate(fh, 1):
            r = riga.split(';', 1)[0].strip()
            if not r:
                continue
            m = re.fullmatch(r'(\w+)::?', r)
            if m:  # etichetta globale: un canale (o una sua parte)
                chiudi()
                canale = m.group(1)
                batteria = canale.endswith('_Ch4') or canale.endswith('_Ch8')
                continue
            parti = r.split(None, 1)
            cmd = parti[0]
            args = [a.strip() for a in parti[1].split(',')] if len(parti) > 1 else []
            if cmd in ('drum_note', 'drum_speed', 'noise_note'):
                batteria = True
                frase = []
            elif cmd == 'octave' and args:
                ottava = int(args[0])
            elif cmd in ('note_type', 'notetype') and args:
                vel = int(args[0])
            elif cmd == 'note' and len(args) == 2 and args[0] in NOTE:
                frase.append((ottava * 12 + NOTE.index(args[0]), int(args[1]) * vel,
                              f'{os.path.basename(path)}:{n}'))
            elif cmd == 'rest' or cmd in ('sound_ret', 'sound_loop', 'sound_jump'):
                chiudi()
    chiudi()
    return out


# le triadi maggiori e minori, come insiemi di classi di altezza
TRIADI = [frozenset({r % 12, (r + t) % 12, (r + 7) % 12}) for r in range(12) for t in (3, 4)]


def generico(f):
    """Una figura troppo comune per contare (vedi la docstring)."""
    if len({p for p, _, _ in f}) < 3:
        return True
    classi = {p % 12 for p, _, _ in f}
    return len({d for _, d, _ in f}) == 1 and any(classi <= t for t in TRIADI)


def chiavi(frase, n, ritmo=True):
    """I frammenti di n note di una frase: (chiave, riga d'inizio)."""
    for i in range(len(frase) - n + 1):
        f = frase[i:i + n]
        if generico(f):
            continue
        intervalli = tuple(b[0] - a[0] for a, b in zip(f, f[1:]))
        if ritmo:
            d0 = f[0][1]
            chiave = intervalli + tuple(Fraction(d, d0) for _, d, _ in f[1:])
        else:
            chiave = intervalli
        yield chiave, f[0][2]


def indice(cartelle, n, ritmo):
    idx = {}
    for cartella in cartelle:
        musica = os.path.join(cartella, 'audio', 'music')
        for nome in sorted(os.listdir(musica)):
            if not nome.endswith('.asm'):
                continue
            for canale, frase in frasi(os.path.join(musica, nome)):
                for chiave, dove in chiavi(frase, n, ritmo):
                    idx.setdefault(chiave, f'{os.path.basename(cartella)}/{dove} ({canale})')
    return idx


def confronta(n, ritmo, radice_pret):
    pret = indice([os.path.join(radice_pret, 'pokered'), os.path.join(radice_pret, 'pokeyellow')],
                  n, ritmo)
    trovati = []
    musica = os.path.join(ORIGINALI, 'audio', 'music')
    for nome in sorted(os.listdir(musica)):
        if not nome.endswith('.asm'):
            continue
        for canale, frase in frasi(os.path.join(musica, nome)):
            for chiave, dove in chiavi(frase, n, ritmo):
                if chiave in pret:
                    trovati.append((dove, canale, pret[chiave]))
    return trovati


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--note', type=int, default=5, help='note di fila (intervalli e ritmo)')
    ap.add_argument('--intervalli', type=int, default=0,
                    help='segnala anche N note con gli stessi intervalli, ritmo a parte')
    a = ap.parse_args()
    radice = musica_pret.cartella()
    trovati = confronta(a.note, True, radice)
    for dove, canale, loro in trovati:
        print(f'UGUALE ({a.note} note, intervalli e ritmo): {dove} ({canale}) ~ {loro}')
    if a.intervalli:
        for dove, canale, loro in confronta(a.intervalli, False, radice):
            print(f'avviso ({a.intervalli} note, solo intervalli): {dove} ({canale}) ~ {loro}')
    if trovati:
        print(f'{len(trovati)} frammenti in comune con le musiche di pret', file=sys.stderr)
        sys.exit(1)
    print(f'nessun frammento di {a.note} note in comune con le musiche di pret')


if __name__ == '__main__':
    main()
