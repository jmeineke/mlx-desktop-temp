#include "touch_input.h"

#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "app_config.h"

namespace {
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
uint32_t lastTouchMs = 0;
}

void initTouch() {
  SPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin();
  ts.setRotation(SCREEN_ROTATION);
}

bool shouldToggleBacklight(uint32_t now) {
  if (!ts.tirqTouched() || !ts.touched()) {
    return false;
  }

  TS_Point p = ts.getPoint();
  if (p.z < TOUCH_Z_MIN || now - lastTouchMs <= TOUCH_DEBOUNCE_MS) {
    return false;
  }

  lastTouchMs = now;
  return true;
}
