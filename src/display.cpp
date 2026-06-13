#include "display.h"

#include <TFT_eSPI.h>
#include "app_config.h"

namespace {
TFT_eSPI tft;
TFT_eSprite spr(&tft);
}

void initDisplay() {
  tft.init();
  tft.setRotation(3);
  spr.createSprite(320, SPR_H_PX);
}

void drawBackground() {
  tft.fillScreen(BG);
}

void drawDoorBar(const Door doors[2]) {
  tft.setTextDatum(MC_DATUM);
  for (int i = 0; i < 2; i++) {
    int x = i * 163;
    int w = 157;
    uint16_t color = doors[i].open ? TFT_RED : TFT_DARKGREEN;
    tft.fillRect(x, BAR_Y, w, BAR_H, color);
    tft.setTextColor(TFT_WHITE, color);
    tft.drawString(doors[i].name, x + w / 2, BAR_Y + BAR_H / 2, 2);
  }
  tft.fillRect(157, BAR_Y, 6, BAR_H, BG);
}

void clearStatusBar() {
  tft.fillRect(0, BAR_Y, 320, BAR_H, BG);
}

void drawWifiOfflineBar(uint32_t nextWifiRetryAt) {
  tft.fillRect(0, BAR_Y, 320, BAR_H, TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  uint32_t now = millis();
  uint32_t remainingMs = (nextWifiRetryAt > now) ? (nextWifiRetryAt - now) : 0;
  uint32_t remainingSec = (remainingMs + 999) / 1000;
  String msg = "WIFI OFFLINE  RETRY " + String(remainingSec);
  tft.drawString(msg, 160, BAR_Y + BAR_H / 2, 2);
}

void drawWifiConnectingBar() {
  tft.fillRect(0, BAR_Y, 320, BAR_H, TFT_ORANGE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_BLACK, TFT_ORANGE);
  tft.drawString("CONNECTING TO WIFI", 160, BAR_Y + BAR_H / 2, 2);
}

void drawSensorError() {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_RED, BG);
  tft.drawString("SENSOR NOT FOUND", 160, 120, 4);
}

void drawObjectTemp(float f) {
  uint16_t col = (f > 122) ? TFT_RED : (f >= 115) ? TFT_GREEN : TFT_BLUE;
  spr.fillSprite(BG);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(col, BG);

  String val  = String(f, 1);
  int    numW = spr.textWidth(val, 8);
  int    x    = (320 - numW) / 2;
  int    y    = (SPR_H_PX - spr.fontHeight(8)) / 2;

  spr.drawString(val, x, y, 8);
  spr.fillCircle(x + numW + 10, y + 6, 6, col);
  spr.fillCircle(x + numW + 10, y + 6, 3, BG);

  spr.pushSprite(0, SPR_Y);
}
