// ============================================================================
//  GOD_SSS_TIER_VPN — src/net/tls.h
//  Ham TLS akışı (pimpl) — Hola trial proxy'sine bağlanan relay katmanı.
//  Linux  : OpenSSL   (test edilen ana yol)
//  Windows: SChannel  (bağımlılıksız exe için)
//  ÖNEMLİ: Hola WAF'ı SNI'li bazı parmak izlerini 407-suspicious ile reddiyor;
//  sni="" gönderilirse ClientHello'da SNI uzantısı YOK (freevpn.py hide_sni).
// ============================================================================
#pragma once
#include "net/net.h"
#include <string>

namespace gt {
namespace net {

class TlsStream {
public:
    TlsStream();
    ~TlsStream();
    TlsStream(const TlsStream&) = delete;
    TlsStream& operator=(const TlsStream&) = delete;

    // ip:port'e TCP + TLS. sni boşsa SNI gönderilmez; sertifika DOĞRULANMAZ
    // (Hola proxy sertifikası IP'ye değil host'a verilir — curl --proxy-insecure
    // ile aynı durum). ver: "" otomatik | "tls13" | "tls12" (şifreleme paneli).
    // Windows/SChannel: ver yalnız bilgi amaçlı (OS en iyi sürümü seçer).
    bool connect(const std::string& ip, int port, const std::string& sni,
                 int timeout_s, std::string& err, const std::string& ver = "");

    int  write(const void* p, size_t n);   // >=0: bayt sayısı; -1 hata
    int  read(void* buf, size_t n);        // >=0: bayt; 0: EOF; -1 hata
    bool has_pending() const;              // TLS tamponunda bekleyen veri
    sock_t fd() const;                     // alttaki soket (poll için)
    void close();

private:
    struct Impl;
    Impl* impl_;
};

} // namespace net
} // namespace gt
