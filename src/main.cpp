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
  static bool wasWifiOnline = false;
  static bool wasGarageReady = false;
  static int32_t lastDisplayedRetrySeconds = -1;

  bool isWifiCurrentlyOnline = isWifiOnline();
  bool doorStateChanged = doors[0].open != doors[0].lastOpen || doors[1].open != doors[1].lastOpen;
  bool doorNameChanged = doors[0].name != doors[0].lastName || doors[1].name != doors[1].lastName;
  bool areAllDoorsClosed = !doors[0].open && !doors[1].open;
  bool garageStatusChanged = doorStateChanged || doorNameChanged;
  bool wifiStateChanged = isWifiCurrentlyOnline != wasWifiOnline;
  bool garageReadyChanged = garageReady != wasGarageReady;

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
  } else if (isWifiCurrentlyOnline && !garageReady && (!hasStatusBarBeenDrawn || wifiStateChanged)) {
    clearStatusBar();
    hasStatusBarBeenDrawn = true;
  } else if (isWifiCurrentlyOnline &&
             garageReady &&
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
      beep();
    }

    drawDoorBar(doors);
    doors[0].lastOpen = doors[0].open;
    doors[1].lastOpen = doors[1].open;
    doors[0].lastName = doors[0].name;
    doors[1].lastName = doors[1].name;
    hasStatusBarBeenDrawn = true;
  }

  wasWifiOnline = isWifiCurrentlyOnline;
  wasGarageReady = garageReady;
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
  if (handleTemperature(now) && !isBacklightOn) {
    isBacklightOn = true;
    setBacklightOn(true);
  }
}
