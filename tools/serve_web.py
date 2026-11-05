#!/usr/bin/env python3
"""Serve web/ in locale come `python3 -m http.server`, con i tipi MIME di .wasm e
.webmanifest. Gli sprite non passano da qui: l'installer e la PWA li scaricano da PMD
SpriteCollab e li convertono nel browser (web/pmd.js).

Uso: python3 tools/serve_web.py [porta] [--sito] [--locale]   (porta predefinita 8000)
     poi http://localhost:8000/ (installer) e http://localhost:8000/gioca/ (PWA)
     --sito serve invece _site/ (tools/build_sito.sh), come su GitHub Pages: la presentazione
     in /, l'installer in /installa/ e la PWA in /gioca/
     --locale ascolta solo su 127.0.0.1 (senza, anche dagli altri dispositivi della rete)
"""
import http.server
import os
import sys

QUI = os.path.dirname(os.path.abspath(__file__))
RADICE = os.path.join(QUI, '..', '_site' if '--sito' in sys.argv else 'web')


class Pagine(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=RADICE, **k)


Pagine.extensions_map = {**Pagine.extensions_map, '.wasm': 'application/wasm',
                         '.webmanifest': 'application/manifest+json',
                         '.js': 'text/javascript', '.mjs': 'text/javascript'}

if __name__ == '__main__':
    numeri = [a for a in sys.argv[1:] if a.isdigit()]
    porta = int(numeri[0]) if numeri else 8000
    print(f'{os.path.basename(os.path.normpath(RADICE))}/ su http://localhost:{porta}/ (PWA: /gioca/)')
    http.server.ThreadingHTTPServer(('127.0.0.1' if '--locale' in sys.argv else '', porta), Pagine).serve_forever()
