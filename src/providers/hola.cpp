// ============================================================================
//  GOD_SSS_TIER_VPN — src/providers/hola.cpp
// ============================================================================
#include "providers/hola.h"
#include "core/json.h"
#include "core/log.h"
#include "core/util.h"
#include "net/httpclient.h"
#include "net/net.h"
#include "net/tls.h"
#include <cmath>
#include <thread>

namespace gt {

static const char* HOLA_CCGI = "https://client.hola.org/client_cgi/";
static const char* HOLA_EXT_VER = "1.0";       // 999.999.999 reddediliyor; "1.0" geçerli

static bool inet_addr_ok(const std::string& s) {
    // basit IPv4 biçim denetimi
    int dots = 0;
    for (char c : s) {
        if (c == '.') dots++;
        else if (c < '0' || c > '9') return false;
    }
    return dots == 3;
}

HolaIdentity hola_acquire(const std::string& country, int timeout_s) {
    HolaIdentity out;
    out.country = country;
    std::string last_err;

    for (int uuid_try = 0; uuid_try < 4; uuid_try++) {
        std::string u = uuid_hex();
        std::vector<std::pair<std::string, std::string>> hdrs;

        // ---- background_init (login=1) ----
        std::string post = "login=1&ver=" + url_encode(HOLA_EXT_VER);
        auto r = net::http_post_form(std::string(HOLA_CCGI) + "background_init?uuid=" + u,
                                     post, timeout_s);
        if (!r.ok() || r.body.empty()) {
            last_err = "background_init HTTP " + std::to_string(r.status) + " " + r.err;
            gt::LogWarn("[hola] background_init başarısız (%s) — 3sn sonra yeni uuid",
                        last_err.c_str());
            std::this_thread::sleep_for(std::chrono::seconds(3));
            continue;
        }
        J init;
        if (!J::parse(r.body, init)) {
            last_err = "background_init JSON çözülemedi";
            std::this_thread::sleep_for(std::chrono::seconds(3));
            continue;
        }
        if (init.bl("blocked")) {
            last_err = "blocked=true (IP geçici bayraklı)";
            gt::LogWarn("[hola] blocked=true — 8sn beklenip yeni uuid denenecek");
            std::this_thread::sleep_for(std::chrono::seconds(8));
            continue;
        }
        std::string session_key = init.s("key");
        std::string ext_ver = init.s("ver", HOLA_EXT_VER);
        if (session_key.empty()) {
            last_err = "background_init 'key' yok";
            std::this_thread::sleep_for(std::chrono::seconds(3));
            continue;
        }

        // ---- zgettunnels (boş yanıt normaldir — Go aracı gibi backoff'lu retry) ----
        for (int attempt = 0; attempt < 6; attempt++) {
            std::string qs = "country=" + url_encode(country) + "&limit=3&ext_ver=" +
                             url_encode(ext_ver) + "&browser=chrome&product=cws&uuid=" + u +
                             "&session_key=" + url_encode(session_key) +
                             "&is_premium=0&ping_id=" + ping_id_str();
            auto r2 = net::http_get(std::string(HOLA_CCGI) + "zgettunnels?" + qs, timeout_s);
            if (r2.ok() && !r2.body.empty() && r2.body != "{}") {
                J tun;
                if (J::parse(r2.body, tun) && tun.has("ip_list")) {
                    out.uuid = u;
                    out.login = "user-uuid-" + u + "-is_prem-0";
                    out.agent_key = tun.s("agent_key");
                    out.session_key = session_key;
                    out.ext_ver = ext_ver;
                    // uç noktalar: http protokollü zagent'ler
                    const J* proto = tun.find("protocol");
                    const J* ports = tun.find("port");
                    int trial_port = ports ? (int)ports->i("trial", 22225) : 22225;
                    if (out.agent_key.empty()) {
                        last_err = "agent_key yok";
                        break;
                    }
                    for (auto& kv : tun.obj) {
                        if (kv.first == "ip_list") {
                            for (auto& hip : kv.second.obj) {
                                std::string pr = "http";
                                if (proto) {
                                    const J* ph = proto->find(hip.first);
                                    if (ph && ph->t == J::STR) pr = lower(ph->str);
                                }
                                if (pr != "http") continue;
                                HolaEndpoint e;
                                e.host = hip.first;
                                e.ip = hip.second.t == J::STR ? hip.second.str : "";
                                e.port = trial_port;
                                std::string nerr;
                                if (e.ip.empty() || inet_addr_ok(e.ip.c_str()) == false)
                                    e.ip = net::resolve_ipv4(e.host, nerr);
                                out.endpoints.push_back(e);
                            }
                        }
                    }
                    if (!out.endpoints.empty()) {
                        out.ok = true;
                        return out;
                    }
                    last_err = "uygun http zagent yok";
                } else {
                    last_err = "zgettunnels gövdesi bozuldu";
                }
            } else {
                last_err = "zgettunnels boş yanıt (HTTP " + std::to_string(r2.status) + ")";
            }
            double back = 2.0 + rand01() * 2.0;
            std::this_thread::sleep_for(std::chrono::milliseconds((int)(back * 1000)));
        }
    }
    out.err = "Hola kimlik akışı başarısız — son hata: " + last_err +
              " (IP geçici bayraklanmış olabilir; 1-2 dk sonra tekrar deneyin)";
    return out;
}

// tek deneme: SNI modu + auth büyük/küçük + absolute-form GET
static std::string exit_ip_once(const HolaEndpoint& ep, const std::string& login,
                                const std::string& agent_key, bool use_sni,
                                bool flip_auth, int timeout_s) {
    net::TlsStream stream;
    std::string err;
    std::string sni = use_sni ? ep.host : "";
    if (!stream.connect(ep.ip, ep.port, sni, timeout_s, err)) return "";

    std::string ah = basic_auth(login, agent_key);
    if (flip_auth && ah.compare(0, 6, "Basic ") == 0) ah = "basic " + ah.substr(6);

    std::string req =
        "GET http://ip-api.com/json/?fields=query,country,countryCode,isp HTTP/1.1\r\n"
        "Host: ip-api.com\r\n"
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36\r\n"
        "Accept: */*\r\n"
        "Proxy-Authorization: " + ah + "\r\n"
        "Connection: close\r\n\r\n";
    if (stream.write(req.data(), req.size()) < 0) return "";

    std::string raw;
    char buf[8192];
    int n;
    while ((n = stream.read(buf, sizeof buf)) > 0) raw.append(buf, (size_t)n);
    size_t hh = raw.find("\r\n\r\n");
    if (hh == std::string::npos) return "";
    std::string head = raw.substr(0, hh);
    std::string body = raw.substr(hh + 4);
    if (head.find(" 200 ") == std::string::npos) return "";
    // chunked olabilir
    if (raw.find("chunked") != std::string::npos || raw.find("Chunked") != std::string::npos) {
        std::string dechunked;
        size_t pos = 0;
        while (pos + 2 <= body.size()) {
            size_t eol = body.find("\r\n", pos);
            if (eol == std::string::npos) break;
            long chunk = strtol(body.substr(pos, eol - pos).c_str(), nullptr, 16);
            if (chunk <= 0) break;
            size_t data = eol + 2;
            if (data + (size_t)chunk > body.size()) chunk = (long)(body.size() - data);
            dechunked.append(body, data, (size_t)chunk);
            pos = data + (size_t)chunk + 2;
        }
        body = dechunked;
    }
    return body;
}

// ----------------------------------------------------------------------------
//  Çıkış IP sorgusu — absolute-form GET (CONNECT'e gerek yok; 80 portu trial
//  proxy'de CONNECT ile reddediliyor — freevpn.py'de kanıtlanmış yol).
//  Deneme matrisi: [SNI yok, SNI var] x [Basic, basic] — freevpn.py'deki
//  hide_sni + _flip_auth_scheme kombinasyonunun aynısı.
// ----------------------------------------------------------------------------
std::string hola_exit_ip_json(const HolaEndpoint& ep, const std::string& login,
                              const std::string& agent_key, int timeout_s) {
    for (int use_sni = 0; use_sni <= 1; use_sni++)
        for (int flip = 0; flip <= 1; flip++) {
            std::string body = exit_ip_once(ep, login, agent_key, use_sni == 1,
                                            flip == 1, timeout_s);
            if (!body.empty()) {
                J chk;
                if (J::parse(body, chk) && chk.has("query")) return body;
            }
        }
    J e = J::makeObj();
    e["status"] = "err";
    e["err"] = "cikis ip sorgusu basarisiz (tum SNI/auth varyantlari denendi)";
    return e.dump();
}

} // namespace gt
