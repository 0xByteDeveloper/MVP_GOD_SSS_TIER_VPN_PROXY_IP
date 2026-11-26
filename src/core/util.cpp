// ============================================================================
//  GOD_SSS_TIER_VPN — src/core/util.cpp
// ============================================================================
#include "core/util.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <mutex>
#include <random>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>

#ifdef _WIN32
#  include <direct.h>                    // _mkdir
#  define GT_MKDIR(p) _mkdir(p)
#else
#  define GT_MKDIR(p) mkdir(p, 0755)
#endif

namespace gt {

static std::mutex g_rng_mu;
static std::mt19937_64 g_rng{std::random_device{}()};

static uint64_t rng_next() {
    std::lock_guard<std::mutex> g(g_rng_mu);
    return g_rng();
}
double rand01() {
    std::lock_guard<std::mutex> g(g_rng_mu);
    return std::uniform_real_distribution<double>(0.0, 1.0)(g_rng);
}
std::string rand_hex(int chars) {
    static const char* HEX = "0123456789abcdef";
    std::string out;
    out.reserve((size_t)chars);
    for (int i = 0; i < chars; i++) out += HEX[rng_next() & 0xF];
    return out;
}
std::string uuid_hex() { return rand_hex(32); }
std::string ping_id_str() {
    // python: f"{random.random():.17f}" -> 0.48392012345678901
    char b[32];
    snprintf(b, sizeof b, "0.%017llu", rng_next() % 100000000000000000ULL);
    return b;
}

std::string b64_encode(const std::string& raw) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((raw.size() + 2) / 3) * 4);
    uint32_t val = 0; int bits = 0;
    for (unsigned char c : raw) {
        val = (val << 8) | c; bits += 8;
        while (bits >= 6) { bits -= 6; out += T[(val >> bits) & 0x3F]; }
    }
    if (bits > 0) out += T[(val << (6 - bits)) & 0x3F];
    while (out.size() % 4) out += '=';
    return out;
}

std::string url_encode(const std::string& s) {
    static const char* HEX = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3 / 2 + 8);
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~')
            out += (char)c;
        else { out += '%'; out += HEX[c >> 4]; out += HEX[c & 0xF]; }
    }
    return out;
}

std::string upper(const std::string& s) {
    std::string o = s;
    for (auto& c : o) if (c >= 'a' && c <= 'z') c -= 32;
    return o;
}

std::string b64_decode(const std::string& s) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::string out;
    uint32_t buf = 0; int bits = 0;
    for (char c : s) {
        int v = val(c);
        if (v < 0) continue;                  // '=' ve boşluklar atlanır
        buf = (buf << 6) | (uint32_t)v; bits += 6;
        if (bits >= 8) { bits -= 8; out += (char)((buf >> bits) & 0xFF); }
    }
    return out;
}

uint64_t now_ms() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
std::string now_iso() {
    time_t t = time(nullptr);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    char b[32];
    strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ", &tmv);
    return b;
}
std::string fmt_duration(uint64_t ms) {
    uint64_t s = ms / 1000;
    if (s < 60)      return std::to_string(s) + "s";
    if (s < 3600)    return std::to_string(s / 60) + "m " + std::to_string(s % 60) + "s";
    if (s < 86400)   return std::to_string(s / 3600) + "h " + std::to_string((s % 3600) / 60) + "m";
    return std::to_string(s / 86400) + "g " + std::to_string((s % 86400) / 3600) + "h";
}

bool read_file(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}
bool write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(content.data(), (std::streamsize)content.size());
    return f.good();
}
bool file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}
bool mkdir_p(const std::string& path) {
    if (path.empty()) return false;
    std::string cur;
    for (size_t i = 0; i < path.size(); i++) {
        cur += path[i];
        if (path[i] == '/' && i > 0) GT_MKDIR(cur.c_str());
    }
    GT_MKDIR(path.c_str());
    return file_exists(path);
}

std::string lower(const std::string& s) {
    std::string o = s;
    for (auto& c : o) if (c >= 'A' && c <= 'Z') c += 32;
    return o;
}
std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) b--;
    return s.substr(a, b - a);
}
std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}
std::map<std::string, std::string> parse_query(const std::string& query) {
    std::map<std::string, std::string> out;
    for (auto& kv : split(query, '&')) {
        size_t eq = kv.find('=');
        if (eq != std::string::npos) out[kv.substr(0, eq)] = kv.substr(eq + 1);
        else if (!kv.empty()) out[kv] = "";
    }
    return out;
}

std::string basic_auth(const std::string& login, const std::string& password) {
    return "Basic " + b64_encode(login + ":" + password);
}

} // namespace gt
