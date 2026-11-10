## Root Firmware Checkpoint (2026-11-10)

The owner approved **ESP-IDF 5.5.5 / CMake for the full firmware**, with Arduino 3.3.12 as a component and LVGL 8.4.0. The root project now has a portable registry lock and separate ignored build/configuration/component output. PlatformIO and the obsolete Moonraker/control/touch/sensor application have been removed.

The current application is deliberately **display-only**: capacity checks, one 8 KiB UI task pinned to core1, native SPI2 GC9A01, two internal 240x24 RGB565 DMA strips, explicit LVGL PSRAM allocation, a 1.5-second splash followed by RGB/caption/uptime and heap logs. Panel initialization bytes match the accepted spike. The rotation API drains DMA and changes panel MADCTL on the UI task, never allocating software-rotation buffers. No application network task, settings, portal or printer client runs yet; settings-to-UI rotation dispatch remains future work.

Owner flashed/accepted the **440,432-byte Phase 1 RGB/uptime** image. The new **467,616-byte Phase 2** image (88% headroom) compiles/links but is not yet hardware-validated. Rotation mappings pass native CTest; the shared-source splash passes host bounds/pixel checks. Those checks do not prove physical rotation or boot timing. Generated UI evidence is preserved as 28 native captures plus a hash-verified 75-file source bundle. Required assets remain under the new tree, with optional art unlinked pending visual review.

See [README.md](../README.md) for root build commands, [docs/SCREENS.md](SCREENS.md) for visual preservation and [CREALITY_WEBSOCKET_IMPL.md](../CREALITY_WEBSOCKET_IMPL.md) for exact evidence. The following sections describe the separate historical/accepted spike workload.

# Architecture and measured budgets

## Final hardware (2026-11-08)

ESP32-S3-Zero with 4 MiB embedded flash and 2 MiB quad-SPI PSRAM; GC9A01 240 x 240 panel. One SPI map: module SCL = SCK GPIO1, SDA = MOSI GPIO2, DC GPIO3, CS GPIO4, RST GPIO5. No MISO, touch/sensors, or backlight control; BOOT remains GPIO0. Native USB Serial/JTAG uses the board's USB-C (GPIO19/20); flashing/reset authorized on COM8. GPIO3 is an S3 strapping pin: owner-confirmed repeated cold boots passed with the display attached; recheck after hardware/pull changes.

The Phase 0.3 spike initializes/tests PSRAM and allocates LVGL objects and GIF decoder memory explicitly using `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`, without internal fallback. Two 240 x 24 draw buffers remain `MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA`. Keep the validated 40 MHz display SPI and original 150 x 150 Creality GIF. Serial and `/api/status` report internal heap, PSRAM, and internal DMA headroom separately; startup verifies 4 MiB flash and 2 MiB initialized PSRAM.

No application 32 KiB SPI cap is required: ESP-IDF 5.5.5 checks the configured bus maximum, set to one 11,520-byte strip in the accepted native HAL. Retain independent DMA strips to preserve internal heap; PSRAM capacity does not justify a full-frame internal buffer. Historical TFT_eSPI S3 setup requested 65,536 bytes but chunked images above 16,384 pixels into blocking writes before DMA; this is library behavior, not a fixed IDF restriction or the current display path.

The current S3 image uses **native ESP-IDF SPI2**, software CS, mode 0 / 40 MHz / `SPI_DEVICE_NO_DUMMY`, and the owner's working net-clock GC9A01 reset/init sequence (`MADCTL=0x48`, RGB565 `0x55`, inversion on). Commands are blocking; pixel strips use one persistent queued DMA transaction, released only after completion before `lv_disp_flush_ready`. TFT_eSPI no longer drives the panel. **Owner-confirmed rendering, ten-minute playback and repeated cold boots pass; the Phase 0.3 S3 rerun is accepted.** Failed TFT_eSPI attempts below are historical; no later phase or 24-hour soak is claimed.

## Accepted Native IDF S3 Measurements

2026-11-08: build/COM8 flash verified, application **1,269,200 bytes (`0x135dd0`)**, 69% factory-partition headroom. The reference `C:/D_Drive/Temp/net-clock` was inspected read-only; saved credentials were not read. Keep its tested reset/init and software-CS behavior, not its full-frame buffer or application-selected 32 KiB chunk cap. Our bus maximum is sized for one 11,520-byte DMA strip.

Saved serial log `spikes/display/build/s3/log.knomi_display_spike.20261108082526.txt` reaches **1,554,715 ms (~25.9 min)**, one boot record, one connection attempt, 369 printer messages, all links active, zero JSON errors/observed oversized messages, no panic/watchdog/decoder failure. Owner independently confirms visible RGB/orientation/GIF/stopwatch checks, ten-minute playback, and repeated cold boots after capture closure. COM8 is released; no separate cold-boot serial trace or 24-hour soak is claimed.

Stable sample summary excludes the first five seconds after source changes (129 Creality / 125 standby samples). Sizes below are bytes; FPS measures completed LVGL refreshes.

| Workload | Internal free | Internal largest | PSRAM free | PSRAM largest | Refresh FPS |
|---|---|---|---|---|---|
| Boot | 296,199 | 204,800 | 2,095,000 | 2,064,384 | 0.00 |
| Network ready, before first GIF | 203,499 | 131,072 | 2,090,348 | 2,064,384 | 0.00 |
| Creality stable, including final HTTP workload | 203,159–203,519 | 126,976–131,072 | 1,998,128 | 1,966,080 | 8.52–9.40 |
| Standby stable | 203,459–203,519 | 131,072 | 2,052,400 | 1,966,080 | 31.66–32.60 |

