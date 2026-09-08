#include "session.h"

#include "config.h"
#include "hardware.h"
#include "state.h"
#include "storage.h"

#include <time.h>

namespace {
uint32_t validationStartedMs = 0;
bool validationCountdownStarted = false;
uint8_t stableLoadSamples = 0;
uint32_t noLoadStartedMs = 0;
uint32_t lastCheckpointMs = 0;
uint32_t finishedAtMs = 0;
SessionEvent pendingEvent = SessionEvent::NONE;

uint64_t epochMillis() {
  const time_t now = time(nullptr);
  return now > 1700000000 ? static_cast<uint64_t>(now) * 1000ULL : 0ULL;
}

String makeSessionId() {
  const uint64_t chip = ESP.getEfuseMac();
  char id[34];
  snprintf(id, sizeof(id), "S-%04X-%010lu", static_cast<unsigned int>(chip & 0xFFFF),
           static_cast<unsigned long>(millis()));
  return String(id);
}

void emit(SessionEvent event) {
  pendingEvent = event;
}

void updateTotals() {
  if (currentSession.state != SessionState::MONITORING) return;
  const uint32_t now = millis();
  currentSession.durationSec = currentSession.recoveredDurationSec +
    ((now - currentSession.startedAtMs) / 1000U);
  float deltaKwh = sensorReading.energy - currentSession.startEnergyKwh;
  if (deltaKwh < 0 || !sensorReading.valid) deltaKwh = 0;
  currentSession.energyWh = currentSession.baseEnergyWh + deltaKwh * 1000.0f;
  currentSession.cost = (currentSession.energyWh / 1000.0f) * runtimeConfig.tariffPerKwh;
  if (sensorReading.valid && sensorReading.power > currentSession.peakPowerW) {
    currentSession.peakPowerW = sensorReading.power;
  }
  currentSession.averagePowerW = currentSession.durationSec > 0
    ? currentSession.energyWh * 3600.0f / currentSession.durationSec : 0.0f;
}

void finishSession(EndReason reason) {
  if (currentSession.state == SessionState::MONITORING) {
    updateTotals();
  }

  if (sensorReading.valid &&
      sensorReading.power > currentSession.peakPowerW) {
    currentSession.peakPowerW = sensorReading.power;
  }
  currentSession.endReason = reason;
  currentSession.endedAtEpochMs = epochMillis();
  currentSession.state = reason == EndReason::OVERLOAD
    ? SessionState::OVERLOAD : SessionState::FINISHING;
  hardwareSetRelay(false);

  const bool saved = storageAppendHistory(currentSession, sensorReading);
  if (saved) {
    storageClearCheckpoint();
    if (reason == EndReason::OVERLOAD) {
  // Buang trigger OK lama/noise dari perpindahan relay.
  // Setelah ini hanya tekanan OK baru yang diterima.
  hardwareClearOkPress();

  alarmLatched = true;

  Serial.println(
    "[alarm] overload latched; waiting for OK"
  );

  emit(SessionEvent::OVERLOAD_TRIGGERED);
} else {
      currentSession.state = SessionState::FINISHED;
      finishedAtMs = millis();
      emit(SessionEvent::FINISHED);
    }
    Serial.printf("[session] finished id=%s reason=%s saved=yes\n",
                  currentSession.id.c_str(), endReasonText(reason));
  } else {
    currentSession.state = SessionState::ERROR;
    Serial.println("[session] history save failed; checkpoint retained");
  }
}

void beginMonitoring() {
  const uint32_t now = millis();
  currentSession.state = SessionState::MONITORING;
  currentSession.startedAtMs = now;
  currentSession.startEnergyKwh = sensorReading.energy;
  noLoadStartedMs = 0;
  lastCheckpointMs = now;
  storageSaveCheckpoint();
  emit(currentSession.recovered ? SessionEvent::RECOVERY_RESUMED : SessionEvent::MONITORING_STARTED);
  Serial.printf("[session] monitoring id=%s load=%s recovered=%s\n",
                currentSession.id.c_str(), currentSession.loadName.c_str(),
                currentSession.recovered ? "yes" : "no");
}
}

