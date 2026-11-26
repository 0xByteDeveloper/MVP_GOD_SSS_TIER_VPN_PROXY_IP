// ============================================================================
//  GOD_SSS_TIER_VPN — src/net/net.h
//  Platform soyut soket katmanı (Linux: POSIX, Windows: WinSock)
//  + TCP bağlan (timeout'lu), dinle, relaya hazır send/recv yardımcıları
// ============================================================================
#pragma once
#include <cstdint>
#include <string>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   typedef SOCKET sock_t;
#  define GT_BAD_SOCK INVALID_SOCKET
#  define poll WSAPoll          // POSIX poll() yok — WSAPoll aynı imza (Vista+)
#else
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <poll.h>
   typedef int sock_t;
#  define GT_BAD_SOCK (-1)
#endif

namespace gt {
namespace net {

void init();                    // WinSock WSAStartup (Linux'ta no-op)
void cleanup();
void close_sock(sock_t s);
bool set_sock_timeout(sock_t s, int seconds);       // SO_RCVTIMEO/SO_SNDTIMEO
std::string sock_err_str();

// TCP istemci: non-blocking connect + poll; başarısızsa GT_BAD_SOCK
sock_t tcp_connect(const std::string& ip, int port, int timeout_s);

// Sunucu dinleme soketi (127.0.0.1)
sock_t tcp_listen(int port, std::string& err);

// Ad çözümleme (IPv4 öncelikli)
std::string resolve_ipv4(const std::string& host, std::string& err);

// blocking I/O
int send_all(sock_t s, const void* data, size_t len);
int recv_some(sock_t s, void* buf, size_t len);

// HTTP yanıt başlığı okuma: "\r\n\r\n" görene dek satır satır oku
// (CONNECT yanıtları gövdesiz olduğundan satır satır okuma ZORUNLU —
//  gövdeyle karışıp asılı kalmamak için; freevpn.py'deki _read_response_head
//  ile aynı mantık)
std::string read_response_head(sock_t s);

} // namespace net
} // namespace gt
