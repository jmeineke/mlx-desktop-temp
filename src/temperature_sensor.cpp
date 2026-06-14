#include "temperature_sensor.h"

#include <Adafruit_MLX90614.h>
#include <Wire.h>
#include <math.h>
#include "app_config.h"
#include "display.h"

namespace {
Adafruit_MLX90614 infraredTemperatureSensor;
float objectTemperatureSamples[WIN];
int sampleBufferIndex = 0;
bool sampleBufferFilled = false;
float lastDisplayedObjectTemperatureFahrenheit = -999;

float trimmedMean(const float *samples, int sampleCount) {
  if (sampleCount < 4) {
    float sum = 0;
    for (int i = 0; i < sampleCount; i++) {
      sum += samples[i];
    }
    return sum / sampleCount;
  }

  float minimumSample = samples[0];
  float maximumSample = samples[0];
  float sum = 0;
  for (int i = 0; i < sampleCount; i++) {
    sum += samples[i];
    if (samples[i] < minimumSample) minimumSample = samples[i];
    if (samples[i] > maximumSample) maximumSample = samples[i];
  }
  return (sum - minimumSample - maximumSample) / (sampleCount - 2);
}
}

bool initTemperatureSensor() {
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(250);
  return infraredTemperatureSensor.begin();
}

bool handleTemperature(uint32_t now) {
  bool hasDrasticTemperatureChange = false;

  static uint32_t lastSampleAtMs = 0;
  if (now - lastSampleAtMs >= SAMPLE_MS) {
    lastSampleAtMs = now;
    objectTemperatureSamples[sampleBufferIndex] = infraredTemperatureSensor.readObjectTempF();
    sampleBufferIndex++;
    if (sampleBufferIndex >= WIN) {
      sampleBufferIndex = 0;
      sampleBufferFilled = true;
    }
  }

  static uint32_t lastDrawAtMs = 0;
  if (now - lastDrawAtMs >= DRAW_MS) {
    lastDrawAtMs = now;
    int sampleCount = sampleBufferFilled ? WIN : sampleBufferIndex;
    if (sampleCount > 0) {
      float objectTemperatureFahrenheit = trimmedMean(objectTemperatureSamples, sampleCount);
      if (lastDisplayedObjectTemperatureFahrenheit > -998 &&
          fabs(objectTemperatureFahrenheit - lastDisplayedObjectTemperatureFahrenheit) >= TEMP_WAKE_DELTA_F) {
        hasDrasticTemperatureChange = true;
      }
      if (fabs(objectTemperatureFahrenheit - lastDisplayedObjectTemperatureFahrenheit) >= 0.1f) {
        drawObjectTemp(objectTemperatureFahrenheit);
        lastDisplayedObjectTemperatureFahrenheit = objectTemperatureFahrenheit;
      }
    }
  }

  return hasDrasticTemperatureChange;
}
