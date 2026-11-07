# Creality WebSocket implementation

## 2026-11-06: Phase 0.1 display spike

Status: **Phase 0.1 completed and accepted by the owner on 2026-11-06.** All device checks, including the stopwatch, 10-minute stability run, and manual reset test, passed as expected.

- Owner approved ESP-IDF with Arduino as a component for an isolated display spike.
- Existing ESP-IDF 5.5.5 and its Python 3.11 environment activated successfully; no PlatformIO installation needed.
- Dependencies: Arduino 3.3.12, LVGL 8.4.0, TFT_eSPI V2.5.43 (commit `6ae4c97c5a07bd64fce8993e9cfa95d9a81e7c81`).
- Project: `spikes/display`; existing firmware, assets, and PlatformIO configuration are unchanged.
- Owner authorized flashing COM7 and confirmed serial-debug wiring: SCK18, MOSI23, DC26, CS5, RST33; no MISO or backlight control.
- Uses two internal DMA RGB565 buffers of 240 x 24 pixels (23,040 bytes total), internal-heap LVGL allocation, one UI task on core 1, and one GIF object.
- Initial 30-second RGB pattern precedes repeated 30-second swaps between `gif_creality` and `gif_standby_green`.
- Serial logs every five seconds: free internal byte-addressable heap, largest free block, minimum free heap, and completed LVGL refreshes per second. Static-pattern FPS is expected to settle to zero; GIF FPS measures refreshes, not SPI transactions or decoder-only performance.
- Wi-Fi, web server, and WebSocket are deliberately absent: these measurements are a display-only baseline, not Phase 0.3 acceptance.
- English, Celsius, 24-hour time, and the 10-minute result timeout confirmed; existing protocol evidence gaps remain deferred.
- Current branch retained; no tag or commit created.

## Validation

- ESP-IDF environment activation: passed (`ESP-IDF v5.5.5`, Python 3.11.2, dependency checks OK).
- TFT_eSPI version resolution: passed; upstream tag is uppercase `V2.5.43`, at the pinned commit above.
- First command: `idf.py -C spikes/display build`.
- Build result: blocked before compilation while CMake FetchContent downloaded TFT_eSPI. GitHub redirected the archive request to `codeload.github.com`, which the Zscaler gateway rejected with HTTP 403. No network security settings were changed or bypassed.
- At the initial failure, Arduino and LVGL dependency resolution had not yet been reached. The owner did not manually approve or arrange access for any download; the network automatically approved the archives after processing time.
- Retry after automatic network approval of TFT_eSPI: the pinned archive downloaded and extracted successfully. Its upstream CMake tried registering an IDF component before IDF initialization; added FetchContent `SOURCE_SUBDIR knomi_fetch_only` so the existing local wrapper registers it at the correct stage.
- Focused validation: `idf.py -C spikes/display reconfigure` passed the previous CMake failure and resolved 34 dependency entries (including the installed IDF). Versions are recorded in `spikes/display/dependencies.lock`.
- Earlier blocker: HTTP 403 downloading Arduino 3.3.12 from `https://components-file.espressif.com/components/espressif/arduino-esp32/3.3.12/espressif__arduino-esp32-v3.3.12.zip`; resolved after automatic network approval, without manual owner approval.
- LVGL 8.4.0 published download link: `https://components.espressif.com/api/downloads/?object_type=component&object_id=dcc10114-d4e5-443e-9fb0-d7fac09493fa` (archive download not yet reached by the build).
- The network processes individual packages rather than broadly allowing URL prefixes. Exact archive URLs are listed below, resolved from registry JSON for the locked versions. Metadata queries do not download package archives.
- Latest `idf.py -C spikes/display reconfigure`: Arduino 3.3.12 downloaded successfully; stopped at HTTP 403 on `espressif/cbor` 0.6.1~4. `chmorgan/esp-libhelix-mp3` 1.0.3 is already available. No further blocked-archive retries or bypass attempted.
- An earlier independent LVGL 8.4.0 archive test ended with a connection reset before ZIP validation. The temporary download was removed. The later component-manager retry downloaded LVGL successfully after automatic network approval.
- A standalone Python metadata probe could not validate the server certificate using its default certificate bundle. Windows-trusted HTTPS metadata requests succeeded with certificate validation enabled; no certificate checks were disabled.
- Owner explained the network's automatic approval process: an initial archive request can be blocked while scanning; retry after approximately 30 seconds. Package-specific approval remains in effect; no broad allowlisting is required.
- Made one normal GET request per known archive to initiate approval, without downloading response bodies in that pass. Used the interval to inspect TFT_eSPI DMA completion and GC9A01 rotation behavior, then retried the IDF component manager. No timed wait command or network-security changes were used.
- Result: all 34 dependency entries processed successfully, including LVGL 8.4.0; `idf.py -C spikes/display reconfigure` completed successfully (147.1 s configuring, 12.5 s generating). Earlier CBOR/Arduino blocks and LVGL connection reset are no longer active blockers.
- Initial build compiled the broad Arduino dependency set; resumed completed objects after interrupting the filtered-output command. Library selection was not changed.
- Found `CONFIG_LV_CONF_SKIP=y` in IDF defaults. Added `LV_KCONFIG_IGNORE` so the spike's custom `lv_conf.h` controls allocation, byte swapping, logging, and GIF support. Compile-time assertions enforce swapped RGB565, custom allocation, and GIF support. Paused the GIF timer before replacing the source to avoid a null-decoder timer callback after failed allocation.
- `idf.py -C spikes/display -p COM7 flash`: passed. Application binary is 560,320 bytes (`0x88cc0`), with 86% of the factory app partition free. Esptool identified ESP32-D0WD-V3 revision 3.1, 40 MHz crystal; bootloader, application, and partition-table hashes verified. Flash completed and hard reset succeeded. No full-chip erase was performed.
- Coredump support is disabled (`CONFIG_ESP_COREDUMP_ENABLE_TO_NONE=y`); the final 64 KiB partition remains reserved.
- Initial 75-second custom pyserial capture was empty; this did not prove a firmware failure. Standard `idf.py -C spikes/display -p COM7 monitor` with its reset handling receives boot and runtime logs normally at 115200 baud.
- Runtime confirms IDF 5.5.5, LVGL 8.4.0, TFT_eSPI 2.5.43, DIO 40 MHz flash, 4 MB capacity, 240 MHz CPU, and the intended serial-debug pin map. Owner confirmed startup bars are red/green/blue left-to-right, text is upright and white on black, and both GIFs animate and switch without the listed visible problems.
- Baseline internal byte-addressable heap (bytes): before display free 288,540 / largest 155,648 / minimum 288,540; after DMA buffers free 267,140 / largest 155,648 / minimum 263,756; static pattern free 262,784 / largest 155,648 / minimum 262,660. Static refresh FPS settles toward zero, as expected. GIF and long-run measurements follow below when available.
- Boot warning: detected Boya flash uses the generic driver; boot and upload succeed. No crash or DMA error appeared in the initial startup capture. This is not yet a 10-minute stability pass or the network-loaded Phase 0.3 budget.
- Standard monitor captured through firmware uptime 111,022 ms without another boot, DMA error, or GIF decoding failure, then was stopped cleanly with Ctrl+]. COM7 is released. This covers the initial pattern, both GIFs, and a return to the Creality GIF, not the full 10-minute gate.
- Steady samples, excluding five-second windows containing a source transition:

| Stage | Free internal heap (bytes) | Largest block (bytes) | Minimum-ever heap (bytes) | Completed-refresh FPS |
|---|---|---|---|---|
| RGB pattern after initial settling | 262,584 | 155,648 | 262,584 | 0.00 |
| Creality GIF, first and second visits | 168,316 | 98,304 | 143,724 | 7.93-8.20 |
| Green standby GIF | 223,612 | 98,304 | 143,724 | 26.92-27.19 |

The repeated Creality visit had the same heap values as the first visit. Its approximately 8 FPS matches the asset's 120 ms frame delay. These are display-only measurements and do not establish the later Wi-Fi/web/WebSocket memory budget.

### Playback stopwatch follow-up

- Added a centered white stopwatch below the GIFs, now formatted as zero-padded `MM:SS` (e.g. `01:03`), in a fixed 170 x 18 label at y=200. The largest GIF ends at y=195, leaving a gap above the label.
- Hidden during the RGB startup pattern. Starts at `00:00` with the first GIF, updates once per second using `esp_timer_get_time()`, and continues across source changes. A device restart resets it; startup's 30 seconds do not count as playback time.
- Validation: `idf.py -C spikes/display build` passed; `idf.py -C spikes/display -p COM7 flash` passed with verified hashes. New application size: 560,528 bytes (`0x88d90`). The upload restarted the device and begins a new stability run.
- Owner confirmed stopwatch visibility and continuity, uninterrupted playback through the 10-minute test, and successful manual reset behavior with the original format. At `10:00`, ten minutes of GIF playback have elapsed since boot, excluding the startup pattern. The earlier measured heap/FPS table predates the stopwatch; its label adds allocations and up to one additional refresh per second.
- 2026-11-07: changed only the initial text and running formatter from `Xm Ys` to zero-padded `MM:SS`. Build passed; this format-only update has not been reflashed or visually rechecked. Minutes continue beyond 99 rather than wrapping.

**Acceptance:** the owner reports every device test completed successfully and as expected, and accepts Phase 0.1 as complete. The 10-minute run and reset acceptance are owner-observed results; the recorded serial capture still covers only 111 seconds, not ten minutes. No longer serial trace or additional heap measurements are claimed. Phase 0.3's network-loaded memory budget and later phases are not completed by this acceptance.

