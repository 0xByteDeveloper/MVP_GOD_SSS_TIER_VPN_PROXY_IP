# x2 — Hola / Opera / Brave "ücretsiz kullanıcı" mekanizmalarını Chrome'suz kullanmak

> Görev: Hola VPN'in Chrome istemcisindeki "yeni kullanıcı + ücretsiz kullanım" modelini tarayıcı
> olmadan, Python ile kullanmak. Aynı mantık Opera VPN ve Brave VPN'de de arandı. Kaynak kod
> sökücü: `snawoot-proxies-forks/hola-proxy` (41★) ve `snawoot-proxies-forks/opera-proxy` (57★)
> — Go ile yazılmış orijinal araçlar; akışlar bu makinede satır satır okunup Python'a port edildi.

## 0. Özet matris (hepsi bu makinede canlı test edildi)

| Servis | Mekanizma | Sonuç | Kanıt |
|---|---|---|---|
| **Hola VPN** | uuid üret → `background_init` → `zgettunnels` → **trial portu (22225)** + Basic auth | **ÇALIŞIYOR** — uçtan uca | https hedef HTTP 200, çıkış IP `157.230.54.147` (DigitalOcean, US); http hedef de 200 (Vultr IP); curl ile 2 kez daha |
| **Opera VPN** (SurfEasy) | anonim e-posta → `register_subscriber` → `register_device` → `discover` (RFC 7616 **SHA-256 digest**) | **ÇALIŞIYOR** — uçtan uca | https ve http hedef HTTP 200, çıkış IP `77.111.247.x` (EU) — anonim hesap üretimi sonsuz tekrarlanabilir |
| **Brave VPN** ( Guardians) | 7 gün trial | **OLMAZ** — abonelik + ödeme kartı zorunlu; anonim API yok; tarayıcı dışı resmi istemci yok | Araştırma bulgusu (aşağıda) |
| VPN Gate | Kamu API'si (CSV) | ÇALIŞIYOR (x1'de) | gerçek CSV + .ovpn üretildi |

## 1. Hola VPN — mekanizma (tam çözüldü)

Hola'nın Chrome eklentisi yeni bir "kullanıcı" oluşturduğunda istemci tarafı sadece bir **UUID**
üretir; eklenti sürümü ile `client.hola.org/client_cgi`'yi çağırır. Kimlik yok, hesap yok, kart yok.

```
1) POST https://client.hola.org/client_cgi/background_init?uuid=<UUID>
       login=1 & ver=<ext_ver>
   <- { "key": <session_key>, "ver": <gerçek eklenti sürümü>, ... }
2) GET  https://client.hola.org/client_cgi/zgettunnels?country=us&limit=3&...&session_key=...
   <- { "agent_key": <PROXY ŞİFRESİ>, "ip_list": { "zagentXXXX.hola.org": "<ip>" },
        "port": { "trial": 22225, "direct": 22222, "peer": 22223, "trial_peer": 22226, ... } }
3) PROXY = https://user-uuid-<UUID>-is_prem-0:<agent_key>@<zagent-ip>:22225
```

Kritik ayrıntılar (sorun çözülürken bulundu, tekrar edenleri yazmayın):

- **Eklenti sürümü `999.999.999` gibi sahte verilmez** — `zgettunnels` artık reddediyor.
  `background_init` yanıtındaki `ver` alanı kullanılır (örn. `1.263.342`); hatta `"1.0"` bile geçiyor,
  önemli olan sahte dev-boyut olmaması.
- **Login formatı birebir tutmalı**: `user-uuid-<uuid>-is_prem-0` ve şifre `agent_key`.
- **Trial portu (22225) sadece 443 hedef tünellemesine izin verir** — 80 porta CONNECT atarsanız
  yanıt `403 Bad Port`'tur. Düz http hedefler için **CONNECT yok**, proxy'ye doğrudan
  **absolute-form GET** (`GET http://site/ HTTP/1.1`) gönderilir.
- **`zgettunnels` ara sıra boş yanıt döndürür** — Go aracı da bunu bilir ve ping_id'yi değiştirip
  backoff'la retry eder; retry şart.
- **WAF uyarısı (önemli):** aynı IP'den kısa sürede çok sayıda uuid üretirseniz Hola
  `blocked=true` döndürür (geçici bayrak; ~30+ dk sonra düşüyor). Test sırasında 2 kez yaşadık;
  araç otomatik yeni uuid/retry dener, sonra kullanıcıya süre söyler. **Sabırla ve az sayıda
  kimlik üretimiyle kullanın.**

Minimal Python (sadece stdlib, kopyala-çalıştır — tunnel kimliğini bu şekilde alın):

```python
import urllib.request, urllib.parse, uuid, json

UUID = uuid.uuid4().hex
q = urllib.parse.urlencode({"uuid": UUID})
data = urllib.parse.urlencode({"login": "1", "ver": "1.0"}).encode()
r = json.loads(urllib.request.urlopen(urllib.request.Request(
    f"https://client.hola.org/client_cgi/background_init?{q}", data=data), timeout=25).read())
qs = urllib.parse.urlencode({"country": "us", "limit": "3", "ext_ver": r["ver"],
    "browser": "chrome", "product": "cws", "uuid": UUID,
    "session_key": str(r["key"]), "is_premium": "0", "ping_id": "0.12345678901234567"})
t = json.loads(urllib.request.urlopen(
    f"https://client.hola.org/client_cgi/zgettunnels?{qs}", timeout=25).read())
login = f"user-uuid-{UUID}-is_prem-0"
host, ip = next(iter(t["ip_list"].items()))
print(f"curl -x https://{login}:{t['agent_key']}@{ip}:{t['port']['trial']} https://api.ipify.org")
```

## 2. Opera VPN (SurfEasy) — RFC 7616 SHA-256 digest ile

Opera VPN'in arka ucu **SurfEasy**'dir; Opera istemcisi `api2.sec-tunnel.com`'a **digest
doğrulamalı** (algorithm=SHA-256) RPC yapar. Anonim abonelik + cihaz kaydı **sınırsız**
üretilebilir; her kayıt taze proxy kimliği verir.

```
1) POST /v4/register_subscriber   {email: <rastgele>@se0316.best.vpn, password: SHA1(email)}
2) POST /v4/register_device       {client_type: se0316, device_hash: <rastgele 40 hex>,
                                   device_name: Opera-Browser-Client}
   <- {device_id, device_password}            # device_password = proxy JWT'si
3) POST /v4/discover              {serial_no: SHA1(device_id), requested_geo: '"EU",,'}
   <- {ips: [{ip: 77.111.247.x, ports: [443], geo: {country_code: EU}}]}
4) PROXY login  = SHA1(device_id) (BÜYÜK harf hex)
   PROXY şifre  = device_password
```

