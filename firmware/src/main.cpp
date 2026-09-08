#include <Arduino.h>

#include "cloud.h"
#include "config.h"
#include "hardware.h"
#include "network.h"
#include "session.h"
#include "state.h"
#include "storage.h"

namespace {
uint32_t lastCommandPollMs = 0;
uint32_t lastConfigPollMs = 0;
uint32_t lastSyncMs = 0;
uint8_t cloudTurn = 0;
bool resetConfirmationActive = false;

void enterBootMenu() {
  hardwareSetRelay(false);
  menuScreen = MenuScreen::BOOT;
  menuIndex = 0;
}

void performReset() {
  hardwareSetRelay(false);
  storageClearAll();
  networkClearCredentials();
  Serial.println("[system] local data and Wi-Fi credentials cleared; restarting");
  delay(150);
  ESP.restart();
}

void handleBootSelection() {
  if (menuIndex == 0) {
    menuScreen = MenuScreen::NONE;
    networkStartOnline();
    return;
  }
  if (menuIndex == 1) {
    menuScreen = MenuScreen::NONE;
    networkEnterManualOffline();
    sessionStartOfflineNext();
    return;
  }
  menuScreen = MenuScreen::RESET_CONFIRM;
  menuIndex = 0;
  resetConfirmationActive = true;
}

void handleMenuButtons(bool navPressed, bool okPressed) {
  if (navPressed) {
    const uint8_t count = menuScreen == MenuScreen::BOOT ? 3 : 2;
    menuIndex = (menuIndex + 1) % count;
  }
  if (!okPressed) return;

  if (menuScreen == MenuScreen::BOOT) {
    handleBootSelection();
  } else if (menuScreen == MenuScreen::OFFLINE_NEXT) {
    if (menuIndex == 0) {
      offlineSessionLock = false;
      networkEnterManualOffline();
      menuScreen = MenuScreen::NONE;
      sessionStartOfflineNext();
    } else {
      sessionSetIdle();
      offlineSessionLock = false;
      menuScreen = MenuScreen::NONE;
      networkStartOnline();
    }
  } else if (menuScreen == MenuScreen::RESET_CONFIRM) {
    if (menuIndex == 1 && resetConfirmationActive) performReset();
    resetConfirmationActive = false;
    enterBootMenu();
  }
}

void handleCommand(const CloudCommand& command) {
  if (command.type == "START") {
    if (
        command.durationSec > 0 &&
        (
            command.durationSec <
                Config::MIN_TIMED_DURATION_SEC ||
            command.durationSec >
                Config::MAX_TIMED_DURATION_SEC
        )
    ) {
        cloudAcknowledge(
            command,
            "REJECTED",
            "Durasi harus 1 menit sampai 24 jam."
        );

        return;
    }

    if (
        sessionStart(
            command.loadName,
            command.durationSec
        )
    ) {
        cloudAcknowledge(
            command,
            "ACCEPTED",
            command.durationSec > 0
                ? "START diterima; timer dimulai "
                  "setelah validasi beban."
                : "START diterima; validasi "
                  "beban dimulai."
        );
    } else {
        cloudAcknowledge(
            command,
            "REJECTED",
            "Alat belum siap memulai sesi baru."
        );
    }

    return;
}
  if (command.type == "STOP") {
    if (sessionIsActive()) {
      sessionStop(EndReason::USER_STOP);
      cloudAcknowledge(command, "COMPLETED", "Sesi dihentikan dan relay sudah OFF.");
    } else {
      cloudAcknowledge(command, "REJECTED", "Tidak ada sesi aktif.");
    }
    return;
  }
  cloudAcknowledge(command, "REJECTED", "Jenis perintah tidak dikenali.");
}

void updateCloud() {
  if (!networkConnected()) return;

  const uint32_t now = millis();

  // Histori pending tetap menjadi prioritas.
  if (
    now - lastSyncMs >=
    Config::CLOUD_SYNC_INTERVAL_MS
  ) {
    lastSyncMs = now;

    const uint16_t pending =
      storagePendingCount();

    if (pending > 0) {
      Serial.printf(
        "[cloud] syncing pending history=%u\n",
        pending
      );

      cloudSyncOneHistory();
      lastSyncMs = millis();
      return;
    }
  }

  // Pembagian giliran:
  // 0 = heartbeat
  // 1 = command START/STOP
  // 2 = konfigurasi
  const uint8_t turn = cloudTurn;
  cloudTurn = (cloudTurn + 1) % 3;

  if (turn == 0) {
    cloudPublishLive(false);
    return;
  }

  if (turn == 1) {
    const uint32_t commandNow = millis();

    if (
      operatingMode == OperatingMode::ONLINE &&
      !offlineSessionLock &&
      commandNow - lastCommandPollMs >=
        Config::CLOUD_COMMAND_INTERVAL_MS
    ) {
      CloudCommand command;

      const bool received =
        cloudPollCommand(command);

      lastCommandPollMs = millis();

      if (received) {
        handleCommand(command);
      }
    }

    return;
  }

  if (turn == 2) {
    const uint32_t configNow = millis();

    if (
      !sessionIsActive() &&
      configNow - lastConfigPollMs >=
        Config::CLOUD_CONFIG_INTERVAL_MS
    ) {
      cloudFetchConfig();
      lastConfigPollMs = millis();
    }
  }
}
}

