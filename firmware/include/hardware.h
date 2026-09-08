#pragma once

#include <Arduino.h>

void hardwareForceSafeOutputs();
void hardwareBegin();
bool hardwareUpdateSensor();
void hardwareUpdateButtons();
bool hardwareTakeOkPress();
bool hardwareTakeNavPress();
void hardwareSetRelay(bool on);
void hardwareUpdateIndicators();
void hardwareRenderDisplay();
void hardwareClearOkPress();
