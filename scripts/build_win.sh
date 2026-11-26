#!/bin/bash
# MVP_GOD_SSS_TIER — Windows çapraz derleme (Linux'tan, zig ile; MSVC gerekmez)
# zig, mingw-w64 başlıklarını içinde barındırır → tek komutla Windows exe:
#   çıktı: bin/MVP_GOD_SSS_TIER.exe (HTML WebUI gömülü) + bin/godtier_hook.dll
# zig kurulumu:  https://ziglang.org/download/  (tek tarball)
set -e
cd "$(dirname "$0")/.."

ZIG=${ZIG:-$(command -v zig || ls /home/z/my-project/.tools/zig-*/zig 2>/dev/null | head -1)}
if [ -z "$ZIG" ] || [ ! -x "$ZIG" ]; then
  echo "[HATA] zig bulunamadı — ZIG=/yol/zig ortam değişkeni verin"
  exit 1
fi
echo "[0] WebUI gömülüyor (src/web_embedded.h)..."
python3 scripts/embed_web.py

mkdir -p bin
echo "[1] MVP_GOD_SSS_TIER.exe (x86_64-windows-gnu — WinHTTP+SChannel, sıfır dış bağımlılık)..."
"$ZIG" c++ -target x86_64-windows-gnu -std=c++17 -O2 -Wno-unknown-pragmas \
    src/main.cpp src/core/util.cpp src/core/log.cpp \
    src/net/net.cpp src/net/httpclient.cpp src/net/tls.cpp \
    src/providers/hola.cpp src/providers/more.cpp \
    src/engine/router.cpp src/engine/instance.cpp \
    src/engine/proclist.cpp src/engine/monitor.cpp \
    src/httpserver.cpp src/api.cpp \
    -I src \
    -lws2_32 -lsecur32 -lcrypt32 -liphlpapi -lwinhttp -ladvapi32 \
    -o bin/MVP_GOD_SSS_TIER.exe

echo "[2] godtier_hook.dll (süreç kancası)..."
"$ZIG" c++ -target x86_64-windows-gnu -O2 -shared -Wno-unknown-pragmas \
    hook/hook_windows.cpp -lws2_32 -luser32 -o bin/godtier_hook.dll

echo "OK  → bin/MVP_GOD_SSS_TIER.exe + bin/godtier_hook.dll"
echo "Windows'ta: bin\\MVP_GOD_SSS_TIER.exe serve 7787  →  http://127.0.0.1:7787/"
