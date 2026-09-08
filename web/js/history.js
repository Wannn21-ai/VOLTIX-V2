import {
  DEVICE_ROOT,
  FIREBASE_CONFIG,
  db,
  onValue,
  ref,
  remove
} from "./firebase-config.js";

import {
  formatDuration,
  formatRupiah,
  renderShell,
  setGlobalStatus,
  showToast
} from "./shell.js";

renderShell("history", "HISTORI");

const $ = id => document.getElementById(id);

let sessions = [];
let live = {};
let selectedSessionId = "";

function escapeHtml(value) {
  return String(value ?? "").replace(
    /[&<>'"]/g,
    character => ({
      "&": "&amp;",
      "<": "&lt;",
      ">": "&gt;",
      "'": "&#39;",
      '"': "&quot;"
    })[character]
  );
}

function number(value, digits = 1) {
  return (Number(value) || 0).toFixed(digits);
}

function stamp(value) {
  const date = new Date(Number(value) || 0);

  if (Number.isNaN(date.getTime()) || date.getTime() === 0) {
    return "—";
  }

  return date.toLocaleString("id-ID");
}

function online() {
  const heartbeat = Number(live.heartbeat || 0);

  return heartbeat > 0 &&
    Date.now() - heartbeat < 12000;
}

function getEnergyWh(item) {
  const energyWh = Math.max(0, Number(item.energyWh) || 0);
  const energyFromKwh =
    Math.max(0, Number(item.energyKwh) || 0) * 1000;

  return Math.max(energyWh, energyFromKwh);
}

function reasonText(reason) {
  const labels = {
    USER_STOP: "Dihentikan pengguna",
    LOAD_REMOVED: "Beban dilepas",
    LOAD_REMOVED_AFTER_POWER_LOSS: "Beban dilepas setelah listrik padam",
    OVERLOAD: "Proteksi overload",
    NO_LOAD_DETECTED: "Beban tidak terdeteksi",
    TIMER_COMPLETED: "Waktu monitoring selesai",
    SYSTEM_RESET: "Reset sistem"
  };

  return labels[String(reason || "").toUpperCase()] ||
    reason ||
    "Selesai";
}

function setDetailText(id, value) {
  const element = $(id);

  if (element) {
    element.textContent = value;
  }
}

function updateDetail(item) {
  setDetailText("detail-load", item.loadName || "Tanpa nama");
  setDetailText("detail-time", stamp(item.endedAt));

  const startMode = String(item.mode || "—").toUpperCase();
  const endMode = String(item.endMode || startMode).toUpperCase();

  setDetailText(
    "detail-mode",
    startMode === endMode
      ? startMode
      : `${startMode} → ${endMode}`
  );

  setDetailText(
    "detail-duration",
    formatDuration(item.durationSec)
  );

  const timed =
  item.timed === true ||
  Number(item.requestedDurationSec || 0) > 0;

setDetailText(
  "detail-timed",
  timed ? "Bertimer" : "Tanpa batas"
);

setDetailText(
  "detail-requested-duration",
  timed
    ? formatDuration(
        item.requestedDurationSec
      )
    : "—"
);

  setDetailText(
    "detail-energy",
    `${number(getEnergyWh(item), 4)} Wh`
  );

  setDetailText(
    "detail-cost",
    formatRupiah(item.cost)
  );

  setDetailText(
    "detail-average",
    `${number(item.averagePowerW, 1)} W`
  );

  setDetailText(
    "detail-peak",
    `${number(item.peakPowerW, 1)} W`
  );

  setDetailText(
    "detail-voltage",
    `${number(item.voltage, 1)} V`
  );

  setDetailText(
    "detail-current",
    `${number(item.current, 3)} A`
  );

  setDetailText(
    "detail-frequency",
    `${number(item.frequency, 1)} Hz`
  );

  setDetailText(
    "detail-pf",
    number(item.powerFactor, 2)
  );

  setDetailText(
    "detail-recovered",
    item.recovered === true ? "Ya" : "Tidak"
  );

  setDetailText(
    "detail-reason",
    reasonText(item.endReason)
  );
}

function openDetail(id) {
  const item = sessions.find(session => session.id === id);

  if (!item) {
    return;
  }

  selectedSessionId = id;
  updateDetail(item);
  $("history-detail-modal").classList.add("open");
}

function closeDetail() {
  selectedSessionId = "";
  $("history-detail-modal").classList.remove("open");
}

function render() {
  const query = $("search").value.trim().toLowerCase();

  const filtered = sessions.filter(item =>
    String(item.loadName || "")
      .toLowerCase()
      .includes(query)
  );

  const totalEnergyKwh =
    sessions.reduce(
      (total, item) => total + getEnergyWh(item),
      0
    ) / 1000;

  const totalCost = sessions.reduce(
    (total, item) => total + (Number(item.cost) || 0),
    0
  );

  $("sum-count").textContent = String(sessions.length);
  $("sum-energy").textContent = totalEnergyKwh.toFixed(4);
  $("sum-cost").textContent = formatRupiah(totalCost);

  $("sum-overload").textContent = String(
    sessions.filter(
      item =>
        String(item.endReason || "").toUpperCase() ===
        "OVERLOAD"
    ).length
  );

  $("history-status").textContent =
    query.length > 0
      ? `${filtered.length} hasil`
      : `${sessions.length} histori`;

  $("history-body").innerHTML = filtered.length
    ? filtered.map(item => {
        const overload =
          String(item.endReason || "").toUpperCase() ===
          "OVERLOAD";

        return `
          <tr
            class="history-row"
            data-history-id="${escapeHtml(item.id)}"
            tabindex="0"
            title="Klik untuk melihat detail"
          >
            <td>${stamp(item.endedAt)}</td>

            <td>
              <strong>
                ${escapeHtml(item.loadName || "Tanpa nama")}
              </strong>
            </td>

            <td>${escapeHtml(item.mode || "—")}</td>

            <td>${formatDuration(item.durationSec)}</td>

            <td>${number(getEnergyWh(item), 4)} Wh</td>

            <td>${formatRupiah(item.cost)}</td>

            <td>${number(item.peakPowerW, 1)} W</td>

            <td>
              <span class="status-chip ${
                overload ? "overload" : "normal"
              }">
                ${escapeHtml(reasonText(item.endReason))}
              </span>
            </td>

            <td>
              <button
                class="history-delete-button"
                type="button"
                data-delete-id="${escapeHtml(item.id)}"
                aria-label="Hapus histori ${escapeHtml(
                  item.loadName || "Tanpa nama"
                )}"
              >
                Hapus
              </button>
            </td>
          </tr>
        `;
      }).join("")
    : `
      <tr>
        <td colspan="9" class="empty-row">
          Tidak ada histori yang sesuai.
        </td>
      </tr>
    `;

  if (selectedSessionId) {
    const selected = sessions.find(
      item => item.id === selectedSessionId
    );

    if (selected) {
      updateDetail(selected);
    } else {
      closeDetail();
    }
  }
}

async function loadHistory() {
  try {
    const url =
      `${FIREBASE_CONFIG.databaseURL}/` +
      `${DEVICE_ROOT}/history.json?t=${Date.now()}`;

    const response = await fetch(url, {
      cache: "no-store"
    });

    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }

    const data = await response.json();

    sessions = Object.entries(data || {}).map(
      ([id, item]) => ({
        id,
        ...(item || {})
      })
    );

    sessions.sort(
      (a, b) =>
        Number(b.endedAt || 0) -
        Number(a.endedAt || 0)
    );

    render();
  } catch (error) {
    console.error("[history] gagal membaca histori:", error);

    $("history-status").textContent =
      "Gagal membaca histori";
  }
}

