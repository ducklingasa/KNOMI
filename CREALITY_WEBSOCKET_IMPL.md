# Creality WebSocket implementation

Final hardware: **ESP32-S3-Zero**, 4 MiB flash / 2 MiB PSRAM, GC9A01 on SCK1/MOSI2/DC3/CS4/RST5; native USB Serial/JTAG **COM8**. This log records S3 implementation and validation only.

## 2026-11-10: Phase 2 splash, configuration and rotation checkpoint

- Continued after owner accepted the Phase 1 RGB/uptime image. No new flash/erase/monitor was performed. The accepted default MADCTL0x48, panel initialization bytes, fixed pins, 40MHz SPI and internal DMA/PSRAM policies remain unchanged.
- Added UI-task-only `display_hal::set_rotation()` with validity checks, pending-DMA drain, register0x36 update and full-screen invalidation. Mappings relative to the accepted base are **48/28/88/E8**; BGR retained. No software-rotation/frame buffer. Native CMake/CTest covers all four values/default preservation and invalid 8-bit indices: **1/1 test passes**. Alternate physical orientation and Phase 3 settings binding are not yet validated/implemented.
- Added `app_config.h` and a shared C/LVGL S0 module: green 48px KNOMI, white 18px edition, gray 14px development version. Main UI loop shows it for **1,500 monotonic ms**, then returns to the diagnostic RGB/uptime screen. No placeholder network behavior or printer controls were added.
- The previous numeric-only 48px font could not render KNOMI. Regenerated it with the same pinned Inter4.1/converter1.5.3 at 4bpp, now printable ASCII/degree/Celsius (**97 glyphs**). Verified actual cmap coverage and updated the validator/recipe. The original source archive and 28 legacy-reference screenshots remain unchanged.
- Disabled 26 unused control/complex-widget/theme options; retained 13 required read-only widget/GIF/QR/layout options, including the non-interactive heater slider. Root dark default retained for the new UI; native legacy renderer has its independent configuration.
- Full IDF configure/compile/link/partition-size gates pass: **467,616 bytes (`0x722a0`)**, **88% app-partition headroom**. This is the new unflashed Phase 2 image, not the earlier 440,432-byte owner-accepted image.
- Added a separate native `splash_preview` executable compiling the **actual firmware S0 module and current fonts**. Exported `docs/screens/phase2/splash.png`, circular preview and source manifest, distinct from the original gallery. All three label rectangles fit the round area; mask/pixel check passes with **2,657 green title pixels**, and the image was visually inspected. This is a host unswapped-RGB565 preview, not a physical display/timer test.
- All 20 retained asset sources compiled after the font extension; editor diagnostics clear. Native CMake/CTest is now an actual project under `test`, while production C++ model/parser tests and networking/settings remain future work. New splash/boot timing and rotated orientations still require owner hardware checks.

## 2026-11-10: Owner validates the root display-only image

Owner reports: **"Flashed and validated, RGB screen uptime all good."** This accepts the root Phase 1 image's flash and visible RGB/uptime behavior. It does not imply a newly captured serial log, repeated cold boots, rotated orientations, networking or production screens. The 440,432-byte image/build details are in the checkpoint below; Phase 2 continues from this validated default-orientation display path.

## 2026-11-10: Approved IDF migration and Phase 1 build checkpoint

Status: **cleanup and display-only root build implemented; hardware revalidation pending**. Owner approved ESP-IDF/CMake for the full firmware after reference rendering. No new flash/erase/monitor, printer control, branch, tag or commit was performed.

