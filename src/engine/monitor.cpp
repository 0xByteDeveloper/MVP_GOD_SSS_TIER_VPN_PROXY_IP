// ============================================================================
//  GOD_SSS_TIER_VPN — src/engine/monitor.cpp
// ============================================================================
#include "engine/monitor.h"
#include "engine/instance.h"
#include "engine/proclist.h"
#include "core/log.h"
#include "core/util.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// platform başlıkları GLOBAL kapsamda olmalı (COM uuid şablonları namespace
// içinde özelleşemez — mingw-w64 __CRT_UUID_DECL kısıtı)
#ifndef _WIN32
#  include <dirent.h>
#  include <sys/stat.h>
#else
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <iphlpapi.h>
#  pragma comment(lib, "iphlpapi.lib")
#  pragma comment(lib, "ws2_32.lib")
#endif

namespace gt {

Monitor::Monitor(InstanceManager* mgr, int poll_ms)
    : mgr_(mgr), poll_ms_(poll_ms) {}
Monitor::~Monitor() { stop(); }

void Monitor::start() {
    if (running_.exchange(true)) return;
    th_ = std::thread([this] { loop(); });
}
void Monitor::stop() {
    if (!running_.exchange(false)) return;
    if (th_.joinable()) th_.join();
}

// ---- tarama (platform) --------------------------------------------------------
#ifndef _WIN32
static std::map<unsigned long, std::pair<long, std::string>> build_inode_map() {
    // inode -> (pid, process name)
    std::map<unsigned long, std::pair<long, std::string>> out;
    DIR* d = opendir("/proc");
    if (!d) return out;
    struct dirent* de;
    while ((de = readdir(d)) != nullptr) {
        if (de->d_name[0] < '0' || de->d_name[0] > '9') continue;
        long pid = atol(de->d_name);
        char fdp[64];
        snprintf(fdp, sizeof fdp, "/proc/%ld/fd", pid);
        DIR* fd = opendir(fdp);
        if (!fd) continue;
        // isim (stat'ten)
        std::string name;
        char sp[64];
        snprintf(sp, sizeof sp, "/proc/%ld/stat", pid);
        FILE* f = fopen(sp, "r");
        if (f) {
            char line[512];
            if (fgets(line, sizeof line, f)) {
                char* p1 = strchr(line, '(');
                char* p2 = strrchr(line, ')');
                if (p1 && p2 && p2 > p1) {
                    size_t n = (size_t)(p2 - p1 - 1);
                    char nm[64];
                    if (n > sizeof nm - 1) n = sizeof nm - 1;
                    memcpy(nm, p1 + 1, n);
                    nm[n] = 0;
                    name = nm;
                }
            }
            fclose(f);
        }
        struct dirent* fde;
        while ((fde = readdir(fd)) != nullptr) {
            if (fde->d_name[0] == '.') continue;
            char linkp[128];
            snprintf(linkp, sizeof linkp, "/proc/%ld/fd/%s", pid, fde->d_name);
            char target[256];
            ssize_t n = readlink(linkp, target, sizeof target - 1);
            if (n <= 0) continue;
            target[n] = 0;
            if (strncmp(target, "socket:[", 8) == 0) {
                unsigned long ino = strtoul(target + 8, nullptr, 10);
                out[ino] = {pid, name};
            }
        }
        closedir(fd);
    }
    closedir(d);
    return out;
}

static std::vector<TcpConnInfo> scan_linux() {
    std::vector<TcpConnInfo> out;
    auto inodes = build_inode_map();
    const char* files[] = {"/proc/net/tcp", "/proc/net/tcp6"};
    for (auto* path : files) {
        FILE* f = fopen(path, "r");
        if (!f) continue;
        char line[512];
        fgets(line, sizeof line, f);                      // başlık
        while (fgets(line, sizeof line, f)) {
            // sl local rem st ... inode
            char lp[64], rp[64];
            unsigned long sl = 0, inode = 0;
            int st = 0;
            if (sscanf(line, "%lu: %63s %63s %X %*s %*s %*s %*s %lu %*s %*s %*s",
                       &sl, lp, rp, &st, &inode) < 5)
                continue;
            if (st != 1) continue;                         // yalnız ESTABLISHED
            // hex (little-endian) IPv4 → noktalı; tcp6 hex16 uzun — olduğu gibi kısalt
            auto hexip4 = [](const std::string& h) -> std::string {
                if (h.size() != 8) return "[v6]" + h.substr(0, 8);
                unsigned v = (unsigned)strtoul(h.c_str(), nullptr, 16);
                char b[32];
                snprintf(b, sizeof b, "%u.%u.%u.%u", (v & 0xFF), (v >> 8) & 0xFF,
                         (v >> 16) & 0xFF, (v >> 24) & 0xFF);
                return b;
            };
            auto split_addr = [&](const char* a, std::string& ip, int& port) {
                const char* colon = strrchr(a, ':');
                if (!colon) return;
                ip = hexip4(std::string(a, (size_t)(colon - a)));
                port = (int)strtol(colon + 1, nullptr, 16);
            };
            TcpConnInfo c;
            split_addr(lp, c.local, c.rport);
            split_addr(rp, c.remote, c.rport);
            if (c.remote.rfind("[v6]", 0) == 0 || c.local.rfind("[v6]", 0) == 0) continue;
            auto it = inodes.find(inode);
            if (it != inodes.end()) {
                c.pid = it->second.first;
                c.pname = it->second.second;
            }
            c.state = "ESTABLISHED";
            out.push_back(c);
        }
        fclose(f);
    }
    return out;
}

std::vector<TcpConnInfo> Monitor::scan() { return scan_linux(); }

#else // ---- Windows: GetExtendedTcpTable ----
// (başlıklar dosya başında global kapsamda include edildi)

std::vector<TcpConnInfo> Monitor::scan() {
    std::vector<TcpConnInfo> out;
    // TCP_TABLE_OWNER_PID: her satırın sahibi pid ile gelir (dwOwningPid)
    ULONG sz = 0;
    GetExtendedTcpTable(nullptr, &sz, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    std::vector<char> buf(sz ? sz : 1);
    if (GetExtendedTcpTable(buf.data(), &sz, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0)
            != NO_ERROR) return out;
    PMIB_TCPTABLE_OWNER_PID t = (PMIB_TCPTABLE_OWNER_PID)buf.data();
    for (DWORD i = 0; i < t->dwNumEntries; i++) {
        MIB_TCPROW_OWNER_PID& r = t->table[i];
        if (r.dwState != MIB_TCP_STATE_ESTAB) continue;
        TcpConnInfo c;
        in_addr la, ra;
        la.S_un.S_addr = r.dwLocalAddr;
        ra.S_un.S_addr = r.dwRemoteAddr;
        c.local = std::string(inet_ntoa(la)) + ":" +
                  std::to_string(ntohs((u_short)r.dwLocalPort));
        c.remote = std::string(inet_ntoa(ra));
        c.rport = ntohs((u_short)r.dwRemotePort);
        c.pid = (long)r.dwOwningPid;
        c.state = "ESTABLISHED";
        out.push_back(c);
    }
    return out;
}
#endif

// ---- döngü + olaylar ----------------------------------------------------------
void Monitor::loop() {
    while (running_.load()) {
        auto conns = scan();
        std::unique_lock<std::mutex> g(mu_);
        last_ = conns;
        for (auto& c : conns) {
            std::string key = std::to_string(c.pid) + "|" + c.remote + ":" +
                              std::to_string(c.rport);
            uint64_t now = now_ms();
            auto it = seen_.find(key);
            if (it == seen_.end() || now - it->second > 60000) {
                // yeni (veya 1 dk içinde yeniden görülen) bağlantı -> olay
                // attach eşleşmesi
                c.via = "";
                for (auto* inst : mgr_->list()) {
                    for (long p : inst->attached_pids) {
                        if (p == c.pid) {
                            c.via = "inst#" + std::to_string(inst->id) + " " + inst->label +
                                    " (" + inst->country + ")";
                        }
                    }
                }
                remember(c);
                seen_[key] = now;
            }
        }
        // seen_ büyümesini sınırla
        if (seen_.size() > 4096) {
            for (auto it = seen_.begin(); it != seen_.end();) {
                if (now_ms() - it->second > 120000) it = seen_.erase(it);
                else ++it;
            }
        }
        g.unlock();                                // oto-yakalama KİLİTSİZ çalışsın
        // OTO-YAKALA: kutuların desenleri (metin2client.exe vb.) yeni açılan
        // süreçleri otomatik kutulara bağlar (doluysa sıradaki eşleşen kutuya)
        auto_capture();
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms_));
    }
}

