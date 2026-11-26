// ============================================================================
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