- Froze **75 original visual source files** into `docs/screens/original/legacy-source.zip`, SHA-256 `d217210378b874fbb9c7a609aaf0a8dceb34b3d31402349ba787287682b205a3`, with per-file hashes. CMake verifies/extracts it when live UI sources are absent. Archive-backed PNG hashes matched all **28** live-source captures before removal; export/pixel/motion validation passes after cleanup.
- Guarded legacy UI removal against the frozen hashes. Moved 10 GIFs, four regenerated fonts, two small icons and four native heater images into `src/ui/assets`. Six static asset headers now include LVGL directly; validation confirms no other source changes, preserving pixel arrays/descriptors/red. Remaining decorative/control-art candidates are retained unlinked under `assets/reference`, not shipped as functional controls.
- Removed the obsolete Moonraker application, generated UI/control/touch callbacks, v2 sensors/CST816S, old pin profiles, OTA script, old partition/PlatformIO files and LVGL backup. The old application main was verified against HEAD before replacing it with the already-built display bootstrap. Printer fixtures/expected baselines and the accepted display-spike implementation were not changed; its two asset paths were updated for relocation.
- Added root IDF/CMake, `src/idf_component.yml`, `sdkconfig.defaults`, portable registry `dependencies.lock`, `partitions_4MB.csv`, fixed GPIO map and the extracted native SPI2 display HAL. The GC9A01 initialization table exactly matches the accepted spike; software CS, mode0/40MHz/NO_DUMMY, two internal 240x24 DMA buffers, completion-before-flush-ready, MADCTL0x48/RGB5650x55/inversion remain. LVGL allocation explicitly requests PSRAM, tick uses esp_timer, byte swap is 1.
- An initial direct-cache build generated local-path lock entries; replaced that setup before acceptance with normal root managed components and a portable registry lock, using copied cached downloads. Root resolves IDF **5.5.5**, Arduino **3.3.12**, LVGL **8.4.0**. Unused web/JSON packages were removed by the manager only from the root cache. Dependency warnings remain visible; third-party sources were not edited.
- `idf.py -B build/esp32s3 build` passes configure/compile/link/bootloader/partition-size gates. Final portable-cache application is **440,432 bytes (`0x6b870`)**, **89% factory-app headroom**; bootloader `0x5170`. The earlier direct-cache prototype was 440,448 bytes. Partition generation verifies **24 KiB NVS**, 4 KiB PHY, 3.875 MiB factory app and 64 KiB reserved coredump.
- Current app checks flash/PSRAM, starts the sole LVGL UI task on core1 and displays RGB bars, boot caption and monotonic MM:SS uptime, logging heap every 5 s. No application Wi-Fi/web/WS/settings/control is active. The new image is **not hardware-accepted** and is not the final network/UI memory/flash footprint. Rotation, production UI/client/settings, widget trimming, native C++ tests and live rendering/cold-boot gates remain later work.
- Validation: **20/20 selected asset C sources compile** using accepted S3 flags; ten GIF byte/frame/loop/budget and four font/hash/glyph checks pass; all **14** printer baselines match unchanged; all **16** Python reference tests pass. No expected files regenerated. Updated asset recipes/export paths and documented the new build/checkpoint; saved references are host LVGL renders, not device photos.

## 2026-11-10: Implementation started - native screen preservation

Status: **Phase 1 preservation work started and native references generated; destructive cleanup not started.** No board flash, raw-capture/baseline change or full-firmware migration is claimed.

- Added the isolated `tools/screen_reference` CMake/native C harness. It directly compiles the 12 adaptable generated constructors with existing image/font arrays, generates inert UI globals/event adapters from the header, and supplies synthetic values without Arduino, input devices or printer commands. The native dark theme matches the original initialization; v1 printing applies the actual runtime percentage-centering and sensor-widget deletion.
- Espressif Clang could compile but lacked a working Windows host linker/runtime. No native GCC/Windows SDK was installed. Downloaded portable **Zig 0.14.1** from the official HTTPS release URL and verified SHA-256 `554f5378228923ffd558eac35e21af020c73789d87afeabf4bfd16f2e6feed2c` before extraction. No elevation/TLS bypass; S3 tools/configuration unchanged. Fresh VS Code tasks needed absolute CMake/Ninja paths because they do not inherit an activated IDF PATH.
- Native configure/build passes against cached **LVGL 8.4.0**. Export/check passes for **28 scenarios**: heating 0/25/50/100/125 and target-zero; v1 print 0/57/100 plus v2 comparison; temperatures; GIF/spinner phases; Wi-Fi lost, QR, info, popup and dialog. Saved square RGB and round-mask PNGs, a contact sheet and source/input/tool/executable hashes in `docs/screens/original`.
- Pixel checks reject blank/wrong-sized captures, verify increasing native heater red, and confirm GIF/spinner motion. Visual inspection confirms v1 centered progress, bottom-to-top heater fill, overshoot clamping, and a horizontal strip below the nozzle art. The original Voron GIF begins with eight black frames, so visible reference phases are selected rather than mislabeling the lead-in as a renderer failure. Original welcome/disconnect text clipping remains visible evidence for correction in the new UI.
- Current export environment is **Python 3.14.6 / Pillow 12.3.0**, recorded in the manifest; this does not retroactively change the Phase 0.5 Pillow 12.0.0 record. Source hashes include the already-regenerated fonts. These are current-source LVGL 8.4.0 references, not stock-binary-perfect images or hardware acceptance. Host-only full-frame output/8 MiB LVGL pool does not alter the firmware's internal DMA/PSRAM allocation policy.
- Corrected the earlier font/asset recipe's Wi-Fi symbol note: `gif_wifi.c` exports **`gif_welcome`**, with `gif_welcome_map`. Neither was renamed. Original source/assets remain available until keep assets and optional framing decisions are resolved. New green layouts, timing/chamber content, cleanup and display-only full-firmware build remain follow-up work.

