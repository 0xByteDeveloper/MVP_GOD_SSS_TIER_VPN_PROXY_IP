// ============================================================================
//  MVP_GOD_SSS_TIER — src/engine/router.h
//  Yerel yönlendirici: her instance kendi 127.0.0.1:<port> dinleyicisini
//  açar. Dinleyici PROTOKOL OTOMATİK SEÇER:
//    * SOCKS5  (hook'lu process'ler buraya bağlanır — "gelişmiş Proxifier")
//    * HTTP proxy (CONNECT + absolute-form) — tarayıcı/sistem proxy modu
//  Upstream akışları:
//    * HOLA_TRIAL  : TLS(SNI yok) -> CONNECT (443) / absolute-form (80)
//    * PLAIN_HTTP  : ücretsiz listeden tek düz HTTP proxy
//    * SOCKS5_UP   : SOCKS5 upstream (yerel Tor 9050 / manuel socks ip:port)
//    * CHAIN       : çok-hopslu zincir (2-3 katman — Tor benzeri katmanlama)
//    * VPN_GATE_OVPN: OpenVPN yapılandırma modu (502 bilgi yanıtı)
//  freevpn.py'de uçtan uca kanıtlanmış davranışların birebir taşıması.
// ============================================================================
#pragma once
#include "../net/net.h"
#include "../net/tls.h"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gt {

struct UpstreamConfig {
    enum Kind { HOLA_TRIAL, PLAIN_HTTP, SOCKS5_UP, CHAIN, VPN_GATE_OVPN } kind = HOLA_TRIAL;
    // Hola trial proxy
    std::string h_host, h_ip, h_login, h_agent_key;
    int h_port = 22225;
    // TLS sürüm seçimi (şifreleme paneli): "" = otomatik, "tls13", "tls12"
    std::string tls_ver;
    // düz proxy (kaynak listesinden)
    std::string p_ip;
    int p_port = 0;
    // SOCKS5 upstream (Tor / manuel socks)
    std::string s_ip;
    int s_port = 0;
    // zincir hops'ları (sırayla; her hop http ya da socks5 konuşur)
    struct Hop { bool socks; std::string ip; int port; };
    std::vector<Hop> hops;
};

class Router {
public:
    Router(UpstreamConfig up, int listen_port, std::string tag);
    ~Router();
    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;

    bool start(std::string& err);
    void stop();
    bool running() const { return running_.load(); }
    int port() const { return port_; }

    // istatistik (UI kutucuklarında gösterilir)
    uint64_t conns() const { return conns_.load(); }
    uint64_t bytes_up() const { return bytes_up_.load(); }
    uint64_t bytes_down() const { return bytes_down_.load(); }
    uint64_t fail() const { return fail_.load(); }
    std::string last_err() const;
    void set_upstream(const UpstreamConfig& up);   // New User Mode: kimlik takası

private:
    void acceptLoop();
    void clientThread(sock_t c);
    bool socks5Handshake(sock_t c, std::string& dst_host, int& dst_port,
                         std::string& err);
    bool httpProxyHandshake(sock_t c, std::string& dst_host, int& dst_port,
                            std::string& first_bytes, bool& is_connect,
                            std::string& err);
    // upstream bağlantısı kur (Hola: TLS; düz/socks/zincir: TCP) — CONNECT
    // veya 80 için absolute-form istek hazırlığı döner
    bool establishUpstream(const std::string& host, int port, net::TlsStream& tls,
                           sock_t& rawfd, bool& tls_up, std::string& err);

    UpstreamConfig up_;
    std::string tag_;
    int port_;
    sock_t lst_ = GT_BAD_SOCK;
    std::atomic<bool> running_{false};
    std::thread accept_th_;
    std::vector<std::thread> workers_;      // kısa ömürlü istemci iş parçacıkları
    std::mutex workers_mu_;
    std::atomic<uint64_t> conns_{0}, bytes_up_{0}, bytes_down_{0}, fail_{0};
    mutable std::mutex err_mu_;
    std::string last_err_;
};

// hedef 80 ise absolute-form tek istek çevirisi gövdesi (helper — router.cpp)
std::string build_absolute_request(const std::string& first_bytes,
                                   const std::string& auth_header);

} // namespace gt
