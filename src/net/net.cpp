// ============================================================================
//  GOD_SSS_TIER_VPN — src/net/net.cpp
// ============================================================================
#include "net/net.h"
#include <cstring>
#include <cstdlib>
#include <chrono>

namespace gt {
namespace net {

void init() {
#ifdef _WIN32
    WSADATA w;
    WSAStartup(MAKEWORD(2, 2), &w);
#endif
}
void cleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}
void close_sock(sock_t s) {
    if (s == GT_BAD_SOCK) return;
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}
std::string sock_err_str() {
#ifdef _WIN32
    char buf[128];
    snprintf(buf, sizeof buf, "winsock hatasi %d", (int)WSAGetLastError());
    return buf;
#else
    return std::string(strerror(errno));
#endif
}

bool set_sock_timeout(sock_t s, int seconds) {
#ifdef _WIN32
    DWORD ms = (DWORD)(seconds * 1000);
    return setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&ms, sizeof ms) == 0 &&
           setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&ms, sizeof ms) == 0;
#else
    struct timeval tv;
    tv.tv_sec = seconds; tv.tv_usec = 0;
    return setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) == 0 &&
           setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv) == 0;
#endif
}

sock_t tcp_connect(const std::string& ip, int port, int timeout_s) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip.c_str(), &sa.sin_addr) != 1) {
        // IP değilse çözümle
        std::string err;
        std::string resolved = resolve_ipv4(ip, err);
        if (resolved.empty() || inet_pton(AF_INET, resolved.c_str(), &sa.sin_addr) != 1)
            return GT_BAD_SOCK;
    }
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == GT_BAD_SOCK) return GT_BAD_SOCK;
    // non-blocking connect
#ifdef _WIN32
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
#else
    int fl = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, fl | O_NONBLOCK);
#endif
    int rc = connect(s, (struct sockaddr*)&sa, sizeof sa);
    if (rc != 0) {
#ifdef _WIN32
        int e = WSAGetLastError();
        bool inprog = (e == WSAEWOULDBLOCK || e == WSAEINPROGRESS);
#else
        bool inprog = (errno == EINPROGRESS || errno == EWOULDBLOCK);
#endif
        if (!inprog) { close_sock(s); return GT_BAD_SOCK; }
        struct pollfd pfd;
        pfd.fd = s; pfd.events = POLLOUT; pfd.revents = 0;
        if (poll(&pfd, 1, timeout_s * 1000) != 1 || !(pfd.revents & POLLOUT)) {
            close_sock(s);
            return GT_BAD_SOCK;
        }
    }
    // blocking moda dön
#ifdef _WIN32
    u_long b0 = 0;
    ioctlsocket(s, FIONBIO, &b0);
#else
    int fl2 = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, fl2 & ~O_NONBLOCK);
#endif
    set_sock_timeout(s, 60);
    return s;
}

sock_t tcp_listen(int port, std::string& err) {
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == GT_BAD_SOCK) { err = "socket() hatasi"; return GT_BAD_SOCK; }
    int on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof on);
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // yalnız yerel makine
    sa.sin_port = htons((uint16_t)port);
    if (bind(s, (struct sockaddr*)&sa, sizeof sa) != 0) {
        err = "bind(" + std::to_string(port) + ") hatasi: " + sock_err_str();
        close_sock(s);
        return GT_BAD_SOCK;
    }
    if (listen(s, 16) != 0) {
        err = "listen() hatasi: " + sock_err_str();
        close_sock(s);
        return GT_BAD_SOCK;
    }
    return s;
}

std::string resolve_ipv4(const std::string& host, std::string& err) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) {
        err = "cozulemedi: " + host;
        return "";
    }
    char ipbuf[64];
    void* addr = &((struct sockaddr_in*)res->ai_addr)->sin_addr;
    inet_ntop(AF_INET, addr, ipbuf, sizeof ipbuf);
    std::string ip = ipbuf;
    freeaddrinfo(res);
    return ip;
}

int send_all(sock_t s, const void* data, size_t len) {
    const char* p = (const char*)data;
    size_t left = len;
    while (left > 0) {
        int n = (int)send(s, p, (int)left, 0);
        if (n <= 0) return -1;
        p += n; left -= (size_t)n;
    }
    return 0;
}
int recv_some(sock_t s, void* buf, size_t len) {
    return (int)recv(s, (char*)buf, (int)len, 0);
}

std::string read_response_head(sock_t s) {
    std::string head;
    char b[1];
    while (true) {
        int n = recv_some(s, b, 1);
        if (n <= 0) break;
        head += b[0];
        if (head.size() >= 4 && head.compare(head.size() - 4, 4, "\r\n\r\n") == 0) break;
        if (head.size() > 16384) break;
    }
    return head;
}

} // namespace net
} // namespace gt
