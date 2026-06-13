#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

// --- debug overrides (set to true to test display without opening doors) ---
#define DEBUG_DOOR1_OPEN false
#define DEBUG_DOOR2_OPEN false

// --- wifi + garage ---
#define WIFI_SSID    "MNet"
#define WIFI_PASS    "1121121121abc"
#define GARAGE_HOST  "garage"        // mDNS hostname (resolves garage.local)
#define GARAGE_PATH  "/status"
#define GARAGE_MS    1000    // poll interval
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

// --- settings ---
#define BL_LEVEL          128
#define TOUCH_Z_MIN       100
#define TOUCH_DEBOUNCE_MS 500
#define BEEP_HZ       2000
#define BEEP_MS       200
#define SAMPLE_MS    50
#define DRAW_MS      250
#define WIN          20

// --- colors (RGB565) ---
#define BG    0x0861

// --- door alert bar geometry ---
#define BAR_H   32
#define BAR_Y   0
#define SPR_H_PX 140
#define SPR_Y   (BAR_H + (240 - BAR_H - SPR_H_PX) / 2)

TFT_eSPI            tft;
TFT_eSprite         spr(&tft);
Adafruit_MLX90614   mlx;
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
bool                blOn = true;
bool                doorWakeActive = false;

volatile bool garageReady = false;
bool          mdnsStarted = false;
bool          wifiInitialConnect = false;
uint32_t      nextWifiRetryAt = 0;
IPAddress     garageIp;
float objBuf[WIN];
int   bufIdx  = 0;
bool  bufFull = false;
float lastObj = -999;

struct Door {
  bool   open;
  bool   lastOpen;
  String name;
  String lastName;
};
Door doors[2] = {
  { false, false, "Door 1", "" },
  { false, false, "Door 2", "" },
};

// --- helpers ---

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

// --- drawing ---

void drawBackground() {
  tft.fillScreen(BG);
}

void beep() {
  ledcWriteTone(SPK_CHANNEL, BEEP_HZ);
  delay(BEEP_MS);
  ledcWriteTone(SPK_CHANNEL, 0);
}

void drawDoorBar() {
  tft.setTextDatum(MC_DATUM);
  for (int i = 0; i < 2; i++) {
    int x = i * 163;
    int w = 157;
    uint16_t color = doors[i].open ? TFT_RED : TFT_DARKGREEN;
    tft.fillRect(x, BAR_Y, w, BAR_H, color);
    tft.setTextColor(TFT_WHITE, color);
    tft.drawString(doors[i].name, x + w / 2, BAR_Y + BAR_H / 2, 2);
  }
  tft.fillRect(157, BAR_Y, 6, BAR_H, BG);
}

void clearStatusBar() {
  tft.fillRect(0, BAR_Y, 320, BAR_H, BG);
}

void drawWifiOfflineBar() {
  tft.fillRect(0, BAR_Y, 320, BAR_H, TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  uint32_t now = millis();
  uint32_t remainingMs = (nextWifiRetryAt > now) ? (nextWifiRetryAt - now) : 0;
  uint32_t remainingSec = (remainingMs + 999) / 1000;
  String msg = "WIFI OFFLINE  RETRY " + String(remainingSec);
  tft.drawString(msg, 160, BAR_Y + BAR_H / 2, 2);
}

void drawWifiConnectingBar() {
  tft.fillRect(0, BAR_Y, 320, BAR_H, TFT_ORANGE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_BLACK, TFT_ORANGE);
  tft.drawString("CONNECTING TO WIFI", 160, BAR_Y + BAR_H / 2, 2);
}

void startWifiConnectAttempt() {
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  nextWifiRetryAt = millis() + WIFI_RETRY_MS;
  garageIp = IPAddress();
}

bool resolveGarageIpIfNeeded() {
  if (garageIp != IPAddress()) {
    return true;
  }

  garageIp = MDNS.queryHost(GARAGE_HOST);
  return garageIp != IPAddress();
}

void drawObjectTemp(float f) {
  uint16_t col = (f > 122) ? TFT_RED : (f >= 115) ? TFT_GREEN : TFT_BLUE;
  spr.fillSprite(BG);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(col, BG);

  String val  = String(f, 1);
  int    numW = spr.textWidth(val, 8);
  int    x    = (320 - numW) / 2;
  int    y    = (SPR_H_PX - spr.fontHeight(8)) / 2;

  spr.drawString(val, x, y, 8);
  spr.fillCircle(x + numW + 10, y + 6, 6, col);
  spr.fillCircle(x + numW + 10, y + 6, 3, BG);

  spr.pushSprite(0, SPR_Y);
}

// --- garage polling (runs on core 0, main loop on core 1) ---

void garageTask(void *) {
  for (;;) {
    if (DEBUG_DOOR1_OPEN || DEBUG_DOOR2_OPEN) {
      doors[0] = { DEBUG_DOOR1_OPEN, doors[0].lastOpen, "Joe's door", doors[0].lastName };
      doors[1] = { DEBUG_DOOR2_OPEN, doors[1].lastOpen, "Elaine's door", doors[1].lastName };
    } else if (WiFi.status() == WL_CONNECTED) {
      if (!resolveGarageIpIfNeeded()) { vTaskDelay(pdMS_TO_TICKS(GARAGE_MS)); continue; }
      HTTPClient http;
      http.begin("http://" + garageIp.toString() + GARAGE_PATH);
      int code = http.GET();
      if (code == 200) {
        JsonDocument doc;
        if (!deserializeJson(doc, http.getString())) {
          doors[0].open = doc["single"]["open"].as<bool>();
          doors[1].open = doc["double"]["open"].as<bool>();
          doors[0].name = doc["single"]["name"] | "Door 1";
          doors[1].name = doc["double"]["name"] | "Door 2";
          garageReady = true;
        }
      } else {
        garageIp = IPAddress();
      }
      http.end();
    }
    vTaskDelay(pdMS_TO_TICKS(GARAGE_MS));
  }
}

// --- setup ---

void startMdnsIfNeeded() {
  if (!mdnsStarted && WiFi.status() == WL_CONNECTED) {
    mdnsStarted = MDNS.begin("cyd");
  }
}

void setup() {
  Serial.begin(115200);

  ledcSetup(BL_CHANNEL, 5000, 8);
  ledcAttachPin(BL_PIN, BL_CHANNEL);
  ledcWrite(BL_CHANNEL, BL_LEVEL);

  ledcSetup(SPK_CHANNEL, BEEP_HZ, 8);
  ledcAttachPin(SPK_PIN, SPK_CHANNEL);

  Wire.begin(22, 27);
  delay(250);
  bool sensorOk = mlx.begin();

  tft.init();
  tft.setRotation(3);
  spr.createSprite(320, SPR_H_PX);

  // touch must init AFTER tft.init() — TFT uses VSPI and clobbers touch pins otherwise
  SPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin();
  ts.setRotation(3);

  drawBackground();

  if (!sensorOk) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, BG);
    tft.drawString("SENSOR NOT FOUND", 160, 120, 4);
    Serial.println("MLX90614 not found");
  }

  wifiInitialConnect = true;
  drawWifiConnectingBar();
  startWifiConnectAttempt();
  Serial.print("Connecting to WiFi");
  uint32_t wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  wifiInitialConnect = false;
  Serial.println(WiFi.status() == WL_CONNECTED ? " connected" : " offline");
  startMdnsIfNeeded();

  xTaskCreatePinnedToCore(garageTask, "garage", 8192, nullptr, 1, nullptr, 0);
}