## 2026-11-10: Original KNOMI visual-reference preservation

- Owner chose **all adaptable read-only screens**, close original geometry/typography, remaining + elapsed time in place of printing X/Y/Z sensors, and **original red heater fills only** (no new hot/cooldown thresholds or warnings). Green remains the decorative/theme/progress accent; thermal/error red is not shifted to green. Keep the original warm-yellow numeric emphasis.
- Created `docs/SCREENS.md` from generated C, runtime overlays and existing exported assets. It records the 12 adaptable sources, original heater icon/value geometry, printing rings/typography, temperature panels, network/QR/GIF/info/notification references, deliberate adaptation differences and the pre-cleanup comparison gate. It is not a full-screen renderer, simulator output or physical rendering acceptance.
- Runtime evidence matters: sensor-free v1 centers the print percentage and deletes X/Y/Z widgets, unlike the generated v2 y=50 layout. The legacy theme helper also recolors heating indicators; the hand-written version must explicitly exempt native heater red instead of copying that grouping. Preserve appearance, not old callbacks, duplicate GIF objects or stray style assignments.
- Array/dimension checks measured four large heater images at **167,652 bytes**, six required nozzle/bed images/icons at **172,335 bytes**. Together with selected GIFs this is **630,532 payload bytes**, excluding fonts/code/descriptors. The full-screen decorative background alone is 172,800 bytes; retain/redraw choices require visual comparison. These are flash payload measurements, not linked-firmware or live-memory claims.
- Updated plan colors/screens/asset layout, cleanup keep inventory, Phase 1 preservation prerequisite, Phase 5 reference requirements and visual HIL gate. Keep generated sources until selected LVGL screen renders or photos are saved and assets moved. The missing SquareLine project does not prevent extracting the design from its C output.
- Checked core geometry and local guide references against existing files. No generated screen, firmware source, image, GIF, font, raw capture or expected baseline was modified. Whole-screen previews and S3 visual checks remain pending; no cleanup/next implementation phase or flash was started.

## 2026-11-10: Two new captures and owner-approved baseline revisions

Status: **all 14 fixture baselines revalidated and owner-approved** after iterative review; 16 focused Python tests pass. This extends Phase 0.4 evidence only. No firmware flash, production C++ port, fake printer, live oversized-frame device test or next implementation phase was started. Raw captures were not changed.

### Connect during error, recover, complete and menu leveling

- Added `test/fixtures/k1max_1.3.5.19_error-resume-finish-idle-auto-leveling_20261110-0842.expected.txt` for the supplied K1 Max FW 1.3.5.19 capture: one connection, 2,833 received JSON messages, one oversized 23,382-byte history frame, `tx:0`, no operator notes.
- #1 at t=0 already contains valid `connect:1`, paused `state:5`, `deviceState:1`, self-test step 1 and `errcode:101/key:104/value:""`. This proves that this error, present before the logger connected, is exported in the initial dump. It does not prove every error survives reboot.
- Owner manually solved the fault and resumed. #25 at 24,233 ms clears the error while still paused; #26 at 24,280 ms resumes `state:1` / step 1, **47 ms later**, without stopping the job. The approved sequence starts `ERROR -> PAUSED -> SELF_TESTING`.
- #1313 at 500,923 ms reports completed `state:2`, progress 100, targets/layers/deviceState 0 and elapsed 137 s. #1745 at 1,234,402 ms first reports raw `state:0`: **733,479 ms (12 min 13.479 s) after completion**. The filename/progress are retained. The owner clarified that the printer's completion **OK was pressed soon after finishing**, not after twelve minutes. No exact button marker exists: neither OK-induced flag effects nor a universal automatic timeout is established.
- #1749 at 1,245,539 ms starts menu leveling (`state:0/deviceState:3`), **11.137 s after raw idle**. The old filename/progress remain; elapsed time is now 0. Point strings run from #2447 `1:0.42` to #2783 `36:0.09`; #2791 at 1,756,964 ms returns deviceState/targets to 0. The approved prototype now releases the stale completion latch for this operation and returns IDLE afterwards. Client closes at 1,804,760 ms.

### Boot placeholder, file lists and complete calibration

