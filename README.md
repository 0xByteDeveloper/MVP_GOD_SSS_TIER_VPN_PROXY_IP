# MVP_GOD_SSS_TIER — Ücretsiz IP / Proxy / VPN Sistemi

**Saf C++17 motoru + HTML yönetim arayüzü** — "derin işlemler C++'ta, yönetim
tarayıcıdan" (`http://127.0.0.1:7787/`) deseni; MVP_GOD_SSS_TIER_SIMULATOR_UI
web-yönetim mimarisinin birebir aynı yaklaşımı. **SIFIR üçüncü parti
çalışma-zamanı bağımlılığı**: Windows build'i yalnız işletim sistemi API'lerini
kullanır (WinHTTP + SChannel + WinSock) — virüs/telemetri riski taşıyan
indirilebilir kütüphane yok; kimlikleri (Hola uuid) motorun kendisi üretir,
istatistik/telemetri sunucusu yok.

```
   ┌────────────────────────────────────────────────────────────────────────────┐
   │ [MVP_GOD_SSS_TIER]  "+ KUTU EKLE"        3 instance · 6 kutu · motor C++17 │
   ├──────────────┬──────────────────────────────────────────┬──────────────────┤
   │ INSTANCE     │ [KUTU İZGARASI — kare kutular]            │ Motor durumu     │
   │ KUTULARI     │  ┌─────────┐ ┌─────────┐ ┌─────────┐      │ Tor yerel:...   │
   │  x1 oyuncu1  │  │ BOŞ     │ │ x1 TR   │ │ x2 TR   │      │ TÜM PC modu     │
   │  x2 oyuncu2  │  │ AYARLAR │ │34.105.. │ │34.105.. │      │ Şifreleme/      │
   │  vg1 OVPN    │  └─────────┘ │metin2🎯 │ │metin2🎯 │      │ kurallar        │
   │ (kare kutu:  │  ┌─────────┐ └─────────┘ └─────────┘      │                  │
   │  default BOŞ)│  │ BOŞ ...  │ OTO-YAKALA: metin2client.exe │                  │
   ├──────────────┴──────────┬───────────────┬─────────────────┴──────────────────┤
   │ Instance | Süreç Yönlendir (search_textbox) | Debug Terminal | VPN Gate ... │
   ├────────────────────────────────────────────────────────────────────────────┤
   │ DEBUG KONSOLU — motor logu + PID→IP olayları + OTO-YAKALA akışı (canlı)    │
   └────────────────────────────────────────────────────────────────────────────┘
```

## Kutu (instance) modeli

- **Instance = kare kutu**: açılışta 6 boş kutu görünür (BOŞ + **AYARLAR**
  düğmesi). AYARLAR → sağlayıcı, ülke, şifreleme, oto-yakalama deseni, kapasite
  seçilir → kutu dolu çalışır. "+ KUTU EKLE" ile yenisi eklenir.
- Her kutunun kendi kimliği, ülkesi, yerel SOCKS5/HTTP portu (22000+) ve
  **gerçeklik geçmişi** (JSONL kullanıcı logu) vardır: `x1 user hola`,
  `x2 user hola` … eşzamanlı; motor yeniden başlasa bile geri gelir.
- **AYARLAR'da seçilebilen sağlayıcılar** (hepsi ücretsiz, sömürüsüz):
  | Sağlayıcı | Açıklama | Şifreleme |
  | --- | --- | --- |
  | **HOLA VPN** | trial kimliği; **uuid'yi motor üretir** (istemci yazılımı/telemetri YOK). Süre bitince **NEW USER MODE** tek tuşla yeni kullanıcı | TLS (1.2/1.3 seçilebilir) |
  | **ÜCRETSİZ PROXY LİSTESİ** | monosans + TheSpeedX + proxyscrape listelerinden **canlı test edilmiş** otomatik seçim (http + socks5) | düz / zincir |
  | **MANUEL SOCKS5** | elle ip:port | düz |
  | **TOR** | yerel Tor SOCKS (9050/9150 — gerçek el sıkışmayla doğrulanır) yoksa otomatik 3-zincir (Tor benzeri) | katmanlı |
  | **ZİNCİR** | 2/3 bağımsız SOCKS5 çıkışı üst üste (Tor mantığında katmanlama) | katmanlı |
  | **VPN GATE** | SoftEther kamu VPN geçitleri — ülke filtreli puan listesi, **.ovpn dışa aktarma** (OpenVPN istemcisiyle tünellenir) | OpenVPN |
