# KNOMI → Creality K-series WebSocket display: rework plan

Status: **planning complete, ready for implementation** · Last update: 2026-10-19 (GIF set switched to the green `gif_*_green` variants and `gif_creality` replaces `gif_voron`; sizes re-measured, §3.3)

Related documents:

- [docs/CREALITY_WS_PROTOCOL.md](docs/CREALITY_WS_PROTOCOL.md): protocol reference and state-derivation rules (normative for Phase 4)
- Documents still to be written are listed in §12

---

## 1. Original requirements

* Change the project for Creality 3D printers
  * Keep only KNOMI v1 functionaly (remove v2 version)
  * Replace Moonraker API with Creality K‑series direct WebSocket connection. Example applications using this:
    * https://github.com/3dg1luk43/ha_creality_ws
    * https://github.com/hurricaneb/creality_k1_api
    * https://github.com/rathlinus/ha-creality-lan
    * https://github.com/DaviBe92/k2-websocket-re
  * Rework settings (WS IP address, port number) and display screens after setup
  * No control only information display
  * States: idle, processing, printing, paused, stopped, completed, error, self-testing
  * Entities: status, progress, time left, temperatures (nozzle/bed/chamber), current layer/total layers, etc.
  * Remove everything which is not required
  * Update platform and libraries which are kept if possible

* The project will run on a custom built hardware
  * ESP32 Wemos D1 mini (ESP32-WROOM-32)
  * 1.28 inch TFT round LCD 240x240 GC9A01 driver
  * Wiring:
    * TFT_MOSI 23
    * TFT_SCLK 18
    * TFT_CS 5
    * TFT_DC 26
    * TFT_RST 33
    * LCD back-light pin hard wired on

* Change partition table to fit into the 4MB flash

## 2. Clarified decisions (owner answers, 2026-10-15)

| Topic | Decision |
|---|---|
| Printer models | **K1, K1C, K1 Max, K2 / K2 Pro / K2 Plus**. All of them report a chamber temperature (`boxTemp`). |
| Firmware update | **USB only.** No OTA: drop the OTA library, upload script, and second app slot. |
| Visual style | **Mixed:** KNOMI GIF animations for setup, idle, processing, self-test, and completed; data screens for printing and temperatures |
| GIF assets (2026-10-19) | Use the **green variants** `gif_*_green.c` instead of the original red `gif_*.c` wherever one exists; **`gif_creality.c` replaces `gif_voron.c`** (idle logo). `gif_qgling_green` is **kept** for the self-test auto-leveling step. UI accent follows the GIFs: **green**. |
| Physical input | **BOOT button on GPIO0** (on the board) is the only input. There is no touch screen and the backlight is hard-wired. |
| Printing layout | **Auto-rotating pages** (progress/time ↔ temperatures ↔ layer/file); a double-click jumps to the next page |
| GUI library | **LVGL 8.4.0** (last v8; its GIF decoder needs ~20 % less RAM than v9) |
| Web page security | **No admin password.** The device is LAN-only; risk accepted (see §11). |
| Language / units | English, °C, 24 h time (defaults; no answer given) |
| Reference captures | Owner's **K1 Max**: full print with calibration, print without calibration, pause → resume → stop, connect during a print (DWIN sw 1.3.3.8 / 1.3.3.29), plus **idle connect on the latest firmware 1.3.5.19**. Location: `C:\D_Drive\Linde\A5 - Private\Creality\`. These are the primary evidence for the protocol doc. |
| Target printer firmware | **Current Creality firmware (K1 family 1.3.5.x, K2 current)** is primary; 1.3.3.x stays supported because its captures are the print-scenario fixtures |
| Completed/Stopped screen time | Default **10 min**, configurable; 0 = until the next state change (default; no answer given) |

## 3. Findings from the code review

The repository is the BTT KNOMI firmware: `knomiv2` (ESP32-S3, touch, sensors) and `knomiv1` (ESP32 **with 8 MB PSRAM**).
It uses SquareLine-generated LVGL 8.3.7 screens, polls Moonraker over HTTP, and provides an AsyncWebServer setup page.
`d:\Linde\A5 - Private\ESP32\Knomi_good` is the original BTT v1 firmware. It is proven on this exact hardware
(`wemos_d1_mini32`, TFT pins DC 26 / RST 33, SPI 27 MHz, 10-line LVGL buffer, GIFs ≤ 210×125, 3.0 MB firmware image).

### 3.1 Blockers for the target hardware (must fix)

| # | Problem | Where | Fix |
|---|---|---|---|
| B1 | LVGL heap uses `ps_malloc`, which returns NULL without PSRAM | `include/lv_conf.h` `LV_MEM_CUSTOM_ALLOC` | Use `malloc`/`free`/`realloc` on the internal heap |
| B2 | Full-frame 240×240 draw buffer (115 KB) from PSRAM | `lvgl_hal.cpp` | Two 240×24-line DMA buffers (2×11.5 KB) |
| B3 | Board `esp32r8` declares PSRAM and 16 MB flash; `spiffs_16MB.csv` | `buildroot/boards`, `platformio.ini` | Board `wemos_d1_mini32` (4 MB, no PSRAM) plus the new partition table (§6) |
| B4 | v1 pins differ from the hardware (DC 19→**26**, RST 4→**33**, BL 2 → none) | `pinout_knomi_v1.h` | New `board_pins.h` |
| B5 | `gif_print_ok` is 240×240 → the GIF decoder needs 4·W·H ≈ **225 KB RAM** | `src/gif/gif_print_ok.c` | Re-render at ≤ 150×150 |
| B6 | Two GIF objects alive at the same time (welcome + main) → ~150 KB RAM | `lvgl_usr.cpp` | Exactly **one** GIF object in the whole UI (§8.4) |
| B7 | Fonts are generated at **1 bpp** (jagged) | `src/ui/fonts` | Regenerate at 4 bpp with the needed glyphs |
| B8 | LVGL 8.3.7 needs a manual `lv_disp.c` patch (PR 4487) | `platformio.ini` comment, `lv_disp_(bugfix_backup).c` | LVGL 8.4.0 contains the fix; delete the backup file |

### 3.2 Defects in the existing design (avoid re-implementing them)

| # | Defect | Where |
|---|---|---|
| D1 | LVGL is called from a non-UI task (`lv_popup_warning` inside the Moonraker HTTP task), which is a data race | `moonraker.cpp` |
| D2 | `knomi_config` is written by the AsyncTCP web handler and read by the other tasks without locking | `webserver.cpp`, `wifi_setup.cpp` |
| D3 | Wi-Fi password printed to serial and **echoed in the HTTP response** | `wifi_setup.cpp`, `webserver.cpp` |
| D4 | HTML template values are unquoted (`value=$ip$`), which breaks on spaces and allows injection | `index_html.h` |
| D5 | On a STA connect failure the mode is **saved permanently as "ap"**, so after a router outage the device stays in AP mode forever | `wifi_setup.cpp` |
| D6 | Blocking 15 s connect loop; `goto restart` | `wifi_setup.cpp` |
| D7 | Config stored as a raw struct dump in emulated EEPROM, including `lv_color_t` and enums, without schema versioning | `wifi_setup.cpp` |
| D8 | An `AsyncWebServerRequest*` is kept across tasks for the scan callback (use-after-free if the client disconnects) | `webserver.cpp` |
| D9 | `millis()` overflow-prone timing (`ms > millis()`) | `lv_auto_goto_idle.cpp`, `lv_moonraker_change_screen.cpp` |
| D10 | OTA cannot work with the given partition table (otadata present but no `ota_1`); AsyncElegantOTA is an unmaintained BTT fork | `partition.csv`, `platformio.ini` |
| D11 | `README.md` is empty and the repository has no LICENSE file (upstream BTT KNOMI has none either) | repo root |