- Added the sibling baseline for `k1max_1.3.5.19_boot-idle-complete-calibration_20261110-1011.jsonl`: logger started before printer boot; one connection, 1,797 JSON messages, two oversized frames, `tx:0`, no notes.
- #1 at 52 ms is a **1,426-byte / 76-field placeholder**, `connect:0`, temperatures `""`, `err:{}`. Identity/version, `boxConfig`, temperature/motion/error, `linuxOtaInfo`, `tfCard` and max-temperature capability deltas arrive before readiness. Only the existing whitelist is needed; do not retain file-list/boxConfig payloads for the display.
- #11/#12 at 15,314/16,412 ms are unsolicited **79,463-byte `retGcodeFileInfo2`** frames. #13 at 18,191 ms reports `connect:1`. The read-only logger sends no requests and stays connected; the prototype skips the two oversized lists and advances NOT_READY -> IDLE. This reproduces the previously missing placeholder/list evidence, not a guarantee of those pushes on every connect or proof that an active job's fields are all re-sent.
- Owner-associated `deviceState:4` Input Shaping runs #25 (46,760 ms) -> #822 (296,657 ms), raw state 0. `deviceState:3` Bed Leveling begins #824 (296,776 ms), receives 36 string point updates #1490 -> #1769, and ends #1771 (782,614 ms), returning targets/deviceState to 0. Client closes at 796,334 ms. Mappings are specific to reviewed K1 Max FW 1.3.5.19 evidence, not all models.

### Revised prototype and verification

- Separate first-reach heater flags for nozzle/bed; **nozzle priority**, consistent with the legacy UI. Actual >= target completes that heater once; later dips do not retrigger. Changed targets, off/on and new jobs re-arm it; unknown temperatures cannot prove completion. Self-test/calibration/resume stay the primary operation, with the selected heater appended.
- Idle deviceState 3/4/7 selects Bed leveling/Input shaping/Homing and clears an old COMPLETED/STOPPED latch before the result rule. Result timeout/UI animations are not simulated by this Python prototype.
- Owner explicitly required **ERROR for every nonzero errcode in valid data**, including paused/printing; a badge alone or locally suppressed fault is insufficient. NOT_READY still hides invalid placeholder fields. Local acknowledgement cannot clear/suppress a real error or send printer control. The November 9 baseline now enters ERROR at #1115, not #1159; this supersedes the older 51-ms-only ERROR observation below, without changing its raw capture.
- Regenerated all 14 baselines with `tools/derive_prototype.py`; five older sequences remained identical, the others changed where the revised labels/rules apply. All 14 match independent fresh replay. Owner reviewed and approved the final versions before documentation was updated.
- Added `test/test_derive_prototype.py`; **16/16 tests pass** via `python -m unittest discover -s test -p test_derive_prototype.py -v`. Covers exact/above-target completion, priority/rearm/no retrigger, unknown/placeholder data, new jobs, calibration/result precedence, resume labels and error precedence across raw states 0–9/clearing. Milestone timestamps were checked against raw event numbering. Editor diagnostics and whitespace checks pass; production timing/debounce, parser and hardware checks remain separate.
- Updated protocol evidence/fields/rules/open points and plan screens/acceptance criteria. Preserved the current owner-edited self-test label table in the protocol. Earlier log entries below describe their then-current prototype and acceptance, not today's baseline set.

## 2026-11-09: Phase 0.5 asset preparation

Status: **offline asset gate passed**; no firmware flash or next phase started.

- The ten selected source GIFs in `docs/screens/gif/` already match the embedded C arrays byte-for-byte. Kept their bytes, filenames and symbols unchanged; no resize or needless regeneration. `tools/check_assets.py` checks descriptor dimensions/byte count/color format, all frames through Pillow and the existing GIF parser, durations, infinite looping and budgets.
- Measured GIF payload **458,197 bytes (447.46 KiB)** versus 470 KiB allowed; maximum `4*width*height` canvas/frame estimate **90,000 bytes** versus 90,000 allowed. This excludes the transient LZW table and other runtime allocations; Phase 0.3 remains the S3 network-loaded PSRAM evidence.
- Regenerated `ui_font_InterSemiBold14/18/24/48` using upstream **Inter 4.1 SemiBold** and **lv_font_conv 1.5.3**, all at **4 bpp**, uncompressed. Public font symbols retained. Included the pinned unmodified TTF and upstream OFL license under `tools/fonts/`; exact generation/source hashes in `tools/fonts.md`.
- Glyph sets: 318 each at 14/18 px (printable ASCII, supported Latin-1/Extended-A, Celsius), 97 at 24 px (ASCII/degree/Celsius), 16 at 48 px (numeric/percent/colon/C/space/degree/Celsius). Explicit upstream exclusions U+00AD and U+0149 are documented. Legacy 16/20/32 px fonts remain untouched for later cleanup.
- Installed Pillow 12.0.0 in the configured Python 3.14.6 environment; pinned font/converter downloads succeeded with normal TLS. Validation initially exposed the Wi-Fi descriptor's legacy `gif_welcome_map` array name; the checker now follows `.data` rather than guessing array names. No GIF changes were required.
- `python tools/check_assets.py --compile` passed: all GIF/font checks and **14/14 syntax compiles** using the accepted S3 build database (ESP-IDF 5.5.5 / LVGL 8.4.0). No firmware/build objects overwritten. Generated fonts were not added to the running two-GIF spike workload; all-asset on-device playback, new-screen rendering, full-firmware linking and soak remain later checks.

