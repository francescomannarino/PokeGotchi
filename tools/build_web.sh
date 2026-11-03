#!/bin/bash
# Prepara web/ per provarlo in locale (python3 tools/serve_web.py): il firmware dell'installer
# (web/firmware/, da tools/build_firmware.sh), sprites.pak e, se c'è emsdk, la PWA in web/gioca/
# (tools/build_pwa.sh). Il sito pubblicato lo assembla tools/build_sito.sh.
# Uso: bash tools/build_web.sh
set -e
cd "$(dirname "$0")/.."

bash tools/build_firmware.sh
mkdir -p web/firmware
cp dist/boot.bin dist/pokegotchi.bin web/firmware/

echo "Impacchetto gli sprite..."
python3 tools/pack_bundle.py

echo "PWA..."
bash tools/build_pwa.sh || echo "PWA non compilata (serve emsdk): web/gioca/ non aggiornata"
