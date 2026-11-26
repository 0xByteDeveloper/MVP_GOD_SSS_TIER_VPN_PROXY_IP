// ============================================================================
//  GOD_SSS_TIER_VPN — src/core/json.h
//  Minik JSON parser + serializer (tek başlık, sıra korunumlu obje)
//  Cevaplar /api/* uçlarında hem okuma (Hola API) hem yazmada kullanılır.
// ============================================================================
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <utility>
#include <cstdio>

namespace gt {

class J {
public:
    enum Type { NUL, BOOL, NUM, STR, ARR, OBJ };
    Type t = NUL;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<J> arr;
    std::vector<std::pair<std::string, J>> obj;   // ekleme sırası korunur

    J() {}
    J(bool v)                : t(BOOL), b(v) {}
    J(int v)                 : t(NUM), num(v) {}
    J(long v)                : t(NUM), num(v) {}
    J(long long v)           : t(NUM), num((double)v) {}
    J(unsigned long long v)  : t(NUM), num((double)v) {}
    J(double v)              : t(NUM), num(v) {}
    J(const char* v)         : t(STR), str(v) {}
    J(const std::string& v)  : t(STR), str(v) {}

    static J makeArr() { J j; j.t = ARR; return j; }
    static J makeObj() { J j; j.t = OBJ;  return j; }

    // --- obje erişimi (yoksa ekler) ---
    J& operator[](const std::string& key) {
        if (t != OBJ) { t = OBJ; }
        for (auto& kv : obj) if (kv.first == key) return kv.second;
        obj.emplace_back(key, J());
        return obj.back().second;
    }
    const J* find(const std::string& key) const {
        if (t != OBJ) return nullptr;
        for (auto& kv : obj) if (kv.first == key) return &kv.second;
        return nullptr;
    }
    bool has(const std::string& key) const { return find(key) != nullptr; }

    // --- dizi erişimi ---
    J& push(J v) { if (t != ARR) t = ARR; arr.push_back(std::move(v)); return arr.back(); }
    size_t size() const { return t == ARR ? arr.size() : (t == OBJ ? obj.size() : 0); }

    // --- tip güvenli okuma ---
    std::string s(const std::string& k, const std::string& def = "") const {
        const J* v = find(k); return (v && v->t == STR) ? v->str : def;
    }
    long long i(const std::string& k, long long def = 0) const {
        const J* v = find(k); return (v && v->t == NUM) ? (long long)v->num : def;
    }
    double f(const std::string& k, double def = 0) const {
        const J* v = find(k); return (v && v->t == NUM) ? v->num : def;
    }
    bool bl(const std::string& k, bool def = false) const {
        const J* v = find(k);
        if (!v) return def;
        if (v->t == BOOL) return v->b;
        if (v->t == NUM) return v->num != 0;
        return def;
    }

    // --- serileştirme ---
    void escapeInto(std::string& out) const {
        out += '"';
        for (char c : str) {
            unsigned char u = (unsigned char)c;
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b";  break;
                case '\f': out += "\\f";  break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (u < 0x20) {
                        char b[8]; snprintf(b, sizeof b, "\\u%04x", u); out += b;
                    } else out += c;
            }
        }
        out += '"';
    }
    void dump(std::string& out) const {
        switch (t) {
            case NUL:  out += "null"; break;
            case BOOL: out += b ? "true" : "false"; break;
            case NUM: {
                if (std::floor(num) == num && std::fabs(num) < 9.0e15) {
                    char b[32]; snprintf(b, sizeof b, "%lld", (long long)num); out += b;
                } else {
                    char b[40]; snprintf(b, sizeof b, "%.17g", num); out += b;
                }
                break;
            }
            case STR: escapeInto(out); break;
            case ARR:
                out += '[';
                for (size_t k = 0; k < arr.size(); k++) { if (k) out += ','; arr[k].dump(out); }
                out += ']';
                break;
            case OBJ:
                out += '{';
                for (size_t k = 0; k < obj.size(); k++) {
                    if (k) out += ',';
                    J keyStr(obj[k].first);
                    keyStr.escapeInto(out);
                    out += ':';
                    obj[k].second.dump(out);
                }
                out += '}';
                break;
        }
    }
    std::string dump() const { std::string o; o.reserve(4096); dump(o); return o; }

