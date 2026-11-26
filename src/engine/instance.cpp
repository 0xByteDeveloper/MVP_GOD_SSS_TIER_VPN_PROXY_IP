// ============================================================================
//  MVP_GOD_SSS_TIER — src/engine/instance.cpp
// ============================================================================
#include "engine/instance.h"
#include "core/json.h"
#include "core/log.h"
#include "core/util.h"
#include "providers/more.h"
#include <cstdio>
#include <cstdlib>
#include <thread>

#ifndef _WIN32
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/wait.h>
#else
#  include <windows.h>
#  include <wininet.h>      // InternetSetOption (WinInet broadcast)
#endif

namespace gt {

// ---- Instance ----------------------------------------------------------------
J Instance::to_json() const {
    std::lock_guard<std::mutex> g(mu);
    J o = J::makeObj();
    o["id"] = id;
    o["provider"] = provider;
    o["country"] = country;
    o["label"] = label;
    o["uuid"] = uuid;
    o["login"] = login;
    o["status"] = status;
    o["port"] = local_port;
    o["enc_mode"] = enc_mode;
    o["capture"] = capture;
    o["max_procs"] = max_procs;
    o["exit_ip"] = exit_ip;
    o["exit_country"] = exit_country;
    o["exit_isp"] = exit_isp;
    o["created_ms"] = (long long)created_ms;
    o["created_iso"] = now_iso();
    o["uptime"] = fmt_duration(now_ms() - created_ms);
    o["refresh_count"] = (long long)refresh_count;
    o["conns"] = router ? (long long)router->conns() : 0;
    o["bytes_up"] = router ? (long long)router->bytes_up() : 0;
    o["bytes_down"] = router ? (long long)router->bytes_down() : 0;
    o["fail"] = router ? (long long)router->fail() : 0;
    o["router_err"] = router ? router->last_err() : "";
    J ap = J::makeArr();
    for (long p : attached_pids) ap.push(p);
    o["attached_pids"] = ap;
    J an = J::makeArr();
    for (auto& n : attached_names) an.push(n);
    o["attached_names"] = an;
    o["profile_activity"] = profile_activity;
    o["profile_session_min"] = profile_session_min;
    o["profile_style"] = profile_style;
    J hops = J::makeArr();
    for (auto& h : hops_addrs) hops.push(h);
    o["hops"] = hops;
    o["vpngate_hostname"] = vpngate_hostname;
    o["vpngate_ovpn_path"] = vpngate_ovpn_path;
    J eps = J::makeArr();
    for (auto& e : endpoints) {
        J eo = J::makeObj();
        eo["host"] = e.host; eo["ip"] = e.ip; eo["port"] = e.port;
        eps.push(eo);
    }
    o["endpoints"] = eps;
    return o;
}

// kullanıcı logu (append — data/logs/users/user_<label>_<uuid8>.jsonl)
static std::string g_manager_data_dir = "data";   // manager kurulumda ayarlar

void Instance::user_log_append(const std::string& line) const {
    mkdir_p(g_manager_data_dir);
    FILE* f = fopen(user_log_path.c_str(), "a");
    if (!f) return;
    fputs(line.c_str(), f);
    fputc('\n', f);
    fclose(f);
}

// kalıcı alanlardan upstream kur (kalıcılık yüklemesi)
bool Instance::rebuild_upstream(std::string& err) {
    UpstreamConfig up;
    if (provider == "hola" && !endpoints.empty()) {
        up.kind = UpstreamConfig::HOLA_TRIAL;
        up.h_host = endpoints[0].host;
        up.h_ip = endpoints[0].ip;
        up.h_port = endpoints[0].port;
        up.h_login = login;
        up.h_agent_key = agent_key;
        if (enc_mode == "tls13" || enc_mode == "tls12") up.tls_ver = enc_mode;
    } else if (provider == "plain" && !upstream_addr.empty()) {
        up.kind = UpstreamConfig::PLAIN_HTTP;
        size_t c = upstream_addr.rfind(':');
        up.p_ip = upstream_addr.substr(0, c);
        up.p_port = atoi(upstream_addr.substr(c + 1).c_str());
    } else if ((provider == "socks" || provider == "tor") && !upstream_addr.empty()) {
        up.kind = UpstreamConfig::SOCKS5_UP;
        size_t c = upstream_addr.rfind(':');
        up.s_ip = upstream_addr.substr(0, c);
        up.s_port = atoi(upstream_addr.substr(c + 1).c_str());
    } else if (!hops_addrs.empty()) {            // chain / tor-fallback
        up.kind = UpstreamConfig::CHAIN;
        for (auto& a : hops_addrs) {
            bool socks = a.rfind("s5:", 0) == 0;
            std::string addr = socks ? a.substr(3) : a;
            size_t c = addr.rfind(':');
            if (c == std::string::npos) continue;
            UpstreamConfig::Hop h;
            h.ip = addr.substr(0, c);
            h.port = atoi(addr.substr(c + 1).c_str());
            h.socks = socks;
            up.hops.push_back(h);
        }
        if (up.hops.empty()) { err = "zincir hops bozuk"; return false; }
    } else if (provider == "vpngate") {
        up.kind = UpstreamConfig::VPN_GATE_OVPN;
    } else {
        err = "upstream kurulamadı: kalıcı alanlar eksik";
        return false;
    }
    if (router) router->set_upstream(up);
    return true;
}

// ---- Manager ------------------------------------------------------------------
InstanceManager::InstanceManager(std::string data_dir)
    : data_dir_(std::move(data_dir)) {
    g_manager_data_dir = data_dir_ + "/logs/users";
    mkdir_p(data_dir_);
    mkdir_p(data_dir_ + "/logs/users");
    mkdir_p(data_dir_ + "/logs/monitor");
    mkdir_p(data_dir_ + "/vpngate");
    boot_ms = now_ms();
}

std::string InstanceManager::derive_label(const std::string& provider) {
    // x1, x2, ... — sağlam ve okunur
    int n = 1;
    for (auto& i : inst_) if (i->provider == provider) n++;
    return "x" + std::to_string(n);
}

// canlı hop listesi (zincir / tor fallback) — SOCKS5 tercih edilir:
// halka http proxy'leri CONNECT'i çoğunlukla yalnız 443'e açar; SOCKS5 hop'lar
// herhangi ip:port'a CONNECT açar → zincir için güvenilir yol
static std::vector<SourceProxy> alive_hops_auto(int count, std::string& err) {
    std::string serr;
    auto srcs = sources_fetch(300, serr);
    if (srcs.empty()) { err = "kaynak listeleri boş: " + serr; return {}; }
    std::vector<SourceProxy> socks_only;
    for (auto& p : srcs) if (p.type == "socks5") socks_only.push_back(p);
    auto alive = pick_alive(socks_only.empty() ? srcs : socks_only, count);
    if ((int)alive.size() < count)
        gt::LogWarn("[zincir] yalnız %d/%d hop canlı bulundu — bu kadarla devam",
                    (int)alive.size(), count);
    return alive;
}

Instance* InstanceManager::create(const CreateOpts& o, std::string& err) {
    std::lock_guard<std::mutex> g(mu_);

    auto inst = std::make_unique<Instance>();
    inst->id = next_id_++;
    inst->provider = o.provider;
    inst->country = o.country;
    inst->label = o.label.empty() ? derive_label(o.provider) : o.label;
    inst->enc_mode = o.enc.empty() ? "auto" : o.enc;
    inst->capture = o.capture;
    inst->max_procs = o.max_procs > 0 ? o.max_procs : 1;
    inst->created_ms = now_ms();

    UpstreamConfig up;

    if (o.provider == "hola") {
        gt::LogLine(LogColor::Cyan, "[inst#%d] Hola akışı başlıyor (ülke=%s, yeni uuid)...",
                    inst->id, o.country.c_str());
        HolaIdentity hid = hola_acquire(o.country);
        if (!hid.ok) {
            err = hid.err;
            gt::LogErr("[inst#%d] %s", inst->id, err.c_str());
            return nullptr;
        }
        inst->uuid = hid.uuid;
        inst->login = hid.login;
        inst->agent_key = hid.agent_key;
        inst->session_key = hid.session_key;
        inst->ext_ver = hid.ext_ver;
        inst->endpoints = hid.endpoints;
        up.kind = UpstreamConfig::HOLA_TRIAL;
        up.h_host = hid.endpoints[0].host;
        up.h_ip = hid.endpoints[0].ip;
        up.h_port = hid.endpoints[0].port;
        up.h_login = hid.login;
        up.h_agent_key = hid.agent_key;
        if (inst->enc_mode == "tls13" || inst->enc_mode == "tls12") up.tls_ver = inst->enc_mode;
    } else if (o.provider == "plain") {
        std::string addr = o.addr;
        if (addr.empty()) {
            gt::LogLine(LogColor::Cyan, "[inst#%d] düz proxy otomatik seçiliyor (canlı test)...",
                        inst->id);
            std::string aerr;
            auto alive = alive_hops_auto(1, aerr);
            if (alive.empty()) { err = "canlı ücretsiz proxy bulunamadı: " + aerr; return nullptr; }
            addr = alive[0].addr;
        }
        size_t colon = addr.rfind(':');
        if (colon == std::string::npos) { err = "plain addr=IP:PORT gerekli: " + addr; return nullptr; }
        up.kind = UpstreamConfig::PLAIN_HTTP;
        up.p_ip = addr.substr(0, colon);
        up.p_port = atoi(addr.substr(colon + 1).c_str());
        inst->upstream_addr = addr;                // kalıcılık
    } else if (o.provider == "socks") {
        if (o.addr.empty()) { err = "manuel socks: addr=IP:PORT gerekli"; return nullptr; }
        size_t colon = o.addr.rfind(':');
        if (colon == std::string::npos) { err = "socks addr=IP:PORT gerekli: " + o.addr; return nullptr; }
        up.kind = UpstreamConfig::SOCKS5_UP;
        up.s_ip = o.addr.substr(0, colon);
        up.s_port = atoi(o.addr.substr(colon + 1).c_str());
        inst->upstream_addr = o.addr;
    } else if (o.provider == "tor") {
        int tp = tor_detect_port();
        if (tp > 0) {
            gt::LogOk("[inst#%d] yerel Tor SOCKS bulundu: 127.0.0.1:%d", inst->id, tp);
            up.kind = UpstreamConfig::SOCKS5_UP;
            up.s_ip = "127.0.0.1";
            up.s_port = tp;
            inst->upstream_addr = "127.0.0.1:" + std::to_string(tp);
            inst->enc_mode = "tor-yerel";
        } else {
            gt::LogLine(LogColor::Yellow, "[inst#%d] yerel Tor yok — 3-hop zincir "
                        "(Tor-benzeri katmanlama) kuruluyor...", inst->id);
            auto alive = alive_hops_auto(3, err);
            if (alive.empty()) { err = "zincir kurulamadı: " + err; return nullptr; }
            up.kind = UpstreamConfig::CHAIN;
            for (auto& a : alive) {
                UpstreamConfig::Hop h;
                size_t c = a.addr.rfind(':');
                h.ip = a.addr.substr(0, c);
                h.port = atoi(a.addr.substr(c + 1).c_str());
                h.socks = (a.type == "socks5");
                up.hops.push_back(h);
                inst->hops_addrs.push_back((a.type == "socks5" ? "s5:" : "") + a.addr);
            }
            inst->enc_mode = "tor3-zincir";
        }
    } else if (o.provider == "chain") {
        int count = inst->enc_mode == "chain2" ? 2 : 3;
        if (!o.hops.empty()) {                    // manuel hops
            count = (int)o.hops.size();
            up.kind = UpstreamConfig::CHAIN;
            for (auto& a : o.hops) {
                size_t c = a.rfind(':');
                if (c == std::string::npos) continue;
                UpstreamConfig::Hop h;
                h.ip = a.substr(0, c);
                h.port = atoi(a.substr(c + 1).c_str());
                h.socks = false;
                up.hops.push_back(h);
                inst->hops_addrs.push_back(a);
            }
            if (up.hops.empty()) { err = "manuel zincir hops bozuk"; return nullptr; }
        } else {
            gt::LogLine(LogColor::Cyan, "[inst#%d] %d-hop zincir kuruluyor (canlı seçim)...",
                        inst->id, count);
            auto alive = alive_hops_auto(count, err);
            if (alive.empty()) { err = "zincir kurulamadı: " + err; return nullptr; }
            up.kind = UpstreamConfig::CHAIN;
            for (auto& a : alive) {
                UpstreamConfig::Hop h;
                size_t c = a.addr.rfind(':');
                h.ip = a.addr.substr(0, c);
                h.port = atoi(a.addr.substr(c + 1).c_str());
                h.socks = (a.type == "socks5");
                up.hops.push_back(h);
                inst->hops_addrs.push_back((a.type == "socks5" ? "s5:" : "") + a.addr);
            }
        }
    } else if (o.provider == "vpngate") {
        // seçilen sunucu ya da ülkenin en iyisi — .ovpn dışa aktarılır
        std::string b64 = o.vpngate_ovpn_b64;
        std::string host = o.vpngate_hostname;
        if (b64.empty()) {
            std::string ferr;
            auto servers = vpngate_fetch(o.country, 1, ferr);
            if (servers.empty()) { err = "vpngate listesi boş: " + ferr; return nullptr; }
            host = servers[0].hostname;
            b64 = servers[0].ovpn_b64;
            if (inst->country.empty()) inst->country = servers[0].country_short;
        }
        inst->vpngate_hostname = host;
        inst->vpngate_ovpn_path = data_dir_ + "/vpngate/" + inst->label + "_" +
                                  host + ".ovpn";
        if (!write_file(inst->vpngate_ovpn_path, b64_decode(b64))) {
            err = "ovpn dosyası yazılamadı: " + inst->vpngate_ovpn_path;
            return nullptr;
        }
        up.kind = UpstreamConfig::VPN_GATE_OVPN;
    } else {
        err = "bilinmeyen sağlayıcı: " + o.provider;
        return nullptr;
    }

    inst->local_port = next_port_++;
    inst->router = std::make_unique<Router>(up, inst->local_port,
                                            "inst" + std::to_string(inst->id));
    if (!inst->router->start(err)) {
        gt::LogErr("[inst#%d] router başlatılamadı: %s", inst->id, err.c_str());
        return nullptr;
    }
    inst->status = o.provider == "vpngate" ? "OVPN" : "RUNNING";

    // gerçeklik profili (uuid/etiket hash'inden — kullanıcı "kişiliği" sabit kalır)
    unsigned h = 2166136261u;
    for (char c : inst->uuid + inst->label) { h ^= (unsigned char)c; h *= 16777619u; }
    inst->profile_activity = 35 + (h % 61);
    inst->profile_session_min = 18 + (h >> 8) % 90;
    static const char* styles[] = {"gece kullanıcısı", "sabit ofis deseni",
                                   "yoğun ziyaretçi", "seyrek gezgin", "dengeli"};
    inst->profile_style = styles[(h >> 16) % 5];

    // kullanıcı logu
    inst->user_log_path = data_dir_ + "/logs/users/user_" + inst->label + "_" +
                          (inst->uuid.empty() ? inst->label : inst->uuid.substr(0, 8)) + ".jsonl";
    {
        J e = J::makeObj();
        e["ts"] = now_iso();
        e["type"] = "CREATED";
        e["provider"] = o.provider;
        e["country"] = o.country;
        e["enc"] = inst->enc_mode;
        e["capture"] = inst->capture;
        if (!inst->uuid.empty()) e["uuid"] = inst->uuid.substr(0, 8);
        if (!inst->upstream_addr.empty()) e["endpoint"] = inst->upstream_addr;
        if (!inst->hops_addrs.empty()) {
            J ha = J::makeArr();
            for (auto& a : inst->hops_addrs) ha.push(a);
            e["hops"] = ha;
        }
        if (o.provider == "vpngate") {
            e["ovpn"] = inst->vpngate_ovpn_path;
            e["vpngate_host"] = inst->vpngate_hostname;
        }
        inst->user_log_append(e.dump());
    }

    gt::LogOk("[inst#%d %s] %s — 127.0.0.1:%d üzerinden (%s, şifreleme=%s%s%s)",
              inst->id, inst->label.c_str(),
              inst->status.c_str(), inst->local_port, o.provider.c_str(),
              inst->enc_mode.c_str(),
              inst->capture.empty() ? "" : ", oto-yakala: ",
              inst->capture.c_str());
    inst_.push_back(std::move(inst));
    save();
    return inst_.back().get();
}

bool InstanceManager::newuser(int id, std::string& err) {
    std::lock_guard<std::mutex> g(mu_);
    Instance* in = find(id);
    if (!in) { err = "instance yok"; return false; }
    if (in->provider != "hola") { err = "new user yalnız Hola için (diğerleri: refresh)"; return false; }

    gt::LogLine(LogColor::Yellow, "[inst#%d %s] NEW USER MODE — ücretsiz süre doldu, "
                "yeni kimlik üretiliyor...", in->id, in->label.c_str());
    HolaIdentity hid = hola_acquire(in->country);
    if (!hid.ok) { err = hid.err; return false; }
    in->uuid = hid.uuid;
    in->login = hid.login;
    in->agent_key = hid.agent_key;
    in->session_key = hid.session_key;
    in->ext_ver = hid.ext_ver;
    in->endpoints = hid.endpoints;
    in->refresh_count++;
    in->last_newuser_ms = now_ms();
    UpstreamConfig up;
    up.kind = UpstreamConfig::HOLA_TRIAL;
    up.h_host = hid.endpoints[0].host;
    up.h_ip = hid.endpoints[0].ip;
    up.h_port = hid.endpoints[0].port;
    up.h_login = hid.login;
    up.h_agent_key = hid.agent_key;
    if (in->enc_mode == "tls13" || in->enc_mode == "tls12") up.tls_ver = in->enc_mode;
    if (in->router) in->router->set_upstream(up);   // yerel port AYNİ kalır

    J e = J::makeObj();
    e["ts"] = now_iso();
    e["type"] = "NEWUSER";
    e["uuid"] = in->uuid.substr(0, 8);
    e["detail"] = "yeni kullanıcı kimliği alındı (port degismedi)";
    in->user_log_append(e.dump());
    gt::LogOk("[inst#%d %s] yeni kullanıcı hazır: uuid %s (yeni agent_key)",
              in->id, in->label.c_str(), in->uuid.substr(0, 8).c_str());
    save();
    return true;
}

bool InstanceManager::check_exit_ip(int id, std::string& err) {
    Instance* in = find(id);
    if (!in) { err = "instance yok"; return false; }

    std::string body;
    if (in->provider == "hola") {
        if (in->endpoints.empty()) { err = "hola uç noktası yok"; return false; }
        body = hola_exit_ip_json(in->endpoints[0], in->login, in->agent_key);
    } else if (in->provider == "vpngate") {
        err = "VPN Gate kutusu OpenVPN config modundadır — çıkış IP'sini OpenVPN "
              "istemcisi bağlandığında görür";
        return false;
    } else {
        // plain / socks / tor / chain — yerel router üzerinden uçtan uca
        body = socks_exit_ip_json(in->local_port, 25);
    }
    J r;
    if (!J::parse(body, r) || !r.has("query")) {
        err = "çıkış IP'si alınamadı: " + body.substr(0, 200);
        return false;
    }
    {
        std::lock_guard<std::mutex> g2(in->mu);
        in->exit_ip = r.s("query");
        in->exit_country = r.s("country");
        in->exit_isp = r.s("isp");
    }
    J e = J::makeObj();
    e["ts"] = now_iso();
    e["type"] = "EXIT_IP";
    e["ip"] = in->exit_ip;
    e["country"] = in->exit_country;
    in->user_log_append(e.dump());
    gt::LogOk("[inst#%d %s] çıkış IP: %s (%s — %s)", in->id, in->label.c_str(),
              in->exit_ip.c_str(), in->exit_country.c_str(), in->exit_isp.c_str());
    save();
    return true;
}

bool InstanceManager::refresh(int id, std::string& err) {
    // hola: aynı uuid ile zgettunnels yenileme; zincir/plain: canlı hop yeniden seçim
    Instance* in = find(id);
    if (!in) { err = "instance yok"; return false; }
    if (in->provider == "hola") {
        HolaIdentity hid = hola_acquire(in->country);
        if (!hid.ok) { err = hid.err; return false; }
        {
            std::lock_guard<std::mutex> g2(in->mu);
            in->agent_key = hid.agent_key;
            in->endpoints = hid.endpoints;
        }
        UpstreamConfig up;
        up.kind = UpstreamConfig::HOLA_TRIAL;
        up.h_host = hid.endpoints[0].host;
        up.h_ip = hid.endpoints[0].ip;
        up.h_port = hid.endpoints[0].port;
        up.h_login = hid.login;
        up.h_agent_key = hid.agent_key;
        if (in->enc_mode == "tls13" || in->enc_mode == "tls12") up.tls_ver = in->enc_mode;
        if (in->router) in->router->set_upstream(up);
    } else {
        // zincir/düz proxy yenileme: hop'ları yeniden seç
        std::string cerr;
        if (!in->rebuild_upstream(cerr)) { err = cerr; return false; }
        gt::LogOk("[inst#%d %s] upstream yeniden kuruldu", in->id, in->label.c_str());
    }
    in->refresh_count++;
    save();
    return true;
}

bool InstanceManager::update_settings(int id, const std::string& capture, int max_procs,
                                     const std::string& label, std::string& err) {
    Instance* in = find(id);
    if (!in) { err = "instance yok"; return false; }
    {
        std::lock_guard<std::mutex> g2(in->mu);
        if (!capture.empty()) in->capture = capture;
        if (max_procs > 0) in->max_procs = max_procs;
        if (!label.empty()) in->label = label;
    }
    J e = J::makeObj();
    e["ts"] = now_iso();
    e["type"] = "SETTINGS";
    e["capture"] = in->capture;
    e["max_procs"] = in->max_procs;
    in->user_log_append(e.dump());
    gt::LogOk("[inst#%d %s] kutu ayarları güncellendi (oto-yakala: %s, max %d)",
              in->id, in->label.c_str(), in->capture.c_str(), in->max_procs);
    save();
    return true;
}

bool InstanceManager::destroy(int id, std::string& err) {
    std::lock_guard<std::mutex> g(mu_);
    Instance* in = find(id);
    if (!in) { err = "instance yok"; return false; }
    J e = J::makeObj();
    e["ts"] = now_iso();
    e["type"] = "DESTROYED";
    in->user_log_append(e.dump());
    gt::LogWarn("[inst#%d %s] kapatılıyor (port %d)", in->id, in->label.c_str(), in->local_port);
    in->status = "STOPPING";
    if (in->router) in->router->stop();
    for (auto it = inst_.begin(); it != inst_.end(); it++) {
        if (it->get() == in) { inst_.erase(it); break; }
    }
    if (global_on_ && global_inst_ == id) { global_on_ = false; global_inst_ = 0; }
    save();
    return true;
}

bool InstanceManager::attach(int id, long pid, const std::string& name, std::string& err) {
    Instance* in = find(id);
    if (!in) { err = "instance yok"; return false; }
    {
        std::lock_guard<std::mutex> g2(in->mu);
        bool already = false;
        for (long p : in->attached_pids) if (p == pid) { already = true; break; }
        if (!already) {
            in->attached_pids.push_back(pid);
            in->attached_names.push_back(name.empty()
                                             ? "pid:" + std::to_string(pid) : name);
            J e = J::makeObj();
            e["ts"] = now_iso();
            e["type"] = "ATTACH";
            e["pid"] = pid;
            if (!name.empty()) e["name"] = name;
            in->user_log_append(e.dump());
        }
    }                     // kilit burada serbest — save() içerde i->mu tekrar alıyor!
    save();
    return true;
}

bool InstanceManager::detach(int id, long pid, std::string& err) {
    Instance* in = find(id);
    if (!in) { err = "instance yok"; return false; }
    {
        std::lock_guard<std::mutex> g2(in->mu);
        for (size_t k = 0; k < in->attached_pids.size(); k++) {
            if (in->attached_pids[k] == pid) {
                in->attached_pids.erase(in->attached_pids.begin() + (long)k);
                if (k < in->attached_names.size())
                    in->attached_names.erase(in->attached_names.begin() + (long)k);
            }
        }
    }                     // kilit serbest
    save();
    return true;
}

long InstanceManager::launch(int id, const std::string& exe, const std::string& args,
                            std::string& err) {
    Instance* in = find(id);
    if (!in) { err = "instance yok"; return -1; }
    if (in->provider == "vpngate") {
        err = "VPN Gate kutusu .ovpn modundadır — OpenVPN istemcisiyle başlatın "
              "(openvpn --config <ovpn>)";
        return -1;
    }
    // ortam: GODTIER_SOCKS=127.0.0.1:<port> (+ Linux: LD_PRELOAD=hook)
#ifndef _WIN32
    std::string sock = "127.0.0.1:" + std::to_string(in->local_port);
    // hook.so çalıştırılabilirin yanında ya da kök dizinde aranır
    const char* candidates[] = {"hook/libgodtier_hook.so", "../hook/libgodtier_hook.so",
                               "libgodtier_hook.so"};
    std::string hookpath;
    for (auto c : candidates) { if (file_exists(c)) { hookpath = c; break; } }
    if (hookpath.empty()) { err = "hook bulunamadi (hook/libgodtier_hook.so derlendi mi?)"; return -1; }

    // fork + exec (çok iplikli motorda popen KİLİTLENEBİLİR — fork sonrası yalnız
    // async-signal-safe çağrılar yapılıp hemen exec edilir; çıktı log dosyasına)
    std::string logpath = data_dir_ + "/logs/launch_last.log";
    pid_t pid = fork();
    if (pid < 0) { err = "fork hatasi"; return -1; }
    if (pid == 0) {
        // çocuk: motorun tüm soketlerini KAPAT (miras alınan dinleyici/API
        // soketleri açık kalırsa HTTP yanıtı kapanamaz → istemci askıda kalır)
        for (int fd = 3; fd < 1024; fd++) close(fd);
        setenv("GODTIER_SOCKS", sock.c_str(), 1);
        setenv("LD_PRELOAD", hookpath.c_str(), 1);
        int fd = open(logpath.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); close(fd); }
        execlp("/bin/sh", "sh", "-c", (exe + " " + args).c_str(), (char*)nullptr);
        _exit(127);                       // exec başarısız
    }
    // ebeveyn: zombie'ı derhal temizle
    int st; waitpid(pid, &st, WNOHANG);
    gt::LogOk("[inst#%d] süreç başlatıldı: %s (pid %d) → %s (log: %s)",
              id, exe.c_str(), (int)pid, sock.c_str(), logpath.c_str());
    // exe taban adı ile attach et (oto-yakalama ile aynı görünüm)
    std::string base = exe;
    size_t slash = base.rfind('/');
    if (slash != std::string::npos) base = base.substr(slash + 1);
    attach(id, (long)pid, base, err);
    return (long)pid;
#else
    (void)id; (void)exe; (void)args;
    err = "Windows: launch inject_windows.cpp içinde — derlemeye dahil";
    return -1;
#endif
}

