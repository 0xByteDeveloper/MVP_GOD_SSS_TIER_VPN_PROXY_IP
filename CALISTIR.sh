#!/bin/bash
# MVP_GOD_SSS_TIER — tek tuş çalıştırıcı (Linux)
# WebGUI'yi açar ve tarayıcıyı başlatır. Ctrl+C ile durur.
cd "$(dirname "$0")"
if [ ! -x bin/mvp_god_sss_tier ]; then
  echo "[derleniyor...]"
  ./build.sh || exit 1
fi
PORT="${1:-7787}"
( xdg-open "http://127.0.0.1:$PORT/" >/dev/null 2>&1 || true ) &
echo "motor başlıyor — WebGUI: http://127.0.0.1:$PORT/"
exec ./bin/mvp_god_sss_tier serve "$PORT"
