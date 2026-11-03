#!/usr/bin/env python3
"""Serve web/ in locale come `python3 -m http.server`, ma con le richieste HTTP Range
(una sola porzione, risposta 206), che la PWA usa per prendere uno sprite alla volta da
sprites.pak. Come GitHub Pages dà anche ETag e Last-Modified.

Uso: python3 tools/serve_web.py [porta] [--sito]   (porta predefinita 8000)
     poi http://localhost:8000/ (installer) e http://localhost:8000/gioca/ (PWA)
     --sito serve invece _site/ (tools/build_sito.sh), come su GitHub Pages: la presentazione
     in /, l'installer in /installa/ e la PWA in /gioca/
     --locale ascolta solo su 127.0.0.1 (senza, anche dagli altri dispositivi della rete)
"""
import email.utils
import http.server
import os
import re
import sys

QUI = os.path.dirname(os.path.abspath(__file__))
RADICE = os.path.join(QUI, '..', '_site' if '--sito' in sys.argv else 'web')


class ConRange(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=RADICE, **k)

    def end_headers(self):
        self.send_header('Accept-Ranges', 'bytes')
        super().end_headers()

    def send_head(self):
        m = re.fullmatch(r'bytes=(\d*)-(\d*)', self.headers.get('Range', '').strip())
        percorso = self.translate_path(self.path)
        if not m or not os.path.isfile(percorso):
            return super().send_head()
        st = os.stat(percorso)
        n = st.st_size
        if m.group(1):
            da = int(m.group(1))
            a = min(int(m.group(2)), n - 1) if m.group(2) else n - 1
        else:  # bytes=-N: gli ultimi N
            da, a = max(0, n - int(m.group(2) or 0)), n - 1
        if da >= n or a < da:
            self.send_response(416)
            self.send_header('Content-Range', f'bytes */{n}')
            self.send_header('Content-Length', '0')
            self.end_headers()
            return None
        f = open(percorso, 'rb')
        f.seek(da)
        self.send_response(206)
        self.send_header('Content-Type', self.guess_type(percorso))
        self.send_header('Content-Range', f'bytes {da}-{a}/{n}')
        self.send_header('Content-Length', str(a - da + 1))
        self.send_header('ETag', f'"{st.st_mtime_ns:x}-{n:x}"')
        self.send_header('Last-Modified', email.utils.formatdate(st.st_mtime, usegmt=True))
        self.end_headers()
        self._resto = a - da + 1
        return f

    def copyfile(self, sorgente, dest):
        resto = getattr(self, '_resto', None)
        if resto is None:
            return super().copyfile(sorgente, dest)
        self._resto = None
        while resto > 0:
            b = sorgente.read(min(65536, resto))
            if not b:
                break
            dest.write(b)
            resto -= len(b)


ConRange.extensions_map = {**ConRange.extensions_map, '.wasm': 'application/wasm',
                           '.webmanifest': 'application/manifest+json'}

if __name__ == '__main__':
    numeri = [a for a in sys.argv[1:] if a.isdigit()]
    porta = int(numeri[0]) if numeri else 8000
    print(f'{os.path.basename(os.path.normpath(RADICE))}/ su http://localhost:{porta}/ (PWA: /gioca/)')
    http.server.ThreadingHTTPServer(('127.0.0.1' if '--locale' in sys.argv else '', porta), ConRange).serve_forever()
