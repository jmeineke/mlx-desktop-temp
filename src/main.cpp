#include <Arduino.h>

#include "backlight_sound.h"
#include "display.h"
#include "garage_status.h"
#include "temperature_sensor.h"
#include "touch_input.h"
#include "wifi_manager.h"

namespace {
bool isBacklightOn = true;
bool isDoorWakeActive = false;
Door displayedDoors[2] = {
  { false, "" },
  { false, "" },
};

void handleTouch(uint32_t now) {
  if (!shouldToggleBacklight(now)) {
    return;
  }

  isBacklightOn = !isBacklightOn;
  if (isBacklightOn) {
    isDoorWakeActive = false;
  }
  setBacklightOn(isBacklightOn);
}

void handleStatusBar(uint32_t now) {
  static bool hasStatusBarBeenDrawn = false;
  static bool hasEstablishedGarageBaseline = false;
  static bool wasWifiOnline = false;
  static bool wasGarageReady = false;
  static GarageConnectionState previousGarageConnectionState = GARAGE_CONNECTING;
  static int32_t lastDisplayedRetrySeconds = -1;

  bool isWifiCurrentlyOnline = isWifiOnline();
  Door currentDoors[2];
  bool isGarageReady = false;
  GarageConnectionState currentGarageConnectionState = GARAGE_CONNECTING;
  getGarageStatusSnapshot(currentDoors, isGarageReady, currentGarageConnectionState);

  bool doorStateChanged = currentDoors[0].open != displayedDoors[0].open ||
                          currentDoors[1].open != displayedDoors[1].open;
  bool doorNameChanged = currentDoors[0].name != displayedDoors[0].name ||
                         currentDoors[1].name != displayedDoors[1].name;
  bool areAllDoorsClosed = !currentDoors[0].open && !currentDoors[1].open;
  bool garageStatusChanged = doorStateChanged || doorNameChanged;
  bool wifiStateChanged = isWifiCurrentlyOnline != wasWifiOnline;
  bool garageReadyChanged = isGarageReady != wasGarageReady;
  bool garageConnectionStateChanged = currentGarageConnectionState != previousGarageConnectionState;

  if (!isWifiCurrentlyOnline && wifiStateChanged) {
    markGarageNotReady();
  }

  uint32_t nextWifiRetryAt = getNextWifiRetryAt();
  uint32_t remainingMs = (nextWifiRetryAt > now) ? (nextWifiRetryAt - now) : 0;
  int32_t remainingRetrySeconds = (remainingMs + 999) / 1000;

  if (wifiInitialConnect && (!hasStatusBarBeenDrawn || wifiStateChanged)) {
    drawWifiConnectingBar();
    hasStatusBarBeenDrawn = true;
  } else if (!isWifiCurrentlyOnline &&
             (!hasStatusBarBeenDrawn || wifiStateChanged || remainingRetrySeconds != lastDisplayedRetrySeconds)) {
    drawWifiOfflineBar(nextWifiRetryAt);
    hasStatusBarBeenDrawn = true;
  } else if (isWifiCurrentlyOnline &&
             currentGarageConnectionState == GARAGE_CONNECTING &&
             (!hasStatusBarBeenDrawn || wifiStateChanged || garageConnectionStateChanged)) {
    drawGarageConnectingBar();
    hasStatusBarBeenDrawn = true;
  } else if (isWifiCurrentlyOnline &&
             currentGarageConnectionState == GARAGE_OFFLINE &&
             (!hasStatusBarBeenDrawn || wifiStateChanged || garageConnectionStateChanged)) {
    drawGarageOfflineBar();
    hasStatusBarBeenDrawn = true;
  } else if (isWifiCurrentlyOnline &&
             isGarageReady &&
             (!hasStatusBarBeenDrawn || wifiStateChanged || garageReadyChanged || doorStateChanged || doorNameChanged)) {
    if (garageStatusChanged && !isBacklightOn) {
      isBacklightOn = true;
      isDoorWakeActive = !areAllDoorsClosed;
      setBacklightOn(true);
    }

    if (doorStateChanged) {
      if (isDoorWakeActive && areAllDoorsClosed) {
        isBacklightOn = false;
        setBacklightOn(false);
        isDoorWakeActive = false;
      }
    }

    if (hasEstablishedGarageBaseline) {
      if (garageStatusChanged) {
        playDoorStatusChime();
      }
    }

    drawDoorBar(currentDoors);
    displayedDoors[0] = currentDoors[0];
    displayedDoors[1] = currentDoors[1];
    hasEstablishedGarageBaseline = true;
    hasStatusBarBeenDrawn = true;
  }

  wasWifiOnline = isWifiCurrentlyOnline;
  wasGarageReady = isGarageReady;
  previousGarageConnectionState = currentGarageConnectionState;
  lastDisplayedRetrySeconds = remainingRetrySeconds;
}
}

void setup() {
  Serial.begin(115200);

  initBacklight();
  initSpeaker();
  bool sensorOk = initTemperatureSensor();

  initDisplay();
  initTouch();
  drawBackground();

  if (!sensorOk) {
    drawSensorError();
    Serial.println("MLX90614 not found");
  }

  drawWifiConnectingBar();
  beginInitialWifiConnect();
  startGarageTask();
}

void loop() {
  uint32_t now = millis();

  handleTouch(now);
  handleWifiMaintenance(now);
  handleStatusBar(now);
  handleTemperature(now);
}
