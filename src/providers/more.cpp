// ============================================================================
//  MVP_GOD_SSS_TIER — src/providers/more.cpp
// ============================================================================
#include "providers/more.h"
#include "core/json.h"
#include "core/log.h"
#include "core/util.h"
#include "net/httpclient.h"
#include "net/net.h"
#include <algorithm>
#include <cstring>

namespace gt {

// ---- VPN Gate ----------------------------------------------------------------
std::vector<VpnGateServer> vpngate_fetch(const std::string& country, int limit,
                                         std::string& err) {
    std::vector<VpnGateServer> out;
    std::string want = country.empty() ? "" : upper(country);

    // 1) resmi CSV (kullanıcı PC'inde — ev IP'sinde — çalışır)
    {
        auto r = net::http_get("http://www.vpngate.net/api/iphone/", 25);
        if (r.ok() && r.body.size() > 10000 &&
            r.body.find("hostname") != std::string::npos) {
            auto lines = split(r.body, '\n');
            std::map<std::string, size_t> col;
            {
                auto hdr = split(trim(lines[1]), ',');
                for (size_t i = 0; i < hdr.size(); i++) col[lower(trim(hdr[i]))] = i;
            }
            auto need = [&](const char* n) { return col.count(n) ? (int)col[n] : -1; };
            int i_host = need("hostname"), i_score = need("score"), i_ping = need("ping");
            int i_speed = need("speed"), i_cl = need("country_long"), i_cs = need("country_short");
            int i_ovpn = need("openvpn_configdata_base64"), i_sess = need("vpn_session");
            int i_up = need("uptime");
            if (i_host >= 0 && i_ovpn >= 0 && i_cs >= 0) {
                for (size_t k = 2; k < lines.size() && (int)out.size() < limit; k++) {
                    std::string ln = trim(lines[k]);
                    if (ln.empty()) continue;
                    if (ln.rfind("*END*", 0) == 0) break;
                    auto f = split(ln, ',');
                    auto getc = [&](int i) { return i >= 0 && (size_t)i < f.size() ? trim(f[i]) : ""; };
                    if (getc(i_host).empty() || getc(i_ovpn).empty()) continue;
                    VpnGateServer s;
                    s.hostname = getc(i_host);
                    s.ip = getc(i_host);
                    s.country_short = getc(i_cs);
                    s.country_long = getc(i_cl);
                    if (!want.empty() && upper(s.country_short) != want) continue;
                    s.score = atoll(getc(i_score).c_str());
                    s.ping_ms = atoll(getc(i_ping).c_str());
                    s.speed_bps = atoll(getc(i_speed).c_str());
                    s.sessions = atoll(getc(i_sess).c_str());
                    s.uptime_days = atoll(getc(i_up).c_str()) / 86400;
                    s.ovpn_b64 = getc(i_ovpn);
                    out.push_back(s);
                }
            }
        }
    }
    if (!out.empty()) {
        std::sort(out.begin(), out.end(), [](const VpnGateServer& a, const VpnGateServer& b) {
            return a.score > b.score;
        });
        return out;
    }

    // 2) GitHub aynası — veri merkezi IP'lerinde resmi API boş HTML döner;
    //    fdciabdul/Vpngate-Scraper-API saatlik anlık görüntü (JSON)
    {
        auto r = net::http_get(
            "https://raw.githubusercontent.com/fdciabdul/Vpngate-Scraper-API/main/json/data.json",
            30);
        if (!r.ok() || r.body.empty()) {
            err = "vpngate: resmi CSV ve ayna da erişilemedi (HTTP " +
                  std::to_string(r.status) + ")";
            return out;
        }
        J j;
        if (!J::parse(r.body, j)) { err = "vpngate ayna JSON çözülemedi"; return out; }
        const J* root = nullptr;
        if (j.t == J::ARR && !j.arr.empty() && j.arr[0].t == J::OBJ) root = &j.arr[0];
        else if (j.t == J::OBJ) root = &j;
        if (!root) { err = "vpngate ayna biçimi beklenmedik"; return out; }
        const J* servers = root->find("servers");
        if (!servers || servers->t != J::ARR) { err = "vpngate ayna 'servers' yok"; return out; }
        for (auto& s : servers->arr) {
            if ((int)out.size() >= limit) break;
            VpnGateServer v;
            v.hostname = s.s("hostname");
            v.ip = s.s("ip");
            v.country_short = s.s("countryshort");
            v.country_long = s.s("countrylong");
            v.score = atoll(s.s("score").c_str());
            v.ping_ms = atoll(s.s("ping").c_str());
            v.speed_bps = atoll(s.s("speed").c_str());
            v.sessions = atoll(s.s("numvpnsessions").c_str());
            v.uptime_days = atoll(s.s("uptime").c_str()) / 86400;
            v.ovpn_b64 = s.s("openvpn_configdata_base64");
            if (v.hostname.empty() || v.ovpn_b64.empty()) continue;
            if (!want.empty() && upper(v.country_short) != want) continue;
            out.push_back(v);
        }
    }
    std::sort(out.begin(), out.end(), [](const VpnGateServer& a, const VpnGateServer& b) {
        return a.score > b.score;
    });
    return out;
}

std::string vpngate_speed_mbps(long long speed_bps) {
    char b[32];
    snprintf(b, sizeof b, "%.1f Mbps", speed_bps / 1000000.0);
    return b;
}

// ---- Tor --------------------------------------------------------------------
int tor_detect_port() {
    // yalnızca TCP açık olması yetmez (sandbox'lar 9050'yi açık tutabilir) —
    // GERÇEK SOCKS5 el sıkışması doğrulanır: {5,1,0} → {5,0}
    for (int port : {9050, 9150}) {
        sock_t s = net::tcp_connect("127.0.0.1", port, 1);
        if (s == GT_BAD_SOCK) continue;
        net::set_sock_timeout(s, 2);
        unsigned char hi[3] = {5, 1, 0};
        unsigned char rep[2] = {0, 0};
        bool oksocks = net::send_all(s, hi, 3) >= 0 &&
                       net::recv_some(s, rep, 2) == 2 && rep[0] == 5 && rep[1] == 0;
        net::close_sock(s);
        if (oksocks) return port;
    }
    return 0;
}

// ---- kaynak listeleri ---------------------------------------------------------
std::vector<SourceProxy> sources_fetch(int per_source_limit, std::string& err) {
    struct Src { const char* url; const char* type; const char* name; };
    static const Src srcs[] = {
        {"https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/http.txt",  "http",   "monosans"},
        {"https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/socks5.txt", "socks5", "monosans"},
        {"https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/http.txt",       "http",   "speedx"},
        {"https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/socks4.txt",      "socks4", "speedx"},
        {"https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/socks5.txt",      "socks5", "speedx"},
        {"https://api.proxyscrape.com/v2/?request=displayproxies&protocol=http&timeout=10000&country=all&ssl=all&anonymity=all",
                                                                                       "http",   "proxyscrape"},
        {"https://api.proxyscrape.com/v2/?request=displayproxies&protocol=socks5&timeout=10000&country=all&ssl=all&anonymity=all",
                                                                                       "socks5", "proxyscrape"},
    };
    std::vector<SourceProxy> out;
    for (auto& sc : srcs) {
        auto r = net::http_get(sc.url, 20);
        if (!r.ok()) continue;
        int n = 0;
        for (auto& line : split(r.body, '\n')) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            // satır "ip:port" — ek önek/başlık yok
            if (line.find(':') == std::string::npos) continue;
            if (line.size() > 6 && line.find_first_not_of("0123456789.:") != std::string::npos)
                continue;                        // "socks5://..." gibi önekli satırları atla
            SourceProxy p;
            p.addr = line;
            p.type = sc.type;
            p.source = sc.name;
            out.push_back(p);
            if (++n >= per_source_limit) break;
        }
    }
    if (out.empty()) err = "kaynak listeleri boş geldi (ağ filtresi?)";
    return out;
}

