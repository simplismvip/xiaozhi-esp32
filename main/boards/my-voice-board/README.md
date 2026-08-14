# my-voice-board

Custom voice assistant board for xiaozhi-esp32.

## Hardware

- **MCU**: ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM)
- **Microphone**: INMP441 (I2S, full-duplex disabled, simplex mode)
- **Speaker**: MAX98357A (I2S DAC + amplifier)
- **Display (default)**: SPI ST7789 **portrait 240×320** on the carrier board (8-pin: GND VCC SCL SDA RST DC CS BLK)
- **Emotions (SPI LCD)**: `noto-color-emoji_32` assets (official fonts package)
- **Display (optional)**: I2C OLED (SSD1306 / SH1106) for breadboard prototyping
- **LED**: Single LED on GPIO 48 (status indicator)
- **Buttons**: BOOT (GPIO 2 on carrier), volume up/down (GPIO 38 / 39)

## Pin Assignments

### Audio

| Signal | ESP32-S3 GPIO | Notes |
|--------|--------------|-------|
| INMP441 SCK | GPIO 5 | |
| INMP441 WS | GPIO 4 | |
| INMP441 SD | GPIO 6 | |
| MAX98357A BCLK | GPIO 15 | |
| MAX98357A LRC | GPIO 16 | |
| MAX98357A DIN | GPIO 7 | |

### SPI LCD (carrier board — default)

| Signal | ESP32-S3 GPIO | Notes |
|--------|--------------|-------|
| MOSI (SDA) | GPIO 47 | |
| CLK (SCL) | GPIO 21 | |
| DC | GPIO 40 | |
| RST | GPIO 45 | |
| CS | GPIO 41 | |
| Backlight (BLK) | GPIO 42 | |

Panel: **ST7789 portrait 240×320** (e.g. 2.0" GMT020-02-8P). The SPI UI uses portrait orientation (240 px wide × 320 px tall).

Carrier header order (left → right): `GND VCC SCL SDA RST DC CS BLK`.

If colors/orientation look wrong, try **Non-IPS** in menuconfig (portrait 240×320 without color invert). If the panel appears rotated or mirrored after flash, adjust `DISPLAY_SWAP_XY` / `DISPLAY_MIRROR_*` in `config.h` only — do not change idle UI layout code.

### Buttons (carrier board)

| Signal | ESP32-S3 GPIO | Notes |
|--------|--------------|-------|
| BOOT | GPIO 2 | Active low |
| Volume up | GPIO 38 | |
| Volume down | GPIO 39 | |

### Other

| Signal | ESP32-S3 GPIO | Notes |
|--------|--------------|-------|
| LED | GPIO 48 | Status indicator |

### I2C OLED (breadboard — optional)

Select **my-voice-board Display → I2C OLED** in menuconfig, then choose SSD1306 or SH1106 under **OLED Type**.

| Signal | ESP32-S3 GPIO | Notes |
|--------|--------------|-------|
| OLED SDA | GPIO 41 | I2C data |
| OLED SCL | GPIO 42 | I2C clock |
| BOOT | GPIO 0 | Active low (breadboard wiring) |

## Build

```bash
idf.py set-target esp32s3
idf.py menuconfig
# Xiaozhi Assistant → Board Type → My Voice Board
# Default: my-voice-board Display → SPI LCD (ST7789 240x320)
# Breadboard OLED: my-voice-board Display → I2C OLED, then set OLED Type
idf.py build
idf.py -p /dev/cu.usbserial-XXXX flash monitor
```

## Idle home UI (portrait 240×320)

- Top bar: Wi-Fi + battery (existing)
- **Idle**: home panel with large clock, date, weather placeholders (`晴` / `0°C` / `湿度 0%`), and wake hint `说“小智”开始对话`
- **Chat**: home panel hidden; LVGL eye-emotion animator (not GIF) + subtitles shown
- Eye emotions map the 21 standard names onto 5 base animations (blink/sleep/happy/sad/angry); see `docs/superpowers/specs/2026-08-14-eye-emotion-animator-design.md`
- Backlight: 30s after entering idle → 20% (not written to NVS); leave idle restores saved brightness
- Live weather/TH: later via `Display::SetHomeEnvironment(...)` (custom HTTP); phase 1 is placeholders only

Idle home appearance (2026-08-14):
- Default theme is `dark` when NVS has no saved theme.
- Environment block: left-aligned, 30px side inset; 10px below date rule; line 1 weather + temp; line 2 humidity (future metrics one per line).
- Placeholders: `晴 0°C`, `湿度 0%`.
- Status bar and wake hint unchanged.


## LED Status

See `main/led/single_led.cc::OnStateChanged()` for state → LED mode mapping (built-in).

## Bluetooth speaker output

ESP32-S3 has no Classic Bluetooth, so A2DP cannot be added in firmware. To
stream TTS to a Bluetooth speaker, add an external A2DP-sink receiver module
(XY-WRBT / BK8000L / similar) that taps the analog output of the on-board
MAX98357A. No firmware changes required.

See [`bluetooth-speaker.md`](./bluetooth-speaker.md) for the wiring guide,
BOM, and troubleshooting.
