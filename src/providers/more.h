// ============================================================================
//  MVP_GOD_SSS_TIER — src/providers/more.h
//  Ek ücretsiz sağlayıcılar (sömürülmeden, telemetrisiz):
//    * VPN Gate  — SoftEther projesinin kamu VPN geçit listesi (CSV API),
//                  ülke filtreli + OpenVPN yapılandırma (base64) export
//    * Tor       — yerel Tor SOCKS algılama (9050/9150); yoksa zincir fallback
//    * Kaynaklar — GitHub'daki canlı ücretsiz proxy listeleri (http/socks4/socks5)
//    * Zincir    — çok-hops'lu bağlantı için canlı proxy seçimi
//  Tüm çağrılar C++ motorun kendi HTTP istemcisinden (WinHTTP/OpenSSL) gider —
//  hiçbir üçüncü parti istemci/telemetri yok.
// ============================================================================
#pragma once
#include <string>
#include <vector>

namespace gt {

// ---- VPN Gate ---------------------------------------------------------------
struct VpnGateServer {
    std::string hostname, ip, country_short, country_long;
    long long score = 0, ping_ms = 0, speed_bps = 0, sessions = 0, uptime_days = 0;
    std::string ovpn_b64;                  // OpenVPN yapılandırması (base64)
};
// country: 2 harf kod ("" = hepsi); puan sırasına göre ilk `limit` adet
std::vector<VpnGateServer> vpngate_fetch(const std::string& country, int limit,
                                         std::string& err);
std::string vpngate_speed_mbps(long long speed_bps);   // "12.4 Mbps"

// ---- Tor --------------------------------------------------------------------
// yerel Tor SOCKS portu (9050/9150) — 0 dönerse Tor kurulu değil
int tor_detect_port();

// ---- ücretsiz proxy kaynakları (GitHub — x1 araştırması) ---------------------
struct SourceProxy {
    std::string addr;      // ip:port
    std::string type;      // http | socks4 | socks5
    std::string source;    // monosans | speedx | proxyscrape
};
std::vector<SourceProxy> sources_fetch(int per_source_limit, std::string& err);

// TCP açılabilirlik testiyle canlı `count` adet proxy seç (zincir hops'ları)
std::vector<SourceProxy> pick_alive(const std::vector<SourceProxy>& from, int count);

// ---- tünel sağlık kontrolü (hola dışı TÜM sağlayıcılar için) ----------------
// Yerel router'a SOCKS5 istemcisi olarak bağlanır, ip-api.com'a absolute-form
// GET atar: zincir dahil uçtan uca çıkış IP'si kanıtlanır. JSON döner.
std::string socks_exit_ip_json(int local_port, int timeout_s);

} // namespace gt
