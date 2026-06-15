# MLX Desktop Temp

CYD firmware for an ESP32-2432S028R "Cheap Yellow Display". It shows an
MLX90614 object temperature reading, polls the garage monitor, and uses the top
status bar for WiFi, garage connectivity, and door state.

## Hardware

- Board: ESP32-2432S028R CYD, 2.4 inch 320x240 ILI9341 TFT, ESP32, CH340 USB serial
- Sensor: MLX90614-DCI on a GY-906-DCI breakout
- Speaker: small 8 ohm speaker on the CYD speaker connector

### Sensor Wiring

CYD `CN1` silkscreen: `GND | IO22 | IO27 | 3.3V`

| MLX90614 | CYD CN1 |
|----------|---------|
| VDD/VIN  | 3.3V   |
| VSS/GND  | GND    |
| SDA      | IO22   |
| SCL      | IO27   |

Notes:

- Use 3.3V, not VIN.
- GY-906 has onboard pullups and regulator, so no extra resistors are needed.
- `CN1` is JST 1.25 mm. Qwiic SH1.0 cables do not fit.

## Build And Flash

Use VSCode with the PlatformIO extension. Open this folder, then run PlatformIO
Build and Upload.

- Driver: CH340. Install it if no COM port appears.
- First build is slow because PlatformIO downloads the toolchain and libraries.
- TFT_eSPI is configured through `platformio.ini` build flags. Do not edit
  `User_Setup.h`. See `docs/TFT_ESPI_SETUP.md`.

## Configuration

Runtime settings live in `src/app_config.h`.

Important values:

- `WIFI_SSID` / `WIFI_PASS`: WiFi credentials.
- `GARAGE_HOST` / `GARAGE_PATH`: garage monitor endpoint, currently `garage.local/status`.
- `GARAGE_MS`: garage poll interval, currently 5 seconds.
- `GARAGE_HTTP_TIMEOUT_MS`: HTTP timeout, currently 4.9 seconds.
- `GARAGE_FAILURES_BEFORE_OFFLINE`: consecutive failed polls before red garage offline.
- `BL_LEVEL`: backlight PWM brightness when on.
- `SCREEN_ROTATION`: display/touch rotation. `0` and `2` are portrait, `1` and `3` are landscape.

## Firmware Layout

- `src/main.cpp`: setup, main loop, status-bar decisions, backlight wake/auto-off logic.
- `src/app_config.h`: typed configuration constants.
- `src/backlight_sound.*`: backlight PWM and non-blocking sound sequences.
- `src/display.*`: TFT drawing functions.
- `src/garage_status.*`: garage polling task, cached IP, failure debounce, locked status snapshots.
- `src/temperature_sensor.*`: MLX90614 sampling, trimmed mean, temperature drawing.
- `src/touch_input.*`: touch read/debounce for manual backlight toggle.
- `src/wifi_manager.*`: WiFi connect/retry, mDNS startup, WiFi debug logging.

## Garage Status Behavior

The garage task polls `http://garage.local/status` over WiFi. It resolves the
host with mDNS, caches the resolved IP, and keeps that cached IP through HTTP
timeout failures.

Expected JSON:

```json
{
  "single": { "name": "Joe's door", "open": false },
  "double": { "name": "Elaine's door", "open": false }
}
```

Startup mDNS failures stay in `CONNECTING TO GARAGE`. After the first successful
status read, transient failures keep showing the last known door status. Red
`GARAGE OFFLINE` appears only after the configured consecutive failure threshold.

Any garage data change wakes the display if the backlight is off and plays the
garage chime. The first successful status read establishes the baseline and does
not chime.

## Backlight Behavior

Touch toggles the backlight manually.

If a garage data change wakes the display while it was off, that wake is tracked
as door-triggered. When both doors later report closed, the backlight turns off
automatically. If the user had already turned the backlight on manually, the
door-triggered auto-off path does not take over.

## Temperature Display

The MLX90614 object temperature is sampled every `SAMPLE_MS` and drawn every
`DRAW_MS` when the displayed value changes enough to matter. Readings use a
trimmed mean over `WIN` samples to reduce spikes while staying responsive.

Temperature color thresholds are configured in `src/app_config.h`:

- Below `TEMP_COLOR_GREEN_MIN_F`: blue
- From `TEMP_COLOR_GREEN_MIN_F` up to `TEMP_COLOR_RED_MIN_F`: green
- Above `TEMP_COLOR_RED_MIN_F`: red

## Debugging

Serial logs include WiFi status transitions, mDNS startup, garage resolve
results, HTTP failures, garage state transitions, and garage poll failure counts.

Useful symptoms:

- `mDNS queryHost failed, no cached IP`: discovery has not resolved `garage.local`.
- `HTTP GET failed ... code=-11`: HTTP read timeout. Cached IP is retained.
- `Poll failure X/3`: garage failure debounce counter before red offline.