- **Şifreleme seçimi** (AYARLAR → Şifreleme, her kutuda): `OTO` / `TLS 1.3` /
  `TLS 1.2` / `şifresiz (hızlı)` / `2 katman zincir` / `3 katman zincir` /
  `Tor yerel` — uyumluluk ve gecikme etkisi panelde gösterilir; hız
  düşürmeyen seçenek önerilir.

## OTO-YAKALA — MOD 1 / MOD 2 (Proxifier'in gelişmişi)

Kutu AYARLAR'ında **mod combobox'ı** seçilir + süreç deseni yazılır:

**MOD 1 — İLK N (kapasiteli)**: yeni açılan süreçler kapasiteye kadar bu kutuya girer:
- **ilk açılan** `metin2client.exe` → desen eşleşen **1. boş kutuya** (instance1)
- **ikinci açılan** → sıradaki eşleşen boş kutuya (instance3…)
- kutu doluysa (kapasite aşıldıysa) → **diğer eşleşen kutuya** otomatik taşınır

**MOD 2 — HEPSİ (açıksa bile)**: yazılan adı taşıyan TÜM süreçler bu kutuya bağlanır:
- `brave.exe` yaz → brave'in **hepsi**, `chrome` yaz → chrome'un **hepsi**
- **şu an process olarak AÇIK olanlar bile anında yakalanır** (kutu kurulur kurulmaz
  mevcut süreçler taranır) — kapasite sınırsız
- her atama debug terminaline `OTO-YAKALA MOD2` olayı olarak düşer

## Süreç yönlendirme

- *LAUNCH*: seçilen kutudan çıkan program **hook ile başlatılır** — program
  proxy'den habersiz trafiği kutunun SOCKS5 router'ından çıkar
  (Linux: `LD_PRELOAD connect()` kancası — uçtan uca kanıtlı;
  Windows: `godtier_hook.dll` IAT kancası + `CreateRemoteThread` inject).
- *PID'Yİ İZLE*: çalışan süreci kutuya attach et (Windows'ta DLL inject).
- *TÜM PC modu*: Windows'ta WinInet sistem proxy'si kurulumu (kayıt +
  broadcast); Linux'ta launch edilen çocuk süreçler env ile.
- Debug terminali **VPN KULLANMASAK BİLE** tüm PC'nin PID→IP tablosunu
  gösterir (`/proc` / `GetExtendedTcpTable`) — "hangi program hangi IP'ye
  çıkıyor" sorusunun canlı yanıtı; olay akışı toplu kopyalanabilir/kaydedilebilir.

## Kanıtlanmış davranışlar (bu depo, Linux'ta gerçek testler)

