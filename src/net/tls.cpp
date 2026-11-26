// ============================================================================
//  GOD_SSS_TIER_VPN — src/net/tls.cpp
// ============================================================================
#include "net/tls.h"
#include <cstring>
#include <vector>

// Windows başlıkları GLOBAL kapsamda (COM uuid şablon kısıtı)
#ifdef _WIN32
#  define SECURITY_WIN32 1               // sspi.h bunu ister
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <schannel.h>
#  include <security.h>
#  include <sspi.h>
#  pragma comment(lib, "secur32.lib")
#  pragma comment(lib, "crypt32.lib")
#endif

namespace gt {
namespace net {

#ifndef _WIN32
// =============================== OpenSSL (Linux / test edilen yol) =========
#include <openssl/ssl.h>
#include <openssl/err.h>

struct TlsStream::Impl {
    SSL_CTX* ctx = nullptr;
    SSL* ssl = nullptr;
    sock_t s = GT_BAD_SOCK;
    bool alive = false;
    ~Impl() {
        alive = false;
        if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); ssl = nullptr; }
        if (ctx) { SSL_CTX_free(ctx); ctx = nullptr; }
        if (s != GT_BAD_SOCK) { close_sock(s); s = GT_BAD_SOCK; }
    }
};

TlsStream::TlsStream() : impl_(new Impl) {}
TlsStream::~TlsStream() { close(); delete impl_; }

bool TlsStream::connect(const std::string& ip, int port, const std::string& sni,
                        int timeout_s, std::string& err, const std::string& ver) {
    impl_->s = tcp_connect(ip, port, timeout_s);
    if (impl_->s == GT_BAD_SOCK) { err = "proxy tcp baglanamadi: " + ip; return false; }
    impl_->ctx = SSL_CTX_new(TLS_client_method());
    if (!impl_->ctx) { err = "openssl ctx"; return false; }
    SSL_CTX_set_verify(impl_->ctx, SSL_VERIFY_NONE, nullptr);       // sertifikayı doğrulama
    // şifreleme paneli: TLS sürüm sabitleme (tls13/tls12)
    if (ver == "tls13") {
        SSL_CTX_set_min_proto_version(impl_->ctx, TLS1_3_VERSION);
        SSL_CTX_set_max_proto_version(impl_->ctx, TLS1_3_VERSION);
    } else if (ver == "tls12") {
        SSL_CTX_set_min_proto_version(impl_->ctx, TLS1_2_VERSION);
        SSL_CTX_set_max_proto_version(impl_->ctx, TLS1_2_VERSION);
    } else {
        SSL_CTX_set_min_proto_version(impl_->ctx, TLS1_2_VERSION);
    }
    // SNI yalnız istenirse (Hola WAF: SNI parmak izi bazlı 407 riski)
    SSL_CTX_set_options(impl_->ctx, SSL_OP_NO_COMPRESSION);
    // ALPN http/1.1 (CONNECT relay davranışı ile uyumlu)
    // "\x08http/1.1" = uzunluk-önekli protokol listesi (OpenSSL kabası)
    SSL_CTX_set_alpn_protos(impl_->ctx, (const unsigned char*)"\x08http/1.1", 9);
    impl_->ssl = SSL_new(impl_->ctx);
    SSL_set_fd(impl_->ssl, (int)impl_->s);
    if (!sni.empty()) SSL_set_tlsext_host_name(impl_->ssl, sni.c_str());
    if (SSL_connect(impl_->ssl) != 1) {
        unsigned long e = ERR_get_error();
        err = "proxy tls el sikismasi hatasi: " +
              (e ? std::string(ERR_error_string(e, nullptr)) : std::string("zaman asimi/red"));
        return false;
    }
    impl_->alive = true;
    return true;
}

