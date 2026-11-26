#!/usr/bin/env python3
# MVP_GOD_SSS_TIER — web/ dosyalarını C++ başlığına göm (tek-exe: HTML içinde)
# Kullanım:  python3 scripts/embed_web.py   →  src/web_embedded.h üretir
# web/ değişince yeniden çalıştırın (depoya hazır hali commit edilir).
import os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB = os.path.join(ROOT, "web")
FILES = ["index.html", "app.js", "style.css"]
OUT = os.path.join(ROOT, "src", "web_embedded.h")

lines = [
    "// ============================================================================",
    "//  MVP_GOD_SSS_TIER — src/web_embedded.h   [OTOMATİK ÜRETİLDİ — el ile değiştirme]",
    "//  web/ klasörünün exe içine gömülmüş kopyası (scripts/embed_web.py).",
    "//  Tek .exe: web/ klasörü yanında olmasa da WebGUI çalışır (yedek kaynak).",
    "// ============================================================================",
    "#pragma once",
    "#include <map>",
    "#include <string>",
    "",
    "namespace ui {",
    "namespace embedded {",
    "",
    "inline const std::map<std::string, std::string>& files() {",
    "    static const std::map<std::string, std::string> m = {",
]

DELIM = "MVPGEN"
for f in FILES:
    p = os.path.join(WEB, f)
    if not os.path.exists(p):
        print("eksik:", p); sys.exit(1)
    data = open(p, "r", encoding="utf-8").read()
    if (")" + DELIM + '"') in data:
        print("delimiter çakışması:", f); sys.exit(1)
    lines.append('        {"%s", R"%s(%s)%s"},' % (f, DELIM, data, DELIM))

lines += [
    "    };",
    "    return m;",
    "}",
    "",
    "inline std::string get(const std::string& rel) {",
    "    auto it = files().find(rel);",
    "    return it == files().end() ? std::string() : it->second;",
    "}",
    "",
    "} // namespace embedded",
    "} // namespace ui",
    "",
]

with open(OUT, "w", encoding="utf-8") as w:
    w.write("\n".join(lines) + "\n")
print("OK:", OUT, "(%d dosya gömüldü)" % len(FILES))
