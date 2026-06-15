#pragma once

// --- debug overrides (set to true to test display without opening doors) ---
#define DEBUG_DOOR1_OPEN false
#define DEBUG_DOOR2_OPEN false

// --- wifi + garage ---
#define WIFI_SSID    "MNet"
#define WIFI_PASS    "1121121121abc"
#define GARAGE_HOST  "garage"        // mDNS hostname (resolves garage.local)
#define GARAGE_PATH  "/status"
#define GARAGE_MS    2000    // poll interval
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_RETRY_MS 10000

// --- hardware pins ---
#define BL_PIN        21
#define BL_CHANNEL    0
#define SPK_PIN       26
#define SPK_CHANNEL   2
#define TOUCH_CS      33
#define TOUCH_IRQ     36
#define TOUCH_CLK     25
#define TOUCH_MISO    39
#define TOUCH_MOSI    32
#define I2C_SDA       22
#define I2C_SCL       27

// --- settings ---
#define BL_LEVEL          128
#define TOUCH_Z_MIN       1500
#define TOUCH_DEBOUNCE_MS 250
#define BEEP_HZ       1500
#define BEEP_MS       200
#define SAMPLE_MS    50
#define DRAW_MS      100
#define WIN          6
#define TEMP_COLOR_GREEN_MIN_F 115.0f
#define TEMP_COLOR_RED_MIN_F 120.0f

// --- colors (RGB565) ---
#define BG    0x0861

// --- display geometry ---
// Rotation: 0/2 portrait, 1/3 landscape. Use 1 or 3 to flip current orientation 180 degrees.
#define SCREEN_ROTATION 1
#define BAR_H   32
#define BAR_Y   0
#define SPR_H_PX 140
#define SPR_Y   (BAR_H + (240 - BAR_H - SPR_H_PX) / 2)
