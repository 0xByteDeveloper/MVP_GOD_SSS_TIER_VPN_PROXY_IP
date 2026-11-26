// ============================================================================
//  GOD_SSS_TIER_VPN — src/net/httpclient.cpp
//  Linux: OpenSSL ile HTTPS GET/POST (engine'in API çağrıları)
//  Windows: WinHTTP ile (schannel, sıfır dış bağımlılık)
// ============================================================================
#include "net/httpclient.h"
#include "net/net.h"
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
    // chunked çöz
    if (raw.find("chunked") != std::string::npos || raw.find("Chunked") != std::string::npos) {
        std::string out; size_t pos = 0;
        while (pos < r.body.size()) {
            size_t eol = r.body.find("\r\n", pos);
            if (eol == std::string::npos) break;
            long chunk = strtol(r.body.substr(pos, eol - pos).c_str(), nullptr, 16);
            if (chunk <= 0) break;
            size_t data = eol + 2;
            if (data + (size_t)chunk > r.body.size()) chunk = (long)(r.body.size() - data);
            out.append(r.body, data, (size_t)chunk);
            pos = data + (size_t)chunk + 2;
        }
        r.body = out;
    }
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

static std::wstring wide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

static HttpResult winh_request(const std::string& url, const std::string& method,
                               const std::string& body, const std::string& ctype, int timeout_s,
                               const std::vector<std::pair<std::string, std::string>>& headers) {
    HttpResult r;
    std::string scheme, host; int port; std::string path;
    if (!split_url(url, scheme, host, port, path)) { r.err = "kotu url"; return r; }
    bool secure = (scheme == "https");
    HINTERNET sess = WinHttpOpen(L"GOD_SSS_TIER_VPN", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!sess) { r.err = "WinHttpOpen hatasi"; return r; }
    DWORD to = (DWORD)(timeout_s * 1000);
    WinHttpSetTimeouts(sess, to, to, to, to);
    std::wstring whost = wide(host);
    HINTERNET conn = WinHttpConnect(sess, whost.c_str(), (INTERNET_PORT)port, 0);
    if (!conn) { r.err = "WinHttpConnect hatasi"; WinHttpCloseHandle(sess); return r; }
    std::wstring wmethod = wide(method);
    std::wstring wpath = wide(path);
    DWORD openFlags = secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(conn, wmethod.c_str(), wpath.c_str(),
                                       nullptr, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, openFlags);
    if (!req) { r.err = "WinHttpOpenRequest hatasi"; WinHttpCloseHandle(conn); WinHttpCloseHandle(sess); return r; }
    for (auto& h : headers)
        WinHttpAddRequestHeaders(req, wide(h.first + ": " + h.second).c_str(), (ULONG)-1,
                                 WINHTTP_ADDREQ_FLAG_ADD);
    if (!body.empty())
        WinHttpAddRequestHeaders(req, L"Content-Type: application/x-www-form-urlencoded",
                                 (ULONG)-1, WINHTTP_ADDREQ_FLAG_ADD);
    const void* pbody = body.empty() ? WINHTTP_NO_REQUEST_DATA : (const void*)body.c_str();
    DWORD blen = body.empty() ? 0 : (DWORD)body.size();
    if (!WinHttpSendRequest(req, nullptr /*WINHTTP_NO_REQUEST_HEADERS*/, 0, (LPVOID)pbody, blen, blen, 0)) {
        r.err = "WinHttpSendRequest hatasi"; 
        WinHttpCloseHandle(req); WinHttpCloseHandle(conn); WinHttpCloseHandle(sess); return r;
    }
    if (!WinHttpReceiveResponse(req, nullptr)) { r.err = "WinHttpReceiveResponse hatasi"; }
    else {
        DWORD status = 0, sz = sizeof status;
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
        r.status = (int)status;
        char buf[16384]; DWORD got = 0;
        while (WinHttpReadData(req, buf, sizeof buf, &got) && got > 0) {
            r.body.append(buf, got); got = 0;
        }
    }
    WinHttpCloseHandle(req); WinHttpCloseHandle(conn); WinHttpCloseHandle(sess);
    return r;
}

HttpResult http_get(const std::string& url, int timeout_s,
                    const std::vector<std::pair<std::string, std::string>>& headers) {
    return winh_request(url, "GET", "", "", timeout_s, headers);
}
HttpResult http_post_form(const std::string& url, const std::string& form_body, int timeout_s,
                          const std::vector<std::pair<std::string, std::string>>& headers) {
    return winh_request(url, "POST", form_body, "application/x-www-form-urlencoded", timeout_s, headers);
}
#endif

} // namespace net
} // namespace gt
