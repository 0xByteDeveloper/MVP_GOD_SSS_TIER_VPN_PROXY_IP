#!/bin/bash
# MVP_GOD_SSS_TIER — uçtan uca canlı test (TEK bash çağrısında çalışmalı:
# sandbox süreçleri çağrılar arasında ölür → motor + testler birlikte)
cd /home/z/my-project/godtier_vpn
./build.sh > /tmp/build_e2e.log 2>&1 || { echo "DERLEME HATASI"; tail -20 /tmp/build_e2e.log; exit 1; }

rm -rf data
./bin/mvp_god_sss_tier serve 7787 > /tmp/engine_e2e.log 2>&1 &
EPID=$!
sleep 1.5

echo "=== 1) durum ==="
curl -s --max-time 5 http://127.0.0.1:7787/api/status | head -c 400; echo

echo "=== 2) kaynak listesi (GitHub) ==="
SRCS=$(curl -s --max-time 30 "http://127.0.0.1:7787/api/sources?limit=6")
echo "$SRCS" | head -c 400; echo

echo "=== 3) OTO-YAKALA: capture'lı plain instance (manuel addr ile hızlı yol) ==="
ADDR=$(echo "$SRCS" | python3 -c "import json,sys; d=json.load(sys.stdin); ps=[p['addr'] for p in d['proxies'] if p['type']=='http']; print(ps[0] if ps else '')")
echo "kullanilacak proxy: $ADDR"
curl -s --max-time 60 -X POST http://127.0.0.1:7787/api/instances_create \
  -d "{\"provider\":\"plain\",\"addr\":\"$ADDR\",\"label\":\"oyuncu1\",\"enc\":\"auto\",\"capture\":\"metin2client.exe\",\"max_procs\":1}" \
  | head -c 400; echo

echo "=== 4) ikinci kutu (metin2client overflow hedefi) ==="
ADDR2=$(echo "$SRCS" | python3 -c "import json,sys; d=json.load(sys.stdin); ps=[p['addr'] for p in d['proxies'] if p['type']=='http']; print(ps[1] if len(ps)>1 else (ps[0] if ps else ''))")
curl -s --max-time 60 -X POST http://127.0.0.1:7787/api/instances_create \
  -d "{\"provider\":\"plain\",\"addr\":\"$ADDR2\",\"label\":\"oyuncu2\",\"enc\":\"plain\",\"capture\":\"metin2client.exe\",\"max_procs\":1}" \
  | head -c 400; echo

echo "=== 5) TOR kutusu (yerel yok → 3-zincir otomatik) ==="
curl -s --max-time 300 -X POST http://127.0.0.1:7787/api/instances_create \
  -d '{"provider":"tor","label":"toruser","enc":"tor3","capture":"","max_procs":2}' \
  | head -c 400; echo

echo "=== 6) metin2client.exe dummy süreçleri → OTO-YAKALA testi ==="
cat > /tmp/metin2client.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
int main() {
    for (int i = 0; i < 20; i++) {
        struct sockaddr_in sa;
        memset(&sa, 0, sizeof sa);
        sa.sin_family = AF_INET;
        sa.sin_port = htons(80);
        sa.sin_addr.s_addr = htonl(0x5D8AD822);   // 93.138.216.34 — herhangi dış IP
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(s, (struct sockaddr*)&sa, sizeof sa) == 0) {
            printf("baglandi #%d\n", i); fflush(stdout);
        }
        close(s);
        sleep(3);
    }
    return 0;
}
EOF
gcc -O2 -o /tmp/metin2client.exe /tmp/metin2client.c && echo "dummy hazır"
/tmp/metin2client.exe > /tmp/m2a.log 2>&1 & P1=$!
/tmp/metin2client.exe > /tmp/m2b.log 2>&1 & P2=$!
echo "spawn: p1=$P1 p2=$P2"
sleep 5   # monitor taraması (2sn) + oto-yakalama

echo "=== 7) instance durumu (attached_pids = oto-yakala sonucu) ==="
curl -s --max-time 8 http://127.0.0.1:7787/api/instances | python3 -c "
import json,sys
d=json.load(sys.stdin)
for i in d['instances']:
    print(f\"  inst#{i['id']} {i['label']} ({i['provider']}/{i.get('country','')}) port={i['port']} status={i['status']} enc={i.get('enc_mode')} capture={i.get('capture')!r} pids={i['attached_pids']}\")
"

echo "=== 8) oto-yakalama + bağlantı olayları (debug terminali kaynağı) ==="
curl -s --max-time 8 "http://127.0.0.1:7787/api/monitor/events" | python3 -c "
import json,sys
d=json.load(sys.stdin)
evs=d.get('events',[])
print('  toplam olay:', len(evs))
for e in evs[:14]:
    print(f\"  {e.get('ts','')} pid={e.get('pid')} {e.get('pname','')} -> {e.get('remote','')} {('[via '+e['via']+']') if e.get('via') else ''} {('[OTO]' if e.get('type')=='AUTO_ASSIGN' else '')}\")
"

echo "=== 9) tünel uçtan uca: curl --> SOCKS5 router --> dışarı ==="
for PORT in 22000 22001 22002; do
  R=$(curl -s --max-time 25 --socks5-hostname 127.0.0.1:$PORT "http://ip-api.com/json/?fields=query,country,countryCode,isp")
  echo "  port $PORT → $R"
done

echo "=== 10) motor içi check (socks_exit_ip_json) ==="
for ID in 1 2 3; do
  curl -s --max-time 30 -X POST http://127.0.0.1:7787/api/inst/$ID/check | head -c 260; echo
done

echo "=== 11) HOLA dene (sandbox IP'si WAF'ta olabilir — dürüst sonuç) ==="
curl -s --max-time 90 -X POST http://127.0.0.1:7787/api/instances_create \
  -d '{"provider":"hola","country":"TR","label":"holauser","enc":"tls13","capture":""}' \
  | head -c 300; echo

echo "=== 12) VPN Gate + ovpn export ==="
curl -s --max-time 60 "http://127.0.0.1:7787/api/vpngate?limit=4" | python3 -c "
import json,sys
d=json.load(sys.stdin)
print('  sunucular:', [(s['hostname'], s['country'], s['speed']) for s in d.get('servers',[])])
"
curl -s --max-time 60 -X POST http://127.0.0.1:7787/api/instances_create \
  -d '{"provider":"vpngate","country":"TR","label":"vg1","enc":"auto","capture":""}' | head -c 260; echo
ls -la data/vpngate/ 2>/dev/null | head -5

echo "=== 13) users geçmişi (gerçeklik logu) ==="
curl -s --max-time 8 http://127.0.0.1:7787/api/users/1 | python3 -c "
import json,sys
d=json.load(sys.stdin)
for e in d.get('events',[])[:6]: print('  ', e.get('ts'), e.get('type'), {k:v for k,v in e.items() if k not in ('ts','type')})
"

kill $EPID 2>/dev/null
echo "=== motor günlüğü (kanıt) ==="
grep -E "oto-yakala|CALISIYOR|cikis|zincir|hata|blocked" /tmp/engine_e2e.log | head -30
echo "E2E TAMAM"
