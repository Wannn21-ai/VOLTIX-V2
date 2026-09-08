#include "storage.h"

#include "state.h"

#include <LittleFS.h>
#include <Preferences.h>

namespace {
constexpr const char* CHECKPOINT_PATH = "/active.json";
constexpr const char* CHECKPOINT_TEMP_PATH = "/active.tmp";
constexpr const char* HISTORY_DIR = "/history";
Preferences configPreferences;

bool writeJsonAtomic(const char* path, const char* temporaryPath, JsonDocument& document) {
  File file = LittleFS.open(temporaryPath, "w");
  if (!file) return false;
  const bool written = serializeJson(document, file) > 0;
  file.flush();
  file.close();
  if (!written) {
    LittleFS.remove(temporaryPath);
    return false;
  }
  LittleFS.remove(path);
  return LittleFS.rename(temporaryPath, path);
}

bool readJson(const String& path, DynamicJsonDocument& document) {
  File file = LittleFS.open(path, "r");
  if (!file) return false;
  const DeserializationError error = deserializeJson(document, file);
  file.close();
  return !error;
}

String historyPath(const String& sessionId) {
  return String(HISTORY_DIR) + "/" + sessionId + ".json";
}
}

bool storageBegin() {
  if (!LittleFS.begin(true)) {
    Serial.println("[storage] LittleFS mount failed");
    return false;
  }
  if (!LittleFS.exists(HISTORY_DIR)) LittleFS.mkdir(HISTORY_DIR);
  Serial.printf("[storage] ready history=%u pending=%u\n", storageHistoryCount(), storagePendingCount());
  return true;
}

bool storageLoadConfig() {
  if (!configPreferences.begin("voltix-config", true)) return false;
  runtimeConfig.tariffPerKwh = configPreferences.getFloat("tariff", runtimeConfig.tariffPerKwh);
  runtimeConfig.overloadThresholdW = configPreferences.getFloat("overload", runtimeConfig.overloadThresholdW);
  runtimeConfig.loadPowerThresholdW = configPreferences.getFloat("loadW", runtimeConfig.loadPowerThresholdW);
  runtimeConfig.loadCurrentThresholdA = configPreferences.getFloat("loadA", runtimeConfig.loadCurrentThresholdA);
  runtimeConfig.loadRemovedDelaySec = configPreferences.getUShort("removeSec", runtimeConfig.loadRemovedDelaySec);
  runtimeConfig.revision = configPreferences.getULong64("revision", runtimeConfig.revision);
  configPreferences.end();
  return true;
}

bool storageSaveConfig() {
  if (!configPreferences.begin("voltix-config", false)) return false;
  bool ok = true;
  ok = configPreferences.putFloat("tariff", runtimeConfig.tariffPerKwh) > 0 && ok;
  ok = configPreferences.putFloat("overload", runtimeConfig.overloadThresholdW) > 0 && ok;
  ok = configPreferences.putFloat("loadW", runtimeConfig.loadPowerThresholdW) > 0 && ok;
  ok = configPreferences.putFloat("loadA", runtimeConfig.loadCurrentThresholdA) > 0 && ok;
  ok = configPreferences.putUShort("removeSec", runtimeConfig.loadRemovedDelaySec) > 0 && ok;
  ok = configPreferences.putULong64("revision", runtimeConfig.revision) > 0 && ok;
  configPreferences.end();
  return ok;
}

bool storageSaveCheckpoint() {
  if (currentSession.state != SessionState::VALIDATING_LOAD &&
      currentSession.state != SessionState::MONITORING) return false;
  DynamicJsonDocument document(1536);
  document["active"] = true;
  document["id"] = currentSession.id;
  document["loadName"] = currentSession.loadName;
  document["startMode"] = operatingModeText(currentSession.startMode);
  document["state"] = sessionStateText(currentSession.state);
  document["startedAtEpochMs"] = currentSession.startedAtEpochMs;
  document["durationSec"] = currentSession.durationSec;
  document["timed"] =
    currentSession.timed;

document["durationLimitSec"] =
    currentSession.durationLimitSec;
  document["energyWh"] = currentSession.energyWh;
  document["cost"] = currentSession.cost;
  document["peakPowerW"] = currentSession.peakPowerW;
  document["averagePowerW"] = currentSession.averagePowerW;
  document["powerLossObserved"] = currentSession.powerLossObserved;
  return writeJsonAtomic(CHECKPOINT_PATH, CHECKPOINT_TEMP_PATH, document);
}

