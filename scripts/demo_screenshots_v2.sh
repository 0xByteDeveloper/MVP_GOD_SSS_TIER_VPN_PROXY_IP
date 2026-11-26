#!/bin/bash
# MVP_GOD_SSS_TIER v2.1 — CANLI DEMO + EKRAN GÖRÜNTÜLERİ
# (MOD1/MOD2 oto-yakala + WinHTTP dayanıklılık sürümü)
cd /home/z/my-project/godtier_vpn
mkdir -p kanit
pkill -f mvp_god_sss_tier 2>/dev/null; pkill -f metin2client 2>/dev/null; sleep 0.3

AB=agent-browser
EP=http://127.0.0.1:7787

# ---- 0) dummy süreçler (MOD2'nin "açık olanları yakala" kanıtı) ----
gcc -O2 -o /tmp/metin2client.exe /tmp/metin2client.c 2>/dev/null || true
gcc -O2 -o /tmp/game.exe /tmp/metin2client.c 2>/dev/null || true
/tmp/metin2client.exe > /tmp/m2_1.log 2>&1 & M1=$!
/tmp/metin2client.exe > /tmp/m2_2.log 2>&1 & M2=$!
/tmp/metin2client.exe > /tmp/m2_3.log 2>&1 & M3=$!
/tmp/game.exe      > /tmp/g1.log 2>&1   & G1=$!
sleep 1
echo "[+] metin2client.exe x3 + game.exe x1 açık (MOD2 açıkta-yakalama testi)"

# ---- 0b) canlı ücretsiz proxy bul (plain kutuları için — GERÇEK JSON doğrulamalı) ----
PROXY=""
for p in $(curl -s -m 20 https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/http.txt | head -40); do
  R=$(curl -s -m 6 -x "http://$p" "http://ip-api.com/json/?fields=query" 2>/dev/null)
  if echo "$R" | python3 -c "import json,sys; d=json.load(sys.stdin); assert 'query' in d and len(d['query'])>6" 2>/dev/null; then
    PROXY="$p"; echo "[+] canlı proxy (JSON doğrulandı): $PROXY"; break
  fi
done
[ -z "$PROXY" ] && PROXY="127.0.0.1:1984" && echo "[!] canlı proxy yok — dummy adres"

# ---- 1) motoru başlat (temiz data) ----
rm -rf data
./bin/mvp_god_sss_tier serve 7787 > /tmp/engine_shot.log 2>&1 &
sleep 1.5

# ---- 2) tarayıcı: boş kutular ----
$AB open $EP/ >/dev/null 2>&1
$AB set viewport 1680 950 >/dev/null 2>&1
sleep 2.5
$AB screenshot kanit/01_kutular_bos.png >/dev/null 2>&1
echo "[+] 01 boş kutular"

# ---- 3) AYARLAR modalı — HOLA + MOD combobox ----
$AB eval "document.querySelector('.tile.empty .btn-ayarlar').click()" >/dev/null 2>&1
sleep 0.6
$AB eval "var p=document.getElementById('mProvider'); p.value='hola'; p.dispatchEvent(new Event('change'))" >/dev/null 2>&1
$AB eval "document.getElementById('mCountry').value='TR'" >/dev/null 2>&1
$AB eval "var e=document.getElementById('mEnc'); e.value='tls13'; e.dispatchEvent(new Event('change'))" >/dev/null 2>&1
$AB eval "document.getElementById('mCapture').value='metin2client.exe'" >/dev/null 2>&1
sleep 0.8
$AB screenshot kanit/02_ayarlar_modal_hola.png >/dev/null 2>&1
echo "[+] 02 ayarlar modalı (hola/TR/tls13 + oto-yakala)"

# ---- 3b) MOD 2 — HEPSİ seçiliyken ----
$AB eval "var cm=document.getElementById('mCapMode'); cm.value='1'; cm.dispatchEvent(new Event('change'))" >/dev/null 2>&1
sleep 0.8
$AB screenshot kanit/02b_ayarlar_modal_MOD2.png >/dev/null 2>&1
$AB eval "document.getElementById('btnModalCancel').click()" >/dev/null 2>&1
echo "[+] 02b MOD2 (hepsi) görünümü"

