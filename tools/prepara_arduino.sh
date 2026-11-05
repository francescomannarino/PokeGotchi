#!/bin/bash
# Installa con arduino-cli il core ESP32 e le librerie del firmware, alle versioni con cui è
# provato (le stesse della CI). Le librerie servono anche all'emulatore e alla PWA, che ne
# compilano i sorgenti (Arduino_GFX e U8g2) sul PC.
# Uso: bash tools/prepara_arduino.sh          tutto
#      bash tools/prepara_arduino.sh librerie  solo le librerie (emulatore, PWA, test)
set -euo pipefail

CORE="esp32:esp32@3.3.12"
LIBRERIE=(
  "GFX Library for Arduino@1.6.9"
  "SensorLib@0.5.1"
  "XPowersLib@0.3.3"
  "U8g2@2.36.19"
)
URL_ESP32="https://espressif.github.io/arduino-esp32/package_esp32_index.json"

command -v arduino-cli >/dev/null || { echo "serve arduino-cli: https://arduino.github.io/arduino-cli/"; exit 1; }
arduino-cli lib update-index
arduino-cli lib install "${LIBRERIE[@]}"
if [ "${1:-}" != "librerie" ]; then
  arduino-cli core update-index --additional-urls "$URL_ESP32"
  arduino-cli core install "$CORE" --additional-urls "$URL_ESP32"
fi
