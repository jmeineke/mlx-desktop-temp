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
#define GARAGE_URL   "http://garage.local/status"
#define GARAGE_MS    2000    // poll interval

// --- hardware pins ---
#define BL_PIN      21
#define BL_CHANNEL  0

// --- settings ---
#define BL_LEVEL     60          // backlight brightness when on
#define TEMP_ON      85.0f       // object temp (F) that turns the display on
#define TEMP_OFF     84.0f       // ...and the lower bound that turns it back off (hysteresis)
#define SAMPLE_MS    50
#define DRAW_MS      250
#define WIN          20

// --- colors (RGB565) ---
#define BG    0x0861
#define PANEL 0x18E3
#define MUTED 0x8C71

// --- door alert bar geometry ---
#define BAR_H   20    // height of alert bar at top of screen
#define BAR_Y   0
#define SPR_Y   (BAR_H + 4)   // temp sprite starts below bar

TFT_eSPI          tft;
TFT_eSprite       spr(&tft);
Adafruit_MLX90614 mlx;

float objBuf[WIN], ambBuf[WIN];
int   bufIdx  = 0;
bool  bufFull = false;
float lastObj = -999;
float lastAmb = -999;

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
  tft.fillRoundRect(20, 186, 280, 44, 10, PANEL);
}

// Draw the door alert bar at the top. Called whenever door state changes.
void drawDoorBar() {
  tft.fillRect(0, BAR_Y, 320, BAR_H, BG);  // clear bar area

  if (!doors[0].open && !doors[1].open) return;

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_RED);

  if (doors[0].open && doors[1].open) {
    tft.fillRect(0,   BAR_Y, 157, BAR_H, TFT_RED);
    tft.fillRect(163, BAR_Y, 157, BAR_H, TFT_RED);
    tft.drawString(doors[0].name, 80,  BAR_Y + BAR_H / 2, 2);
    tft.drawString(doors[1].name, 240, BAR_Y + BAR_H / 2, 2);
  } else {
    const String &name = doors[0].open ? doors[0].name : doors[1].name;
    tft.fillRect(0, BAR_Y, 320, BAR_H, TFT_RED);
    tft.drawString(name, 160, BAR_Y + BAR_H / 2, 2);
  }
}

void drawObjectTemp(float f) {
  spr.fillSprite(BG);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TFT_WHITE, BG);

  String val  = String(f, 1);
  int    numW = spr.textWidth(val, 8);
  int    unitW= spr.textWidth("F", 4);
  int    gap  = 16;
  int    x    = (320 - (numW + gap + unitW)) / 2;
  int    y    = (140 - 75) / 2;

  spr.drawString(val, x, y, 8);
  spr.drawString("F", x + numW + gap, y + 30, 4);
  spr.fillCircle(x + numW + 10, y + 22, 6, TFT_WHITE);
  spr.fillCircle(x + numW + 10, y + 22, 3, BG);

  spr.pushSprite(0, SPR_Y);
}

void drawAmbientTemp(float f) {
  tft.fillRoundRect(20, 186, 280, 44, 10, PANEL);
  tft.setTextDatum(ML_DATUM);

  tft.setTextColor(MUTED, PANEL);
  tft.drawString("AMBIENT", 40, 208, 4);

  String val  = String(f, 1);
  int    valW = tft.textWidth(val, 4);
  int    unitW= tft.textWidth("F", 4);
  int    x    = 280 - (valW + 16 + unitW);

  tft.setTextColor(TFT_WHITE, PANEL);
  tft.drawString(val, x, 208, 4);
  tft.fillCircle(x + valW + 6, 200, 3, TFT_WHITE);
  tft.fillCircle(x + valW + 6, 200, 1, PANEL);
  tft.drawString("F", x + valW + 14, 208, 4);
}

// --- garage polling (runs on core 0, main loop on core 1) ---

void garageTask(void *) {
  for (;;) {
    if (DEBUG_DOOR1_OPEN || DEBUG_DOOR2_OPEN) {
      doors[0] = { DEBUG_DOOR1_OPEN, doors[0].lastOpen, "Joe's door" };
      doors[1] = { DEBUG_DOOR2_OPEN, doors[1].lastOpen, "Elaine's door" };
    } else if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(GARAGE_URL);
      int code = http.GET();
      if (code == 200) {
        JsonDocument doc;
        if (!deserializeJson(doc, http.getString())) {
          doors[0].open = doc["single"]["open"].as<bool>();
          doors[1].open = doc["double"]["open"].as<bool>();
          doors[0].name = doc["single"]["name"] | "Door 1";
          doors[1].name = doc["double"]["name"] | "Door 2";
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
  ledcWrite(BL_CHANNEL, 0);      // start off; loop turns it on when a door opens or temp is high

  Wire.begin(22, 27);
  delay(250);
  bool sensorOk = mlx.begin();

  tft.init();
  tft.setRotation(1);
  spr.createSprite(320, 140);

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

  // redraw bar if door state changed (updated by garageTask on core 0)
  if (doors[0].open != doors[0].lastOpen || doors[1].open != doors[1].lastOpen) {
    drawDoorBar();
    doors[0].lastOpen = doors[0].open;
    doors[1].lastOpen = doors[1].open;
  }

  // backlight: on when a door is open or object temp is high (hysteresis on temp)
  static bool blOn = false;
  bool tempHot = blOn ? (lastObj > TEMP_OFF) : (lastObj > TEMP_ON);
  bool wantOn  = doors[0].open || doors[1].open || tempHot;
  if (wantOn != blOn) {
    ledcWrite(BL_CHANNEL, wantOn ? BL_LEVEL : 0);
    blOn = wantOn;
  }

  // sensor sampling
  static uint32_t lastSample = 0;
  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;
    objBuf[bufIdx] = mlx.readObjectTempF();
    ambBuf[bufIdx] = mlx.readAmbientTempF();
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
      float amb = trimmedMean(ambBuf, n);
      if (fabs(obj - lastObj) >= 0.1f) { drawObjectTemp(obj); lastObj = obj; }
      if (fabs(amb - lastAmb) >= 0.1f) { drawAmbientTemp(amb); lastAmb = amb; }
    }
  }
}