// ---- sistem geneli -----------------------------------------------------------
bool InstanceManager::set_global(bool on, int id, std::string& err) {
#ifdef _WIN32
    Instance* in = find(id);
    if (on && !in) { err = "instance yok"; return false; }
    // WinInet sistem proxy'si (kullanıcı bazlı kayıt)
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
                      0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) { err = "kayit acilamadi"; return false; }
    if (on) {
        std::string v = "127.0.0.1:" + std::to_string(in->local_port);
        std::wstring wv(v.begin(), v.end());
        DWORD on = 1;
        RegSetValueExW(k, L"ProxyServer", 0, REG_SZ, (BYTE*)wv.c_str(), (DWORD)((wv.size() + 1) * 2));
        RegSetValueExW(k, L"ProxyEnable", 0, REG_DWORD, (BYTE*)&on, sizeof on);
        RegSetValueExW(k, L"ProxyOverride", 0, REG_SZ, (BYTE*)L"<local>", 16);
        global_on_ = true; global_inst_ = id;
        gt::LogOk("[global] TÜM PC trafiği -> 127.0.0.1:%d (inst#%d)", in->local_port, id);
    } else {
        DWORD off = 0;
        RegSetValueExW(k, L"ProxyEnable", 0, REG_DWORD, (BYTE*)&off, sizeof off);
        global_on_ = false; global_inst_ = 0;
        gt::LogWarn("[global] sistem proxy'si KAPALI");
    }
    RegCloseKey(k);
    // açık WinInet süreçlerine haber (InternetSetOption broadcast):
    typedef BOOL (WINAPI *PIS)(HINTERNET, DWORD, LPVOID, DWORD);
    HMODULE hw = GetModuleHandleW(L"wininet.dll");
    if (hw) {
        PIS p = (PIS)GetProcAddress(hw, "InternetSetOptionW");
        if (p) {
            p(nullptr, 39 /*INTERNET_OPTION_SETTINGS_CHANGED*/, nullptr, 0);
            p(nullptr, 37 /*INTERNET_OPTION_REFRESH*/, nullptr, 0);
        }
    }
    return true;
