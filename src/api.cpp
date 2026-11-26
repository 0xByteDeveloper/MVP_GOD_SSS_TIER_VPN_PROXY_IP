// ============================================================================
//  MVP_GOD_SSS_TIER — src/api.cpp
// ============================================================================
#include "api.h"
#include "core/json.h"
#include "core/log.h"
#include "core/util.h"
#include "engine/instance.h"
#include "engine/monitor.h"
#include "engine/proclist.h"
#include "net/httpclient.h"
#include "providers/countries.h"
#include "providers/more.h"

#include <cstring>
#include <thread>

using gt::J;
using ui::HttpServer;
using ui::HttpRequest;

namespace ui {

static ConsoleBuffer* g_console = nullptr;
static gt::InstanceManager* g_mgr = nullptr;
static gt::Monitor* g_mon = nullptr;

void uiSetConsole(ConsoleBuffer* c) { g_console = c; }

// ---- yardımcılar --------------------------------------------------------------
static J parse_body(const HttpRequest& rq) {
    J j;
    if (rq.body.empty()) return j;
    J::parse(rq.body, j);
    return j;
}
static long id_from_path(const HttpRequest& rq, const std::string& prefix) {
    if (rq.path.rfind(prefix, 0) != 0) return 0;
    std::string rest = rq.path.substr(prefix.size());
    size_t slash = rest.find('/');
    if (slash != std::string::npos) rest = rest.substr(0, slash);
    return atol(rest.c_str());
}
static std::string action_from_path(const HttpRequest& rq, const std::string& prefix) {
    if (rq.path.rfind(prefix, 0) != 0) return "";
    std::string rest = rq.path.substr(prefix.size());
    size_t slash = rest.find('/');
    return slash == std::string::npos ? "" : rest.substr(slash + 1);
}

// ---- GET /api/status ------------------------------------------------------------
static std::string apiStatus(const HttpRequest&) {
    J o = J::makeObj();
    o["app"] = "MVP_GOD_SSS_TIER";
    o["version"] = "2.0.0";
    o["engine"] = "C++17 — Hola trial + SOCKS5 router + zincir + PID izleyici";
    o["instances"] = (long long)g_mgr->count();
    o["global_on"] = g_mgr->global_on();
    o["uptime"] = gt::fmt_duration(gt::now_ms() - g_mgr->boot_ms);
    o["countries"] = (long long)gt::HolaCountries().size();
    int tor = gt::tor_detect_port();
    o["tor_local"] = tor;
    o["tor_local_txt"] = tor ? ("127.0.0.1:" + std::to_string(tor)) : "yok (zincir fallback)";
    J pr = J::makeArr();
    pr.push("hola"); pr.push("plain"); pr.push("socks"); pr.push("tor");
    pr.push("chain"); pr.push("vpngate");
    o["providers"] = pr;
    return o.dump();
}

// ---- GET /api/countries ---------------------------------------------------------
static std::string apiCountries(const HttpRequest&) {
    J arr = J::makeArr();
    for (auto& c : gt::HolaCountries()) {
        J o = J::makeObj();
        o["code"] = c.code;
        o["name"] = c.name;
        arr.push(o);
    }
    return arr.dump();
}

// ---- GET /api/enc — şifreleme kataloğu (uyumluluk + hız etkisi) ------------------
static std::string apiEnc(const HttpRequest&) {
    J arr = J::makeArr();
    auto add = [&](const char* id, const char* name, const char* compat,
                   const char* latency, const char* note) {
        J e = J::makeObj();
        e["id"] = id;
        e["name"] = name;
        e["compat"] = compat;
        e["latency"] = latency;
        e["note"] = note;
        arr.push(e);
    };
    add("auto",   "OTO — en hızlı uyumlu", "hola,plain,socks,tor,chain",
        "+0 ms", "Hola: TLS 1.2+ otomatik; düz proxy: şifresiz hızlı yol");
    add("tls13",  "TLS 1.3 (sadece)", "hola",
        "+0 ms", "Trial tüneli yalnız TLS 1.3'e sabitlenir (modern, hızlı)");
    add("tls12",  "TLS 1.2 (sadece)", "hola",
        "+0 ms", "Eski sunucularla uyum — trial tüneli TLS 1.2'ye sabitlenir");
    add("plain",  "Şifresiz (en hızlı)", "plain,socks",
        "+0 ms", "UYARI: halka açık proxy — trafik okunabilir; hız maksimum");
    add("chain2", "2 katman zincir", "chain,plain,tor",
        "+40-80 ms", "İki bağımsız çıkış — ilk katman kimliği görür, ikincisi görmez");
    add("chain3", "3 katman zincir (Tor benzeri)", "chain,plain,tor",
        "+80-160 ms", "Tor'un 3-hop mantığının eşdeğeri — ücretsiz proxy'lerle");
    add("tor3",   "Tor yerel (9050) / 3-zincir", "tor",
        "~150 ms", "Yerel Tor SOCKS varsa gerçek Tor ağı; yoksa otomatik 3-zincir");
    J o = J::makeObj();
    o["options"] = arr;
    return o.dump();
}

// ---- GET /api/instances ----------------------------------------------------------
static std::string apiInstances(const HttpRequest&) {
    J arr = J::makeArr();
    for (auto* i : g_mgr->list()) arr.push(i->to_json());
    J o = J::makeObj();
    o["instances"] = arr;
    return o.dump();
}

// ---- POST /api/instances_create ---------------------------------------------------
static std::string apiCreateInstance(const HttpRequest& rq) {
    J b = parse_body(rq);
    gt::CreateOpts o;
    o.provider = b.s("provider", "hola");
    o.country = b.s("country", "TR");
    o.label = b.s("label");
    o.addr = b.s("addr");                       // plain/socks manuel "ip:port"
    o.enc = b.s("enc", "auto");
    o.capture = b.s("capture");
    o.max_procs = (int)b.i("max_procs", 1);
    for (auto& h : b["hops"].arr) if (h.t == J::STR) o.hops.push_back(h.str);
    o.vpngate_hostname = b.s("vpngate_hostname");
    o.vpngate_ovpn_b64 = b.s("vpngate_ovpn_b64");
    if (o.country.size() == 2) for (auto& c : o.country) c = (char)toupper(c);
    std::string err;
    gt::Instance* in = g_mgr->create(o, err);
    if (!in) {
        J e = J::makeObj();
        e["err"] = err;
        return e.dump();
    }
    // otomatik çıkış IP kontrolü (arka planda — UI hemen kutuyu görür)
    std::thread([in] {
        std::string e2;
        g_mgr->check_exit_ip(in->id, e2);
    }).detach();
    return in->to_json().dump();
}

// ---- POST/DELETE /api/instances/<id>/<action> ---------------------------------------
static std::string apiInstanceAction(const HttpRequest& rq) {
    std::string prefix = "/api/inst/";   // (kayıt anahtarıyla eşleşir)
    long id = id_from_path(rq, prefix);
    std::string action = action_from_path(rq, prefix);
    std::string err;
    bool ok = false;
    if (id <= 0) return "{\"err\":\"gecersiz id\"}";
    if (action == "newuser") ok = g_mgr->newuser(id, err);
    else if (action == "refresh") ok = g_mgr->refresh(id, err);
    else if (action == "check") ok = g_mgr->check_exit_ip(id, err);
    else if (action == "settings") {
        J b = parse_body(rq);
        ok = g_mgr->update_settings(id, b.s("capture"), (int)b.i("max_procs", 1),
                                    b.s("label"), err);
    } else if (action == "" && rq.method == "DELETE") return g_mgr->destroy(id, err) ?
        "{\"ok\":true}" : "{\"err\":\"" + err + "\"}";
    else if (action == "attach") {
        J b = parse_body(rq);
        long pid = b.i("pid", 0);
        ok = g_mgr->attach(id, pid, b.s("name", "pid:" + std::to_string(pid)), err);
    } else if (action == "detach") {
        J b = parse_body(rq);
        long pid = b.i("pid", 0);
        ok = g_mgr->detach(id, pid, err);
    } else if (action == "launch") {
        J b = parse_body(rq);
        long pid = g_mgr->launch(id, b.s("exe"), b.s("args"), err);
        J o = J::makeObj();
        if (pid > 0) { o["pid"] = pid; return o.dump(); }
        o["err"] = err;
        return o.dump();
    } else return "{\"err\":\"bilinmeyen eylem: " + action + "\"}";
    if (ok) {
        if (auto* in = g_mgr->find(id)) return in->to_json().dump();
        return "{\"ok\":true}";
    }
    J e = J::makeObj();
    e["err"] = err;
    return e.dump();
}

// ---- GET /api/processes --------------------------------------------------------------
static std::string apiProcesses(const HttpRequest& rq) {
    std::string q = gt::lower(rq.qp.count("q") ? rq.qp.at("q") : "");
    auto list = gt::list_processes();
    J arr = J::makeArr();
    for (auto& p : list) {
        if (!q.empty() &&
            gt::lower(p.name).find(q) == std::string::npos &&
            gt::lower(p.cmdline).find(q) == std::string::npos) continue;
        J o = J::makeObj();
        o["pid"] = p.pid;
        o["name"] = p.name;
        o["cmdline"] = p.cmdline.substr(0, 160);
        arr.push(o);
    }
    J o = J::makeObj();
    o["processes"] = arr;
    return o.dump();
}

// ---- POST /api/global ------------------------------------------------------------------
static std::string apiGlobal(const HttpRequest& rq) {
    J b = parse_body(rq);
    bool on = b.bl("on");
    int id = (int)b.i("id");
    std::string err;
    bool ok = g_mgr->set_global(on, id, err);
    J o = J::makeObj();
    o["ok"] = ok;
    o["err"] = err;
    return o.dump();
}

// ---- GET /api/monitor/connections ----------------------------------------------------
static std::string apiMonitorConnections(const HttpRequest&) {
    J arr = J::makeArr();
    for (auto& c : g_mon->snapshot()) {
        J o = J::makeObj();
        o["pid"] = c.pid;
        o["pname"] = c.pname;
        o["local"] = c.local;
        o["remote"] = c.remote;
        o["port"] = c.rport;
        o["via"] = c.via;
        arr.push(o);
    }
    J o = J::makeObj();
    o["connections"] = arr;
    return o.dump();
}

// ---- GET /api/monitor/events ----------------------------------------------------------
static std::string apiMonitorEvents(const HttpRequest& rq) {
    size_t since = rq.qp.count("since") ? (size_t)atoll(rq.qp.at("since").c_str()) : 0;
    size_t total = 0;
    J arr = g_mon->events_json(since, 300, total);
    J o = J::makeObj();
    o["events"] = arr;
    o["seq"] = (long long)total;
    return o.dump();
}

// ---- GET /api/console -------------------------------------------------------------------
static std::string apiConsole(const HttpRequest& rq) {
    size_t since = rq.qp.count("since") ? (size_t)atoll(rq.qp.at("since").c_str()) : 0;
    size_t total = 0;
    auto lines = g_console->since(since, 300, &total);
    J arr = J::makeArr();
    for (auto& l : lines) arr.push(l);
    J o = J::makeObj();
    o["lines"] = arr;
    o["seq"] = (long long)total;
    return o.dump();
}

// ---- GET /api/users/<id> — kullanıcı gerçeklik geçmişi ---------------------------------
static std::string apiUserHistory(const HttpRequest& rq) {
    long id = id_from_path(rq, "/api/users/");
    auto* in = g_mgr->find(id);
    if (!in) return "{\"err\":\"instance yok\"}";
    std::string txt;
    J arr = J::makeArr();
    if (gt::read_file(in->user_log_path, txt)) {
        for (auto& line : gt::split(txt, '\n')) {
            line = gt::trim(line);
            if (line.empty()) continue;
            J e;
            if (J::parse(line, e)) arr.push(e);
        }
    }
    J o = J::makeObj();
    o["id"] = id;
    o["label"] = in->label;
    o["uuid8"] = in->uuid.substr(0, 8);
    o["events"] = arr;
    return o.dump();
}

// ---- GET /api/sources — ücretsiz proxy kaynakları (x1; http + socks) --------------------
static std::string apiSources(const HttpRequest& rq) {
    int limit = rq.qp.count("limit") ? atoi(rq.qp.at("limit").c_str()) : 60;
    if (limit <= 0 || limit > 1000) limit = 60;
    std::string type = rq.qp.count("type") ? gt::lower(rq.qp.at("type")) : "";
    std::string err;
    auto proxies = gt::sources_fetch(limit, err);
    if (!type.empty()) {
        std::vector<gt::SourceProxy> f;
        for (auto& p : proxies) if (p.type == type) f.push_back(p);
        proxies = f;
    }
    if ((size_t)limit < proxies.size()) proxies.resize((size_t)limit);
    J arr = J::makeArr();
    for (auto& p : proxies) {
        J o = J::makeObj();
        o["addr"] = p.addr;
        o["type"] = p.type;
        o["source"] = p.source;
        arr.push(o);
    }
    J o = J::makeObj();
    o["proxies"] = arr;
    o["total"] = (long long)arr.size();
    o["err"] = err;
    return o.dump();
}

// ---- GET /api/vpngate — ücretsiz kamu VPN geçişleri (SoftEther) -------------------------
static std::string apiVpngate(const HttpRequest& rq) {
    std::string country = rq.qp.count("country") ? gt::upper(rq.qp.at("country")) : "";
    int limit = rq.qp.count("limit") ? atoi(rq.qp.at("limit").c_str()) : 15;
    if (limit <= 0 || limit > 50) limit = 15;
    std::string err;
    auto servers = gt::vpngate_fetch(country, limit, err);
    J arr = J::makeArr();
    for (auto& s : servers) {
        J o = J::makeObj();
        o["hostname"] = s.hostname;
        o["country"] = s.country_short;
        o["country_long"] = s.country_long;
        o["speed"] = gt::vpngate_speed_mbps(s.speed_bps);
        o["ping_ms"] = (long long)s.ping_ms;
        o["score"] = (long long)s.score;
        o["sessions"] = (long long)s.sessions;
        o["uptime_days"] = (long long)s.uptime_days;
        o["ovpn_b64"] = s.ovpn_b64;            // kutu oluşturmada kullanılır
        arr.push(o);
    }
    J o = J::makeObj();
    o["servers"] = arr;
    o["err"] = err;
    return o.dump();
}

// ---- kayıt -----------------------------------------------------------------------
void registerRoutes(HttpServer& srv, gt::InstanceManager* mgr, gt::Monitor* mon) {
    g_mgr = mgr;
    g_mon = mon;
    srv.routes["/api/status"]               = apiStatus;
    srv.routes["/api/countries"]            = apiCountries;
    srv.routes["/api/enc"]                  = apiEnc;             // şifreleme kataloğu
    srv.routes["/api/instances"]            = apiInstances;      // GET
    srv.routes["/api/instances_create"]     = apiCreateInstance;  // POST
    srv.routes["/api/global"]               = apiGlobal;
    srv.routes["/api/processes"]            = apiProcesses;
    srv.routes["/api/monitor/connections"]  = apiMonitorConnections;
    srv.routes["/api/monitor/events"]       = apiMonitorEvents;
    srv.routes["/api/console"]              = apiConsole;
    srv.routes["/api/sources"]             = apiSources;
    srv.routes["/api/vpngate"]             = apiVpngate;
    srv.routes["/api/users/"]              = apiUserHistory;     // /api/users/<id>

    // /api/instances/<id>/<action> aileleri (önek yönlendirme)
    srv.routes["/api/inst/"]               = apiInstanceAction;  // /api/inst/<id>/<action>
}

} // namespace ui
