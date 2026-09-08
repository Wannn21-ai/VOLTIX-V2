#pragma once

#include "types.h"
#include <ArduinoJson.h>

bool storageBegin();
bool storageLoadConfig();
bool storageSaveConfig();
bool storageSaveCheckpoint();
bool storageLoadCheckpoint(SessionData& session);
bool storageClearCheckpoint();
bool storageAppendHistory(const SessionData& session, const SensorReading& lastReading);
bool storageGetPendingHistory(DynamicJsonDocument& document, String& filePath);
bool storageMarkHistorySynced(const String& filePath);
uint16_t storageHistoryCount();
uint16_t storagePendingCount();
bool storageClearAll();
