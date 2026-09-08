#pragma once

#include "types.h"

extern SensorReading sensorReading;
extern RuntimeConfig runtimeConfig;
extern SessionData currentSession;
extern OperatingMode operatingMode;
extern NetworkState networkState;
extern MenuScreen menuScreen;
extern uint8_t menuIndex;
extern bool relayOn;
extern bool alarmLatched;
extern bool manualOffline;
extern bool offlineSessionLock;

void stateBegin();
const char* operatingModeText(OperatingMode value);
const char* networkStateText(NetworkState value);
const char* sessionStateText(SessionState value);
const char* endReasonText(EndReason value);