#else
    if (on) {
        Instance* in = find(id);
        if (!in) { err = "instance yok"; return false; }
        global_on_ = true; global_inst_ = id;
        gt::LogOk("[global] Linux: sistem geneli yerine çocuk süreçler env ile: "
                  "GODTIER_SOCKS=127.0.0.1:%d (inst#%d) — launch kullanın",
                  in->local_port, id);
    } else {
        global_on_ = false; global_inst_ = 0;
    }
    return true;
#endif
}

// ---- listeleme / kalıcılık ----------------------------------------------------
std::vector<Instance*> InstanceManager::list() {
    std::lock_guard<std::mutex> g(mu_);
    std::vector<Instance*> out;
    for (auto& i : inst_) out.push_back(i.get());
    return out;
}
Instance* InstanceManager::find(int id) {
    for (auto& i : inst_) if (i->id == id) return i.get();
    return nullptr;
}
size_t InstanceManager::count() { return inst_.size(); }

void InstanceManager::save() {
    J root = J::makeObj();
    J arr = J::makeArr();
    for (auto& i : inst_) {
        J o = J::makeObj();
        {
            std::lock_guard<std::mutex> g2(i->mu);
            o["id"] = i->id;
            o["provider"] = i->provider;
            o["country"] = i->country;
            o["label"] = i->label;
            o["enc_mode"] = i->enc_mode;
            o["capture"] = i->capture;
            o["max_procs"] = i->max_procs;
            o["uuid"] = i->uuid;
            o["login"] = i->login;
            o["agent_key"] = i->agent_key;
            o["session_key"] = i->session_key;
            o["ext_ver"] = i->ext_ver;
            o["port"] = i->local_port;
            o["status"] = i->status;
            o["upstream_addr"] = i->upstream_addr;
            o["hops"] = [&] { J a = J::makeArr(); for (auto& h : i->hops_addrs) a.push(h); return a; }();
            o["vpngate_hostname"] = i->vpngate_hostname;
            o["vpngate_ovpn_path"] = i->vpngate_ovpn_path;
            o["exit_ip"] = i->exit_ip;
            o["exit_country"] = i->exit_country;
            o["exit_isp"] = i->exit_isp;
            o["created_ms"] = (long long)i->created_ms;
            o["refresh_count"] = (long long)i->refresh_count;
            o["user_log_path"] = i->user_log_path;
            o["profile_activity"] = i->profile_activity;
            o["profile_session_min"] = i->profile_session_min;
            o["profile_style"] = i->profile_style;
            J ap = J::makeArr();
            for (long p : i->attached_pids) ap.push(p);
            o["attached_pids"] = ap;
            J an = J::makeArr();
            for (auto& n : i->attached_names) an.push(n);
            o["attached_names"] = an;
            J eps = J::makeArr();
            for (auto& e2 : i->endpoints) {
                J eo = J::makeObj();
                eo["host"] = e2.host; eo["ip"] = e2.ip; eo["port"] = e2.port;
                eps.push(eo);
            }
            o["endpoints"] = eps;
        }
        arr.push(o);
    }
    root["instances"] = arr;
    root["next_id"] = next_id_;
    root["next_port"] = next_port_;
    root["global_on"] = global_on_;
    root["global_inst"] = global_inst_;
    write_file(data_dir_ + "/instances.json", root.dump());
}

