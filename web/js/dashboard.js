import { DEVICE_ROOT, db, onValue, ref, serverTimestamp, set } from "./firebase-config.js";
import { formatDuration, formatRupiah, renderShell, setGlobalStatus, showToast } from "./shell.js";

renderShell("dashboard", "DASHBOARD");

const $ = id => document.getElementById(id);
const liveRef = ref(db, `${DEVICE_ROOT}/live`);
const commandRef = ref(db, `${DEVICE_ROOT}/command`);
const ackRef = ref(db, `${DEVICE_ROOT}/ack`);
const configRef = ref(db, `${DEVICE_ROOT}/config`);
const historyRef = ref(db, `${DEVICE_ROOT}/history`);
const ONLINE_WINDOW_MS = 12000;

let live = {};
let config = { tariffPerKwh: 1444.7, overloadThresholdW: 2000 };
let lastAckId = "";
let pendingCommandId = "";
let liveReceivedAtMs = Date.now();
let durationBaseSec = 0;
let validationBaseMs = 5000;
let chart;
let usagePieChart;
let historyItems = [];

function text(id, value) { const node = $(id); if (node) node.textContent = value; }
function number(value, digits = 1) { return (Number(value) || 0).toFixed(digits); }
function online() { return Number(live.heartbeat || 0) > 0 && Date.now() - Number(live.heartbeat) < ONLINE_WINDOW_MS; }
function renderSessionClock(
  sessionState,
  connected
) {
  const elapsedSinceLive = Math.max(
    0,
    Date.now() - liveReceivedAtMs
  );

  if (sessionState === "VALIDATING_LOAD") {
    const remainingMs = Math.max(
      0,
      validationBaseMs - elapsedSinceLive
    );

    const remainingSec = Math.ceil(
      remainingMs / 1000
    );

    text("duration-label", "Validasi beban");
    text(
      "val-duration",
      formatDuration(remainingSec)
    );
    text(
      "duration-note",
      "Hitung mundur sebelum monitoring"
    );

    return;
  }

  if (sessionState === "MONITORING") {
    const additionalSec = connected
      ? Math.floor(elapsedSinceLive / 1000)
      : 0;

    const displayedDuration =
      durationBaseSec + additionalSec;

    text(
      "duration-label",
      "Durasi monitoring"
    );

    text(
      "val-duration",
      formatDuration(displayedDuration)
    );

    text(
      "duration-note",
      connected
        ? "Monitoring sedang berjalan"
        : "Durasi terakhir dari perangkat"
    );

    return;
  }

  text("duration-label", "Durasi monitoring");
  text(
    "val-duration",
    formatDuration(durationBaseSec)
  );
  text(
    "duration-note",
    sessionState === "FINISHED"
      ? "Monitoring selesai"
      : "Dimulai setelah validasi beban"
  );
}
function stateClass(state) {
  if (state === "MONITORING") return "online";
  if (state === "OVERLOAD" || state === "ERROR") return "fault";
  if (state === "IDLE" || state === "FINISHED") return "idle";
  return "transition";
}
function setGauge(id, value, max) {
  const path = $(id);
  if (!path) return;
  const ratio = Math.min(1, Math.max(0, Number(value) / max));
  path.style.strokeDashoffset = String(232 - 232 * ratio);
}

function updateChart(power) {
  if (!window.Chart) return;
  if (!chart) {
    chart = new window.Chart($("power-chart"), {
      type: "line",
      data: { labels: [], datasets: [{ data: [], borderColor: "#00e5ff", backgroundColor: "rgba(0,229,255,.12)", fill: true, tension: .35, pointRadius: 0, borderWidth: 2 }] },
      options: { responsive: true, maintainAspectRatio: false, animation: false, plugins: { legend: { display: false } }, scales: { x: { ticks: { color: "#666", maxTicksLimit: 6 }, grid: { color: "rgba(255,255,255,.04)" } }, y: { beginAtZero: true, ticks: { color: "#666" }, grid: { color: "rgba(255,255,255,.04)" } } } }
    });
  }
  const label = new Date().toLocaleTimeString("id-ID", { hour: "2-digit", minute: "2-digit", second: "2-digit" });
  chart.data.labels.push(label);
  chart.data.datasets[0].data.push(Number(power) || 0);
  if (chart.data.labels.length > 60) { chart.data.labels.shift(); chart.data.datasets[0].data.shift(); }
  chart.update("none");
}

