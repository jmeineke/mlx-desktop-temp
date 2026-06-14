# MLX Desktop Temp

IR temperature readout on a CYD (Cheap Yellow Display). Reads an MLX90614-DCI
non-contact IR sensor over I2C and shows the object temperature in °F on the
display.

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
- **White/blank screen:** the backlight on GPIO21 must be enabled in code (this
  project uses LEDC PWM); also check the display rotation.
- **Ghosting text:** always pass a bg color to `setTextColor(fg, bg)`.

## Garage Door Alerts

Polls `http://garage.local/status` (a local-network endpoint, resolved via mDNS)
every 2 s over WiFi. The poll runs in a FreeRTOS task pinned to core 0 so the
blocking HTTP request never stalls the temperature display on core 1.

When a door is open, a thin red bar appears at the top of the screen showing the
door name in white text. When both doors are open, the bar splits into two
side-by-side halves, one per door. The bar disappears when both doors are closed.

Door names come from the JSON payload (`single.name` / `double.name`), so the
labels follow whatever the garage endpoint reports. The expected shape is:

```json
{"single":{"name":"Joe's door","open":false},
 "double":{"name":"Elaine's door","open":false}}
```

WiFi credentials and the poll URL are set via `#define` at the top of
[src/main.cpp](src/main.cpp). Set `DEBUG_DOOR1_OPEN` / `DEBUG_DOOR2_OPEN` to `true`
to force the bar on for testing without opening a real door.

## Firmware

[src/main.cpp](src/main.cpp) — initializes the sensor, display, and touch, connects
to WiFi, and starts the garage-polling task. The main loop reads object/ambient
temperature in °F and updates the UI only when a value changes.

UI: a large object temperature (TFT_eSPI font 8) with a degree symbol, rendered via
a `TFT_eSprite` for flicker-free updates. Requires `LOAD_FONT8` in the build flags
for the large digits (`LOAD_FONT2` / `LOAD_FONT4` cover the smaller labels).

The backlight (ESP32 LEDC PWM on GPIO21) only turns on when there's something
worth showing: either a garage door is open, or the object temperature is above
`TEMP_ON` (85 °F). It turns back off once both doors are closed and the temperature
drops below `TEMP_OFF` (84 °F) — the 1 °F gap is hysteresis to prevent flicker near
the threshold. When on, brightness is `BL_LEVEL`.

Readings are smoothed with a rolling **trimmed mean**: the sensor is sampled fast
(`SAMPLE_MS`, 50 ms) into a ring buffer (`WIN`, 10 samples = 0.5 s window); each
refresh (`DRAW_MS`, 150 ms) drops the min+max sample and averages the rest. Fast
sampling keeps it responsive while the window + trim keep readings flat and
spike-free. Widen `WIN` for flatter/slower, shrink for snappier.