std::vector<SourceProxy> pick_alive(const std::vector<SourceProxy>& from, int count) {
    std::vector<SourceProxy> out;
    int tried = 0;
    for (auto& p : from) {
        if ((int)out.size() >= count) break;
        if (p.type != "http" && p.type != "socks5") continue;
        if (++tried > 50) break;                 // fazlasını deneme — hızlı olsun
        size_t c = p.addr.rfind(':');
        if (c == std::string::npos) continue;
        std::string ip = p.addr.substr(0, c);
        int port = atoi(p.addr.substr(c + 1).c_str());
        sock_t s = net::tcp_connect(ip, port, 3);
        if (s == GT_BAD_SOCK) continue;
        bool usable = true;
        if (p.type == "socks5") {
            // SOCKS5 no-auth doğrulaması: {5,1,0} → {5,0} (auth isteyenler atlanır)
            net::set_sock_timeout(s, 3);
            unsigned char hi[3] = {5, 1, 0};
            unsigned char rep[2] = {9, 9};
            usable = net::send_all(s, hi, 3) >= 0 &&
                     net::recv_some(s, rep, 2) == 2 && rep[0] == 5 && rep[1] == 0;
        }
        net::close_sock(s);
        if (usable) out.push_back(p);
    }
    return out;
}

// ---- tünel sağlık kontrolü (SOCKS5 istemci + absolute-form GET) --------------
static std::string dechunk(const std::string& body) {
    std::string out;
    size_t pos = 0;
    while (pos + 2 <= body.size()) {
        size_t eol = body.find("\r\n", pos);
        if (eol == std::string::npos) break;
        long chunk = strtol(body.substr(pos, eol - pos).c_str(), nullptr, 16);
        if (chunk <= 0) break;
        size_t data = eol + 2;
        if (data + (size_t)chunk > body.size()) chunk = (long)(body.size() - data);
        out.append(body, data, (size_t)chunk);
        pos = data + (size_t)chunk + 2;
    }
    return out;
}

