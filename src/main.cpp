#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <TFT_eSPI.h>
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
#define GARAGE_MS    2000    // poll interval

// --- hardware pins ---
#define BL_PIN      21
#define BL_CHANNEL  0

// --- settings ---
#define BL_LEVEL     60
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

TFT_eSPI          tft;
TFT_eSprite       spr(&tft);
Adafruit_MLX90614 mlx;

volatile bool garageReady = false;
float objBuf[WIN];
int   bufIdx  = 0;
bool  bufFull = false;
float lastObj = -999;

struct Door {
  bool   open;
  bool   lastOpen;
  String name;
};
Door doors[2] = {
  { false, false, "Door 1" },
  { false, false, "Door 2" },
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
      doors[0] = { DEBUG_DOOR1_OPEN, doors[0].lastOpen, "Joe's door" };
      doors[1] = { DEBUG_DOOR2_OPEN, doors[1].lastOpen, "Elaine's door" };
    } else if (WiFi.status() == WL_CONNECTED) {
      IPAddress ip = MDNS.queryHost(GARAGE_HOST);
      if (ip == IPAddress((uint32_t)0)) { vTaskDelay(pdMS_TO_TICKS(GARAGE_MS)); continue; }
      HTTPClient http;
      http.begin("http://" + ip.toString() + GARAGE_PATH);
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
      }
      http.end();
    }
    vTaskDelay(pdMS_TO_TICKS(GARAGE_MS));
  }
}

// --- setup ---

void setup() {
  Serial.begin(115200);

  ledcSetup(BL_CHANNEL, 5000, 8);
  ledcAttachPin(BL_PIN, BL_CHANNEL);
  ledcWrite(BL_CHANNEL, BL_LEVEL);

  Wire.begin(22, 27);
  delay(250);
  bool sensorOk = mlx.begin();

  tft.init();
  tft.setRotation(3);
  spr.createSprite(320, SPR_H_PX);

  drawBackground();

  if (!sensorOk) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, BG);
    tft.drawString("SENSOR NOT FOUND", 160, 120, 4);
    Serial.println("MLX90614 not found");
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");
  MDNS.begin("cyd");

  xTaskCreatePinnedToCore(garageTask, "garage", 8192, nullptr, 1, nullptr, 0);
}

// --- loop ---

void loop() {
  uint32_t now = millis();

  // redraw bar once garage data is available, then on state change
  static bool barDrawn = false;
  if (garageReady && (!barDrawn || doors[0].open != doors[0].lastOpen || doors[1].open != doors[1].lastOpen)) {
    drawDoorBar();
    doors[0].lastOpen = doors[0].open;
    doors[1].lastOpen = doors[1].open;
    barDrawn = true;
  }

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
