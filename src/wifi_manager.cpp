#include "wifi_manager.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include "app_config.h"
#include "backlight_sound.h"

bool wifiInitialConnect = false;

namespace {
bool mdnsStarted = false;
uint32_t nextWifiRetryAt = 0;
wl_status_t lastLoggedWifiStatus = WL_IDLE_STATUS;
bool wasWifiConnected = false;

const char* wifiStatusToString(wl_status_t status) {
  switch (status) {
    case WL_NO_SHIELD: return "WL_NO_SHIELD";
    case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
    case WL_CONNECTED: return "WL_CONNECTED";
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED: return "WL_DISCONNECTED";
    default: return "WL_UNKNOWN";
  }
}

void logWifiStatusIfChanged() {
  wl_status_t status = WiFi.status();
  if (status == lastLoggedWifiStatus) {
    return;
  }

  Serial.print("[WiFi] Status -> ");
  Serial.print(wifiStatusToString(status));
  if (status == WL_CONNECTED) {
    Serial.print(" IP=");
    Serial.print(WiFi.localIP());
    Serial.print(" RSSI=");
    Serial.print(WiFi.RSSI());
  }
  Serial.println();
  lastLoggedWifiStatus = status;
}

void startWifiConnectAttempt() {
  Serial.print("[WiFi] Begin connect SSID=");
  Serial.println(WIFI_SSID);
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  nextWifiRetryAt = millis() + WIFI_RETRY_MS;
}

void startMdnsIfNeeded() {
  if (!mdnsStarted && WiFi.status() == WL_CONNECTED) {
    mdnsStarted = MDNS.begin("cyd");
    Serial.print("[WiFi] mDNS start ");
    Serial.println(mdnsStarted ? "OK" : "FAIL");
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
  logWifiStatusIfChanged();
  startMdnsIfNeeded();
  handleWifiConnectedTransition();
}

void handleWifiMaintenance(uint32_t now) {
  logWifiStatusIfChanged();
  if (WiFi.status() != WL_CONNECTED && !wifiInitialConnect && (int32_t)(now - nextWifiRetryAt) >= 0) {
    Serial.println("[WiFi] Retrying connect");
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
