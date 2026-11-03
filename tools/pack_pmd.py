#!/usr/bin/env python3
"""Impacchettatore di sprite PMD (SpriteCollab) per la schermata principale.

Genera /mons/pNNN.bin (e psNNN.bin shiny) in formato PGK2 multi-azione:

  char[4] "PGK2"
  u8  nActs
  u16 palCount
  u16 pal[palCount]                  (RGB565)
  per azione:
    u8 id, u8 w, u8 h, u8 nFrames
    u16 ms[nFrames]
    u8 data[w*h*nFrames]             (indici, 0xFF trasparente)

Azioni: 0 Idle, 1 WalkL, 2 WalkR, 3 Sleep, 4 Eat, 5 Hurt, 6 Attack,
7 Pose, 8 Hop, 9 Nod, 10 DeepBreath, 11 Sit. Quelle mancanti si omettono.

  python3 tools/pack_pmd.py             # i 151, normale + shiny
  python3 tools/pack_pmd.py 7 25        # dex specifici
  python3 tools/pack_pmd.py normal 1 4  # solo normali
"""
import os
import struct
import sys
import urllib.request
import xml.etree.ElementTree as ET
from PIL import Image

OUT = os.path.join(os.path.dirname(__file__), 'sdcard', 'mons')
CACHE = os.path.join(os.path.dirname(__file__), 'pmd_cache')
BASE = 'https://raw.githubusercontent.com/PMDCollab/SpriteCollab/master/sprite'
SLOW = 1.4          # il ritmo originale di PMD sembra veloce nel tamagotchi
MIN_MS = 70
ALPHA_T = 128

# (id, nome dell'azione, riga dello sheet) — riga None = 0 se ce n'è una sola
# direzioni dello sheet: 0 giù, 2 DESTRA, 6 SINISTRA (verificato sulla scheda)
ACTIONS = [
    (0, 'Idle', 0),
    (1, 'Walk', 6),   # sinistra
    (2, 'Walk', 2),   # destra
    (3, 'Sleep', 0),
    (4, 'Eat', 0),
    (5, 'Hurt', 0),
    (6, 'Attack', 0),
    (7, 'Pose', 0),
    (8, 'Hop', 0),
    (9, 'Nod', 0),
    (10, 'DeepBreath', 0),
    (11, 'Sit', 0),
]


def fetch(url, dest):
    if os.path.exists(dest):
        return True
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        data = urllib.request.urlopen(req, timeout=30).read()
        open(dest, 'wb').write(data)
        return True
    except Exception:
        return False


def rgb565(r, g, b):
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


def load_animdata(folder):
    path = os.path.join(folder, 'AnimData.xml')
    anims = {}
    tree = ET.parse(path)
    for a in tree.getroot().find('Anims'):
        name = a.find('Name').text
        if a.find('FrameWidth') is None:
            # alias (CopyOf): punta a un'altra animazione
            copy = a.find('CopyOf')
            if copy is not None:
                anims[name] = ('copy', copy.text)
            continue
        anims[name] = (int(a.find('FrameWidth').text), int(a.find('FrameHeight').text),
                       [int(d.text) for d in a.find('Durations')], name)
    # risolve l'alias (il PNG è quello dell'animazione originale)
    for k, v in list(anims.items()):
        if isinstance(v, tuple) and v[0] == 'copy':
            anims[k] = anims.get(v[1])
    return anims


def pack(dexnum, shiny=False):
    sub = '/0000/0001' if shiny else ''
    folder = os.path.join(CACHE, f'{dexnum:04d}{"s" if shiny else ""}')
    base = f'{BASE}/{dexnum:04d}{sub}'
    if not fetch(f'{base}/AnimData.xml', os.path.join(folder, 'AnimData.xml')):
        raise RuntimeError('sin AnimData.xml')
    anims = load_animdata(folder)

    colmap, pal = {}, []
    packed = []
    for aid, name, row in ACTIONS:
        if name not in anims or anims[name] is None:
            continue
        fw, fh, durs, srcname = anims[name]
        png = os.path.join(folder, f'{srcname}-Anim.png')
        if not fetch(f'{base}/{srcname}-Anim.png', png):
            continue
        im = Image.open(png).convert('RGBA')
        rows = im.size[1] // fh
        r = row if rows > row else 0
        nf = min(len(durs), im.size[0] // fw, 24)
        data = bytearray()
        for i in range(nf):
            fr = im.crop((i * fw, r * fh, (i + 1) * fw, (r + 1) * fh))
            for px in fr.getdata():
                if px[3] < ALPHA_T:
                    data.append(0xFF)
                    continue
                k = px[:3]
                if k not in colmap:
                    if len(pal) >= 255:
                        # più vicino (raro in PMD, palette corte)
                        k2 = min(colmap, key=lambda c: sum((a-b)**2 for a, b in zip(c, k)))
                        colmap[k] = colmap[k2]
                    else:
                        colmap[k] = len(pal)
                        pal.append(k)
                data.append(colmap[k])
        ms = [max(MIN_MS, round(d * 1000 / 60 * SLOW)) for d in durs[:nf]]
        packed.append((aid, fw, fh, nf, ms, bytes(data)))

    if not any(p[0] == 0 for p in packed):
        raise RuntimeError('sin Idle')

    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, f'p{"s" if shiny else ""}{dexnum:03d}.bin')
    with open(path, 'wb') as f:
        f.write(b'PGK2')
        f.write(struct.pack('<BH', len(packed), len(pal)))
        for r, g, b in pal:
            f.write(struct.pack('<H', rgb565(r, g, b)))
        for aid, fw, fh, nf, ms, data in packed:
            f.write(struct.pack('<4B', aid, fw, fh, nf))
            f.write(struct.pack(f'<{nf}H', *ms))
            f.write(data)
    kb = os.path.getsize(path) / 1024
    print(f"  -> p{'s' if shiny else ''}{dexnum:03d}.bin: {len(packed)} acciones, "
          f"{len(pal)} colores, {kb:.0f} KB")


if __name__ == '__main__':
    args = sys.argv[1:]
    solo_normal = 'normal' in args
    nums = [int(a) for a in args if a.isdigit()] or list(range(1, 152))
    fallos = []
    for n in nums:
        for sh in ([False] if solo_normal else [False, True]):
            try:
                print(f"#{n:03d}{' shiny' if sh else ''}")
                pack(n, sh)
            except Exception as e:
                print(f"  FALLO: {e}")
                fallos.append((n, sh))
    print(f"FALLOS: {fallos}" if fallos else "TODOS EMPAQUETADOS")