| Test | Sonuç |
| --- | --- |
| `curl --socks5-hostname 127.0.0.1:22001 http://ip-api.com/json` | ✅ UK çıkışı `34.105.164.240` (gerçek dış IP) |
| Motor içi `check` (SOCKS5 istemci → router → dış) | ✅ exit_ip kutuya yazıldı (United Kingdom / Google LLC) |
| **OTO-YAKALA**: 2× `metin2client.exe` açıldı | ✅ pid→inst#1, pid→inst#2 otomatik + `AUTO_ASSIGN` olayları |
| **OTO-YAKALA MOD 2**: açıkta 3× `metin2client.exe` varken kutu kuruldu | ✅ 3 pid de anında yakalandı (açıksa-bile modu) |
| MOD değişimi (AYARLAR'dan MOD1→MOD2) | ✅ kutu anında tüm eşleşenleri kapsar |
| Kalıcılık: motor yeniden başlat | ✅ kutular (zincir hops'ları dahil) geri yüklendi |
| VPN Gate: liste + **.ovpn export** (10 KB gerçek config) | ✅ JP `public-vpn-187` (ayna: fdciabdul scraper) |
| Zincir kurulumu (3 SOCKS5 hop, el sıkışma doğrulamalı seçim) | ✅ kurulur; bedava hop'lar dalgalıysa hata dürüstçe raporlanır → refresh yeni hop seçer |
| Hola akışı (bg_init + zgettunnels) | ⚠️ sandbox IP'si WAF'ta **blocked=true** — motor 8sn bekleyip yeni uuid dener; mekanizma önceki oturumda farklı IP'den uçtan uca kanıtlandı. Kullanıcı IP'sinde çalışır |
| WebGUI: 6 boş kutu → AYARLAR → dolu kutular, 246 ülke | ✅ tarayıcıda doğrulandı (kanit/*.png) |

Kullanım ekran görüntüleri: `kanit/01..08_*.png` (boş kutular, AYARLAR modalı,
**MOD 2 modal görünümü**, dolu kutular, OTO-YAKALA terminali, süreç arama, VPN
Gate, geçmiş, kaynaklar).

## v2.1 — ağ dayanıklılığı ve oto-yakala MOD'ları

- **`background_init HTTP 0 WinHttpSendRequest hatası` kökten çözüldü**: Windows
  HTTP istemcisi artık **3 aşamalı** — (1) direkt WinHTTP (TLS 1.2+ sabit, Chrome
  UA) → (2) sistem proxy'li WinHTTP → (3) ağ engelliyse **DoH ile IP çöz + saf
  C++ SChannel TLS** (ISP/DNS zehirlemesine karşı). Her hata kodu Türkçe teşhis
  metniyle raporlanır (`#12029 baglanti kurulamadi` gibi) — artık "HTTP 0" değil,
  nedeni görür.
- **Oto-yakala MOD 1 / MOD 2** (yukarıda).
- **VPN Gate**: motor 5 dk liste önbelleği tutar — kutu yaparken "listeyi çek"
  anında döner; çekme sırasında bekleme göstergesi + hata metni görünür.
- **CMD konsolu**: `SetConsoleOutputCP(65001)` — Türkçe karakterler bozulmadan.

## Yapı

```
godtier_vpn/                     (repo adı: MVP_GOD_SSS_TIER_VPN_PROXY_IP)
├── src/
│   ├── core/       json.h (minik JSON), util (uuid/b64/ülke-yok), log (konsol sink)
│   ├── net/        net (platform soket), httpclient (OpenSSL/WinHTTP), tls
│   │              (relay TLS — OpenSSL / SChannel sürüm seçimli)
│   ├── providers/  hola (uuid→bg_init→zgettunnels→trial), more (VPN Gate
│   │              + Tor algılama + kaynak listeleri + zincir seçimi)
│   ├── engine/     router (SOCKS5/HTTP + CONNECT/absolute-form + SOCKS5_UP
│   │              + CHAIN relay), instance (kutu modeli, New User, kalıcılık,
│   │              oto-yakalama alanları), proclist, monitor (PID→IP + OTO-YAKALA)
│   ├── httpserver  localhost API + statik dosya + **gömülü WebUI fallback**
│   ├── api.cpp     REST uçları (aşağıda)
│   └── web_embedded.h   [üretildi] web/ içeriği exe içine gömer (tek .exe!)
├── web/           index.html + app.js + style.css (vanilla — derleme yok)
├── hook/          hook_linux.c (LD_PRELOAD) · hook_windows.cpp (IAT+injector)
├── scripts/       embed_web.py · build_win.sh (zig çapraz derleme) ·
│                  e2e_test.sh (uçtan uca kanıt) · demo_screenshots.sh
├── docs/          x1 (ücretsiz proxy depoları) + x2 (Hola/Opera mekanizmaları)
├── tools/         freevpn.py (önceki oturumun Python aracı — CLI kardeşi)
├── kanit/         kullanım ekran görüntüleri + launch logu
├── build.sh (Linux) · build.bat (MSVC) · CMakeLists.txt
└── data/          instances.json + logs/users/*.jsonl (gerçeklik geçmişi)
```

## Çalıştırma

**Windows (teslim edilen hazır exe):**
```bat
bin\MVP_GOD_SSS_TIER.exe serve 7787        →  http://127.0.0.1:7787/
```
Tek exe — `web/` klasörü yanında olmasa da arayüz çalışır (HTML gömülü).
`godtier_hook.dll` LAUNCH/süreç kancası için aynı klasördedir.

**Windows (kaynaktan):** `build.bat` (MSVC) ya da `scripts/build_win.sh`
(zig çapraz derleme — Linux'tan).

**Linux (test edilen yol):**
```bash
sudo apt install g++ libssl-dev python3
./build.sh                              # bin/mvp_god_sss_tier + hook
./bin/mvp_god_sss_tier serve 7787       # WebGUI
./bin/mvp_god_sss_tier selftest TR      # başsız uçtan uca Hola denemesi
./scripts/e2e_test.sh                   # tüm uçtan uca kanıtlar
```

## API (WebGUI'nin kullandığı uçlar)

`GET /api/status` (tor algılama dahil) · `/api/countries` · **`/api/enc`**
(şifreleme kataloğu) · `/api/instances` · `POST /api/instances_create`
`{provider:hola|plain|socks|tor|chain|vpngate, country, label, addr, enc,
capture, max_procs, hops, vpngate_*}` · `POST /api/inst/<id>/newuser|refresh|`
`check|settings|attach|detach|launch` · `POST /api/global` ·
`GET /api/processes?q=` · `/api/monitor/connections|events` · `/api/console` ·
`/api/users/<id>` · `/api/sources?type=` (http+socks5) · `/api/vpngate?country=`

## Önemli teknik notlar

- **Hola trial portu** CONNECT'i 80'de reddediyor → http hedefleri
  absolute-form GET ile taşınır; 443 → CONNECT (freevpn.py'de kanıtlanmış yol).
- **SNI** Hola proxy TLS'inde gönderilmez (WAF parmak izi) — SNI'li varyant
  yedek; auth `Basic`/`basic` çiftiyle retry. **blocked=true** → 8sn bekleyip
  yeni uuid (4 deneme).
- **Tor algılama** gerçek SOCKS5 el sıkışması ister (TCP açık olması yetmez).
- **Zincir hop'ları** el sıkışma-doğrulamalı SOCKS5 listelerinden seçilir
  (halka http proxy'leri CONNECT'i çoğunlukla yalnız 443'e açtığı için);
  hop'lar `s5:` önekiyle kalıcıdır.
- **VPN Gate** resmi CSV'yi (ev IP'lerinde çalışır) dener, veri merkezi
  IP'lerinde saatlik GitHub aynasına düşer (fdciabdul/Vpngate-Scraper-API).
- Bedava proxy'ler doğaları gereği gelip gider — kutuyu **refresh** etmek
  normaldir; zincir hataları dürüstçe raporlanır.
- **Güvenlik**: tüm yurtdışı bağlantılar motorun kendi soket kodundan çıkar;
- DNS: MVP'te yerel çözülür (kancalar `connect()` seviyesinde).
- **Yalnızca kendi ağ trafiğiniz için eğitim amaçlı tersine mühendislik**
  çalışmasıdır (upstream referanslar `scripts/upstream_hola/`,
  `scripts/upstream_opera/` klasörlerinde — snawoot hola-proxy ve opera-proxy).
  Üçüncü taraf servislerin kullanım koşullarına uyun.
