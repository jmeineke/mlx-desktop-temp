#include <Arduino.h>

#include "backlight_sound.h"
#include "display.h"
#include "garage_status.h"
#include "temperature_sensor.h"
#include "touch_input.h"
#include "wifi_manager.h"

namespace {
bool blOn = true;
bool doorWakeActive = false;

void handleTouch(uint32_t now) {
  if (!shouldToggleBacklight(now)) {
    return;
  }

  blOn = !blOn;
  if (blOn) {
    doorWakeActive = false;
  }
  setBacklightOn(blOn);
}

void handleStatusBar(uint32_t now) {
  static bool barDrawn = false;
  static bool lastWifiOnline = false;
  static bool lastGarageReady = false;
  static int32_t lastRetrySeconds = -1;

  bool wifiOnline = isWifiOnline();
  bool doorStateChanged = doors[0].open != doors[0].lastOpen || doors[1].open != doors[1].lastOpen;
  bool doorOpened = (doors[0].open && !doors[0].lastOpen) || (doors[1].open && !doors[1].lastOpen);
  bool allDoorsClosed = !doors[0].open && !doors[1].open;
  bool doorNameChanged = doors[0].name != doors[0].lastName || doors[1].name != doors[1].lastName;
  bool wifiStateChanged = wifiOnline != lastWifiOnline;
  bool garageReadyChanged = garageReady != lastGarageReady;

  if (!wifiOnline && wifiStateChanged) {
    markGarageNotReady();
  }

  uint32_t nextWifiRetryAt = getNextWifiRetryAt();
  uint32_t remainingMs = (nextWifiRetryAt > now) ? (nextWifiRetryAt - now) : 0;
  int32_t retrySeconds = (remainingMs + 999) / 1000;

  if (wifiInitialConnect && (!barDrawn || wifiStateChanged)) {
    drawWifiConnectingBar();
    barDrawn = true;
  } else if (!wifiOnline && (!barDrawn || wifiStateChanged || retrySeconds != lastRetrySeconds)) {
    drawWifiOfflineBar(nextWifiRetryAt);
    barDrawn = true;
  } else if (wifiOnline && !garageReady && (!barDrawn || wifiStateChanged)) {
    clearStatusBar();
    barDrawn = true;
  } else if (wifiOnline && garageReady && (!barDrawn || wifiStateChanged || garageReadyChanged || doorStateChanged || doorNameChanged)) {
    if (doorStateChanged) {
      if (doorOpened && !blOn) {
        blOn = true;
        doorWakeActive = true;
        setBacklightOn(true);
      }
      if (doorWakeActive && allDoorsClosed) {
        if (blOn) {
          blOn = false;
          setBacklightOn(false);
        }
        doorWakeActive = false;
      }
      beep();
    }
    drawDoorBar(doors);
    doors[0].lastOpen = doors[0].open;
    doors[1].lastOpen = doors[1].open;
    doors[0].lastName = doors[0].name;
    doors[1].lastName = doors[1].name;
    barDrawn = true;
  }

  lastWifiOnline = wifiOnline;
  lastGarageReady = garageReady;
  lastRetrySeconds = retrySeconds;
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
