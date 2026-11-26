// ============================================================================
//  GOD_SSS_TIER_VPN — src/httpserver.cpp
// ============================================================================
#include "httpserver.h"
#include "net/net.h"
#include "core/log.h"
#include "core/util.h"
#include "web_embedded.h"          // gömülü WebUI (tek .exe — HTML içinde)
#include <cstring>
#include <thread>

namespace ui {

static std::string mime_of(const std::string& name) {
    size_t dot = name.rfind('.');
    std::string ext = dot == std::string::npos ? "" : name.substr(dot + 1);
    if (ext == "html" || ext == "htm") return "text/html; charset=utf-8";
    if (ext == "js")    return "application/javascript; charset=utf-8";
    if (ext == "css")  return "text/css; charset=utf-8";
    if (ext == "json") return "application/json; charset=utf-8";
    if (ext == "svg")  return "image/svg+xml";
    if (ext == "png")  return "image/png";
    if (ext == "ico")  return "image/x-icon";
    return "application/octet-stream";
}

std::string HttpServer::readFileOr(const std::string& rel, std::string& ct) {
    // yol dolaşımı engelle
    if (rel.find("..") != std::string::npos) return "";
    std::string data;
    ct = mime_of(rel);
    if (!gt::read_file(webRoot + "/" + rel, data))
        data = ui::embedded::get(rel);        // disk yoksa: exe içine gömülü kopya
    return data;
}

bool HttpServer::serve(uint16_t port) {
    std::string err;
    sock_t lst = gt::net::tcp_listen(port, err);
    if (lst == GT_BAD_SOCK) {
        gt::LogErr("dinleme hatasi %u: %s", (unsigned)port, err.c_str());
        return false;
    }
    gt::LogOk("WebGUI dinliyor: http://127.0.0.1:%u/", (unsigned)port);
    while (true) {
        struct sockaddr_in ca;
        socklen_t cl = sizeof ca;
        sock_t c = accept(lst, (struct sockaddr*)&ca, &cl);
        if (c == GT_BAD_SOCK) continue;
        gt::net::set_sock_timeout(c, 15);
        std::thread([this, c] {
            char buf[16384];
            std::string req;
            // başlık + gövde (Content-Length kadar)
            size_t headEnd = std::string::npos;
            while (req.size() < 65536) {
                int n = gt::net::recv_some(c, buf, sizeof buf);
                if (n <= 0) break;
                req.append(buf, (size_t)n);
                headEnd = req.find("\r\n\r\n");
                if (headEnd != std::string::npos) break;
            }
            if (headEnd == std::string::npos) { gt::net::close_sock(c); return; }
            std::string head = req.substr(0, headEnd);
            size_t sp1 = head.find(' ');
            size_t sp2 = head.find(' ', sp1 + 1);
            HttpRequest rq;
            rq.method = head.substr(0, sp1);
            std::string target = head.substr(sp1 + 1, sp2 - sp1 - 1);
            size_t qm = target.find('?');
            rq.path = qm == std::string::npos ? target : target.substr(0, qm);
            rq.query = qm == std::string::npos ? "" : target.substr(qm + 1);
            for (auto& kv : gt::split(rq.query, '&')) {
                size_t eq = kv.find('=');
                if (eq != std::string::npos)
                    rq.qp[kv.substr(0, eq)] = kv.substr(eq + 1);
            }
            // content-length
            size_t cl = 0;
            for (auto& line : gt::split(head, '\n')) {
                if (line.size() > 16 &&
                    (line.compare(0, 14, "Content-Length") == 0 ||
                     line.compare(0, 14, "content-length") == 0)) {
                    size_t colon = line.find(':');
                    if (colon != std::string::npos)
                        cl = (size_t)atol(gt::trim(line.substr(colon + 1)).c_str());
                }
            }
            if (cl > 0 && cl < 1024 * 1024) {
                rq.body = req.substr(headEnd + 4);
                while (rq.body.size() < cl) {
                    int n = gt::net::recv_some(c, buf, sizeof buf);
                    if (n <= 0) break;
                    rq.body.append(buf, (size_t)n);
                }
                if (rq.body.size() > cl) rq.body.resize(cl);
            }

            // --- yanıt ---
            std::string out;
            std::string ct = "application/json; charset=utf-8";
            bool isBody = true;
            auto it = routes.find(rq.path);
            if (it == routes.end()) {
                // önek yönlendirme: "/api/inst/" gibi '/' ile biten anahtarlar
                for (auto& kv : routes) {
                    if (kv.first.size() > 1 && kv.first.back() == '/' &&
                        rq.path.compare(0, kv.first.size(), kv.first) == 0) {
                        it = routes.find(kv.first);
                        break;
                    }
                }
            }
            if (it != routes.end()) {
                out = it->second(rq);
                // handler'lar ham JSON döndürür
            } else if (rq.path == "/" || rq.path == "/index.html") {
                out = readFileOr("index.html", ct);
            } else {
                std::string rel = rq.path.substr(1);
                out = readFileOr(rel, ct);
                if (out.empty() && !rel.empty()) {
                    // 404 JSON — API yolu mu?
                    if (rel.rfind("api/", 0) == 0) {
                        out = "{\"err\":\"bilinmeyen api\"}";
                        ct = "application/json; charset=utf-8";
                    } else {
                        out = "404 — MVP_GOD_SSS_TIER";
                        ct = "text/plain; charset=utf-8";
                    }
                }
            }
            char hdr[256];
            snprintf(hdr, sizeof hdr,
                     "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
                     "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
                     ct.c_str(), out.size());
            gt::net::send_all(c, hdr, strlen(hdr));
            if (isBody && !out.empty())
                gt::net::send_all(c, out.data(), out.size());
            gt::net::close_sock(c);
        }).detach();
    }
    return true;
}

} // namespace ui
