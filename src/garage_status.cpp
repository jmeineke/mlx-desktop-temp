#include "garage_status.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include "app_config.h"

Door doors[2] = {
  { false, false, "Door 1", "" },
  { false, false, "Door 2", "" },
};

volatile bool garageReady = false;
volatile GarageConnectionState garageConnectionState = GARAGE_CONNECTING;

namespace {
GarageConnectionState lastLoggedGarageConnectionState = GARAGE_CONNECTING;
IPAddress lastKnownGarageIp;

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

void garageTask(void *) {
  for (;;) {
    if (DEBUG_DOOR1_OPEN || DEBUG_DOOR2_OPEN) {
      doors[0] = { DEBUG_DOOR1_OPEN, doors[0].lastOpen, "Joe's door", doors[0].lastName };
      doors[1] = { DEBUG_DOOR2_OPEN, doors[1].lastOpen, "Elaine's door", doors[1].lastName };
      garageReady = true;
      garageConnectionState = GARAGE_READY;
      logGarageConnectionStateIfChanged();
    } else if (WiFi.status() == WL_CONNECTED) {
      IPAddress ip = lastKnownGarageIp;
      if (ip == IPAddress()) {
        ip = MDNS.queryHost(GARAGE_HOST);
        if (ip == IPAddress()) {
          garageReady = false;
          garageConnectionState = GARAGE_OFFLINE;
          Serial.println("[Garage] mDNS queryHost failed, no cached IP");
          logGarageConnectionStateIfChanged();
          vTaskDelay(pdMS_TO_TICKS(GARAGE_MS));
          continue;
        }

        lastKnownGarageIp = ip;
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
          doors[0].open = doc["single"]["open"].as<bool>();
          doors[1].open = doc["double"]["open"].as<bool>();
          doors[0].name = doc["single"]["name"] | "Door 1";
          doors[1].name = doc["double"]["name"] | "Door 2";
          lastKnownGarageIp = ip;
          garageReady = true;
          garageConnectionState = GARAGE_READY;
          logGarageConnectionStateIfChanged();
        } else {
          garageReady = false;
          garageConnectionState = GARAGE_OFFLINE;
          Serial.println("[Garage] JSON parse failed");
          logGarageConnectionStateIfChanged();
        }
      } else {
        garageReady = false;
        garageConnectionState = GARAGE_OFFLINE;
        Serial.print("[Garage] HTTP GET failed url=");
        Serial.print(statusUrl);
        Serial.print(" code=");
        Serial.println(code);
        if (code != HTTPC_ERROR_READ_TIMEOUT) {
          lastKnownGarageIp = IPAddress();
        } else {
          Serial.println("[Garage] Keeping cached IP after timeout");
        }
        logGarageConnectionStateIfChanged();
      }
      http.end();
    }
    vTaskDelay(pdMS_TO_TICKS(GARAGE_MS));
  }
}
}

void startGarageTask() {
  xTaskCreatePinnedToCore(garageTask, "garage", 8192, nullptr, 1, nullptr, 0);
}

void markGarageNotReady() {
  garageReady = false;
  garageConnectionState = GARAGE_CONNECTING;
  lastKnownGarageIp = IPAddress();
  logGarageConnectionStateIfChanged();
}
