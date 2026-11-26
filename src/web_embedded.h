// ============================================================================
//  MVP_GOD_SSS_TIER — src/web_embedded.h   [OTOMATİK ÜRETİLDİ — el ile değiştirme]
//  web/ klasörünün exe içine gömülmüş kopyası (scripts/embed_web.py).
//  Tek .exe: web/ klasörü yanında olmasa da WebGUI çalışır (yedek kaynak).
// ============================================================================
#pragma once
#include <map>
#include <string>

namespace ui {
namespace embedded {

inline const std::map<std::string, std::string>& files() {
    static const std::map<std::string, std::string> m = {
        {"index.html", R"MVPGEN(<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="utf-8">
<title>MVP_GOD_SSS_TIER — Ücretsiz IP / Proxy / VPN Sistemi</title>
<link rel="stylesheet" href="style.css">
</head>
<body>

<!-- ================= ÜST BAR ================= -->
<header id="topbar">
  <div class="brand">
    <span class="logo">MVP<span class="tier">_GOD_</span>SSS<span class="vpn">_TIER</span></span>
    <span class="sub">Saf C++ motor + HTML yönetim — Hola · Tor · VPN Gate · zincir · PID yönlendirme</span>
  </div>
  <div class="createbar">
    <button id="btnAddSlot" class="primary">+ KUTU EKLE</button>
    <span class="dim" id="slotHint">kutular solda — boş kutunun AYARLAR'ı ile doldur</span>
  </div>
  <div id="chips" class="chips"></div>
</header>

<div id="layout">

  <!-- ================= SOL: INSTANCE (KUTU) IZGARASI ================= -->
  <aside id="instpanel">
    <div class="panel-title">Instance Kutuları <span id="instCount" class="count"></span></div>
    <div id="instGrid" class="instgrid"></div>
    <div class="panel-title" style="border-top:1px solid var(--line)">
      Sağlayıcılar (sömürüsüz)</div>
    <div class="side-block small">
      <div>· <b>HOLA</b>: her kutu = yeni ücretsiz kullanıcı (uuid bizim üretemiz);
        süre bitince <b>NEW USER MODE</b></div>
      <div>· <b>TOR</b>: yerel Tor SOCKS (9050) yoksa otomatik 3-zincir</div>
      <div>· <b>VPN GATE</b>: kamu VPN'leri — .ovpn export (OpenVPN istemcisi)</div>
      <div>· <b>PROXY LİSTESİ</b>: canlı GitHub listeleri (otomatik seçim)</div>
      <div>· <b>MANUEL</b>: socks/http ip:port elle girilebilir</div>
    </div>
  </aside>

  <!-- ================= MERKEZ: SEKMELER ================= -->
  <main id="center">
    <nav id="tabs">
      <button data-tab="dash" class="active">Instance</button>
      <button data-tab="procs">Süreç Yönlendir</button>
      <button data-tab="mon">Debug Terminal</button>
      <button data-tab="vgate">VPN Gate</button>
      <button data-tab="users">Kullanıcı Geçmişi</button>
      <button data-tab="srcs">Kaynaklar</button>
    </nav>

    <section id="tab-dash" class="tabpage active">
      <div id="dashBody">
        <div class="dbox" style="max-width:420px">
          <div class="k">Seçili kutu yok</div>
          <div class="s">Soldaki kare kutulardan birine tıklayın ya da boş kutunun
          <b>AYARLAR</b> düğmesiyle yeni instance oluşturun.</div>
        </div>
      </div>
    </section>

    <section id="tab-procs" class="tabpage">
      <div class="toolbar">
        <input id="procSearch" type="text" placeholder="search_textbox: program ismi veya PID yaz..." spellcheck="false">
        <button id="btnProcRefresh">Yenile</button>
        <span id="procCount" class="count"></span>
      </div>
      <div class="toolbar" style="background:var(--bg2);border:1px solid var(--line);border-radius:6px;padding:8px">
        <span class="dim">Seçili süreç →</span>
        <select id="procTargetInst" style="background:var(--bg);border:1px solid var(--line);color:var(--fg);padding:4px 6px"></select>
        <button id="btnAttach" class="primary mini">PID'Yİ İZLE</button>
        <span class="dim" style="margin-left:14px">veya</span>
        <input id="launchExe" type="text" placeholder="program yolu: C:\oyun\metin2client.exe / curl http://ip-api.com/json" spellcheck="false" style="flex:1;min-width:260px;background:var(--bg);border:1px solid var(--line);color:var(--fg);padding:5px 8px;border-radius:4px;font-family:Consolas,monospace">
        <button id="btnLaunch" class="warn mini">LAUNCH (hook ile başlat)</button>
      </div>
      <div class="side-block small" style="padding:0 0 8px">
        OTO-YAKALA: kutu ayarlarındaki desenler (ör: metin2client.exe) yeni açılan süreci
        otomatik boş kutuya bağlar — instance1 doluysa ilk açılan instance3'e gider.
      </div>
      <table class="gt"><thead><tr>
        <th>PID</th><th>Program</th><th>Komut satırı</th><th>Yönlendirme</th><th></th>
      </tr></thead><tbody id="procBody"></tbody></table>
    </section>

    <section id="tab-mon" class="tabpage">
      <div class="toolbar">
        <input id="monFilter" type="text" placeholder="filtre: pid / program / ip (boş = hepsi)" spellcheck="false">
        <label class="dim"><input type="checkbox" id="monOnlyVia"> yalnız yönlendirilenler</label>
        <button id="btnMonSnap">Anlık tablo</button>
        <span id="monCount" class="count"></span>
      </div>
      <div style="border:1px solid var(--line);border-radius:6px;max-height:40vh;overflow:auto" id="monEvents"></div>
      <div class="panel-title" style="margin-top:10px">Anlık TCP tablosu (tüm PC — VPN'siz de çalışır)</div>
      <table class="gt"><thead><tr>
        <th>PID</th><th>Program</th><th>Yerel</th><th>Uzak</th><th>Instance</th>
      </tr></thead><tbody id="monBody"></tbody></table>
    </section>

    <section id="tab-vgate" class="tabpage">
      <div class="toolbar">
        <input id="vgCountry" type="text" placeholder="ülke kodu (TR, DE, US... boş = hepsi)" spellcheck="false" style="max-width:220px">
        <button id="btnVgFetch" class="primary">VPN Gate listesini çek (kamu VPN'ler)</button>
        <span id="vgCount" class="count"></span>
      </div>
      <div class="side-block small" style="padding:0 0 8px">
        SoftEther VPN Gate projesinin gönüllü sunucuları — puan sırasına göre en
        iyiler. "KUTU YAP" ile instance kutusuna dönüşür (.ovpn export; gerçek
        tünel için OpenVPN istemcisi gerekir — telemetrysiz kullanım için
        motor config'i diskten verir).
      </div>
      <table class="gt"><thead><tr>
        <th>Sunucu</th><th>Ülke</th><th>Hız</th><th>Ping</th><th>Oturum</th><th>Uptime</th><th>Puan</th><th></th>
      </tr></thead><tbody id="vgBody"></tbody></table>
    </section>

    <section id="tab-users" class="tabpage">
      <div class="toolbar">
        <select id="userSel" style="background:var(--bg);border:1px solid var(--line);color:var(--fg);padding:5px 8px"></select>
        <button id="btnUsersRefresh">Yenile</button>
      </div>
      <table class="gt"><thead><tr>
        <th>Zaman</th><th>Olay</th><th>Detay</th>
      </tr></thead><tbody id="userBody"></tbody></table>
    </section>

    <section id="tab-srcs" class="tabpage">
      <div class="toolbar">
        <select id="srcType" style="background:var(--bg);border:1px solid var(--line);color:var(--fg);padding:5px 8px">
          <option value="">hepsi</option>
          <option value="http">http (plain)</option>
          <option value="socks5">socks5 (tor/manuel gibi)</option>
          <option value="socks4">socks4</option>
        </select>
        <button id="btnSrcs" class="primary">Kaynaklardan çek (monosans + speedx + proxyscrape)</button>
        <span id="srcCount" class="count"></span>
      </div>
      <div class="side-block small" style="padding:0 0 8px">
        x1 araştırmasındaki ücretsiz proxy depoları — satırdaki "KUTU YAP" ile
        instance olur (http → düz proxy kutusu, socks5 → SOCKS5 kutusu).
      </div>
      <table class="gt"><thead><tr>
        <th>Adres</th><th>Tür</th><th>Kaynak</th><th></th>
      </tr></thead><tbody id="srcBody"></tbody></table>
    </section>
  </main>

  <!-- ================= SAĞ: DURUM + GLOBAL ================= -->
  <aside id="sidepanel">
    <div class="panel-title">Motor</div>
    <div class="side-block" id="engineBox">—</div>
    <div class="panel-title">TÜM PC Modu</div>
    <div class="side-block">
      <div class="switchrow">
        <div id="swGlobal" class="switch" title="Windows: WinInet sistem proxy · Linux: çocuk süreçler env"></div>
        <span id="swGlobalLbl" class="dim">kapalı</span>
      </div>
      <div class="side-block small" style="padding:6px 0 0">
        Tüm bilgisayarın trafiği seçili instance'dan geçer.
        Windows'ta WinInet sistem proxy'si kurulur; Linux'ta launch ile başlatılan
        süreçler env üzerinden bağlanır.
      </div>
      <select id="globalInst" style="width:100%;margin-top:6px;background:var(--bg);border:1px solid var(--line);color:var(--fg);padding:4px 6px"></select>
    </div>
    <div class="panel-title">Şifreleme / Kurallar</div>
    <div class="side-block small">
      <div>· Hola trial portu: 22225 — 80 için absolute-form</div>
      <div>· Şifreleme: kutu AYARLAR → TLS1.3/1.2, zincir 2-3 katman</div>
      <div>· blocked=true → motor 8sn bekleyip yeni uuid dener</div>
      <div>· Kullanıcı gerçekliği: data/logs/users/*.jsonl</div>
      <div>· Kimlikler (uuid) motor tarafından üretilir — istemci yazılımı yok</div>
    </div>
  </aside>
</div>

<!-- ================= AYARLAR MODALI ================= -->
<div id="modal" class="modalwrap">
  <div class="modalbox">
    <div class="modal-title">KUTU AYARLARI <span id="modalSlotLbl" class="dim"></span></div>

    <div class="mrow">
      <label class="mk">Sağlayıcı</label>
      <select id="mProvider" class="mv">
        <option value="hola">HOLA VPN — yeni kullanıcı trial (uuid bizde)</option>
        <option value="plain">ÜCRETSİZ PROXY LİSTESİ — otomatik canlı seçim</option>
        <option value="socks">MANUEL SOCKS5 — ip:port gir</option>
        <option value="tor">TOR — yerel SOCKS yoksa 3-zincir (Tor benzeri)</option>
        <option value="chain">ZİNCİR — 2/3 katman (Tor mantığında katmanlama)</option>
        <option value="vpngate">VPN GATE — kamu VPN (ovpn export)</option>
      </select>
      <div id="mProviderNote" class="mnote"></div>
    </div>

    <div class="mrow" id="rowCountry">
      <label class="mk">Ülke</label>
      <select id="mCountry" class="mv"></select>
    </div>

    <div class="mrow" id="rowVgServer" style="display:none">
      <label class="mk">VPN Gate sunucusu</label>
      <select id="mVgServer" class="mv"></select>
      <button id="btnVgLoad" class="mini">listeyi çek</button>
    </div>

    <div class="mrow" id="rowAddr" style="display:none">
      <label class="mk">Manuel adres</label>
      <input id="mAddr" class="mv" type="text" placeholder="1.2.3.4:8080 (boş = otomatik seçim)" spellcheck="false">
    </div>

    <div class="mrow" id="rowHops" style="display:none">
      <label class="mk">Zincir hops</label>
      <input id="mHops" class="mv" type="text" placeholder="boş = otomatik canlı seçim — veya ip:port,ip:port" spellcheck="false">
    </div>

    <div class="mrow">
      <label class="mk">Etiket</label>
      <input id="mLabel" class="mv" type="text" placeholder="x1 (boş = otomatik)" spellcheck="false">
    </div>

    <div class="mrow">
      <label class="mk">Şifreleme</label>
      <select id="mEnc" class="mv"></select>
      <div id="mEncNote" class="mnote"></div>
    </div>

    <div class="mrow">
      <label class="mk">Oto-yakala (süreç adı)</label>
      <input id="mCapture" class="mv" type="text" placeholder="metin2client.exe, game.exe (virgülle)" spellcheck="false">
    </div>

    <div class="mrow">
      <label class="mk">Kutu süreç kapasitesi</label>
      <input id="mMaxProcs" class="mv" type="number" min="1" max="16" value="1" style="max-width:90px">
      <span class="mnote">bu kutuya aynı anda bağlanacak süreç sayısı (doluysa sıradaki kutu)</span>
    </div>

    <div class="modal-btns">
      <button id="btnModalSave" class="primary">KUTUYU DOLDUR (instance oluştur)</button>
      <button id="btnModalCancel">vazgeç</button>
    </div>
    <div id="modalErr" class="modalerr"></div>
  </div>
</div>

<!-- ================= ALT: DEBUG KONSOL ================= -->
<footer id="consolebar">
  <div class="console-title">
    <span>Debug Konsolu (toplu kayıt: metin seç → kopyala)</span>
    <button id="btnConClear">temizle</button>
    <label><input type="checkbox" id="conAuto" checked> otomatik akış</label>
    <span id="conSeq" style="margin-left:auto"></span>
  </div>
  <div id="console" class="console"></div>
</footer>

<div id="busy"></div>
<script src="app.js"></script>
</body>
</html>
)MVPGEN"},
        {"app.js", R"MVPGEN(// ============================================================================
//  MVP_GOD_SSS_TIER — web/app.js (vanilla JS — derleme gerektirmez)
//  C++ motorunun /api/* uçlarıyla konuşur. Referans: MVP_GOD_SSS_TIER
//  SIMULATOR_UI app.js deseni (polling akışları, sekme yönetimi).
// ============================================================================
"use strict";

// ---- yardımcılar -------------------------------------------------------------
const $ = (id) => document.getElementById(id);
const esc = (s) => String(s ?? "").replace(/[&<>"]/g,
  (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));

async function api(path, opts) {
  const r = await fetch(path, opts);
  return r.json();
}
async function apiPost(path, body) {
  return api(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body || {}),
  });
}

const PROV_NAMES = {
  hola: "HOLA VPN", plain: "PROXY LİSTESİ", socks: "MANUEL SOCKS5",
  tor: "TOR", chain: "ZİNCİR", vpngate: "VPN GATE",
};
const PROV_NOTES = {
  hola: "uuid'yi motor üretir (istemci yok, telemetry yok) — zgettunnels → trial tüneli",
  plain: "GitHub listelerinden canlı test edilmiş tek düz proxy (boş bırak = otomatik seç)",
  socks: "herhangi bir SOCKS5 sunucusu — ip:port gir",
  tor: "yerel Tor (9050/9150) varsa gerçek Tor ağı; yoksa 3-hop zincir (Tor benzeri)",
  chain: "2-3 bağımsız çıkış üst üste — katman başına ~+40ms",
  vpngate: "kamu VPN geçidi — .ovpn dışa aktarılır (OpenVPN istemcisiyle tünellenir)",
};

let state = {
  instances: [],
  selected: null,
  consoleSeq: 0,
  monSeq: 0,
  countries: [],
  encOptions: [],
  lastEvents: [],
  slots: [],              // [instanceId|null, ...] — kutu sıraları
  vgServers: [],
};

// ---- kutu (slot) yönetimi --------------------------------------------------------
const SLOT_KEY = "mvp_god_sss_tier_slots";
function loadSlots() {
  try { state.slots = JSON.parse(localStorage.getItem(SLOT_KEY) || "[]"); } catch (e) { state.slots = []; }
  if (!state.slots.length) state.slots = [null, null, null, null, null, null];
}
function saveSlots() { localStorage.setItem(SLOT_KEY, JSON.stringify(state.slots)); }
$("btnAddSlot").onclick = () => { state.slots.push(null); saveSlots(); renderTiles(); };

function slotOfInstance(id) { return state.slots.indexOf(id); }

function renderTiles() {
  const grid = $("instGrid");
  let filled = 0;
  grid.innerHTML = state.slots.map((slotId, idx) => {
    const inst = slotId ? state.instances.find((i) => i.id === slotId) : null;
    if (inst) filled++;
    if (!inst) {
      // boş kutu (default) — AYARLAR ile doldurulur
      return `<div class="tile empty" data-slot="${idx}">
        <div class="tile-top"><span class="tile-lbl dim">KUTU #${idx + 1}</span></div>
        <div class="tile-mid"><span class="tile-big dim">BOŞ</span></div>
        <div class="tile-note dim">sağlayıcı + ülke + şifreleme seç</div>
        <button class="btn-ayarlar primary" data-slot="${idx}">AYARLAR</button>
      </div>`;
    }
    const enc = inst.enc_mode || "auto";
    const cap = inst.capture
      ? `<div class="tile-cap" title="oto-yakalama desenleri">🎯 ${esc(inst.capture)}</div>` : "";
    const prov = PROV_NAMES[inst.provider] || inst.provider;
    const status = inst.status === "OVPN"
      ? `<span class="st OVPN">OVPN</span>`
      : `<span class="st ${esc(inst.status)}">${esc(inst.status)}</span>`;
    return `<div class="tile ${state.selected === inst.id ? "sel" : ""}" data-id="${inst.id}">
      <div class="tile-top">
        <span class="tile-lbl">${esc(inst.label)}</span>
        <span class="ctry">${esc(inst.country || "")}</span>
        ${status}
      </div>
      <div class="tile-mid">
        <span class="tile-big ip">${esc(inst.exit_ip || "…")}</span>
        <span class="tile-prov">${prov}</span>
      </div>
      <div class="tile-note dim">
        :${inst.port} · ${esc(inst.conns || 0)}c · ${esc(enc)}
        ${inst.max_procs > 1 ? ` · max ${inst.max_procs}` : ""}
      </div>
      ${cap}
      <div class="tile-btns">
        <button class="mini" data-act="ayarlar" data-id="${inst.id}">AYARLAR</button>
        ${inst.provider === "hola" ? `<button class="mini warn" data-act="newuser" data-id="${inst.id}">NEW USER</button>` : ""}
        <button class="mini danger" data-act="bosalt" data-id="${inst.id}">✕</button>
      </div>
    </div>`;
  }).join("");
  $("instCount").textContent = filled + "/" + state.slots.length + " dolu";

  // kutu olayları
  document.querySelectorAll(".tile.empty .btn-ayarlar").forEach((b) => {
    b.onclick = (e) => { e.stopPropagation(); openModal(+b.dataset.slot, null); };
  });
  document.querySelectorAll(".tile:not(.empty)").forEach((t) => {
    t.onclick = (e) => {
      if (e.target.dataset.act) return;
      state.selected = +t.dataset.id;
      renderTiles(); renderDash(); fillInstSelects();
    };
  });
  document.querySelectorAll(".tile button[data-act]").forEach((b) => {
    b.onclick = async (e) => {
      e.stopPropagation();
      const id = +b.dataset.id;
      const act = b.dataset.act;
      if (act === "bosalt") {
        if (!confirm("kutudaki instance kapatılsın mı? (kutu boşalır)")) return;
        await api("/api/inst/" + id, { method: "DELETE" });
        const s = slotOfInstance(id);
        if (s >= 0) state.slots[s] = null;
        saveSlots();
      } else if (act === "newuser") {
        busy("yeni kullanıcı üretiliyor (uuid + zgettunnels)...");
        await apiPost("/api/inst/" + id + "/newuser");
        busyOff();
      } else if (act === "ayarlar") {
        openModal(slotOfInstance(id), id);
        return;
      }
      await pollInstances(true);
    };
  });
}

function fmtB(b) {
  if (b > 1048576) return (b / 1048576).toFixed(1) + "M";
  if (b > 1024) return (b / 1024).toFixed(1) + "K";
  return String(b);
}

async function pollInstances(force) {
  const d = await api("/api/instances");
  state.instances = d.instances || [];
  // yeni instance'ları boş kutuya yerleştir (kutu doldu görünümü)
  for (const i of state.instances) {
    if (slotOfInstance(i.id) < 0) {
      const free = state.slots.indexOf(null);
      if (free >= 0) state.slots[free] = i.id; else state.slots.push(i.id);
      saveSlots();
    }
  }
  renderTiles();
  if (state.selected) renderDash();
  fillInstSelects();
  renderChips();
}

function fillInstSelects() {
  const opts = state.instances
    .map((i) => `<option value="${i.id}">#${i.id} ${esc(i.label)} (${esc(i.country || i.provider)}) :${i.port}</option>`)
    .join("");
  $("procTargetInst").innerHTML = opts;
  $("globalInst").innerHTML = opts;
}

// ---- üst cips ------------------------------------------------------------------
function renderChips() {
  const n = state.instances.length;
  const anyIp = state.instances.find((i) => i.exit_ip);
  const tor = state.torLocal ? `tor <b>${esc(state.torLocal)}</b>` : "tor: zincir";
  $("chips").innerHTML =
    `<span class="chip"><b>${n}</b> instance · <b>${state.slots.length}</b> kutu</span>` +
    `<span class="chip">motor <b>Saf C++17</b></span>` +
    (anyIp ? `<span class="chip on">çıkış <b>${esc(anyIp.exit_ip)}</b></span>` : "") +
    `<span class="chip">${tor}</span>` +
    `<span class="chip">ülke <b>${state.countries.length || 246}</b></span>`;
}

// ---- instance detayı ------------------------------------------------------------
function renderDash() {
  const i = state.instances.find((x) => x.id === state.selected);
  if (!i) {
    $("dashBody").innerHTML =
      '<div class="dbox" style="max-width:420px"><div class="k">Seçili kutu yok</div>' +
      '<div class="s">Soldaki kare kutulardan birini tıklayın.</div></div>';
    return;
  }
  const provName = PROV_NAMES[i.provider] || i.provider;
  const hops = (i.hops || []).length
    ? (i.hops || []).map((h) => esc(h)).join(" → ") : null;
  $("dashBody").innerHTML = `
    <div class="dgrid">
      <div class="dbox"><div class="k">Kutu / etiket</div>
        <div class="v">${esc(i.label)} <span class="dim">#${i.id}</span></div>
        <div class="s">${provName} · ${esc(i.provider)}</div></div>
      <div class="dbox"><div class="k">Çıkış IP</div>
        <div class="v big">${esc(i.exit_ip || "?")}</div>
        <div class="s">${esc(i.exit_country || "")} ${esc(i.exit_isp || "")}</div></div>
      <div class="dbox"><div class="k">Yerel SOCKS5 / HTTP</div>
        <div class="v">127.0.0.1:${i.port}</div>
        <div class="s">socks5 veya http proxy olarak kullanılabilir</div></div>
      <div class="dbox"><div class="k">Şifreleme</div>
        <div class="v">${esc(i.enc_mode || "auto")}</div>
        <div class="s">tls sürüm / zincir katmanı</div></div>
      <div class="dbox"><div class="k">Oto-yakala</div>
        <div class="v">${esc(i.capture || "—")}</div>
        <div class="s">kapasite ${i.max_procs} süreç · doluysa sıradaki kutuya</div></div>
      <div class="dbox"><div class="k">Ülke</div>
        <div class="v">${esc(i.country || "—")}</div>
        <div class="s">${i.provider === "vpngate" ? "VPN Gate sunucu ülkesi" : "zgettunnels country="}</div></div>
      <div class="dbox"><div class="k">Kimlik (uuid)</div>
        <div class="v">${i.uuid ? esc(i.uuid.slice(0, 8)) + "…" : "—"}</div>
        <div class="s">${i.uuid ? esc(i.login || "") : "sağlayıcı kimliği yok"}</div></div>
      <div class="dbox"><div class="k">Trafik</div>
        <div class="v">${i.conns} bağlantı</div>
        <div class="s">↑ ${fmtB(i.bytes_up)} · ↓ ${fmtB(i.bytes_down)}</div></div>
      <div class="dbox"><div class="k">Yenileme</div>
        <div class="v">${i.refresh_count || 0}×</div>
        <div class="s">NEW USER MODE / refresh sayacı</div></div>
      <div class="dbox"><div class="k">Gerçeklik profili</div>
        <div class="v">${esc(i.profile_style || "")}</div>
        <div class="s">etkinlik ${i.profile_activity || 0}% · oturum ${i.profile_session_min || 0}dk</div></div>
      ${hops ? `<div class="dbox"><div class="k">Zincir yolu</div>
        <div class="s">${hops}</div></div>` : ""}
      ${i.vpngate_hostname ? `<div class="dbox"><div class="k">VPN Gate sunucusu</div>
        <div class="v">${esc(i.vpngate_hostname)}</div>
        <div class="s">ovpn: ${esc(i.vpngate_ovpn_path || "")}</div></div>` : ""}
      <div class="dbox"><div class="k">Uç noktalar (zagent)</div>
        <div class="s">${(i.endpoints || []).map((e) => esc(e.ip + ":" + e.port)).join("<br>") || "—"}</div></div>
      <div class="dbox"><div class="k">Süreçler (attach)</div>
        <div class="s">${(i.attached_pids || []).join(", ") || "—"} ${esc((i.attached_names || []).join(" "))}</div></div>
    </div>
    <div class="toolbar" style="margin-top:12px">
      ${i.provider === "hola" ? `<button class="warn" onclick="apiPost('/api/inst/${i.id}/newuser')">NEW USER MODE (ücretsiz süre bitti)</button>` : ""}
      <button onclick="apiPost('/api/inst/${i.id}/check')">Çıkış IP kontrol (uçtan uca)</button>
      <button onclick="apiPost('/api/inst/${i.id}/refresh')">refresh</button>
    </div>`;
}

// ---- AYARLAR MODALI -------------------------------------------------------------------
let modal = { slot: -1, editId: 0, vgServer: null };

function openModal(slotIdx, editId) {
  modal = { slot: slotIdx, editId: editId || 0, vgServer: null };
  $("modalErr").textContent = "";
  $("modalSlotLbl").textContent = slotIdx >= 0 ? `— KUTU #${slotIdx + 1}` : "";
  $("mProvider").disabled = !!editId;
  $("mProvider").value = editId
    ? (state.instances.find((i) => i.id === editId) || {}).provider || "hola"
    : "hola";
  const editInst = editId ? state.instances.find((i) => i.id === editId) : null;
  $("mLabel").value = editInst ? editInst.label : "";
  $("mCapture").value = editInst ? editInst.capture || "" : "";
  $("mMaxProcs").value = editInst ? editInst.max_procs || 1 : 1;
  $("mAddr").value = editInst ? (editInst.upstream_addr || "") : "";
  $("mHops").value = editInst ? (editInst.hops || []).join(",") : "";
  $("mEnc").value = editInst ? (editInst.enc_mode || "auto") : "auto";
  if (editInst && editInst.country) $("mCountry").value = editInst.country;
  $("btnModalSave").textContent = editId ? "AYARLARI GÜNCELLE" : "KUTUYU DOLDUR (instance oluştur)";
  providerUI();
  encNote();
  $("modal").classList.add("on");
}
$("btnModalCancel").onclick = () => $("modal").classList.remove("on");
$("modal").onclick = (e) => {
  if (e.target.id === "modal") $("modal").classList.remove("on");
};

function providerUI() {
  const p = $("mProvider").value;
  $("mProviderNote").textContent = PROV_NOTES[p] || "";
  $("rowCountry").style.display = p === "hola" ? "" : "none";
  $("rowVgServer").style.display = p === "vpngate" ? "" : "none";
  $("rowAddr").style.display = p === "plain" || p === "socks" ? "" : "none";
  $("rowHops").style.display = p === "chain" ? "" : "none";
  if (p === "vpngate" && !$("mVgServer").options.length) loadVgServersIntoModal();
}

$("mProvider").onchange = providerUI;

function encNote() {
  const id = $("mEnc").value;
  const e = state.encOptions.find((x) => x.id === id);
  $("mEncNote").textContent = e ? `${e.latency} · uyum: ${e.compat} — ${e.note}` : "";
}
$("mEnc").onchange = encNote;

async function loadEnc() {
  const d = await api("/api/enc");
  state.encOptions = d.options || [];
  $("mEnc").innerHTML = state.encOptions
    .map((o) => `<option value="${esc(o.id)}">${esc(o.name)}</option>`)
    .join("");
  $("mEnc").value = "auto";
  encNote();
}

async function loadVgServersIntoModal() {
  const country = $("mCountry").value || "";
  const d = await api("/api/vpngate?country=" + encodeURIComponent(country) + "&limit=15");
  state.vgServers = d.servers || [];
  $("mVgServer").innerHTML = state.vgServers
    .map((s) => `<option value="${esc(s.hostname)}" data-idx="${state.vgServers.indexOf(s)}">` +
      `${esc(s.hostname)} — ${esc(s.country)} ${esc(s.speed)} ping ${s.ping_ms}ms</option>`)
    .join("") || '<option value="">(liste boş — ülkeyi değiştir)</option>';
}

$("btnVgLoad").onclick = loadVgServersIntoModal;
$("mCountry").onchange = () => {
  if ($("mProvider").value === "vpngate") loadVgServersIntoModal();
};

$("btnModalSave").onclick = async () => {
  $("modalErr").textContent = "";
  if (modal.editId) {
    // yalnız kutu ayarları (etiket/oto-yakala/kapasite)
    const r = await apiPost("/api/inst/" + modal.editId + "/settings", {
      capture: $("mCapture").value.trim(),
      max_procs: +$("mMaxProcs").value || 1,
      label: $("mLabel").value.trim(),
    });
    if (r.err) { $("modalErr").textContent = r.err; return; }
    $("modal").classList.remove("on");
    await pollInstances(true);
    return;
  }
  const provider = $("mProvider").value;
  const body = {
    provider,
    country: provider === "vpngate" ? $("mCountry").value : $("mCountry").value,
    label: $("mLabel").value.trim(),
    enc: $("mEnc").value,
    capture: $("mCapture").value.trim(),
    max_procs: +$("mMaxProcs").value || 1,
    addr: $("mAddr").value.trim(),
    hops: $("mHops").value.trim()
      ? $("mHops").value.trim().split(",").map((s) => s.trim()).filter(Boolean)
      : [],
  };
  if (provider === "vpngate") {
    const sel = $("mVgServer");
    const opt = sel.selectedOptions[0];
    const idx = opt ? +opt.dataset.idx : -1;
    const server = idx >= 0 ? state.vgServers[idx] : state.vgServers[0];
    if (!server) { $("modalErr").textContent = "VPN Gate sunucusu seçilmedi"; return; }
    body.vpngate_hostname = server.hostname;
    body.vpngate_ovpn_b64 = server.ovpn_b64;
    body.country = server.country;
  }
  if (provider === "tor") body.country = "";
  busy("kutu dolduruluyor — C++ motoru sağlayıcı kuruyor...");
  const r = await apiPost("/api/instances_create", body);
  busyOff();
  if (r.err) { $("modalErr").textContent = r.err; return; }
  if (modal.slot >= 0) state.slots[modal.slot] = r.id;
  else state.slots.push(r.id);
  saveSlots();
  state.selected = r.id;
  $("modal").classList.remove("on");
  await pollInstances(true);
  renderDash();
};

// ---- ülke listesi -------------------------------------------------------------
async function loadCountries() {
  const list = await api("/api/countries");
  state.countries = list;
  $("mCountry").innerHTML = list
    .map((c) => `<option value="${c.code}">${c.code} — ${esc(c.name)}</option>`)
    .join("");
  $("mCountry").value = "TR";
}

// ---- sekmeler ----------------------------------------------------------------
document.querySelectorAll("#tabs button").forEach((b) => {
  b.onclick = () => {
    document.querySelectorAll("#tabs button").forEach((x) => x.classList.remove("active"));
    document.querySelectorAll(".tabpage").forEach((x) => x.classList.remove("active"));
    b.classList.add("active");
    $("tab-" + b.dataset.tab).classList.add("active");
    if (b.dataset.tab === "procs") refreshProcs();
    if (b.dataset.tab === "users") fillUserSelect();
    if (b.dataset.tab === "mon") monSnapshot();
    if (b.dataset.tab === "vgate") fetchVgTable();
  };
});

// ---- süreç listesi -----------------------------------------------------------------
let procAll = [];
async function refreshProcs() {
  const d = await api("/api/processes");
  procAll = d.processes || [];
  renderProcs();
}
$("procSearch").oninput = renderProcs;
$("btnProcRefresh").onclick = refreshProcs;

let procSelPid = null;
function renderProcs() {
  const q = $("procSearch").value.toLowerCase();
  const list = procAll.filter(
    (p) => !q || p.name.toLowerCase().includes(q) ||
            String(p.pid).includes(q) || (p.cmdline || "").toLowerCase().includes(q)
  );
  $("procCount").textContent = list.length + " / " + procAll.length;
  $("procBody").innerHTML = list
    .slice(0, 400)
    .map((p) => {
      const inst = state.instances.find((i) => (i.attached_pids || []).includes(p.pid));
      return `<tr class="${procSelPid === p.pid ? "sel" : ""}" data-pid="${p.pid}">
        <td class="acc">${p.pid}</td>
        <td>${esc(p.name)}</td>
        <td class="dim" style="max-width:380px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">${esc(p.cmdline || "")}</td>
        <td>${inst ? `<span class="ok">#${inst.id} ${esc(inst.label)} (${esc(inst.country || inst.provider)})</span>` : '<span class="dim">—</span>'}</td>
        <td><button class="mini" data-pid="${p.pid}">${procSelPid === p.pid ? "seçili" : "seç"}</button></td>
      </tr>`;
    })
    .join("");
  document.querySelectorAll("#procBody button[data-pid]").forEach((b) => {
    b.onclick = () => { procSelPid = +b.dataset.pid; renderProcs(); };
  });
  document.querySelectorAll("#procBody tr[data-pid]").forEach((tr) => {
    tr.onclick = (e) => { if (e.target.tagName !== "BUTTON") { procSelPid = +tr.dataset.pid; renderProcs(); } };
  });
}

// PID'Yİ İZLE (attach)
$("btnAttach").onclick = async () => {
  if (!procSelPid) return alert("önce tablodan bir süreç seç");
  const id = +$("procTargetInst").value;
  const name = (procAll.find((p) => p.pid === procSelPid) || {}).name || "";
  await apiPost("/api/inst/" + id + "/attach", { pid: procSelPid, name });
  await pollInstances(true);
};

// LAUNCH: hook ile yeni süreç başlat (Linux LD_PRELOAD / Windows inject)
$("btnLaunch").onclick = async () => {
  const id = +$("procTargetInst").value;
  const line = $("launchExe").value.trim();
  if (!line) return alert("program satırı boş — ör: metin2client.exe ya da curl http://ip-api.com/json");
  const sp = line.indexOf(" ");
  const exe = sp < 0 ? line : line.slice(0, sp);
  const args = sp < 0 ? "" : line.slice(sp + 1);
  const r = await apiPost("/api/inst/" + id + "/launch", { exe, args });
  if (r.err) alert("launch hatası: " + r.err);
  else {
    busy("başlatıldı (pid " + r.pid + ") — trafik kutudan çıkıyor");
    setTimeout(() => busyOff(), 900);
  }
  refreshProcs();
  await pollInstances(true);
};

// ---- debug terminal (monitor) --------------------------------------------------------
function monLine(e) {
  const cls = e.type === "AUTO_ASSIGN" ? "autol" : "evline";
  const tag = e.type === "AUTO_ASSIGN" ? `<span class="mag">OTO-YAKALA</span> ` : "";
  return `<div class="${cls}"><span class="t">${esc(e.ts)}</span> ` +
    tag +
    `<span class="pid">pid ${e.pid}</span> <span class="rmt">${esc(e.pname || "")}</span> ` +
    `→ <span class="rmt">${esc(e.remote)}</span>` +
    (e.via ? ` <span class="via">[${esc(e.via)}]</span>` : "") +
    `</div>`;
}
async function pollMonitor() {
  const d = await api("/api/monitor/events?since=" + state.monSeq);
  if (d.events && d.events.length) {
    state.monSeq = d.seq;
    state.lastEvents = state.lastEvents.concat(d.events).slice(-400);
    renderMonEvents();
  }
}
function renderMonEvents() {
  const q = $("monFilter").value.toLowerCase();
  const onlyVia = $("monOnlyVia").checked;
  const shown = state.lastEvents.filter(
    (e) => (!onlyVia || e.via || e.type === "AUTO_ASSIGN") &&
           (!q || String(e.pid).includes(q) ||
            (e.pname || "").toLowerCase().includes(q) ||
            (e.remote || "").toLowerCase().includes(q))
  );
  $("monCount").textContent = shown.length + " olay";
  $("monEvents").innerHTML =
    shown.slice(-200).reverse().map(monLine).join("") ||
    '<div class="side-block small">bağlantı olayı yok — tablo doldukça akar</div>';
}
$("monFilter").oninput = renderMonEvents;
$("monOnlyVia").onchange = renderMonEvents;
async function monSnapshot() {
  const d = await api("/api/monitor/connections");
  $("monBody").innerHTML = (d.connections || [])
    .map((c) => `<tr>
      <td class="acc">${c.pid}</td><td>${esc(c.pname || "?")}</td>
      <td class="dim">${esc(c.local)}</td><td>${esc(c.remote)}:${c.port}</td>
      <td>${c.via ? `<span class="mag">${esc(c.via)}</span>` : '<span class="dim">—</span>'}</td>
    </tr>`)
    .join("") || '<tr><td colspan="5" class="dim">bağlantı yok</td></tr>';
}
$("btnMonSnap").onclick = monSnapshot;

// ---- VPN Gate sekmesi ------------------------------------------------------------------
async function fetchVgTable() {
  const country = $("vgCountry").value.trim().toUpperCase();
  busy("VPN Gate listesi çekiliyor (vpngate.net API)...");
  const d = await api("/api/vpngate?country=" + encodeURIComponent(country) + "&limit=15");
  busyOff();
  const list = d.servers || [];
  state.vgServers = list;
  $("vgCount").textContent = list.length + " sunucu" + (d.err ? " (" + d.err + ")" : "");
  $("vgBody").innerHTML = list
    .map((s, i) => `<tr>
      <td class="acc">${esc(s.hostname)}</td>
      <td>${esc(s.country)} <span class="dim">${esc(s.country_long || "")}</span></td>
      <td class="acc">${esc(s.speed)}</td>
      <td class="dim">${s.ping_ms}ms</td>
      <td class="dim">${s.sessions}</td>
      <td class="dim">${s.uptime_days}g</td>
      <td class="dim">${s.score}</td>
      <td><button class="mini primary" data-vg="${i}">KUTU YAP</button></td>
    </tr>`)
    .join("") || '<tr><td colspan="8" class="dim">sunucu yok — ülke kodunu kontrol et</td></tr>';
  document.querySelectorAll("#vgBody button[data-vg]").forEach((b) => {
    b.onclick = () => {
      const s = state.vgServers[+b.dataset.vg];
      if (!s) return;
      // AYARLAR modalını vpngate+hazır sunucu ile aç
      openModal(-1, 0);
      $("mProvider").disabled = false;
      $("mProvider").value = "vpngate";
      providerUI();
      $("mCountry").value = s.country || "TR";
      loadVgServersIntoModal().then(() => {
        state.vgServers = [s].concat(state.vgServers);
        $("mVgServer").innerHTML =
          `<option value="${esc(s.hostname)}" data-idx="0" selected>` +
          `${esc(s.hostname)} — ${esc(s.country)} ${esc(s.speed)} ping ${s.ping_ms}ms</option>`;
      });
    };
  });
}
$("btnVgFetch").onclick = fetchVgTable;

// ---- kullanıcı geçmişi ------------------------------------------------------------------
function fillUserSelect() {
  $("userSel").innerHTML = state.instances
    .map((i) => `<option value="${i.id}">#${i.id} ${esc(i.label)} — ${esc(i.uuid ? i.uuid.slice(0, 8) : i.provider)}</option>`)
    .join("");
}
$("btnUsersRefresh").onclick = loadUserLog;
$("userSel").onchange = loadUserLog;
async function loadUserLog() {
  const id = +$("userSel").value || (state.instances[0] || {}).id;
  if (!id) return;
  const d = await api("/api/users/" + id);
  $("userBody").innerHTML = (d.events || [])
    .map((e) => `<tr>
      <td class="dim">${esc(e.ts)}</td>
      <td class="${typeClass(e.type)}">${esc(e.type)}</td>
      <td>${esc(logDetail(e))}</td>
    </tr>`)
    .join("") || '<tr><td colspan="3" class="dim">geçmiş yok</td></tr>';
}
function typeClass(t) {
  if (t === "CREATED" || t === "TUNNELS_OK" || t === "EXIT_IP") return "ok";
  if (t === "NEWUSER" || t === "SETTINGS") return "warn";
  if (t === "DESTROYED") return "err";
  return "acc";
}
function logDetail(e) {
  const parts = [];
  for (const k of ["uuid", "country", "provider", "enc", "capture", "agent_key",
                   "endpoint", "hops", "ip", "pid", "name", "detail", "ovpn"])
    if (e[k] !== undefined && e[k] !== null && e[k] !== "") parts.push(`${k}=${e[k]}`);
  return parts.join(" ") || JSON.stringify(e);
}

// ---- kaynaklar (x1 depoları) -------------------------------------------------------------
$("btnSrcs").onclick = loadSources;
async function loadSources() {
  const type = $("srcType").value;
  busy("proxy listeleri çekiliyor (GitHub raw)...");
  const d = await api("/api/sources?limit=80" + (type ? "&type=" + type : ""));
  busyOff();
  const list = d.proxies || [];
  $("srcCount").textContent = list.length + " adet";
  $("srcBody").innerHTML = list
    .map((p) => `<tr data-addr="${esc(p.addr)}" data-type="${esc(p.type)}">
      <td class="acc">${esc(p.addr)}</td><td class="dim">${esc(p.type)}</td>
      <td class="mag">${esc(p.source)}</td>
      <td><button class="mini primary" ${p.type === "socks4" ? "disabled title='socks4 desteklenmiyor'" : ""}>KUTU YAP</button></td>
    </tr>`)
    .join("");
  document.querySelectorAll("#srcBody tr[data-addr] button").forEach((b) => {
    b.onclick = async () => {
      const tr = b.closest("tr");
      const addr = tr.dataset.addr;
      const type = tr.dataset.type;
      const provider = type === "socks5" ? "socks" : "plain";
      busy(provider === "socks" ? "SOCKS5 kutusu kuruluyor..." : "düz proxy kutusu kuruluyor...");
      await apiPost("/api/instances_create", {
        provider, addr,
        label: "p" + Math.floor(Math.random() * 90 + 10),
        enc: provider === "socks" ? "plain" : "auto",
      });
      busyOff();
      await pollInstances(true);
    };
  });
}

// ---- global anahtarı ---------------------------------------------------------------------
$("swGlobal").onclick = async () => {
  const on = !$("swGlobal").classList.contains("on");
  const id = +$("globalInst").value || (state.instances[0] || {}).id;
  if (on && !id) return alert("önce instance gerek");
  await apiPost("/api/global", { on, id });
  await pollStatus();
};
async function pollStatus() {
  const s = await api("/api/status");
  state.torLocal = s.tor_local ? s.tor_local_txt : "";
  $("engineBox").innerHTML =
    `<div class="kv"><span class="k">uygulama</span><span class="v">${esc(s.app)} ${esc(s.version)}</span></div>` +
    `<div class="kv"><span class="k">instance</span><span class="v">${s.instances}</span></div>` +
    `<div class="kv"><span class="k">çalışma</span><span class="v">${esc(s.uptime)}</span></div>` +
    `<div class="kv"><span class="k">ülke</span><span class="v">${s.countries}</span></div>` +
    `<div class="kv"><span class="k">Tor yerel</span><span class="v ${s.tor_local ? "ok" : "dim"}">${esc(s.tor_local_txt)}</span></div>`;
  const on = s.global_on;
  $("swGlobal").classList.toggle("on", !!on);
  $("swGlobalLbl").textContent = on ? "AÇIK (tüm PC)" : "kapalı";
  renderChips();
}

// ---- motor konsolu -------------------------------------------------------------------------
$("btnConClear").onclick = () => { $("console").innerHTML = ""; };
async function pollConsole() {
  if (!$("conAuto").checked) return;
  const d = await api("/api/console?since=" + state.consoleSeq);
  if (d.lines && d.lines.length) {
    state.consoleSeq = d.seq;
    const con = $("console");
    con.innerHTML += d.lines
      .map((l) => {
        let cls = "";
        if (/OK|CALISIYOR|oto-yakala/.test(l)) cls = "okline";
        else if (/hata|basarisiz|HATA/.test(l)) cls = "errline";
        else if (/WARN|blocked|uyari|reddi/.test(l)) cls = "warnline";
        return `<div class="${cls}">${esc(l)}</div>`;
      })
      .join("");
    con.scrollTop = con.scrollHeight;
  }
  $("conSeq").textContent = "seq " + state.consoleSeq;
}

// ---- meşgul göstergesi ---------------------------------------------------------------------
function busy(t) { $("busy").textContent = t || "çalışıyor..."; $("busy").classList.add("on"); }
function busyOff() { $("busy").classList.remove("on"); }

// ---- döngü ------------------------------------------------------------------------------------
(async function init() {
  loadSlots();
  await loadCountries();
  await loadEnc();
  await pollInstances(true);
  await pollStatus();
  loadUserLog();
  setInterval(pollInstances, 2500);
  setInterval(pollMonitor, 2000);
  setInterval(pollConsole, 1000);
  setInterval(pollStatus, 10000);
  setInterval(() => { if ($("tab-procs").classList.contains("active")) refreshProcs(); }, 5000);
})();
)MVPGEN"},
        {"style.css", R"MVPGEN(/* GOD_SSS_TIER_VPN — IDA tarzı koyu tema (referans: MVP_GOD_SSS_TIER_SIMULATOR_UI) */
:root {
  --bg: #0d1117; --bg2: #161b22; --bg3: #1c2129;
  --fg: #c9d1d9; --dim: #8b949e; --line: #30363d;
  --acc: #58a6ff; --acc2: #1f6feb;
  --ok: #3fb950; --warn: #d29922; --err: #f85149; --mag: #bc8cff;
  --code: #e6edf3;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
html, body { height: 100%; overflow: hidden; }
body {
  font: 13px/1.45 "Segoe UI", system-ui, sans-serif;
  background: var(--bg); color: var(--fg);
  display: flex; flex-direction: column;
}

/* ---------- üst bar ---------- */
#topbar {
  display: flex; align-items: center; gap: 14px;
  padding: 6px 12px; background: var(--bg2); border-bottom: 1px solid var(--line);
  flex-wrap: wrap;
}
.brand .logo { font-weight: 800; font-size: 15px; color: var(--mag); letter-spacing: .5px; }
.brand .logo .tier { color: var(--warn); }
.brand .logo .vpn { color: var(--acc); }
.brand .sub { color: var(--dim); font-size: 11px; margin-left: 10px; }
.createbar { display: flex; gap: 6px; flex: 1; min-width: 560px; align-items: center; }
.createbar select, .createbar input {
  background: var(--bg); border: 1px solid var(--line); color: var(--fg);
  padding: 5px 8px; font-family: Consolas, monospace; font-size: 12px; border-radius: 4px;
}
.createbar #selCountry { max-width: 220px; }
.createbar #inLabel { width: 90px; }
.chips { display: flex; gap: 6px; flex-wrap: wrap; }
.chip {
  background: var(--bg3); border: 1px solid var(--line); border-radius: 10px;
  padding: 1px 9px; font-size: 11px; color: var(--dim); font-family: Consolas, monospace;
}
.chip b { color: var(--fg); font-weight: 600; }
.chip.on b { color: var(--ok); }

button {
  background: var(--bg3); color: var(--fg); border: 1px solid var(--line);
  padding: 5px 12px; border-radius: 4px; cursor: pointer; font-size: 12px;
}
button:hover { border-color: var(--acc); color: #fff; }
button.primary { background: var(--acc2); border-color: var(--acc2); color: #fff; font-weight: 600; }
button.primary:hover { background: var(--acc); }
button.warn { background: #3d2e00; border-color: var(--warn); color: var(--warn); }
button.danger { background: #3d0d0d; border-color: var(--err); color: var(--err); }
button.big { width: 100%; padding: 9px; font-size: 13px; margin-top: 8px; }
button.mini { padding: 2px 8px; font-size: 11px; }

/* ---------- ana yerleşim ---------- */
#layout { flex: 1; display: flex; min-height: 0; }

/* sol: instanslar */
#instpanel {
  width: 330px; min-width: 240px; background: var(--bg2);
  border-right: 1px solid var(--line); display: flex; flex-direction: column;
}
.panel-title {
  padding: 6px 10px; font-size: 11px; color: var(--dim); text-transform: uppercase;
  letter-spacing: .5px; border-bottom: 1px solid var(--line); background: var(--bg3);
}
.panel-title .count { color: var(--acc); margin-left: 4px; }
.instlist { flex: 1; overflow-y: auto; }
.instcard {
  border-bottom: 1px solid #21262d; padding: 7px 9px; cursor: pointer; position: relative;
}
.instcard:hover { background: var(--bg3); }
.instcard.sel { background: #12233f; border-left: 2px solid var(--acc); }
.instcard .row1 { display: flex; align-items: baseline; gap: 8px; font-family: Consolas, monospace; }
.instcard .lbl { color: var(--warn); font-weight: 700; }
.instcard .ctry { color: var(--acc); }
.instcard .st { margin-left: auto; font-size: 10px; padding: 0 6px; border-radius: 8px; border: 1px solid; }
.instcard .st.RUNNING { color: var(--ok); border-color: var(--ok); }
.instcard .st.ERROR { color: var(--err); border-color: var(--err); }
.instcard .row2 { display: flex; gap: 10px; color: var(--dim); font-size: 11px; margin-top: 3px; font-family: Consolas, monospace; }
.instcard .ip { color: var(--ok); }
.instcard .row3 { display: flex; gap: 4px; margin-top: 5px; flex-wrap: wrap; }
.instcard .muted { color: var(--dim); font-size: 10px; margin-top: 4px; }

/* merkez */
#center { flex: 1; min-width: 0; display: flex; flex-direction: column; background: var(--bg); }
#tabs {
  display: flex; background: var(--bg2); border-bottom: 1px solid var(--line);
}
#tabs button {
  background: none; border: none; border-right: 1px solid var(--line); color: var(--dim);
  padding: 8px 16px; border-radius: 0; font-size: 12px;
}
#tabs button.active { color: var(--fg); background: var(--bg); border-top: 2px solid var(--acc); }
.tabpage { flex: 1; overflow: auto; padding: 12px; display: none; min-height: 0; }
.tabpage.active { display: block; }

.toolbar { display: flex; gap: 8px; align-items: center; margin-bottom: 8px; flex-wrap: wrap; }
.toolbar input[type=text] {
  flex: 1; min-width: 200px; background: var(--bg); border: 1px solid var(--line);
  color: var(--fg); padding: 6px 10px; border-radius: 4px; font-family: Consolas, monospace;
}
.toolbar .count { color: var(--dim); font-size: 11px; }

/* tablolar */
table.gt { border-collapse: collapse; width: 100%; font-size: 12px; }
table.gt th {
  text-align: left; color: var(--dim); font-size: 10px; text-transform: uppercase;
  letter-spacing: .5px; border-bottom: 1px solid var(--line); padding: 4px 8px;
  position: sticky; top: 0; background: var(--bg2);
}
table.gt td { padding: 4px 8px; border-bottom: 1px solid #21262d; font-family: Consolas, monospace; }
table.gt tr:hover td { background: var(--bg3); }
table.gt tr.sel td { background: #12233f; }
.mono { font-family: Consolas, monospace; }
.ok { color: var(--ok); } .warn { color: var(--warn); } .err { color: var(--err); }
.dim { color: var(--dim); } .acc { color: var(--acc); } .mag { color: var(--mag); }

/* instance detayı */
.dgrid { display: grid; grid-template-columns: repeat(auto-fill, minmax(210px, 1fr)); gap: 8px; }
.dbox { background: var(--bg2); border: 1px solid var(--line); border-radius: 6px; padding: 10px; }
.dbox .k { color: var(--dim); font-size: 10px; text-transform: uppercase; letter-spacing: .5px; }
.dbox .v { font-family: Consolas, monospace; font-size: 15px; margin-top: 4px; color: var(--code); }
.dbox .v.big { color: var(--ok); font-size: 18px; }
.dbox .s { color: var(--dim); font-size: 11px; margin-top: 2px; }

/* debug terminal akışları */
.evline {
  font-family: Consolas, monospace; font-size: 11.5px; padding: 2px 8px;
  border-bottom: 1px solid #161b22; white-space: pre-wrap;
}
.evline .t { color: var(--dim); }
.evline .pid { color: var(--acc); }
.evline .via { color: var(--mag); }
.evline .rmt { color: var(--code); }

/* sağ panel */
#sidepanel {
  width: 290px; min-width: 250px; background: var(--bg2);
  border-left: 1px solid var(--line); display: flex; flex-direction: column; overflow-y: auto;
}
.side-block { padding: 8px 10px; font-size: 12px; }
.side-block.small { font-size: 11px; color: var(--dim); }
.kv { display: flex; justify-content: space-between; padding: 2px 0; font-family: Consolas, monospace; }
.kv .k { color: var(--dim); }
.kv .v { color: var(--code); }
.switchrow { display: flex; align-items: center; gap: 8px; padding: 4px 0; }
.switch {
  width: 40px; height: 20px; border-radius: 12px; background: var(--bg3);
  border: 1px solid var(--line); position: relative; cursor: pointer; flex-shrink: 0;
}
.switch::after {
  content: ""; position: absolute; top: 1px; left: 1px; width: 16px; height: 16px;
  border-radius: 50%; background: var(--dim); transition: all .15s;
}
.switch.on { background: #123524; border-color: var(--ok); }
.switch.on::after { left: 21px; background: var(--ok); }

/* alt: debug konsolu */
#consolebar {
  height: 190px; display: flex; flex-direction: column; background: #0a0d12;
  border-top: 1px solid var(--line);
}
#consolebar.min { height: 34px; }
#consolebar.min #console { display: none; }
.console-title {
  display: flex; align-items: center; gap: 12px; padding: 4px 12px; color: var(--dim);
  font-size: 11px; background: var(--bg3); border-bottom: 1px solid var(--line);
}
.console-title span:first-child { color: var(--warn); text-transform: uppercase; letter-spacing: 1px; }
#console {
  flex: 1; overflow-y: auto; font-family: Consolas, monospace; font-size: 11.5px;
  padding: 4px 10px; white-space: pre-wrap; color: #9fb0c0;
}
#console .okline { color: var(--ok); }
#console .errline { color: var(--err); }
#console .warnline { color: var(--warn); }
#busy {
  position: fixed; inset: 0; background: rgba(13, 17, 23, .75); display: none;
  align-items: center; justify-content: center; color: var(--acc); z-index: 99;
  font-family: Consolas, monospace;
}
#busy.on { display: flex; }

@media (max-width: 1100px) { #sidepanel { display: none; } }
@media (max-width: 780px) { #instpanel { display: none; } }

/* ================= MVP_GOD_SSS_TIER — KARE KUTU INSTANCE'LAR ================= */
#instpanel { width: 372px; }
.instgrid {
  flex: 1; overflow-y: auto; padding: 10px;
  display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px;
  align-content: start;
}
.tile {
  position: relative; display: flex; flex-direction: column;
  background: var(--bg2); border: 1px solid var(--line); border-radius: 8px;
  padding: 8px; min-height: 168px; cursor: pointer;
  box-shadow: 0 1px 0 rgba(0,0,0,.3);
}
.tile:hover { border-color: var(--acc); }
.tile.sel { border-color: var(--acc); background: #12233f; box-shadow: 0 0 0 1px var(--acc); }
.tile.empty {
  border: 2px dashed #3d444d; background: transparent;
  align-items: center; justify-content: space-between; text-align: center;
}
.tile.empty .tile-big { font-size: 20px; font-weight: 800; letter-spacing: 2px; margin-top: 14px; }
.tile.empty .tile-note { font-size: 10px; padding: 0 6px; }
.btn-ayarlar { width: 92%; margin: 0 0 4px; padding: 7px 6px; font-size: 12px; font-weight: 700; }
.tile-top { display: flex; align-items: baseline; gap: 6px; font-family: Consolas, monospace; }
.tile-lbl { color: var(--warn); font-weight: 800; font-size: 13px; }
.tile .ctry { color: var(--acc); font-size: 11px; }
.tile .st { margin-left: auto; font-size: 9.5px; padding: 0 6px; border-radius: 8px; border: 1px solid; }
.tile .st.RUNNING { color: var(--ok); border-color: var(--ok); }
.tile .st.OVPN { color: var(--mag); border-color: var(--mag); }
.tile .st.ERROR { color: var(--err); border-color: var(--err); }
.tile .st.INIT { color: var(--warn); border-color: var(--warn); }
.tile-mid { display: flex; flex-direction: column; margin: 6px 0 2px; }
.tile-big.ip { color: var(--ok); font-family: Consolas, monospace; font-size: 15px; font-weight: 700; }
.tile-prov { color: var(--dim); font-size: 10px; letter-spacing: .5px; margin-top: 2px; }
.tile-note { font-size: 10px; font-family: Consolas, monospace; }
.tile-cap {
  margin-top: 4px; font-size: 10px; color: var(--mag); font-family: Consolas, monospace;
  background: #1d1622; border: 1px solid #3d2f4f; border-radius: 4px; padding: 2px 6px;
  overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
}
.tile-btns { display: flex; gap: 4px; margin-top: auto; flex-wrap: wrap; }

/* ---- AYARLAR MODALI ---- */
.modalwrap {
  position: fixed; inset: 0; background: rgba(4, 8, 14, .78); display: none;
  align-items: center; justify-content: center; z-index: 200;
}
.modalwrap.on { display: flex; }
.modalbox {
  width: 560px; max-width: 94vw; max-height: 88vh; overflow-y: auto;
  background: var(--bg2); border: 1px solid var(--acc2); border-radius: 10px;
  padding: 14px 16px; box-shadow: 0 12px 48px rgba(0,0,0,.6);
}
.modal-title {
  font-size: 14px; font-weight: 800; color: var(--mag); letter-spacing: .5px;
  padding-bottom: 8px; border-bottom: 1px solid var(--line); margin-bottom: 10px;
}
.mrow { margin-bottom: 10px; }
.mk { display: block; font-size: 10.5px; color: var(--dim); text-transform: uppercase;
      letter-spacing: .5px; margin-bottom: 3px; }
.mv {
  width: 100%; background: var(--bg); border: 1px solid var(--line); color: var(--fg);
  padding: 7px 9px; border-radius: 5px; font-family: Consolas, monospace; font-size: 12px;
}
.mv:focus { border-color: var(--acc); outline: none; }
.mnote { font-size: 10.5px; color: var(--dim); margin-top: 3px; }
.modal-btns { display: flex; gap: 8px; margin-top: 12px; }
.modalerr { color: var(--err); font-size: 12px; margin-top: 8px; font-family: Consolas, monospace; }

/* OTO-YAKALA terminal satırı */
.autol {
  font-family: Consolas, monospace; font-size: 11.5px; padding: 2px 8px;
  border-bottom: 1px solid #161b22; white-space: pre-wrap; background: #161226;
}

@media (max-width: 780px) { #instpanel { display: none; } }
)MVPGEN"},
    };
    return m;
}

inline std::string get(const std::string& rel) {
    auto it = files().find(rel);
    return it == files().end() ? std::string() : it->second;
}

} // namespace embedded
} // namespace ui