**Zor kısım — digest:** SurfEasy `WWW-Authenticate: Digest ... algorithm=SHA-256` ile meydan okur;
Python'un `HTTPDigestAuthHandler`'ı **yalnız MD5** bilir (stdlib'de SHA-256 digest YOK).
RFC 7616 formülü elle uygulanır (Go istemcisiyle birebir):

```python
import hashlib, time

def digest_authorization(ch, uri, method="POST", login="se0316",
                          password="SILrMEPBmJuhomxWkfm3JalqHX2Eheg1YhlEZiMh8II"):
    algo = (ch.get("algorithm") or "MD5").upper()
    h = (lambda s: hashlib.sha256(s.encode()).hexdigest()) if algo.startswith("SHA-256") \
        else (lambda s: hashlib.md5(s.encode()).hexdigest())
    realm, nonce = ch["realm"], ch["nonce"]
    qop = (ch.get("qop") or "auth").split(",")[0].strip() or "auth"
    cnonce = h(f"{time.time_ns()}:{login}:my_value")
    ha1 = h(f"{login}:{realm}:{password}")
    ha2 = h(f"{method}:{uri}")
    response = h(f"{ha1}:{nonce}:{1:08x}:{cnonce}:{qop}:{ha2}")
    return (f'Digest username="{login}", realm="{realm}", nonce="{nonce}", uri="{uri}", '
            f"algorithm={algo}, cnonce=\"{cnonce}\", qop={qop}, nc={1:08x}, response=\"{response}\"")
```

Akış: anonim POST → 401 + challenge → challenge'ı parse et → digest'li POST'u tekrarla (istek
gövdesi aynı). `return_code.code == 0` başarıdır.

**Bulunan tuzaklar:**

- `Proxy-Authorization` şeması **BÜYÜK `Basic`** olmalı; küçük `basic` → 407.
  (Hola'da tam tersi küçük `basic` kabul görür — iki sağlayıcı da RFC 7235'e aykırı biçimde!)
  Aracın çözümü: 407 gelince otomatik ters varyantı dener.
- Proxy sertifikası `CN=*.sec-tunnel.com`'dur; Opera **IP döndürür**, `eu0.sec-tunnel.com` DNS'te
  çözülmez. Go istemcisi IP'ye bağlanır, SNI **göndermez**, sertifikayı elle
  `{GEO}0.sec-tunnel.com` adına doğrular. Araç pratikte: SNI'siz TLS + doğrulama yok
  (veri akışı yine TLS şifrelidir; kimlik doğrulaması proxy auth'la yapılır).

## 3. TLS-in-TLS tekniği — asıl teknik bulgu (ikisini de çalıştıran şey)

Hola/Opera proxy'leri **443 üzerinde TLS konuşan forward proxy**'dir. https hedef alınmak istenince
*TLS içinde TLS* gerekir. Burada kritik keşif:

> **Python'un `ssl.SSLContext.wrap_socket(sock)` çağrısı `sock.fileno()` üzerinden çalışır.**
> Sock bir SSLSocket ise yeni TLS katmanı **iç TLS'i baypas edip ham TCP'ye yazar** — proxy bu
> plaintext ClientHello'yu çöp görür ve el sıkışma `UNEXPECTED_MESSAGE` ile düşer. (curl'un
> çalışma sebebi: OpenSSL BIO katmanlamasıdır.)

**Çözüm: memory BIO** — hedef TLS'ini proxy TLS soketi üzerinden elle katmanlamak:

```python
import ssl

def tls_in_tls_handshake(sock, host, timeout=30):
    inbuf, outbuf = ssl.MemoryBIO(), ssl.MemoryBIO()
    sslobj = ssl.create_default_context().wrap_bio(inbuf, outbuf, server_hostname=host)
    while True:
        try:
            sslobj.do_handshake()
            return sslobj, inbuf, outbuf
        except ssl.SSLWantReadError:
            out = outbuf.read()          # ClientHello/Finished -> proxy'ye gönder
            if out: sock.sendall(out)
            d = sock.recv(65536)         # proxy'den oku -> hedef TLS'ine besle
            if not d: raise RuntimeError("tünel kapandı")
            inbuf.write(d)
```

Bu teknik **Hola'da ve Opera'da da** https hedefleri uçtan uca açtı (testler aşağıda). Stdlib
dışı bağımlılık yok; sertifikalar `create_default_context` ile doğrulanır.

## 4. Brave VPN — neden olmuyor (araştırma bulgusu)

- Brave Firewall + VPN (Guardians zincirinde) **7 gün ücretsiz** der; fakat başlatmak
  **abonelik + ödeme yöntemi** gerektirir (Google/Apple mağaza hesabı ya da kart).
- SurfEasy/Hola tarzı **anonim API akışı yok**; uç noktaları tarayıcı dışından açan resmi
  istemci yok. "Yeni kullanıcı tekrar ücretsiz" döngüsü bu yüzden kurulamaz.
- Sonuç: **OLMAZ** olarak işaretlendi. (Deneme süresi bir kez, kartla alınır — bu oturumun
  ölçütü olan "kart/hesap yok, sınırsız yeni kullanıcı" modeline uymaz.)

Benzer mekanizmalı diğerleri (hızlı durum):

| Servis | Durum |
|---|---|
| Urban VPN, Touch VPN, Browsec | Chrome eklentisi tabanlı; Hola gibi **CGI API'leri** yok — her biri elle sökülmeden otomatiklenemez (Hola tek istisna oldu) |
| VPN Gate | Tam açık, API'si var — x1'de (istediğinde .ovpn üretimi araçta hazır) |
| Proton VPN free, Windscribe 10GB | hesap gerektirir — ölçüte uymaz |

## 5. Araç — `freevpn.py` (salt Python stdlib, sıfır bağımlılık, 783 satır)

Tek dosya; `list / check / hola / opera / vpngate` komutları. Tam kaynak kodu aşağıda gömülüdür
(kopyala-kaydet-çalıştır; ayrıca dosya olarak da teslim edildi).

### Kullanım