// ---- OTO-YAKALA (MOD 1 / MOD 2) ------------------------------------------------
// MOD 1 (capture_mode=0) — İLK N: desen eşleşen YENİ açılan süreç kutuya
//   bağlanır (kapasiteye kadar); kutu doluysa sıradaki eşleşen kutu alır
//   (kullanıcının metin2 senaryosu: 1. metin2client → inst1, 2. → inst3,
//   inst1 doluysa ilk açılan inst3'e bağlanır).
// MOD 2 (capture_mode=1) — HEPSİ: yazılan adı taşıyan TÜM süreçler bu kutuya
//   bağlanır: brave.exe → tüm brave'ler, chrome → tüm chromelar; ŞU AN AÇIK
//   OLANLAR BİLE anında yakalanır (her tarama mevcut süreçleri de tarar).
static bool capture_match(const std::string& capture, const std::string& nm,
                          const std::string& cm) {
    for (auto& pat : split(capture, ',')) {
        std::string pt = lower(trim(pat));
        if (pt.empty()) continue;
        if (nm == pt || cm == pt ||
            (pt.size() > 15 && nm == pt.substr(0, 15))) return true;   // /proc 15 kr. kısaltma
    }
    return false;
}
static bool pid_in_any_box(const std::vector<Instance*>& insts, long pid) {
    for (auto* in2 : insts) {
        std::lock_guard<std::mutex> g2(in2->mu);
        for (long ap : in2->attached_pids)
            if (ap == pid) return true;
    }
    return false;
}
void Monitor::auto_capture() {
    auto insts = mgr_->list();                 // id artan sıra
    bool any = false;
    for (auto* in : insts) if (!in->capture.empty()) { any = true; break; }
    if (!any) return;

    auto procs = list_processes();
    uint64_t now = now_ms();

    // ---- MOD 2 — HEPSİ: (önce çalışır — açıkta olanlar anında yakalanır) ----
    for (auto* in : insts) {
        if (in->capture.empty() || in->capture_mode != 1) continue;
        for (auto& p : procs) {
            if (p.pid <= 4) continue;                    // çekirdek iş parçacıkları
            std::string nm = lower(p.name);
            std::string cm;
            if (!p.cmdline.empty()) {
                std::string first = p.cmdline.substr(0, p.cmdline.find(' '));
                size_t sl = first.rfind('/');
                if (sl != std::string::npos) first = first.substr(sl + 1);
                cm = lower(first);
            }
            if (!capture_match(in->capture, nm, cm)) continue;
            {   // bu kutuda zaten mi? (yeniden ekleme yapma)
                std::lock_guard<std::mutex> g2(in->mu);
                bool mine = false;
                for (long ap : in->attached_pids) if (ap == p.pid) { mine = true; break; }
                if (mine) continue;
            }
            if (pid_in_any_box(insts, p.pid)) continue;  // başka kutudaysa dokunma
            std::string aerr;
            mgr_->attach(in->id, p.pid, p.name, aerr);
            {
                std::lock_guard<std::mutex> g2(mu_);
                J e = J::makeObj();
                e["ts"] = now_iso();
                e["type"] = "AUTO_ASSIGN";
                e["pid"] = p.pid;
                e["pname"] = p.name;
                e["remote"] = "oto-yakala MOD2 (hepsi)";
                e["via"] = "inst#" + std::to_string(in->id) + " " + in->label +
                            " (" + in->provider + "/" + in->country + ")";
                events_.push_back(e);
                ev_seq_++;
                if (events_.size() > 2000)
                    events_.erase(events_.begin(), events_.begin() + 1000);
            }
            gt::LogLine(LogColor::Magenta,
                "[oto-yakala MOD2] %s (pid %ld) -> inst#%d %s (%s) — hepsi modu",
                p.name.c_str(), p.pid, in->id, in->label.c_str(), in->provider.c_str());
        }
    }

    // ---- MOD 1 — İLK N: yeni açılan süreçler (kapasite + sıradaki kutuya geçiş) ----
    for (auto& p : procs) {
        if (known_pids_.count(p.pid)) continue;
        known_pids_[p.pid] = now;
        std::string nm = lower(p.name);
        std::string cm;
        if (!p.cmdline.empty()) {
            std::string first = p.cmdline.substr(0, p.cmdline.find(' '));
            size_t sl = first.rfind('/');
            if (sl != std::string::npos) first = first.substr(sl + 1);
            cm = lower(first);
        }
        for (auto* in : insts) {
            if (in->capture.empty() || in->capture_mode != 0) continue;
            if (!capture_match(in->capture, nm, cm)) continue;
            if (pid_in_any_box(insts, p.pid)) break;    // çift atamayı önle
            {
                std::lock_guard<std::mutex> g2(in->mu);
                if ((int)in->attached_pids.size() >= in->max_procs) continue;  // dolu → sıradaki kutu
            }
            std::string aerr;
            mgr_->attach(in->id, p.pid, p.name, aerr);
            {
                std::lock_guard<std::mutex> g2(mu_);
                J e = J::makeObj();
                e["ts"] = now_iso();
                e["type"] = "AUTO_ASSIGN";
                e["pid"] = p.pid;
                e["pname"] = p.name;
                e["remote"] = "oto-yakala";
                e["via"] = "inst#" + std::to_string(in->id) + " " + in->label +
                            " (" + in->provider + "/" + in->country + ")";
                events_.push_back(e);
                ev_seq_++;
                if (events_.size() > 2000)
                    events_.erase(events_.begin(), events_.begin() + 1000);
            }
            gt::LogLine(LogColor::Magenta,
                "[oto-yakala] %s (pid %ld) -> inst#%d %s (%s)",
                p.name.c_str(), p.pid, in->id, in->label.c_str(), in->provider.c_str());
            break;                             // pid bir kutuya bağlandı
        }
    }
    // pid hafızasını sınırla (1 saat önce görülenler unutulur → yeniden açılırsa tekrar yakalanır)
    if (known_pids_.size() > 16384) {
        for (auto it = known_pids_.begin(); it != known_pids_.end();) {
            if (now_ms() - it->second > 3600000) it = known_pids_.erase(it);
            else ++it;
        }
    }
}