## 2026-11-09: Phase 0.4 owner acceptance and step labels

- Owner reviewed all **12** `.expected.txt` baselines and confirmed they look correct. The accepted files and prototype remain unchanged.
- Owner-confirmed K1 Max self-test labels: **1 Foreign object detection; 2 Homing; 3 Nozzle cleaning in progress; 4 Homing; 5 Bed leveling**. Replaced inferred labels in the protocol and screen plan; the planned `gif_qgling_green` leveling animation now maps to step 5, not step 4. No assets or firmware were changed.
- Owner believes naturally occurring errors require user interaction with the printer and do not clear automatically. Recorded as an owner-reported expectation, not a universal firmware guarantee. The November 9 capture proves clearing during stopping only; KNOMI acknowledgement must remain local display behavior and cannot resolve a printer fault.
- All other protocol open points are deferred by the owner. Phase 0.4 is complete and owner-accepted within its fixture-review scope; no next phase was started.

## 2026-11-09: Phase 0.4 additional error fixture

- Reviewed owner-provided `test/fixtures/k1max_1.3.5.19_error_20261109-1120.jsonl` and added its sibling `.expected.txt`, extending the fixture/baseline set from 11 to **12**. The unchanged prototype reproduces all 12 saved sequences; no firmware build/flash or live capture acquisition was needed.
- Capture: K1 Max FW 1.3.5.19, one connection, 1,205 JSON messages, three oversized messages (maximum 23,402 bytes), two operator notes, final client-initiated close at 697,768 ms. Raw capture was not changed.
- #1114 (`t=587582`) reports paused `state:5`; #1115 (`t=587638`) reports `errcode:101`, `key:104`, `value:""`. Operator note at `t=643092` identifies a foreign-object detection on the printing plate with printing paused. This is observed association, not a universal code catalogue.
- #1159 (`t=657861`) reports stopping `state:7` while the error is still merged; #1160 (`t=657912`) clears the error; #1164 (`t=659655`) reports stopped `state:4`. The operator later notes selecting stop. Preserve the prototype's **51 ms `ERROR` transition** before `PROCESSING(Stopping)` in the baseline; no prototype/UI rule changes were made.
- Updated plan and protocol evidence/open points: nonzero payload and clearing during this stop flow are now captured. Spontaneous clearing, acknowledgement and reconnect persistence remain unverified. All baselines describe frame-numbered prototype states, not UI timing or production C++ acceptance.

## 2026-11-08: Native IDF display repair and accepted S3 rerun

Status: **Phase 0.3 S3 rerun passed:** build/COM8 flash, network-loaded memory margin, owner-confirmed rendering/ten-minute playback, and repeated cold boots. No later implementation phase was started; 24-hour soak, other GIF assets, and oversized-message coverage remain separate work.

- Owner supplied the working local `C:/D_Drive/Temp/net-clock` S3-Zero project with identical GPIO1–5 wiring. Inspected only source/default hardware settings, not saved credentials; that project was not modified. Its display path uses native IDF SPI2 throughout, software CS, mode 0 / 40 MHz / `SPI_DEVICE_NO_DUMMY`, reset low 20 ms then high 120 ms, software reset plus 150 ms delay, and the full GC9A01 initialization table (`MADCTL=0x48`, pixel format `0x55`, inversion on, sleep-out/display-on delays).
- Replaced the spike's failing mixed TFT_eSPI/Arduino register path with this native IDF setup. Kept LVGL 8.4.0, PSRAM allocation, the network workload, original GIFs, and two internal 240×24 RGB565 buffers. Address-window and memory-write commands are blocking; each pre-swapped pixel strip is queued as a persistent DMA transaction, with CS held low until `spi_device_get_trans_result` returns it. Only then release CS and call `lv_disp_flush_ready`; no full-frame buffer or 32 KiB application cap was introduced. The bus maximum is sized for one 11,520-byte strip.
- Build passed: **1,269,200 bytes (`0x135dd0`)**, 69% factory-partition headroom. Initial upload failed because COM8 was absent; owner reconnected it and retry verified all bootloader/application/partition hashes. No full-chip erase. Boot confirms native IDF SPI2 panel initialization, 4 MiB flash, 2 MiB PSRAM with passing memory test, 240 MHz / 2 cores.
- **Owner confirmed all visible checks pass:** RGB bars red/green/blue left-to-right, upright white text, both GIFs clean, continuous `MM:SS`, and ten minutes of uninterrupted playback. Repeated cold boots were checked after capture closure and passed by owner observation; no separate cold-boot serial traces are claimed.
- Saved serial log: `spikes/display/build/s3/log.knomi_display_spike.20261108082526.txt`, through **1,554,715 ms uptime (~25.9 min)**. One boot record, one printer connection attempt, **369** received messages, all links active, zero JSON errors / observed oversized messages, and no panic/watchdog/decoder failure. Monitor/log closed cleanly and COM8 is released. This is not a 24-hour soak or proof of oversized-frame behavior.
- Stable samples exclude the first five seconds after each source change. FPS is completed-refresh rate, not decoder-only speed. 129 Creality and 125 standby samples were summarized from the saved log.

