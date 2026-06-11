# MLX Desktop Temp

IR temperature readout on a CYD (Cheap Yellow Display). Reads an MLX90614-DCI
non-contact IR sensor over I2C and shows object + ambient temperature in °F on
the display.

## Hardware

- **Board:** ESP32-2432S028R "CYD" — 2.4" 320×240 ILI9341 TFT, ESP32, CH340 USB-serial
- **Sensor:** MLX90614-DCI (GY-906-DCI breakout, 4-pin, narrow ~5° field of view)

### Wiring — sensor to CYD `CN1` connector (JST 1.25mm 4-pin)

`CN1` silkscreen: `GND | IO22 | IO27 | 3.3V`

| MLX90614 | CYD CN1 |
|----------|---------|
| VDD/VIN  | 3.3V    |
| VSS/GND  | GND     |
| SDA      | IO22    |
| SCL      | IO27    |

Notes:
- Use **3.3V**, not VIN (5V). Sensor is a 3.3V part.
- GY-906 has onboard pullups + regulator — no extra resistors.
- VSS = ground. Do not put 3.3V on VSS (reversed power kills the sensor).
- `CN1` is JST 1.25mm. Qwiic SH1.0 cables do **not** fit (wrong pitch + wrong pin order).

## Build & Flash

VSCode + PlatformIO IDE extension. Open folder, Build (✓), Upload (→).

- Driver: CH340. Install if no COM port appears.
- First build is slow (pulls toolchain + libs).

### Config notes ([platformio.ini](platformio.ini))

- TFT_eSPI is configured entirely via `build_flags` — do **not** edit `User_Setup.h`.
  See [docs/TFT_ESPI_SETUP.md](docs/TFT_ESPI_SETUP.md).
- `platform` pinned to `espressif32@6.13.0`.

## Gotchas (hit during setup)

- **`No serial data received` on upload:** check `pio device list`. Multiple CH340
  devices (e.g. a Baofeng radio programming cable) confuse auto-detect — unplug the
  other one or set `upload_port` explicitly.
- **`ModuleNotFoundError: No module named 'intelhex'`:** PlatformIO penv was broken.
  Fix: `C:/Python313/python.exe -m pip install intelhex`.
- **White/blank screen:** backlight GPIO21 must be driven HIGH in code; check rotation.
- **Ghosting text:** always pass a bg color to `setTextColor(fg, bg)`.

## Firmware

[src/main.cpp](src/main.cpp) — init sensor + display, then loop reads object/ambient
temp in °F (every 250 ms) and updates the UI only when a value changes. Display-only
(no touch).

UI: large 7-segment object temperature that color-codes by value
(blue <60 / green <85 / orange <99 / red ≥99 °F), degree symbols, and an ambient
panel. The big number is rendered via a `TFT_eSprite` for flicker-free updates.
Requires `LOAD_FONT7` in build flags for the 7-segment font.

Backlight dims to ~8% (`BL_DIM=20`) after `DIM_TIMEOUT` ms (default 15 s) of no
touch. Any touch restores full brightness. Uses ESP32 LEDC PWM on GPIO21.
Touch uses XPT2046 on VSPI (CLK=25, MISO=39, MOSI=32, CS=33) — wake only.
A touch is counted when XPT2046 Z pressure exceeds `TOUCH_Z_MIN` (100); below
that is electrical noise.

**Init order matters:** TFT_eSPI defaults to the VSPI port, so `tft.init()` must
run *before* the touch `SPI.begin()/ts.begin()`. Init touch last or `tft.init()`
clobbers the touch pin config — symptom: touch Z stuck at 4095.

Readings are smoothed with a rolling **trimmed mean**: the sensor is sampled fast
(`SAMPLE_MS`, 50 ms) into a ring buffer (`WIN`, 20 samples = 1 s window); each
refresh (`DRAW_MS`, 250 ms) drops the min+max sample and averages the rest. Fast
sampling keeps it responsive while the window + trim keep readings flat and
spike-free. Widen `WIN` for flatter/slower, shrink for snappier.
