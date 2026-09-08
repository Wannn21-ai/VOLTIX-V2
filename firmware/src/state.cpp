#include "state.h"
#include "config.h"

SensorReading sensorReading;
RuntimeConfig runtimeConfig;
SessionData currentSession;
OperatingMode operatingMode = OperatingMode::OFFLINE;
NetworkState networkState = NetworkState::DISCONNECTED;
MenuScreen menuScreen = MenuScreen::BOOT;
uint8_t menuIndex = 0;
bool relayOn = false;
bool alarmLatched = false;
bool manualOffline = false;
bool offlineSessionLock = false;

void stateBegin() {
  sensorReading = SensorReading();
  runtimeConfig.tariffPerKwh = Config::DEFAULT_TARIFF_PER_KWH;
  runtimeConfig.overloadThresholdW = Config::DEFAULT_OVERLOAD_W;
  runtimeConfig.loadPowerThresholdW = Config::DEFAULT_LOAD_POWER_W;
  runtimeConfig.loadCurrentThresholdA = Config::DEFAULT_LOAD_CURRENT_A;
  runtimeConfig.loadRemovedDelaySec = Config::DEFAULT_LOAD_REMOVED_DELAY_SEC;
  runtimeConfig.revision = 0;
  currentSession = SessionData();
  operatingMode = OperatingMode::OFFLINE;
  networkState = NetworkState::DISCONNECTED;
  menuScreen = MenuScreen::BOOT;
  menuIndex = 0;
  relayOn = false;
  alarmLatched = false;
  manualOffline = false;
  offlineSessionLock = false;
}

const char* operatingModeText(OperatingMode value) {
  return value == OperatingMode::ONLINE ? "ONLINE" : "OFFLINE";
}

const char* networkStateText(NetworkState value) {
  switch (value) {
    case NetworkState::DISCONNECTED: return "DISCONNECTED";
    case NetworkState::CONNECTING: return "CONNECTING";
    case NetworkState::CONNECTED: return "CONNECTED";
    case NetworkState::PORTAL: return "PORTAL";
  }
  return "UNKNOWN";
}

const char* sessionStateText(SessionState value) {
  switch (value) {
    case SessionState::IDLE: return "IDLE";
    case SessionState::VALIDATING_LOAD: return "VALIDATING_LOAD";
    case SessionState::MONITORING: return "MONITORING";
    case SessionState::FINISHING: return "FINISHING";
    case SessionState::FINISHED: return "FINISHED";
    case SessionState::OVERLOAD: return "OVERLOAD";
    case SessionState::ERROR: return "ERROR";
  }
  return "UNKNOWN";
}

const char* endReasonText(EndReason value) {
  switch (value) {
    case EndReason::NONE: return "NONE";
    case EndReason::USER_STOP: return "USER_STOP";
    case EndReason::LOAD_REMOVED: return "LOAD_REMOVED";
    case EndReason::LOAD_REMOVED_AFTER_POWER_LOSS: return "LOAD_REMOVED_AFTER_POWER_LOSS";
    case EndReason::OVERLOAD: return "OVERLOAD";
    case EndReason::NO_LOAD_DETECTED: return "NO_LOAD_DETECTED";
    case EndReason::SYSTEM_RESET: return "SYSTEM_RESET";
  }
  return "UNKNOWN";
}
