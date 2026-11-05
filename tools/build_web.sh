#!/bin/bash
# Prepara web/ per provarlo in locale (python3 tools/serve_web.py): il firmware dell'installer
# (web/firmware/, da tools/build_firmware.sh) e, se c'è emsdk, la PWA in web/gioca/
# (tools/build_pwa.sh). Il sito pubblicato lo assembla tools/build_sito.sh. Gli sprite non ci
# sono: l'installer e la PWA li convertono nel browser (web/pmd.js).
# Uso: bash tools/build_web.sh
set -e
cd "$(dirname "$0")/.."

bash tools/build_firmware.sh
bash tools/esp_web_tools.sh web
mkdir -p web/firmware
cp dist/boot.bin dist/pokegotchi.bin web/firmware/

echo "PWA..."
bash tools/build_pwa.sh || echo "PWA non compilata (serve emsdk): web/gioca/ non aggiornata"