int TlsStream::write(const void* p, size_t n) {
    if (!impl_->alive) return -1;
    const char* c = (const char*)p; size_t off = 0;
    while (off < n) {
        int w = SSL_write(impl_->ssl, c + off, (int)(n - off));
        if (w <= 0) return -1;
        off += (size_t)w;
    }
    return (int)n;
}
int TlsStream::read(void* buf, size_t n) {
    if (!impl_->alive) return -1;
    int r = SSL_read(impl_->ssl, buf, (int)n);
    if (r < 0) {
        int e = SSL_get_error(impl_->ssl, r);
        if (e == SSL_ERROR_ZERO_RETURN) return 0;
        return -1;
    }
    return r;
}
bool TlsStream::has_pending() const {
    return impl_->alive && SSL_pending(impl_->ssl) > 0;
}
sock_t TlsStream::fd() const { return impl_->s; }
void TlsStream::close() {
    // Impl yıkıcısı çağrılınca kaynağı tekrar salacak; burada yalnız sırayla kapat
    if (impl_) {
        if (impl_->alive && impl_->ssl) SSL_shutdown(impl_->ssl);
        if (impl_->ssl) { SSL_free(impl_->ssl); impl_->ssl = nullptr; }
        if (impl_->ctx) { SSL_CTX_free(impl_->ctx); impl_->ctx = nullptr; }
        if (impl_->s != GT_BAD_SOCK) { close_sock(impl_->s); impl_->s = GT_BAD_SOCK; }
        impl_->alive = false;
    }
}

#else
// =============================== SChannel (Windows) =========================
// (başlıklar dosya başında global kapsamda include edildi)

struct TlsStream::Impl {
    CredHandle cred{};
    CtxtHandle ctx{};
    bool haveCtx = false, haveCred = false, alive = false, handshakeDone = false;
    sock_t s = GT_BAD_SOCK;
    SecPkgContext_StreamSizes sizes{};
    std::string inBuf;      // henüz şifre çözülmemiş ham TLS kayıtları
    std::string plainLeft;  // çözülmüş ama okuyıcıya henüz verilmemiş düz metin
    ~Impl() {
        alive = false;
        if (haveCtx) { DeleteSecurityContext(&ctx); haveCtx = false; }
        if (haveCred) { FreeCredentialsHandle(&cred); haveCred = false; }
        if (s != GT_BAD_SOCK) { close_sock(s); s = GT_BAD_SOCK; }
    }
};

TlsStream::TlsStream() : impl_(new Impl) {}
TlsStream::~TlsStream() { close(); delete impl_; }

static bool sendAll(sock_t s, const void* p, size_t n) {
    const char* c = (const char*)p; size_t off = 0;
    while (off < n) {
        int w = send(s, c + off, (int)(n - off), 0);
        if (w <= 0) return false;
        off += (size_t)w;
    }
    return true;
}

