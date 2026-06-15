#pragma once

#include <Arduino.h>

// --- debug overrides (set to true to test display without opening doors) ---
constexpr bool DEBUG_DOOR1_OPEN = false;
constexpr bool DEBUG_DOOR2_OPEN = false;

// --- wifi + garage ---
constexpr const char* WIFI_SSID = "MNet";
constexpr const char* WIFI_PASS = "1121121121abc";
constexpr const char* GARAGE_HOST = "garage";  // mDNS hostname (resolves garage.local)
constexpr const char* GARAGE_PATH = "/status";
constexpr uint32_t GARAGE_MS = 5000;  // poll interval
constexpr uint32_t GARAGE_HTTP_TIMEOUT_MS = 4900;
constexpr uint8_t GARAGE_FAILURES_BEFORE_OFFLINE = 3;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000;
constexpr uint32_t WIFI_RETRY_MS = 10000;

// --- hardware pins ---
constexpr uint8_t BL_PIN = 21;
constexpr uint8_t BL_CHANNEL = 0;
constexpr uint8_t SPK_PIN = 26;
constexpr uint8_t SPK_CHANNEL = 2;
constexpr uint8_t TOUCH_CS = 33;
constexpr uint8_t TOUCH_IRQ = 36;
constexpr uint8_t TOUCH_CLK = 25;
constexpr uint8_t TOUCH_MISO = 39;
constexpr uint8_t TOUCH_MOSI = 32;
constexpr uint8_t I2C_SDA = 22;
constexpr uint8_t I2C_SCL = 27;

// --- settings ---
constexpr uint8_t BL_LEVEL = 128;
constexpr int TOUCH_Z_MIN = 1500;
constexpr uint32_t TOUCH_DEBOUNCE_MS = 250;
constexpr uint16_t SPEAKER_IDLE_HZ = 1500;
constexpr uint32_t SAMPLE_MS = 50;
constexpr uint32_t DRAW_MS = 100;
constexpr int WIN = 6;
constexpr float TEMP_COLOR_GREEN_MIN_F = 115.0f;
constexpr float TEMP_COLOR_RED_MIN_F = 120.0f;

// --- colors (RGB565) ---
constexpr uint16_t BG = 0x0861;

// --- display geometry ---
// Rotation: 0/2 portrait, 1/3 landscape. Use 1 or 3 to flip current orientation 180 degrees.
constexpr uint8_t SCREEN_ROTATION = 1;
constexpr int BAR_H = 32;
constexpr int BAR_Y = 0;
constexpr int SPR_H_PX = 140;
constexpr int SPR_Y = BAR_H + (240 - BAR_H - SPR_H_PX) / 2;