# ---- 4) kutular: inst1=MOD2 (hepsi), inst2=MOD1 (kapasite 1), inst3=vpngate ----
curl -s --max-time 30 -X POST $EP/api/instances_create -d "{\"provider\":\"plain\",\"addr\":\"$PROXY\",\"label\":\"oyuncu1\",\"enc\":\"auto\",\"capture\":\"metin2client.exe\",\"capture_mode\":1}" -o /dev/null
echo "[+] oyuncu1 = MOD2 HEPSİ (metin2client.exe) — açık 3 süreç de yakalanır"
sleep 3   # monitor MOD2 geçişine izin ver
curl -s --max-time 30 -X POST $EP/api/instances_create -d "{\"provider\":\"plain\",\"addr\":\"$PROXY\",\"label\":\"oyuncu2\",\"enc\":\"plain\",\"capture\":\"game.exe\",\"capture_mode\":0,\"max_procs\":1}" -o /dev/null
echo "[+] oyuncu2 = MOD1 İLK N kapasite 1 (game.exe)"
curl -s --max-time 60 -X POST $EP/api/instances_create -d '{"provider":"vpngate","country":"","label":"vg1","enc":"auto"}' -o /tmp/vg_inst.json
python3 -c "import json;d=json.load(open('/tmp/vg_inst.json'));print('[+] vpngate kutusu:', d.get('vpngate_hostname'), d.get('status'))" 2>/dev/null
sleep 4

# ---- 5) dolu kutular + çıkış IP ----
curl -s --max-time 25 -X POST $EP/api/inst/1/check -o /dev/null &
curl -s --max-time 25 -X POST $EP/api/inst/2/check -o /dev/null &
sleep 6
$AB eval "location.reload()" >/dev/null 2>&1
sleep 3
$AB screenshot kanit/03_kutular_dolu.png >/dev/null 2>&1
echo "[+] 03 dolu kutular (MOD2-hepsi rozeti + 3 pid + OVPN)"

# ---- 6) debug terminal ----
$AB eval "document.querySelector('[data-tab=mon]').click()" >/dev/null 2>&1
sleep 2.5
$AB screenshot kanit/04_debug_terminal_otoyakala.png >/dev/null 2>&1
echo "[+] 04 debug terminali (OTO-YAKALA MOD2 + pid→IP)"

# ---- 7) süreç yönlendirme ----
$AB eval "document.querySelector('[data-tab=procs]').click()" >/dev/null 2>&1
sleep 1
$AB eval "document.getElementById('procSearch').value='metin2'; document.getElementById('procSearch').dispatchEvent(new Event('input'))" >/dev/null 2>&1
sleep 2
$AB screenshot kanit/05_surec_yonlendirme.png >/dev/null 2>&1
echo "[+] 05 süreç yönlendirme"

# ---- 8) VPN Gate sekmesi ----
$AB eval "document.querySelector('[data-tab=vgate]').click()" >/dev/null 2>&1
sleep 1
$AB eval "document.getElementById('btnVgFetch').click()" >/dev/null 2>&1
sleep 10
$AB screenshot kanit/06_vpngate_listesi.png >/dev/null 2>&1
echo "[+] 06 VPN Gate listesi"

# ---- 9) kullanıcı geçmişi ----
$AB eval "document.querySelector('[data-tab=users]').click()" >/dev/null 2>&1
sleep 1.5
$AB screenshot kanit/07_kullanici_gecmisi.png >/dev/null 2>&1
echo "[+] 07 kullanıcı gerçeklik geçmişi"

# ---- 10) kaynaklar ----
$AB eval "document.querySelector('[data-tab=srcs]').click()" >/dev/null 2>&1
$AB eval "document.getElementById('btnSrcs').click()" >/dev/null 2>&1
sleep 8
$AB screenshot kanit/08_kaynaklar.png >/dev/null 2>&1
echo "[+] 08 kaynak listeleri"

# ---- 11) kanıt dökümü ----
echo "=== instance durumu ==="
curl -s --max-time 10 $EP/api/instances | python3 -c "
import json,sys
for i in json.load(sys.stdin)['instances']:
    print(f\"  #{i['id']} {i['label']} {i['provider']} mode={i.get('capture_mode')} pids={i['attached_pids']} st={i['status']} exit={i['exit_ip']}\")"
echo "=== oto-yakala MOD2 satırları ==="
grep -a "MOD2" /tmp/engine_shot.log | head -5

$AB close >/dev/null 2>&1
kill $M1 $M2 $M3 $G1 2>/dev/null
pkill -f metin2client 2>/dev/null; pkill -f game.exe 2>/dev/null
pkill -f "mvp_god_sss_tier serve" 2>/dev/null
echo "DEMO + GÖRÜNTÜLER TAMAM"
