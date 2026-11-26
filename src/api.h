// ============================================================================
//  GOD_SSS_TIER_VPN — src/api.h
// ============================================================================
#pragma once
#include "httpserver.h"
#include <string>

namespace gt { class InstanceManager; class Monitor; }

namespace ui {
class HttpServer; class ConsoleBuffer;
void registerRoutes(HttpServer& srv, gt::InstanceManager* mgr, gt::Monitor* mon);
void uiSetConsole(ConsoleBuffer* c);
}

// ----------------------------------------------------------------------------
//  API uçları (hepsi /api/ altında, JSON):
//    GET  /api/status              motor durumu + sayaçlar
//    GET  /api/countries            Hola ülke listesi (246 kod)
//    GET  /api/instances            instance listesi (canlı istatistikle)
//    POST /api/instances            {provider, country, label}  -> oluştur
//    POST /api/instances/<id>/newuser     NEW USER MODE (uuid takası)
//    POST /api/instances/<id>/refresh     zgettunnels yenile (aynı uuid)
//    POST /api/instances/<id>/check       çıkış IP kontrolü (tünel sağlık)
//    DELETE /api/instances/<id>            kapat
//    GET  /api/processes           süreç listesi (search_textbox kaynağı)
//    POST /api/instances/<id>/attach      {pid}
//    POST /api/instances/<id>/detach     {pid}
//    POST /api/instances/<id>/launch     {exe, args}  (hook ile başlat)
//    POST /api/global              {on, id}  tüm PC (WinInet / env)
//    GET  /api/monitor/connections son tarama (PID bazlı TCP tablosu)
//    GET  /api/monitor/events?since=   debug terminal olay akışı
//    GET  /api/console?since=      motor log akışı (WebGUI konsolu)
//    GET  /api/users/<id>          kullanıcı gerçeklik geçmişi (JSONL oku)
//    GET  /api/sources?q=&limit=   ücretsiz proxy kaynağı listesi (x1)
// ----------------------------------------------------------------------------