| Workload | Internal free (bytes) | Internal largest | PSRAM free | PSRAM largest | Refresh FPS |
|---|---|---|---|---|---|
| Boot | 296,199 | 204,800 | 2,095,000 | 2,064,384 | 0.00 |
| Network ready, before first GIF | 203,499 | 131,072 | 2,090,348 | 2,064,384 | 0.00 |
| Creality stable samples, including final HTTP workload | 203,159–203,519 | 126,976–131,072 | 1,998,128 | 1,966,080 | 8.52–9.40 |
| Standby stable samples | 203,459–203,519 | 131,072 | 2,052,400 | 1,966,080 | 31.66–32.60 |

Five diagnostic HTTP requests passed during Creality playback at `http://192.168.0.74/api/status`: internal free **199,207–199,483**, largest **122,880–126,976**; PSRAM free **1,997,668–1,998,128**, largest **1,966,080**; internal DMA free **191,419–191,695**, largest **122,880–126,976**. Final internal minimum **160,768**, PSRAM minimum **1,973,536**. After requests completed, the serial DMA sample recovered to free **195,371**, largest **126,976**. Internal and DMA capability masks overlap; do not add them as separate pools. PSRAM margin comfortably exceeds **112,500 bytes** before first load and under GIF/HTTP workload: retain original assets, no resize required by this gate.

TFT_eSPI no longer drives the spike display. Its unused pinned fetch/component wrapper remains for later cleanup; the old SPI3 image and measurements in the next section are historical failed-rendering evidence, not current configuration. Full-firmware build-system migration remains a separate decision.

## 2026-11-08: Phase 0.3 ESP32-S3-Zero rerun

Status: **historical S3 TFT_eSPI attempt: memory margin passed, physical rendering failed. Superseded by the accepted native IDF repair above.**