void InstanceManager::load() {
    std::string txt;
    if (!read_file(data_dir_ + "/instances.json", txt) || txt.empty()) return;
    J root;
    if (!J::parse(txt, root)) return;
    for (auto& o : root["instances"].arr) {
        auto inst = std::make_unique<Instance>();
        inst->id = o.i("id");
        inst->provider = o.s("provider");
        inst->country = o.s("country");
        inst->label = o.s("label");
        inst->enc_mode = o.s("enc_mode", "auto");
        inst->capture = o.s("capture");
        inst->max_procs = (int)o.i("max_procs", 1);
        inst->uuid = o.s("uuid");
        inst->login = o.s("login");
        inst->agent_key = o.s("agent_key");
        inst->session_key = o.s("session_key");
        inst->ext_ver = o.s("ext_ver");
        inst->local_port = (int)o.i("port");
        inst->status = o.s("status", "RUNNING");
        inst->exit_ip = o.s("exit_ip");
        inst->exit_country = o.s("exit_country");
        inst->exit_isp = o.s("exit_isp");
        inst->upstream_addr = o.s("upstream_addr");
        for (auto& h : o["hops"].arr) if (h.t == J::STR) inst->hops_addrs.push_back(h.str);
        inst->vpngate_hostname = o.s("vpngate_hostname");
        inst->vpngate_ovpn_path = o.s("vpngate_ovpn_path");
        inst->created_ms = (uint64_t)o.i("created_ms");
        inst->refresh_count = (uint64_t)o.i("refresh_count");
        inst->user_log_path = o.s("user_log_path");
        inst->profile_activity = (int)o.i("profile_activity");
        inst->profile_session_min = (int)o.i("profile_session_min");
        inst->profile_style = o.s("profile_style");
        for (auto& p : o["attached_pids"].arr) inst->attached_pids.push_back(p.t == J::NUM ? (long)p.num : 0);
        for (auto& n : o["attached_names"].arr) if (n.t == J::STR) inst->attached_names.push_back(n.str);
        for (auto& e2 : o["endpoints"].arr) {
            HolaEndpoint ep;
            ep.host = e2.s("host"); ep.ip = e2.s("ip"); ep.port = (int)e2.i("port");
            inst->endpoints.push_back(ep);
        }
        if (inst->user_log_path.empty())
            inst->user_log_path = data_dir_ + "/logs/users/user_" + inst->label + "_" +
                                  (inst->uuid.empty() ? inst->label : inst->uuid.substr(0, 8)) + ".jsonl";
        // upstream'i kalıcı alanlardan kur (router'a verilecek)
        UpstreamConfig up;
        if (inst->provider == "hola" && !inst->endpoints.empty()) {
            up.kind = UpstreamConfig::HOLA_TRIAL;
            up.h_host = inst->endpoints[0].host;
            up.h_ip = inst->endpoints[0].ip;
            up.h_port = inst->endpoints[0].port;
            up.h_login = inst->login;
            up.h_agent_key = inst->agent_key;
            if (inst->enc_mode == "tls13" || inst->enc_mode == "tls12") up.tls_ver = inst->enc_mode;
        } else if (inst->provider == "plain" && !inst->upstream_addr.empty()) {
            size_t c = inst->upstream_addr.rfind(':');
            up.kind = UpstreamConfig::PLAIN_HTTP;
            up.p_ip = inst->upstream_addr.substr(0, c);
            up.p_port = atoi(inst->upstream_addr.substr(c + 1).c_str());
        } else if ((inst->provider == "socks" || inst->provider == "tor") &&
                   !inst->upstream_addr.empty()) {
            size_t c = inst->upstream_addr.rfind(':');
            up.kind = UpstreamConfig::SOCKS5_UP;
            up.s_ip = inst->upstream_addr.substr(0, c);
            up.s_port = atoi(inst->upstream_addr.substr(c + 1).c_str());
        } else if (!inst->hops_addrs.empty()) {
            up.kind = UpstreamConfig::CHAIN;
            for (auto& a : inst->hops_addrs) {
                bool socks = a.rfind("s5:", 0) == 0;
                std::string addr = socks ? a.substr(3) : a;
                size_t c = addr.rfind(':');
                if (c == std::string::npos) continue;
                UpstreamConfig::Hop h;
                h.ip = addr.substr(0, c);
                h.port = atoi(addr.substr(c + 1).c_str());
                h.socks = socks;
                up.hops.push_back(h);
            }
        } else if (inst->provider == "vpngate") {
            up.kind = UpstreamConfig::VPN_GATE_OVPN;
        } else {
            continue;                            // bozuk kayıt — atla
        }
        std::string e3;
        inst->router = std::make_unique<Router>(up, inst->local_port,
                                                "inst" + std::to_string(inst->id));
        if (!inst->router->start(e3)) {
            gt::LogWarn("[inst#%d] port %d alınamadı (%s) — port kaydırılıyor",
                        inst->id, inst->local_port, e3.c_str());
            inst->local_port = next_port_++;
            inst->router = std::make_unique<Router>(up, inst->local_port,
                                                    "inst" + std::to_string(inst->id));
            if (!inst->router->start(e3)) continue;
        }
        inst_.push_back(std::move(inst));
    }
    next_id_ = std::max(next_id_, (int)root.i("next_id"));
    next_port_ = std::max(next_port_, (int)root.i("next_port"));
    global_on_ = root.bl("global_on");
    global_inst_ = (int)root.i("global_inst");
    if (!inst_.empty())
        gt::LogOk("kalıcılık: %zu instance geri yüklendi (data/instances.json)", inst_.size());
}

} // namespace gt