void sessionBegin() {
  validationStartedMs = 0;
  validationCountdownStarted = false;
  stableLoadSamples = 0;
  noLoadStartedMs = 0;
  lastCheckpointMs = 0;
  finishedAtMs = 0;
  pendingEvent = SessionEvent::NONE;
}

bool sessionRecoverIfAvailable() {
  SessionData checkpoint;
  if (!storageLoadCheckpoint(checkpoint)) return false;
  currentSession = checkpoint;
  operatingMode = checkpoint.startMode;
  if (operatingMode == OperatingMode::OFFLINE) manualOffline = true;
  currentSession.state = SessionState::VALIDATING_LOAD;
  validationStartedMs = millis();
  validationCountdownStarted = false;
  stableLoadSamples = 0;
  hardwareSetRelay(true);
  emit(SessionEvent::RECOVERY_STARTED);
  Serial.printf("[recovery] validating saved session %s\n", currentSession.id.c_str());
  return true;
}

bool sessionStart(const String& loadName) {
  if (sessionIsActive() || alarmLatched || currentSession.state == SessionState::ERROR) return false;
  currentSession = SessionData();
  currentSession.id = makeSessionId();
  currentSession.loadName = loadName.length() ? loadName.substring(0, 31) : "Tanpa Nama";
  currentSession.startMode = operatingMode;
  currentSession.state = SessionState::VALIDATING_LOAD;
  currentSession.startedAtEpochMs = epochMillis();
  validationStartedMs = millis();
  stableLoadSamples = 0;
  noLoadStartedMs = 0;
  lastCheckpointMs = validationStartedMs;
  validationCountdownStarted = false;
  finishedAtMs = 0;
  hardwareSetRelay(true);
  storageSaveCheckpoint();
  emit(SessionEvent::VALIDATION_STARTED);
  Serial.printf("[session] validation started id=%s load=%s mode=%s\n",
                currentSession.id.c_str(), currentSession.loadName.c_str(),
                operatingModeText(currentSession.startMode));
  return true;
}

bool sessionStartOfflineNext() {
  operatingMode = OperatingMode::OFFLINE;
  const String name = String("Offline Device ") + String(storageHistoryCount() + 1);
  sessionSetIdle();
  return sessionStart(name);
}

void sessionStop(EndReason reason) {
  if (currentSession.state == SessionState::VALIDATING_LOAD) {
    hardwareSetRelay(false);
    storageClearCheckpoint();
    currentSession.endReason = reason;
    currentSession.state = SessionState::IDLE;
    emit(SessionEvent::FINISHED);
    return;
  }
  if (currentSession.state != SessionState::MONITORING) return;
  finishSession(reason);
}

