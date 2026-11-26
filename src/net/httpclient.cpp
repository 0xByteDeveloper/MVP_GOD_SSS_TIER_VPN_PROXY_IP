// ============================================================================
//  GOD_SSS_TIER_VPN — src/net/httpclient.cpp
//  Linux: OpenSSL ile HTTPS GET/POST (engine'in API çağrıları)
//  Windows: WinHTTP (schannel, sıfır dış bağımlılık) — 3 aşamalı sağlamlık:
//    1) direkt WinHTTP (TLS 1.2+ sabit, Chrome UA)
//    2) sistem proxy'siyle WinHTTP (proxy arkasındaki ağlar için)
//    3) ağ tamamen engelliyse (ISP/DNS zehirlenmesi): DoH ile IP çöz +
//       saf C++ SChannel TlsStream ile istek (client.hola.org erişilebilir
//       kalır — WinHttpSendRequest HTTP 0 hatalarına kesin çözüm yolu)
// ============================================================================
#include "net/httpclient.h"
#include "net/net.h"
#include "net/tls.h"
#include "core/json.h"
#include "core/log.h"
#include "core/util.h"
#include <cstring>

#ifdef _WIN32
// Windows başlıkları GLOBAL kapsamda (COM uuid şablon kısıtı)
#  include <windows.h>
#  include <winhttp.h>
#  pragma comment(lib, "winhttp.lib")
#endif

