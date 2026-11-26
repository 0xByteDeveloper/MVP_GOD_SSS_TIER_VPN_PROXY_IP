// ============================================================================
//  GOD_SSS_TIER_VPN — src/httpserver.h
//  localhost HTTP sunucusu (MVP_GOD_SSS_TIER_SIMULATOR_UI http_server deseni):
//    + POST gövde okuma (JSON API)
//    + web/ dizininden statik dosya servisi
//    + halka tamponlu konsol (debug terminali kaynağı)
// ============================================================================
#pragma once
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace ui {

class ConsoleBuffer {
public:
    void push(const std::string& line) {
        std::lock_guard<std::mutex> g(m_);
        lines_.push_back(line);
        seq_++;
        if (lines_.size() > 4000) lines_.erase(lines_.begin(), lines_.begin() + 2000);
    }
    // since'ten itibaren yeni satırlar; toplam sayıyı da döner
    std::vector<std::string> since(size_t idx, size_t max, size_t* total) {
        std::lock_guard<std::mutex> g(m_);
        if (total) *total = seq_;
        std::vector<std::string> out;
        for (size_t i = idx; i < seq_ && out.size() < max; i++) {
            size_t pos = i - (seq_ - lines_.size());
            if (pos < lines_.size()) out.push_back(lines_[pos]);
        }
        return out;
    }
    size_t seq() {
        std::lock_guard<std::mutex> g(m_);
        return seq_;
    }
private:
    std::mutex m_;
    std::deque<std::string> lines_;
    size_t seq_ = 0;
};

struct HttpRequest {
    std::string method;
    std::string path;                       // ? öncesi
    std::string query;                      // ? sonrası
    std::map<std::string, std::string> qp;  // çözülmüş query parametreleri
    std::string body;                       // POST gövdesi
};

class HttpServer {
public:
    using Handler = std::function<std::string(const HttpRequest&)>; // JSON döner
    // path -> handler (statik dosyalar hariç)
    std::map<std::string, Handler> routes;
    std::string webRoot = "web";            // statik dosyalar
    ConsoleBuffer* console = nullptr;

    bool serve(uint16_t port);              // döngü — dönmeyen (Ctrl+C)
private:
    std::string readFileOr(const std::string& rel, std::string& ct);
};

} // namespace ui