    // --- çözümleme ---
    static bool parse(const std::string& text, J& out) {
        size_t pos = 0;
        if (!skipWs(text, pos)) return false;
        if (!parseValue(text, pos, out)) return false;
        skipWs(text, pos);
        return pos == text.size();
    }

private:
    static bool skipWs(const std::string& s, size_t& p) {
        while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\r' || s[p] == '\n')) p++;
        return p < s.size();
    }
    static bool parseValue(const std::string& s, size_t& p, J& out) {
        if (!skipWs(s, p)) return false;
        char c = s[p];
        if (c == '{') return parseObj(s, p, out);
        if (c == '[') return parseArr(s, p, out);
        if (c == '"') { out.t = STR; return parseStr(s, p, out.str); }
        if (s.compare(p, 4, "true") == 0)  { p += 4; out = J(true);  return true; }
        if (s.compare(p, 5, "false") == 0) { p += 5; out = J(false); return true; }
        if (s.compare(p, 4, "null") == 0)  { p += 4; out = J();     return true; }
        // sayı
        size_t q = p;
        if (s[p] == '-') p++;
        bool digits = false;
        while (p < s.size() && ((s[p] >= '0' && s[p] <= '9'))) { p++; digits = true; }
        if (p < s.size() && s[p] == '.') { p++; while (p < s.size() && (s[p] >= '0' && s[p] <= '9')) { p++; digits = true; } }
        if (!digits) return false;
        if (p < s.size() && (s[p] == 'e' || s[p] == 'E')) { p++;
            if (p < s.size() && (s[p] == '+' || s[p] == '-')) p++;
            while (p < s.size() && s[p] >= '0' && s[p] <= '9') p++;
        }
        out = J(std::strtod(s.substr(q, p - q).c_str(), nullptr));
        return true;
    }
    static bool parseStr(const std::string& s, size_t& p, std::string& out) {
        p++; // açan tırnak
        while (p < s.size() && s[p] != '"') {
            char c = s[p];
            if ((unsigned char)c == 0 && p + 1 < s.size()) { p++; continue; }
            if (c == '\\') {
                if (p + 1 >= s.size()) return false;
                char e = s[p + 1]; p += 2;
                switch (e) {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    case 'u': {
                        if (p + 4 > s.size()) return false;
                        unsigned cp = (unsigned)strtoul(s.substr(p, 4).c_str(), nullptr, 16);
                        p += 4;
                        // UTF-8'e çevir (BMP; surrogate çifti basitçe atlanır)
                        if (cp < 0x80) out += (char)cp;
                        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
                        else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
                        break;
                    }
                    default: return false;
                }
            } else { out += c; p++; }
        }
        if (p >= s.size()) return false;
        p++; // kapatan tırnak
        return true;
    }
    static bool parseArr(const std::string& s, size_t& p, J& out) {
        p++;
        out = J::makeArr();
        if (!skipWs(s, p)) return false;
        if (s[p] == ']') { p++; return true; }
        while (true) {
            J v;
            if (!parseValue(s, p, v)) return false;
            out.arr.push_back(std::move(v));
            if (!skipWs(s, p)) return false;
            if (s[p] == ',') { p++; continue; }
            if (s[p] == ']') { p++; return true; }
            return false;
        }
    }
    static bool parseObj(const std::string& s, size_t& p, J& out) {
        p++;
        out = J::makeObj();
        if (!skipWs(s, p)) return false;
        if (s[p] == '}') { p++; return true; }
        while (true) {
            if (!skipWs(s, p) || s[p] != '"') return false;
            std::string key;
            if (!parseStr(s, p, key)) return false;
            if (!skipWs(s, p) || s[p] != ':') return false;
            p++;
            J v;
            if (!parseValue(s, p, v)) return false;
            out.obj.emplace_back(std::move(key), std::move(v));
            if (!skipWs(s, p)) return false;
            if (s[p] == ',') { p++; continue; }
            if (s[p] == '}') { p++; return true; }
            return false;
        }
    }
};

} // namespace gt