### 3.3 Memory facts (ESP32-WROOM-32, no PSRAM)

- Internal DRAM is about 320 KB. With Wi-Fi, lwIP, and AsyncTCP running, expect roughly **150–190 KB free** and a **largest free block of about 100–110 KB**. Phase 0.3 measures the real numbers.
- LVGL 8 GIF decoder (16-bit color) RAM: `4·W·H` (canvas and frame) plus a transient LZW table of up to ~24 KB.
- Assets (re-measured 2026-10-19 from the GIF streams inside `src/gif/*.c`). KB = 1024 bytes. All of them loop forever, and the C arrays are `LV_IMG_CF_RAW_CHROMA_KEYED`. **Green variants are used wherever one exists** (§2). `gif_wifi` (white, neutral) and `gif_print_ok` (already green) have no `_green` file and are kept as they are.

| GIF (used) | Replaces | Size | Frames × delay = loop | Flash | RAM (4·W·H) | Use |
|---|---|---|---|---|---|---|
| gif_creality | gif_voron | 150×150 | 14 × 120 ms = 1.68 s | 83.1 KB | **87.9 KB** | idle logo (S5) |
| gif_standby_green | gif_standby | 174×51 | 60 × 30 ms = 1.80 s | 27.8 KB | 34.7 KB | idle (S5) |
| gif_wifi | – (kept) | 150×100 | 366 × 30 ms = 10.98 s | 35.6 KB | 58.6 KB | setup (S1) |
| gif_homing_green | gif_homing | 96×96 | 25 × 40 ms = 1.00 s | 43.1 KB | 36.0 KB | processing: homing (S6) |
| gif_probing_green | gif_probing | 155×123 | 30 × 40 ms = 1.20 s | 29.1 KB | 74.5 KB | self-test (S7) |
| gif_qgling_green | gif_qgling | 207×104 | 200 × 40 ms = 8.00 s | 86.1 KB | 84.1 KB | self-test step 4 "Auto-leveling" (S7) |
| gif_heated_green | gif_heated | 176×82 | 80 × 30 ms = 2.40 s | 7.5 KB | 56.4 KB | heating finished → printing (S8) |
| gif_print_green | gif_print | 160×90 | 67 × 40 ms + 1 × 360 ms = 3.04 s | 45.7 KB | 56.2 KB | printing interstitial (S8) |
| gif_print_ok | – (kept) | 144×144 | 31 frames (mostly 30 ms) = 1.80 s | 6.9 KB | 81.0 KB | completed (S10) |
| gif_printed_green | gif_printed | 177×70 | 60 × 30 ms = 1.80 s | 82.6 KB | 48.4 KB | completed (S10) |
| **Total** | | | | **≈ 448 KB** | max **87.9 KB** | |

  Comment, plain-text, and XMP application extensions were stripped from these GIFs with `tools/strip_gif_meta.py` (≈ 1 KB each; `NETSCAPE2.0` is kept because LVGL's gifdec stops after one pass without it), then the C files were regenerated with `tools/bin2c.py`.

  Compared with the previous asset set, the change is **+85 KB flash** (`gif_creality` and `gif_qgling_green` added; `gif_homing_green` is 85 KB smaller than `gif_homing`). The peak RAM does not change: `gif_creality` has the same 150×150 canvas as the `gif_voron` it replaces, and the same peak as `gif_print`.
- Deleted: `gif_voron` (Voron branding) and the original red `gif_heated`, `gif_homing`, `gif_print`, `gif_printed`, `gif_probing`, `gif_qgling`, and `gif_standby` (replaced by their `_green` variants).
- **Rule:** at most one GIF is decoded at any time, and **max canvas ≤ 90 KB**. After full init (Wi-Fi, web server, and WS connected), the largest free block must be ≥ 1.25 × the largest canvas (**≥ 110 KB** for the 87.9 KB canvas of `gif_creality`). `gif_creality` is in the idle rotation, so it is loaded often. Phase 0.3 decides whether to downscale it.

## 4. Target architecture

```
                    core 0                                   core 1
 ┌────────────────────────────────────────────┐   ┌────────────────────────────┐
 │ net_task (prio 5)                          │   │ ui_task (prio 4)           │
 │  WifiManager  (STA/AP state machine, mDNS, │   │  DisplayHal (TFT_eSPI DMA) │
 │                captive DNS, NTP)           │   │  LVGL 8.4 lv_timer_handler │
 │  CrealityClient (own streaming WS client,  │   │  UiManager (screen FSM,    │
 │                skips >4 KB frames, watch-  │   │   pages, timeouts, button) │
 │                dog, backoff, JSON filter)  │   │  Screens (one GIF object)  │
 │  PrinterModel (merge+coerce, state derive) │   │                            │
 │        │ publishes PrinterSnapshot ────────┼──▶│ reads latest snapshot      │
 │        │ (mutex-protected copy + seq no.)  │   │ (copy, never holds lock    │
 │                                            │   │  during LVGL calls)        │
 │ AsyncTCP task (library)                    │   │ Button (GPIO0) polled here │
 │  WebPortal: reads snapshot/config copies,  │   └────────────────────────────┘
 │  posts ConfigChange to net_task queue      │
 └────────────────────────────────────────────┘
 Settings (NVS Preferences, schema v1, mutex): loaded at boot, written only by net_task
```

Rules:

- **Only `ui_task` calls LVGL.** Other tasks communicate through `PrinterSnapshot`, `NetStatus`, and FreeRTOS queues (fixes D1).
- Snapshots are plain structs (fixed-size `char[]`, no `String`), copied under a mutex. A sequence number lets the UI skip redraws when nothing changed.
- Web handlers never write settings directly. They validate input and post a `ConfigChange` to `net_task`, which persists and applies it (fixes D2 and D8).
- The protocol logic (`PrinterModel`) is hardware-independent and lives in `lib/creality_model/`, so it can be unit-tested on the host (`env:native`).

### 4.1 Target repository layout

```
platformio.ini
partitions_4MB.csv
include/lv_conf.h
src/
  main.cpp                    boot: serial, settings, tasks
  app_config.h                FW_VERSION, defaults (AP SSID prefix, port 9999, timeouts)
  board_pins.h                TFT + BOOT pins
  tft_setup.h                 TFT_eSPI user setup (picked up via __has_include)
  settings/settings.{h,cpp}   NVS Preferences, validation, mutex
  net/wifi_manager.{h,cpp}
  net/web_portal.{h,cpp}
  net/web_portal_html.h       generated from web/index.html (gzipped PROGMEM)
  printer/creality_client.{h,cpp}
  ui/display_hal.{h,cpp}
  ui/ui_manager.{h,cpp}
  ui/screens/*.{h,cpp}        one file per screen (§8)
  ui/theme.{h,cpp}            colors, styles, fonts
  ui/assets/gif/*.c           kept GIFs (§3.3; `_green` file and symbol names unchanged)
  ui/assets/fonts/*.c         regenerated 4 bpp fonts
  ui/assets/icons/*.c         small nozzle/bed/chamber icons (≤ 32 px)
lib/Button/                   kept (GPIO0)
lib/creality_model/           PrinterModel: merge, coercion, state derivation (no Arduino deps)
lib/ws_stream/                RFC 6455 client-side frame parser/encoder (no Arduino deps) + thin WiFiClient adapter
web/index.html                editable source of the settings page
test/test_creality_model/     Unity tests (native)
test/fixtures/*.jsonl         captured printer traces + *.expected.txt state sequences (Phase 0.4)
tools/WebSocketLogger.html    ✅ browser capture tool: records timed WS events + markers → capture file (§4.2)
tools/convert_capture.py      legacy owner captures ("MESSAGE <- {json}" / "> RECEIVED: {json}") → capture file without timing (§4.2)
tools/derive_prototype.py     ✅ reference implementation of the state rules (replays capture files and legacy captures)
tools/fake_printer.py         replay capture files / scripted scenarios as a WS server on :9999
tools/bin2c.py                GIF/binary → LVGL lv_img_dsc_t (LV_IMG_CF_RAW)
tools/build_web.py            web/index.html → gzipped C array (pre-build script)
tools/fonts.md                exact lv_font_conv commands
docs/…                        §12
```

### 4.2 Capture and fixture format (`knomi-ws-capture` v1)

All of these use the same file format: new printer captures, converted legacy captures, test fixtures (`test/fixtures/*.jsonl`), and fake-printer replays.
It is written by `tools/WebSocketLogger.html` and `tools/convert_capture.py`.

**Capture tool.** `tools/WebSocketLogger.html` is a single HTML file that you open directly in a desktop browser; nothing needs to be installed.

- **Recording:**
  - Every WebSocket event is recorded with a timestamp.
  - Auto-reconnect is on by default, so a printer reboot or a dropped link is recorded as `close` / `connect_failed` / `open` events.
  - Heartbeats are answered with `ok`, as the firmware does.
  - A screen wake lock keeps the computer awake.
  - The page asks for confirmation before it closes with unsaved events.
- **Markers:** one-click buttons (print started, pause, resume, stop, error, reboot) and a free-text "Printer screen: …" note.
- **Live display:**
  - The log numbers frames `#n` (same numbering as `derive_prototype.py`) and flags frames larger than 4 KB.
  - Clicking a line opens it in an inspector.
  - The log can be filtered and can hide temperature-only deltas.
  - A side panel shows the merged printer state and auto-detects the model, hostname, and firmware.
- **Saving:** the file name is `<model>_<fw>_<scenario>_<yyyymmdd-hhmm>.jsonl`, e.g. `k1max_1.3.5.19_pause-resume-stop_20261015-1732.jsonl`. Saving does not stop the capture, so intermediate copies can be saved during long prints.

**File format:** UTF-8 JSONL with one object per line. Line 1 is the header:

```json
{"type":"meta","format":"knomi-ws-capture","version":1,"tool":"tools/WebSocketLogger.html",
 "started":"2026-10-15T15:32:32.879Z","saved":"2026-10-15T17:02:10.114Z","url":"ws://192.168.0.55:9999/",
 "scenario":"pause-resume-stop","notes":"PLA, calibration on, Creality Print closed",
 "printer":{"model":"K1 Max","hostname":"K1Max-BF1B","fw":"1.3.5.19","modelVersion":"printer hw ver:;…;DWIN sw ver:1.3.5.19;"},
 "counts":{"connections":1,"rx":5120,"rx_json":5118,"rx_large":4,"rx_bytes":812345,"tx":0,"notes":3},
 "user_agent":"…"}
```

Each following line is one event. `t` is milliseconds since the capture start (monotonic clock):

| `type` | Fields | Meaning | Replay (`fake_printer.py`, native tests, `derive_prototype.py`) |
|---|---|---|---|
| `open` | `url` | Connection established | Starts a connection segment |
| `rx` | `len` and either `data` (text, **verbatim**) or `b64` (binary) | Message from the printer | Sent or merged at `t` (scaled) |
| `tx` | `len`, `data`, `auto: true` for heartbeat replies | Message sent by the capture tool | Ignored |
| `close` | `code`, `reason`, `clean`; `by: "client"` if the tool closed it | Connection ended | Fake printer closes the socket with `code` (not when `by: "client"`). Model: `reset()` → `OFFLINE`. |
| `connect_failed` | `url`, `code` or `error` | Connection attempt failed (printer off or rebooting) | Fake printer refuses connections until the next `open` |
| `error` | – | WebSocket error after `open` | Ignored |
| `note` | `text` | Operator marker, e.g. `Pause pressed` or `Printer screen: Auto-leveling` | Ignored by replay; printed next to state changes and test failures |

Rules:

- **Exact text:** `data` is stored exactly as received, never re-serialized. This preserves the byte size (the 4 KB skip limit), number formats (`"27.050000"`), and key order. `len` is the UTF-8 byte count and is informational only; readers recompute it.
- **Frame numbers:** `#n` counts the `rx` messages whose text starts with `{`, from 1 to the end of the file across reconnects. The same numbers appear in protocol doc §5.5, in `*.expected.txt`, and in the logger display.
- **Legacy captures:** converted legacy captures have no `t`. Replays then use a fixed spacing of 250 ms (the printing frame rate).
- **Compatibility:** readers ignore unknown event types and fields. `version` changes only on incompatible changes.
- **Limitation:** the browser API delivers whole messages, not wire frames. Fragmentation, ping/pong, and the handshake are not recorded; the fake printer scripts these (Phase 4.5).
- **Sizes:** about 1.5 MB per print hour plus the large list frames. Fixtures are committed as they are, without trimming, so that they match the real traffic.

## 5. Platform and libraries

| Component | Current | Target (pinned) | Notes |
|---|---|---|---|
| PlatformIO platform | `espressif32@6.4.0` (Arduino 2.0.x) | **pioarduino `55.03.312`** (Arduino-ESP32 **3.3.12**, ESP-IDF 5.5.5) | The official `platformio/espressif32` (v7.1.3) still ships Arduino 2.0.17. **Fallback:** `espressif32@7.1.3` if Phase 0.1 fails. |
| Board | `esp32r8` (custom, PSRAM) | `wemos_d1_mini32` (4 MB, DIO, 40 MHz) | Delete `buildroot/boards/` |
| LVGL | 8.3.7 + manual patch | **8.4.0** | Delete `lv_disp_(bugfix_backup).c` |
| TFT_eSPI | 2.5.0 | **2.5.43** | Core 3.x compatibility is verified in Phase 0.1. **Fallback:** LovyanGFX 1.2.x (same HAL interface). |
| ArduinoJson | ^6.19.4 | **7.4.3** | Use `JsonDocument` with a `DeserializationOption::Filter` |
| WebSocket client | – (HTTPClient) | **Own minimal streaming client** (`lib/ws_stream`, ≈ 400 LOC) over `WiFiClient` | The K1 Max pushes large list frames unsolicited: **78 KB `retGcodeFileInfo2` twice after every connect on FW 1.3.5.19**, and 20–28.5 KB `historyList`/`elapseVideoList` at job start/end. `links2004/WebSockets` 2.7.2 closes the connection (1009) above `WEBSOCKETS_MAX_DATA_SIZE` (it would never stay connected on 1.3.5.x) and `malloc`s whole frames. Our client uses a static 4 KB buffer and stream-discards larger frames (protocol doc §4). |
| Web server | ottowinter ESPAsyncWebServer-esphome 3.0 | **ESP32Async/ESPAsyncWebServer 3.12.1** + **ESP32Async/AsyncTCP 3.5.0** | Maintained fork (3.12.1 requires AsyncTCP ^3.5.0) |
| OTA | bigtreetech/AsyncElegantOTA | **removed** | USB-only decision |
| LIS2DW12, Adafruit_SHT4X, CST816S | v2 only | **removed** | |
| Core libraries used | – | WiFi, WiFiClient, DNSServer, ESPmDNS, Preferences, esp_sntp, mbedtls (SHA-1/base64 for the WS handshake) | |

### 5.1 `platformio.ini` (target draft)

```ini
[platformio]
default_envs = knomi

[common]
lib_deps =
    lvgl/lvgl@8.4.0
    bodmer/TFT_eSPI@2.5.43
    bblanchon/ArduinoJson@7.4.3
    ESP32Async/AsyncTCP@3.5.0
    ESP32Async/ESPAsyncWebServer@3.12.1

[env:knomi]
platform  = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.312/platform-espressif32.zip
board     = wemos_d1_mini32
framework = arduino
board_build.partitions = partitions_4MB.csv
monitor_speed   = 115200
monitor_filters = esp32_exception_decoder
extra_scripts   = pre:tools/build_web.py
lib_deps  = ${common.lib_deps}
build_flags =
    -Iinclude -Isrc
    -DLV_CONF_INCLUDE_SIMPLE
    -DCORE_DEBUG_LEVEL=1
    -DKNOMI_LOG_LEVEL=2

[env:knomi_debug]
extends     = env:knomi
build_type  = debug
build_flags = ${env:knomi.build_flags}
    -DCORE_DEBUG_LEVEL=3 -DKNOMI_LOG_LEVEL=4 -DKNOMI_HEAP_OVERLAY=1 -DLV_USE_PERF_MONITOR=1

[env:native]
platform  = native
test_framework = unity
lib_deps  = bblanchon/ArduinoJson@7.4.3
build_flags = -std=gnu++17
```

### 5.2 `lv_conf.h` key changes

| Setting | Value | Reason |
|---|---|---|
| `LV_MEM_CUSTOM` | 1, with `malloc`/`free`/`realloc` | No PSRAM (B1). The GIF canvas cannot fit in a static LVGL pool. |
| `LV_COLOR_DEPTH` / `LV_COLOR_16_SWAP` | 16 / **1** | DMA flush pushes pre-swapped pixels (remove the SquareLine `SWAP 0` check) |
| `LV_DISP_DEF_REFR_PERIOD` | 20 ms | Smooth GIFs without starving Wi-Fi |
| `LV_USE_GIF`, `LV_USE_QRCODE` | 1 | Setup QR codes |
| Widgets | keep label, arc, img, bar, spinner, line, canvas (QR); **disable** roller, meter, keyboard, chart, calendar, colorwheel, imgbtn, msgbox, tabview, tileview, win, list, menu, span, spinbox, table, textarea | Flash and RAM |
| Themes | default only (disable basic/mono) | |
| Built-in fonts | Montserrat 16 (for `LV_SYMBOL_*` glyphs) | Inter fonts handle text |
| `LV_USE_LOG` | 0 (release) / WARN in debug | |

## 6. Partition table (4 MB, USB-only)

Replace `spiffs_16MB.csv` with `partitions_4MB.csv`:

```csv
# Name,   Type, SubType,  Offset,   Size,     Flags
nvs,      data, nvs,      0x9000,   0x7000,
factory,  app,  factory,  0x10000,  0x3E0000,
coredump, data, coredump, 0x3F0000, 0x10000,
# total 0x400000 (4 MB)
```

Changes from the hardware-proven `partition.csv`:

- `otadata` and `spiffs` are removed (no OTA, no filesystem used).
- NVS grows from 20 KB to 28 KB by using the freed gap before 0x10000.
- The app is a `factory` partition of **3.875 MB**, compared with 3.125 MB before.

Firmware estimate is ~1.8–2.2 MB (the Arduino 3.x baseline plus ≈ 0.46 MB of GIF assets, §3.3), leaving more than 40 % headroom.

> When switching partition layouts, erase the chip once (`pio run -t erase`) so that stale `otadata`/NVS content is gone.
> The coredump partition is used only if the Arduino core was built with `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH`; verify this in Phase 0.1. If it is off, the partition stays reserved and harmless.

## 7. Settings (NVS namespace `knomi`, schema version 1)

| Key | Type | Default | Validation | Applied |
|---|---|---|---|---|
| `schema` | u8 | 1 | Migrate or reset on mismatch | boot |
| `wifi_ssid` | str ≤ 32 | "" | 1–32 bytes | reconnect |
| `wifi_pass` | str ≤ 64 | "" | 0 or 8–63 chars (64 hex) | reconnect; **never logged or returned** |
| `hostname` | str ≤ 31 | `knomi-XXXX` (last 2 MAC bytes) | RFC 1123 label | restart |
| `printer_host` | str ≤ 63 | "" | IPv4 or hostname (`.local` allowed, resolved with mDNS) | reconnect WS |
| `printer_port` | u16 | **9999** | 1–65535 | reconnect WS |
| `ap_ssid` | str ≤ 32 | `KNOMI-XXXX` | 1–32 bytes | next AP start |
| `ap_pass` | str ≤ 63 | "" (open) | "" or 8–63 chars | next AP start |
| `rotation` | u8 | 0 | 0/1/2/3 (×90°) | live |
| `page_secs` | u8 | 6 | 3–60 | live |
| `result_min` | u16 | 10 | 0–1440 (0 = until the next state change) | live |
| `tz` | str ≤ 47 | `UTC0` | POSIX TZ string | live (ETA clock; optional, Phase 7) |

Behavior:

- **Factory reset** erases the namespace.
- The web API returns passwords only as `"set": true/false` (fixes D3).

## 8. Display specification (240×240 round)

General rules:

- **Safe area:** text stays inside a centered 170×170 square; arcs use the outer ring (r = 112–120).
- **Fonts** (Inter SemiBold, 4 bpp, regenerated):
  - 14 and 18 px: ASCII + Latin-1 + Latin Ext-A, so that accented filenames render
  - 24 px: ASCII
  - 48 px: digits, `% : ° C`, and space
  - LVGL Montserrat 16 for symbols
- **Colors:**
  - KNOMI green `#329A40` (accent and progress; matches the green GIFs)
  - amber `#F0A020` (paused)
  - light green `#5DA910` (completed; with the ✓ symbol so that it stays distinct from the accent)
  - red `#E53935` (error)
  - grey `#808080` (stopped and secondary text)
  - background black
- **Transitions:** fade 300 ms. `UiManager` never starts a screen load while another one is still animating (it queues the latest target instead).
- **Number formats:**
  - Temperatures: `215°` actual and `/220°` target. A target of 0 is shown as `–`.
  - Time: `1h 23m`, `12m`, or `45s`.
  - Unknown values: `--`.

### 8.1 Logical state → screen

| Logical state (protocol doc §5.4) | Screen | Leaves when |
|---|---|---|
| Boot | S0 Splash (1.5 s) | Always → network screens |
| No Wi-Fi config, or setup mode forced | S1 Setup (AP) | Wi-Fi configured and connected |
| Wi-Fi connecting / lost | S2 Wi-Fi connecting / S3 Wi-Fi lost | Connected |
| Printer host not configured | S4a "Configure printer" (device URL + QR) | Host saved |
| `OFFLINE` | S4b Printer connecting / unreachable | WS up and first frame received |
| `NOT_READY` (`connect == 0`; FW 1.3.5.x placeholder dump) | S4b with the text "Printer starting…" (no values shown) | `{"connect":1}` received |
| `IDLE` | S5 Idle pages (GIF ↔ temps) | State change |
| `PROCESSING` | S6 Processing | State change |
| `SELF_TESTING` | S7 Self-test | State change |
| `PRINTING` | S8 Printing pages | State change |
| `PAUSED` | S8 with paused styling | State change |
| `STOPPED` | S9 Stopped | `result_min` elapsed or state change |
| `COMPLETED` | S10 Completed sequence | `result_min` elapsed or state change |
| `ERROR` | S11 Error | errcode = 0, a job becomes active, or the button acknowledges it |
| Button click | S12 Info overlay (10 s) | Timeout or click |

### 8.2 Screens

| ID | Content |
|---|---|
| S0 Splash | "KNOMI" in 48 px, "Creality edition", and the FW version. While it shows: check the display, settings load, and log free heap. |
| S1 Setup | Page A: `gif_wifi` (top), "Join Wi-Fi", the AP SSID, and the AP password if set. Page B: QR code `WIFI:T:nopass;S:<ssid>;;` (or WPA) plus `http://192.168.4.1`. Pages alternate every 8 s. |
| S2 Wi-Fi connecting | LVGL spinner (ring), "Connecting to", SSID |
| S3 Wi-Fi lost | Wi-Fi symbol (grey), "Wi-Fi lost – retrying", SSID, elapsed time. After 5 min the AP starts **in addition** (AP+STA) so the user can reconfigure; the saved mode is **not** changed (fixes D5). |
| S4a Configure printer | "Open http://<ip>" and "or http://<hostname>.local", plus a QR code of the URL |
| S4b Printer offline | Spinner (red after the 3rd failure), "Printer <host>:<port>", "Connecting… / Unreachable, retry in N s", own IP (small) |
| S5 Idle | Page A: `gif_creality` (Creality logo). Page B: `gif_standby_green`. Page C: three temperature rows (nozzle, bed, chamber) with icons, plus a line "Last: <file> ✓/✕" if there is a stale result. Pages rotate every `page_secs`; A → B swaps the source of the single GIF object. **If any target > 0 while idle** (manual preheat, or the post-print nozzle heating seen on the K1 Max), page C is shown permanently with the label "Heating". |
| S6 Processing | Top label with the sub-label (Heating / Homing / Bed leveling / Preparing / Stopping… / Resuming…). Center: `gif_homing_green` when homing, otherwise two horizontal bars (nozzle, bed) showing actual/target, with numbers. |
| S7 Self-test | `gif_probing_green` in the center, switching to `gif_qgling_green` while `withSelfTest` = 4 ("Auto-leveling"; the step name is still unverified, protocol doc §5.3). Label "Self-test · step N" plus the step name (protocol doc §5.3: Preparing / Heating / Nozzle cleaning / Auto-leveling / Calibrating), and nozzle/bed temperatures (small). `withSelfTest` is a **step number** (1–5, then 100), not a percentage. The outer ring shows 5 step segments, with completed steps filled. |
| S8 Printing | The outer progress arc (`printProgress`) persists on all pages. P1: big `57%`, "1h 23m left" (`--` while `printLeftTime` is 0), status word "Printing" (or the transitional "Pausing / stopping…"). P2: nozzle/bed/chamber actual/target. P3: "Layer 57 / 240", "Elapsed 2h 01m", filename (basename, circular scrolling), optional "Fan 100 % · Speed 100 %". On the processing → printing transition, `gif_heated_green` plays for ≈ 7 s (3 loops of 2.4 s), then `gif_print_green` plays once (3.0 s) as an interstitial. **Paused:** amber arc, status word "Paused", rotation continues. **Error badge** (red dot + code) at the top if errcode ≠ 0. |
| S9 Stopped | Grey ring, ✕ symbol, "Print stopped" (or "Print failed" for state 3), filename, "at 57 %", elapsed time |
| S10 Completed | `gif_print_ok` (1.8 s, one loop) → `gif_printed_green` (≈ 7 s, 4 loops of 1.8 s) → summary: light-green ring, ✓, "Completed", filename, total time. On the summary, the button double-click replays the animation. |
| S11 Error | Red ring, ⚠ symbol, "Printer error", `code 500 · key 121`, and the printer model. A click acknowledges the error (remembered until the errcode changes). |
| S12 Info | Device IP, hostname, RSSI, printer host:port, link state, printer model and firmware ("update available 1.3.5.22" from `linuxOtaInfo`, if pushed), FW version, free heap / largest block |

### 8.3 Button (GPIO0, active low, internal pull-up)

| Action | Effect |
|---|---|
| Click | Show/hide S12 Info; acknowledge an error on S11 |
| Double-click | Next page on rotating screens (S5, S8, S1); this pauses auto-rotation for 30 s |
| Hold ≥ 5 s | The screen shows a countdown. On release: **start setup mode** (AP + captive portal). Settings are kept. |
| Hold ≥ 15 s | **Factory reset** (erase NVS, reboot). The countdown screen turns red after 5 s. |

> GPIO0 is a strapping pin: holding it during power-on/reset enters the ROM bootloader. Document this in the README.
> The existing `lib/Button` debounce and click detection are reused. Hold durations are measured in `UiManager`.

### 8.4 GIF handling

- Create **one** `lv_gif` object at boot. Screens borrow it by re-parenting it with `lv_obj_set_parent` and set the source with `lv_gif_set_src`.
- `lv_gif_set_src` closes the previous decoder before it opens the next one, so only one canvas exists at a time.
- After `lv_gif_set_src`, check that `((lv_gif_t*)obj)->gif != NULL`. If decoding failed (out of memory), hide the object, show the static fallback icon, and log the largest free block.
- Pause the GIF (`lv_timer_pause(gif->timer)`) when it is not visible.

## 9. Networking

### 9.1 Wi-Fi manager (non-blocking state machine in `net_task`)

```
BOOT ─▶ no ssid ───────────────────────────────▶ AP_SETUP (AP + captive DNS + portal)
     └▶ ssid ─▶ STA_CONNECTING ─(ok)─▶ STA_CONNECTED ─(lost)─▶ STA_RECONNECTING
                    │ (20 s fail)                                   │ (5 min)
                    └──────────────▶ AP_FALLBACK (AP+STA, keeps retrying STA every 30 s;
                                                  AP stops 60 s after STA reconnects)
```

- `WiFi.setAutoReconnect(true)` and `WiFi.persistent(false)`; credentials come from NVS only.
- The hostname is set before `WiFi.begin`. mDNS advertises `<hostname>.local` with service `_http._tcp`.
- AP IP `192.168.4.1/24`. The captive portal answers `/generate_204`, `/hotspot-detect.html`, `/connecttest.txt`, and `/ncsi.txt`, and redirects any other host to the portal.
- After STA connects, start NTP with `configTzTime(tz, "pool.ntp.org", "time.google.com")`. "NTP valid" means the epoch is greater than 2025-01-01.
- `WiFi.scanNetworks(async=true)` runs only on demand (`/api/scan`). Results are cached for 30 s.

### 9.2 Creality client (normative behavior in the protocol doc)

**Transport: own streaming WebSocket client** (`lib/ws_stream`, RFC 6455 client subset):

- Connection:
  - Resolve the host before every connect: `.local` names through `MDNS.queryHost`, others through `WiFi.hostByName`.
  - `WiFiClient` TCP connect with a 5 s timeout and `setNoDelay(true)`.
- HTTP upgrade:
  - `GET / HTTP/1.1` with `Sec-WebSocket-Key`: 16 random bytes from `esp_random()`, base64-encoded with mbedtls.
  - Validate `101` and `Sec-WebSocket-Accept` (SHA-1 via mbedtls, part of the core).
- Frame parser: a byte-wise state machine (header → extended length → payload).
  - Server frames are unmasked. Client frames are masked with a random key.
  - Text payloads up to **4 KB** go into a **static buffer** and are delivered as a complete message.
  - **Larger payloads are read and discarded in 512-byte chunks without allocation.** The first 32 bytes are kept for a diagnostic log line; the connection stays open.
  - Continuation frames are assembled while the total is ≤ 4 KB; otherwise the whole message is discarded.
  - `ping` → `pong`; `close` → echo the close and reconnect; binary frames are discarded.
- The parser is pure C++ without Arduino dependencies, so it is unit-tested natively (split headers, 16- and 64-bit lengths, a 28 KB frame, fragmentation, a close frame).

**Protocol layer:**

- No library heartbeat; the reconnect interval is driven by our own backoff (1 s ×1.8 up to 60 s, ±20 % jitter).
- Text message handling:
  - `ok` → ignore.
  - JSON with `ModeCode:"heart_beat"` → send `ok` (not seen on the K1 Max; kept for other firmware).
  - Any other JSON → deserialize with the filter, then `PrinterModel::merge`.
- ArduinoJson uses a **custom allocator on a static 8 KB arena**, so parsing never touches the heap.
- Silence watchdog: 10 s → send the `ReqPrinterPara` probe; 30 s → disconnect. The idle K1 Max sends about 1 frame/s.
- On (re)connect: `PrinterModel::reset()` so that stale keys do not stick.
- **Read-only guarantee:** the client exposes no API other than sending `ok` and the `ReqPrinterPara` get. A unit test greps the source for `"set"` and fails if found.
- A snapshot is published at most every 100 ms (coalescing bursts) and immediately on a logical state change.

### 9.3 Web portal (ESPAsyncWebServer, port 80)

| Route | Method | Purpose |
|---|---|---|
| `/` | GET | Single-page settings UI (gzipped). Sections: Status, Printer, Wi-Fi, Display, System. |
| `/api/status` | GET | `{fw, uptime, heap:{free,min,largest}, wifi:{mode,ssid,rssi,ip}, printer:{link,host,port,model,fw,state,sub,progress,left,job,layer,total,nozzle:{a,t},bed:{a,t},chamber:{a,t},file,err:{code,key}}}` |
| `/api/config` | GET | Current settings; passwords only as `"wifi_pass_set": true` |
| `/api/config` | POST | JSON body; validated server-side. Returns 400 with field errors, or `{ok:true, restart:false}`. |
| `/api/scan` | GET | Starts or returns the async scan: `[{ssid,rssi,secure}]` |
| `/api/restart` | POST | Reboots after 500 ms |
| `/api/factory-reset` | POST | Erases NVS and reboots (the UI asks for confirmation) |
| captive routes | GET | Only in AP mode |

- All values are injected through JSON and `textContent`; no server-side templating (fixes D4).
- No authentication (owner decision). CORS is disabled and POST requires `Content-Type: application/json`, which blocks trivial cross-site form posts.

## 10. Implementation phases

Each phase ends with a **green build** (`pio run -e knomi`), passing native tests (from Phase 4), and a short HIL check.
Sizes: S ≤ 0.5 d, M ≈ 1–2 d, L ≈ 3+ d.

### Phase 0: Spikes and data capture (de-risking)

| # | Task | Size | Exit criteria |
|---|---|---|---|
| 0.1 | Toolchain spike: pioarduino 55.03.312 + `wemos_d1_mini32` + TFT_eSPI 2.5.43 + LVGL 8.4.0 "hello" with DMA flush on the real panel | M | Colors, orientation, and inversion correct; no crash for 10 min. If TFT_eSPI fails on core 3.x, try LovyanGFX, then the official `espressif32@7.1.3` fallback. Record the decision in `docs/ARCHITECTURE.md`. |
| 0.2 | SPI speed: try 80 MHz (IOMUX pins 18/23/5), fall back to 40 MHz | S | No artifacts for 10 min of GIF playback |
| 0.3 | Heap budget: Wi-Fi STA + AsyncWebServer + WS client connected to the printer + one GIF; log `heap_caps_get_free_size`/`get_largest_free_block`/`get_minimum_free_size` every 5 s | M | Numbers recorded in `docs/ARCHITECTURE.md`. Decide on downscaling `gif_print_green` and `gif_creality` (both 87.9 KB canvases, §3.3). |
| 0.4 | Protocol capture. **K1 Max: done** (5 owner sessions, see §2; print scenarios on 1.3.3.x, idle on 1.3.5.19). Steps:<br>• Convert the legacy captures with `tools/convert_capture.py` into `test/fixtures/k1max_<fw>_<scenario>.jsonl` (§4.2, no timing). Write `*.expected.txt` from protocol doc §5.5.<br>• Record all new captures with **`tools/WebSocketLogger.html`** (§4.2):<br>&nbsp;&nbsp;– Pick the scenario from the list and keep auto-reconnect and keep-awake on.<br>&nbsp;&nbsp;– Add a marker for every action on the printer.<br>&nbsp;&nbsp;– Use "Screen shows…" to record what the printer screen shows, especially the **self-test step names**.<br>&nbsp;&nbsp;– Note in the notes field whether other clients (Creality Print, Home Assistant) were connected.<br>&nbsp;&nbsp;– Save, then copy the file to `test/fixtures/`.<br>• **Highest priority: repeat the print scenarios on 1.3.5.19** (`full-print`, `pause-resume-stop`, `connect-during-print`, `idle-reconnect`, i.e. reconnect twice while idle).<br>• Then record the same scenarios on the **K2 family** (and K1/K1C if available).<br>• Add: `cancel-printer-screen`, `cancel-self-test`, `error` (runout or forced error), and `printer-reboot` (auto-reconnect records the gap). | M | K1 Max fixtures in the repo, **including at least one 1.3.5.x print**. ≥ 1 K2 trace per scenario. Every new capture has timing and markers. Protocol doc §7 answered (self-test step names confirmed from the screen markers). |
| 0.5 | Asset prep: re-render `gif_print_ok` ≤ 150×150 (gifsicle `--resize-fit 150x150 -O3 --colors 64`, keep the green palette); `tools/bin2c.py`; generate 4 bpp fonts (`tools/fonts.md`) | S | Assets compile; RAM per GIF ≤ 90 KB; GIF flash total ≤ 470 KB (§3.3) |

### Phase 1: Cleanup (remove everything not required)

| # | Task | Size |
|---|---|---|
| 1.1 | Create branch `creality-rework`; tag the current state `pre-creality` | S |
| 1.2 | Delete per the inventory in §10.1 | S |
| 1.3 | Temporary `main.cpp` that boots the display only; build green | S |

#### 10.1 Removal and keep inventory

| Path | Action | Reason |
|---|---|---|
| `src/moonraker.{h,cpp}` | delete | Replaced by the Creality client |
| `src/pinout*.h` | delete → `board_pins.h` | v2 removed; v1 pins wrong (B4) |
| `src/lis2dw12.cpp`, `src/sht4x.cpp`, `lib/CST816S/` | delete | v2-only sensors and touch |
| `src/lvgl_hal.{h,cpp}` | rewrite → `ui/display_hal` | Remove touch, AW9346 backlight pulse code, and the PSRAM buffer |
| `src/lvgl_usr.{h,cpp}` | rewrite → `ui/ui_manager` | Extrude/roller logic not needed |
| `src/ui/` (SquareLine output: `ui.c/h`, `ui_helpers`, `components/`, `screens/*`, `images/*`, `CMakeLists.txt`, `filelist.txt`) | delete | Control screens; the SquareLine project file is not in the repo, so the generated code cannot be regenerated. New screens are hand-written. |
| `src/ui/images/ui_img_nozzle_png.c`, `ui_img_bed_png.c` | **keep → `ui/assets/icons/`** (check size ≤ 40 px) | Small icons, reused |
| `src/ui/fonts/*` | replace with regenerated 4 bpp fonts | B7 |
| `src/ui_overlay/*` | delete (logic re-implemented in `ui_manager`/screens) | Moonraker-, touch-, and control-specific |
| `src/gif/gif_voron.c` | delete → replaced by `gif_creality.c` | Voron branding (§2) |
| `src/gif/gif_heated.c`, `gif_homing.c`, `gif_print.c`, `gif_printed.c`, `gif_probing.c`, `gif_qgling.c`, `gif_standby.c` | delete | Replaced by their `_green` variants (§2) |
| `src/gif/gif_*_green.c`, `gif_creality.c`, `gif_wifi.c`, `gif_print_ok.c` | move → `ui/assets/gif/` (file and symbol names unchanged) | §3.3 |
| `src/webserver.cpp`, `src/index_html.h` | rewrite → `net/web_portal*` + `web/index.html` | D2, D3, D4, D8 |
| `src/wifi_setup.cpp`, `src/knomi.h`, `src/config.h` | rewrite → `net/wifi_manager`, `settings/`, `app_config.h` | D5, D6, D7 |
| `include/favicon.h` | delete (inline SVG favicon in HTML) | BTT logo, 27 KB |
| `include/lv_conf.h` | edit per §5.2 | |
| `lib/Button/` | keep | GPIO0 button |
| `buildroot/boards/*` | delete | Standard board used |
| `spiffs_16MB.csv` | delete → `partitions_4MB.csv` | §6 |
| `platformio_upload_ota.py` | delete | USB only |
| `lv_disp_(bugfix_backup).c` | delete | B8 |
| `test/README` | replace with real tests | |

### Phase 2: Platform, display HAL, LVGL configuration

| # | Task | Size |
|---|---|---|
| 2.1 | `platformio.ini` per §5.1; `partitions_4MB.csv` per §6; `lv_conf.h` per §5.2 | S |
| 2.2 | `board_pins.h`, `tft_setup.h` (GC9A01, pins 23/18/5/26/33, no MISO, no BL, SPI frequency from Phase 0.2, color order and inversion from Phase 0.1) | S |
| 2.3 | `display_hal`: `tft.begin()`, `initDMA()`, two 240×24 DMA buffers (`heap_caps_malloc(MALLOC_CAP_DMA)`), flush via `pushImageDMA`, rotation from settings (`tft.setRotation`) | M |
| 2.4 | `ui_task` skeleton on core 1: `lv_timer_handler()` loop with `vTaskDelay(5 ms)`; splash S0 | S |

### Phase 3: Settings, Wi-Fi, web portal

| # | Task | Size |
|---|---|---|
| 3.1 | `settings` module: schema, defaults, validation functions (shared by the web API and unit tests), mutex, `ConfigChange` queue | M |
| 3.2 | `wifi_manager` state machine (§9.1), captive DNS, mDNS, NTP | M |
| 3.3 | `web/index.html` (vanilla JS, ≤ 15 KB gzipped) + `tools/build_web.py` pre-build gzip → `web_portal_html.h` | M |
| 3.4 | `web_portal` routes (§9.3), JSON validation, captive routes | M |
| 3.5 | Network screens S1–S4a, wired to `NetStatus` | M |

### Phase 4: Creality client and printer model

| # | Task | Size |
|---|---|---|
| 4.1 | `lib/creality_model`: `PrinterModel` struct with `has_value` flags; `coerce_number()`; `merge(JsonObjectConst)`; `connect` valid flag; chamber capability by late discovery (protocol doc §3); filename basename plus UTF-8-safe truncation (≤ 63 bytes); model/board-code detection; firmware string parsing; offered printer update (`linuxOtaInfo.version`) | M |
| 4.2 | `derive_state()` per protocol doc §5.4/5.5, including the sub-label, debounce, freshness, error acknowledgement, and new-job detection | M |
| 4.3 | Unit tests (`test/test_creality_model`): coercion (number / string / "" / null / "007"), delta merge, every rule in protocol doc §5.4, and **replay of each fixture against its expected state sequence**. The K1 Max sequences are already defined in protocol doc §5.5; the Python reference `tools/derive_prototype.py` is the behavioural oracle. The C++ port must produce the identical sequence for every fixture.<br>The replay harness reads §4.2 files with ArduinoJson:<br>• Each `rx` text goes through the same ≤ 4 KB limit as the client.<br>• The injected clock advances to each event's `t` (250 ms steps for legacy files), so debounce and freshness are tested with the real timing.<br>• A `close` event acts as a disconnect: `reset()`, then `OFFLINE`, as `derive_prototype.py` does. | M |
| 4.4 | `lib/ws_stream` frame parser/encoder + native unit tests (protocol doc §4, plan §9.2), then the `creality_client` adapter: connect/resolve, handshake, heartbeat, silence watchdog, backoff, JSON filter with the static arena, snapshot publish, statistics (frames, skipped large frames with their first key, reconnects, max frame size) exposed in `/api/status` | L |
| 4.5 | `tools/fake_printer.py`: replays §4.2 capture files at 1–50× speed, **including the real 20–28 KB and 78 KB list frames**.<br>• Each `open`…`close` segment serves one client connection; `rx` events are sent at their recorded `t`.<br>• A server-side `close` closes the socket with the recorded code.<br>• A `connect_failed` gap refuses connections for its duration (printer reboot).<br>• `tx`, `note`, and `error` events are skipped, and notes are echoed to the console.<br>Scripted faults (silence, close, a 128 KB frame, fragmented frames, blank fields, the 1.3.5.x placeholder dump with `connect:0`, `ok` frames, heartbeat on/off) | S |

### Phase 5: UI screens and state machine

| # | Task | Size |
|---|---|---|
| 5.1 | `theme`: colors, fonts, styles, shared widgets (outer arc, temperature row, centered status label) | S |
| 5.2 | `ui_manager`: screen FSM per §8.1, page rotation, result timeout, interstitial GIF sequencing, transition guard, single GIF object | L |
| 5.3 | Screens S5–S12 per §8.2 | L |
| 5.4 | `KNOMI_HEAP_OVERLAY` (debug): small heap/fps text at the bottom | S |

### Phase 6: Button

| # | Task | Size |
|---|---|---|
| 6.1 | Integrate `lib/Button` on GPIO0 in `ui_task`; actions per §8.3, including hold countdown screens; setup mode and factory reset through `ConfigChange` | S |

### Phase 7: Hardening and polish

| # | Task | Size |
|---|---|---|
| 7.1 | Task watchdog on `ui_task`/`net_task`; `esp_reset_reason()` shown on the Info screen and in `/api/status` | S |
| 7.2 | Optional ETA clock on P3 ("Done ≈ 14:35") when NTP is valid and `tz` is set | S |
| 7.3 | Log hygiene: no secrets; `KNOMI_LOG_LEVEL` macros | S |
| 7.4 | 24 h soak test (§13): no reboot; minimum free heap and largest block stable (drift < 2 KB) | M |

### Phase 8: Documentation and release

| # | Task | Size |
|---|---|---|
| 8.1 | Write the documents in §12 | M |
| 8.2 | GitHub Actions: `pio run -e knomi` + `pio test -e native` on push/PR; upload `firmware.bin` and the merged `knomi-full.bin` (bootloader + partitions + app, for the ESP Web Flasher) as artifacts | S |
| 8.3 | Version `2.0.0-creality`, `CHANGELOG.md`, release with the binaries | S |

### Dependencies

```
0.1 ─▶ 0.2, 0.3, 2.x          0.4 ─▶ 4.2/4.3 (fixtures)        0.5 ─▶ 5.3
1.x ─▶ 2.x ─▶ 3.x, 5.x        4.1 ─▶ 4.2 ─▶ 4.3                 3.2 ─▶ 4.4
4.4 + 5.2 ─▶ 5.3 ─▶ 6.1 ─▶ 7.x ─▶ 8.x
```

Phase 4.1–4.3 (pure logic, native tests) can be developed in parallel with Phases 2–3.

## 11. Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| TFT_eSPI is not stable on Arduino core 3.x | Display broken | Phase 0.1 gate; fall back to LovyanGFX or to Arduino 2.0.17 (`espressif32@7.1.3`) |
| Heap fragmentation from repeated GIF switching | GIF fails to load after hours | One GIF object; similar canvas sizes (35–88 KB); fallback icon; 24 h soak; downscale GIFs if needed. The idle rotation swaps `gif_creality` (88 KB) ↔ `gif_standby_green` (35 KB) every `page_secs`, so the soak test runs mostly idle. |
| Printer firmware differences in state codes | Wrong screen | Rules verified on K1 Max captures + fixtures + debounce; unknown codes → PROCESSING if a file is set, else IDLE; log unknown codes. **K2 captures still needed.** |
| Large unsolicited list frames (78 KB file list after every connect on 1.3.5.19, growing with the file count; 20–28.5 KB history/timelapse lists at job start/end) | Endless reconnects or heap exhaustion | Own streaming client skips frames > 4 KB without allocating (§9.2); fake printer replays the real large frames |
| Placeholder dump on 1.3.5.x (`connect:0`, all values 0) | 0 °C shown, chamber hidden, or a false IDLE during a print | `NOT_READY` state until `connect:1`; capabilities re-evaluated on every merge; one `ReqPrinterPara` at `connect` 0 → 1; verify with a 1.3.5.x connect-during-print capture |
| Stale error or result after a reconnect | Misleading screen | Freshness rules (§5.5), error acknowledgement, error badge during active jobs |
| Concurrent-client limit on the printer | KNOMI or Creality Print disconnected | Verify in Phase 0.4; only one socket from KNOMI; backoff |
| Creality firmware update changes the protocol | Data missing | Tolerant parser (unknown/missing fields → `--`); fixtures make regressions visible |
| No auth on the web page (owner decision) | Anyone on the LAN can change settings | LAN-only device, JSON-only POST, no secrets returned; can be added later |
| Asset licensing: BTT KNOMI has no LICENSE; reference projects are (A)GPL; `gif_creality` shows the Creality logo (trademark) | Distribution issues | Personal/private use of the BTT GIFs and the logo animation; implement the protocol from facts, no copied code; Inter font is OFL (include the license) |
| GPIO0 held at power-up | Boots into the ROM bootloader | Documented in the README |
| Long browser capture lost or cut (tab closed, laptop sleeps, Wi-Fi drop) | Missing or incomplete fixture | The logger warns before the page closes with unsaved events, holds a screen wake lock, and auto-reconnects (gaps are recorded as `close`/`connect_failed`). Save intermediate copies during long prints. |

## 12. Documentation deliverables

| Document | Content | Phase |
|---|---|---|
| `docs/CREALITY_WS_PROTOCOL.md` | ✅ written and revised with the K1 Max captures (verified sequences, state 2 = completed, self-test steps, large-frame handling, rules validated by a prototype). Extend with the K2 findings. | 0.4 |
| `README.md` | Overview, photo, features, supported printers, hardware and wiring, build and flash (PlatformIO + ESP Web Flasher), first setup walkthrough, button actions, troubleshooting (Wi-Fi, printer unreachable, blank screen, colors inverted), licenses and credits | 8.1 |
| `docs/HARDWARE.md` | Wiring table, schematic notes (GPIO5/GPIO0 strapping, 3.3 V supply, no backlight control), enclosure notes, SPI speed result | 0.1 / 8.1 |
| `docs/ARCHITECTURE.md` | Tasks and cores, data flow, snapshot and queue contracts, memory budget (Phase 0.3 numbers), Phase 0.1 platform decision, module responsibilities | 0.3 / 8.1 |
| `docs/SCREENS.md` | Expanded §8: mockups (PNG from the LVGL simulator or photos) for each screen and page, state → screen table, button table | 5.3 |
| `docs/TESTING.md` | Native tests, fake printer usage, capture walkthrough (`tools/WebSocketLogger.html`, scenarios, markers, naming; §4.2), HIL checklist (§13) | 4.5 / 7.4 |
| `tools/fonts.md` | Exact `lv_font_conv` commands and glyph ranges | 0.5 |
| `CHANGELOG.md` | From 2.0.0-creality onward | 8.3 |

## 13. Test and acceptance

### 13.1 Automated

- **Native unit tests** (`pio test -e native`): coercion, merge, derivation rules, freshness/debounce (with an injected clock), fixture replays, settings validation, time and temperature formatting.
- **Fake-printer integration** (manual run against the device on the LAN): all scenarios in Phase 4.5.

### 13.2 Hardware-in-the-loop checklist

| # | Scenario | Expected |
|---|---|---|
| H1 | Fresh flash, no config | S1 setup shows the AP SSID and a QR code. The phone joins and the captive portal opens. |
| H2 | Save Wi-Fi + printer IP | S2 → S4b → S5 within 20 s; settings survive a reboot |
| H3 | Wrong Wi-Fi password | AP fallback after 20 s; the settings are **not** switched to AP permanently |
| H4 | Router off for 10 min, then on | S3 → (AP+STA after 5 min) → reconnects automatically; AP disappears |
| H5 | Printer powered off | S4b with backoff; recovers within 60 s after the printer boots |
| H6 | Full print on K1-family and K2 | idle → (self-test) → processing (heating/homing) → printing pages → completed sequence → idle after `result_min` |
| H7 | Pause/resume | Amber paused styling, then back to printing |
| H8 | Cancel during printing and during heating | Stopped screen (no false "completed"); "Stopping…" while the printer reports state 7 |
| H9 | Printer error (e.g. runout or a CFS error) | Error screen, or error badge during an active job; a click acknowledges it |
| H10 | KNOMI reboot after a print finished hours ago | Idle with "Last: … ✓", **not** the Completed screen |
| H11 | Creality Print / Home Assistant connected at the same time | Both keep working |
| H12 | Button: click / double / hold 5 s / hold 15 s | Info / next page / setup mode / factory reset |
| H13 | Accented filename (e.g. `Fogaskerék_PLA.gcode`) | Rendered correctly and scrolling |
| H14 | 24 h soak while idle + one long print | No reset; heap stable; GIFs always load |
| H15 | Connect to a printer on FW 1.3.5.x (78 KB file list ×2), job start/end (`historyList` ×2), and opening the file list in Creality Print | **No WS reconnect** (the reconnect counter in `/api/status` is unchanged); the skipped-frame counter increases; "Printer starting…" → idle data within 2 s |
| H16 | Print with calibration vs without | Self-test screen shows steps 1–5 vs 1–4 with names |

### 13.3 Requirement → acceptance mapping

| Requirement | Verified by |
|---|---|
| v1 only, v2 removed | Phase 1 inventory; only `env:knomi`, `env:knomi_debug`, and `env:native` exist |
| Moonraker → Creality WS | Phase 4; H2, H5, H6 |
| Settings (WS IP, port) + screens after setup | §7, §9.3; H1–H2, S4a |
| No control | Read-only guarantee test (§9.2) |
| 8 states | Derivation unit tests + H6–H9 |
| Entities (status, progress, time left, nozzle/bed/chamber, layers, file, etc.) | S8 pages; `/api/status` |
| Remove unneeded code | §10.1 |
| Updated platform and libraries | §5 versions pinned |
| Custom hardware / wiring | `board_pins.h`, Phase 0.1–0.2 |
| 4 MB partition table | §6; the image fits with > 40 % headroom |
