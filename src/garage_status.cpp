#include "garage_status.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <freertos/semphr.h>
#include "app_config.h"

namespace {
Door doors[2] = {
  { false, "Door 1" },
  { false, "Door 2" },
};

bool garageReady = false;
GarageConnectionState garageConnectionState = GARAGE_CONNECTING;
GarageConnectionState lastLoggedGarageConnectionState = GARAGE_CONNECTING;
IPAddress lastKnownGarageIp;
bool hasReceivedGarageStatus = false;
uint8_t consecutiveGarageFailures = 0;
SemaphoreHandle_t garageStatusMutex = nullptr;

const char* garageConnectionStateToString(GarageConnectionState state) {
  switch (state) {
    case GARAGE_CONNECTING: return "CONNECTING";
    case GARAGE_OFFLINE: return "OFFLINE";
    case GARAGE_READY: return "READY";
    default: return "UNKNOWN";
  }
}

void logGarageConnectionStateIfChanged() {
  if (garageConnectionState == lastLoggedGarageConnectionState) {
    return;
  }

  Serial.print("[Garage] State -> ");
  Serial.println(garageConnectionStateToString(garageConnectionState));
  lastLoggedGarageConnectionState = garageConnectionState;
}

void lockGarageStatus() {
  if (garageStatusMutex != nullptr) {
    xSemaphoreTake(garageStatusMutex, portMAX_DELAY);
  }
}

void unlockGarageStatus() {
  if (garageStatusMutex != nullptr) {
    xSemaphoreGive(garageStatusMutex);
  }
}

IPAddress getLastKnownGarageIp() {
  lockGarageStatus();
  IPAddress ip = lastKnownGarageIp;
  unlockGarageStatus();
  return ip;
}

void setLastKnownGarageIp(IPAddress ip) {
  lockGarageStatus();
  lastKnownGarageIp = ip;
  unlockGarageStatus();
}

void markGaragePollSucceeded(IPAddress ip, const Door updatedDoors[2]) {
  lockGarageStatus();
  doors[0] = updatedDoors[0];
  doors[1] = updatedDoors[1];
  lastKnownGarageIp = ip;
  hasReceivedGarageStatus = true;
  consecutiveGarageFailures = 0;
  garageReady = true;
  garageConnectionState = GARAGE_READY;
  logGarageConnectionStateIfChanged();
  unlockGarageStatus();
}

void markGaragePollFailed(const char* reason, bool clearCachedIp) {
  lockGarageStatus();
  if (clearCachedIp) {
    lastKnownGarageIp = IPAddress();
  }
  consecutiveGarageFailures++;
  Serial.print("[Garage] Poll failure ");
  Serial.print(consecutiveGarageFailures);
  Serial.print("/");
  Serial.print(GARAGE_FAILURES_BEFORE_OFFLINE);
  Serial.print(" reason=");
  Serial.println(reason);

  if (consecutiveGarageFailures < GARAGE_FAILURES_BEFORE_OFFLINE) {
    garageConnectionState = hasReceivedGarageStatus ? GARAGE_READY : GARAGE_CONNECTING;
    garageReady = hasReceivedGarageStatus;
  } else {
    garageReady = false;
    garageConnectionState = GARAGE_OFFLINE;
  }

  logGarageConnectionStateIfChanged();
  unlockGarageStatus();
}

void garageTask(void *) {
  for (;;) {
    if (DEBUG_DOOR1_OPEN || DEBUG_DOOR2_OPEN) {
      lockGarageStatus();
      doors[0] = { DEBUG_DOOR1_OPEN, "Joe's door" };
      doors[1] = { DEBUG_DOOR2_OPEN, "Elaine's door" };
      garageReady = true;
      garageConnectionState = GARAGE_READY;
      logGarageConnectionStateIfChanged();
      unlockGarageStatus();
    } else if (WiFi.status() == WL_CONNECTED) {
      IPAddress ip = getLastKnownGarageIp();
      if (ip == IPAddress()) {
        ip = MDNS.queryHost(GARAGE_HOST);
        if (ip == IPAddress()) {
          Serial.println("[Garage] mDNS queryHost failed, no cached IP");
          markGaragePollFailed("mdns", false);
          vTaskDelay(pdMS_TO_TICKS(GARAGE_MS));
          continue;
        }

        setLastKnownGarageIp(ip);
        Serial.print("[Garage] Resolved garage.local to ");
        Serial.println(ip);
      }

      HTTPClient http;
      String statusUrl = "http://" + ip.toString() + GARAGE_PATH;
      http.begin(statusUrl);
      http.setTimeout(GARAGE_HTTP_TIMEOUT_MS);
      http.setReuse(false);
      int code = http.GET();
      if (code == 200) {
        JsonDocument doc;
        if (!deserializeJson(doc, http.getString())) {
          Door updatedDoors[2] = {
            { doc["single"]["open"].as<bool>(), doc["single"]["name"] | "Door 1" },
            { doc["double"]["open"].as<bool>(), doc["double"]["name"] | "Door 2" },
          };
          markGaragePollSucceeded(ip, updatedDoors);
        } else {
          Serial.println("[Garage] JSON parse failed");
          markGaragePollFailed("json", false);
        }
      } else {
        Serial.print("[Garage] HTTP GET failed url=");
        Serial.print(statusUrl);
        Serial.print(" code=");
        Serial.println(code);
        bool isTimeout = code == HTTPC_ERROR_READ_TIMEOUT;
        if (isTimeout) {
          Serial.println("[Garage] Keeping cached IP after timeout");
        }
        markGaragePollFailed("http", !isTimeout);
      }
      http.end();
    }
    vTaskDelay(pdMS_TO_TICKS(GARAGE_MS));
  }
}
}

void startGarageTask() {
  garageStatusMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(garageTask, "garage", 8192, nullptr, 1, nullptr, 0);
}

void getGarageStatusSnapshot(Door snapshotDoors[2], bool &snapshotGarageReady, GarageConnectionState &snapshotConnectionState) {
  lockGarageStatus();
  snapshotDoors[0] = doors[0];
  snapshotDoors[1] = doors[1];
  snapshotGarageReady = garageReady;
  snapshotConnectionState = garageConnectionState;
  unlockGarageStatus();
}

void markGarageNotReady() {
  lockGarageStatus();
  garageReady = false;
  garageConnectionState = GARAGE_CONNECTING;
  lastKnownGarageIp = IPAddress();
  hasReceivedGarageStatus = false;
  consecutiveGarageFailures = 0;
  logGarageConnectionStateIfChanged();
  unlockGarageStatus();
}
