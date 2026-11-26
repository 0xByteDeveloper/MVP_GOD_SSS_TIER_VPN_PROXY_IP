// ============================================================================
//  MVP_GOD_SSS_TIER — src/main.cpp
//  Komut satırı:
//    mvp_god_sss_tier serve [port]        WebGUI + motor (C++ derin işlemler)
//    mvp_god_sss_tier selftest [ülke]      başsız uçtan uca Hola testi
//    mvp_god_sss_tier countries            ülke kodlarını listele
//  Çalışma dizini web/ + data/ (instances.json, logs/) üretir.
//  HTML arayüz exe içine gömülüdür (src/web_embedded.h) — web/ yanında
//  olmasa da WebGUI çalışır.
// ============================================================================
#include "api.h"
#include "httpserver.h"
#include "core/log.h"
#include "core/util.h"
#include "engine/instance.h"
#include "engine/monitor.h"
#include "providers/countries.h"

#include <cstring>
#include <string>
#include <thread>
#ifndef _WIN32
#  include <signal.h>
#endif

using namespace ui;
using gt::InstanceManager;
using gt::Monitor;
using gt::Instance;

static int CmdCountries() {
    gt::LogLine(gt::LogColor::Cyan, "== Hola ülkeleri (%zu) ==", gt::HolaCountries().size());
    size_t k = 0;
    for (auto& c : gt::HolaCountries()) {
        gt::Log("  %-3s %s", c.code, c.name);
        if (++k >= 30) { gt::Log("  ... (tamamı WebGUI → /api/countries)"); break; }
    }
    return 0;
}

static int CmdSelftest(const std::string& country) {
    gt::LogLine(gt::LogColor::Magenta,
        "MVP_GOD_SSS_TIER — başsız uçtan uca Hola denemesi (ülke=%s)\n", country.c_str());
    InstanceManager mgr("data");
    std::string err;
    gt::LogLine(gt::LogColor::Cyan, "-- 1) yeni kullanıcı (uuid) + zgettunnels --");
    gt::CreateOpts o;
    o.provider = "hola";
    o.country = country;
    o.label = "selftest";
    Instance* in = mgr.create(o, err);
    if (!in) {
        gt::LogErr("instance kurulamadı: %s", err.c_str());
        return 2;
    }
    gt::LogLine(gt::LogColor::Cyan, "-- 2) tünel sağlık kontrolü (absolute-form GET) --");
    if (!mgr.check_exit_ip(in->id, err)) {
        gt::LogErr("çıkış IP alınamadı: %s", err.c_str());
        return 3;
    }
    gt::LogLine(gt::LogColor::Cyan, "-- 3) yerel SOCKS5 dinleyici --");
    gt::LogOk("127.0.0.1:%d dinliyor — test: curl --socks5 127.0.0.1:%d http://ip-api.com/json",
              in->local_port, in->local_port);
    gt::Log("\nSONUÇ: kullanıcı=%s  çıkış=%s (%s)  port=%d\n",
            in->uuid.substr(0, 8).c_str(), in->exit_ip.c_str(),
            in->exit_country.c_str(), in->local_port);
    mgr.destroy(in->id, err);
    return 0;
}

int main(int argc, char** argv) {
    gt::net::init();
    gt::LogInit();
#ifndef _WIN32
    signal(SIGCHLD, SIG_IGN);   // launch'ın çocuklarını otomatik temizle (zombie yok)
#endif

    std::string cmd = argc > 1 ? argv[1] : "serve";
    uint16_t port = 7787;

    if (cmd == "countries") return CmdCountries();
    if (cmd == "selftest") return CmdSelftest(argc > 2 ? argv[2] : "TR");

    // serve [port]
    if (argc > 2) port = (uint16_t)strtoul(argv[2], nullptr, 10);

    ConsoleBuffer con;
    uiSetConsole(&con);
    gt::LogSetSink([&con](const std::string& line) { con.push(line); });

    gt::LogLine(gt::LogColor::Magenta,
        "MVP_GOD_SSS_TIER — ücretsiz ip/proxy/vpn sistemi (Saf C++17 + WebGUI)\n");
    gt::Log("  Hola trial kimliği (uuid motorun ürettiği) | Tor/zincir | VPN Gate | SOCKS5 router | PID izleyici\n\n");

    InstanceManager mgr("data");
    mgr.load();                                 // önceki instance'lar geri gelir
    Monitor mon(&mgr, 2000);
    mon.start();                                // VPN KULLANMASAK BİLE pid->IP tablosu

    HttpServer srv;
    srv.webRoot = "web";
    srv.console = &con;
    registerRoutes(srv, &mgr, &mon);

    gt::Log("\nTarayıcıda aç: http://127.0.0.1:%u/\n", (unsigned)port);

    if (!srv.serve(port)) {
        gt::LogErr("sunucu başlatılamadı (port %u meşgul?)", (unsigned)port);
        return 2;
    }
    return 0;
}