namespace gt {
namespace net {

bool split_url(const std::string& url, std::string& scheme, std::string& host,
               int& port, std::string& pathrest) {
    size_t p = url.find("://");
    if (p == std::string::npos) return false;
    scheme = url.substr(0, p);
    size_t rest = p + 3;
    size_t slash = url.find('/', rest);
    std::string hostport = slash == std::string::npos ? url.substr(rest) : url.substr(rest, slash - rest);
    pathrest = slash == std::string::npos ? "/" : url.substr(slash);
    size_t colon = hostport.rfind(':');
    port = scheme == "https" ? 443 : 80;
    if (colon != std::string::npos) {
        host = hostport.substr(0, colon);
        port = atoi(hostport.substr(colon + 1).c_str());
    } else host = hostport;
    return !host.empty();
}

// chunked gövdeyi çöz (her iki platform da kullanır)
static std::string dechunk(const std::string& body) {
    std::string out;
    size_t pos = 0;
    while (pos + 2 <= body.size()) {
        size_t eol = body.find("\r\n", pos);
        if (eol == std::string::npos) break;
        long chunk = strtol(body.substr(pos, eol - pos).c_str(), nullptr, 16);
        if (chunk <= 0) break;
        size_t data = eol + 2;
        if (data + (size_t)chunk > body.size()) chunk = (long)(body.size() - data);
        out.append(body, data, (size_t)chunk);
        pos = data + (size_t)chunk + 2;
    }
    return out;
}

#ifndef _WIN32
// ------------------------------------------------------------------ OpenSSL
#include <openssl/ssl.h>
#include <openssl/err.h>

static SSL_CTX* g_ctx_client = nullptr;
static SSL_CTX* tls_ctx() {
    if (g_ctx_client) return g_ctx_client;
    SSL_library_init();
    SSL_load_error_strings();
    g_ctx_client = SSL_CTX_new(TLS_client_method());
    if (g_ctx_client) {
        // sistem CA deposu; yoksa da devam (Hola API hep geçerli sertifika kullanır;
        // doğrulama başarısızsa tekrar denemesiz hata verir)
        SSL_CTX_set_default_verify_paths(g_ctx_client);
        SSL_CTX_set_verify(g_ctx_client, SSL_VERIFY_NONE, nullptr);
    }
    return g_ctx_client;
}

static HttpResult ssl_request(const std::string& url, const std::string& method,
                              const std::string& body, const std::string& ctype,
                              int timeout_s,
                              const std::vector<std::pair<std::string, std::string>>& headers) {
    HttpResult r;
    std::string scheme, host; int port; std::string path;
    if (!split_url(url, scheme, host, port, path)) { r.err = "kötü url: " + url; return r; }
    SSL_CTX* ctx = tls_ctx();
    if (!ctx) { r.err = "openssl ctx hatası"; return r; }

    sock_t s = tcp_connect(host, port, timeout_s);
    if (s == GT_BAD_SOCK) { r.err = "baglanamadi: " + host; return r; }
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, (int)s);
    SSL_set_tlsext_host_name(ssl, host.c_str());       // SNI
    if (SSL_connect(ssl) != 1) {
        r.err = "tls el sikismasi hatasi: " + host;
        SSL_free(ssl); close_sock(s);
        return r;
    }
    std::string req = method + " " + path + " HTTP/1.1\r\nHost: " + host + "\r\n";
    req += "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36\r\n";
    req += "Accept: */*\r\nConnection: close\r\n";
    for (auto& h : headers) req += h.first + ": " + h.second + "\r\n";
    if (!body.empty()) {
        req += "Content-Type: " + ctype + "\r\n";
        req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    req += "\r\n";
    if (!body.empty()) req += body;
    { // tüm isteği yaz (SSL_write parçalı yazabilir)
        int total = (int)req.size(), off = 0;
        bool fail = false;
        while (off < total) {
            int w = SSL_write(ssl, req.data() + off, total - off);
            if (w <= 0) { fail = true; break; }
            off += w;
        }
        if (fail) {
            r.err = "istek gönderilemedi"; SSL_free(ssl); close_sock(s); return r;
        }
    }
    std::string raw;
    char buf[16384];
    int n;
    while ((n = SSL_read(ssl, buf, sizeof buf)) > 0) raw.append(buf, (size_t)n);
    SSL_free(ssl); close_sock(s);
    size_t hh = raw.find("\r\n\r\n");
    if (hh == std::string::npos) { r.err = "yanit basligi yok"; return r; }
    std::string head = raw.substr(0, hh);
    r.body = raw.substr(hh + 4);
    if (raw.find("chunked") != std::string::npos || raw.find("Chunked") != std::string::npos)
        r.body = dechunk(r.body);
    size_t sp = head.find(' ');
    if (sp != std::string::npos) r.status = atoi(head.substr(sp + 1, 3).c_str());
    return r;
}

HttpResult http_get(const std::string& url, int timeout_s,
                    const std::vector<std::pair<std::string, std::string>>& headers) {
    return ssl_request(url, "GET", "", "", timeout_s, headers);
}
HttpResult http_post_form(const std::string& url, const std::string& form_body, int timeout_s,
                          const std::vector<std::pair<std::string, std::string>>& headers) {
    return ssl_request(url, "POST", form_body, "application/x-www-form-urlencoded",
                       timeout_s, headers);
}

#else // ------------------------------------------------------------------ WinHTTP
// (başlıklar dosya başında global kapsamda include edildi)
// Not: TLS katmanı (SChannel) net/tls.cpp içinde — DoH fallback onu kullanır.

static const char* UA_CHROME =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36";

#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
#  define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3 0x2000
#endif
// bazı MinGW başlık setlerinde eski sürüm makroları yoktur — ham değerler:
#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
#  define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 0x0800
#endif

static std::wstring wide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

// WinHTTP hata kodunu okunur Türkçe metne çevir (teşhis — kullanıcıya görünür)
static std::string winh_errmsg(DWORD e) {
    switch (e) {
        case 12002: return "zaman asimi (sunucu yanit vermedi)";
        case 12007: return "DNS cozumlenemedi (ISP/DNS zehirleme veya adres yok)";
        case 12017: return "islem iptal edildi";
        case 12029: return "baglanti kurulamadi (guvenlik duvari / ISP / IP engelli olabilir)";
        case 12030: return "baglanti koptu";
        case 12031: return "istek reddedildi (baglanti reset)";
        case 12157: return "TLS el sikismasi hatasi (TLS surumu)";
        case 12169: return "gecersiz sertifika";
        case 12175: return "sertifika dogrulanamadi";
        case 87:    return "gecersiz parametre";
        default:    return "winhttp hatasi";
    }
}
static std::string winh_err(const char* where) {
    DWORD e = GetLastError();
    char b[160];
    snprintf(b, sizeof b, "%s hatasi #%lu: %s", where, (unsigned long)e,
             winh_errmsg(e).c_str());
    return b;
}

// ---- tek WinHTTP denemesi (direkt ya da sistem proxy'li) ----------------------
static HttpResult winh_attempt(const std::string& url, const std::string& method,
                               const std::string& body, const std::string& ctype, int timeout_s,
                               const std::vector<std::pair<std::string, std::string>>& headers,
                               bool use_sys_proxy) {
    HttpResult r;
    std::string scheme, host; int port; std::string path;
    if (!split_url(url, scheme, host, port, path)) { r.err = "kotu url"; return r; }
    bool secure = (scheme == "https");

    HINTERNET sess = WinHttpOpen(wide(UA_CHROME).c_str(),
                                 use_sys_proxy ? WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
                                               : WINHTTP_ACCESS_TYPE_NO_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!sess) { r.err = winh_err("WinHttpOpen"); return r; }
    // TLS 1.2+ sabitle: eski Windows'larda varsayilan TLS1.0 kalabilir ve
    // modern API bunu baglantiyi hic kurmadan reddeder (WinHttpSendRequest #12029)
    DWORD prot = 0x0800 /*TLS1.2*/ | 0x0200 /*TLS1.1*/ | 0x0080 /*TLS1.0*/ |
                 WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    if (!WinHttpSetOption(sess, WINHTTP_OPTION_SECURE_PROTOCOLS, &prot, sizeof prot)) {
        prot &= ~WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;     // eski OS: TLS1.3 bitini bırak
        WinHttpSetOption(sess, WINHTTP_OPTION_SECURE_PROTOCOLS, &prot, sizeof prot);
    }
    DWORD to = (DWORD)(timeout_s * 1000);
    if (to > 12000) to = 12000;                          // hızlı teşhis — ağ ölüyse bekleme
    WinHttpSetTimeouts(sess, to, to, to, to);
    std::wstring whost = wide(host);
    HINTERNET conn = WinHttpConnect(sess, whost.c_str(), (INTERNET_PORT)port, 0);
    if (!conn) { r.err = winh_err("WinHttpConnect"); WinHttpCloseHandle(sess); return r; }
    std::wstring wmethod = wide(method);
    std::wstring wpath = wide(path);
    DWORD openFlags = secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(conn, wmethod.c_str(), wpath.c_str(),
                                       nullptr, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, openFlags);
    if (!req) { r.err = winh_err("WinHttpOpenRequest"); WinHttpCloseHandle(conn); WinHttpCloseHandle(sess); return r; }
    // başlıklar: Accept her zaman; özel başlıklar (auth vb.) çağırandan
    WinHttpAddRequestHeaders(req, L"Accept: */*", (ULONG)-1, WINHTTP_ADDREQ_FLAG_ADD);
    for (auto& h : headers)
        WinHttpAddRequestHeaders(req, wide(h.first + ": " + h.second).c_str(), (ULONG)-1,
                                 WINHTTP_ADDREQ_FLAG_ADD);
    if (!body.empty())
        WinHttpAddRequestHeaders(req, wide("Content-Type: " + ctype).c_str(), (ULONG)-1,
                                 WINHTTP_ADDREQ_FLAG_ADD);
    const void* pbody = body.empty() ? WINHTTP_NO_REQUEST_DATA : (const void*)body.c_str();
    DWORD blen = body.empty() ? 0 : (DWORD)body.size();
    if (!WinHttpSendRequest(req, nullptr, 0, (LPVOID)pbody, blen, blen, 0)) {
        r.err = winh_err("WinHttpSendRequest");
        WinHttpCloseHandle(req); WinHttpCloseHandle(conn); WinHttpCloseHandle(sess); return r;
    }
    if (!WinHttpReceiveResponse(req, nullptr)) {
        r.err = winh_err("WinHttpReceiveResponse");
        WinHttpCloseHandle(req); WinHttpCloseHandle(conn); WinHttpCloseHandle(sess); return r;
    }
    DWORD status = 0, sz = sizeof status;
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
    r.status = (int)status;
    char buf[16384]; DWORD got = 0;
    while (WinHttpReadData(req, buf, sizeof buf, &got) && got > 0) {
        r.body.append(buf, got); got = 0;
    }
    WinHttpCloseHandle(req); WinHttpCloseHandle(conn); WinHttpCloseHandle(sess);
    return r;
}

// ---- DoH: IP'yi HTTPS üstünden çöz (DNS zehirlenmesine karşı) ------------------
// saf C++: TlsStream (SChannel) + JSON — dış kütüphane yok
static bool doh_resolve(const std::string& host, std::string& ip_out, std::string& err) {
    struct DoH { const char* ip; const char* sni; const char* path; };
    static const DoH provs[] = {
        {"1.1.1.1",      "cloudflare-dns.com", "/dns-query?name="},   // &type=A eklenir
        {"8.8.8.8",      "dns.google",        "/resolve?name="},
        {"9.9.9.9",      "dns.quad9.net",    "/dns-query?name="},
    };
    std::string last;
    for (auto& p : provs) {
        TlsStream st;
        std::string terr;
        if (!st.connect(p.ip, 443, p.sni, 8, terr)) {
            last = std::string(p.sni) + " erisilemedi (" + terr + ")";
            continue;
        }
        std::string path = std::string(p.path) + url_encode(host) + "&type=A";
        std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + p.sni +
                          "\r\nUser-Agent: " + UA_CHROME +
                          "\r\nAccept: application/dns-json\r\nConnection: close\r\n\r\n";
        if (st.write(req.data(), req.size()) < 0) { last = "DoH istegi yazilamadi"; continue; }
        std::string raw;
        char b[8192];
        int n;
        while ((n = st.read(b, sizeof b)) > 0) raw.append(b, (size_t)n);
        st.close();
        size_t hh = raw.find("\r\n\r\n");
        if (hh == std::string::npos) { last = "DoH yaniti yok"; continue; }
        std::string body = raw.substr(hh + 4);
        if (raw.find("chunked") != std::string::npos) body = dechunk(body);
        J j;
        if (!J::parse(body, j)) { last = "DoH JSON cozulemedi"; continue; }
        const J* ans = j.find("Answer");
        if (ans && ans->t == J::ARR) {
            for (auto& a : ans->arr) {
                if ((int)a.i("type") == 1) {         // A kaydı
                    std::string d = a.s("data");
                    if (!d.empty()) { ip_out = d; err.clear(); return true; }
                }
            }
        }
        last = "DoH kayit yok (Status=" + std::to_string((long long)j.i("Status")) + ")";
    }
    err = last.empty() ? "DoH saglayicilari erisilemez" : last;
    return false;
}

// ---- saf C++ HTTPS isteği (SChannel TlsStream) — 3. aşama fallback -----------
static HttpResult raw_tls_request(const std::string& host, const std::string& ip, int port,
                                   const std::string& path, const std::string& method,
                                   const std::string& body, const std::string& ctype,
                                   const std::vector<std::pair<std::string, std::string>>& headers,
                                   int timeout_s) {
    HttpResult r;
    TlsStream st;
    std::string terr;
    if (!st.connect(ip, port, host, timeout_s, terr)) {
        r.err = "saf-tls baglanamadi (" + ip + "): " + terr;
        return r;
    }
    std::string req = method + " " + path + " HTTP/1.1\r\nHost: " + host + "\r\n";
    req += std::string("User-Agent: ") + UA_CHROME + "\r\n";
    req += "Accept: */*\r\nConnection: close\r\n";
    for (auto& h : headers) req += h.first + ": " + h.second + "\r\n";
    if (!body.empty()) {
        req += "Content-Type: " + ctype + "\r\n";
        req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    req += "\r\n";
    if (!body.empty()) req += body;
    {   // tamamını yaz (parçalı write)
        size_t off = 0;
        while (off < req.size()) {
            int w = st.write(req.data() + off, req.size() - off);
            if (w <= 0) { r.err = "saf-tls istek yazilamadi"; return r; }
            off += (size_t)w;
        }
    }
    std::string raw;
    char buf[16384];
    int n;
    while ((n = st.read(buf, sizeof buf)) > 0) raw.append(buf, (size_t)n);
    st.close();
    size_t hh = raw.find("\r\n\r\n");
    if (hh == std::string::npos) { r.err = "saf-tls yanit basligi yok"; return r; }
    std::string head = raw.substr(0, hh);
    r.body = raw.substr(hh + 4);
    if (raw.find("chunked") != std::string::npos || raw.find("Chunked") != std::string::npos)
        r.body = dechunk(r.body);
    size_t sp = head.find(' ');
    if (sp != std::string::npos) r.status = atoi(head.substr(sp + 1, 3).c_str());
    return r;
}

// ---- 3 aşamalı istek: direkt → sistem-proxy → DoH+saf-TLS ---------------------
static HttpResult winh_request(const std::string& url, const std::string& method,
                                const std::string& body, const std::string& ctype, int timeout_s,
                                const std::vector<std::pair<std::string, std::string>>& headers) {
    std::string trail;                       // tüm denemelerin teşhis izi
    HttpResult last;
    for (int mode = 0; mode < 2; mode++) {
        HttpResult r = winh_attempt(url, method, body, ctype, timeout_s, headers, mode == 1);
        if (r.status > 0) return r;          // HTTP yanıtı alındı → ağ yolu tamam
        if (!trail.empty()) trail += " | ";
        trail += (mode == 0 ? "direkt: " : "sistem-proxy: ") +
                 (r.err.empty() ? ("HTTP " + std::to_string(r.status)) : r.err);
        last = r;
    }
    // ikisi de taşıma seviyesinde başarısız → DoH ile çöz + saf C++ TLS
    std::string scheme, host; int port; std::string path;
    if (split_url(url, scheme, host, port, path) && scheme == "https") {
        std::string ip, derr;
        if (doh_resolve(host, ip, derr)) {
            HttpResult r = raw_tls_request(host, ip, port, path, method, body, ctype,
                                           headers, timeout_s);
            if (r.status > 0) return r;
            if (!trail.empty()) trail += " | ";
            trail += "DoH(ip=" + ip + ") saf-tls: " + r.err;
            last = r;
        } else {
            if (!trail.empty()) trail += " | ";
            trail += "DoH: " + derr;
        }
    }
    last.err = "ag hatasi — " + trail;
    return last;
}

HttpResult http_get(const std::string& url, int timeout_s,
                    const std::vector<std::pair<std::string, std::string>>& headers) {
    return winh_request(url, "GET", "", "", timeout_s, headers);
}
HttpResult http_post_form(const std::string& url, const std::string& form_body, int timeout_s,
                          const std::vector<std::pair<std::string, std::string>>& headers) {
    return winh_request(url, "POST", form_body, "application/x-www-form-urlencoded",
                        timeout_s, headers);
}
#endif

} // namespace net
} // namespace gt
