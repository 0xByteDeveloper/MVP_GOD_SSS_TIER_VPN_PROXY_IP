#!/bin/bash
# MVP_GOD_SSS_TIER — CANLI KULLANIM DEMOSU + EKRAN GÖRÜNTÜLERİ
# (tek bash çağrısında: motor + kutular + oto-yakalama + tarayıcı)
cd /home/z/my-project/godtier_vpn
mkdir -p kanit/v1
mv -f kanit/UI_ilk.png kanit/UI_dashboard.png kanit/v1/ 2>/dev/null

AB=agent-browser
EP=http://127.0.0.1:7787

# ---- 0) dummy metin2client.exe (oto-yakalama testi için) ----
cat > /tmp/metin2client.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
int main() {
    for (int i = 0; i < 40; i++) {
        struct sockaddr_in sa; memset(&sa, 0, sizeof sa);
        sa.sin_family = AF_INET; sa.sin_port = htons(80);
        sa.sin_addr.s_addr = htonl(0x5D8AD822);
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(s, (struct sockaddr*)&sa, sizeof sa) == 0) { printf("baglanti #%d\n", i); fflush(stdout); }
        close(s); sleep(3);
    }
    return 0;
}
EOF
gcc -O2 -o /tmp/metin2client.exe /tmp/metin2client.c

# ---- 1) motoru başlat (temiz data ile) ----
rm -rf data
./bin/mvp_god_sss_tier serve 7787 > /tmp/engine_shot.log 2>&1 &
sleep 1.5

# ---- 2) tarayıcı: boş kutular ----
$AB open $EP/ >/dev/null 2>&1
$AB set viewport 1680 950 >/dev/null 2>&1
sleep 2.5
$AB screenshot kanit/01_kutular_bos.png >/dev/null 2>&1
echo "[+] 01 boş kutular çekildi"

# ---- 3) AYARLAR modalı: HOLA + ülke + şifreleme + oto-yakala ----
$AB eval "document.querySelector('.tile.empty .btn-ayarlar').click()" >/dev/null 2>&1
sleep 0.6
$AB eval "var p=document.getElementById('mProvider'); p.value='hola'; p.dispatchEvent(new Event('change'))" >/dev/null 2>&1
$AB eval "document.getElementById('mCountry').value='TR'" >/dev/null 2>&1
$AB eval "var e=document.getElementById('mEnc'); e.value='tls13'; e.dispatchEvent(new Event('change'))" >/dev/null 2>&1
$AB eval "document.getElementById('mLabel').value='hola-tr'" >/dev/null 2>&1
$AB eval "document.getElementById('mCapture').value='metin2client.exe'" >/dev/null 2>&1
sleep 0.8
$AB screenshot kanit/02_ayarlar_modal_hola.png >/dev/null 2>&1
$AB eval "document.getElementById('btnModalCancel').click()" >/dev/null 2>&1
echo "[+] 02 ayarlar modalı (hola/tr/tls13/oto-yakala) çekildi"

# ---- 4) gerçek kutular oluştur (API'den; kanıtlanmış canlı proxy) ----
curl -s --max-time 30 -X POST $EP/api/instances_create \
  -d '{"provider":"plain","addr":"34.105.252.3:3128","label":"oyuncu1","enc":"auto","capture":"metin2client.exe","max_procs":1}' -o /dev/null
echo "[+] oyuncu1 kuruldu (plain, oto-yakala: metin2client.exe)"
curl -s --max-time 30 -X POST $EP/api/instances_create \
  -d '{"provider":"plain","addr":"34.105.252.3:3128","label":"oyuncu2","enc":"plain","capture":"metin2client.exe","max_procs":1}' -o /dev/null
echo "[+] oyuncu2 kuruldu (ikinci metin2 kutusu — dolu kutuya taşma senaryosu)"
curl -s --max-time 90 -X POST $EP/api/instances_create \
  -d '{"provider":"vpngate","country":"","label":"vg1","enc":"auto","capture":""}' -o /tmp/vg_inst.json
python3 -c "import json;d=json.load(open('/tmp/vg_inst.json'));print('[+] vpngate kutusu:', d.get('vpngate_hostname'), d.get('status'))" 2>/dev/null

