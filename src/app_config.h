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
#define SPK_CHANNEL   1
#define TOUCH_CS      33
#define TOUCH_IRQ     36
#define TOUCH_CLK     25
#define TOUCH_MISO    39
#define TOUCH_MOSI    32
#define I2C_SDA       22
#define I2C_SCL       27

// --- settings ---
#define BL_LEVEL          128
#define TOUCH_Z_MIN       1000
#define TOUCH_DEBOUNCE_MS 250
#define BEEP_HZ       2000
#define BEEP_MS       200
#define SAMPLE_MS    50
#define DRAW_MS      250
#define WIN          20

// --- colors (RGB565) ---
#define BG    0x0861

// --- display geometry ---
#define BAR_H   32
#define BAR_Y   0
#define SPR_H_PX 140
#define SPR_Y   (BAR_H + (240 - BAR_H - SPR_H_PX) / 2)
