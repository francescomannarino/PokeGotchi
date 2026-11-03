#!/bin/bash
# Compila la PWA in web/gioca/: il firmware intero in WebAssembly con Emscripten
# (gli stessi sorgenti dell'emulatore di tools/emu, più pwa/hal_web.cpp) e la pagina di pwa/.
# Uso: bash tools/build_pwa.sh
# Prova: python3 tools/serve_web.py (serve web/ con le richieste Range per sprites.pak),
# poi http://localhost:8000/gioca/ (con ?debug la console seriale)
# Serve emsdk: se emcc non è nel PATH si carica $EMSDK/emsdk_env.sh (predefinito ~/emsdk).
set -e
cd "$(dirname "$0")/.."

if ! command -v emcc >/dev/null 2>&1; then
  ENV="${EMSDK:-$HOME/emsdk}/emsdk_env.sh"
  if [ ! -f "$ENV" ]; then
    echo "emcc non trovato: installa emsdk (https://emscripten.org/docs/getting_started/downloads.html)"
    echo "in ~/emsdk, oppure imposta EMSDK"
    exit 1
  fi
  source "$ENV" >/dev/null 2>&1
fi

make -C tools/emu web BUILD=build/web CXX=em++ CC=emcc AR=emar \
  CXXFLAGS="-std=c++17 -O2 -Wall -Wno-unused-parameter -Wno-format-security -Wno-format-truncation" \
  CFLAGS="-O2"

mkdir -p web/gioca/icone
cp tools/emu/build/web/pokegotchi.js tools/emu/build/web/pokegotchi.wasm web/gioca/
cp pwa/index.html pwa/app.js pwa/audio.js pwa/salvataggi.js pwa/sprite.js pwa/stile.css pwa/app.webmanifest web/gioca/
cp pwa/icone/*.png web/gioca/icone/

# il service worker: la cache della shell prende il nome da FW_VERSION + l'impronta dei file
# copiati, così ogni build diversa si installa come aggiornamento
FW=$(sed -n 's/^#define FW_VERSION "\([^"]*\)".*/\1/p' ui_common.h)
IMPRONTA=$(cd web/gioca && find . -type f ! -name sw.js | sort | xargs sha256sum | sha256sum | cut -c1-10)
sed "s/@VERSIONE@/$FW-$IMPRONTA/" pwa/sw.js > web/gioca/sw.js
echo "PWA $FW-$IMPRONTA in web/gioca/ ($(du -sh web/gioca | cut -f1))"
