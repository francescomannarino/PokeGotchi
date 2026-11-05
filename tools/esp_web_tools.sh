#!/bin/bash
# ESP Web Tools accanto all'installer, così chi lo apre non contatta una CDN di terzi.
# Scarica il pacchetto npm alla versione fissata, ne controlla lo SHA-512 (quello del
# registro npm) e copia dist/web/ e la licenza in CARTELLA/esp-web-tools/.
# Uso: bash tools/esp_web_tools.sh CARTELLA      (web/ in locale, _site/installa/ per il sito)
# Per cambiare versione: VERSIONE e SHA512 (da `npm view esp-web-tools@X.Y.Z dist.integrity`),
# e l'hash SRI di install-button.js in web/index.html.
set -euo pipefail

VERSIONE=10.4.0
SHA512=3pwkeFFm5Fj7UQo8SJNYK5RXrtNCpq6X9QoI6bMT4GBZWgrJqjn0YvM9ihG74BtMoSFYXfmDtkehuxe50PTMPQ==
DEST="${1:?uso: esp_web_tools.sh CARTELLA}/esp-web-tools"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
curl -sfL -o "$TMP/ewt.tgz" "https://registry.npmjs.org/esp-web-tools/-/esp-web-tools-$VERSIONE.tgz"
TROVATO=$(openssl dgst -sha512 -binary "$TMP/ewt.tgz" | openssl base64 -A)
if [ "$TROVATO" != "$SHA512" ]; then
  echo "esp-web-tools $VERSIONE: SHA-512 diverso da quello atteso" >&2
  exit 1
fi
tar xzf "$TMP/ewt.tgz" -C "$TMP" package/dist/web package/LICENSE
rm -rf "$DEST"
mkdir -p "$DEST"
cp "$TMP"/package/dist/web/* "$TMP/package/LICENSE" "$DEST/"
echo "ESP Web Tools $VERSIONE -> $DEST"