void Monitor::remember(const TcpConnInfo& c) {
    J e = J::makeObj();
    e["ts"] = now_iso();
    e["pid"] = c.pid;
    e["pname"] = c.pname;
    e["remote"] = c.remote + ":" + std::to_string(c.rport);
    e["via"] = c.via;
    events_.push_back(e);
    ev_seq_++;
    if (events_.size() > 2000) {
        events_.erase(events_.begin(), events_.begin() + 1000);
    }
    // debug terminaline de akıt (renkli)
    if (!c.pname.empty())
        gt::LogLine(LogColor::Cyan, "[mon] pid %ld (%s) -> %s:%d%s",
                    c.pid, c.pname.c_str(), c.remote.c_str(), c.rport,
                    c.via.empty() ? "" : ("  [" + c.via + "]").c_str());
}

std::vector<TcpConnInfo> Monitor::snapshot() {
    std::lock_guard<std::mutex> g(mu_);
    return last_;
}

J Monitor::events_json(size_t since, size_t max, size_t& total) {
    std::lock_guard<std::mutex> g(mu_);
    total = ev_seq_;
    J arr = J::makeArr();
    size_t skip = 0;
    for (auto& e : events_) {
        if (skip++ < since) continue;      // NOTE: events_ halka tamponuyla seq hizası
        if ((size_t)arr.size() >= max) break;
        arr.push(e);
    }
    return arr;
}

size_t Monitor::event_count() {
    std::lock_guard<std::mutex> g(mu_);
    return ev_seq_;
}

} // namespace gt
