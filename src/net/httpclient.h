// ============================================================================
//  GOD_SSS_TIER_VPN — src/net/httpclient.h
//  Basit HTTP/HTTPS istemcisi.
//    Linux  : OpenSSL  (client.hola.org API çağrıları, proxy listeleri)
//    Windows: WinHTTP (bağımlılık yok — schannel üzerinden)
//  Amaç: Hola background_init / zgettunnels + ücretsiz proxy kaynak listeleri.
// ============================================================================
#pragma once
#include <string>
#include <vector>
#include <utility>

namespace gt {
namespace net {

struct HttpResult {
    int status = 0;             // 0 = bağlanılamadı
    std::string body;
    std::string err;
    bool ok() const { return status >= 200 && status < 300; }
};

// headers: isteğe özel başlık (User-Agent, Proxy-Authorization...)
HttpResult http_get(const std::string& url, int timeout_s,
                    const std::vector<std::pair<std::string, std::string>>& headers = {});

HttpResult http_post_form(const std::string& url,
                          const std::string& form_body,   // "login=1&ver=1.0" (url-encoded)
                          int timeout_s,
                          const std::vector<std::pair<std::string, std::string>>& headers = {});

// URL'den host:port:path ayrıştır
bool split_url(const std::string& url, std::string& scheme, std::string& host,
               int& port, std::string& pathrest);

} // namespace net
} // namespace gt
