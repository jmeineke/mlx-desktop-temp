#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite num = TFT_eSprite(&tft);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

#define TOUCH_CS   33
#define TOUCH_CLK  25
#define TOUCH_MISO 39
#define TOUCH_MOSI 32
XPT2046_Touchscreen ts(TOUCH_CS);

// --- backlight PWM ---
#define BL_PIN      21
#define BL_CHANNEL  0
#define BL_BRIGHT   255
#define BL_DIM      10          // dim amount (0-255)
#define DIM_TIMEOUT 15000       // ms of no touch before dim

uint32_t lastTouch = 0;
bool isDimmed = false;

void setBrightness(uint8_t v) { ledcWrite(BL_CHANNEL, v); }

// --- palette (RGB565) ---
#define BG      0x0861   // near-black navy
#define PANEL   0x18E3   // dark grey panel
#define DIM     0x8C71   // muted grey text

// big-number sprite geometry
#define SPR_W   320
#define SPR_H   140
#define SPR_X   0
#define SPR_Y   24

// rolling-average filter
#define WIN        20         // samples in window (20 * 50ms = 1s)
#define SAMPLE_MS  50         // sensor sample period
#define DRAW_MS    250        // display refresh period

float bufObj[WIN], bufAmb[WIN];
int   bIdx = 0;
bool  bFull = false;

float lastObj = -999, lastAmb = -999; // last drawn

// trimmed mean: drop the single lowest + highest sample, average the rest
float trimmedMean(const float *b, int n) {
  if (n <= 0) return NAN;
  if (n < 4) {                         // too few to trim, plain mean
    float s = 0; for (int i = 0; i < n; i++) s += b[i];
    return s / n;
  }
  float mn = b[0], mx = b[0], sum = 0;
  for (int i = 0; i < n; i++) {
    sum += b[i];
    if (b[i] < mn) mn = b[i];
    if (b[i] > mx) mx = b[i];
  }
  return (sum - mn - mx) / (n - 2);
}


void drawStatic() {
  tft.fillScreen(BG);

  // ambient panel shell
  tft.fillRoundRect(20, 186, 280, 44, 10, PANEL);
}

void drawObject(float f) {
  num.fillSprite(BG);
  String s = String(f, 1);
  // pick largest size where everything fits in sprite width
  int sz = 2;
  num.setTextSize(sz);
  int gap = sz * 14;
  int wn = num.textWidth(s, 7);
  int wf = num.textWidth("F", 4);
  if (wn + gap + wf + 20 > SPR_W) {
    sz = 1; gap = 14;
    num.setTextSize(sz);
    wn = num.textWidth(s, 7);
    wf = num.textWidth("F", 4);
  }

  int total = wn + gap + wf;
  int x = (SPR_W - total) / 2;
  int yTop = (SPR_H - (sz == 2 ? 96 : 48)) / 2;

  num.setTextDatum(TL_DATUM);
  num.setTextColor(TFT_WHITE, BG);
  num.drawString(s, x, yTop, 7);

  num.setTextSize(1);
  num.setTextColor(TFT_WHITE, BG);
  num.drawString("F", x + wn + gap, yTop + (sz == 2 ? 48 : 16), 4);
  int rSz = sz == 2 ? 8 : 5;
  int dx = x + wn + (sz == 2 ? 14 : 9);
  int dy = yTop + (sz == 2 ? 16 : 10);
  num.fillCircle(dx, dy, rSz, TFT_WHITE);
  num.fillCircle(dx, dy, rSz - 3, BG);

  num.pushSprite(SPR_X, SPR_Y);
}

void drawAmbient(float f) {
  tft.fillRoundRect(20, 186, 280, 44, 10, PANEL);

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(DIM, PANEL);
  tft.drawString("AMBIENT", 40, 208, 4);

  // right-aligned value with degree symbol, using cursor advance
  String s = String(f, 1);
  int wv = tft.textWidth(s, 4);
  int wf = tft.textWidth("F", 4);
  int xEnd = 280;                        // right inset
  int x = xEnd - (wv + 16 + wf);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, PANEL);
  tft.drawString(s, x, 208, 4);
  tft.fillCircle(x + wv + 6, 200, 3, TFT_WHITE);
  tft.fillCircle(x + wv + 6, 200, 1, PANEL);
  tft.drawString("F", x + wv + 14, 208, 4);
}

void setup() {
  Serial.begin(115200);

  ledcSetup(BL_CHANNEL, 5000, 8);   // channel, freq, resolution
  ledcAttachPin(BL_PIN, BL_CHANNEL);
  setBrightness(BL_BRIGHT);
  lastTouch = millis();

  Wire.begin(22, 27);           // SDA=22, SCL=27
  delay(250);                   // sensor needs time to wake after power-on
  bool ok = mlx.begin();

  tft.init();                   // TFT first — it grabs the VSPI port
  tft.setRotation(1);           // landscape 320x240
  num.createSprite(SPR_W, SPR_H);

  // touch SPI last, so its VSPI pin config survives
  SPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin();
  ts.setRotation(1);

  drawStatic();

  if (!ok) {
    Serial.println("MLX90614 not found");
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(0xF800, BG);
    tft.drawString("SENSOR NOT FOUND", 160, 120, 4);
  }
}

void loop() {
  uint32_t now = millis();

  // touch → wake / reset dim timer (Z threshold filters ghost touches)
  #define TOUCH_Z_MIN 100
  static uint32_t tTouch = 0;
  bool touched = false;
  if (now - tTouch >= 50) {
    tTouch = now;
    touched = (ts.getPoint().z > TOUCH_Z_MIN);
  }
  if (touched) {
    lastTouch = now;
    if (isDimmed) { setBrightness(BL_BRIGHT); isDimmed = false; }
  } else if (!isDimmed && (now - lastTouch >= DIM_TIMEOUT)) {
    setBrightness(BL_DIM);
    isDimmed = true;
  }

  static uint32_t tSample = 0, tDraw = 0;

  // fast sampling into ring buffer
  if (now - tSample >= SAMPLE_MS) {
    tSample = now;
    bufObj[bIdx] = mlx.readObjectTempF();
    bufAmb[bIdx] = mlx.readAmbientTempF();
    bIdx++;
    if (bIdx >= WIN) { bIdx = 0; bFull = true; }
  }

  // slower display refresh from the averaged window
  if (now - tDraw >= DRAW_MS) {
    tDraw = now;
    int n = bFull ? WIN : bIdx;
    if (n > 0) {
      float obj = trimmedMean(bufObj, n);
      float amb = trimmedMean(bufAmb, n);
      if (fabs(obj - lastObj) >= 0.1f) { drawObject(obj); lastObj = obj; }
      if (fabs(amb - lastAmb) >= 0.1f) { drawAmbient(amb); lastAmb = amb; }
    }
  }
}
