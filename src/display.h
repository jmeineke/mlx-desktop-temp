#pragma once

#include <Arduino.h>
#include "door_state.h"

void initDisplay();
void drawBackground();
void drawDoorBar(const Door doors[2]);
void clearStatusBar();
void drawWifiOfflineBar(uint32_t nextWifiRetryAt);
void drawWifiConnectingBar();
void drawGarageConnectingBar();
void drawGarageOfflineBar();
void drawSensorError();
void drawObjectTemp(float temperatureFahrenheit);
