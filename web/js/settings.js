import {
  DEVICE_ROOT,
  db,
  onValue,
  ref,
  serverTimestamp,
  update
} from "./firebase-config.js";

import {
  renderShell,
  setGlobalStatus,
  showToast
} from "./shell.js";

renderShell("settings", "PENGATURAN");

const $ = id => document.getElementById(id);

const configRef = ref(db, `${DEVICE_ROOT}/config`);

let revision = 0;
let sessionState = "IDLE";

const tariffPresets = {
  "900": 1352.00,
  "1300": 1444.70,
  "2200": 1444.70,
  "3500-5500": 1699.53,
  "6600-plus": 1699.53
};

const overloadPresets = {
  "mcb-4": 900,
  "mcb-6": 1300,
  "mcb-10": 2200,
  "mcb-16": 3500,
  "mcb-20": 4400,
  "mcb-30-32": 6600
};

function hasPreset(presets, key) {
  return Object.prototype.hasOwnProperty.call(presets, key);
}

function makeEditable(input) {
  input.disabled = false;
  input.readOnly = false;
  input.removeAttribute("disabled");
  input.removeAttribute("readonly");
}

function makeReadonly(input) {
  input.disabled = false;
  input.readOnly = true;
  input.removeAttribute("disabled");
  input.setAttribute("readonly", "");
}

function findPreset(presets, savedPreset, value) {
  if (savedPreset === "custom") {
    return "custom";
  }

  if (
    hasPreset(presets, savedPreset) &&
    Math.abs(presets[savedPreset] - value) < 0.01
  ) {
    return savedPreset;
  }

  return (
    Object.keys(presets).find(
      key => Math.abs(presets[key] - value) < 0.01
    ) || "custom"
  );
}

function updateTariffPreset(clearCustom = false) {
  const preset = $("tariff-preset").value;
  const input = $("tariff");
  const info = $("tariff-info");

  if (preset === "custom") {
    makeEditable(input);

    if (clearCustom) {
      input.value = "";
    }

    input.placeholder = "Masukkan tarif custom";

    if (info) {
      info.textContent =
        "Masukkan nilai tarif listrik dalam Rp/kWh.";
    }

    if (clearCustom) {
      input.focus();
    }

    return;
  }

  makeReadonly(input);

  input.value = tariffPresets[preset];
  input.placeholder = "";

  if (info) {
    const formatted = new Intl.NumberFormat("id-ID", {
      style: "currency",
      currency: "IDR",
      minimumFractionDigits: 2,
      maximumFractionDigits: 2
    }).format(tariffPresets[preset]);

    info.textContent = `${formatted} per kWh`;
  }
}

function updateOverloadPreset(clearCustom = false) {
  const preset = $("overload-preset").value;
  const input = $("overload");
  const info = $("overload-info");

  if (preset === "custom") {
    makeEditable(input);

    if (clearCustom) {
      input.value = "";
    }

    input.placeholder = "Masukkan batas custom";

    if (info) {
      info.textContent =
        "Masukkan batas overload dalam Watt.";
    }

    if (clearCustom) {
      input.focus();
    }

    return;
  }

  makeReadonly(input);

  input.value = overloadPresets[preset];
  input.placeholder = "";

  if (info) {
    const formatted = Number(
      overloadPresets[preset]
    ).toLocaleString("id-ID");

    info.textContent =
      `Batas overload sekitar ${formatted} W`;
  }
}

$("tariff-preset").addEventListener("change", () => {
  updateTariffPreset(true);
});

$("overload-preset").addEventListener("change", () => {
  updateOverloadPreset(true);
});

onValue(
  configRef,
  snapshot => {
    const config = snapshot.val() || {};

    revision = Number(config.revision || 0);

    const tariffValue = Number(
      config.tariffPerKwh ?? 1444.70
    );

    const overloadValue = Number(
      config.overloadThresholdW ?? 2000
    );

    const savedTariffPreset = String(
      config.tariffPlan || ""
    );

    const savedOverloadPreset = String(
      config.overloadPreset || ""
    );

    $("tariff-preset").value = findPreset(
      tariffPresets,
      savedTariffPreset,
      tariffValue
    );

    $("tariff").value = tariffValue;
    updateTariffPreset(false);

    $("overload-preset").value = findPreset(
      overloadPresets,
      savedOverloadPreset,
      overloadValue
    );

    $("overload").value = overloadValue;
    updateOverloadPreset(false);

    $("power-threshold").value =
      config.loadPowerThresholdW ?? 1;

    $("current-threshold").value =
      config.loadCurrentThresholdA ?? 0.02;

    $("removed-delay").value =
      config.loadRemovedDelaySec ?? 3;

    $("config-status").textContent = "Pengaturan aktif";
  },
  error => {
    console.error("[settings] config read failed:", error);

    $("config-status").textContent =
      "Gagal membaca konfigurasi.";
  }
);

onValue(
  ref(db, `${DEVICE_ROOT}/live`),
  snapshot => {
    const live = snapshot.val() || {};

    sessionState = String(
      live.sessionState || "IDLE"
    ).toUpperCase();

    const heartbeat = Number(live.heartbeat || 0);

    const online =
      heartbeat > 0 &&
      Date.now() - heartbeat < 12000;

    setGlobalStatus(online);
  }
);

$("settings-form").addEventListener(
  "submit",
  async event => {
    event.preventDefault();

    if (!["IDLE", "FINISHED"].includes(sessionState)) {
      showToast(
        "Hentikan session sebelum mengubah konfigurasi.",
        "error"
      );

      return;
    }

    const tariff = Number($("tariff").value);
    const overload = Number($("overload").value);
    const powerThreshold = Number(
      $("power-threshold").value
    );
    const currentThreshold = Number(
      $("current-threshold").value
    );
    const removedDelay = Number(
      $("removed-delay").value
    );

    if (!Number.isFinite(tariff) || tariff < 0) {
      showToast(
        "Nilai tarif tidak valid.",
        "error"
      );

      return;
    }

    if (
      !Number.isFinite(overload) ||
      overload < 10 ||
      overload > 10000
    ) {
      showToast(
        "Batas overload harus antara 10–10.000 W.",
        "error"
      );

      return;
    }

    const payload = {
      tariffPerKwh: tariff,
      tariffPlan: $("tariff-preset").value,

      overloadThresholdW: overload,
      overloadPreset: $("overload-preset").value,

      loadPowerThresholdW: powerThreshold,
      loadCurrentThresholdA: currentThreshold,
      loadRemovedDelaySec: removedDelay,

      revision: Math.max(
        Date.now(),
        revision + 1
      ),

      updatedAt: serverTimestamp()
    };

    try {
      await update(configRef, payload);

      showToast(
        "Pengaturan tersimpan. ESP32 akan menerapkannya saat idle.",
        "success"
      );
    } catch (error) {
      console.error("[settings] save failed:", error);

      showToast(
        "Pengaturan gagal disimpan.",
        "error"
      );
    }
  }
);