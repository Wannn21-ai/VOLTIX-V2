#include "cloud.h"

#include "config.h"
#include "network.h"
#include "session.h"
#include "state.h"
#include "storage.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace {
uint32_t lastLiveMs = 0;
String lastCommandId;
bool liveConnectionAnnounced = false;

String databaseUrl(const String& path) {
  return String(Config::FIREBASE_DATABASE_URL) + "/" + path + ".json";
}

int request(const char* method, const String& path, const String& payload, String* response = nullptr) {
  if (!networkConnected()) return -1;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(2200);
  http.setTimeout(2800);
  if (!http.begin(client, databaseUrl(path))) return -2;
  http.addHeader("Content-Type", "application/json");

  int code = -3;
  if (strcmp(method, "GET") == 0) code = http.GET();
  else if (strcmp(method, "PUT") == 0) code = http.PUT(payload);
  else if (strcmp(method, "PATCH") == 0) code = http.PATCH(payload);
  else if (strcmp(method, "POST") == 0) code = http.POST(payload);
  else if (strcmp(method, "DELETE") == 0) code = http.sendRequest("DELETE");
  if (response && code > 0) *response = http.getString();
  http.end();
  return code;
}

bool success(int code) {
  return code >= 200 && code < 300;
}

const char* eventText(SessionEvent event) {
  switch (event) {
    case SessionEvent::VALIDATION_STARTED: return "VALIDATION_STARTED";
    case SessionEvent::MONITORING_STARTED: return "MONITORING_STARTED";
    case SessionEvent::NO_LOAD: return "NO_LOAD";
    case SessionEvent::FINISHED: return "FINISHED";
    case SessionEvent::OVERLOAD_TRIGGERED: return "OVERLOAD_TRIGGERED";
    case SessionEvent::ALARM_ACKNOWLEDGED: return "ALARM_ACKNOWLEDGED";
    case SessionEvent::RECOVERY_STARTED: return "RECOVERY_STARTED";
    case SessionEvent::RECOVERY_RESUMED: return "RECOVERY_RESUMED";
    default: return "NONE";
  }
}

bool validConfig(float tariff, float overload, float loadPower, float loadCurrent,
                 uint16_t delaySec) {
  return tariff >= 0 && tariff <= 100000 && overload >= 10 && overload <= 10000 &&
         loadPower >= 0 && loadPower <= 100 && loadCurrent >= 0 && loadCurrent <= 5 &&
         delaySec >= 1 && delaySec <= 60;
}
}

void cloudBegin() {
  lastLiveMs = 0;
  lastCommandId = "";
  liveConnectionAnnounced = false;
}

bool cloudPublishLive(bool force) {
  const uint32_t now = millis();
  if (!networkConnected()) return false;
  if (!force && lastLiveMs > 0 && now - lastLiveMs < Config::CLOUD_LIVE_INTERVAL_MS) return false;

  DynamicJsonDocument document(3072);
  document["deviceId"] = Config::DEVICE_ID;
  document["firmwareVersion"] = Config::FIRMWARE_VERSION;
  document["operatingMode"] = operatingModeText(operatingMode);
  document["networkState"] = networkStateText(networkState);
  document["sessionState"] = sessionStateText(currentSession.state);
  document["relayOn"] = relayOn;
  document["alarmLatched"] = alarmLatched;
  document["offlineSessionLock"] = offlineSessionLock;
  document["pendingHistory"] = storagePendingCount();
  document["uptimeSec"] = now / 1000U;
  document.createNestedObject("heartbeat")[".sv"] = "timestamp";

  JsonObject sensor = document.createNestedObject("sensor");
  sensor["valid"] = sensorReading.valid;
  sensor["voltage"] = sensorReading.voltage;
  sensor["current"] = sensorReading.current;
  sensor["power"] = sensorReading.power;
  sensor["energy"] = sensorReading.energy;
  sensor["frequency"] = sensorReading.frequency;
  sensor["powerFactor"] = sensorReading.powerFactor;
  sensor["loadDetected"] = sensorReading.loadDetected;

  JsonObject session = document.createNestedObject("session");
  session["id"] = currentSession.id;
  session["loadName"] = currentSession.loadName;
  session["state"] = sessionStateText(currentSession.state);
  session["endReason"] = endReasonText(currentSession.endReason);
  session["durationSec"] = currentSession.durationSec;
  session["timed"] =
    currentSession.timed;

session["durationLimitSec"] =
    currentSession.durationLimitSec;

session["remainingSec"] =
    sessionRemainingSec();
  session["validationRemainingMs"] =
  sessionValidationRemainingMs();
  session["energyWh"] = currentSession.energyWh;
  session["energyKwh"] = currentSession.energyWh / 1000.0f;
  session["cost"] = currentSession.cost;
  session["peakPowerW"] = currentSession.peakPowerW;
  session["averagePowerW"] = currentSession.averagePowerW;
  session["recovered"] = currentSession.recovered;

  String payload;
  serializeJson(document, payload);
  const int code = request("PUT", "voltix/live", payload);
  lastLiveMs = millis();
  const bool published = success(code);

if (published && !liveConnectionAnnounced) {
  liveConnectionAnnounced = true;
  Serial.println("[cloud] Firebase heartbeat online");
} else if (!published) {
  liveConnectionAnnounced = false;
  Serial.printf("[cloud] live publish failed code=%d\n", code);
}

return published;
}

