#include "backlight_sound.h"

#include "app_config.h"

void initBacklight() {
  ledcSetup(BL_CHANNEL, 5000, 8);
  ledcAttachPin(BL_PIN, BL_CHANNEL);
  setBacklightOn(true);
}

void initSpeaker() {
  ledcSetup(SPK_CHANNEL, BEEP_HZ, 8);
  ledcAttachPin(SPK_PIN, SPK_CHANNEL);
}

void setBacklightOn(bool on) {
  ledcWrite(BL_CHANNEL, on ? BL_LEVEL : 0);
}

void beep() {
  ledcWriteTone(SPK_CHANNEL, BEEP_HZ);
  delay(BEEP_MS);
  ledcWriteTone(SPK_CHANNEL, 0);
}