void setup() {
  hardwareForceSafeOutputs();
  Serial.begin(115200);
  delay(50);
  Serial.printf("\n%s firmware %s booting\n", Config::PROJECT_NAME, Config::FIRMWARE_VERSION);
  stateBegin();
  hardwareForceSafeOutputs();
  hardwareBegin();
  storageBegin();
  storageLoadConfig();
  sessionBegin();
  networkBegin();
  cloudBegin();

  if (sessionRecoverIfAvailable()) {
    menuScreen = MenuScreen::NONE;
    if (operatingMode == OperatingMode::ONLINE) networkStartOnline();
    else networkEnterManualOffline();
  } else {
    enterBootMenu();
  }
}

void loop() {
  hardwareUpdateButtons();
  const bool navPressed = hardwareTakeNavPress();
  const bool okPressed = hardwareTakeOkPress();

  if (alarmLatched && okPressed) {
    sessionAcknowledgeAlarm();
  } else if (menuScreen != MenuScreen::NONE) {
    handleMenuButtons(navPressed, okPressed);
  }

  networkUpdate();
  if (!manualOffline && sessionIsActive() && operatingMode == OperatingMode::ONLINE &&
      networkState == NetworkState::DISCONNECTED) {
    operatingMode = OperatingMode::OFFLINE;
    offlineSessionLock = true;
    Serial.println("[mode] Wi-Fi lost; session locked to OFFLINE until it finishes");
  }

  const bool freshSensorSample = hardwareUpdateSensor();
  sessionUpdate(freshSensorSample);

  if (offlineSessionLock && !sessionIsActive() && networkConnected()) {
    offlineSessionLock = false;
    operatingMode = OperatingMode::ONLINE;
    Serial.println("[mode] completed offline segment; ONLINE restored");
  }

  if ((manualOffline || offlineSessionLock) && sessionReadyForNextMenu() &&
      menuScreen == MenuScreen::NONE) {
    menuScreen = MenuScreen::OFFLINE_NEXT;
    menuIndex = 0;
  }

  const SessionEvent event = sessionTakeEvent();

  if (
  event == SessionEvent::NO_LOAD &&
  (manualOffline || offlineSessionLock) &&
  menuScreen == MenuScreen::NONE) 
  {
  menuScreen = MenuScreen::OFFLINE_NEXT;
  menuIndex = 0;
  }

  if (event != SessionEvent::NONE && networkConnected()) {
    cloudPublishLive(true);
    cloudPushEvent(event);
  }

  updateCloud();
  hardwareUpdateIndicators();
  hardwareRenderDisplay();
  delay(2);
}