bool TlsStream::connect(const std::string& ip, int port, const std::string& sni,
                        int timeout_s, std::string& err, const std::string& ver) {
    // ver: SChannel yolunda OS varsayılanı kullanılır (TLS 1.2/1.3 otomatik
    // pazarlık) — sürüm sabitleme OpenSSL yolunda uygulanır
    (void)ver;
    impl_->s = tcp_connect(ip, port, timeout_s);
    if (impl_->s == GT_BAD_SOCK) { err = "proxy tcp baglanamadi"; return false; }

    // 1) kimlik bilgileri (schannel, doğrulama KAPALI — Hola proxy sertifikası
    //    IP bağlantısında eşleşmez: curl --proxy-insecure ile eşdeğer)
    SCHANNEL_CRED sc{};
    sc.dwVersion = SCHANNEL_CRED_VERSION;
    sc.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS |
                 SCH_CRED_IGNORE_NO_REVOCATION_CHECK;
    SECURITY_STATUS st = AcquireCredentialsHandleW(nullptr, (SEC_WCHAR*)UNISP_NAME_W,
                              SECPKG_CRED_OUTBOUND, nullptr, &sc, nullptr, nullptr,
                              &impl_->cred, nullptr);
    if (st != SEC_E_OK) { err = "AcquireCredentialsHandle hatasi"; return false; }
    impl_->haveCred = true;

    // 2) el sıkışma döngüsü (InitializeSecurityContext + token alışverişi)
    std::string target = sni.empty() ? ip : sni;    // SChannel SNI'yi hedef addan türetir
    SecBufferDesc inDesc, outDesc;
    SecBuffer inBuf[2], outBuf[1];
    DWORD ctxReq = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT |
                   ISC_REQ_CONFIDENTIALITY | ISC_RET_EXTENDED_ERROR |
                   ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;
    DWORD attr = 0;
    bool first = true;
    std::string token;
    while (true) {
        memset(outBuf, 0, sizeof outBuf);
        outBuf[0].BufferType = SECBUFFER_TOKEN;
        outBuf[0].pvBuffer = nullptr; outBuf[0].cbBuffer = 0;
        outDesc.ulVersion = SECBUFFER_VERSION;
        outDesc.cBuffers = 1;
        outDesc.pBuffers = outBuf;

        if (first) {
            memset(&inDesc, 0, sizeof inDesc);      // ilk çağrı: girdi yok
            st = InitializeSecurityContextW(&impl_->cred, nullptr,
                    (SEC_WCHAR*)std::wstring(target.begin(), target.end()).c_str(),
                    ctxReq, 0, SECURITY_NETWORK_DREP, &inDesc,
                    0, &impl_->ctx, &outDesc, &attr, nullptr);
            first = false;
        } else {
            inBuf[0].BufferType = SECBUFFER_TOKEN;
            inBuf[0].pvBuffer = (void*)token.data();
            inBuf[0].cbBuffer = (ULONG)token.size();
            inBuf[1].BufferType = SECBUFFER_EMPTY;
            inDesc.ulVersion = SECBUFFER_VERSION;
            inDesc.cBuffers = 2;
            inDesc.pBuffers = inBuf;
            st = InitializeSecurityContextW(&impl_->cred, &impl_->ctx,
                    (SEC_WCHAR*)std::wstring(target.begin(), target.end()).c_str(),
                    ctxReq, 0, SECURITY_NETWORK_DREP, &inDesc,
                    0, &impl_->ctx, &outDesc, &attr, nullptr);
            token.clear();
        }
        impl_->haveCtx = true;
        if (outBuf[0].pvBuffer && outBuf[0].cbBuffer > 0) {
            if (!sendAll(impl_->s, outBuf[0].pvBuffer, outBuf[0].cbBuffer)) {
                FreeContextBuffer(outBuf[0].pvBuffer);
                err = "token gonderilemedi";
                return false;
            }
            FreeContextBuffer(outBuf[0].pvBuffer);
        }
        if (st == SEC_E_OK) break;
        if (st != SEC_I_CONTINUE_NEEDED) {
            err = "tls el sikismasi hatasi (schannel: 0x" +
                  std::to_string((unsigned long)st) + ")";
            return false;
        }
        // sunucu token'ını oku (tek segment okuma — MSDN örneği aynı şekilde)
        char b[16384];
        int n = recv_some(impl_->s, b, sizeof b);
        if (n <= 0) { err = "el sikismasinda karsi taraf kapandi"; return false; }
        token.assign(b, (size_t)n);
    }

    // 3) akım boyutları (header/trailer)
    if (QueryContextAttributes(&impl_->ctx, SECPKG_ATTR_STREAM_SIZES, &impl_->sizes) != SEC_E_OK) {
        err = "StreamSizes alinamadi"; return false;
    }
    impl_->alive = true;
    impl_->handshakeDone = true;
    return true;
}

