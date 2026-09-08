#pragma once

#include "types.h"

void cloudBegin();
bool cloudPublishLive(bool force = false);
bool cloudPollCommand(CloudCommand& command);
bool cloudAcknowledge(const CloudCommand& command, const char* status, const String& message);
bool cloudFetchConfig();
bool cloudSyncOneHistory();
bool cloudPushEvent(SessionEvent event);