bool storageLoadCheckpoint(SessionData& session) {
  if (!LittleFS.exists(CHECKPOINT_PATH)) return false;
  DynamicJsonDocument document(1536);
  if (!readJson(CHECKPOINT_PATH, document) || document["active"] != true) return false;
  session = SessionData();
  session.id = document["id"].as<String>();
  session.loadName = document["loadName"].as<String>();
  session.startMode = String(document["startMode"] | "ONLINE") == "OFFLINE"
    ? OperatingMode::OFFLINE : OperatingMode::ONLINE;
  session.state = SessionState::VALIDATING_LOAD;
  session.startedAtEpochMs = document["startedAtEpochMs"] | static_cast<uint64_t>(0);
  session.durationSec = document["durationSec"] | 0U;
  session.recoveredDurationSec = session.durationSec;
  session.timed =
    document["timed"] | false;

session.durationLimitSec =
    document["durationLimitSec"] | 0U;

if (session.durationLimitSec == 0) {
    session.timed = false;
}
  session.energyWh = document["energyWh"] | 0.0f;
  session.baseEnergyWh = session.energyWh;
  session.cost = document["cost"] | 0.0f;
  session.peakPowerW = document["peakPowerW"] | 0.0f;
  session.averagePowerW = document["averagePowerW"] | 0.0f;
  session.powerLossObserved = document["powerLossObserved"] | true;
  session.recovered = true;
  return session.id.length() > 0;
}

bool storageClearCheckpoint() {
  LittleFS.remove(CHECKPOINT_TEMP_PATH);
  return !LittleFS.exists(CHECKPOINT_PATH) || LittleFS.remove(CHECKPOINT_PATH);
}

bool storageAppendHistory(const SessionData& session, const SensorReading& lastReading) {
  if (!LittleFS.exists(HISTORY_DIR)) LittleFS.mkdir(HISTORY_DIR);
  const String path = historyPath(session.id);
  const String tempPath = path + ".tmp";
  DynamicJsonDocument document(2048);
  document["id"] = session.id;
  document["deviceId"] = "voltix-001";
  document["loadName"] = session.loadName;
  document["mode"] = operatingModeText(session.startMode);
  document["endMode"] = operatingModeText(operatingMode);
  document["sessionState"] = sessionStateText(session.state);
  document["endReason"] = endReasonText(session.endReason);
  document["startedAt"] = session.startedAtEpochMs;
  document["endedAt"] = session.endedAtEpochMs;
  document["durationSec"] = session.durationSec;
  document["timed"] =
    session.timed;

document["requestedDurationSec"] =
    session.durationLimitSec;
  document["energyWh"] = session.energyWh;
  document["energyKwh"] = session.energyWh / 1000.0f;
  document["cost"] = session.cost;
  document["averagePowerW"] = session.averagePowerW;
  document["peakPowerW"] = session.peakPowerW;
  document["voltage"] = lastReading.voltage;
  document["current"] = lastReading.current;
  document["frequency"] = lastReading.frequency;
  document["powerFactor"] = lastReading.powerFactor;
  document["tariffPerKwh"] = runtimeConfig.tariffPerKwh;
  document["overloadThresholdW"] = runtimeConfig.overloadThresholdW;
  document["recovered"] = session.recovered;
  document["synced"] = false;
  return writeJsonAtomic(path.c_str(), tempPath.c_str(), document);
}

bool storageGetPendingHistory(DynamicJsonDocument& document, String& filePath) {
  File directory = LittleFS.open(HISTORY_DIR);
  if (!directory || !directory.isDirectory()) return false;
  File file = directory.openNextFile();
  while (file) {
    const String path = file.path();
    file.close();
    if (!path.endsWith(".tmp")) {
      document.clear();
      if (readJson(path, document) && document["synced"] != true) {
        filePath = path;
        directory.close();
        return true;
      }
    }
    file = directory.openNextFile();
  }
  directory.close();
  return false;
}

bool storageMarkHistorySynced(const String& filePath) {
  DynamicJsonDocument document(2048);
  if (!readJson(filePath, document)) return false;
  document["synced"] = true;
  const String tempPath = filePath + ".tmp";
  return writeJsonAtomic(filePath.c_str(), tempPath.c_str(), document);
}

uint16_t storageHistoryCount() {
  File directory = LittleFS.open(HISTORY_DIR);
  if (!directory || !directory.isDirectory()) return 0;
  uint16_t count = 0;
  File file = directory.openNextFile();
  while (file) {
    const String path = file.path();
    if (!file.isDirectory() && path.endsWith(".json")) ++count;
    file.close();
    file = directory.openNextFile();
  }
  directory.close();
  return count;
}

uint16_t storagePendingCount() {
  File directory = LittleFS.open(HISTORY_DIR);
  if (!directory || !directory.isDirectory()) return 0;
  uint16_t count = 0;
  File file = directory.openNextFile();
  while (file) {
    const String path = file.path();
    file.close();
    DynamicJsonDocument document(2304);
    if (path.endsWith(".json") && readJson(path, document) && document["synced"] != true) ++count;
    file = directory.openNextFile();
  }
  directory.close();
  return count;
}

bool storageClearAll() {
  storageClearCheckpoint();
  File directory = LittleFS.open(HISTORY_DIR);
  if (directory && directory.isDirectory()) {
    File file = directory.openNextFile();
    while (file) {
      const String path = file.path();
      file.close();
      LittleFS.remove(path);
      file = directory.openNextFile();
    }
    directory.close();
  }
  if (configPreferences.begin("voltix-config", false)) {
    configPreferences.clear();
    configPreferences.end();
  }
  return true;
}