# ---- 5) metin2client.exe süreçlerini aç → OTO-YAKALA ----
/tmp/metin2client.exe > /tmp/m2_1.log 2>&1 & P1=$!
/tmp/metin2client.exe > /tmp/m2_2.log 2>&1 & P2=$!
echo "[+] metin2client.exe x2 açıldı (pid $P1, $P2) — 6sn bekleniyor"
sleep 6

# ---- 6) dolu kutular + çıkış IP ----
curl -s --max-time 30 -X POST $EP/api/inst/1/check -o /dev/null &
curl -s --max-time 30 -X POST $EP/api/inst/2/check -o /dev/null &
sleep 7
$AB eval "location.reload()" >/dev/null 2>&1
sleep 3
$AB screenshot kanit/03_kutular_dolu.png >/dev/null 2>&1
echo "[+] 03 dolu kutular çekildi (çıkış IP + OVPN + oto-yakalama rozetleri)"

# ---- 7) debug terminal ----
$AB eval "document.querySelector('[data-tab=mon]').click()" >/dev/null 2>&1
sleep 2
$AB eval "document.getElementById('monOnlyVia').checked=false; document.getElementById('monOnlyVia').dispatchEvent(new Event('change'))" >/dev/null 2>&1
sleep 1.5
$AB screenshot kanit/04_debug_terminal_otoyakala.png >/dev/null 2>&1
echo "[+] 04 debug terminali (OTO-YAKALA + pid→IP akışı) çekildi"

# ---- 8) süreç yönlendirme (search_textbox) ----
$AB eval "document.querySelector('[data-tab=procs]').click()" >/dev/null 2>&1
sleep 1
$AB eval "document.getElementById('procSearch').value='metin2'; document.getElementById('procSearch').dispatchEvent(new Event('input'))" >/dev/null 2>&1
sleep 2
$AB screenshot kanit/05_surec_yonlendirme.png >/dev/null 2>&1
echo "[+] 05 süreç yönlendirme (search: metin2) çekildi"

# ---- 9) VPN Gate sekmesi ----
$AB eval "document.querySelector('[data-tab=vgate]').click()" >/dev/null 2>&1
sleep 1
$AB eval "document.getElementById('btnVgFetch').click()" >/dev/null 2>&1
sleep 12
$AB screenshot kanit/06_vpngate_listesi.png >/dev/null 2>&1
echo "[+] 06 VPN Gate listesi çekildi"

# ---- 10) kullanıcı geçmişi ----
$AB eval "document.querySelector('[data-tab=users]').click()" >/dev/null 2>&1
sleep 1.5
$AB screenshot kanit/07_kullanici_gecmisi.png >/dev/null 2>&1
echo "[+] 07 kullanıcı gerçeklik geçmişi çekildi"

# ---- 11) kaynaklar ----
$AB eval "document.querySelector('[data-tab=srcs]').click()" >/dev/null 2>&1
$AB eval "document.getElementById('btnSrcs').click()" >/dev/null 2>&1
sleep 8
$AB screenshot kanit/08_kaynaklar.png >/dev/null 2>&1
echo "[+] 08 kaynak listeleri çekildi"

# ---- 12) kanıt: motor içi durum + tünel ----
echo "=== instance durumu (kanıt) ==="
curl -s --max-time 10 $EP/api/instances | python3 -c "
import json,sys
for i in json.load(sys.stdin)['instances']:
    print(f\"  #{i['id']} {i['label']} {i['provider']} port={i['port']} st={i['status']} enc={i['enc_mode']} exit={i['exit_ip']} ({i['exit_country']}) pids={i['attached_pids']}\")"
echo "=== tünelden gerçek çıkış (kanıt) ==="
curl -s --max-time 25 --socks5-hostname 127.0.0.1:22000 "http://ip-api.com/json/?fields=query,country,countryCode,isp"
echo

# ---- temizlik ----
$AB close >/dev/null 2>&1
kill $P1 $P2 2>/dev/null
pkill -f metin2client 2>/dev/null
pkill -f "mvp_god_sss_tier serve" 2>/dev/null
ls -la kanit/*.png 2>/dev/null
echo "DEMO + EKRAN GÖRÜNTÜLERİ TAMAM"
