#!/usr/bin/env bash
# Esegue l'intera batteria di test (logica del firmware + strumenti).
#   ./test/run_tests.sh          normale
#   ./test/run_tests.sh --asan   in più con AddressSanitizer + UBSan
set -uo pipefail
cd "$(dirname "$0")"

fail=0

echo "== logica del firmware (pet.cpp / i18n.cpp / dex.h) =="
make --no-print-directory run || fail=1

if [[ "${1:-}" == "--asan" ]]; then
  echo
  echo "== la misma bateria con AddressSanitizer + UBSan =="
  make --no-print-directory asan || fail=1
fi

echo
echo "== herramientas y ficheros generados (tools/) =="
python3 test_tools.py || fail=1

echo
echo "== emulatore: smoke test di 48 h accelerate (tools/emu) =="
if command -v sdl2-config >/dev/null || pkg-config --exists sdl2 2>/dev/null; then
  make --no-print-directory -C ../tools/emu smoke || fail=1
else
  echo "saltato: manca SDL2 (sudo apt install libsdl2-dev / brew install sdl2)"
fi

echo
if [[ $fail -eq 0 ]]; then
  echo "TODO OK"
else
  echo "HAY FALLOS"
fi
exit $fail
