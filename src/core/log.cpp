// ============================================================================
//  GOD_SSS_TIER_VPN — src/core/log.cpp
// ============================================================================
#include "core/log.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#ifndef _WIN32
#  include <unistd.h>
#else
#  include <io.h>              // _isatty
#endif

namespace gt {

static std::mutex g_log_mu;
static std::function<void(const std::string&)> g_sink = nullptr;
static bool g_color = false;

void LogInit() {
    // stderr bir TTY ise renklendir (2>&1 redirect edilen sunucularda düz metin)
#ifdef _WIN32
    g_color = _isatty(2) != 0;   // Windows: VT renkler consolda desteksiz olabilir
#else
    g_color = isatty(2) != 0;
#endif
}

void LogSetSink(std::function<void(const std::string&)> sink) {
    std::lock_guard<std::mutex> g(g_log_mu);
    g_sink = std::move(sink);
}

static const char* color_code(LogColor c, bool& on) {
    on = true;
    switch (c) {
        case LogColor::Red:     return "\x1b[31m";
        case LogColor::Green:   return "\x1b[32m";
        case LogColor::Yellow:  return "\x1b[33m";
        case LogColor::Cyan:    return "\x1b[36m";
        case LogColor::Magenta: return "\x1b[35m";
        default:                on = false; return "";
    }
}

static void emit(const char* color, const char* fmt, va_list ap) {
    char buf[2048];
    vsnprintf(buf, sizeof buf, fmt, ap);
    std::string line = color ? color : "";
    line += buf;
    line += g_color ? "\x1b[0m" : "";
    line += "\n";
    fputs(line.c_str(), stderr);      // sunucu stderr'ine (CALISTIR.sh 2>&1 ile)
    fflush(stderr);
    std::function<void(const std::string&)> sink;
    { std::lock_guard<std::mutex> g(g_log_mu); sink = g_sink; }
    if (sink) {
        // konsol için ANSI soyulmuş düz satır
        std::string plain = color ? "" : "";
        plain += buf;
        plain += "\n";
        sink(plain);
    }
}

void Log(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    emit(nullptr, fmt, ap);
    va_end(ap);
}
void LogLine(LogColor c, const char* fmt, ...) {
    const char* col = nullptr; bool on = false;
    col = color_code(c, on);
    va_list ap; va_start(ap, fmt);
    emit(col, fmt, ap);
    va_end(ap);
}
void LogOk(const char* fmt, ...)   { va_list ap; va_start(ap, fmt); emit(g_color ? "\x1b[32m" : "", fmt, ap); va_end(ap); }
void LogWarn(const char* fmt, ...) { va_list ap; va_start(ap, fmt); emit(g_color ? "\x1b[33m" : "", fmt, ap); va_end(ap); }
void LogErr(const char* fmt, ...)  { va_list ap; va_start(ap, fmt); emit(g_color ? "\x1b[31m" : "", fmt, ap); va_end(ap); }

} // namespace gt
