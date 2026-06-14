#include "wifi_manager.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include "app_config.h"
#include "backlight_sound.h"

bool wifiInitialConnect = false;

namespace {
bool mdnsStarted = false;
uint32_t nextWifiRetryAt = 0;
bool wasWifiConnected = false;

void startWifiConnectAttempt() {
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  nextWifiRetryAt = millis() + WIFI_RETRY_MS;
}

void startMdnsIfNeeded() {
  if (!mdnsStarted && WiFi.status() == WL_CONNECTED) {
    mdnsStarted = MDNS.begin("cyd");
  }
}

void handleWifiConnectedTransition() {
  bool isWifiConnected = WiFi.status() == WL_CONNECTED;
  if (isWifiConnected && !wasWifiConnected) {
    playWifiConnectedChime();
  }
  wasWifiConnected = isWifiConnected;
}
}

void beginInitialWifiConnect() {
  wifiInitialConnect = true;
  startWifiConnectAttempt();
  Serial.print("Connecting to WiFi");

  uint32_t wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }

  wifiInitialConnect = false;
  Serial.println(WiFi.status() == WL_CONNECTED ? " connected" : " offline");
  startMdnsIfNeeded();
  handleWifiConnectedTransition();
}

void handleWifiMaintenance(uint32_t now) {
  if (WiFi.status() != WL_CONNECTED && !wifiInitialConnect && (int32_t)(now - nextWifiRetryAt) >= 0) {
    Serial.println("Retrying WiFi");
    startWifiConnectAttempt();
  }
  startMdnsIfNeeded();
  handleWifiConnectedTransition();
}

bool isWifiOnline() {
  return WiFi.status() == WL_CONNECTED;
}

uint32_t getNextWifiRetryAt() {
  return nextWifiRetryAt;
}
