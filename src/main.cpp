#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// --- hardware pins ---
#define TOUCH_CS    33
#define TOUCH_CLK   25
#define TOUCH_MISO  39
#define TOUCH_MOSI  32
#define BL_PIN      21
#define BL_CHANNEL  0

// --- settings ---
#define BL_BRIGHT    255          // backlight full (0-255)
#define BL_DIM       10           // backlight dim (0-255)
#define DIM_TIMEOUT  15000        // ms idle before dim
#define TOUCH_Z_MIN  100          // minimum Z to count as a real touch
#define SAMPLE_MS    50           // sensor sample interval
#define DRAW_MS      250          // display refresh interval
#define WIN          20           // rolling average window size

// --- colors (RGB565) ---
#define BG    0x0861   // dark navy
#define PANEL 0x18E3   // dark grey
#define MUTED 0x8C71   // grey label text

TFT_eSPI           tft;
TFT_eSprite        spr(&tft);
Adafruit_MLX90614  mlx;
XPT2046_Touchscreen ts(TOUCH_CS);

float objBuf[WIN], ambBuf[WIN];
int   bufIdx  = 0;
bool  bufFull = false;

float lastObj = -999;
float lastAmb = -999;

uint32_t lastTouchTime = 0;
bool     dimmed        = false;

// --- helpers ---

void setBrightness(uint8_t v) {
  ledcWrite(BL_CHANNEL, v);
}

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

void drawObjectTemp(float f) {
  spr.fillSprite(BG);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TFT_WHITE, BG);

  String val = String(f, 1);
  int numW  = spr.textWidth(val, 8);
  int unitW = spr.textWidth("F", 4);
  int gap   = 16;
  int x     = (320 - (numW + gap + unitW)) / 2;
  int y     = (140 - 75) / 2;

  spr.drawString(val, x, y, 8);
  spr.drawString("F", x + numW + gap, y + 30, 4);
  spr.fillCircle(x + numW + 10, y + 22, 6, TFT_WHITE);
  spr.fillCircle(x + numW + 10, y + 22, 3, BG);

  spr.pushSprite(0, 24);
}

void drawAmbientTemp(float f) {
  tft.fillRoundRect(20, 186, 280, 44, 10, PANEL);
  tft.setTextDatum(ML_DATUM);

  tft.setTextColor(MUTED, PANEL);
  tft.drawString("AMBIENT", 40, 208, 4);

  String val = String(f, 1);
  int valW  = tft.textWidth(val, 4);
  int unitW = tft.textWidth("F", 4);
  int x     = 280 - (valW + 16 + unitW);

  tft.setTextColor(TFT_WHITE, PANEL);
  tft.drawString(val, x, 208, 4);
  tft.fillCircle(x + valW + 6, 200, 3, TFT_WHITE);
  tft.fillCircle(x + valW + 6, 200, 1, PANEL);
  tft.drawString("F", x + valW + 14, 208, 4);
}

// --- setup ---

void setup() {
  Serial.begin(115200);

  ledcSetup(BL_CHANNEL, 5000, 8);
  ledcAttachPin(BL_PIN, BL_CHANNEL);
  setBrightness(BL_BRIGHT);
  lastTouchTime = millis();

  Wire.begin(22, 27);
  delay(250);
  bool sensorOk = mlx.begin();

  tft.init();                    // TFT must init before touch — both share VSPI
  tft.setRotation(1);
  spr.createSprite(320, 140);

  SPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin();
  ts.setRotation(1);

  drawBackground();

  if (!sensorOk) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, BG);
    tft.drawString("SENSOR NOT FOUND", 160, 120, 4);
    Serial.println("MLX90614 not found");
  }
}

// --- loop ---

void loop() {
  uint32_t now = millis();

  // backlight dim / wake on touch
  static uint32_t lastTouchPoll = 0;
  if (now - lastTouchPoll >= 50) {
    lastTouchPoll = now;
    if (ts.getPoint().z > TOUCH_Z_MIN) {
      lastTouchTime = now;
      if (dimmed) { setBrightness(BL_BRIGHT); dimmed = false; }
    }
  }
  if (!dimmed && (now - lastTouchTime >= DIM_TIMEOUT)) {
    setBrightness(BL_DIM);
    dimmed = true;
  }

  // collect sensor samples into rolling buffer
  static uint32_t lastSample = 0;
  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;
    objBuf[bufIdx] = mlx.readObjectTempF();
    ambBuf[bufIdx] = mlx.readAmbientTempF();
    bufIdx++;
    if (bufIdx >= WIN) { bufIdx = 0; bufFull = true; }
  }

  // redraw display from averaged buffer
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
