# KNOMI → Creality K-series WebSocket display: rework plan

Status: **S3 Phase 0.3 passed; Phase 0.4 extended to 14 owner-accepted baselines; Phase 0.5 assets prepared and compile-checked** · Last update: 2026-11-10

Related documents:

- [docs/CREALITY_WS_PROTOCOL.md](docs/CREALITY_WS_PROTOCOL.md): protocol reference and state-derivation rules (normative for Phase 4)
- [CREALITY_WEBSOCKET_IMPL.md](CREALITY_WEBSOCKET_IMPL.md): implementation decisions, validation results, and pending acceptance
- [docs/SCREENS.md](docs/SCREENS.md): source-derived original KNOMI visual references, owner-approved adaptations, thermal-red exception and pre-cleanup preservation gate
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
  * **Final hardware: ESP32-S3-Zero**, 4 MiB flash, 2 MiB embedded quad-SPI PSRAM
  * 1.28 inch TFT round LCD 240x240 GC9A01 driver
  * One wiring with serial logging enabled through native USB Serial/JTAG: SCL/SCK GPIO1, SDA/MOSI GPIO2, DC GPIO3, CS GPIO4, RST GPIO5; MISO is not connected. SCL/SDA are the module's SPI labels, not an I2C bus.
  * LCD backlight is hard-wired on; GPIO signals are 3.3 V even if the display module accepts 5 V power

* Change partition table to fit into the 4MB flash

## 2. Clarified decisions (owner answers, 2026-10-15)

| Topic | Decision |
|---|---|
| Printer models | **K1, K1C, K1 Max, K2 / K2 Pro / K2 Plus**. All of them report a chamber temperature (`boxTemp`). |
| Firmware update | **USB only.** No OTA: drop the OTA library, upload script, and second app slot. |
| Visual style | **Mixed:** KNOMI GIF animations for setup, idle, processing, self-test, and completed; data screens for printing and temperatures |
| Original-screen preservation (2026-11-10) | **All adaptable read-only screens** guide the hand-written UI. Closely preserve KNOMI geometry, large numerals, icons, ring framing and typography; add extra Creality information on rotating pages. Retain the original gray/red nozzle/bed heating fills unchanged; only decorative/theme/progress accents become green. **No new hot/cooldown thresholds or warnings.** Replace printing X/Y/Z sensor content with **remaining and elapsed time**. Source measurements, v1 runtime differences, assets and required render comparisons are in `docs/SCREENS.md`; generated screens are not deleted until that preservation gate is met. |
| GIF assets (2026-10-19; step mapping confirmed 2026-11-09) | Use the **green variants** `gif_*_green.c` instead of the original red `gif_*.c` wherever one exists; **`gif_creality.c` replaces `gif_voron.c`** (idle logo). `gif_qgling_green` is **kept** for self-test step 5, Bed leveling. UI accent follows the GIFs: **green**. |
| Physical input | **BOOT button on GPIO0** (on the board) is the only input. There is no touch screen and the backlight is hard-wired. |
| Final hardware (2026-11-08) | **ESP32-S3-Zero**, 4 MiB flash and 2 MiB embedded PSRAM; owner chip report identifies ESP32-S3 QFN56 revision v0.2, XMC flash, AP_3v3 PSRAM, 40 MHz crystal. One display map: SCK1/MOSI2/DC3/CS4/RST5. Native USB-C flashing/reset authorized on **COM8**. BOOT GPIO0, no touch/sensors/MISO/backlight control; printer `192.168.0.55:9999`. Allocate LVGL/GIF memory explicitly in PSRAM; retain internal DMA buffers. S3 build/flash, memory margin, rendering, ten-minute playback and repeated cold boots passed. |
| Printing layout | **Auto-rotating pages** (progress/time ↔ temperatures ↔ layer/file); a double-click jumps to the next page |
| GUI library | **LVGL 8.4.0** (last v8; its GIF decoder needs ~20 % less RAM than v9) |
| Web page security | **No admin password.** The device is LAN-only; risk accepted (see §11). |
| Language / units | English, °C, 24 h time (owner confirmed 2026-11-06) |
| Reference captures | Owner's **K1 Max** legacy sessions: full print with calibration, print without calibration, pause → resume → stop, connect during a print (DWIN sw 1.3.3.8 / 1.3.3.29), plus idle connect on 1.3.5.19. In addition, `test/fixtures/` contains **14** timed K1 Max 1.3.5.19 JSONL captures with reviewed sibling expected sequences. November 10 adds connecting during an existing error/recovering/resuming/completing/menu leveling, plus boot-time placeholder/file-list/input-shaping/leveling evidence. All captures have `tx:0`. Remaining gaps are deferred in protocol doc §7. |
| Target printer firmware | **Current Creality firmware (K1 family 1.3.5.x, K2 current)** is primary; 1.3.3.x remains a compatibility target based on legacy owner captures, which are not checked in as fixtures |
| Completed/Stopped screen time | Default **10 min**, configurable; 0 = until the next state change (owner confirmed 2026-11-06) |
| Validated implementation slice (2026-11-08) | Isolated `spikes/display`: **ESP-IDF 5.5.5 with Arduino 3.3.12 as a component**, LVGL 8.4.0, native IDF SPI2/GC9A01, PSRAM allocation and internal DMA strips. COM8 flash and network-loaded S3 validation passed; owner performs visible checks. |
| Build-system scope | ESP-IDF/CMake replaces PlatformIO for the isolated spike only. Full-firmware build, native-test, and CI migration decisions remain separate follow-up decisions. §5.1 remains the full-firmware draft. |
| Git operations (2026-11-06) | Work on the current branch; **no branch or tag creation**. |
| Display SPI speed (2026-11-08) | **40 MHz retained**, validated by the accepted native IDF S3 run. Higher speeds, such as 80 MHz, are untested and optional future improvements only if needed. |
| Network downloads | **No manual approval.** The network automatically processes packages after an initial attempt and a variable interval. Retry normally after processing; keep network controls and certificate checks enabled. Applies to clean builds/redownloads too. After several failed normal attempts with processing time allowed, report the **package name, version, exact attempted download URL, and error** to the user. Current versions are recorded in the spike lock file. |
| Playback stopwatch (S3-validated, 2026-11-08) | Zero-padded `MM:SS` below the GIFs; starts with the first GIF after network readiness, continues across switches, excludes the RGB startup period and resets on reboot. Owner confirmed visibility, continuity, ten-minute playback and repeated cold boots on the COM8 image. |
| Phase 0.4 owner revalidation (2026-11-10) | Owner accepted all **14** regenerated sequences after revising the prototype: separate nozzle/bed heating, first observed actual >= target completes that heater until re-armed, nozzle priority, idle `deviceState:3` leveling / `4` input shaping, and calibration preempting stale results. **Every nonzero error in valid data produces ERROR, including paused/printing; local acknowledgement cannot suppress it.** Self-test labels remain in protocol doc §5.3. Sixteen focused Python tests and all 14 replay comparisons pass; no production C++/firmware claim. |
| Post-print timing (2026-11-10) | New capture: raw `state:2 -> 0` after **733,479 ms (12 min 13.479 s)**, compared with about 63 s in an earlier fixture. Owner pressed the printer's completion **OK soon after finishing**, not after twelve minutes. No exact button marker: neither universal timeout nor OK-induced flag behavior is proven. Protocol doc §5.2b records the timeline and remaining uncertainty. |

