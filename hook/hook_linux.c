// ============================================================================
//  GOD_SSS_TIER_VPN — hook/hook_linux.c
//  LD_PRELOAD connect() kancası: hedef programın TÜM TCP bağlantılarını
//  seçilen instance'ın yerel SOCKS5 router'ına (127.0.0.1:22000+) yönlendir.
//  "Proxifier'in gelişmişi" — sürecin kendi ağ yığını yok, socket'i
//  instance'a bağlanıp SOCKS5 CONNECT ile aynı hedefe açar; kalan akış
//  uygulamaya şeffaf (TLS uygulamanın kendisinde — parmak izi uygulamanın).
//
//  Kullanım (Linux test yolu — engine launch komutu bunu otomatik yapar):
//    GODTIER_SOCKS=127.0.0.1:22001 LD_PRELOAD=$PWD/hook/libgodtier_hook.so \
//        curl http://ip-api.com/json
//  (port 80 hedefleri router'ın absolute-form çevirisi üzerinden;
//   443 hedefleri CONNECT relay üzerinden geçer)
// ============================================================================
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

static int (*real_connect)(int, const struct sockaddr*, socklen_t);
static pthread_once_t once = PTHREAD_ONCE_INIT;
static struct sockaddr_in g_target;     // instance SOCKS5 adresi
static int g_ready = 0;

static void init_hook(void) {
    real_connect = dlsym(RTLD_NEXT, "connect");
    const char* env = getenv("GODTIER_SOCKS");
    if (!env || !*env) return;         // tanımsız: yönlendirme yok (yalnız izleme)
    char ip[64] = {0};
    int port = 0;
    if (sscanf(env, "%63[^:]:%d", ip, &port) != 2 || port <= 0) return;
    memset(&g_target, 0, sizeof g_target);
    g_target.sin_family = AF_INET;
    g_target.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, ip, &g_target.sin_addr) != 1) return;
    g_ready = 1;
}

// SOCKS5 el sıkışması: 5,1,0 | 5,0 | CONNECT | yanıt
static int socks5_connect(const struct sockaddr_in* dst) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    // gerçek connect (döngüye girmesin diye doğrudan syscall)
    if (real_connect(s, (const struct sockaddr*)&g_target, sizeof g_target) != 0) {
        close(s);
        return -1;
    }
    unsigned char greeting[3] = {5, 1, 0};
    if (write(s, greeting, 3) != 3) { close(s); return -1; }
    unsigned char resp[2];
    if (read(s, resp, 2) != 2 || resp[0] != 5 || resp[1] != 0) { close(s); return -1; }
    // yalnız IPv4 (MVP); IPv6 hedefler yerel gider
    unsigned char req[10] = {5, 1, 0, 1};
    memcpy(req + 4, &dst->sin_addr, 4);
    memcpy(req + 8, &dst->sin_port, 2);
    if (write(s, req, 10) != 10) { close(s); return -1; }
    unsigned char rep[10];
    // yanıt başlığı 4 + addr(4) + port(2) = 10 bayt (ATYP=1)
    if (read(s, rep, 10) != 10 || rep[1] != 0) { close(s); return -1; }
    return s;   // aynı fd'yi döndürüyoruz — uygulama farketmeden devam eder
}

int connect(int fd, const struct sockaddr* addr, socklen_t len) {
    pthread_once(&once, init_hook);
    if (!g_ready || !real_connect)
        return real_connect(fd, addr, len);
    if (addr->sa_family != AF_INET) {
        // IPv6/Unix: yönlendirme yok
        return real_connect(fd, addr, len);
    }
    const struct sockaddr_in* dst = (const struct sockaddr_in*)addr;
    unsigned int ip = ntohl(dst->sin_addr.s_addr);
    // 127.0.0.0/8 ve instance portlarına gidenler yerel kalsın
    if ((ip >> 24) == 127) return real_connect(fd, addr, len);
    if (ntohs(dst->sin_port) == 0) return real_connect(fd, addr, len);

    // SOCKS5 tüneliyle aynı hedefe aç, sonra fd'yle takas et
    int tun = socks5_connect(dst);
    if (tun < 0) {
        errno = ECONNREFUSED;
        return -1;
    }
    // tun fd'sini uygulamaya fd olarak geri ver: ikisini yer değiştir
    if (tun != fd) {
        if (dup2(tun, fd) < 0) { close(tun); errno = EBADF; return -1; }
        close(tun);       // fd artık tuneli gösteriyor
    }
    return 0;
}
