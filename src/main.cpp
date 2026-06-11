#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

void setup() {
  Serial.begin(115200);

  pinMode(21, OUTPUT);        // backlight
  digitalWrite(21, HIGH);

  Wire.begin(22, 27);         // SDA=22, SCL=27
  if (!mlx.begin()) Serial.println("MLX90614 not found");

  tft.init();
  tft.setRotation(1);         // landscape 320x240
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
}

void loop() {
  float obj = mlx.readObjectTempF();
  float amb = mlx.readAmbientTempF();

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Object", 160, 50, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(obj, 1) + " F   ", 160, 110, 6);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Ambient " + String(amb, 1) + " F   ", 160, 190, 4);

  delay(500);
}
