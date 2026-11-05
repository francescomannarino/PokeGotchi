#!/usr/bin/env python3
"""Scarica i sorgenti audio dei disassemblaggi di pret (pokered, pokeyellow) a un commit
fissato, in tools/musica/.cache_pret/ (ignorata da git). Li usano solo gli strumenti locali:

  - tools/confronta_musica.py, che confronta i brani originali con le musiche di Rosso/Blu
    e Giallo;
  - tools/gen_musica.py --pret, che produce un musica_dati.h con le musiche di pret, solo per
    uso personale.

Le musiche sono di Junichi Masuda, (C) Nintendo / Creatures / GAME FREAK: non stanno nel
repository e non sono coperte dalla sua licenza.

  python3 tools/musica_pret.py   # scarica (se manca) e stampa la cartella
"""
import io
import os
import sys
import tarfile
import urllib.request

QUI = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(QUI, 'musica', '.cache_pret')

# repository -> commit (l'archivio di GitHub di quel commit è immutabile)
COMMIT = {
    'pokered': '0dbf95cd12f5b1511c62ce69efe283000f780a8f',
    'pokeyellow': 'ca1e8ba5c866e02cfa350ec9b3e5ce2e92f9ac39',
}


def _scarica(repo, sha, dest):
    url = f'https://codeload.github.com/pret/{repo}/tar.gz/{sha}'
    print(f'scarico {url}', file=sys.stderr)
    with urllib.request.urlopen(url, timeout=120) as r:
        dati = r.read()
    tmp = dest + '.parziale'
    os.makedirs(tmp, exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(dati), mode='r:gz') as tar:
        # solo i file .asm sotto audio/, senza la cartella radice dell'archivio
        for m in tar.getmembers():
            parti = m.name.split('/', 1)
            if len(parti) < 2 or not m.isfile() or not parti[1].startswith('audio/') \
                    or not parti[1].endswith('.asm') or '..' in parti[1].split('/'):
                continue
            percorso = os.path.join(tmp, parti[1])
            os.makedirs(os.path.dirname(percorso), exist_ok=True)
            with tar.extractfile(m) as src, open(percorso, 'wb') as out:
                out.write(src.read())
    os.rename(tmp, dest)


def cartella():
    """La cartella con pokered/ e pokeyellow/ (ognuna con il suo audio/), scaricata se manca."""
    for repo, sha in COMMIT.items():
        dest = os.path.join(CACHE, f'{repo}-{sha[:12]}')
        if not os.path.isdir(dest):
            _scarica(repo, sha, dest)
    # gen_musica e confronta_musica cercano <radice>/pokered e <radice>/pokeyellow
    radice = os.path.join(CACHE, 'radice')
    os.makedirs(radice, exist_ok=True)
    for repo, sha in COMMIT.items():
        link = os.path.join(radice, repo)
        bersaglio = os.path.join('..', f'{repo}-{sha[:12]}')
        if os.path.islink(link) and os.readlink(link) != bersaglio:
            os.unlink(link)
        if not os.path.lexists(link):
            os.symlink(bersaglio, link)
    return radice


if __name__ == '__main__':
    print(cartella())