async function deleteOneHistory(id) {
  const item = sessions.find(session => session.id === id);

  if (!item) {
    return;
  }

  const confirmed = window.confirm(
    `Hapus histori "${item.loadName || "Tanpa nama"}"?`
  );

  if (!confirmed) {
    return;
  }

  try {
    await remove(
      ref(db, `${DEVICE_ROOT}/history/${id}`)
    );

    sessions = sessions.filter(
      session => session.id !== id
    );

    if (selectedSessionId === id) {
      closeDetail();
    }

    render();
    showToast("Histori berhasil dihapus.", "success");
  } catch (error) {
    console.error("[history] delete gagal:", error);
    showToast("Histori gagal dihapus.", "error");
  }
}

async function deleteAllHistory() {
  if (!sessions.length) {
    showToast("Belum ada histori untuk dihapus.", "error");
    return;
  }

  const confirmed = window.confirm(
    `Hapus semua ${sessions.length} histori dari Firebase?`
  );

  if (!confirmed) {
    return;
  }

  try {
    await remove(
      ref(db, `${DEVICE_ROOT}/history`)
    );

    sessions = [];
    closeDetail();
    render();

    showToast(
      "Semua histori berhasil dihapus.",
      "success"
    );
  } catch (error) {
    console.error("[history] delete all gagal:", error);
    showToast("Semua histori gagal dihapus.", "error");
  }
}

