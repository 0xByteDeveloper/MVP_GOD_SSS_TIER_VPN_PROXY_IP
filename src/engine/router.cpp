// ============================================================================
//  MVP_GOD_SSS_TIER — src/engine/router.cpp
// ============================================================================
#include "engine/router.h"
#include "core/log.h"
#include "core/util.h"
#include <cstring>

#ifndef _WIN32
#  include <poll.h>
#  include <unistd.h>
#endif

namespace gt {

// ---- küçük yardımcılar -------------------------------------------------------
static std::string read_head_from_tls(net::TlsStream& st) {
    std::string head;
    char b[1];
    while (true) {
        int n = st.read(b, 1);
        if (n <= 0) break;
        head += b[0];
        if (head.size() >= 4 && head.compare(head.size() - 4, 4, "\r\n\r\n") == 0) break;
        if (head.size() > 16384) break;
    }
    return head;
}

std::string build_absolute_request(const std::string& first_bytes,
                                   const std::string& auth_header) {
    if (first_bytes.empty()) return "";
    size_t eol = first_bytes.find("\r\n");
    if (eol == std::string::npos) return "";
    std::string line = first_bytes.substr(0, eol);
    size_t sp1 = line.find(' ');
    size_t sp2 = line.rfind(' ');
    if (sp1 == std::string::npos || sp2 == sp1) return "";
    std::string method = line.substr(0, sp1);
    std::string target = line.substr(sp1 + 1, sp2 - sp1 - 1);
    std::string ver = line.substr(sp2 + 1);
    if (!target.empty() && target[0] == '/') {
        // origin form — host başlığından al
        size_t hp = first_bytes.find("\r\nHost: ");
        if (hp == std::string::npos) hp = first_bytes.find("\r\nhost: ");
        if (hp == std::string::npos) return "";
        size_t hs = hp + 7;
        size_t he = first_bytes.find("\r\n", hs);
        std::string host = trim(first_bytes.substr(hs, he - hs));
        target = "http://" + host + target;
    }
    std::string out = method + " " + target + " " + ver + "\r\n";
    out += first_bytes.substr(eol + 2);               // kalan başlıklar + gövde
    if (!auth_header.empty() &&
        out.find("Proxy-Authorization:") == std::string::npos)
        out += "Proxy-Authorization: " + auth_header + "\r\n";
    return out;
}

// istemciden tam istek başlığını oku (\r\n\r\n görene dek)
static std::string read_head_from_sock(sock_t c) {
    std::string head;
    char b[1];
    while (true) {
        int n = net::recv_some(c, b, 1);
        if (n <= 0) break;
        head += b[0];
        if (head.size() >= 4 && head.compare(head.size() - 4, 4, "\r\n\r\n") == 0) break;
        if (head.size() > 32768) break;
    }
    return head;
}

// ---- zincir/socks istemci yardımcıları (upstream SOCKS5 + CONNECT) -----------
// mevcut fd üzerinden SOCKS5 CONNECT (no-auth) — Tor ve socks hops için
static bool socks5_connect_through(sock_t fd, const std::string& host, int port,
                                   std::string& err) {
    // selamlama 3 bayt: VER(5) NMETHODS(1) METHOD(0=no-auth)
    unsigned char hi[3] = {5, 1, 0};
    if (net::send_all(fd, hi, 3) < 0) { err = "socks el sikismasi yazilamadi"; return false; }
    unsigned char r[2];
    if (net::recv_some(fd, r, 2) != 2 || r[0] != 5 || r[1] != 0) {
        err = "socks upstream auth istiyor (no-auth degil)"; return false;
    }
    unsigned char req[300];
    size_t n = 0;
    req[n++] = 5; req[n++] = 1; req[n++] = 0; req[n++] = 3;   // CONNECT, DOMAIN
    size_t hl = host.size() > 255 ? 255 : host.size();
    req[n++] = (unsigned char)hl;
    memcpy(req + n, host.c_str(), hl); n += hl;
    req[n++] = (unsigned char)((port >> 8) & 0xFF);
    req[n++] = (unsigned char)(port & 0xFF);
    if (net::send_all(fd, req, n) < 0) { err = "socks CONNECT yazilamadi"; return false; }
    unsigned char rep[4];
    if (net::recv_some(fd, rep, 4) != 4 || rep[1] != 0) {
        err = "socks CONNECT reddi"; return false;
    }
    // bağlı adres gövdesini tüket (ATYP'e göre)
    if (rep[3] == 1) {
        unsigned char t[6];
        if (net::recv_some(fd, t, 6) != 6) { err = "socks yanit kisa"; return false; }
    } else if (rep[3] == 3) {
        unsigned char l;
        if (net::recv_some(fd, &l, 1) != 1) { err = "socks yanit kisa"; return false; }
        unsigned char t[256];
        if (l && net::recv_some(fd, t, l) != l) { err = "socks yanit kisa"; return false; }
        unsigned char pb[2];
        if (net::recv_some(fd, pb, 2) != 2) { err = "socks yanit kisa"; return false; }
    } else if (rep[3] == 4) {
        unsigned char t[18];
        if (net::recv_some(fd, t, 18) != 18) { err = "socks yanit kisa"; return false; }
    }
    return true;
}

// mevcut fd üzerinden HTTP CONNECT (zincir http hop'ları)
static bool http_connect_through(sock_t fd, const std::string& host, int port,
                                 std::string& err) {
    std::string req = "CONNECT " + host + ":" + std::to_string(port) +
                      " HTTP/1.1\r\nHost: " + host + ":" + std::to_string(port) +
                      "\r\n\r\n";
    if (net::send_all(fd, req.data(), req.size()) < 0) { err = "zincir CONNECT yazilamadi"; return false; }
    std::string head = net::read_response_head(fd);
    std::string status = head.substr(0, head.find("\r\n"));
    if (head.find(" 200 ") == std::string::npos) {
        err = "zincir hop reddi: " + status;
        return false;
    }
    return true;
}

// ----------------------------------------------------------------------------
Router::Router(UpstreamConfig up, int listen_port, std::string tag)
    : up_(std::move(up)), tag_(std::move(tag)), port_(listen_port) {}

Router::~Router() { stop(); }

std::string Router::last_err() const {
    std::lock_guard<std::mutex> g(err_mu_);
    return last_err_;
}

void Router::set_upstream(const UpstreamConfig& up) { up_ = up; }

bool Router::start(std::string& err) {
    lst_ = net::tcp_listen(port_, err);
    if (lst_ == GT_BAD_SOCK) return false;
    running_.store(true);
    accept_th_ = std::thread([this] { acceptLoop(); });
    return true;
}

void Router::stop() {
    if (!running_.exchange(false)) return;
    if (lst_ != GT_BAD_SOCK) { net::close_sock(lst_); lst_ = GT_BAD_SOCK; }
    if (accept_th_.joinable()) accept_th_.join();
}

void Router::acceptLoop() {
    while (running_.load()) {
        struct pollfd pfd;
        pfd.fd = lst_; pfd.events = POLLIN; pfd.revents = 0;
        if (poll(&pfd, 1, 500) != 1) continue;
        struct sockaddr_in ca;
        socklen_t cl = sizeof ca;
        sock_t c = accept(lst_, (struct sockaddr*)&ca, &cl);
        if (c == GT_BAD_SOCK) continue;
        net::set_sock_timeout(c, 90);
        conns_.fetch_add(1);
        std::thread t([this, c] { clientThread(c); });
        {
            std::lock_guard<std::mutex> g(workers_mu_);
            workers_.push_back(std::move(t));
            (void)g;
        }
    }
}

void Router::clientThread(sock_t c) {
    std::string dst_host; int dst_port = 0;
    std::string err;

    // --- protokol otomatik seçim: ilk bayta bak ---
    char peek[1];
    std::string first_bytes;
    bool is_socks = false, is_connect = false;
    {
        // 1 bayt peek (MSG_PEEK — veriyi tükettirmeden bak)
        long n = recv(c, peek, 1, MSG_PEEK);
        if (n == 1 && (unsigned char)peek[0] == 0x05) is_socks = true;
    }
    bool go = false;
    if (is_socks) {
        go = socks5Handshake(c, dst_host, dst_port, err);
    } else {
        go = httpProxyHandshake(c, dst_host, dst_port, first_bytes, is_connect, err);
    }
    if (!go) {
        fail_.fetch_add(1);
        { std::lock_guard<std::mutex> g(err_mu_); last_err_ = err; }
        net::close_sock(c);
        return;
    }

    // --- upstream kur ---
    net::TlsStream tls;
    sock_t rawfd = GT_BAD_SOCK;
    bool tls_up = false;
    if (!establishUpstream(dst_host, dst_port, tls, rawfd, tls_up, err)) {
        fail_.fetch_add(1);
        { std::lock_guard<std::mutex> g(err_mu_); last_err_ = err; }
        // SOCKS5'e hata yanıtı ver (bağlantı reddi)
        if (is_socks) {
            char rep[10] = {5, 1, 0, 1, 0, 0, 0, 0, 0, 0};
            net::send_all(c, rep, 10);
        } else {
            std::string r = "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n"
                            "X-MvpGodTier-Error: " + err.substr(0, 100) + "\r\n\r\n";
            net::send_all(c, r.data(), r.size());
        }
        net::close_sock(c);
        return;
    }
    if (is_socks) {
        char rep[10] = {5, 0, 0, 1, 0, 0, 0, 0, 0, 0};
        net::send_all(c, rep, 10);
    } else if (is_connect) {
        std::string r = "HTTP/1.1 200 Connection Established\r\n\r\n";
        net::send_all(c, r.data(), r.size());
    }

    // ---- 80 portu: Hola trial'da CONNECT 403 — ilk isteği absolute-form'a
    // çevirip upstream'e gönder (freevpn.py'deki kanıtlanmış yol). SOCKS5
    // istemcisinden gelen ilk istek henüz okunmadıysa şimdi okunur.
    if (dst_port == 80 && !is_connect) {
        std::string req = first_bytes;
        if (req.empty()) req = read_head_from_sock(c);
        std::string auth = up_.kind == UpstreamConfig::HOLA_TRIAL
                               ? basic_auth(up_.h_login, up_.h_agent_key) : "";
        std::string absreq = build_absolute_request(req, auth);
        if (!absreq.empty()) {
            int w = tls_up ? tls.write(absreq.data(), absreq.size())
                           : net::send_all(rawfd, absreq.data(), absreq.size());
            if (w >= 0) bytes_up_.fetch_add(absreq.size());
        }
    }

    // --- iki yönlü relay ---
    sock_t upfd = tls_up ? tls.fd() : rawfd;
    struct pollfd pf[2];
    char buf[16384];
    while (running_.load()) {
        pf[0].fd = c;      pf[0].events = POLLIN; pf[0].revents = 0;
        pf[1].fd = upfd;   pf[1].events = POLLIN; pf[1].revents = 0;
        int pr = poll(pf, 2, 1000);
        if (pr < 0) break;
        if (pr == 0 && !(tls_up && tls.has_pending())) continue;
        // istemci -> upstream
        if (pf[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            int n = net::recv_some(c, buf, sizeof buf);
            if (n < 0) break;
            if (n == 0) break;                     // istemci kapandı
            int w = tls_up ? tls.write(buf, (size_t)n) : net::send_all(rawfd, buf, (size_t)n);
            if (w < 0) break;
            bytes_up_.fetch_add((uint64_t)n);
        }
        // upstream -> istemci (TLS tamponundaki bekleyen veri dahil)
        if ((pf[1].revents & (POLLIN | POLLHUP | POLLERR)) || (tls_up && tls.has_pending())) {
            int n = tls_up ? tls.read(buf, sizeof buf) : net::recv_some(rawfd, buf, sizeof buf);
            if (n < 0) break;
            if (n == 0) break;                     // upstream kapandı
            if (net::send_all(c, buf, (size_t)n) < 0) break;
            bytes_down_.fetch_add((uint64_t)n);
        }
    }
    if (tls_up) tls.close();
    if (rawfd != GT_BAD_SOCK) net::close_sock(rawfd);
    net::close_sock(c);
}

// ---- SOCKS5 (RFC 1928 — yalnız CONNECT, no-auth) -----------------------------
bool Router::socks5Handshake(sock_t c, std::string& dst_host, int& dst_port,
                             std::string& err) {
    unsigned char h[2];
    if (net::recv_some(c, h, 2) != 2) { err = "socks5 selamlasma kisa"; return false; }
    if (h[0] != 5) { err = "socks5 degil"; return false; }
    int nmethods = h[1];
    unsigned char mbuf[16];
    if (nmethods > 0 && nmethods <= 16) {
        if (net::recv_some(c, mbuf, (size_t)nmethods) != nmethods) { err = "yontem okunamadi"; return false; }
    } else if (nmethods > 16) {
        err = "yontem sayisi fazla"; return false;
    }
    unsigned char ok[2] = {5, 0};
    if (net::send_all(c, ok, 2) < 0) { err = "yanit yazilamadi"; return false; }

    unsigned char r[4];
    if (net::recv_some(c, r, 4) != 4) { err = "istek basligi kisa"; return false; }
    if (r[1] != 1) { err = "yalniz CONNECT desteklenir"; return false; }
    char addr[260];
    if (r[3] == 1) {          // IPv4
        unsigned char ip[4];
        if (net::recv_some(c, ip, 4) != 4) { err = "ipv4 kisa"; return false; }
        snprintf(addr, sizeof addr, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    } else if (r[3] == 3) {    // alan adı
        unsigned char l;
        if (net::recv_some(c, &l, 1) != 1) { err = "alan uzunlugu kisa"; return false; }
        char name[256];
        if (l > 0 && net::recv_some(c, name, l) != l) { err = "alan kisa"; return false; }
        name[l] = 0;
        snprintf(addr, sizeof addr, "%s", name);
    } else if (r[3] == 4) {    // IPv6 — desteklenmez (MVP): hata yanıtı
        err = "ipv6 desteklenmiyor";
        return false;
    } else { err = "atyp bilinmiyor"; return false; }
    unsigned char pb[2];
    if (net::recv_some(c, pb, 2) != 2) { err = "port kisa"; return false; }
    dst_host = addr;
    dst_port = (pb[0] << 8) | pb[1];
    return true;
}

// ---- HTTP proxy otomatik tanıma (tarayıcı / sistem proxy modu) ---------------
bool Router::httpProxyHandshake(sock_t c, std::string& dst_host, int& dst_port,
                                std::string& first_bytes, bool& is_connect,
                                std::string& err) {
    std::string head;
    char b[1];
    while (true) {
        int n = net::recv_some(c, b, 1);
        if (n <= 0) { err = "http proxy: ilk istek okunamadi"; return false; }
        head += b[0];
        if (head.size() >= 4 && head.compare(head.size() - 4, 4, "\r\n\r\n") == 0) break;
        if (head.size() > 32768) break;
    }
    first_bytes = head;
    size_t sp1 = head.find(' ');
    std::string method = sp1 == std::string::npos ? "" : head.substr(0, sp1);
    size_t sp2 = head.find(' ', sp1 + 1);
    std::string target = sp2 == std::string::npos ? "" : head.substr(sp1 + 1, sp2 - sp1 - 1);
    if (target.rfind("http://", 0) == 0 || target.rfind("https://", 0) == 0) {
        // absolute-form
        std::string rest = target.substr(target.find("//") + 2);
        size_t slash = rest.find('/');
        std::string hostport = slash == std::string::npos ? rest : rest.substr(0, slash);
        size_t colon = hostport.rfind(':');
        if (colon != std::string::npos) {
            dst_host = hostport.substr(0, colon);
            dst_port = atoi(hostport.substr(colon + 1).c_str());
        } else { dst_host = hostport; dst_port = 80; }
    } else if (lower(method) == "connect") {
        // CONNECT host:port
        size_t colon = target.rfind(':');
        if (colon == std::string::npos) { err = "CONNECT hedefi bozuk"; return false; }
        dst_host = target.substr(0, colon);
        dst_port = atoi(target.substr(colon + 1).c_str());
        is_connect = true;
    } else {
        err = "http proxy istegi anlasilamadi: " + head.substr(0, 40);
        return false;
    }
    return true;
}

// ---- upstream ----------------------------------------------------------------
bool Router::establishUpstream(const std::string& host, int port,
                               net::TlsStream& tls, sock_t& rawfd, bool& tls_up,
                               std::string& err) {
    const std::string ah = basic_auth(up_.h_login, up_.h_agent_key);
    // Host header varyantı (flip) — freevpn.py _flip_auth_scheme
    const std::string ah_flipped = ah.compare(0, 6, "Basic ") == 0
                                       ? "basic " + ah.substr(6) : ah;

    if (up_.kind == UpstreamConfig::HOLA_TRIAL) {
        // 1) TLS'e bağlan: önce SNI yok (kanıtlanmış), sonra SNI'li
        for (int mode = 0; mode < 2; mode++) {
            std::string sni = mode == 0 ? "" : up_.h_host;
            std::string cerr;
            if (!tls.connect(up_.h_ip, up_.h_port, sni, 30, cerr, up_.tls_ver)) {
                err = "hola upstream tls: " + cerr;
                continue;
            }
            // 2) 443 -> CONNECT (güvenli yol); 80 -> absolute-form (CONNECT 403)
            if (port == 80) {
                tls_up = true;
                return true;   // absolute-form, relay döngüsü ilk isteği çevirir
            }
            for (int flip = 0; flip < 2; flip++) {
                std::string use = flip == 0 ? ah : ah_flipped;
                std::string req = "CONNECT " + host + ":" + std::to_string(port) +
                                  " HTTP/1.1\r\nHost: " + host + ":" + std::to_string(port) +
                                  "\r\nProxy-Authorization: " + use +
                                  "\r\nProxy-Connection: keep-alive\r\n\r\n";
                if (tls.write(req.data(), req.size()) < 0) { err = "CONNECT yazilamadi"; break; }
                std::string head = read_head_from_tls(tls);
                std::string status = head.substr(0, head.find("\r\n"));
                if (status.find(" 200 ") != std::string::npos) {
                    tls_up = true;
                    return true;
                }
                err = "hola CONNECT reddi: " + status;
                // aynı TLS akışında flip denenemez (yanıt zaten geldi) — yeniden bağlan
                break;
            }
        }
        return false;
    }

    if (up_.kind == UpstreamConfig::PLAIN_HTTP) {
        // düz TCP proxy
        rawfd = net::tcp_connect(up_.p_ip, up_.p_port, 20);
        if (rawfd == GT_BAD_SOCK) { err = "düz proxy bağlantı hatası"; return false; }
        if (port == 80) { tls_up = false; return true; }
        std::string req = "CONNECT " + host + ":" + std::to_string(port) +
                          " HTTP/1.1\r\nHost: " + host + ":" + std::to_string(port) + "\r\n\r\n";
        if (net::send_all(rawfd, req.data(), req.size()) < 0) { err = "CONNECT yazilamadi"; net::close_sock(rawfd); rawfd = GT_BAD_SOCK; return false; }
        std::string head = net::read_response_head(rawfd);
        std::string status = head.substr(0, head.find("\r\n"));
        if (status.find(" 200 ") == std::string::npos) {
            err = "düz proxy CONNECT reddi: " + status;
            net::close_sock(rawfd); rawfd = GT_BAD_SOCK;
            return false;
        }
        tls_up = false;
        return true;
    }

    if (up_.kind == UpstreamConfig::SOCKS5_UP) {
        // tek SOCKS5 upstream (Tor / manuel socks) — ham tünel
        rawfd = net::tcp_connect(up_.s_ip, up_.s_port, 20);
        if (rawfd == GT_BAD_SOCK) { err = "socks upstream bağlanamadı: " + up_.s_ip; return false; }
        if (!socks5_connect_through(rawfd, host, port, err)) {
            net::close_sock(rawfd); rawfd = GT_BAD_SOCK;
            return false;
        }
        tls_up = false;
        return true;
    }

    if (up_.kind == UpstreamConfig::CHAIN) {
        // çok-hopslu zincir: her hop bir sonrakine CONNECT açar, son hop hedefe
        if (up_.hops.empty()) { err = "zincir hops yok"; return false; }
        rawfd = net::tcp_connect(up_.hops[0].ip, up_.hops[0].port, 20);
        if (rawfd == GT_BAD_SOCK) { err = "zincir 1. hop bağlanamadı"; return false; }
        for (size_t i = 1; i < up_.hops.size(); i++) {
            bool ok = up_.hops[i - 1].socks
                          ? socks5_connect_through(rawfd, up_.hops[i].ip, up_.hops[i].port, err)
                          : http_connect_through(rawfd, up_.hops[i].ip, up_.hops[i].port, err);
            if (!ok) { net::close_sock(rawfd); rawfd = GT_BAD_SOCK; return false; }
        }
        if (!up_.hops.back().socks ? http_connect_through(rawfd, host, port, err)
                                   : socks5_connect_through(rawfd, host, port, err)) {
            net::close_sock(rawfd); rawfd = GT_BAD_SOCK;
            return false;
        }
        tls_up = false;
        return true;
    }

    if (up_.kind == UpstreamConfig::VPN_GATE_OVPN) {
        err = "VPN Gate kutusu OpenVPN yapılandırma modundadır — .ovpn dosyasını "
              "kullanın (Yönlendirme bu kutu için devre dışı)";
        return false;
    }

    err = "bilinmeyen upstream türü";
    return false;
}

} // namespace gt