## 3. Findings from the code review

The repository is the BTT KNOMI firmware: `knomiv2` (ESP32-S3, touch, sensors) and `knomiv1` (ESP32 **with 8 MB PSRAM**).
It uses SquareLine-generated LVGL 8.3.7 screens, polls Moonraker over HTTP, and provides an AsyncWebServer setup page.
The owner's working S3 net-clock reference and accepted native IDF spike validate the GC9A01 with the final GPIO1–5 wiring at 40 MHz. Retain this display path during migration; native USB GPIO19/20 are separate from the display pins.

### 3.1 Blockers for the target hardware (must fix)

| # | Problem | Where | Fix |
|---|---|---|---|
| B1 | LVGL allocation assumed the original board's PSRAM configuration | `include/lv_conf.h` `LV_MEM_CUSTOM_ALLOC` | Initialize and verify 2 MiB S3 PSRAM; use explicit `heap_caps_malloc`/`heap_caps_realloc` with `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`, and `heap_caps_free`. Fail visibly rather than silently consuming internal DMA/network heap. |
| B2 | Full-frame 240×240 draw buffer (115 KB) from PSRAM | `lvgl_hal.cpp` | Two 240×24-line DMA buffers (2×11.5 KB) |
| B3 | Legacy board profiles declare the wrong flash/PSRAM capacities | `buildroot/boards`, `platformio.ini` | ESP32-S3 target, 4 MiB flash, 2 MiB quad-SPI PSRAM, native USB logging, and the partition table (§6). Full-firmware PlatformIO profile remains a draft; spike uses IDF. |
| B4 | Legacy v1/v2 display pins differ from the new custom board; TFT_eSPI S3 rendering failed | `pinout_knomi_v1.h`, `pinout_knomi_v2.h`, display HAL | One map SCK1/MOSI2/DC3/CS4/RST5. Use accepted native IDF SPI2/software CS and the owner's working net-clock GC9A01 sequence. Failed TFT_eSPI FSPI/SPI3 attempts are historical; no production/debug pin switch. |
| B6 | Two GIF objects alive at the same time (welcome + main) → ~150 KB RAM | `lvgl_usr.cpp` | Exactly **one** GIF object in the whole UI (§8.4) |
| B7 | Legacy fonts were generated at **1 bpp** (jagged) | `src/ui/fonts` | Phase 0.5 regenerated the target 14/18/24/48 px Inter fonts at 4 bpp; unused 16/20/32 px legacy files remain for Phase 1 cleanup. |
| B8 | LVGL 8.3.7 needs a manual `lv_disp.c` patch (PR 4487) | `platformio.ini` comment, `lv_disp_(bugfix_backup).c` | LVGL 8.4.0 contains the fix; delete the backup file |

**Accepted display-bus reference:** the owner's net-clock project and repaired S3 spike run the GC9A01 using native IDF SPI2, mode 0 at 40 MHz, no MISO, software-controlled CS, and `SPI_DEVICE_NO_DUMMY`. Retain the tested reset/init sequence, MADCTL (`0x36`) `0x48`, RGB565 pixel format (`0x3A`) `0x55`, and swapped LVGL pixels. Owner-confirmed orientation/colors/playback pass; do not reintroduce the failed TFT_eSPI path during full-firmware migration.

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

### 3.3 Memory facts and S3 allocation policy

- **Current target:** 2 MiB PSRAM for LVGL objects and GIF decoder allocations; two 240×24 RGB565 DMA buffers remain explicitly internal (23,040 bytes). Network stacks/tasks and the fixed JSON/WS reserves remain unchanged; PSRAM is not a replacement for internal DMA memory.
- **SPI transfer behavior (verified source, 2026-11-08):** no application 32 KiB cap. ESP-IDF 5.5.5 validates transfers against the configured bus maximum; the accepted native SPI2 spike sets this to one **11,520-byte** strip. Retain two independent 240×24 RGB565 strips; extra PSRAM is for LVGL/GIF allocations, not larger internal buffers or full-frame transfers. Historical TFT_eSPI 2.5.43 requested a 65,536-byte bus maximum but chunked images above 16,384 pixels into blocking 1,024-pixel writes before queuing the remainder: library behavior, not a fixed IDF restriction, and no longer the active HAL.
- Measure internal and PSRAM total/free/largest/minimum heap independently, plus internal DMA free/largest blocks and completed-refresh FPS. Verify detected flash/PSRAM sizes at boot. A PSRAM allocation failure must not silently fall back to internal memory.
- **Measured S3 baseline:** network-ready internal free **203,499 bytes**, largest **131,072**; PSRAM free **2,090,348**, largest **2,064,384**. Under GIF/HTTP workload PSRAM largest remains **1,966,080**; final internal minimum **160,768**. See §10 and `CREALITY_WEBSOCKET_IMPL.md` for sample ranges and workload conditions.
- LVGL 8 GIF decoder (16-bit color) RAM: `4·W·H` (canvas and frame) plus a transient LZW table of up to ~24 KB.
- The tested direct-SPI clock project uses a full 240×240 RGB565 framebuffer (115,200 bytes). **Do not copy that buffer into KNOMI**; keep the two internal 240×24-line DMA draw buffers in B2 on the S3 too.
- Assets (re-measured 2026-10-19 from the GIF streams inside `src/gif/*.c`). KB = 1024 bytes. All of them loop forever, and the C arrays are `LV_IMG_CF_RAW_CHROMA_KEYED`. **Green variants are used wherever one exists** (§2). `gif_wifi` (white, neutral) and `gif_print_ok` (already green) have no `_green` file and are kept as they are.