void sessionUpdate(bool freshSensorSample) {
  const uint32_t now = millis();

  // ==========================================================
  // VALIDASI BEBAN — 5 DETIK
  // ==========================================================
  if (
    currentSession.state ==
    SessionState::VALIDATING_LOAD
  ) {
    // Countdown dimulai dari sampel PZEM pertama.
    if (
      freshSensorSample &&
      !validationCountdownStarted
    ) {
      validationCountdownStarted = true;
      validationStartedMs = now;

      Serial.println(
        "[session] load validation countdown 5 seconds"
      );
    }

    if (!validationCountdownStarted) {
      return;
    }

    const uint32_t validationElapsed =
      now - validationStartedMs;

    // Proteksi overload tetap aktif saat validasi.
    if (
      freshSensorSample &&
      sensorReading.valid &&
      sensorReading.power >=
        runtimeConfig.overloadThresholdW
    ) {
      finishSession(EndReason::OVERLOAD);
      return;
    }

    // Validasi harus berjalan penuh selama 5 detik.
    if (
      validationElapsed <
      Config::LOAD_VALIDATION_TIMEOUT_MS
    ) {
      return;
    }

    if (
      sensorReading.valid &&
      sensorReading.loadDetected
    ) {
      beginMonitoring();
      return;
    }

    if (currentSession.recovered) {
      finishSession(
        EndReason::LOAD_REMOVED_AFTER_POWER_LOSS
      );
    } else {
      hardwareSetRelay(false);
      storageClearCheckpoint();

      currentSession.endReason =
        EndReason::NO_LOAD_DETECTED;

      currentSession.state =
        SessionState::IDLE;

      emit(SessionEvent::NO_LOAD);

      Serial.println(
        "[session] validation rejected: no load"
      );
    }

    return;
  }

  // ==========================================================
  // MONITORING
  // ==========================================================
  if (
    currentSession.state ==
    SessionState::MONITORING
  ) {
    updateTotals();

    if (
      now - lastCheckpointMs >=
      Config::CHECKPOINT_INTERVAL_MS
    ) {
      lastCheckpointMs = now;
      storageSaveCheckpoint();
    }

    if (
      !freshSensorSample ||
      !sensorReading.valid
    ) {
      return;
    }

    if (
      sensorReading.power >=
      runtimeConfig.overloadThresholdW
    ) {
      finishSession(EndReason::OVERLOAD);
      return;
    }

    const bool acPresent =
      sensorReading.voltage >=
      Config::AC_PRESENT_MIN_VOLTAGE;

    if (!acPresent) {
      currentSession.powerLossObserved = true;
      noLoadStartedMs = 0;
      storageSaveCheckpoint();
      return;
    }

    const bool belowThreshold =
      sensorReading.power <
        runtimeConfig.loadPowerThresholdW &&
      sensorReading.current <
        runtimeConfig.loadCurrentThresholdA;

    if (!belowThreshold) {
      noLoadStartedMs = 0;
      return;
    }

    if (noLoadStartedMs == 0) {
      noLoadStartedMs = now;
    }

    const uint32_t removeDelayMs =
      static_cast<uint32_t>(
        runtimeConfig.loadRemovedDelaySec
      ) * 1000U;

    if (
      now - noLoadStartedMs >=
      removeDelayMs
    ) {
      finishSession(
        currentSession.powerLossObserved
          ? EndReason::LOAD_REMOVED_AFTER_POWER_LOSS
          : EndReason::LOAD_REMOVED
      );
    }

    return;
  }

  // ==========================================================
  // SELESAI
  // ==========================================================
  if (
    currentSession.state ==
      SessionState::FINISHED &&
    operatingMode ==
      OperatingMode::ONLINE &&
    finishedAtMs > 0 &&
    now - finishedAtMs >=
      Config::FINISHED_SUMMARY_MS
  ) {
    sessionSetIdle();
  }
}

void sessionAcknowledgeAlarm() {
  if (!alarmLatched) return;
  alarmLatched = false;
  currentSession.state = SessionState::FINISHED;
  finishedAtMs = millis();

  // Matikan LED merah dan buzzer sebelum komunikasi Firebase.
  hardwareUpdateIndicators();

  emit(SessionEvent::ALARM_ACKNOWLEDGED);
  Serial.println("[alarm] acknowledged by OK button");
}

void sessionSetIdle() {
  if (alarmLatched || currentSession.state == SessionState::ERROR) return;
  hardwareSetRelay(false);
  currentSession.state = SessionState::IDLE;
  currentSession.endReason = EndReason::NONE;
  noLoadStartedMs = 0;
  stableLoadSamples = 0;
  finishedAtMs = 0;
}

bool sessionIsActive() {
  return currentSession.state == SessionState::VALIDATING_LOAD ||
         currentSession.state == SessionState::MONITORING ||
         currentSession.state == SessionState::FINISHING;
}

bool sessionReadyForNextMenu() {
  return currentSession.state == SessionState::FINISHED && !alarmLatched &&
         finishedAtMs > 0 && millis() - finishedAtMs >= Config::FINISHED_SUMMARY_MS;
}

SessionEvent sessionTakeEvent() {
  const SessionEvent event = pendingEvent;
  pendingEvent = SessionEvent::NONE;
  return event;
}

uint32_t sessionValidationRemainingMs() {
  if (
    currentSession.state !=
    SessionState::VALIDATING_LOAD
  ) {
    return 0;
  }

  if (!validationCountdownStarted) {
    return Config::LOAD_VALIDATION_TIMEOUT_MS;
  }

  const uint32_t elapsed =
    millis() - validationStartedMs;

  if (
    elapsed >=
    Config::LOAD_VALIDATION_TIMEOUT_MS
  ) {
    return 0;
  }

  return
    Config::LOAD_VALIDATION_TIMEOUT_MS -
    elapsed;
}