std::string socks_exit_ip_json(int local_port, int timeout_s) {
    auto attempt = [&]() -> std::string {
        sock_t c = net::tcp_connect("127.0.0.1", local_port, 3);
        if (c == GT_BAD_SOCK) return "";
        // SOCKS5 el sıkışma (no-auth) — selamlama 3 bayt: VER NMETHODS METHOD
        unsigned char hi[3] = {5, 1, 0};
        if (net::send_all(c, hi, 3) < 0) { net::close_sock(c); return ""; }
        unsigned char resp[2];
        if (net::recv_some(c, resp, 2) != 2 || resp[0] != 5 || resp[1] != 0) {
            net::close_sock(c); return "";
        }
        // CONNECT ip-api.com:80 (alan adı ATYP=3)
        const char* host = "ip-api.com";
        size_t hl = strlen(host);
        unsigned char req[300];
        size_t n = 0;
        req[n++] = 5; req[n++] = 1; req[n++] = 0; req[n++] = 3;
        req[n++] = (unsigned char)hl;
        memcpy(req + n, host, hl); n += hl;
        req[n++] = 0; req[n++] = 80;                       // port 80 (BIG endian)
        net::set_sock_timeout(c, timeout_s);
        if (net::send_all(c, req, n) < 0) { net::close_sock(c); return ""; }
        unsigned char rrep[26];
        if (net::recv_some(c, rrep, 4) != 4 || rrep[1] != 0) { net::close_sock(c); return ""; }
        // bağlı adresin kalan baytları (ATYP'e göre değişken) tüket
        if (rrep[3] == 1) { if (net::recv_some(c, rrep + 4, 6) != 6) { net::close_sock(c); return ""; } }
        else if (rrep[3] == 3) {
            unsigned char l; if (net::recv_some(c, &l, 1) != 1) { net::close_sock(c); return ""; }
            if (l && net::recv_some(c, rrep, l) != l) { net::close_sock(c); return ""; }
            if (net::recv_some(c, rrep, 2) != 2) { net::close_sock(c); return ""; }
        } else if (rrep[3] == 4) { if (net::recv_some(c, rrep + 4, 18) != 18) { net::close_sock(c); return ""; } }
        else { net::close_sock(c); return ""; }
        // absolute-form GET (80 portu — hola trial'daki kanıtlanmış yol)
        std::string get =
            "GET /json/?fields=query,country,countryCode,isp HTTP/1.1\r\n"
            "Host: ip-api.com\r\n"
            "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36\r\n"
            "Accept: */*\r\nConnection: close\r\n\r\n";
        if (net::send_all(c, get.data(), get.size()) < 0) { net::close_sock(c); return ""; }
        std::string raw;
        char buf[8192];
        int got;
        while ((got = net::recv_some(c, buf, sizeof buf)) > 0) raw.append(buf, (size_t)got);
        net::close_sock(c);
        size_t hh = raw.find("\r\n\r\n");
        if (hh == std::string::npos) return "";
        std::string head = raw.substr(0, hh);
        std::string body = raw.substr(hh + 4);
        if (head.find(" 200 ") == std::string::npos) return "";
        if (raw.find("chunked") != std::string::npos) body = dechunk(body);
        return body;
    };
    std::string body = attempt();
    J j;
    if (!body.empty() && J::parse(body, j) && j.has("query")) return body;
    J e = J::makeObj();
    e["status"] = "err";
    e["err"] = "tunel cikis ip sorgusu basarisiz (yerel router zinciri acilmadi)";
    return e.dump();
}

} // namespace gt