| GIF (used) | Replaces | Size | Frames × delay = loop | Flash | RAM (4·W·H) | Use |
|---|---|---|---|---|---|---|
| gif_creality | gif_voron | 150×150 | 14 × 120 ms = 1.68 s | 83.1 KB | **87.9 KB** | idle logo (S5) |
| gif_standby_green | gif_standby | 174×51 | 60 × 30 ms = 1.80 s | 27.8 KB | 34.7 KB | idle (S5) |
| gif_wifi | – (kept) | 150×100 | 366 × 30 ms = 10.98 s | 35.6 KB | 58.6 KB | setup (S1) |
| gif_homing_green | gif_homing | 96×96 | 25 × 40 ms = 1.00 s | 43.1 KB | 36.0 KB | processing: homing (S6) |
| gif_probing_green | gif_probing | 155×123 | 30 × 40 ms = 1.20 s | 29.1 KB | 74.5 KB | self-test (S7) |
| gif_qgling_green | gif_qgling | 207×104 | 200 × 40 ms = 8.00 s | 86.1 KB | 84.1 KB | self-test step 5 "Bed leveling" (S7) |
| gif_heated_green | gif_heated | 176×82 | 80 × 30 ms = 2.40 s | 7.5 KB | 56.4 KB | heating finished → printing (S8) |
| gif_print_green | gif_print | 160×90 | 67 × 40 ms + 1 × 360 ms = 3.04 s | 45.7 KB | 56.2 KB | printing interstitial (S8) |
| gif_print_ok | – (kept) | 144×144 | 31 frames (mostly 30 ms) = 1.80 s | 6.9 KB | 81.0 KB | completed (S10) |
| gif_printed_green | gif_printed | 177×70 | 60 × 30 ms = 1.80 s | 82.6 KB | 48.4 KB | completed (S10) |
| **Total** | | | | **447.46 KiB (458,197 bytes)** | max **87.9 KiB (90,000 bytes)** | |

  Comment, plain-text, and XMP application extensions were stripped from these GIFs with `tools/strip_gif_meta.py` (≈ 1 KB each; `NETSCAPE2.0` is kept because LVGL's gifdec stops after one pass without it), then the C files were regenerated with `tools/bin2c.py`.

  Compared with the previous asset set, the change is **+85 KB flash** (`gif_creality` and `gif_qgling_green` added; `gif_homing_green` is 85 KB smaller than `gif_homing`). The peak RAM does not change: `gif_creality` has the same 150×150 canvas as the `gif_voron` it replaces, and the same peak as `gif_print`.
- Deleted: `gif_voron` (Voron branding) and the original red `gif_heated`, `gif_homing`, `gif_print`, `gif_printed`, `gif_probing`, `gif_qgling`, and `gif_standby` (replaced by their `_green` variants).
- **Rule:** at most one GIF is decoded at any time, and **max canvas ≤ 90 KB**. After full init (Wi-Fi, web server, and WS connected), the largest free block in the **actual GIF allocation heap (PSRAM)** must be ≥ 1.25 × the largest canvas (**112,500 bytes** for the 90,000-byte estimate of `gif_creality`). Check before first load and repeated swaps under HTTP traffic; track internal/DMA headroom separately. The S3 run passes this margin with the original 150×150 Creality asset; no downscale is required by this gate.
- **Phase 0.5 verification:** `python tools/check_assets.py --compile` checks all ten source GIFs against their C arrays, decodes every frame, verifies descriptors/looping/budgets, checks the four fonts and compiles all 14 C assets against the configured S3/LVGL flags. Source GIFs already match, so no regeneration or downscaling was needed. See `tools/fonts.md` for pinned tools and exact commands; all-asset device playback remains separate.
- **Original heater art retained (2026-11-10):** keep the four native gray/red heating images (**167,652 payload bytes**) plus the small nozzle/bed icons (**4,683 bytes**), total **172,335 bytes**. This static-image payload is additional to the GIF-only budget, not a new framebuffer or measured runtime allocation. Combined GIF/six-image payload is **630,532 bytes**, excluding fonts/code/descriptors. Recheck full-firmware flash after integration; avoid copying the 172,800-byte decorative background when close primitives can reproduce it after visual comparison. See `docs/SCREENS.md` for exact dimensions and reference-only assets.

## 4. Target architecture

```
                    core 0                                   core 1
 ┌────────────────────────────────────────────┐   ┌────────────────────────────┐
 │ net_task (prio 5)                          │   │ ui_task (prio 4)           │
 │  WifiManager  (STA/AP state machine, mDNS, │   │  DisplayHal (IDF SPI2 DMA) │
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
  tft_setup.h                 panel pins, dimensions, SPI frequency for native HAL
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
  ui/assets/images/*.c        original gray/red heater artwork (large native images; separately budgeted)
lib/Button/                   kept (GPIO0)
lib/creality_model/           PrinterModel: merge, coercion, state derivation (no Arduino deps)
lib/ws_stream/                RFC 6455 client-side frame parser/encoder (no Arduino deps) + thin WiFiClient adapter
web/index.html                editable source of the settings page
test/test_creality_model/     Unity tests (native)
test/fixtures/*.jsonl         14 captured printer traces with reviewed sibling *.expected.txt sequences (Phase 0.4 / 4.3)
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
- **Frame numbers:** `#n` counts the `rx` messages whose text starts with `{`, from 1 to the end of the file across reconnects. These numbers appear in `*.expected.txt` and in the logger display. The protocol doc's legacy frame references are from separate captures and are not fixture replay baselines.
- **Legacy captures:** converted legacy captures have no `t`. Replays then use a fixed spacing of 250 ms (the printing frame rate).
- **Compatibility:** readers ignore unknown event types and fields. `version` changes only on incompatible changes.
- **Limitation:** the browser API delivers whole messages, not wire frames. Fragmentation, ping/pong, and the handshake are not recorded; the fake printer scripts these (Phase 4.5).
- **Sizes:** about 1.5 MB per print hour plus the large list frames. Fixtures are committed as they are, without trimming, so that they match the real traffic.

## 5. Platform and libraries

| Component | Current | Target (pinned) | Notes |
|---|---|---|---|
| PlatformIO platform | `espressif32@6.4.0` (Arduino 2.0.x) | **pioarduino `55.03.312`** (Arduino-ESP32 **3.3.12**, ESP-IDF 5.5.5) | The official `platformio/espressif32` (v7.1.3) still ships Arduino 2.0.17. **Fallback:** `espressif32@7.1.3` if Phase 0.1 fails. |
| Board | Legacy `esp32r8` / `esp32s3r8` profiles | ESP32-S3-Zero, 4 MB flash, 2 MB quad-SPI PSRAM | IDF spike first; validate exact full-firmware board overrides before migration |
| LVGL | 8.3.7 + manual patch | **8.4.0** | Delete `lv_disp_(bugfix_backup).c` |
| Display HAL | TFT_eSPI 2.5.0; historical spike 2.5.43 | **Native ESP-IDF SPI2 / GC9A01** | S3 spike rendering accepted using owner's working net-clock reset/init sequence and queued DMA strips. TFT_eSPI remains historical/unused in spike; full-firmware build migration is separate. |
| ArduinoJson | ^6.19.4 | **7.4.3** | Use `JsonDocument` with a `DeserializationOption::Filter` |
| WebSocket client | – (HTTPClient) | **Own minimal streaming client** (`lib/ws_stream`, ≈ 400 LOC) over `WiFiClient` | Checked-in K1 Max 1.3.5.19 fixtures contain unsolicited 23.4 KB `historyList`, up to 28.7 KB `elapseVideoList`, and two **79,463-byte `retGcodeFileInfo2`** frames during November 10 boot, before `connect:1`. File-list pushes are not observed on every connection. `links2004/WebSockets` 2.7.2 closes the connection (1009) above `WEBSOCKETS_MAX_DATA_SIZE` (15 KB default) and `malloc`s whole frames. Our client uses a static 4 KB buffer and stream-discards larger frames (protocol doc §4). |
| Web server | ottowinter ESPAsyncWebServer-esphome 3.0 | **ESP32Async/ESPAsyncWebServer 3.12.1** + **ESP32Async/AsyncTCP 3.5.0** | Maintained fork (3.12.1 requires AsyncTCP ^3.5.0) |
| OTA | bigtreetech/AsyncElegantOTA | **removed** | USB-only decision |
| LIS2DW12, Adafruit_SHT4X, CST816S | v2 only | **removed** | |
| Core libraries used | – | WiFi, WiFiClient, DNSServer, ESPmDNS, Preferences, esp_sntp, mbedtls (SHA-1/base64 for the WS handshake) | |

### 5.1 `platformio.ini` (target draft)

The generic S3 board and memory/USB overrides below are **not yet full-firmware build-validated**. They must match the accepted IDF spike's 4 MiB flash/2 MiB quad-SPI PSRAM configuration before migration; do not use the board profile's default flash capacity or octal PSRAM settings.

```ini
[platformio]
default_envs = knomi

[common]
lib_deps =
    lvgl/lvgl@8.4.0
    bblanchon/ArduinoJson@7.4.3
    ESP32Async/AsyncTCP@3.5.0
    ESP32Async/ESPAsyncWebServer@3.12.1

[env:knomi]
platform  = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.312/platform-espressif32.zip
board     = esp32-s3-devkitc-1
framework = arduino
board_upload.flash_size = 4MB
board_build.arduino.memory_type = qio_qspi
board_build.flash_mode = dio
board_build.f_flash = 40000000L
board_build.partitions = partitions_4MB.csv
monitor_speed   = 115200
monitor_filters = esp32_exception_decoder
extra_scripts   = pre:tools/build_web.py
lib_deps  = ${common.lib_deps}
build_flags =
    -Iinclude -Isrc
    -DLV_CONF_INCLUDE_SIMPLE
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
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
| `LV_MEM_CUSTOM` | 1; `heap_caps_malloc` / `heap_caps_realloc` with `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`, `heap_caps_free` | Put LVGL/GIF allocations in verified S3 PSRAM, preserve internal DMA/network heap (B1). |
| `LV_COLOR_DEPTH` / `LV_COLOR_16_SWAP` | 16 / **1** | DMA flush pushes pre-swapped pixels (remove the SquareLine `SWAP 0` check) |
| `LV_DISP_DEF_REFR_PERIOD` | 20 ms | Smooth GIFs without starving Wi-Fi |
| `LV_USE_GIF`, `LV_USE_QRCODE` | 1 | Setup QR codes |
| Widgets | keep label, arc, img, bar, spinner, line, canvas (QR); retain slider only if needed as a non-interactive image-backed heater fill; **disable** roller, meter, keyboard, chart, calendar, colorwheel, imgbtn, msgbox, tabview, tileview, win, list, menu, span, spinbox, table, textarea | Preserve read-only visuals without printer-control/touch widgets |
| Themes | default only (disable basic/mono) | |
| Built-in fonts | Montserrat 16 (for `LV_SYMBOL_*` glyphs) | Inter fonts handle text |
| `LV_USE_LOG` | 0 (release) / WARN in debug | |

## 6. Partition table (4 MB, USB-only)

Replace `spiffs_16MB.csv` with `partitions_4MB.csv`. Keep the tested reference project's NVS (`0x9000`) and PHY (`0xf000`) offsets; reserve the final 64 KiB for KNOMI coredumps, so the app partition is 64 KiB smaller than the reference project's full-size factory app:

```csv
# Name,   Type, SubType,  Offset,   Size,     Flags
nvs,      data, nvs,      0x9000,   0x6000,
phy_init, data, phy,      0xf000,   0x1000,
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

- **Visual reference:** `docs/SCREENS.md` records original source geometry and runtime changes. Preserve recognizable layouts rather than replacing them with generic bars/rows. New dense text defaults to the centered 170×170 safe area; original centered heater-value anchors require round-mask validation rather than a blanket inset. Printing uses the original 212/230/240 px concentric rings, not a single forced outer-radius geometry.
- **Fonts** (Inter SemiBold, 4 bpp, regenerated):
  - 14 and 18 px: printable ASCII + Latin-1 + Latin Ext-A, so that accented filenames render; upstream Inter excludes U+00AD soft hyphen and deprecated U+0149 (documented in `tools/fonts.md`)
  - 24 px: printable ASCII plus degree and Celsius signs
  - 48 px: digits, `% : ° C`, space, and the Celsius sign retained for compatibility
  - LVGL Montserrat 16 for symbols
- **Colors:**
  - KNOMI green `#329A40` (accent and progress; matches the green GIFs)
  - original red pixels for nozzle/bed **heating-fill images**, with gray base art; exempt from all theme/GIF palette conversion. This preserves heating graphics only, not a new temperature-threshold/cooldown warning
  - original warm yellow `#FFD164` for emphasized actual temperatures and large progress numerals; preserve the KNOMI numeric hierarchy
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
| `ERROR` | S11 Error, including while paused/printing | Printer clears errcode to 0, or link/readiness changes; job activity/local acknowledgement alone never suppresses ERROR |
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
| S5 Idle | Page A: `gif_creality` (Creality logo). Page B: `gif_standby_green`. Page C: read-only adaptation of the original paired nozzle/bed panels, preserving icons and actual-over-target hierarchy; use the removed Preheat-button area for chamber data if the render fits, plus "Last: <file> ✓/✕" when applicable. Pages rotate every `page_secs`; A → B swaps the source of the single GIF object. **If any target > 0 while idle**, keep page C visible. Label "Nozzle heating" or "Bed heating" until that heater first reaches target, nozzle first; after both reach target keep temperatures visible without a false heating label on later dips. No touch/preheat controls. |
| S6 Processing | Nozzle/bed heating uses the original centered large gray/red icon-fill presentation: 24 px actual above and target below, native asset scale, selected heater first. This supersedes the generic two-bar heating draft. Red fill is clamped actual/target, never recolored green; unknown/zero target is handled safely. For Homing/Bed leveling/Input shaping/Preparing/Stopping/Resuming, keep the operation identifiable and expose the selected heater without losing step/operation information; combined layout requires preview per `docs/SCREENS.md`. Use `gif_homing_green` for homing. Idle `deviceState:3/4/7` preempts and clears a stale result latch even when the previous filename/progress 100 remain. |
| S7 Self-test | `gif_probing_green` in the center, switching to `gif_qgling_green` while `withSelfTest` = 5 ("Bed leveling"). Label "Self-test · step N" plus the name in protocol doc §5.3, the selected pending heater (nozzle first), and nozzle/bed temperatures (small). `withSelfTest` is a **step number** (1–5, then 100), not a percentage. The outer ring shows 5 step segments, with completed steps filled. |
| S8 Printing | Preserve the original 48 px warm-yellow percentage, 212 px / 20 px progress arc and 230/240 px decorative rings; theme arcs green. The sensor-free v1 runtime centers the percentage (generated v2 y=50 is not the whole reference). P1 adds compact **remaining and elapsed time** below the percentage instead of X/Y/Z sensor bars, plus job status/selected pending heater; exact spacing follows the render guide. P2 adapts original nozzle/bed temperature panels with chamber. P3 shows layer/total, filename and optional fan/speed. Keep auto-rotation and the progress frame on data pages. On processing → printing, the existing heated/print GIF interstitial sequence remains. **Paused:** same composition with amber state styling. **Any nonzero error switches to S11**, even while paused/printing. |
| S9 Stopped | Grey ring, ✕ symbol, "Print stopped" (or "Print failed" for state 3), filename, "at 57 %", elapsed time |
| S10 Completed | `gif_print_ok` (1.8 s, one loop) → `gif_printed_green` (≈ 7 s, 4 loops of 1.8 s) → summary: light-green ring, ✓, "Completed", filename, total time. On the summary, the button double-click replays the animation. |
| S11 Error | Red ring, ⚠ symbol, "Printer error", code/key, and printer model. Takes precedence over all ready-printer job/calibration states. A click marks the error seen locally but leaves logical ERROR and the fault visible while errcode is nonzero; it never clears the printer's error or sends a command. Captures prove printer-side clearing during stop and manual recovery/resume, not a universal clearing rule. |
| S12 Info | Device IP, hostname, RSSI, printer host:port, link state, printer model and firmware ("update available 1.3.5.22" from `linuxOtaInfo`, if pushed), FW version, free heap / largest block |

### 8.3 Button (GPIO0, active low, internal pull-up)

| Action | Effect |
|---|---|
| Click | Show/hide S12 Info; on S11 mark the error seen locally without clearing/suppressing active ERROR |
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
- Silence watchdog: 10 s → send the `ReqPrinterPara` probe; 30 s → disconnect. Idle traffic observed is about 1 frame/s on 1.3.3.x and 1 frame every 5 s on 1.3.5.19; the 10 s probe leaves margin for current-firmware jitter.
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

**S3 acceptance criteria:** build/COM8 flash, native USB boot logs, detected 4 MiB flash/2 MiB PSRAM and memory test, internal/PSRAM/DMA heap and FPS with Wi-Fi/web/WS active, five diagnostic requests, and repeated GIF swaps using the original assets. Require PSRAM largest block ≥112,500 bytes before Creality loads, stable headroom with no decoder failure/reset, and owner-observed RGB/orientation, animations, ten-minute playback, and cold boots. These criteria passed on 2026-11-08. Long-term soak, other assets and oversized-message coverage remain separate gates.

**Accepted S3 results (native IDF repair):** application **1,269,200 bytes**, COM8 hashes verified; 4 MiB flash, 2 MiB PSRAM at 40 MHz with passing memory test, 2 cores / 240 MHz. Replaced failing TFT_eSPI with native SPI2/software CS, using the owner's exact-wiring net-clock reset/init sequence and queued 11,520-byte DMA strips. Serial evidence reaches **1,554.7 s (~25.9 min)**, one boot/connection attempt, 369 messages, no panic/watchdog/decoder failure or JSON errors. Five HTTP requests passed; PSRAM largest block 2,064,384 before first GIF / 1,966,080 under GIF/HTTP workload; final internal minimum 160,768. Creality ~9 FPS, standby ~32 FPS. Owner confirms correct rendering, continuous stopwatch, **ten-minute playback and repeated cold boots pass**. **Phase 0.3 S3 rerun passed**; other assets, 24-hour soak and oversized-frame coverage remain separate gates. Earlier failed TFT_eSPI results are preserved as history in `CREALITY_WEBSOCKET_IMPL.md` and `docs/ARCHITECTURE.md`; no later phase has been started.

| # | Task | Size | Exit criteria |
|---|---|---|---|
| 0.1 | **S3 display validation passed, 2026-11-08:** isolated ESP-IDF 5.5.5 + Arduino 3.3.12 + LVGL 8.4.0/native SPI2 spike on final GPIO1–5 wiring, COM8. Two internal 240×24 DMA buffers, PSRAM-backed LVGL/GIF, one GIF object and continuous stopwatch. Validated as part of the network-loaded S3 run, not a separate display-only benchmark. | M | **Passed:** verified build/flash, colors/orientation, both animations, stopwatch, owner-observed ten-minute playback and repeated cold boots. Serial evidence reaches 1,554.7 s; no separate cold-boot trace is claimed. |
| 0.2 | **40 MHz accepted on S3, 2026-11-08.** Retain the validated native SPI2 configuration; no speed increase now. | S | Owner-observed ten-minute playback without reported artifacts. Higher speeds remain untested and optional; test stability before adopting any increase. |
| 0.3 | **S3 network-loaded validation passed, 2026-11-08:** Wi-Fi STA, ESPAsyncWebServer 3.12.1 / AsyncTCP 3.5.0, read-only WS to **192.168.0.55:9999**, one GIF and 40 MHz SPI. ArduinoJson 7.4.3 static 8 KiB arena; 4 KiB WS buffer. Menuconfig credentials; COM8 flash verified. Captured ~25.9 min with 369 messages, one connection attempt, zero JSON errors; five HTTP requests passed. | M | **Margin passed:** PSRAM largest 2,064,384 before first GIF / 1,966,080 under GIF/HTTP workload vs 112,500 required. Original assets retained; final internal minimum 160,768. Rendering, ten-minute playback and repeated cold boots passed. Other assets, 24-hour soak and oversized frames remain separate validation; production client/model hardening remains Phase 4. |
| 0.4 | **Completed, extended and revalidated 2026-11-10:** 14 K1 Max 1.3.5.19 captures/sibling baselines are owner-approved. New captures verify an error present on initial connect, clearing before resume, completion/raw idle/menu leveling, boot `connect:0 -> 1`, two unsolicited 79,463-byte file lists, idle input shaping and 36-point leveling. Revised Python prototype: first-reach nozzle/bed heating with nozzle priority, calibration clears stale result latch, and every valid nonzero error overrides paused/printing. | M | **14/14 baselines match fresh replay; 16 Python regression tests pass.** Timing evidence and the owner's early completion-OK press are recorded separately; button effects/universal timeout remain unresolved. No firmware flash, production C++ implementation, fake printer or live oversized-frame device test. Other gaps remain in protocol doc §7. |
| 0.5 | **Completed 2026-11-09:** verified all ten selected GIF sources already match their C arrays; regenerated 14/18/24/48 px Inter SemiBold fonts at **4 bpp**, preserving symbols. Pinned Inter 4.1 source/OFL, converter 1.5.3, glyph ranges/exclusions and regeneration commands in `tools/fonts.md`; repeatable validation in `tools/check_assets.py`. | S | **Passed:** all GIF frames decode/loop; GIF payload **458,197 bytes (447.46 KiB) ≤ 470 KiB**, maximum canvas/frame estimate **90,000 bytes**. Four fonts pass hash/4-bpp/glyph checks; **14/14 C assets compile-check** with accepted S3/LVGL flags. No new firmware image, flash or all-asset physical playback is claimed; UI integration and soak remain later work. |

### Phase 1: Cleanup (remove everything not required)

| # | Task | Size |
|---|---|---|
| 1.1 | **Superseded by owner decision 2026-11-06:** retain current branch; do not create a branch or tag | S |
| 1.2 | First satisfy `docs/SCREENS.md` preservation gate: retain required art, render selected original screens or obtain photos, save clearly labeled **original** references, and resolve retain/redraw strategy. Then delete per §10.1; do not remove generated evidence before it is preserved. Target comparison renders follow in Phase 5, not as a prerequisite for cleanup | S |
| 1.3 | Temporary `main.cpp` that boots the display only; build green | S |

#### 10.1 Removal and keep inventory

| Path | Action | Reason |
|---|---|---|
| `src/moonraker.{h,cpp}` | delete | Replaced by the Creality client |
| `src/pinout*.h` | delete → `board_pins.h` | v2 removed; v1 pins wrong (B4) |
| `src/lis2dw12.cpp`, `src/sht4x.cpp`, `lib/CST816S/` | delete | v2-only sensors and touch |
| `src/lvgl_hal.{h,cpp}` | rewrite → `ui/display_hal` | Remove touch, AW9346 backlight pulse code, and the PSRAM buffer |
| `src/lvgl_usr.{h,cpp}` | rewrite → `ui/ui_manager` | Extrude/roller logic not needed |
| `src/ui/` (SquareLine output: `ui.c/h`, `ui_helpers`, `components/`, `screens/*`, `images/*`, `CMakeLists.txt`, `filelist.txt`) | delete **only after** reference/render preservation and moving the explicit keep assets/fonts below | Missing project does not make the generated visual evidence useless. New screens are hand-written to the measured guide; printer-control/touch callbacks are not ported |
| `src/ui/images/ui_img_nozzle_png.c`, `ui_img_bed_png.c` | **keep → `ui/assets/icons/`** (24×30 and 29×29) | Small icons; 4,683 payload bytes combined |
| `src/ui/images/ui_img_nozzle_gray_png.c`, `ui_img_nozzle_red_png.c`, `ui_img_bed_gray_png.c`, `ui_img_bed_red_png.c` | **keep → `ui/assets/images/`**, native dimensions and red pixels unchanged | Essential original heating visuals; 167,652 payload bytes combined, not subject to the small-icon limit |
| Other original framing/background/network/notification images | Preserve exported reference images; decide retain versus close native-drawn equivalent with comparison renders **before deletion** | Avoid indiscriminate removal or copying large decorative bitmaps; measured candidates in `docs/SCREENS.md` |
| `src/ui/fonts/*` | Keep/move regenerated 4-bpp Inter 14/18/24/48; remove unused legacy fonts after reference preservation | Preserve typography hierarchy without restoring the missing SquareLine project |
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
| 2.2 | `board_pins.h`, display setup (GC9A01; native IDF SPI2/software CS; one map SCK1/MOSI2/DC3/CS4/RST5; native USB logging; no MISO/BL; accepted 40 MHz/reset/init sequence, MADCTL `0x48`, pixel format `0x55`, inversion on). | S |
| 2.3 | `display_hal`: native IDF initialization, two internal 240×24 DMA buffers, queued SPI transactions; hold CS through command/pixels, release only after completion and then `lv_disp_flush_ready`. Initialize/verify PSRAM before LVGL. Add settings-based rotation with validated panel orientation; no alternate pin map or full-frame buffer. | M |
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
| 4.2 | `derive_state()` per protocol doc §5.4/5.5, including separate first-reach heater latches/nozzle priority, calibration preempting stale results, unconditional ready-printer ERROR precedence, sub-label, debounce, freshness, local acknowledgement without suppressing ERROR, and new-job detection | M |
| 4.3 | Unit tests (`test/test_creality_model`): coercion (number / string / "" / null / "007"), delta merge, every rule in protocol doc §5.4, and **replay of each fixture against a reviewed `*.expected.txt` sequence**. The protocol doc §5.5 legacy sequences are not the fixture baselines; generate the baselines from the checked-in 1.3.5.19 JSONL with `tools/derive_prototype.py`, review them, and commit them during Phase 0.4. The C++ port must produce the identical sequence for every fixture.<br>The replay harness reads §4.2 files with ArduinoJson:<br>• Each `rx` text goes through the same ≤ 4 KB limit as the client.<br>• The injected clock advances to each event's `t` (250 ms steps for legacy files), so debounce and freshness are tested with the real timing.<br>• A `close` event acts as a disconnect: `reset()`, then `OFFLINE`, as `derive_prototype.py` does. | M |
| 4.4 | `lib/ws_stream` frame parser/encoder + native unit tests (protocol doc §4, plan §9.2), then the `creality_client` adapter: connect/resolve, handshake, heartbeat, silence watchdog, backoff, JSON filter with the static arena, snapshot publish, statistics (frames, skipped large frames with their first key, reconnects, max frame size) exposed in `/api/status` | L |
| 4.5 | `tools/fake_printer.py`: replays §4.2 capture files at 1–50× speed, including checked-in 23–28.7 KB history/timelapse frames and the boot capture's **79,463-byte file lists / connect:0 placeholder**. Also test synthetic frames larger than the observed maximum.<br>• Each `open`…`close` segment serves one client connection; `rx` events are sent at their recorded `t`.<br>• A server-side `close` closes the socket with the recorded code.<br>• A `connect_failed` gap refuses connections for its duration (printer reboot).<br>• `tx`, `note`, and `error` events are skipped, and notes are echoed to the console.<br>Scripted faults (silence, close, oversized frame, fragmented frames, blank fields, `ok` frames, heartbeat on/off) | S |

### Phase 5: UI screens and state machine

| # | Task | Size |
|---|---|---|
| 5.1 | `theme`: role-specific colors/fonts/styles and reference-derived widgets; green decorative/progress arcs, native red heater fills excluded from recoloring, warm-yellow primary numerals. Follow `docs/SCREENS.md` | S |
| 5.2 | `ui_manager`: screen FSM per §8.1, page rotation, result timeout, interstitial GIF sequencing, transition guard, single GIF object | L |
| 5.3 | Screens S5–S12 per §8.2 and `docs/SCREENS.md`; closely match original read-only compositions, compare LVGL renders, then verify on S3. New fields/pages must not crowd the original heating/progress layouts | L |
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
| Full-firmware display migration regresses accepted S3 behavior | Display broken | Retain native IDF SPI2/software CS, tested reset/init and swapped RGB565; rerun visible playback/cold-boot gates after migration. Historical TFT_eSPI path failed rendering. |
| Heap fragmentation from repeated GIF switching | GIF fails to load after hours | One GIF object; similar canvas sizes (35–88 KB); fallback icon; 24 h soak; downscale GIFs if needed. The idle rotation swaps `gif_creality` (88 KB) ↔ `gif_standby_green` (35 KB) every `page_secs`, so the soak test runs mostly idle. |
| Printer firmware differences in state codes | Wrong screen | K1 Max fixtures verify `state:7` (stopping) and `state:9` (pre-job transient), in addition to the documented print states; unknown codes → PROCESSING if a file is set, else IDLE; log unknown codes. **K2 captures still needed.** |
| Large unsolicited list frames (23.4–28.7 KB history/timelapse and 79,463-byte file lists in fixtures) | Reconnect churn or heap exhaustion | Own streaming client skips frames > 4 KB without allocating (§9.2); replay observed frames and larger synthetic cases; live device validation is still required |
| Placeholder dump on 1.3.5.x (`connect:0`, zero/empty/invalid values) | Invalid temperatures, hidden chamber or false IDLE during startup | Boot fixture verifies NOT_READY until `connect:1`, with capabilities arriving first. Re-evaluate on every merge; one proposed `ReqPrinterPara` at 0 -> 1 remains an unverified safety net for active-job fields, not a captured request |
| Stale error or result after a reconnect | Misleading screen | Freshness rules (§5.5); never suppress a valid nonzero ERROR because a job is paused/printing or locally acknowledged. A new idle calibration clears the old result latch |
| Concurrent-client limit on the printer | KNOMI or Creality Print disconnected | Verify in Phase 0.4; only one socket from KNOMI; backoff |
| Creality firmware update changes the protocol | Data missing | Tolerant parser (unknown/missing fields → `--`); fixtures make regressions visible |
| No auth on the web page (owner decision) | Anyone on the LAN can change settings | LAN-only device, JSON-only POST, no secrets returned; can be added later |
| Asset licensing: BTT KNOMI has no LICENSE; reference projects are (A)GPL; `gif_creality` shows the Creality logo (trademark) | Distribution issues | Personal/private use of the BTT GIFs and the logo animation; implement the protocol from facts, no copied code; Inter font is OFL (include the license) |
| S3 GPIO3 is a strapping pin used for display DC | External pulls could affect the strap sampled at reset | Repeated cold boots passed with the attached display; recheck after hardware/pull changes. Use native USB Serial/JTAG on GPIO19/20 for logs. |
| GPIO0 held at power-up | Boots into the ROM bootloader | Documented in the README |
| Long browser capture lost or cut (tab closed, laptop sleeps, Wi-Fi drop) | Missing or incomplete fixture | The logger warns before the page closes with unsaved events, holds a screen wake lock, and auto-reconnects (gaps are recorded as `close`/`connect_failed`). Save intermediate copies during long prints. |

## 12. Documentation deliverables

| Document | Content | Phase |
|---|---|---|
| `docs/CREALITY_WS_PROTOCOL.md` | ✅ written and revised with legacy and checked-in K1 Max evidence (state 2 = completed; fixture-verified states 7/9; large-frame handling; fixture limitations). Extend with K2 findings and update as remaining captures resolve open points. | 0.4 |
| `README.md` | Overview, photo, features, supported printers, hardware and wiring, build and flash (PlatformIO + ESP Web Flasher), first setup walkthrough, button actions, troubleshooting (Wi-Fi, printer unreachable, blank screen, colors inverted), licenses and credits | 8.1 |
| `docs/HARDWARE.md` | S3-Zero 4 MB/2 MB configuration, single GPIO1–5 SPI pin table, native USB logging, GPIO3/GPIO0 strapping, 3.3 V signal/5 V power caveat, hard-wired backlight, GC9A01 SPI mode/CS/40 MHz settings, enclosure notes | 0.1 / 8.1 |
| `docs/ARCHITECTURE.md` | Tasks and cores, data flow, snapshot and queue contracts, memory budget (Phase 0.3 numbers), Phase 0.1 platform decision, module responsibilities | 0.3 / 8.1 |
| `docs/SCREENS.md` | **Written 2026-11-10:** source-derived measurements/mapping for all 12 adaptable screens, v1 runtime progress override, approved styling/thermal-red exception, measured keep assets and pre-cleanup gate. Full-screen native LVGL previews/device photos and final composed layouts remain pending, not claimed as generated screenshots | Before 1.2 / 5.3 |
| `docs/TESTING.md` | Native tests, fake printer usage, capture walkthrough (`tools/WebSocketLogger.html`, scenarios, markers, naming; §4.2), HIL checklist (§13) | 4.5 / 7.4 |
| `tools/fonts.md` | Written: pinned Inter 4.1/OFL, converter commands, glyph ranges/exclusions, GIF regeneration and repeatable S3 compile-check instructions | 0.5 |
| `CHANGELOG.md` | From 2.0.0-creality onward | 8.3 |

## 13. Test and acceptance

### 13.1 Automated

- **Current Python reference checks:** `python -m unittest discover -s test -p test_derive_prototype.py -v` passes 16 focused tests. All 14 owner-approved baseline files match fresh `tools/derive_prototype.py` replay. They assert pre-debounce logical sequences, not wall-clock timing, physical UI transitions or production C++ behavior.
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
| H9 | Nonzero printer error, including initial connect while paused and while printing | S11 ERROR always visible while the valid error remains nonzero; a local acknowledgement never clears/suppresses it. Printer clears error -> derive the current job state |
| H10 | KNOMI reboot after a print finished hours ago | Idle with "Last: … ✓", **not** the Completed screen |
| H11 | Creality Print / Home Assistant connected at the same time | Both keep working |
| H12 | Button: click / double / hold 5 s / hold 15 s | Info / next page / setup mode / factory reset |
| H13 | Accented filename (e.g. `Fogaskerék_PLA.gcode`) | Rendered correctly and scrolling |
| H14 | 24 h soak while idle + one long print | No reset; heap stable; GIFs always load |
| H15 | K1 Max 1.3.5.19 boot/connect and job start/end, or fake-printer replay of captured history/timelapse/79,463-byte file lists | No WS reconnect; skipped-frame counter increases. The boot fixture contains the list push, but a live push is not required on every connection |
| H16 | Print with calibration vs without | Self-test screen shows steps 1–5 vs 1–4 with names |
| H17 | Replay the boot fixture's two 79,463-byte lists and placeholder, plus a larger synthetic frame; repeat live startup when practical | Oversized frame discarded without reconnect. NOT_READY hides zero/empty/stale fields until `connect:1`; capabilities discovered in earlier deltas remain usable. Do not infer request behavior from receive-only captures |
| H18 | Repeated cold boots on S3-Zero with the single GPIO1–5 display map and native USB logging enabled | Display resets and firmware starts reliably; USB logs return after re-enumeration; 4 MB flash and 2 MB PSRAM initialized each boot |
| H19 | Menu-started leveling/input shaping after a completed print, with old job fields retained | DeviceState 3/4 selects the corresponding PROCESSING operation, clears stale result latch, keeps selected nozzle/bed heater sub-label until first target reach, and returns IDLE when deviceState becomes 0 |
| H20 | Original-visual comparison: nozzle/bed fill 0/25/50/100%, overshoot/unknown/zero target; print 0/57/100% and paused; short/long times and extra pages | Matches the source/reference compositions in `docs/SCREENS.md`; native heater fill remains red, decorative arcs green, warm-yellow numerals legible, no clipping/overlap, no old sensor/control widgets or added cooldown threshold behavior |

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