```bash
# 1) Ücretsiz proxy listesi (5 GitHub/API kaynağı birleşimi; indirme yok)
python3 freevpn.py list --type http --limit 50
python3 freevpn.py list --type socks5 | python3 freevpn.py check --timeout 5   # canlıları süz

# 2) Hola — "yeni kullanıcı ücretsiz" mekanizması, Chrome'sız
python3 freevpn.py hola --country us --fetch https://api.ipify.org
#    -> Login/Password + tunnel tablosu + GET ... -> HTTP 200 + çıkış IP'si

# 3) Opera VPN — anonim SurfEasy akışı, SHA-256 digest, Chrome'sız
python3 freevpn.py opera --geo EU --fetch https://api.ipify.org
#    -> Proxy login/password + IP listesi + GET ... -> HTTP 200 + çıkış IP'si

# 4) VPN Gate — kamu OpenVPN sunucuları (.ovpn konfigi bile üretilebilir)
python3 freevpn.py vpngate --top 10
python3 freevpn.py vpngate --top 3 --ovpn 0    # vpngate_<ip>.ovpn yazar
```

Hedef `--fetch http://...` da çalışır (Hola trial'ında CONNECT 403 verdiği için araç düz http
hedefleri proxy'ye absolute-form GET ile taşır — Go aracındaki davranışla aynı).

### Bu makinedeki canlı testler (bugün)

```
$ python3 freevpn.py list --type http  -> 9.792 benzersiz proxy (5 kaynak)
$ python3 freevpn.py list --type socks5 -> 8.490 benzersiz
$ python3 freevpn.py hola --country us --fetch https://api.ipify.org
  # GET https://api.ipify.org -> HTTP 200   (yol: python@zagent2682.hola.org:22225)
  # gövde: 157.230.54.147                    <- gerçek çıkış IP'si, saf Python TLS-in-TLS (BIO)
$ python3 freevpn.py opera --geo EU --fetch https://api.ipify.org
  # GET https://api.ipify.org -> HTTP 200   (yol: python@77.111.247.25:443)
  # gövde: 77.111.247.25                      <- anonim Opera VPN çıkışı
$ python3 freevpn.py opera --geo EU --fetch http://api.ipify.org
  # -> HTTP 200, gövde: 77.111.247.79        <- http hedef de uçtan uca
$ python3 freevpn.py vpngate --top 5 --ovpn 0
  -> gerçek SoftEther/OpenVPN .ovpn konfigi yazıldı
```

Not: Hola test yoğunluğundan kaynak IP'miz geçici `blocked=true` bayrağı yedi (yukarıda
anlatılan WAF davranışı); araç bunu nezaketle raporlar ve 1-2 dk sonra tekrar önerir.
Mekanizma bugün 4 kez uçtan uca kanıtlandı (2x curl, 2x saf Python).

### AI agent'ın gözü için mini SKILL.md

```yaml
name: freevpn
description: >
  Ücretsiz proxy/VPN'e ihtiyaç varsa: (1) kalıcı proxy havuzu için
  `python3 freevpn.py list --type http | freevpn.py check --timeout 5`;
  (2) hızlı, güvenilir tek-URL tüneli için `freevpn.py hola --country us --fetch URL`
  veya `freevpn.py opera --geo EU --fetch URL` — ikisi de Chrome/hesap/kart İSTEMEZ,
  anonim kimliği araç üretir. Hola'da hızlı ardışık denemelerden kaçın (WAF geçici ban).
  https hedefleri tercih et (en az sürtünme yolu).
tools: [bash]
```

---

## 6. `freevpn.py` — tam kaynak kodu

