#!/usr/bin/env python3
"""Screenshot della documentazione: le foto dello smoke test dell'emulatore (`make smoke`,
tools/emu/smoke.txt). In docs/img/schermate/ (README, COMANDI.md, sito) ridotte a metà e a 64
colori; in docs/libretto/img/ (il libretto da stampare) a piena risoluzione, a 128 colori.

Uso: python3 tools/foto_doc.py [--senza-smoke]   (servono SDL2 per l'emulatore e Pillow)

Con --senza-smoke usa le foto già in tools/emu/build/smoke/foto/.
"""
import os
import subprocess
import sys

from PIL import Image

QUI = os.path.dirname(os.path.abspath(__file__))
EMU = os.path.join(QUI, 'emu')
FOTO = os.path.join(EMU, 'build', 'smoke', 'foto')
USCITA = os.path.join(QUI, '..', 'docs', 'img', 'schermate')
LIBRETTO = os.path.join(QUI, '..', 'docs', 'libretto', 'img')

LATO = 233     # metà dello schermo 466x466
COLORI = 64

# foto dello smoke test -> nome nella documentazione
SCELTE = {
    '01-iniziale.png': 'iniziale.png',
    '03-principale.png': 'principale.png',
    '05-scheda-2.png': 'scheda-lotta.png',
    '09-impostazioni.png': 'impostazioni.png',
    '10b-audio.png': 'audio.png',
    '10-salvataggi.png': 'salvataggi.png',
    '11-pokedex.png': 'pokedex.png',
    '12-pokedex-dettaglio.png': 'pokedex-dettaglio.png',
    '23-collezione.png': 'collezione.png',
    '13-minigioco.png': 'minigioco.png',
    '13a-giochi.png': 'giochi.png',
    '13d-grotta.png': 'grotta.png',
    '13g-surf.png': 'surf.png',
    '13j-scossone.png': 'scossone.png',
    '15-battaglia.png': 'battaglia.png',
    '16-battaglia-mosse.png': 'battaglia-mosse.png',
    '16b-collegamento.png': 'collegamento.png',
    '29-aggiorna-nuova.png': 'aggiorna.png',
}

# foto per il libretto, a piena risoluzione: tutte le schermate che il libretto racconta
LIBRETTO_FOTO = {
    '01-iniziale.png': 'iniziale.png', '02-uovo.png': 'uovo.png',
    '03-principale.png': 'principale.png', '04-scheda-1.png': 'scheda-profilo.png',
    '05-scheda-2.png': 'scheda-lotta.png', '06-scheda-3.png': 'scheda-medaglie.png',
    '07-scheda-4.png': 'scheda-progressi.png', '08-tastiera.png': 'tastiera-nome.png',
    '09-impostazioni.png': 'impostazioni.png', '10-salvataggi.png': 'salvataggi.png',
    '10b-audio.png': 'audio.png', '11-pokedex.png': 'pokedex.png',
    '12-pokedex-dettaglio.png': 'pokedex-dettaglio.png', '13-minigioco.png': 'palla.png',
    '13a-giochi.png': 'giochi.png', '13c-grotta-zero.png': 'grotta-zero.png',
    '13d-grotta.png': 'grotta.png', '13e-grotta-fine.png': 'grotta-fine.png',
    '13b-grotta-joystick.png': 'joystick.png', '13g-surf.png': 'surf.png',
    '13h-surf-giro.png': 'surf-giro.png', '13j-scossone.png': 'scossone.png',
    '13k-colpetti.png': 'colpetti.png', '14-sacco.png': 'sacco.png',
    '15-battaglia.png': 'battaglia.png', '16-battaglia-mosse.png': 'battaglia-mosse.png',
    '16a-battaglia-squadra.png': 'battaglia-squadra.png',
    '16b-collegamento.png': 'collegamento.png', '18-dopo-48h.png': 'abbandono.png',
    '19-cerimonia.png': 'cerimonia.png', '21-collezione-domanda.png': 'collezione-domanda.png',
    '22-pokedex-collezione.png': 'pokedex-collezione.png', '23-collezione.png': 'collezione.png',
    '24-collezione-dettaglio.png': 'collezione-dettaglio.png',
    '27-aggiorna-reti.png': 'aggiorna-reti.png', '28-aggiorna-password.png': 'aggiorna-password.png',
    '29-aggiorna-nuova.png': 'aggiorna-nuova.png', '30-cibo.png': 'cibo.png',
    '31-aggiorna-scarica.png': 'aggiorna-scarica.png',
}


def riduci(sorgente, destinazione):
    img = Image.open(sorgente).convert('RGB').resize((LATO, LATO), Image.LANCZOS)
    img = img.quantize(COLORI, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    img.save(destinazione, optimize=True)


def piena(sorgente, destinazione):
    img = Image.open(sorgente).convert('RGB')
    img = img.quantize(128, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    img.save(destinazione, optimize=True)


def main():
    if '--senza-smoke' not in sys.argv:
        subprocess.run(['make', '-C', EMU, 'smoke'], check=True)
    for cartella, scelte, fai in ((USCITA, SCELTE, riduci), (LIBRETTO, LIBRETTO_FOTO, piena)):
        os.makedirs(cartella, exist_ok=True)
        for sorgente, nome in scelte.items():
            fai(os.path.join(FOTO, sorgente), os.path.join(cartella, nome))
        totale = sum(os.path.getsize(os.path.join(cartella, n)) for n in scelte.values())
        print(f'{os.path.normpath(cartella)}: {len(scelte)} foto, {totale // 1024} KB')


if __name__ == '__main__':
    main()
