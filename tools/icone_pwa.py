#!/usr/bin/env python3
"""Disegna le icone della PWA in pwa/icone/: una Pokéball con i colori della
cornice di pwa/stile.css.

  icona-192.png, icona-512.png   "any": la palla su fondo trasparente
  icona-maskable-512.png         "maskable": fondo pieno, palla dentro la zona sicura (80%)
  apple-touch-icon.png           180 px, fondo pieno (iOS riempie di nero la trasparenza)

Uso: python3 tools/icone_pwa.py   (serve Pillow)
"""
import os

from PIL import Image, ImageDraw

QUI = os.path.dirname(os.path.abspath(__file__))
USCITA = os.path.join(QUI, '..', 'pwa', 'icone')

SFONDO = (0x14, 0x16, 0x1c, 255)
ROSSO = (0xe3, 0x35, 0x0d, 255)
BIANCO = (0xf2, 0xf2, 0xf2, 255)
NERO = (0x16, 0x17, 0x1b, 255)
VETRO = (0, 0, 0, 255)
CERCHIO = (0xc9, 0xc9, 0xc9, 255)
SOVRA = 4  # sovracampionamento, per i bordi morbidi


def palla(lato, diametro, fondo):
    """Immagine `lato`x`lato` con la palla di `diametro` (frazione del lato) al centro."""
    n = lato * SOVRA
    img = Image.new('RGBA', (n, n), fondo)
    d = ImageDraw.Draw(img)
    r = n * diametro / 2
    c = n / 2
    box = lambda rr: (c - rr, c - rr, c + rr, c + rr)
    # bordo nero, metà rossa, metà bianca, fascia nera
    d.ellipse(box(r), fill=NERO)
    ri = r * 0.94
    d.pieslice(box(ri), 180, 360, fill=ROSSO)
    d.pieslice(box(ri), 0, 180, fill=BIANCO)
    fascia = r * 0.08
    d.rectangle((c - ri, c - fascia, c + ri, c + fascia), fill=NERO)
    # al centro lo schermo rotondo, come il vetro nero della cornice
    d.ellipse(box(r * 0.42), fill=NERO)
    d.ellipse(box(r * 0.36), fill=CERCHIO)
    d.ellipse(box(r * 0.32), fill=VETRO)
    return img.resize((lato, lato), Image.LANCZOS)


def main():
    os.makedirs(USCITA, exist_ok=True)
    trasparente = (0, 0, 0, 0)
    icone = {
        'icona-192.png': palla(192, 0.92, trasparente),
        'icona-512.png': palla(512, 0.92, trasparente),
        'icona-maskable-512.png': palla(512, 0.66, SFONDO),
        'apple-touch-icon.png': palla(180, 0.78, SFONDO),
    }
    for nome, img in icone.items():
        p = os.path.join(USCITA, nome)
        img.save(p, optimize=True)
        print(f'{os.path.relpath(p)}: {img.size[0]}x{img.size[1]}')


if __name__ == '__main__':
    main()