From an activated ESP-IDF 5.5.5 PowerShell:

```powershell
Set-Location spikes/display
idf.py build
idf.py -p COM7 flash
idf.py -p COM7 monitor
```

If an approved local TFT_eSPI V2.5.43 source tree is supplied instead, configure with `idf.py -D FETCHCONTENT_SOURCE_DIR_TFT_ESPI=C:/path/to/TFT_eSPI reconfigure` before building. Arduino and LVGL still require their pinned managed components.

The spike uses a factory-only 4 MiB partition layout with reserved coredump space. Flash replaces the current application; no full-chip erase is performed automatically.

## 2026-11-06: Phase 0.2 SPI speed decision

Status: **completed; retain 40 MHz display SPI.**

- Owner chose to keep the validated 40 MHz baseline, using the accepted Phase 0.1 GIF stability test as evidence. Existing `SPI_FREQUENCY=40000000` remains unchanged; no rebuild or flash is needed for this decision.
- Higher speeds, such as 80 MHz, are untested and deferred as an optional future improvement if rendering performance requires it. They are not required for current implementation or acceptance; any future increase needs renewed panel/wiring stability testing.

## Device checks (owner-confirmed passed)

1. After reset, observe the first 30 seconds: three bars must be red, green, blue from left to right, with white `TOP - RGB` / `DMA 40 MHz` text above on a black background. Check upright, readable text and expected colors, not inverted or tinted colors.
2. At approximately 30 seconds, the centered Creality animation and `00:00` stopwatch appear; at approximately 60 seconds, the green standby animation appears. GIFs then alternate every 30 seconds while the stopwatch keeps counting once per second. Both must animate repeatedly, rather than display one frozen frame. Check that the stopwatch remains readable below either GIF and does not reset at a GIF switch.
3. Check both animations for random pixels, flicker, torn horizontal bands, trails after a source change, clipping, or label overlap. No `GIF allocation failed` message should appear.
4. Leave it powered until the stopwatch reaches at least `10:00`. There must be no reset to the initial RGB pattern, lockup, blank screen, or progressive corruption. Compare heap values at repeated visits to the same GIF; values for different GIFs are expected to differ. The minimum-ever heap need not recover.
5. After the run, use the board's RESET/EN button once and confirm the RGB pattern and GIF sequence start again reliably. Do not hold BOOT during reset, as GPIO0 selects the ROM bootloader.

The owner confirmed all checks above passed. This spike has no printer connection or button-driven application UI.

## Network download behavior

**No package download was manually approved by the owner.** The network automatically processes and approves packages after an initial download attempt and a variable delay (approximately 30 seconds in this session, not a guaranteed deadline).

For future package downloads, including redownloads of these same versions:

1. Attempt the normal package download. A temporary block or connection failure may initiate network processing rather than indicate a permanent denial.
2. Allow processing time, then retry the same download normally. If progress needs to pause, tell the user which package is pending, let the user wait, and retry when they return; waiting does not mean they must manually approve it.
3. Keep certificate validation and network controls enabled. If failures persist after the processing interval and retry, report the exact package, URL, and error instead of assuming approval or attempting a bypass.

This behavior applies to other packages as well. A previous successful download does not guarantee immediate availability on a future clean build or redownload.

## Package-specific download approvals

These are exact archive URLs returned by registry metadata for `spikes/display/dependencies.lock`, retained for future clean builds. All are now downloaded successfully through the component manager after automatic approval. IDF is installed; TFT_eSPI, Arduino, and the MP3 dependency are omitted here. The broad dependency set comes from Arduino's component manifest, not from intended KNOMI application features.