function render() {
  const connected = online();
  const sensor = live.sensor || {};
  const session = live.session || {};
  const sessionState = String(live.sessionState || session.state || "IDLE").toUpperCase();
  const mode = String(live.operatingMode || "—").toUpperCase();
  const relayOn = live.relayOn === true;
  const alarm = live.alarmLatched === true;
  const active = ["VALIDATING_LOAD", "MONITORING", "FINISHING"].includes(sessionState);

  setGlobalStatus(connected, connected ? "Alat online" : "Alat offline");
  text("hero-mode", mode);
  text("hero-session", sessionState);
  text("hero-relay", relayOn ? "ON" : "OFF");
  text("hero-network", connected ? "ONLINE" : "OFFLINE");
  text("val-power", number(sensor.power, 1));
  text("val-voltage", number(sensor.voltage, 1));
  text("val-current", number(sensor.current, 3));
  text("val-pf", number(sensor.powerFactor, 2));
  text("val-energy", number(session.energyKwh, 4));
  text("val-cost", formatRupiah(session.cost));
  renderSessionClock(sessionState, connected);
  text("val-peak", number(session.peakPowerW, 1));
  text("val-load-name", session.loadName || "—");
  text("val-load-detected", sensor.loadDetected ? "Terdeteksi" : "Tidak terdeteksi");
  text("val-alarm", alarm ? "OVERLOAD" : "Normal");
  text("val-pending-sync", `${Number(live.pendingHistory || 0)} pending`);
  text("val-heartbeat", live.heartbeat ? new Date(Number(live.heartbeat)).toLocaleTimeString("id-ID") : "—");
  text("session-id", session.loadName || "");
  setGauge("gauge-voltage", sensor.voltage, 250);
  setGauge("gauge-current", sensor.current, 16);

  const badge = $("device-badge");
  badge.className = `badge ${stateClass(sessionState)}`;
  badge.textContent = connected ? sessionState : "OFFLINE";
  $("overload-banner").classList.toggle("show", alarm || sessionState === "OVERLOAD");
  $("offline-banner").classList.toggle("show", mode === "OFFLINE" || !connected);
  $("btn-start").disabled = !connected || active || alarm;
  $("btn-stop").disabled = !connected || !active;
  const note = $("command-note");
  if (!connected) { note.textContent = ""; note.dataset.tone = ""; }
  else if (alarm) { note.textContent = "Alarm harus dikonfirmasi dari tombol OK pada alat."; note.dataset.tone = "danger"; }
  else if (sessionState === "MONITORING") { note.textContent = `Monitoring ${session.loadName || "beban"} sedang berjalan.`; note.dataset.tone = "success"; }
  else if (sessionState === "VALIDATING_LOAD") { note.textContent = "Relay ON, sedang memvalidasi beban…"; note.dataset.tone = "warning"; }
  else { note.textContent = "Alat siap memulai sesi baru."; note.dataset.tone = ""; }
  updateChart(sensor.power);

  function historyEnergyWh(item) {
  const energyWh = Math.max(
    0,
    Number(item.energyWh) || 0
  );

  const energyFromKwh =
    Math.max(0, Number(item.energyKwh) || 0) * 1000;

  return Math.max(energyWh, energyFromKwh);
}

function renderUsagePie() {
  const canvas = $("usage-pie-chart");
  const empty = $("usage-pie-empty");

  if (!canvas || !window.Chart) {
    return;
  }

  const grouped = new Map();

  historyItems.forEach(item => {
    const loadName =
      String(item.loadName || "Tanpa nama").trim() ||
      "Tanpa nama";

    const key = loadName.toLowerCase();

    const saved = grouped.get(key) || {
      label: loadName,
      energyWh: 0
    };

    saved.energyWh += historyEnergyWh(item);
    grouped.set(key, saved);
  });

  const entries = [...grouped.values()]
    .filter(item => item.energyWh > 0)
    .sort((a, b) => b.energyWh - a.energyWh);

  if (!entries.length) {
    canvas.hidden = true;
    empty.hidden = false;

    if (usagePieChart) {
      usagePieChart.destroy();
      usagePieChart = null;
    }

    return;
  }

  canvas.hidden = false;
  empty.hidden = true;

  const labels = entries.map(item => item.label);

  const values = entries.map(item =>
    Number(item.energyWh.toFixed(4))
  );

  const colors = [
    "#00e5ff",
    "#00e676",
    "#ffab00",
    "#a66cff",
    "#ff1744",
    "#29b6f6",
    "#ff6d00",
    "#26a69a"
  ];

  if (usagePieChart) {
    usagePieChart.data.labels = labels;
    usagePieChart.data.datasets[0].data = values;

    usagePieChart.data.datasets[0].backgroundColor =
      labels.map(
        (_, index) => colors[index % colors.length]
      );

    usagePieChart.update();
    return;
  }

  usagePieChart = new window.Chart(canvas, {
    type: "pie",

    data: {
      labels,

      datasets: [{
        data: values,

        backgroundColor: labels.map(
          (_, index) => colors[index % colors.length]
        ),

        borderColor: "#071015",
        borderWidth: 2
      }]
    },

    options: {
      responsive: true,
      maintainAspectRatio: false,

      plugins: {
        legend: {
          position: "bottom",

          labels: {
            color: "#aab4bf",
            padding: 14,
            usePointStyle: true
          }
        },

        tooltip: {
          callbacks: {
            label(context) {
              return `${context.label}: ${Number(
                context.raw
              ).toFixed(4)} Wh`;
            }
          }
        }
      }
    }
  });
}
}