// --- loop ---

void loop() {
  uint32_t now = millis();

  // touch: toggle backlight
  static uint32_t lastTouchMs = 0;
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();
    if (p.z >= TOUCH_Z_MIN && now - lastTouchMs > TOUCH_DEBOUNCE_MS) {
      lastTouchMs = now;
      blOn = !blOn;
      if (blOn) {
        doorWakeActive = false;
      }
      ledcWrite(BL_CHANNEL, blOn ? BL_LEVEL : 0);
    }
  }

  startMdnsIfNeeded();

  // redraw the top bar for WiFi state first, then door state when online
  static bool barDrawn = false;
  static bool lastWifiOnline = false;
  static bool lastGarageReady = false;
  static int32_t lastRetrySeconds = -1;
  bool wifiOnline = WiFi.status() == WL_CONNECTED;
  bool doorStateChanged = doors[0].open != doors[0].lastOpen || doors[1].open != doors[1].lastOpen;
  bool doorNameChanged = doors[0].name != doors[0].lastName || doors[1].name != doors[1].lastName;
  bool wifiStateChanged = wifiOnline != lastWifiOnline;
  bool garageReadyChanged = garageReady != lastGarageReady;

  if (!wifiOnline && !wifiInitialConnect && (int32_t)(now - nextWifiRetryAt) >= 0) {
    Serial.println("Retrying WiFi");
    startWifiConnectAttempt();
  }
  if (!wifiOnline && wifiStateChanged) {
    garageIp = IPAddress();
    garageReady = false;
  }

  uint32_t remainingMs = (nextWifiRetryAt > now) ? (nextWifiRetryAt - now) : 0;
  int32_t retrySeconds = (remainingMs + 999) / 1000;

  if (wifiInitialConnect && (!barDrawn || wifiStateChanged)) {
    drawWifiConnectingBar();
    barDrawn = true;
  } else if (!wifiOnline && (!barDrawn || wifiStateChanged || retrySeconds != lastRetrySeconds)) {
    drawWifiOfflineBar();
    barDrawn = true;
  } else if (wifiOnline && !garageReady && (!barDrawn || wifiStateChanged)) {
    clearStatusBar();
    barDrawn = true;
  } else if (wifiOnline && garageReady && (!barDrawn || wifiStateChanged || garageReadyChanged || doorStateChanged || doorNameChanged)) {
    if (doorStateChanged) {
      if (!blOn) {
        blOn = true;
        doorWakeActive = true;
        ledcWrite(BL_CHANNEL, BL_LEVEL);
      }
      beep();
    }
    drawDoorBar();
    doors[0].lastOpen = doors[0].open;
    doors[1].lastOpen = doors[1].open;
    doors[0].lastName = doors[0].name;
    doors[1].lastName = doors[1].name;
    barDrawn = true;
  }

  if (doorWakeActive && !doors[0].open && !doors[1].open) {
    if (blOn) {
      blOn = false;
      ledcWrite(BL_CHANNEL, 0);
    }
    doorWakeActive = false;
  }

  lastWifiOnline = wifiOnline;
  lastGarageReady = garageReady;
  lastRetrySeconds = retrySeconds;

  // sensor sampling
  static uint32_t lastSample = 0;
  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;
    objBuf[bufIdx] = mlx.readObjectTempF();
    bufIdx++;
    if (bufIdx >= WIN) { bufIdx = 0; bufFull = true; }
  }

  // display refresh
  static uint32_t lastDraw = 0;
  if (now - lastDraw >= DRAW_MS) {
    lastDraw = now;
    int n = bufFull ? WIN : bufIdx;
    if (n > 0) {
      float obj = trimmedMean(objBuf, n);
      if (fabs(obj - lastObj) >= 0.1f) { drawObjectTemp(obj); lastObj = obj; }
    }
  }
}
