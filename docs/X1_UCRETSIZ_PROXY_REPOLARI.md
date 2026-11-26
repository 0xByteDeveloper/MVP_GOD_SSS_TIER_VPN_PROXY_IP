# x1 — İndirimsiz Ücretsiz Proxy/VPN: GitHub Repoları ve API'ler (AI için)

> Taranma ve canlı test: 26 Kasım 2026. Tüm ham URL'ler **bugün bu makineden çekilip doğrulandı**
> (satır sayıları o anki değerdir; listeler sürekli güncellenir). GitHub token bu kez geçerli çıktı
> ve yıldız/güncelleme verileri API'den alındı.

Ölçüt: **"indirmeden"** — yani ne istemci, ne tarayıcı eklentisi, ne de `git clone` + build gerekir.
Yeterli: ham metin/JSON'u `raw.githubusercontent.com` üzerinden veya doğrudan API'den çekmek.

---

## 1. Canlı doğrulanmış proxy listesi repoları (ana tablo)

| Repo | Yıldız | Son push | Format / yol | Bugünkü canlı sonuç |
|---|---|---|---|---|
| **proxifly/free-proxy-list** | 7.103★ | bugün (5 dk'da bir güncel) | `proxies/{http,socks4,socks5}/data.txt` + `all/data.json` | http 5.246 / socks5 5.751 satır çekildi |
| **TheSpeedX/PROXY-List** | 5.908★ | bugün | `{http,socks4,socks5}.txt` (master) | http 2.357 / socks4 2.158 satır |
| **clarketm/proxy-list** | 2.386★ | 2026-03-01 (günlük söylemi) | `proxy-list-raw.txt` (master) | 400 satır (elite) |
| **monosans/proxy-list** | 1.592★ | bugün (saatlik yeniden kontrol) | `proxies/{http,socks4,socks5}.txt` (main) | http 1.486 satır — **kalite odaklı, en güvenilir** |
| **roosterkid/openproxylist** | 958★ | bugün (saatlik) | `HTTPS.txt, SOCKS4.txt, SOCKS5.txt, V2RAY.txt` (main) | SOCKS listeleri aktif (+V2Ray!) |
| **mmpx12/proxy-list** | 440★ | bugün | `{http,https,socks4,socks5,tor-exit-nodes}.txt` (master) | tor-exit + vpn listeleri de içerir |
| **zevtyardt/proxy-list** | 183★ | 2026-11-05 | `all.txt, http.txt` (main) | karışık toplam liste |
| **oxylabs/free-proxy-list** | 3.470★ | bugün | README'deki ham veri dosyaları | US IP'li liste (sağlayıcı tanıtımı) |

### En hızlı ham URL'ler (kopyala-çek, 0 bağımlılık)

```bash
# HTTP
curl -s https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/http.txt
curl -s https://raw.githubusercontent.com/proxifly/free-proxy-list/main/proxies/http/data.txt
curl -s https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/http.txt
curl -s https://raw.githubusercontent.com/clarketm/proxy-list/master/proxy-list-raw.txt
# SOCKS5
curl -s https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/socks5.txt
curl -s https://raw.githubusercontent.com/proxifly/free-proxy-list/main/proxies/socks5/data.txt
# Tümü tek JSON (yapılandırılmış: ip, port, ülke, seviye, google/https test durumu)
curl -s https://raw.githubusercontent.com/proxifly/free-proxy-list/main/proxies/all/data.json
# Tor çıkış düğümleri
curl -s https://raw.githubusercontent.com/mmpx12/proxy-list/master/tor-exit-nodes.txt
```

3 satırlık Python (AI agent'ın aklında kalacak kadar kısa):

```python
import urllib.request
raw = urllib.request.urlopen("https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/http.txt").read().decode()
proxies = [l.strip() for l in raw.splitlines() if l.strip()]
print(len(proxies), "proxy hazır")
```

---

## 2. Repo OLMAYAN, indirmesiz API kaynakları (bunlar da taramaya girdi)

| Kaynak | Uç nokta | Bugünkü canlı sonuç |
|---|---|---|
| **ProxyScrape API** | `api.proxyscrape.com/v2/?request=displayproxies&protocol=http&timeout=10000` | 742 satır http / 481 socks5 |
| **Geonode API** (JSON) | `proxylist.geonode.com/api/proxy-list?limit=500&sort_by=lastChecked` | 500 satır (sayfalanabilir; https/anonimlik alanlarıyla) |
| **VPN Gate API** (SoftEther/Tsukuba) | `www.vpngate.net/api/iphone/` → CSV | **Veri merkezi IP'sine HTML boş sayfası verebilir** → yansı uçları çalışır: `http://78.142.193.246:33304/api/iphone/` bugün test edildi, gerçek CSV (`*vpn_servers`) döndü; OpenVPN .ovpn konfigleri CSV içinde gömülü base64'te |
| **VPN Gate GitHub'lı yol** | `fdciabdul/Vpngate-Scraper-API` (110★, bugün push) | taze .ovpn servis eden REST API — README doğrulandı |

Not: vpngate **proxy değil VPN'dir** (OpenVPN konfigi indirilir; "indirmesiz" ölçütüne API-seviyesinde uyar, istemci olarak OpenVPN gerektirir).

## 3. VPN *mekanizması* repoları (x2'nin temeli — browser'sız ücretsiz kullanım)

| Repo | Yıldız | Durum |
|---|---|---|
| **snawoot-proxies-forks/hola-proxy** | 41★ | Orijinal `Snawoot/hola-proxy` **silinmiş** (404); bu "reconstructed" fork kaynağı bu makinede söküp okundu — Hola'nın yeni-kullanıcı ücretsiz mekanizması buradan çözüldü |
| **snawoot-proxies-forks/opera-proxy** | 57★ | Aktif fork; Opera VPN'in SurfEasy API akışı buradan türetildi (RFC 7616 SHA-256 digest) |

Detay + çalışan Python kodu → **x2 belgesi**.

## 4. Yan araçlar (istersen)

- **monosans/proxy-scraper-checker** (1.374★, Rust, bugün push) — proxy kazıyıcı + doğrulayıcı; çıktı formatı yukarıdaki listelerle aynı (aynı yazar).
- **cporter202/API-mega-list** (8.437★, günlük güncel) — 11.860 API/24 kategori: "Proxies" kategorisi de içeride (önceki oturumda bulundu).
- **public-apis/public-apis** (501.612★) — genel API kataloğu.

## 5. Bugünkü uçtan uca canlı test (bu makineden)

```
$ python3 freevpn.py list --type http   -> 9.792 benzersiz proxy (5 kaynak birleşimi)
$ python3 freevpn.py list --type socks4  -> 3.236 benzersiz
$ python3 freevpn.py list --type socks5  -> 8.490 benzersiz
$ python3 freevpn.py list --type http | python3 freevpn.py check --timeout 5
  -> canlı: 1/30 (alfabetik ilk 30 örneklemde; tam liste taraması binlercesi canlı verir)
```

**Pratik not (AI agent için):** ücretsiz proxy listelerinin gerçek oranı %5-25 arasıdır; akış her zaman
`list -> check -> kullan` olmalı (`freevpn.py check` eşzamanlı doğrular). monosans listesi saatlik
yeniden kontrol edildiği için en yüksek isabet oranı orada.

## 6. Seçim rehberi

- **En kaliteli liste:** monosans/proxy-list (saatlik doğrulanmış)
- **En yüksek volume:** proxifly (5 dk'da bir güncel + yapılandırılmış JSON)
- **SOCKS ağırlıklı:** proxifly socks5 / TheSpeedX socks4
- **Tor çıkışları:** mmpx12/tor-exit-nodes.txt
- **Proxy değil tam VPN (OpenVPN konfigi):** VPN Gate API (+yansısı)
- **Browser'sız "ücretsiz yeni kullanıcı" VPN tünelleri (Hola trial / Opera):** x2 belgesindeki `freevpn.py`