bool cloudPollCommand(CloudCommand& command) {
  if (!networkConnected() || operatingMode != OperatingMode::ONLINE || offlineSessionLock) return false;
  String response;
  const int code = request("GET", "voltix/command", "", &response);
  if (!success(code) || response == "null" || response.length() < 4) return false;

  DynamicJsonDocument document(1024);
  if (deserializeJson(document, response)) return false;
  const String id = document["id"].as<String>();
  if (id.length() == 0 || id == lastCommandId) return false;

  String ackResponse;
  const int ackCode = request("GET", "voltix/ack", "", &ackResponse);
  if (success(ackCode) && ackResponse != "null") {
    DynamicJsonDocument ackDocument(512);
    if (!deserializeJson(ackDocument, ackResponse) &&
        ackDocument["commandId"].as<String>() == id) {
      lastCommandId = id;
      request("DELETE", "voltix/command", "");
      return false;
    }
  }
  lastCommandId = id;
  command.id = id;
  command.type = document["type"].as<String>();
  command.loadName = document["loadName"].as<String>();
  command.durationSec =
    document["durationSec"] | 0U;
  command.createdAt = document["createdAt"] | static_cast<uint64_t>(0);
  command.type.toUpperCase();

  Serial.printf(
  "[cloud] command received id=%s type=%s\n",
  command.id.c_str(),
  command.type.c_str()
);
  return true;
}

bool cloudAcknowledge(const CloudCommand& command, const char* status, const String& message) {
  DynamicJsonDocument document(768);
  document["commandId"] = command.id;
  document["type"] = command.type;
  document["status"] = status;
  document["message"] = message;
  document["sessionId"] = currentSession.id;
  document.createNestedObject("acknowledgedAt")[".sv"] = "timestamp";
  String payload;
  serializeJson(document, payload);
  const bool acknowledged = success(request("PUT", "voltix/ack", payload));
  if (acknowledged) request("DELETE", "voltix/command", "");
  return acknowledged;
}

bool cloudFetchConfig() {
  if (!networkConnected() || sessionIsActive()) return false;
  String response;
  const int code = request("GET", "voltix/config", "", &response);
  if (!success(code) || response == "null") return false;
  DynamicJsonDocument document(1024);
  if (deserializeJson(document, response)) return false;
  const uint64_t revision = document["revision"] | static_cast<uint64_t>(0);
  if (revision <= runtimeConfig.revision) return false;

  const float tariff = document["tariffPerKwh"] | runtimeConfig.tariffPerKwh;
  const float overload = document["overloadThresholdW"] | runtimeConfig.overloadThresholdW;
  const float loadPower = document["loadPowerThresholdW"] | runtimeConfig.loadPowerThresholdW;
  const float loadCurrent = document["loadCurrentThresholdA"] | runtimeConfig.loadCurrentThresholdA;
  const uint16_t delaySec = document["loadRemovedDelaySec"] | runtimeConfig.loadRemovedDelaySec;
  if (!validConfig(tariff, overload, loadPower, loadCurrent, delaySec)) {
    Serial.println("[cloud] ignored invalid runtime config");
    return false;
  }
  runtimeConfig.tariffPerKwh = tariff;
  runtimeConfig.overloadThresholdW = overload;
  runtimeConfig.loadPowerThresholdW = loadPower;
  runtimeConfig.loadCurrentThresholdA = loadCurrent;
  runtimeConfig.loadRemovedDelaySec = delaySec;
  runtimeConfig.revision = revision;
  storageSaveConfig();
  Serial.printf(
  "[cloud] applied config revision=%llu overload=%.1f W\n",
  static_cast<unsigned long long>(revision),
  runtimeConfig.overloadThresholdW
);
  return true;
}

bool cloudSyncOneHistory() {
  if (!networkConnected()) return false;
  DynamicJsonDocument document(2304);
  String filePath;
  if (!storageGetPendingHistory(document, filePath)) return false;
  const String id = document["id"].as<String>();
  if (id.length() == 0) return false;
  document["synced"] = true;
  const uint64_t endedAt = document["endedAt"] | static_cast<uint64_t>(0);
  if (endedAt == 0) {
    document.remove("endedAt");
    document.createNestedObject("endedAt")[".sv"] = "timestamp";
  }
  document.createNestedObject("syncedAt")[".sv"] = "timestamp";
  String payload;
  serializeJson(document, payload);
  
  const int code = request("PUT", "voltix/history/" + id, payload);
  if (!success(code)) {
    Serial.printf("[cloud] history sync failed id=%s code=%d\n", id.c_str(), code);
    return false;
  }

  storageMarkHistorySynced(filePath);
  Serial.printf("[cloud] history synced id=%s\n", id.c_str());
  return true;
}

bool cloudPushEvent(SessionEvent event) {
  if (!networkConnected() || event == SessionEvent::NONE) return false;
  DynamicJsonDocument document(768);
  document["type"] = eventText(event);
  document["sessionId"] = currentSession.id;
  document["sessionState"] = sessionStateText(currentSession.state);
  document["endReason"] = endReasonText(currentSession.endReason);
  document.createNestedObject("createdAt")[".sv"] = "timestamp";
  String payload;
  serializeJson(document, payload);
  return success(request("POST", "voltix/events", payload));
}
