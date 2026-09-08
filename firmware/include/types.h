#pragma once

#include <Arduino.h>

enum class OperatingMode : uint8_t { ONLINE, OFFLINE };
enum class NetworkState : uint8_t { DISCONNECTED, CONNECTING, CONNECTED, PORTAL };
enum class SessionState : uint8_t {
  IDLE,
  VALIDATING_LOAD,
  MONITORING,
  FINISHING,
  FINISHED,
  OVERLOAD,
  ERROR
};
enum class EndReason : uint8_t {
    NONE,
    USER_STOP,
    LOAD_REMOVED,
    LOAD_REMOVED_AFTER_POWER_LOSS,
    OVERLOAD,
    NO_LOAD_DETECTED,
    TIMER_COMPLETED,
    SYSTEM_RESET
};
enum class MenuScreen : uint8_t { NONE, BOOT, OFFLINE_NEXT, RESET_CONFIRM };
enum class SessionEvent : uint8_t {
  NONE,
  VALIDATION_STARTED,
  MONITORING_STARTED,
  NO_LOAD,
  FINISHED,
  OVERLOAD_TRIGGERED,
  ALARM_ACKNOWLEDGED,
  RECOVERY_STARTED,
  RECOVERY_RESUMED
};

struct SensorReading {
  float voltage = 0;
  float current = 0;
  float power = 0;
  float energy = 0;
  float frequency = 0;
  float powerFactor = 0;
  bool valid = false;
  bool loadDetected = false;
  uint32_t readAtMs = 0;
};

struct RuntimeConfig {
  float tariffPerKwh = 1444.70f;
  float overloadThresholdW = 2000.0f;
  float loadPowerThresholdW = 1.0f;
  float loadCurrentThresholdA = 0.02f;
  uint16_t loadRemovedDelaySec = 3;
  uint64_t revision = 0;
};

struct SessionData {
  String id;
  String loadName;
  OperatingMode startMode = OperatingMode::ONLINE;
  SessionState state = SessionState::IDLE;
  EndReason endReason = EndReason::NONE;
  uint32_t startedAtMs = 0;
  uint64_t startedAtEpochMs = 0;
  uint64_t endedAtEpochMs = 0;
  uint32_t durationSec = 0;
  uint32_t recoveredDurationSec = 0;
  bool timed = false;
  uint32_t durationLimitSec = 0;
  float startEnergyKwh = 0;
  float baseEnergyWh = 0;
  float energyWh = 0;
  float cost = 0;
  float peakPowerW = 0;
  float averagePowerW = 0;
  bool recovered = false;
  bool powerLossObserved = false;
};

struct CloudCommand {
    String id;
    String type;
    String loadName;
    uint32_t durationSec = 0;
    uint64_t createdAt = 0;
};
