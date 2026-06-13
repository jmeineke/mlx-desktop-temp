#pragma once

#include <Arduino.h>

extern bool wifiInitialConnect;

void beginInitialWifiConnect();
void handleWifiMaintenance(uint32_t now);
bool isWifiOnline();
uint32_t getNextWifiRetryAt();
