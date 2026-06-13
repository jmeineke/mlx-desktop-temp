#include "temperature_sensor.h"

#include <Adafruit_MLX90614.h>
#include <Wire.h>
#include <math.h>
#include "app_config.h"
#include "display.h"

namespace {
Adafruit_MLX90614 mlx;
float objBuf[WIN];
int   bufIdx  = 0;
bool  bufFull = false;
float lastObj = -999;

float trimmedMean(const float *buf, int n) {
  if (n < 4) {
    float sum = 0;
    for (int i = 0; i < n; i++) sum += buf[i];
    return sum / n;
  }

  float mn = buf[0], mx = buf[0], sum = 0;
  for (int i = 0; i < n; i++) {
    sum += buf[i];
    if (buf[i] < mn) mn = buf[i];
    if (buf[i] > mx) mx = buf[i];
  }
  return (sum - mn - mx) / (n - 2);
}
}

bool initTemperatureSensor() {
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(250);
  return mlx.begin();
}

void handleTemperature(uint32_t now) {
  static uint32_t lastSample = 0;
  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;
    objBuf[bufIdx] = mlx.readObjectTempF();
    bufIdx++;
    if (bufIdx >= WIN) {
      bufIdx = 0;
      bufFull = true;
    }
  }

  static uint32_t lastDraw = 0;
  if (now - lastDraw >= DRAW_MS) {
    lastDraw = now;
    int n = bufFull ? WIN : bufIdx;
    if (n > 0) {
      float obj = trimmedMean(objBuf, n);
      if (fabs(obj - lastObj) >= 0.1f) {
        drawObjectTemp(obj);
        lastObj = obj;
      }
    }
  }
}