int TlsStream::write(const void* p, size_t n) {
    if (!impl_->alive) return -1;
    // şifrele: header + veri + trailer tek SECBUFFER_STREAM
    size_t total = impl_->sizes.cbHeader + n + impl_->sizes.cbTrailer;
    std::vector<char> enc(total);
    SecBuffer bufs[4];
    bufs[0].BufferType = SECBUFFER_STREAM_HEADER;
    bufs[0].pvBuffer = enc.data(); bufs[0].cbBuffer = impl_->sizes.cbHeader;
    bufs[1].BufferType = SECBUFFER_DATA;
    bufs[1].pvBuffer = enc.data() + impl_->sizes.cbHeader;
    bufs[1].cbBuffer = (ULONG)n;
    memcpy(bufs[1].pvBuffer, p, n);
    bufs[2].BufferType = SECBUFFER_STREAM_TRAILER;
    bufs[2].pvBuffer = enc.data() + impl_->sizes.cbHeader + n;
    bufs[2].cbBuffer = impl_->sizes.cbTrailer;
    bufs[3].BufferType = SECBUFFER_EMPTY; bufs[3].pvBuffer = nullptr; bufs[3].cbBuffer = 0;
    SecBufferDesc desc{ SECBUFFER_VERSION, 4, bufs };
    SECURITY_STATUS st = EncryptMessage(&impl_->ctx, 0, &desc, 0);
    if (st != SEC_E_OK) return -1;
    if (!sendAll(impl_->s, enc.data(), bufs[0].cbBuffer + bufs[1].cbBuffer + bufs[2].cbBuffer))
        return -1;
    return (int)n;
}

int TlsStream::read(void* buf, size_t n) {
    if (!impl_->alive) return -1;
    // önceki çözülmemiş düz metin tamponu
    if (!impl_->plainLeft.empty()) {
        size_t c = impl_->plainLeft.size() < n ? impl_->plainLeft.size() : n;
        memcpy(buf, impl_->plainLeft.data(), c);
        impl_->plainLeft = impl_->plainLeft.substr(c);
        return (int)c;
    }
    while (true) {
        if (!impl_->inBuf.empty()) {
            SecBuffer bufs[4];
            bufs[0].BufferType = SECBUFFER_DATA;
            bufs[0].pvBuffer = (void*)impl_->inBuf.data();
            bufs[0].cbBuffer = (ULONG)impl_->inBuf.size();
            for (int k = 1; k < 4; k++) { bufs[k].BufferType = SECBUFFER_EMPTY; bufs[k].pvBuffer = nullptr; bufs[k].cbBuffer = 0; }
            SecBufferDesc desc{ SECBUFFER_VERSION, 4, bufs };
            SECURITY_STATUS st = DecryptMessage(&impl_->ctx, &desc, 0, nullptr);
            if (st == SEC_E_OK) {
                std::string plain;
                for (auto& b : bufs)
                    if (b.BufferType == SECBUFFER_DATA) plain.append((char*)b.pvBuffer, b.cbBuffer);
                // EXTRA: bir sonraki kaydın şifreli başı — inBuf başında tutulur
                size_t consumed = impl_->inBuf.size();
                for (auto& b : bufs)
                    if (b.BufferType == SECBUFFER_EXTRA) consumed = (size_t)((char*)b.pvBuffer - impl_->inBuf.data());
                impl_->inBuf = impl_->inBuf.substr(consumed);
                if (plain.empty()) continue;         // yalnız uyarı/handshake kaydı — devam
                size_t c = plain.size() < n ? plain.size() : n;
                memcpy(buf, plain.data(), c);
                if (c < plain.size()) impl_->plainLeft = plain.substr(c);
                return (int)c;
            }
            if (st == SEC_E_INCOMPLETE_MESSAGE) { /* kayıt eksik — aşağıda okumaya devam */ }
            else { impl_->alive = false; return -1; }
        }
        char b[16384];
        int r = recv_some(impl_->s, b, sizeof b);
        if (r <= 0) { impl_->alive = false; return 0; }
        impl_->inBuf.append(b, (size_t)r);
    }
}

bool TlsStream::has_pending() const {
    return impl_->alive && (!impl_->inBuf.empty() || !impl_->plainLeft.empty());
}
sock_t TlsStream::fd() const { return impl_->s; }
void TlsStream::close() {
    if (impl_) {
        if (impl_->haveCtx) { DeleteSecurityContext(&impl_->ctx); impl_->haveCtx = false; }
        if (impl_->haveCred) { FreeCredentialsHandle(&impl_->cred); impl_->haveCred = false; }
        if (impl_->s != GT_BAD_SOCK) { close_sock(impl_->s); impl_->s = GT_BAD_SOCK; }
        impl_->alive = false;
    }
}
#endif

} // namespace net
} // namespace gt
