# KNOMI Creality Display

Read-only Creality K-series display firmware for an ESP32-S3-Zero and 240 x 240 GC9A01 panel.

## Current Checkpoint

The legacy Moonraker/touch/sensor/control application has been removed. The root ESP-IDF project currently builds a **display-only boot image**: RGB bars, a boot caption and uptime, with heap logs over native USB. It checks for 4 MiB flash and 2 MiB initialized PSRAM. Wi-Fi setup, web portal, printer connection, settings and production screens are not implemented in this image.

The earlier network-loaded display spike remains under `spikes/display`. Its hardware acceptance is separate from this new image, which has **not been flashed or physically validated**.

## Hardware

| Signal | GPIO |
|---|---|
| GC9A01 SCK | 1 |
| MOSI | 2 |
| DC | 3 |
| CS | 4 |
| Reset | 5 |
| BOOT button | 0 |

No MISO, touch, sensors or software backlight control. Native USB Serial/JTAG uses GPIO19/20. Display SPI is 40 MHz; LVGL uses PSRAM with two internal 240 x 24 DMA strips. GPIO0/GPIO3 are strapping pins; do not hold BOOT during normal power-on.

## Build

Use an activated **ESP-IDF 5.5.5** shell. The component manifest pins Arduino-ESP32 **3.3.12** and LVGL **8.4.0**; the root registry lock records transitive dependencies. PlatformIO is no longer the firmware build system.

From the repository root in PowerShell:

```powershell
$build = Join-Path $PWD 'build/esp32s3'
idf.py -B $build -D IDF_TARGET=esp32s3 -D "SDKCONFIG=$build/sdkconfig" `
	-D SDKCONFIG_DEFAULTS=sdkconfig.defaults build
```

Output is under `build/esp32s3`; the application is `knomi_creality.bin`. The USB-only partition table has one factory app slot and no OTA/filesystem partition. SDK configuration, managed components, local VS Code settings and build output are ignored. Do not copy private spike settings or credentials into tracked files.

Flashing is a separate hardware action. After confirming the correct connected board/port and backup implications:

```powershell
idf.py -B build/esp32s3 -p COM8 flash monitor
```

No full-chip erase is required for the currently matching layout by default. Review migration implications before deliberately changing an existing device's partition layout or erasing its settings.

## Reference and Validation

- [docs/SCREENS.md](docs/SCREENS.md): original KNOMI visual guide, native reference gallery and thermal-red preservation rules.
- [tools/screen_reference/README.md](tools/screen_reference/README.md): reproducible host renderer and verified source archive; no printer commands.
- [tools/fonts.md](tools/fonts.md): pinned fonts, generation commands and asset checks.
- [test/README](test/README): current Python reference tests and capture baselines.
- [CREALITY_WEBSOCKET_PLAN.md](CREALITY_WEBSOCKET_PLAN.md), [CREALITY_WEBSOCKET_IMPL.md](CREALITY_WEBSOCKET_IMPL.md): scope, implementation evidence and pending hardware checks.

This checkpoint builds at 440,432 bytes (89% app-partition headroom). That is not the final network/UI firmware size. Asset licensing and original upstream restrictions remain described in the plan; retaining a reference is not a new distribution license.
# KNOMI1 and 2