Five HTTP requests during Creality playback passed: internal free 199,207–199,483, largest 122,880–126,976; PSRAM free 1,997,668–1,998,128, largest 1,966,080; DMA free 191,419–191,695, largest 122,880–126,976. Final internal minimum 160,768; PSRAM minimum 1,973,536. Serial DMA sample after requests: free 195,371 / largest 126,976. Capability groups overlap; do not add internal and DMA figures. PSRAM margin exceeds 112,500 bytes before load and under GIF/HTTP workload: **retain original GIF assets, no downscale required by this gate**. Other GIF assets, oversized frames, and the 24-hour soak still require their planned validation.

## Historical TFT_eSPI S3 Measurements (2026-11-08)

Build/COM8 flash passed with verified hashes: application 1,317,440 bytes (`0x141a40`), 68% factory-partition headroom. Native USB confirms 2 cores, revision v0.2, 240 MHz CPU, 4,194,304-byte flash and 2,097,152-byte PSRAM at 40 MHz; PSRAM memory test passed. IDF's 32 KiB internal DMA reserve is not a SPI transfer-size limit.

Saved log: `spikes/display/build/s3/log.knomi_display_spike.20261108015910.txt`. First segment reached uptime 276,687 ms with repeated GIF swaps, all links active, messages increasing to 61, one connection attempt, no decoder failure/panic/watchdog report, and zero JSON errors/oversized messages. USB disconnected and reconnected with a power-on reset; a separate segment reached 46,361 ms. Do not claim a continuous ten-minute run or infer the power cycle's cause.

All sizes are bytes; FPS counts completed LVGL refreshes rather than verified visible panel frames. Source-transition windows are excluded.

| Workload | Internal free | Internal largest | PSRAM free | PSRAM largest | Refresh FPS |
|---|---|---|---|---|---|
| Boot | 286,103 | 188,416 | 2,095,000 | 2,064,384 | 0.00 |
| Network ready, before first GIF | 192,851 | 118,784 | 2,090,348 | 2,064,384 | 0.00 |
| Creality after HTTP / repeated swaps | 192,519–192,555 | 114,688 | 1,998,128 | 1,966,080 | 8.75–9.20 |
| Standby after HTTP / repeated swaps | 192,495–192,555 | 114,688 | 2,052,400 | 1,966,080 | 31.59–32.20 |

Five diagnostic HTTP requests passed at `http://192.168.0.74/api/status`. Response-time internal free 188,539–188,819, largest 114,688; PSRAM free 1,973,536–1,998,128, largest 1,966,080. Internal minimum-ever heap after HTTP was 150,120; PSRAM minimum 1,973,536. Steady internal DMA free approximately 184,743–184,767, largest 114,688; internal and DMA capability pools overlap and must not be added together.

**This historical S3 attempt passed allocation margin but failed physical rendering/ten-minute playback and did not establish cold-boot acceptance.** It is superseded by the accepted native IDF run above; completed-refresh counters alone did not establish correct panel output.

## Validated display stack

The isolated S3 project is `spikes/display`, using ESP-IDF 5.5.5 with Arduino-ESP32 3.3.12 as a component, LVGL 8.4.0 and native IDF SPI2/GC9A01. COM8 build/flash and owner-observed display, animation, stopwatch, ten-minute stability and repeated cold-boot checks passed on 2026-11-08. Retain 40 MHz SPI; higher speeds remain untested and optional.

LVGL uses the spike's custom configuration via `LV_KCONFIG_IGNORE`, explicit PSRAM allocation, swapped RGB565, and two internal 240 x 24 DMA draw buffers (23,040 bytes total). Only the UI task on core 1 calls LVGL. Exactly one GIF object is decoded at a time; its source alternates every 30 seconds. The stopwatch uses monotonic time and starts with the first GIF after network readiness.

## Phase 0.3 workload

The network task runs on core 0 with a 6 KiB stack; the UI task has an 8 KiB stack. AsyncTCP/web callbacks execute on the library's task. Diagnostics are plain structs copied under a short critical section; no lock is held during LVGL, HTTP, or socket operations.

- Wi-Fi STA uses credentials configured locally through menuconfig. SDK configuration and generated build files are ignored; credentials must not be shared or distributed with binaries.
- ESPAsyncWebServer 3.12.1 with AsyncTCP 3.5.0 serves a GET-only diagnostic endpoint at `/api/status`.
- A read-only WebSocket workload connects to the owner's printer at `192.168.0.55:9999`, path `/`. A static 4 KiB text buffer and 512-byte discard chunks avoid allocating whole oversized messages.
- Filtered ArduinoJson 7.4.3 parsing uses a static 8 KiB arena. These two static reserves total 12 KiB before dynamic Wi-Fi/server/socket allocations.
- The first GIF waits for Wi-Fi, web server, WS connection, and at least one text message. Later samples require all three connection flags active and ongoing printer traffic to qualify as network-loaded evidence.

This is a measurement spike, not the completed production transport/model. Native parser tests, production reconnect backoff, settings, printer snapshots/state derivation, and protocol hardening remain later work.

## Asset-budget decision

**Retain the original 150 x 150 Creality asset.** Its canvas/frame estimate is 90,000 bytes; the 1.25 x margin target is 112,500 bytes. The accepted S3 run measured PSRAM largest **2,064,384 bytes** before first load and **1,966,080 bytes** during GIF/HTTP workload, passing the margin without resizing. Track internal/DMA headroom independently rather than allocating GIFs there.

The current S3 Phase 0.3 validation passed; other assets, live oversized-frame handling and the 24-hour soak still need their planned checks. Revalidate headroom and visible behavior after changes to assets, display HAL or allocation policy.