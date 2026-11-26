// ============================================================================
//  GOD_SSS_TIER_VPN — src/providers/hola.h
//  Hola VPN "yeni kullanıcı ücretsiz trial" akışı (browser'sız):
//    1. uuid üret (yeni kullanıcı = bu uuid)
//    2. POST background_init?uuid=...   → session key + güncel ext_ver
//    3. GET  zgettunnels?country=...      → agent_key + zagent ip_list + portlar
//    4. login = user-uuid-<uuid>-is_prem-0 ; şifre = agent_key
//    5. upstream: TLS(ip:trial_port) + Proxy-Authorization (SNI YOK — WAF kuralı)
//  Kaynak: snawoot hola-proxy (reconstructed fork) mekanizması; freevpn.py'de
//  uçtan uca doğrulandı (Vultr çıkış IP'si görüldü).
// ============================================================================
#pragma once
#include <string>
#include <vector>

namespace gt {

struct HolaEndpoint {
    std::string host;      // zagentXXX.hola.org
    std::string ip;        // çözümlenmiş IP
    int port = 22225;      // trial (veya direct 22222)
};

struct HolaIdentity {
    std::string uuid;          // "kullanıcı"
    std::string login;         // user-uuid-...-is_prem-0
    std::string agent_key;     // Basic auth şifresi
    std::string session_key;
    std::string ext_ver;
    std::string country;        // talep edilen ülke kodu
    std::vector<HolaEndpoint> endpoints;
    std::string err;
    bool ok = false;
};

// Tam akış (retry + backoff dahil — freevpn.py davranışı)
HolaIdentity hola_acquire(const std::string& country, int timeout_s = 25);

// Çıkış IP'sini tünelden sorgula: absolute-form GET http://ip-api.com/json
// (ip-api.com: ücretsiz, JSON'da ülke de verir)
// dönen: {"status":"ok", "query":"185.x.x.x", "country":"..."} veya hata
std::string hola_exit_ip_json(const HolaEndpoint& ep, const std::string& login,
                              const std::string& agent_key, int timeout_s = 20);

} // namespace gt