- Owner reports ESP32-S3 QFN56 revision v0.2, embedded XMC 4 MB flash and AP_3v3 2 MB PSRAM, 40 MHz crystal. Native USB VID `303a`, PID `1001`; Windows detects **COM8**. Native USB-C flashing/reset authorized. BOOT GPIO0, display/backlight/no-MISO arrangement, local Wi-Fi credentials, and printer `192.168.0.55:9999` unchanged.
- Single GC9A01 SPI map: SCL/SCK1, SDA/MOSI2, DC3, CS4, RST5. SCL/SDA are SPI module labels. Keep 40 MHz display SPI, swapped RGB565, rotation 0, BGR/inversion configuration, and original Creality/standby assets; revalidate physically on S3.
- IDF target `esp32s3`, Arduino variant `esp32s3`, DIO 40 MHz / 4 MB flash, quad-SPI PSRAM at 40 MHz with boot initialization/memory test, USB Serial/JTAG console. LVGL/GIF allocators explicitly request `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` with no internal fallback. Two 240 x 24 DMA buffers remain internal. Static JSON/WS reserves and workload unchanged.
- Investigated and removed the provisional 32 KiB application cap after owner clarification. Installed ESP-IDF 5.5.5 validates against the bus maximum; TFT_eSPI 2.5.43 S3 requests 65,536 bytes. Its `pushImageDMA` overload used here applies blocking 1,024-pixel chunks above 16,384 pixels (32 KiB), then queues the remainder as DMA; the implementation threshold differs from its 32,768-pixel comment. This is not a fixed IDF restriction. Keep existing 11,520-byte strips, which avoid the library fallback; no transfer-size guard or buffer enlargement was retained.
- Startup checks detected flash and initialized PSRAM capacities before launching tasks. Serial logs now include separate PSRAM total/free/largest/minimum and internal DMA free/largest; `/api/status` retains `heap` as internal and adds `psram` and `dma` groups.
- S3 build/configuration live under ignored `spikes/display/build/s3`; load private local credential defaults first when needed, then tracked S3 defaults last so hardware values take precedence. Do not print/read credential fields or distribute configured binaries. An initial defaults-order error disabled PSRAM and selected the wrong target/console; corrected only hardware fields and regenerated. Defaults do not override an already populated `sdkconfig`.
- S3 dependency resolution updates `dependencies.lock`. Initial `cjson`, `esp-dl`, and `esp_new_jpeg` archive requests returned HTTP 403; ordinary retries succeeded after automatic network processing. All required downloads completed. No manual approval or TLS/security bypass.
- GCC 14 reports misleading indentation in pinned TFT_eSPI S3 macros. The local component wrapper keeps this warning visible but non-fatal using PRIVATE `-Wno-error=misleading-indentation`; application/other components retain their warning policy. No downloaded dependency sources were edited.
- Initial default-SPI runtime failed with `StoreProhibited` at address `0x10`: TFT_eSPI's default S3 register selection uses Arduino's `FSPI` enumeration, now 0 in Arduino 3.3.12, where IDF expects raw register index 2/3. Explicit `USE_FSPI_PORT` removed that invalid-address fault but stalled during the first SPI command with watchdog reports. `USE_HSPI_PORT` (SPI3) completed initialization/refresh counters but failed physical rendering. None of these TFT_eSPI configurations is accepted; the native SPI2 repair supersedes them. The FSPI stall's full cause is not established; watchdog checks remain enabled.
- Final SPI3 application: **1,317,440 bytes (`0x141a40`)**, 68% factory-partition headroom. COM8 upload verified bootloader/application/partition-table hashes and reset the device, with no full-chip erase. Native USB logs confirm 2 cores, revision 2 (v0.2), 240 MHz CPU, 4,194,304-byte flash, 2,097,152-byte PSRAM at 40 MHz, and a passing SPI SRAM memory test. IDF reserves 32 KiB internal heap for DMA/internal allocations; this is a memory reserve, not a 32 KiB SPI transfer cap.
- Allocation margin is measured against **PSRAM**: before Creality load, require largest block ≥112,500 bytes. This attempt passed that margin with 2,064,384 bytes before first load and 1,966,080 bytes during GIF/HTTP workloads, but failed rendering despite completed-refresh counters. It did not establish acceptance; the native IDF repair above subsequently passed.

### S3 measurement results

Saved serial evidence: `spikes/display/build/s3/log.knomi_display_spike.20261108015910.txt`. First segment reached uptime **276,687 ms**, with repeated source swaps and printer messages increasing to **61**, one connection attempt, all links active, zero JSON errors and zero observed oversized messages. No panic, watchdog report, or decoder failure appeared in that segment. A USB disconnect/reconnect then showed `reset=1` (power-on), followed by a separate segment through 46,361 ms; do not treat the combined log as one uninterrupted stability run or infer the power cycle's cause. The monitor was stopped and its log closed; COM8 is released.

All heap values are bytes. Internal capability mask is `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`; PSRAM is `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`. FPS excludes source-transition windows and counts completed LVGL refreshes, **not verified panel frames**.

| Workload | Internal free | Internal largest | PSRAM free | PSRAM largest | Refresh FPS |
|---|---|---|---|---|---|
| Boot | 286,103 | 188,416 | 2,095,000 | 2,064,384 | 0.00 |
| Network ready, pattern, before first GIF | 192,851 | 118,784 | 2,090,348 | 2,064,384 | 0.00 |
| Creality after HTTP requests / repeated swaps | 192,519–192,555 | 114,688 | 1,998,128 | 1,966,080 | 8.75–9.20 |
| Standby after HTTP requests / repeated swaps | 192,495–192,555 | 114,688 | 2,052,400 | 1,966,080 | 31.59–32.20 |

