#!/usr/bin/env python3
"""Il libretto del giocatore in PDF (A5, stampabile anche a libretto su A4).

Sorgente in docs/libretto/ (libretto.html con i capitoli, libretto.css, libretto.js che
impagina) più il testo a pixel di docs/stile/pixel.js, le foto di docs/libretto/img/
(tools/foto_doc.py) e i diagrammi di docs/img/comandi/. Si assembla in build/libretto/ e si
stampa con Chrome headless: prima si legge il DOM impaginato per controllare che ogni blocco
stia nella sua pagina e che le pagine siano un multiplo di 4, poi si stampa.

Uso: python3 tools/libretto.py [-o dist/libretto-del-giocatore.pdf] [--html]
  --html   prepara solo build/libretto/ (da aprire nel browser per lavorare sul testo)
Serve Chrome o Chromium (variabile CHROME per indicarne uno).
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys

QUI = os.path.dirname(os.path.abspath(__file__))
RADICE = os.path.dirname(QUI)
SORGENTE = os.path.join(RADICE, 'docs', 'libretto')
BUILD = os.path.join(RADICE, 'build', 'libretto')
USCITA = os.path.join(RADICE, 'dist', 'libretto-del-giocatore.pdf')
ATTESA_MS = '30000'  # tempo virtuale per caricare le immagini e impaginare

sys.path.insert(0, QUI)
import font5x7  # noqa: E402


def versione():
    with open(os.path.join(RADICE, 'ui_common.h'), encoding='utf-8') as f:
        return re.search(r'#define FW_VERSION "([^"]+)"', f.read()).group(1)


def chrome():
    for nome in (os.environ.get('CHROME'), 'google-chrome', 'google-chrome-stable', 'chromium',
                 'chromium-browser'):
        if nome and shutil.which(nome):
            return shutil.which(nome)
    sys.exit('serve Chrome o Chromium (o la variabile CHROME)')


def prepara():
    shutil.rmtree(BUILD, ignore_errors=True)
    shutil.copytree(SORGENTE, BUILD)
    shutil.copy(os.path.join(RADICE, 'docs', 'stile', 'pixel.js'), BUILD)
    shutil.copytree(os.path.join(RADICE, 'docs', 'img', 'comandi'), os.path.join(BUILD, 'comandi'))
    with open(os.path.join(BUILD, 'font5x7.js'), 'w', encoding='utf-8') as f:
        f.write(f'const FONT5X7 = "{font5x7.FONT.hex()}";\n')
    html = os.path.join(BUILD, 'libretto.html')
    with open(html, encoding='utf-8') as f:
        testo = f.read().replace('@VERSIONE@', versione())
    with open(html, 'w', encoding='utf-8') as f:
        f.write(testo)
    return html


def esegui(argomenti):
    base = [chrome(), '--headless=new', '--disable-gpu', '--hide-scrollbars',
            '--run-all-compositor-stages-before-draw', f'--virtual-time-budget={ATTESA_MS}']
    if os.environ.get('CI'):
        base.append('--no-sandbox')  # i runner della CI non hanno il sandbox degli utenti
    r = subprocess.run(base + argomenti, capture_output=True, text=True, timeout=300)
    if r.returncode != 0:
        sys.exit(f'Chrome è uscito con {r.returncode}:\n{r.stderr[-2000:]}')
    return r.stdout


def controlla(html):
    dom = esegui(['--dump-dom', 'file://' + html])
    m = re.search(r'<body[^>]*data-pagine="(\d+)"[^>]*data-avvisi="([^"]*)"', dom)
    if not m:
        sys.exit('il libretto non si è impaginato (libretto.js non è arrivato in fondo)')
    pagine = int(m.group(1))
    avvisi = json.loads(m.group(2).replace('&quot;', '"').replace('&amp;', '&'))
    for a in avvisi:
        print('AVVISO:', a)
    if avvisi:
        sys.exit('ci sono blocchi più alti di una pagina: accorciali o dividili')
    if pagine % 4:
        sys.exit(f'{pagine} pagine: per la stampa a libretto servono multipli di 4')
    return pagine


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('-o', '--uscita', default=USCITA)
    ap.add_argument('--html', action='store_true', help='solo build/libretto/, senza PDF')
    a = ap.parse_args()
    html = prepara()
    if a.html:
        print(f'pronto: {html}')
        return
    pagine = controlla(html)
    os.makedirs(os.path.dirname(os.path.abspath(a.uscita)), exist_ok=True)
    esegui(['--no-pdf-header-footer', f'--print-to-pdf={os.path.abspath(a.uscita)}',
            'file://' + html])
    print(f'{os.path.relpath(a.uscita)}: {pagine} pagine A5, '
          f'{os.path.getsize(a.uscita) // 1024} KB')


if __name__ == '__main__':
    main()
