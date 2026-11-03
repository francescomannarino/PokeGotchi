#!/usr/bin/env python3
"""Impacchetta tutti gli sprite della SD (tools/sdcard/mons/*.bin) in un unico
file web/sprites.pak perché l'installer web li carichi con un clic.

Formato PGAK (little-endian):
  char[4]  "PGAK"
  uint16   count
  count x { uint8 nameLen; char name[nameLen]; uint32 size }   (indice)
  ...dati di ogni file, nello stesso ordine...

L'installer (web/index.html) lo scarica, lo divide secondo l'indice e manda ogni
file alla scheda con il protocollo PUT (come tools/send_sd.py).
"""
import glob
import os
import struct

HERE = os.path.dirname(__file__)
MONS = os.path.join(HERE, 'sdcard', 'mons')
OUT = os.path.join(HERE, '..', 'web', 'sprites.pak')


def main():
    files = sorted(glob.glob(os.path.join(MONS, '*.bin')))
    if not files:
        raise SystemExit('no hay sprites en ' + MONS)
    names = ['mons/' + os.path.basename(f) for f in files]
    blobs = [open(f, 'rb').read() for f in files]

    with open(OUT, 'wb') as o:
        o.write(b'PGAK')
        o.write(struct.pack('<H', len(files)))
        for name, blob in zip(names, blobs):
            nb = name.encode()
            o.write(struct.pack('<B', len(nb)))
            o.write(nb)
            o.write(struct.pack('<I', len(blob)))
        for blob in blobs:
            o.write(blob)

    total = sum(len(b) for b in blobs)
    print(f'{os.path.normpath(OUT)}: {len(files)} sprites, {total / 1048576:.1f} MB datos '
          f'({os.path.getsize(OUT) / 1048576:.1f} MB total)')


if __name__ == '__main__':
    main()
