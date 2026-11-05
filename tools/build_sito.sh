#!/bin/bash
# Assembla il sito di GitHub Pages in _site/, con le parti che ci sono:
#   /                 la presentazione (site/), titoli a pixel (docs/stile/pixel.js)
#   /installa/        l'installer (web/index.html, web/pmd.js, web/manifest.json, web/firmware/)
#   /gioca/           la PWA (web/gioca/, da tools/build_pwa.sh)
# Niente sprite: l'installer e la PWA li scaricano da PMD SpriteCollab e li convertono nel
# browser (pmd.js).
#   /libretto-del-giocatore.pdf   (dist/, da tools/libretto.py)
#   /THIRD_PARTY_NOTICES.txt, /CREDITI_SPRITE.txt   licenze di terzi e autori degli sprite
# Quello che manca si segnala e si salta: in locale basta questo script per vedere la pagina.
# Uso: bash tools/build_sito.sh, poi python3 tools/serve_web.py _site
set -euo pipefail
cd "$(dirname "$0")/.."

VER=$(sed -n 's/^#define FW_VERSION "\([^"]*\)".*/\1/p' ui_common.h)
S=_site
rm -rf "$S"
mkdir -p "$S/js" "$S/img" "$S/installa"

sed "s/@VERSIONE@/$VER/g" site/index.html > "$S/index.html"
cp site/stile.css "$S/"
cp docs/stile/pixel.js "$S/js/"
python3 tools/font5x7.py js > "$S/js/font5x7.js"
cp docs/libretto/img/*.png "$S/img/"
cp pwa/icone/icona-192.png "$S/icona.png"
# in .txt, che il browser mostra come testo (il .md lo scaricherebbe)
cp THIRD_PARTY_NOTICES.md "$S/THIRD_PARTY_NOTICES.txt"
cp docs/CREDITI_SPRITE.md "$S/CREDITI_SPRITE.txt"
touch "$S/.nojekyll"  # Pages non deve passare i file da Jekyll

cp web/index.html web/pmd.js web/manifest.json "$S/installa/"
bash tools/esp_web_tools.sh "$S/installa"
if ls web/firmware/*.bin >/dev/null 2>&1; then
  cp -r web/firmware "$S/installa/"
else
  echo "AVVISO: niente web/firmware/: l'installer non avrà il firmware"
fi
if [ -d web/gioca ]; then
  cp -r web/gioca "$S/gioca"
else
  echo "AVVISO: niente web/gioca/ (bash tools/build_pwa.sh): la PWA manca"
fi
if [ -f dist/libretto-del-giocatore.pdf ]; then
  cp dist/libretto-del-giocatore.pdf "$S/"
else
  echo "AVVISO: niente dist/libretto-del-giocatore.pdf (python3 tools/libretto.py)"
fi

echo "OK -> $S/ ($(du -sh "$S" | cut -f1))"
