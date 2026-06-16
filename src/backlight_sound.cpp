#include "backlight_sound.h"

#include <freertos/queue.h>
#include "app_config.h"

namespace {
enum SoundSequenceId {
  SOUND_SEQUENCE_DOOR_CHIME,
  SOUND_SEQUENCE_WIFI_CHIME,
};

struct SoundSequence {
  const uint16_t* noteFrequencies;
  const uint16_t* noteDurationsMs;
  uint8_t noteCount;
  uint16_t gapMs;
  uint8_t repeatCount;
  uint16_t repeatGapMs;
};

constexpr uint16_t DOOR_CHIME_FREQUENCIES[] = { 1760, 880, 1760, 880 };
constexpr uint16_t DOOR_CHIME_DURATIONS_MS[] = { 45, 45, 45, 60 };
constexpr SoundSequence DOOR_CHIME = {
  DOOR_CHIME_FREQUENCIES,
  DOOR_CHIME_DURATIONS_MS,
  4,
  25,
  3,
  200,
};

constexpr uint16_t WIFI_CHIME_FREQUENCIES[] = { 784, 1047, 1319, 1568 };
constexpr uint16_t WIFI_CHIME_DURATIONS_MS[] = { 70, 80, 100, 170 };
constexpr SoundSequence WIFI_CHIME = {
  WIFI_CHIME_FREQUENCIES,
  WIFI_CHIME_DURATIONS_MS,
  4,
  25,
  1,
  0,
};

QueueHandle_t soundQueue = nullptr;

void stopSoundTone() {
  ledcWriteTone(SPK_CHANNEL, 0);
}

const SoundSequence& getSoundSequence(SoundSequenceId sequenceId) {
  return sequenceId == SOUND_SEQUENCE_WIFI_CHIME ? WIFI_CHIME : DOOR_CHIME;
}

void playSoundSequence(const SoundSequence& sequence) {
  for (uint8_t repeatIndex = 0; repeatIndex < sequence.repeatCount; repeatIndex++) {
    for (uint8_t noteIndex = 0; noteIndex < sequence.noteCount; noteIndex++) {
      ledcWriteTone(SPK_CHANNEL, sequence.noteFrequencies[noteIndex]);
      vTaskDelay(pdMS_TO_TICKS(sequence.noteDurationsMs[noteIndex]));
      stopSoundTone();
      if (noteIndex + 1 < sequence.noteCount) {
        vTaskDelay(pdMS_TO_TICKS(sequence.gapMs));
      }
    }

    if (repeatIndex + 1 < sequence.repeatCount) {
      vTaskDelay(pdMS_TO_TICKS(sequence.repeatGapMs));
    }
  }
}

void soundTask(void*) {
  SoundSequenceId sequenceId;
  for (;;) {
    if (xQueueReceive(soundQueue, &sequenceId, portMAX_DELAY) == pdTRUE) {
      playSoundSequence(getSoundSequence(sequenceId));
    }
  }
}

void queueSoundSequence(SoundSequenceId sequenceId) {
  if (soundQueue != nullptr) {
    xQueueOverwrite(soundQueue, &sequenceId);
  }
}
}

void initBacklight() {
  ledcSetup(BL_CHANNEL, 5000, 8);
  ledcAttachPin(BL_PIN, BL_CHANNEL);
  setBacklightOn(true);
}

void initSpeaker() {
  ledcSetup(SPK_CHANNEL, SPEAKER_IDLE_HZ, 8);
  ledcAttachPin(SPK_PIN, SPK_CHANNEL);
  soundQueue = xQueueCreate(1, sizeof(SoundSequenceId));
  xTaskCreatePinnedToCore(soundTask, "sound", 2048, nullptr, 1, nullptr, 0);
}

void setBacklightOn(bool on) {
  ledcWrite(BL_CHANNEL, on ? BL_LEVEL : 0);
}

void playDoorStatusChime() {
  queueSoundSequence(SOUND_SEQUENCE_DOOR_CHIME);
}

void playWifiConnectedChime() {
  queueSoundSequence(SOUND_SEQUENCE_WIFI_CHIME);
}