- Minimum-ever internal heap after the five HTTP requests: **150,120**; PSRAM minimum: **1,973,536**. Steady internal DMA free is approximately **184,743–184,767**, with largest block **114,688**. Capability groups overlap; do not add internal and DMA figures as if they were separate pools.
- Five consecutive GET requests to `http://192.168.0.74/api/status` passed during the first Creality visit. Response-time internal free: **188,539–188,819**, largest **114,688**; PSRAM free: **1,973,536–1,998,128**, largest **1,966,080**; DMA free **180,751–181,031**, largest **114,688**. These samples include HTTP/JSON/network transient allocations and are not the quiescent serial baseline.
- **Owner physical checks failed on this S3 image:** stale mirrored content and static multicolored random pixels despite confirmed GPIO1–5 wiring, common ground and backlight power. Both-GIF/stopwatch and ten-minute playback failed. These failures were resolved by the native IDF repair above; heap/FPS counters alone did not prove working rendering.
- Follow-up used the owner's working native IDF panel sequence, then repeated the display/heap gates successfully. Long-term soak and oversized-message coverage remain untested.

From an activated ESP-IDF 5.5.5 PowerShell at the repository root:

```powershell
$spike = Join-Path $PWD 'spikes/display'
$build = Join-Path $spike 'build/s3'
idf.py -C $spike -B $build -D IDF_TARGET=esp32s3 -D "SDKCONFIG=$build/sdkconfig" -D 'SDKCONFIG_DEFAULTS=sdkconfig;sdkconfig.defaults' build
idf.py -C $spike -B $build -p COM8 flash
idf.py -C $spike -B $build -p COM8 monitor
```

The first `sdkconfig` defaults file is only for existing local credentials; a fresh checkout should use tracked defaults alone (`SDKCONFIG_DEFAULTS=sdkconfig.defaults`) and configure credentials through `idf.py -C $spike -B $build menuconfig`. Open **KNOMI Phase 0.3 heap spike**, enter credentials locally and confirm printer `192.168.0.55:9999`. Do not share credentials or screenshots containing them. Generated SDK configuration is ignored but plaintext locally and embedded in the binary; keep it out of tracked defaults/logs/distributed artifacts. Verify generated hardware fields after configuration changes; defaults alone do not replace saved settings.

## Current S3 workload and display checks

- Network task on core 0 (6 KiB stack), UI task on core 1 (8 KiB); only the UI task calls LVGL. Plain diagnostic snapshots are copied under a short critical section; no lock spans LVGL/HTTP/socket operations.
- Wi-Fi STA, ESPAsyncWebServer 3.12.1 / AsyncTCP 3.5.0, GET-only `/api/status`, read-only WS to `192.168.0.55:9999`. WS validates upgrade/accept headers, handles continuation/ping/pong/close, masks client messages, and discards oversized/binary messages in 512-byte chunks using a static 4 KiB text buffer. Outbound application messages are heartbeat `ok` and the read-only `ReqPrinterPara` silence probe.
- Filtered ArduinoJson 7.4.3 parsing uses a static 8 KiB arena; web responses exercise transient async-task allocations. First GIF/stopwatch wait for Wi-Fi, server, WS and at least one text message. One GIF alternates Creality/standby every 30 seconds; `MM:SS` updates once per second from monotonic time and continues across swaps.
- Five-second logs report internal/PSRAM/DMA heaps, FPS, link flags and message/error/oversized/connection counters. `reconnects` counts attempts including the initial connection. Only active-link samples with continuing printer traffic qualify as network-loaded evidence.
- Owner-confirmed S3 checks: correct startup RGB/text orientation, clean animation and readable continuous stopwatch for both GIFs, uninterrupted playback through `10:00`, and repeated cold boots with display attached. Do not hold BOOT during normal boot; GPIO0 selects the ROM bootloader.
- This remains a measurement spike: fixed one-second retries, not production backoff/model/state derivation or completed Phase 4 protocol hardening. Other assets, live oversized-frame tests and the 24-hour soak remain pending. Higher SPI speeds are untested; retain the accepted 40 MHz configuration.

## Network download behavior

**No package download was manually approved by the owner.** The network automatically processes and approves packages after an initial download attempt and a variable delay (approximately 30 seconds in this session, not a guaranteed deadline).

For future package downloads, including redownloads of these same versions:

1. Attempt the normal package download. A temporary block or connection failure may initiate network processing rather than indicate a permanent denial.
2. Allow processing time, then retry the same download normally. If progress needs to pause, tell the user which package is pending, let the user wait, and retry when they return; waiting does not mean they must manually approve it.
3. Keep certificate validation and network controls enabled. If a package still fails after several normal download attempts with processing time allowed between retries, report the attempted **package name, version, exact download URL, and error** to the user instead of assuming approval or attempting a bypass.

This behavior applies to other packages as well. A previous successful download does not guarantee immediate availability on a future clean build or redownload.

For S3 clean builds, use `spikes/display/dependencies.lock` as the authoritative resolved dependency list. Arduino's broad component manifest supplies dependencies beyond KNOMI's intended application features; no separate archive-approval history is retained here.