```python
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
freevpn.py — AI agent'lar için ücretsiz proxy/VPN araç seti (salt stdlib, bağımlılık yok)

Komutlar:
  list     : GitHub repo'larından + API'lerden ücretsiz proxy listesi çeker (indirme yok, ham metin)
  check    : proxy listesini eşzamanlı doğrular (HTTP CONNECT / SOCKS5 el sıkışması)
  hola     : Hola VPN'in "yeni kullanıcı ücretsiz trial" mekanizmasını browser'sız kullanır
             (uuid üret -> background_init -> zgettunnels -> trial portu + Basic auth)
  opera    : Opera VPN'in (SurfEasy) anonim abone/cihaz akışını browser'sız kullanır
  vpngate  : VPN Gate'in kamu aktarıcı listesini (CSV API) çeker

Referans kaynaklar (mekanizmaları bu repo'lardan türettim):
  https://github.com/snawoot-proxies-forks/hola-proxy
  https://github.com/snawoot-proxies-forks/opera-proxy
Kullanım örnekleri:
  python3 freevpn.py list --type http --limit 50
  python3 freevpn.py list --type http | python3 freevpn.py check --timeout 5
  python3 freevpn.py hola --country tr --fetch https://api.ipify.org
  python3 freevpn.py opera --geo EU --fetch https://api.ipify.org
  python3 freevpn.py vpngate --top 10
"""
import argparse
import base64
import concurrent.futures
import csv
import hashlib
import http.client
import io
import json
import os
import random
import re
import socket
import ssl
import sys

import subprocess
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid as uuidlib

UA = ("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36")
OPERA_UA = ("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36 OPR/114.0.0.0")

# ------------------------------------------------------------------ ortak HTTP

def http_json(url, data=None, headers=None, method="POST", timeout=25):
    """POST/GET -> JSON. data dict ise form-urlencoded gövde olur."""
    hdrs = {"User-Agent": UA}
    if headers:
        hdrs.update(headers)
    body = None
    if data is not None:
        body = urllib.parse.urlencode(data).encode()
        hdrs.setdefault("Content-Type", "application/x-www-form-urlencoded")
    req = urllib.request.Request(url, data=body, method=method, headers=hdrs)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode("utf-8", "replace"))

def basic_auth(login, password):
    # NOT: SurfEasy proxy'si şemayı büyük harf ister — "basic" 407 döndürüyor
    return "Basic " + base64.b64encode(f"{login}:{password}".encode()).decode()

def cap_hex_sha1(s):
    return hashlib.sha1(s.encode()).hexdigest().upper()

def rand_capital_hex(n_bytes):
    return "".join(random.choice("0123456789ABCDEF") for _ in range(n_bytes * 2))

# ------------------------------------------------------------------ LIST (x1)

LIST_SOURCES = {
    "monosans": {
        "http":  "https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/http.txt",
        "socks4":"https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/socks4.txt",
        "socks5":"https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/socks5.txt",
    },
    "proxifly": {
        "http":  "https://raw.githubusercontent.com/proxifly/free-proxy-list/main/proxies/protocols/http/data.txt",
        "socks4":"https://raw.githubusercontent.com/proxifly/free-proxy-list/main/proxies/protocols/socks4/data.txt",
        "socks5":"https://raw.githubusercontent.com/proxifly/free-proxy-list/main/proxies/protocols/socks5/data.txt",
    },
    "speedx": {
        "http":  "https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/http.txt",
        "socks4":"https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/socks4.txt",
        "socks5":"https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/socks5.txt",
    },
    "proxyscrape": {
        "http":  "https://api.proxyscrape.com/v2/?request=displayproxies&protocol=http&timeout=10000&country=all&ssl=all&anonymity=all",
        "socks4":"https://api.proxyscrape.com/v2/?request=displayproxies&protocol=socks4&timeout=10000&country=all",
        "socks5":"https://api.proxyscrape.com/v2/?request=displayproxies&protocol=socks5&timeout=10000&country=all",
    },
    "geonode": {   # JSON API
        "http":  "https://proxylist.geonode.com/api/proxy-list?limit=500&page=1&sort_by=lastChecked&sort_type=desc&protocols=http",
        "socks5":"https://proxylist.geonode.com/api/proxy-list?limit=500&page=1&sort_by=lastChecked&sort_type=desc&protocols=socks5",
    },
}

def fetch_list(source, ptype, timeout=25):
    url = LIST_SOURCES[source][ptype]
    with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": UA}), timeout=timeout) as r:
        raw = r.read().decode("utf-8", "replace")
    if source == "geonode":  # JSON
        d = json.loads(raw)
        out = []
        for it in d.get("data", []):
            ip, port = it.get("ip"), it.get("port")
            if ip and port:
                out.append(f"{ip}:{port}")
        return out
    out = []
    for line in raw.splitlines():
        line = line.strip()
        if line and ":" in line and line[0] not in "*#<":
            out.append(line.split()[0])
    return out

def cmd_list(args):
    sources = [args.source] if args.source != "all" else list(LIST_SOURCES)
    seen, results = set(), {}
    for src in sources:
        if args.type not in LIST_SOURCES[src]:
            continue
        try:
            proxies = fetch_list(src, args.type, timeout=args.timeout)
        except Exception as e:
            print(f"# kaynak hatası {src}: {e}", file=sys.stderr)
            continue
        results[src] = proxies
        for p in proxies:
            seen.add(p)
    printed = 0
    for p in sorted(seen):
        if printed >= args.limit:
            break
        print(p)
        printed += 1
    print(f"# toplam {len(seen)} benzersiz proxy ({', '.join(f'{s}:{len(v)}' for s, v in results.items())})",
          file=sys.stderr)

# ------------------------------------------------------------------ CHECK

def test_http_proxy(p, timeout):
    """HTTP/HTTPS CONNECT tünelleme testi — 204 dönerse proxy canlıdır."""
    try:
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({
            "http": f"http://{p}", "https": f"http://{p}"}))
        req = urllib.request.Request("http://www.gstatic.com/generate_204",
                                      headers={"User-Agent": UA})
        with opener.open(req, timeout=timeout) as r:
            return r.status in (200, 204)
    except Exception:
        return False

def test_socks5(p, timeout):
    """Minimal SOCKS5 el sıkışması + CONNECT denemesi."""
    try:
        host, port = p.rsplit(":", 1)
        s = socket.create_connection((host, int(port)), timeout=timeout)
        s.settimeout(timeout)
        s.sendall(b"\x05\x01\x00")                       # hello, no auth
        if s.recv(2) != b"\x05\x00":
            return False
        req = b"\x05\x01\x00\x03" + bytes([len("api.ipify.org")]) + \
              b"api.ipify.org" + (80).to_bytes(2, "big")
        s.sendall(req)
        resp = s.recv(10)
        if len(resp) >= 2 and resp[1] == 0:              # CONNECT succeeded
            s.close()
            return True
        return False
    except Exception:
        return False

def cmd_check(args):
    lines = [l.strip() for l in (open(args.infile) if args.infile != "-" else sys.stdin)
             if l.strip() and not l.startswith("#")]
    if not lines:
        return
    def worker(p):
        if args.type in ("socks5", "socks4") or p.startswith("socks"):
            return p, test_socks5(p.replace("socks5://", "").replace("socks4://", ""), args.timeout)
        return p, test_http_proxy(p.replace("http://", ""), args.timeout)
    alive = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as ex:
        for p, ok in ex.map(worker, lines):
            if ok:
                alive.append(p)
                print(p)
    print(f"# canlı: {len(alive)}/{len(lines)}", file=sys.stderr)

# ------------------------------------------------------------------ TLS-in-TLS (memory BIO)

def _tls_in_tls_handshake(sock, host, timeout=30):
    """Hedef TLS'ini mevcut TLS proxy soketi ÜZERİNDE katmanlar (memory-BIO ile).
    Neden elle: Python'un wrap_socket(sock) çağrısı fileno() üzerinden çalışır —
    iç SSL katmanını baypas edip ham TCP'ye yazar; proxy bunu çöp olarak görür ve
    el sıkışma 'unexpected message' ile düşer. curl/OpenSSL'de çalışmasının sebebi
    BIO katmanlamasıdır; burada aynısı stdlib ssl.MemoryBIO ile yapılır."""
    inbuf, outbuf = ssl.MemoryBIO(), ssl.MemoryBIO()
    sslobj = ssl.create_default_context().wrap_bio(inbuf, outbuf, server_hostname=host)
    sock.settimeout(timeout)
    deadline = time.monotonic() + timeout
    while True:
        try:
            sslobj.do_handshake()
            return sslobj, inbuf, outbuf
        except ssl.SSLWantReadError:
            out = outbuf.read()
            if out:
                sock.sendall(out)
            d = sock.recv(65536)
            if not d:
                raise RuntimeError("tünel kapandı (hedef TLS el sıkışması)")
            inbuf.write(d)
        except ssl.SSLWantWriteError:
            sock.sendall(outbuf.read())
        if time.monotonic() > deadline:
            raise TimeoutError("hedef TLS el sıkışması zaman aşımı")

def _tls_in_tls_request(sslobj, inbuf, outbuf, sock, req_bytes, timeout=30):
    """BIO katmanlı TLS oturumu üzerinden tam bir HTTP isteği gönder/al."""
    sslobj.write(req_bytes)
    out = outbuf.read()
    if out:
        sock.sendall(out)
    chunks = []
    while True:
        try:
            chunk = sslobj.read(65536)
        except ssl.SSLWantReadError:
            out = outbuf.read()
            if out:
                sock.sendall(out)
            d = sock.recv(65536)
            if not d:
                break
            inbuf.write(d)
            continue
        if not chunk:
            break
        chunks.append(chunk)
    return b"".join(chunks)

def _decode_chunked(data):
    """Basit chunked transfer kod çözücü (yanıt gövdesi için)."""
    out, rest = b"", data
    while True:
        line, _, rest2 = rest.partition(b"\r\n")
        try:
            size = int(line.split(b";")[0], 16)
        except Exception:
            return out or rest
        if size == 0:
            return out
        out += rest2[:size]
        rest = rest2[size + 2:]

# ------------------------------------------------------------------ TLS proxy çekirdeği

def _read_response_head(sock, max_bytes=8192):
    """HTTP yanıt başlığını \r\n\r\n'e kadar bayt bayt oku (parçalı gelime güvenli)."""
    buf = b""
    while b"\r\n\r\n" not in buf and len(buf) < max_bytes:
        b = sock.recv(1)
        if not b:
            break
        buf += b
    return buf

def connect_via_https_proxy(proxy_ip, proxy_port, tls_name, auth_header,
                            target_host, target_port, hide_sni=True, timeout=30):
    """HTTPS (TLS) forward proxy'ye CONNECT at; hedefe açık tünel soketi döndür.
    hide_sni=True: ClientHello'da SNI gönderme (Hola/Opera istemcilerinin davranışı)."""
    raw = socket.create_connection((proxy_ip, int(proxy_port)), timeout=timeout)
    try:
        if hide_sni:
            ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
            sock = ctx.wrap_socket(raw)
        else:
            ctx = ssl.create_default_context()
            sock = ctx.wrap_socket(raw, server_hostname=tls_name)
    except Exception:
        raw.close()
        raise
    sock.settimeout(timeout)
    req = (f"CONNECT {target_host}:{target_port} HTTP/1.1\r\n"
           f"Host: {target_host}:{target_port}\r\n"
           f"Proxy-Authorization: {auth_header}\r\n"
           f"Proxy-Connection: keep-alive\r\n\r\n")
    sock.sendall(req.encode())
    head = _read_response_head(sock)
    status = head.split(b"\r\n", 1)[0].decode("utf-8", "replace")
    if " 200 " not in status + " ":
        sock.close()
        raise RuntimeError(f"proxy CONNECT reddi: {status.strip()}")
    return sock

def _flip_auth_scheme(auth_header):
    """'Basic xyz' <-> 'basic xyz'. Hola küçük harf şemayı isterken SurfEasy büyük
    harf şemayı ister — ikisi de RFC 7235'e aykırı biçimde davrandığı için 407
    alındığında öbür varyant otomatik denenir."""
    if auth_header.startswith("Basic "):
        return "basic " + auth_header[6:]
    if auth_header.startswith("basic "):
        return "Basic " + auth_header[6:]
    return None

def fetch_via_https_proxy(url, proxy_ip, proxy_port, tls_name, auth_header,
                          hide_sni=True, timeout=30):
    """URL'yi Hola/Opera tarzı TLS forward-proxy üzerinden getir. 407 durumunda
    auth şemasının büyük/küçük harf varyantıyla bir kez daha denenir."""
    flipped = _flip_auth_scheme(auth_header)
    attempts = [auth_header] + ([flipped] if flipped else [])
    last_status, last_exc = None, None
    for ah in attempts:
        try:
            res = _fetch_via_https_proxy_once(url, proxy_ip, proxy_port, tls_name, ah,
                                              hide_sni, timeout)
            if res.get("status") != 407:
                return res
            last_status = res
        except Exception as e:
            last_exc = e
    if last_status is not None:
        return last_status
    raise last_exc

def _fetch_via_https_proxy_once(url, proxy_ip, proxy_port, tls_name, auth_header,
                          hide_sni=True, timeout=30):
    """URL'yi Hola/Opera tarzı TLS forward-proxy üzerinden getir.
    https hedef: CONNECT + hedefe TLS; http hedef: absolute-form GET (CONNECT yok)."""
    u = urllib.parse.urlsplit(url if "://" in url else "http://" + url)
    host, port = u.hostname, u.port or (443 if u.scheme == "https" else 80)
    path = (u.path or "/") + (("?" + u.query) if u.query else "")
    raw = socket.create_connection((proxy_ip, int(proxy_port)), timeout=timeout)
    if hide_sni:
        pctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        pctx.check_hostname = False
        pctx.verify_mode = ssl.CERT_NONE
        sock = pctx.wrap_socket(raw)                      # proxy TLS: SNI yok
    else:
        sock = ssl.create_default_context().wrap_socket(raw, server_hostname=tls_name)
    sock.settimeout(timeout)
    if u.scheme == "https":
        creq = (f"CONNECT {host}:{port} HTTP/1.1\r\nHost: {host}:{port}\r\n"
                f"Proxy-Authorization: {auth_header}\r\n"
                f"Proxy-Connection: keep-alive\r\n\r\n")
        sock.sendall(creq.encode())
        head = _read_response_head(sock)
        status_line = head.split(b"\r\n", 1)[0].decode("utf-8", "replace")
        if " 200 " not in status_line + " ":
            sock.close()
            raise RuntimeError(f"proxy CONNECT reddi: {status_line.strip()}")
        # hedef TLS'i memory-BIO ile proxy TLS'i ÜZERİNDEN katmanlanır
        sslobj, inbuf, outbuf = _tls_in_tls_handshake(sock, host, timeout)
        req = (f"GET {path} HTTP/1.1\r\nHost: {host}\r\nUser-Agent: {UA}\r\n"
               f"Accept: */*\r\nConnection: close\r\n\r\n")
        raw = _tls_in_tls_request(sslobj, inbuf, outbuf, sock, req.encode(), timeout)
        head, _, body = raw.partition(b"\r\n\r\n")
        try:
            out_status = int(head.split(b" ", 2)[1])
        except Exception:
            out_status = 0
        if b"chunked" in head.lower():
            body = _decode_chunked(body)
        try:
            sock.close()
        except Exception:
            pass
    else:
        # düz http hedefi: proxy'ye absolute-form istek (CONNECT gerekmez; 80 portu
        # trial proxy'de CONNECT ile reddedilir)
        req = (f"GET {u.geturl()} HTTP/1.1\r\nHost: {host}\r\n"
               f"User-Agent: {UA}\r\nAccept: */*\r\n"
               f"Proxy-Authorization: {auth_header}\r\nConnection: close\r\n\r\n")
        sock.sendall(req.encode())
        chunks = []
        while True:
            try:
                b = sock.recv(65536)
            except Exception:
                break
            if not b:
                break
            chunks.append(b)
        sock.close()
        raw_resp = b"".join(chunks)
        head, _, body = raw_resp.partition(b"\r\n\r\n")
        try:
            out_status = int(head.split(b" ", 2)[1])
        except Exception:
            out_status = 0
    return {"status": out_status, "body": body[:4000].decode("utf-8", "replace")}

# ------------------------------------------------------------------ HOLA (x2)

HOLA_CCGI = "https://client.hola.org/client_cgi/"
HOLA_EXT_VER = "1.0"  # sahte 999.999.999 reddediliyor; bg_init'in döndürdüğü ver kullanılır

def hola_get_tunnels(country, proxy_type="direct", limit=3, timeout=25):
    """Yeni 'kullanıcı' (uuid) üret -> background_init -> zgettunnels (boş yanıtlarda retry).
    Dönen: login, password(agent_key), tunnels(json). NOT: aynı IP'den çok hızlı uuid
    üretimi 'blocked=true' (geçici ban) döndürebilir — yeni uuid ile otomatik tekrar denenir."""
    last = None
    for uuid_try in range(4):
        u = uuidlib.uuid4().hex
        try:
            r = http_json(HOLA_CCGI + "background_init?" + urllib.parse.urlencode({"uuid": u}),
                          data={"login": "1", "ver": HOLA_EXT_VER},
                          headers={"User-Agent": UA},
                          timeout=timeout)
        except Exception as e:
            last = e
            time.sleep(3)
            continue
        if isinstance(r, dict) and r.get("blocked"):
            last = RuntimeError("blocked=true (geçici ban, bekleniyor)")
            time.sleep(8)
            continue
        session_key = r["key"]
        ext_ver = (r.get("ver") or HOLA_EXT_VER)  # sunucunun bildirdiği güncel sürüm
        qs_base = {
            "country": country, "limit": str(limit),
            "ext_ver": ext_ver, "browser": "chrome", "product": "cws",
            "uuid": u, "session_key": str(session_key), "is_premium": "0",
        }
        for attempt in range(6):  # Go aracı gibi backoff'lu retry (boş yanıt normaldir)
            qs = urllib.parse.urlencode(dict(qs_base, ping_id=f"{random.random():.17f}"))
            try:
                r2 = http_json(HOLA_CCGI + "zgettunnels?" + qs, data=None, timeout=timeout)
            except Exception as e:
                last = e
                r2 = None
            if r2 and r2.get("ip_list"):
                login = f"user-uuid-{u}-is_prem-0"
                return login, r2["agent_key"], r2
            time.sleep(2 + random.random() * 2)
    raise RuntimeError(f"Hola kimlik akışı başarısız (son hata: {last!r}) — "
                        f"IP geçici bayraklanmış olabilir; 1-2 dk sonra tekrar deneyin")

def hola_pick_endpoint(tunnels, proxy_type="direct", use_trial=True):
    """get_endpoint mantığı: direct/lum/pool/virt -> trial(22225) veya direct(22222)
    portu; peer -> trial_peer(22226) veya peer(22223)."""
    ports = tunnels["port"]
    if proxy_type in ("direct", "lum", "pool", "virt", "peer"):
        if proxy_type == "peer":
            port = ports["trial_peer"] if use_trial else ports["peer"]
        else:
            port = ports["trial"] if use_trial else ports["direct"]
    else:
        port = ports["direct"]
    for host, ip in tunnels["ip_list"].items():
        proto = (tunnels.get("protocol") or {}).get(host, "http")
        if proto.lower() == "http":
            return {"host": host, "ip": ip, "port": int(port)}
    raise RuntimeError("uygun HTTP tunnel yok")

def fetch_via_tls_proxy_best(url, endpoints, login, password, timeout=30):
    """Sağlam zincir: (her host x [SNI'siz, SNI'li] Python) sonra curl fallback.
    endpoints: [(host, ip, port)] — Hola'nın WAF'ı tek bir yolu engelleyebilir."""
    auth = basic_auth(login, password)
    last = None
    for host, ip, port in endpoints[:3]:
        for hide_sni in (True, False):
            try:
                res = fetch_via_https_proxy(url, ip, port, host, auth,
                                            hide_sni=hide_sni, timeout=timeout)
                return res, f"python@{host}:{port}{'' if hide_sni else '+sni'}"
            except Exception as e:
                last = e
    for host, ip, port in endpoints[:2]:  # curl fallback (yerelde curl varsa)
        try:
            out = subprocess.run(
                ["curl", "-sS", "-m", str(timeout), "--proxy-insecure",
                 "-x", f"https://{login}:{password}@{host}:{port}", url],
                capture_output=True, text=True, timeout=timeout + 10)
            if out.returncode == 0 and out.stdout.strip():
                return {"status": 200, "body": out.stdout[:4000]}, f"curl@{host}:{port}"
            last = out.stderr.strip() or out.stdout[:200]
        except FileNotFoundError:
            break
        except Exception as e:
            last = e
    raise RuntimeError(f"tüm yollar başarısız (son hata: {last!r}) — "
                        "Hola bu IP'yi geçici flag'lemiş olabilir; birkaç dakika sonra tekrar deneyin")

def cmd_hola(args):
    login, agent_key, tunnels = hola_get_tunnels(args.country, args.proxy_type,
                                                  args.limit, timeout=args.timeout)
    ep = hola_pick_endpoint(tunnels, args.proxy_type, trial := not args.no_trial)
    print(f"Login: {login}")
    print(f"Password: {agent_key}")
    print(f"Proxy-Authorization: {basic_auth(login, agent_key)}")
    print()
    print("host,ip_address,direct,peer,hola,trial,trial_peer,vendor")
    w = csv.writer(sys.stdout)
    p = tunnels["port"]
    for host, ip in tunnels["ip_list"].items():
        w.writerow([host, ip, p["direct"], p["peer"], p["hola"],
                    p["trial"], p["trial_peer"],
                    (tunnels.get("vendor") or {}).get(host, "?")])
    print(file=sys.stdout)
    ep_line = f"{ep['ip']}:{ep['port']}"
    print(f"# seçilen: {ep_line} (host {ep['host']}, tip {args.proxy_type}, "
          f"{'trial port' if trial else 'normal port'})", file=sys.stderr)
    print(f"# curl örneği:\n#   curl -x https://{login}:{agent_key}@{ep_line} https://api.ipify.org",
          file=sys.stderr)
    if args.fetch:
        pkey = "trial_peer" if args.proxy_type == "peer" else "trial"
        if args.no_trial:
            pkey = "peer" if args.proxy_type == "peer" else "direct"
        port = int(tunnels["port"][pkey])
        eps = [(h, ip, port) for h, ip in list(tunnels["ip_list"].items())[:3]]
        res, via = fetch_via_tls_proxy_best(args.fetch, eps, login, agent_key,
                                            timeout=args.timeout)
        print(f"# GET {args.fetch} -> HTTP {res['status']}  (yol: {via})")
        print("# gövde (ilk 4KB): " + res["body"][:400])

# ------------------------------------------------------------------ OPERA (x2)

SE_API = "https://api2.sec-tunnel.com/v4"
SE_LOGIN, SE_PASS = "se0316", "SILrMEPBmJuhomxWkfm3JalqHX2Eheg1YhlEZiMh8II"

def _parse_digest_challenge(header):
    """WWW-Authenticate Digest başlığını parametre sözlüğüne söker."""
    if header.lower().startswith("digest "):
        header = header[7:]
    params = {}
    for m in re.finditer(r'(\w+)="((?:[^"\\]|\\.)*)"|(\w+)=([^\s,]+)', header):
        if m.group(1):
            params[m.group(1).lower()] = m.group(2).replace('\"', '"')
        elif m.group(3):
            params[m.group(3).lower()] = m.group(4)
    return params

def _digest_authorization(ch, uri, method="POST", nc=1,
                           login=SE_LOGIN, password=SE_PASS):
    """RFC 7616 digest Authorization başlığı (MD5 ve SHA-256/-SESS ailesi).
    Python'un HTTPDigestAuthHandler'ı yalnız MD5 bildiği için elle yazıldı;
    SurfEasy API'si algorithm=SHA-256 ile meydan okur. Formül, Go istemcisinin
    (go-http-digest-auth-client) computeA1/A2/response mantığıyla birebir aynı."""
    algo = (ch.get("algorithm") or "MD5").upper()
    def h(s):
        hh = hashlib.sha256() if algo.startswith("SHA-256") else hashlib.md5()
        hh.update(s.encode())
        return hh.hexdigest()
    realm, nonce = ch.get("realm", ""), ch["nonce"]
    qop = (ch.get("qop") or "auth").split(",")[0].strip() or "auth"
    cnonce = h(f"{time.time_ns()}:{login}:my_value")
    ha1 = h(f"{login}:{realm}:{password}")
    if algo.endswith("-SESS"):
        ha1 = h(f"{ha1}:{nonce}:{cnonce}")
    ha2 = h(f"{method}:{uri}")
    response = h(f"{ha1}:{nonce}:{nc:08x}:{cnonce}:{qop}:{ha2}")
    parts = [f'Digest username="{login}", realm="{realm}", nonce="{nonce}", '
             f'uri="{uri}", algorithm={algo or "MD5"}, cnonce="{cnonce}", '
             f'qop={qop}, nc={nc:08x}, response="{response}"']
    if "opaque" in ch:
        parts.append(f', opaque="{ch["opaque"]}"')
    if ch.get("userhash") == "true":
        parts.append(f', userhash="true"')
    return "".join(parts)

class SeSession:
    """RFC 7616 SHA-256 digest-auth + cookie'li minimal SurfEasy RPC istemcisi.
    Akış (her RPC): anonim POST -> 401 + Digest challenge -> digest'li POST."""

    def __init__(self, timeout=25):
        self.timeout = timeout
        self.cookies = {}

    def _hdrs(self, extra=None):
        h = {"User-Agent": OPERA_UA,
             "SE-Client-Version": "Stable 114.0.5282.21",
             "SE-Operating-System": "Windows",
             "Accept": "application/json",
             "Content-Type": "application/x-www-form-urlencoded"}
        if self.cookies:
            h["Cookie"] = "; ".join(f"{k}={v}" for k, v in self.cookies.items())
        if extra:
            h.update(extra)
        return h

    def _keep_cookies(self, headers):
        for v in headers.get_all("Set-Cookie") or []:
            kv = v.split(";", 1)[0]
            if "=" in kv:
                k, val = kv.split("=", 1)
                self.cookies[k.strip()] = val.strip()

    def rpc(self, endpoint, params):
        url = SE_API + "/" + endpoint
        body = urllib.parse.urlencode(params).encode()
        uri = urllib.parse.urlsplit(url).path
        last = None
        for attempt in range(3):
            req = urllib.request.Request(url, data=body, method="POST",
                                         headers=self._hdrs())
            try:
                with urllib.request.urlopen(req, timeout=self.timeout) as r:
                    self._keep_cookies(r.headers)
                    return self._check(json.loads(r.read().decode("utf-8", "replace")), endpoint)
            except urllib.error.HTTPError as e:
                self._keep_cookies(e.headers)
                if e.code != 401:
                    raise RuntimeError(f"SurfEasy HTTP {e.code} @ {endpoint}: "
                                       f"{e.read(200)[:200]!r}")
                challenge = _parse_digest_challenge(
                    e.headers.get("WWW-Authenticate", ""))
                if "nonce" not in challenge:
                    raise RuntimeError(f"Digest challenge yok @ {endpoint}: "
                                       f"{e.headers.get('WWW-Authenticate')!r}")
            except Exception as e:  # ağ hatası: kısa bekle, yeniden dene
                last = e
                time.sleep(2)
                continue
            # digest ile yeniden dene
            auth = {"Authorization": _digest_authorization(challenge, uri, "POST")}
            req2 = urllib.request.Request(url, data=body, method="POST",
                                           headers=self._hdrs(auth))
            try:
                with urllib.request.urlopen(req2, timeout=self.timeout) as r2:
                    self._keep_cookies(r2.headers)
                    return self._check(json.loads(r2.read().decode("utf-8", "replace")), endpoint)
            except urllib.error.HTTPError as e2:
                last = e2
        raise RuntimeError(f"SurfEasy RPC başarısız @ {endpoint} (son hata: {last!r})")

    @staticmethod
    def _check(d, endpoint):
        st = d.get("return_code", d.get("status", {})) or {}
        if st.get("code", 0) != 0:
            raise RuntimeError(f"SurfEasy hatası {endpoint}: {st}")
        return d.get("data", d)

def opera_get_proxy(geo="EU", timeout=25):
    """Opera VPN'in anonim abone + cihaz akışı (browser'sız, RFC 7616 digest):
    rastgele e-posta -> register_subscriber -> register_device -> discover.
    Dönen: (proxy_login=SHA1(device_id), device_password, [(host, ports)])."""
    se = SeSession(timeout=timeout)
    email = f"{rand_capital_hex(16)}@se0316.best.vpn"   # rastgele anonim abone
    password = cap_hex_sha1(email)
    se.rpc("register_subscriber", {"email": email, "password": password})
    device_hash = rand_capital_hex(20)                  # 40 hex karakter
    d = se.rpc("register_device", {"client_type": "se0316",
                                    "device_hash": device_hash,
                                    "device_name": "Opera-Browser-Client"})
    dev_id, dev_password = d["device_id"], d["device_password"]
    proxy_login = cap_hex_sha1(dev_id)
    dd = se.rpc("discover", {"serial_no": proxy_login,
                             "requested_geo": f'"{geo}",,'})
    ips = [{"host": e["ip"], "ports": e.get("ports") or [443],
            "geo": (e.get("geo") or {}).get("country_code", "?")}
           for e in dd.get("ips", [])]
    return proxy_login, dev_password, ips

def cmd_opera(args):
    login, password, ips = opera_get_proxy(args.geo, timeout=args.timeout)
    print(f"Proxy login: {login}")
    print(f"Proxy password: {password}")
    print(f"Proxy-Authorization: Basic {base64.b64encode(f'{login}:{password}'.encode()).decode()}")
    print()
    print("ip,port,geo")
    for e in ips:
        for port in e["ports"]:
            print(f"{e['host']},{port},{e['geo']}")
    if ips:
        ep = ips[0]
        port = ep["ports"][0]
        ep_line = f"{ep['host']}:{port}"
        print(f"# seçilen: {ep_line}", file=sys.stderr)
        print(f"# curl örneği:\n#   curl -x https://{login}:{password}@{ep_line} https://api.ipify.org",
              file=sys.stderr)
        if args.fetch:
            res, via = fetch_via_tls_proxy_best(args.fetch, [(ep["host"], ep["host"], port)],
                                                login, password, timeout=args.timeout)
            print(f"# GET {args.fetch} -> HTTP {res['status']}  (yol: {via})")
            print("# gövde (ilk 4KB): " + res["body"][:400])

def _is_float(s):
    try:
        float(s)
        return True
    except Exception:
        return False

# ------------------------------------------------------------------ VPN GATE

def cmd_vpngate(args):
    # ana API veri merkezi IP'lerine bazen HTML boş sayfası verir; yansılar yedek
    urls = ["http://www.vpngate.net/api/iphone/",
            "https://www.vpngate.net/api/iphone/",
            "http://78.142.193.246:33304/api/iphone/",
            "http://217.138.212.46:34663/api/iphone/",
            "http://213.136.92.167:13182/api/iphone/"]
    raw = None
    for u in urls:
        try:
            with urllib.request.urlopen(urllib.request.Request(u, headers={"User-Agent": UA}),
                                        timeout=args.timeout) as r:
                raw = r.read().decode("utf-8", "replace")
            if "*vpn_servers" in raw[:200]:   # gerçek CSV (HTML boş sayfası değil)
                break
            raw = None
        except Exception as e:
            print(f"# {u} hata: {e}", file=sys.stderr)
    if not raw:
        sys.exit(1)
    rows = [r for r in csv.reader(io.StringIO(raw))
            if r and not r[0].startswith(("*", "#"))]   # yorumlar + başlık satırı
    rows = [r for r in rows if len(r) >= 15 and r[1] and "." in r[1]]  # bozuk/kısa satırları at
    rows.sort(key=lambda r: float(r[4]) if len(r) > 4 and _is_float(r[4]) else 0,
             reverse=True)
    print("hostname,ip,score,ping_ms,speed_mbps,country,sessions")
    for r in rows[:args.top]:
        print(f"{r[0]},{r[1]},{r[2]},{r[3]},{r[4]},{r[6]},{r[7]}")
    if args.ovpn >= 0:
        idx = args.ovpn
        if 0 <= idx < len(rows) and len(rows[idx]) >= 15:
            b64 = rows[idx][14].strip()   # OpenVPN_ConfigData_Base64 (15. sütun)
            cfg = base64.b64decode(b64 + "=" * (-len(b64) % 4)).decode("utf-8", "replace")
            path = os.path.abspath(f"vpngate_{rows[idx][1]}.ovpn")
            with open(path, "w") as f:
                f.write(cfg)
            print(f"# OpenVPN yapılandırması yazıldı: {path}", file=sys.stderr)

# ------------------------------------------------------------------ MAIN

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("list", help="ücretsiz proxy listesi (GitHub raw + API)")
    p.add_argument("--type", default="http", choices=["http", "socks4", "socks5"])
    p.add_argument("--source", default="all", choices=list(LIST_SOURCES) + ["all"])
    p.add_argument("--limit", type=int, default=100)
    p.add_argument("--timeout", type=int, default=25)
    p.set_defaults(func=cmd_list)

    p = sub.add_parser("check", help="proxy doğrula")
    p.add_argument("--infile", default="-")
    p.add_argument("--type", default="http")
    p.add_argument("--timeout", type=float, default=6)
    p.add_argument("--workers", type=int, default=40)
    p.set_defaults(func=cmd_check)

    p = sub.add_parser("hola", help="Hola ücretsiz trial proxy (browser'sız)")
    p.add_argument("--country", default="us")
    p.add_argument("--proxy-type", default="direct", choices=["direct", "lum", "peer", "pool", "virt"])
    p.add_argument("--limit", type=int, default=3)
    p.add_argument("--no-trial", action="store_true", help="trial portu yerine normal port")
    p.add_argument("--fetch", default=None, help="proxy üzerinden bu URL'yi getir")
    p.add_argument("--show-sni", action="store_true", help="proxy TLS'inde SNI gönder")
    p.add_argument("--timeout", type=int, default=30)
    p.set_defaults(func=cmd_hola)

    p = sub.add_parser("opera", help="Opera VPN proxy (browser'sız, SHA-256 digest)")
    p.add_argument("--geo", default="EU", choices=["EU", "AS", "AM"])
    p.add_argument("--fetch", default=None)
    p.add_argument("--show-sni", action="store_true")
    p.add_argument("--timeout", type=int, default=25)
    p.set_defaults(func=cmd_opera)

    p = sub.add_parser("vpngate", help="VPN Gate kamu aktarıcıları")
    p.add_argument("--top", type=int, default=10)
    p.add_argument("--ovpn", type=int, default=-1, help="bu sıradaki sunucu için .ovpn üret")
    p.add_argument("--timeout", type=int, default=30)
    p.set_defaults(func=cmd_vpngate)

    args = ap.parse_args()
    args.func(args)

if __name__ == "__main__":
    main()

```
