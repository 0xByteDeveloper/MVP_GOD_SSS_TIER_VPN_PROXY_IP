// ============================================================================
//  GOD_SSS_TIER_VPN — src/engine/monitor.h
//  DEBUG TERMİNALİ arka ucu: PID bazlı TCP bağlantı izleme — VPN KULLANMASAK
//  BİLE çalışır ("hangi pid hangi IP'ye/proxy'ye çıkıyor" sorusunun cevabı).
//    Linux  : /proc/net/tcp + inode->pid eşlemesi (/proc/*/fd taraması)
//    Windows: GetExtendedTcpTable (dwOwningPid)
//  Attach edilmiş PID'ler instance etiketiyle eşleştirilir.
// ============================================================================
#pragma once
#include <atomic>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "core/json.h"

namespace gt {

class InstanceManager;

struct TcpConnInfo {
    long pid = 0;
    std::string pname;
    std::string local;
    std::string remote;
    int rport = 0;
    std::string state;      // ESTABLISHED / SYN_SENT / ...
    std::string via;        // attach edilmişse: "inst#2 (US exit 185.x)"
};

class Monitor {
public:
    Monitor(InstanceManager* mgr, int poll_ms = 2000);
    ~Monitor();
    void start();
    void stop();

    // son tarama (tüm PC'nin bağlantıları)
    std::vector<TcpConnInfo> snapshot();
    // UI akışı: seq'dan sonraki olaylar (yeni bağlantı satırları)
    J events_json(size_t since, size_t max, size_t& total);
    size_t event_count();

private:
    void loop();
    std::vector<TcpConnInfo> scan();          // platform taraması
    void remember(const TcpConnInfo& c);       // yeni bağlantı -> olay
    void auto_capture();                       // OTO-YAKALA: desen eşleşen süreci kutuya ata

    InstanceManager* mgr_;
    int poll_ms_;
    std::atomic<bool> running_{false};
    std::thread th_;
    std::mutex mu_;
    std::vector<TcpConnInfo> last_;
    std::map<std::string, uint64_t> seen_;      // tuple -> son görülme
    std::map<long, uint64_t> known_pids_;      // oto-yakalama: görülmüş pid'ler
    std::deque<J> events_;                     // halka tampon
    size_t ev_seq_ = 0;
};

} // namespace gt