async function sendCommand(type, payload = {}) {
  if (!online()) { showToast("Alat sedang offline.", "error"); return; }
  const id = `cmd-${Date.now()}-${Math.random().toString(16).slice(2, 8)}`;
  pendingCommandId = id;
  await set(commandRef, { id, type, ...payload, createdAt: serverTimestamp() });
  showToast(type === "START" ? "Perintah START dikirim." : "Perintah STOP dikirim.");
}

onValue(liveRef, snapshot => {
  live = snapshot.val() || {};

  const session = live.session || {};

  liveReceivedAtMs = Date.now();

  durationBaseSec = Number(
    session.durationSec || 0
  );

  validationBaseMs = Number(
    session.validationRemainingMs ?? 5000
  );

  render();
});
onValue(configRef, snapshot => { config = { ...config, ...(snapshot.val() || {}) }; });
onValue(historyRef, snapshot => {
  const historyData = snapshot.val() || {};

  historyItems = Object.values(historyData);

  text(
    "history-count",
    `${historyItems.length} sesi tersimpan`
  );

  renderUsagePie();
});
onValue(ackRef, snapshot => {
  const ack = snapshot.val() || {};
  if (!ack.commandId || ack.commandId === lastAckId) return;
  lastAckId = ack.commandId;
  if (pendingCommandId && ack.commandId !== pendingCommandId) return;
  pendingCommandId = "";
  const ok = ["ACCEPTED", "COMPLETED", "MONITORING"].includes(String(ack.status || "").toUpperCase());
  showToast(ack.message || `Perintah ${ack.status || "diproses"}`, ok ? "success" : "error");
});

$("btn-start").addEventListener("click", () => { $("start-modal").classList.add("open"); $("load-name").focus(); });
$("btn-cancel").addEventListener("click", () => $("start-modal").classList.remove("open"));
$("start-modal").addEventListener("click", event => { if (event.target === $("start-modal")) $("start-modal").classList.remove("open"); });
$("start-form").addEventListener("submit", async event => {
  event.preventDefault();
  const loadName = $("load-name").value.trim();
  if (!loadName) return;
  $("start-modal").classList.remove("open");
  await sendCommand("START", { loadName });
});
$("btn-stop").addEventListener("click", () => sendCommand("STOP"));

window.setInterval(render, 1000);
render();
