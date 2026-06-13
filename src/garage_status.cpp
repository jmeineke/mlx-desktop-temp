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

namespace {
void garageTask(void *) {
  for (;;) {
    if (DEBUG_DOOR1_OPEN || DEBUG_DOOR2_OPEN) {
      doors[0] = { DEBUG_DOOR1_OPEN, doors[0].lastOpen, "Joe's door", doors[0].lastName };
      doors[1] = { DEBUG_DOOR2_OPEN, doors[1].lastOpen, "Elaine's door", doors[1].lastName };
    } else if (WiFi.status() == WL_CONNECTED) {
      IPAddress ip = MDNS.queryHost(GARAGE_HOST);
      if (ip == IPAddress()) {
        vTaskDelay(pdMS_TO_TICKS(GARAGE_MS));
        continue;
      }

      HTTPClient http;
      http.begin("http://" + ip.toString() + GARAGE_PATH);
      int code = http.GET();
      if (code == 200) {
        JsonDocument doc;
        if (!deserializeJson(doc, http.getString())) {
          doors[0].open = doc["single"]["open"].as<bool>();
          doors[1].open = doc["double"]["open"].as<bool>();
          doors[0].name = doc["single"]["name"] | "Door 1";
          doors[1].name = doc["double"]["name"] | "Door 2";
          garageReady = true;
        }
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
}
