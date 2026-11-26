// ============================================================================
//  GOD_SSS_TIER_VPN — src/core/util.h
//  UUID üretimi, base64, url-encode, zaman, dosya yardımcıları
// ============================================================================
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace gt {

// ---- kimlik ----
std::string uuid_hex();                       // 32 karakter hex (Hola uuid)
std::string rand_hex(int chars);               // ping_id vb.
double rand01();

// ---- kodlama ----
std::string b64_encode(const std::string& raw);
std::string b64_decode(const std::string& s);    // vpngate ovpn çözümü
std::string url_encode(const std::string& s); // form-urlencoded

// ---- zaman ----
uint64_t now_ms();
std::string now_iso();                        // 2026-11-27T12:34:56Z
std::string fmt_duration(uint64_t ms);        // "1s 02m" -> "3d 04h 05m" gibi

// ---- dosya ----
bool read_file(const std::string& path, std::string& out);
bool write_file(const std::string& path, const std::string& content);
bool mkdir_p(const std::string& path);
bool file_exists(const std::string& path);

// ---- metin ----
std::string lower(const std::string& s);
std::string upper(const std::string& s);
std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& s, char sep);
std::map<std::string, std::string> parse_query(const std::string& query);

// Hola basic auth başlığı: "Basic <b64(login:password)>"
std::string basic_auth(const std::string& login, const std::string& password);

// ping_id için ondalık: 17 hane
std::string ping_id_str();

} // namespace gt