$("history-body").addEventListener("click", event => {
  const deleteButton =
    event.target.closest("[data-delete-id]");

  if (deleteButton) {
    event.stopPropagation();
    deleteOneHistory(deleteButton.dataset.deleteId);
    return;
  }

  const row = event.target.closest("[data-history-id]");

  if (row) {
    openDetail(row.dataset.historyId);
  }
});

$("history-body").addEventListener("keydown", event => {
  if (event.key !== "Enter" && event.key !== " ") {
    return;
  }

  if (event.target.closest("[data-delete-id]")) {
    return;
  }

  const row = event.target.closest("[data-history-id]");

  if (row) {
    event.preventDefault();
    openDetail(row.dataset.historyId);
  }
});

$("history-detail-close").addEventListener(
  "click",
  closeDetail
);

$("history-detail-modal").addEventListener(
  "click",
  event => {
    if (event.target === $("history-detail-modal")) {
      closeDetail();
    }
  }
);

document.addEventListener("keydown", event => {
  if (
    event.key === "Escape" &&
    $("history-detail-modal").classList.contains("open")
  ) {
    closeDetail();
  }
});

$("btn-delete-all").addEventListener(
  "click",
  deleteAllHistory
);

$("search").addEventListener("input", render);

$("btn-export").addEventListener("click", () => {
  if (!sessions.length) {
    showToast(
      "Belum ada histori untuk diekspor.",
      "error"
    );
    return;
  }

  const columns = [
    "endedAt",
    "loadName",
    "mode",
    "durationSec",
    "energyWh",
    "energyKwh",
    "cost",
    "averagePowerW",
    "peakPowerW",
    "timed",
"requestedDurationSec",
    "endReason"
  ];

  const quote = value =>
    `"${String(value ?? "").replaceAll('"', '""')}"`;

  const csv = [
    columns.join(","),
    ...sessions.map(item =>
      columns.map(key => quote(item[key])).join(",")
    )
  ].join("\n");

  const url = URL.createObjectURL(
    new Blob([csv], {
      type: "text/csv;charset=utf-8"
    })
  );

  const link = document.createElement("a");
  link.href = url;
  link.download =
    `voltix-history-${new Date()
      .toISOString()
      .slice(0, 10)}.csv`;

  link.click();
  URL.revokeObjectURL(url);
});

onValue(
  ref(db, `${DEVICE_ROOT}/live`),
  snapshot => {
    live = snapshot.val() || {};
    setGlobalStatus(online());
  }
);

loadHistory();

window.setInterval(loadHistory, 5000);
window.setInterval(() => setGlobalStatus(online()), 1000);