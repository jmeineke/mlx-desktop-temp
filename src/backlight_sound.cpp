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

void playDoorStatusChime() {
  const int noteFrequencies[] = { 1760, 880, 1760, 880 };
  const int noteDurationsMs[] = { 45, 45, 45, 60 };
  const int gapMs = 25;

  for (int i = 0; i < 4; i++) {
    ledcWriteTone(SPK_CHANNEL, noteFrequencies[i]);
    delay(noteDurationsMs[i]);
    ledcWriteTone(SPK_CHANNEL, 0);
    if (i < 3) {
      delay(gapMs);
    }
  }
}

void playWifiConnectedChime() {
  const int noteFrequencies[] = { 784, 1047, 1319, 1568 };
  const int noteDurationsMs[] = { 70, 80, 100, 170 };
  const int gapMs = 25;

  for (int i = 0; i < 4; i++) {
    ledcWriteTone(SPK_CHANNEL, noteFrequencies[i]);
    delay(noteDurationsMs[i]);
    ledcWriteTone(SPK_CHANNEL, 0);
    if (i < 3) {
      delay(gapMs);
    }
  }
}
