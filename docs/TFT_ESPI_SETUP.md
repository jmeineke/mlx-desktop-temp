# TFT_eSPI Setup for ESP32-2432S028R (CYD)

## Key Insight: No User_Setup.h Editing Required

All TFT_eSPI config is injected via `platformio.ini` build flags using `-DUSER_SETUP_LOADED=1`. This tells the library to skip `User_Setup.h` entirely. Do not edit `User_Setup.h` in the library directory.

## Hardware: Two Separate SPI Buses

The CYD uses two SPI buses — do not mix them up:

| Bus | Used For | Pins |
|-----|----------|------|
| HSPI | TFT display (ILI9341) | MOSI=13, MISO=12, SCLK=14, CS=15, DC=2 |
| VSPI | Touch controller (XPT2046) | MOSI=32, MISO=39, SCLK=25, CS=33 |

Other display pins:
- RST: `-1` (tied to ESP32 reset, not a GPIO)
- Backlight: GPIO 21 — must be driven HIGH explicitly in code

## platformio.ini Build Flags

```ini
build_flags =
    -DUSER_SETUP_LOADED=1
    -DILI9341_DRIVER=1
    -DTFT_WIDTH=240
    -DTFT_HEIGHT=320
    -DTFT_MOSI=13
    -DTFT_MISO=12
    -DTFT_SCLK=14
    -DTFT_CS=15
    -DTFT_DC=2
    -DTFT_RST=-1
    -DTFT_BL=21
    -DTOUCH_CS=33
    -DLOAD_GLCD=1
    -DLOAD_FONT2=1
    -DLOAD_GFXFF=1
    -DSMOOTH_FONT=1
    -DSPI_FREQUENCY=55000000
    -DSPI_TOUCH_FREQUENCY=2500000
```

## Display Initialization

```cpp
// Backlight MUST be enabled before or right after TFT init
pinMode(21, OUTPUT);
digitalWrite(21, HIGH);

TFT_eSPI tft;
tft.init();
tft.setRotation(1);  // landscape (320x240)
tft.fillScreen(TFT_BLACK);
```

`setRotation(1)` = landscape, 320px wide × 240px tall.

## Touch Initialization

XPT2046 is on VSPI. Must call `SPI.begin()` with explicit pins before `ts.begin()`:

```cpp
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

#define TOUCH_CS  33
#define TOUCH_CLK 25
#define TOUCH_MISO 39
#define TOUCH_MOSI 32

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS);

// In setup():
touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
ts.begin(touchSPI);
ts.setRotation(1);
```

Do not pass the SPI instance to the XPT2046 constructor — pass it to `begin()`.

## Font Usage

Built-in fonts (no external header needed):

```cpp
tft.setTextFont(1);  // 8px — small labels
tft.setTextFont(2);  // 16px — readable body text
tft.setTextFont(4);  // 26px — headings
```

Enable in build flags: `-DLOAD_FONT2=1`, `-DLOAD_GLCD=1`, etc.

For GFX free fonts (requires `-DLOAD_GFXFF=1`):
```cpp
#include <Fonts/FreeSans9pt7b.h>
tft.setFreeFont(&FreeSans9pt7b);
```

**Do not mix Adafruit GFX library with TFT_eSPI** — both define `FreeSans9pt7b` and will cause a linker redefinition error. Remove Adafruit GFX from `lib_deps` if present.

## Text Rendering Without Ghosting

Always pass a background color to `setTextColor()`:

```cpp
tft.setTextColor(TFT_WHITE, TFT_BLACK);  // fg, bg — erases old text
```

Without the background color, old characters bleed through on updates.

## lib_deps

```ini
lib_deps =
    Bodmer/TFT_eSPI
    paulstoffregen/XPT2046_Touchscreen
    me-no-dev/ESPAsyncWebServer
    ArduinoJson
    tzapu/WiFiManager
    ricmoo/qrcode
```

## Common Mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Wrong SPI bus for TFT (VSPI instead of HSPI) | Blank display | Use MOSI=13, SCLK=14, CS=15 |
| Backlight not driven HIGH | Blank display (backlit black) | `pinMode(21,OUTPUT); digitalWrite(21,HIGH)` |
| RST set to 4 instead of -1 | May not init properly | Set `TFT_RST=-1` |
| Adafruit GFX in lib_deps | `FreeSans9pt7b` redefinition error | Remove Adafruit GFX |
| Calling `ts.begin()` without SPI.begin() first | Touch not working | Call `SPI.begin(pins)` first |
| `setTextColor(fg)` without bg | Text ghosts on update | Always pass bg color |

## References

- [TFT_eSPI GitHub](https://github.com/Bodmer/TFT_eSPI)
- [CYD pin reference](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)
