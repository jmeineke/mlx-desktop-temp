#include "display.h"

#include <TFT_eSPI.h>
#include "app_config.h"

namespace {
TFT_eSPI display;
TFT_eSprite temperatureSprite(&display);
}

void initDisplay() {
  display.init();
  display.setRotation(SCREEN_ROTATION);
  temperatureSprite.createSprite(320, SPR_H_PX);
}

void drawBackground() {
  display.fillScreen(BG);
}

void drawDoorBar(const Door doors[2]) {
  display.setTextDatum(MC_DATUM);
  for (int i = 0; i < 2; i++) {
    int x = i * 163;
    int width = 157;
    uint16_t color = doors[i].open ? TFT_RED : TFT_DARKGREEN;
    display.fillRect(x, BAR_Y, width, BAR_H, color);
    display.setTextColor(TFT_WHITE, color);
    display.drawString(doors[i].name, x + width / 2, BAR_Y + BAR_H / 2, 2);
  }
  display.fillRect(157, BAR_Y, 6, BAR_H, BG);
}

void clearStatusBar() {
  display.fillRect(0, BAR_Y, 320, BAR_H, BG);
}

void drawWifiOfflineBar(uint32_t nextWifiRetryAt) {
  display.fillRect(0, BAR_Y, 320, BAR_H, TFT_RED);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(TFT_WHITE, TFT_RED);
  uint32_t now = millis();
  uint32_t remainingMs = (nextWifiRetryAt > now) ? (nextWifiRetryAt - now) : 0;
  uint32_t remainingSec = (remainingMs + 999) / 1000;
  String message = "WIFI OFFLINE  RETRY " + String(remainingSec);
  display.drawString(message, 160, BAR_Y + BAR_H / 2, 2);
}

void drawWifiConnectingBar() {
  display.fillRect(0, BAR_Y, 320, BAR_H, TFT_ORANGE);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(TFT_BLACK, TFT_ORANGE);
  display.drawString("CONNECTING TO WIFI", 160, BAR_Y + BAR_H / 2, 2);
}

void drawGarageConnectingBar() {
  display.fillRect(0, BAR_Y, 320, BAR_H, TFT_DARKCYAN);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(TFT_WHITE, TFT_DARKCYAN);
  display.drawString("CONNECTING TO GARAGE", 160, BAR_Y + BAR_H / 2, 2);
}

void drawGarageOfflineBar() {
  display.fillRect(0, BAR_Y, 320, BAR_H, TFT_MAROON);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(TFT_WHITE, TFT_MAROON);
  display.drawString("GARAGE OFFLINE", 160, BAR_Y + BAR_H / 2, 2);
}

void drawSensorError() {
  display.setTextDatum(MC_DATUM);
  display.setTextColor(TFT_RED, BG);
  display.drawString("SENSOR NOT FOUND", 160, 120, 4);
}

void drawObjectTemp(float temperatureFahrenheit) {
  uint16_t color = (temperatureFahrenheit > TEMP_COLOR_RED_MIN_F) ? TFT_RED :
                   (temperatureFahrenheit >= TEMP_COLOR_GREEN_MIN_F) ? TFT_GREEN :
                   TFT_BLUE;
  temperatureSprite.fillSprite(BG);
  temperatureSprite.setTextDatum(TL_DATUM);
  temperatureSprite.setTextColor(color, BG);

  String temperatureText = String(temperatureFahrenheit, 1);
  int textWidth = temperatureSprite.textWidth(temperatureText, 8);
  int x = (320 - textWidth) / 2;
  int y = (SPR_H_PX - temperatureSprite.fontHeight(8)) / 2;

  temperatureSprite.drawString(temperatureText, x, y, 8);
  temperatureSprite.fillCircle(x + textWidth + 10, y + 6, 6, color);
  temperatureSprite.fillCircle(x + textWidth + 10, y + 6, 3, BG);
  temperatureSprite.pushSprite(0, SPR_Y);
}
