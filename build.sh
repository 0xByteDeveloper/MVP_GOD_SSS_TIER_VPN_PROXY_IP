#!/bin/bash
# MVP_GOD_SSS_TIER — Linux derleme (test edilen yol)
# Gereksinim: g++ (>=9, C++17), OpenSSL dev başlıkları, python3 (web gömme)
#   Debian/Ubuntu: sudo apt install g++ libssl-dev python3
set -e
cd "$(dirname "$0")"
mkdir -p bin hook

echo "[gömme] web/ → src/web_embedded.h (HTML exe içinde)..."
python3 scripts/embed_web.py

echo "[derleme] mvp_god_sss_tier (motor)..."
g++ -std=c++17 -O2 -Wall -Wno-unused-result \
    src/main.cpp src/core/util.cpp src/core/log.cpp \
    src/net/net.cpp src/net/httpclient.cpp src/net/tls.cpp \
    src/providers/hola.cpp src/providers/more.cpp \
    src/engine/router.cpp src/engine/instance.cpp \
    src/engine/proclist.cpp src/engine/monitor.cpp \
    src/httpserver.cpp src/api.cpp \
    -I src -pthread -lssl -lcrypto -o bin/mvp_god_sss_tier
echo "[derleme] libgodtier_hook.so (LD_PRELOAD kancası)..."
gcc -shared -fPIC -O2 -o hook/libgodtier_hook.so hook/hook_linux.c -ldl -pthread
echo "OK  → bin/mvp_god_sss_tier  +  hook/libgodtier_hook.so"
echo "Çalıştır:"
echo "  cd $(pwd) && ./bin/mvp_god_sss_tier serve 7787   # WebGUI: http://127.0.0.1:7787"
echo "  ./bin/mvp_god_sss_tier selftest TR              # başsız uçtan uca Hola testi"
