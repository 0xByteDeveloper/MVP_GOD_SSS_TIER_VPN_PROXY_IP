// ============================================================================
//  MVP_GOD_SSS_TIER — src/engine/instance.h
//  Instance = "IP değiştiren kullanıcı" (kimlik + yerel router portu + kutu)
//    * x1 user hola, x2 user hola ... — hepsi eşzamanlı, her birinin kendi
//      kimliği, ülkesi, portu ve GERÇEKLİK GEÇMİŞİ (user log) vardır.
//    * New User Mode: ücretsiz süre bitince uuid yenilenir (aynı portta
//      upstream takası — kopmadan; bağlantılar yenilenir).
//    * KUTU modeli: UI'daki kare kutular bunlardır — boş kutu = instance yok;
//      AYARLAR ile doldurulur (sağlayıcı/ülke/şifreleme/oto-yakalama).
// ============================================================================
#pragma once
#include "../core/json.h"
#include "../providers/hola.h"
#include "engine/router.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace gt {

// ---- oluşturma ayarları (AYARLAR modalından gelen her şey) -------------------
struct CreateOpts {
    std::string provider;         // hola | plain | socks | tor | chain | vpngate
    std::string country;          // hola: TR/US/... ; vpngate: 2 harf; diğerleri bilgi
    std::string label;            // x1, x2 ... (boşsa otomatik)
    std::string addr;             // plain/socks manuel "1.2.3.4:8080" (boşsa otomatik)
    std::string enc;              // auto | tls13 | tls12 | plain | chain2 | chain3 | tor3
    std::string capture;          // "metin2client.exe, game.exe" — oto-yakalama desenleri
    int capture_mode = 0;         // 0=MOD1 ilk N (kapasiteye kadar, doluysa sıradaki kutu)
                                  // 1=MOD2 HEPSİ (o adı taşıyan tüm süreçler — açık olanlar bile)
    int max_procs = 1;            // bu kutuya aynı anda bağlanacak süreç üst sınırı (MOD1)
    std::vector<std::string> hops;// manuel zincir hops "ip:port" (chain için; boşsa otomatik)
    // vpngate: seçilen sunucu (ayarlama sırasında API'den alınan satır)
    std::string vpngate_hostname;
    std::string vpngate_ovpn_b64;
};

struct Instance {
    int id = 0;
    std::string provider;               // "hola" | "plain" | "socks" | "tor" | "chain" | "vpngate"
    std::string country;                 // hola ülke kodu (TR, US, DE...)
    std::string label;                  // kullanıcı etiketi: x1, x2 ...
    std::string enc_mode;               // şifreleme seçimi (UI kutusunda gösterilir)
    // hola kimliği
    std::string uuid, login, agent_key, session_key, ext_ver;
    std::vector<HolaEndpoint> endpoints;
    // router
    std::unique_ptr<Router> router;
    int local_port = 0;
    // durum
    std::string status = "INIT";        // RUNNING | ERROR | STOPPING | OVPN
    std::string exit_ip, exit_country, exit_isp;
    std::string upstream_addr;           // plain/socks: ip:port (kalıcılık)
    std::vector<std::string> hops_addrs; // zincir hops (kalıcılık)
    std::string vpngate_hostname;        // vpngate sunucu host adı
    std::string vpngate_ovpn_path;       // data/vpngate/<label>.ovpn
    uint64_t created_ms = 0, last_newuser_ms = 0, refresh_count = 0;
    std::vector<long> attached_pids;
    std::vector<std::string> attached_names;
    // oto-yakalama (kutu ayarları — monitor bunları izler)
    std::string capture;                 // desen listesi (virgülle)
    int capture_mode = 0;                // 0=MOD1 ilk N · 1=MOD2 hepsi (açıksa bile)
    int max_procs = 1;
    // gerçeklik profili (uuid'den türetilmiş "insan gibi" kullanım deseni)
    int profile_activity = 0;            // 0..100 (%)
    int profile_session_min = 0;         // dakika
    std::string profile_style;           // "gece kullanıcı", "sabit ofis"...

    std::string user_log_path;
    mutable std::mutex mu;

    J to_json() const;
    void user_log_append(const std::string& line) const;   // data/logs/users/*.jsonl
    // zincir upstream'ini yeniden kur (kalıcılık geri yüklemesi / ayar değişimi)
    bool rebuild_upstream(std::string& err);
};

class InstanceManager {
public:
    explicit InstanceManager(std::string data_dir);

    // yaşam döngüsü — CreateOpts ile: sağlayıcı + ülke + şifreleme + oto-yakalama
    Instance* create(const CreateOpts& o, std::string& err);
    bool newuser(int id, std::string& err);           // NEW USER MODE
    bool destroy(int id, std::string& err);
    bool check_exit_ip(int id, std::string& err);     // tünel sağlık (TÜM sağlayıcılar)
    bool refresh(int id, std::string& err);          // zgettunnels yenile (aynı uuid)
    bool update_settings(int id, const std::string& capture, int max_procs,
                         const std::string& label, std::string& err);
    bool update_settings(int id, const std::string& capture, int max_procs,
                         int capture_mode, const std::string& label, std::string& err);

    // process yönlendirme
    bool attach(int id, long pid, const std::string& name, std::string& err);
    bool detach(int id, long pid, std::string& err);
    long launch(int id, const std::string& exe, const std::string& args,
                std::string& err);                    // hook ile başlat (pid döner)

    // sistem geneli (Windows: WinInet proxy; Linux: alt süreçler env)
    bool set_global(bool on, int id, std::string& err);
    bool global_on() const { return global_on_; }

    std::vector<Instance*> list();
    Instance* find(int id);
    size_t count();

    void load();                                        // data/instances.json
    void save();                                        // kalıcılık
    const std::string& data_dir() const { return data_dir_; }
    uint64_t boot_ms = 0;

private:
    std::string data_dir_;
    std::vector<std::unique_ptr<Instance>> inst_;
    mutable std::mutex mu_;
    int next_id_ = 1;
    int next_port_ = 22000;
    bool global_on_ = false;
    int global_inst_ = 0;
    std::string derive_label(const std::string& provider);   // x1, x2...
};

} // namespace gt
