#!/bin/bash
# Compila il firmware e prepara in dist/ i file di una release (li usa anche la CI):
#   pokegotchi-<ver>-ota.bin   l'app da sola: la scarica l'aggiornamento via WiFi
#   pokegotchi.json            il manifest che la scheda legge (versione, url, dimensione, sha256)
#   boot.bin                   bootloader + tabella delle partizioni, a 0x0 (installer web)
#   pokegotchi.bin             boot_app0 (otadata) + l'app, a 0xE000 (installer web)
# Due pezzi per l'installer, mai un'immagine unica da 0x0: merge-bin riempie i buchi di 0xFF e
# la NVS (0x9000-0xDFFF, la partita) verrebbe cancellata a ogni flash.
#
# Uso: bash tools/build_firmware.sh
#   PG_REPO=owner/nome  il repository delle release (predefinito: quello di ota.h); la CI
#                       passa github.repository, così un fork si aggiorna dalle sue release
# La cartella dello sketch deve chiamarsi PokeGotchi (regola di arduino-cli).
set -euo pipefail
cd "$(dirname "$0")/.."
if [ "$(basename "$PWD")" != "PokeGotchi" ]; then
  echo "la cartella del repository deve chiamarsi PokeGotchi, come lo sketch (ora: $(basename "$PWD"))"
  exit 1
fi

FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
VER=$(sed -n 's/^#define FW_VERSION "\([^"]*\)".*/\1/p' ui_common.h)
REPO=${PG_REPO:-$(sed -n 's/^#define PG_REPO "\([^"]*\)".*/\1/p' ota.h)}
[ -n "$VER" ] && [ -n "$REPO" ] || { echo "FW_VERSION o PG_REPO non trovati"; exit 1; }

echo "Compilo PokeGotchi $VER (release da $REPO)..."
arduino-cli compile --fqbn "$FQBN" --export-binaries \
  --build-property "compiler.cpp.extra_flags=-DPG_REPO=\"$REPO\"" .

B=build/esp32.esp32.esp32s3
# esptool dal PATH, altrimenti quello del core ESP32 di arduino-cli
ESPTOOL=$(command -v esptool.py || command -v esptool || \
  ls -d "$HOME"/.arduino15/packages/esp32/tools/esptool_py/*/esptool 2>/dev/null | tail -1)
rm -rf dist
mkdir -p dist
cp "$B/PokeGotchi.ino.bin" "dist/pokegotchi-$VER-ota.bin"
"$ESPTOOL" --chip esp32s3 merge-bin -o dist/boot.bin \
  0x0     "$B/PokeGotchi.ino.bootloader.bin" \
  0x8000  "$B/PokeGotchi.ino.partitions.bin"
"$ESPTOOL" --chip esp32s3 merge-bin -t 0xe000 -o dist/pokegotchi.bin \
  0xe000  "$B/boot_app0.bin" \
  0x10000 "$B/PokeGotchi.ino.bin"

python3 - "$VER" "$REPO" <<'PY'
import hashlib, json, os, sys
ver, repo = sys.argv[1], sys.argv[2]
fine = os.path.getsize('dist/boot.bin')
assert fine <= 0x9000, f'boot.bin arriva a {fine:#x}: coprirebbe la NVS a 0x9000'
ota = f'dist/pokegotchi-{ver}-ota.bin'
dati = open(ota, 'rb').read()
assert len(dati) <= 3 * 1024 * 1024, 'il firmware non sta nella partizione OTA da 3 MB'
manifest = {
    'version': ver,
    'url': f'https://github.com/{repo}/releases/download/v{ver}/{os.path.basename(ota)}',
    'size': len(dati),
    'sha256': hashlib.sha256(dati).hexdigest(),
}
with open('dist/pokegotchi.json', 'w') as f:
    json.dump(manifest, f, indent=2)
    f.write('\n')
print(json.dumps(manifest, indent=2))
PY
echo "OK -> dist/ ($(du -h "dist/pokegotchi-$VER-ota.bin" | cut -f1) di app)"
