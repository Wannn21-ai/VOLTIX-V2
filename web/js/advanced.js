import {
  DEVICE_ROOT,
  db,
  onValue,
  ref
} from "./firebase-config.js";

import {
  renderShell,
  setGlobalStatus
} from "./shell.js";

renderShell("advanced", "ADVANCED");

const $ = id => document.getElementById(id);

function number(value, digits) {
  return (Number(value) || 0).toFixed(digits);
}

onValue(
  ref(db, `${DEVICE_ROOT}/live`),

  snapshot => {
    const live = snapshot.val() || {};
    const sensor = live.sensor || {};
    const session = live.session || {};

    const heartbeat = Number(live.heartbeat || 0);

    const isOnline =
      heartbeat > 0 &&
      Date.now() - heartbeat < 12000;

    setGlobalStatus(isOnline);

    $("adv-voltage").textContent =
      number(sensor.voltage, 1);

    $("adv-current").textContent =
      number(sensor.current, 3);

    $("adv-power").textContent =
      number(sensor.power, 1);

    $("adv-energy").textContent =
      number(sensor.energy, 4);

    $("adv-frequency").textContent =
      number(sensor.frequency, 1);

    $("adv-pf").textContent =
      number(sensor.powerFactor, 2);

    $("adv-mode").textContent =
      live.operatingMode || "—";

    $("adv-network").textContent =
      live.networkState ||
      (isOnline ? "CONNECTED" : "DISCONNECTED");

    $("adv-session").textContent =
      live.sessionState ||
      session.state ||
      "IDLE";

    $("adv-relay").textContent =
      live.relayOn ? "ON" : "OFF";

    $("adv-valid").textContent =
      sensor.valid ? "YA" : "TIDAK";

    $("adv-pending").textContent =
      String(live.pendingHistory || 0);
  }
);