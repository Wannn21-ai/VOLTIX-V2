#pragma once

#include "types.h"

void sessionBegin();
bool sessionRecoverIfAvailable();
bool sessionStart(
     const String& loadName,
     uint32_t durationLimitSec = 0);
bool sessionStartOfflineNext();
void sessionStop(EndReason reason);
void sessionUpdate(bool freshSensorSample);
void sessionAcknowledgeAlarm();
void sessionSetIdle();
bool sessionIsActive();
bool sessionReadyForNextMenu();
SessionEvent sessionTakeEvent();
uint32_t sessionValidationRemainingMs();
uint32_t sessionRemainingSec();
