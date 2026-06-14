#include "touch_input.h"

#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "app_config.h"

namespace {
XPT2046_Touchscreen touchscreen(TOUCH_CS, TOUCH_IRQ);
uint32_t lastTouchToggleAtMs = 0;
}

void initTouch() {
  SPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touchscreen.begin();
  touchscreen.setRotation(SCREEN_ROTATION);
}

bool shouldToggleBacklight(uint32_t now) {
  if (!touchscreen.tirqTouched() || !touchscreen.touched()) {
    return false;
  }

  TS_Point touchPoint = touchscreen.getPoint();
  if (touchPoint.z < TOUCH_Z_MIN || now - lastTouchToggleAtMs <= TOUCH_DEBOUNCE_MS) {
    return false;
  }

  lastTouchToggleAtMs = now;
  return true;
}