| Package | Version | Archive URL |
|---|---|---|
| espressif/cbor | 0.6.1~4 | https://components-file.espressif.com/components/espressif/cbor/0.6.1~4/espressif__cbor-v0.6.1_4.zip |
| espressif/esp-dsp | 1.8.2 | https://components-file.espressif.com/components/espressif/esp-dsp/1.8.2/espressif__esp-dsp-v1.8.2.zip |
| espressif/esp-modbus | 2.1.4 | https://components-file.espressif.com/components/espressif/esp-modbus/2.1.4/espressif__esp-modbus-v2.1.4.zip |
| espressif/esp-zboss-lib | 1.6.4 | https://components-file.espressif.com/components/espressif/esp-zboss-lib/1.6.4/espressif__esp-zboss-lib-v1.6.4.zip |
| espressif/esp-zigbee-lib | 1.6.8 | https://components-file.espressif.com/components/espressif/esp-zigbee-lib/1.6.8/espressif__esp-zigbee-lib-v1.6.8.zip |
| espressif/esp_daylight | 1.0.1 | https://components-file.espressif.com/components/espressif/esp_daylight/1.0.1/espressif__esp_daylight-v1.0.1.zip |
| espressif/esp_diag_data_store | 1.1.1 | https://components-file.espressif.com/components/espressif/esp_diag_data_store/1.1.1/espressif__esp_diag_data_store-v1.1.1.zip |
| espressif/esp_diagnostics | 1.3.4 | https://components-file.espressif.com/components/espressif/esp_diagnostics/1.3.4/espressif__esp_diagnostics-v1.3.4.zip |
| espressif/esp_insights | 1.3.4 | https://components-file.espressif.com/components/espressif/esp_insights/1.3.4/espressif__esp_insights-v1.3.4.zip |
| espressif/esp_modem | 2.1.0 | https://components-file.espressif.com/components/espressif/esp_modem/2.1.0/espressif__esp_modem-v2.1.0.zip |
| espressif/esp_rainmaker | 2.1.0 | https://components-file.espressif.com/components/espressif/esp_rainmaker/2.1.0/espressif__esp_rainmaker-v2.1.0.zip |
| espressif/esp_schedule | 1.3.3 | https://components-file.espressif.com/components/espressif/esp_schedule/1.3.3/espressif__esp_schedule-v1.3.3.zip |
| espressif/esp_secure_cert_mgr | 2.9.3 | https://components-file.espressif.com/components/espressif/esp_secure_cert_mgr/2.9.3/espressif__esp_secure_cert_mgr-v2.9.3.zip |
| espressif/jsmn | 1.2.0 | https://components-file.espressif.com/components/espressif/jsmn/1.2.0/espressif__jsmn-v1.2.0.zip |
| espressif/json_generator | 2.1.0 | https://components-file.espressif.com/components/espressif/json_generator/2.1.0/espressif__json_generator-v2.1.0.zip |
| espressif/json_parser | 2.0.0 | https://components-file.espressif.com/components/espressif/json_parser/2.0.0/espressif__json_parser-v2.0.0.zip |
| espressif/lan867x | 2.1.0 | https://components-file.espressif.com/components/espressif/lan867x/2.1.0/espressif__lan867x-v2.1.0.zip |
| espressif/lan86xx_common | 1.0.1 | https://components-file.espressif.com/components/espressif/lan86xx_common/1.0.1/espressif__lan86xx_common-v1.0.1.zip |
| espressif/libsodium | 1.0.22~1 | https://components-file.espressif.com/components/espressif/libsodium/1.0.22~1/espressif__libsodium-v1.0.22_1.zip |
| espressif/mdns | 1.15.1 | https://components-file.espressif.com/components/espressif/mdns/1.15.1/espressif__mdns-v1.15.1.zip |
| espressif/network_provisioning | 1.0.2 | https://components-file.espressif.com/components/espressif/network_provisioning/1.0.2/espressif__network_provisioning-v1.0.2.zip |
| espressif/qrcode | 0.1.0~2 | https://components-file.espressif.com/components/espressif/qrcode/0.1.0~2/espressif__qrcode-v0.1.0_2.zip |
| espressif/rmaker_cmd_resp | 1.0.0 | https://components-file.espressif.com/components/espressif/rmaker_cmd_resp/1.0.0/espressif__rmaker_cmd_resp-v1.0.0.zip |
| espressif/rmaker_common | 1.12.0 | https://components-file.espressif.com/components/espressif/rmaker_common/1.12.0/espressif__rmaker_common-v1.12.0.zip |
| espressif/rmaker_common_events | 1.2.0 | https://components-file.espressif.com/components/espressif/rmaker_common_events/1.2.0/espressif__rmaker_common_events-v1.2.0.zip |
| espressif/rmaker_console | 1.1.0 | https://components-file.espressif.com/components/espressif/rmaker_console/1.1.0/espressif__rmaker_console-v1.1.0.zip |
| espressif/rmaker_system_ctrl | 1.0.0 | https://components-file.espressif.com/components/espressif/rmaker_system_ctrl/1.0.0/espressif__rmaker_system_ctrl-v1.0.0.zip |
| espressif/rmaker_time_sync | 1.0.2 | https://components-file.espressif.com/components/espressif/rmaker_time_sync/1.0.2/espressif__rmaker_time_sync-v1.0.2.zip |
| espressif/rmaker_work_queue | 1.1.0 | https://components-file.espressif.com/components/espressif/rmaker_work_queue/1.1.0/espressif__rmaker_work_queue-v1.1.0.zip |
| joltwallet/littlefs | 1.23.1 | https://components-file.espressif.com/components/joltwallet/littlefs/1.23.1/joltwallet__littlefs-v1.23.1.zip |
| lvgl/lvgl | 8.4.0 | https://components-file.espressif.com/components/lvgl/lvgl/8.4.0/lvgl__lvgl-v8.4.0.zip |