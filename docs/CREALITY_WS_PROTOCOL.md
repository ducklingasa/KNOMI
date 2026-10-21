# Creality K-series LAN WebSocket protocol (read-only client reference)

Implementation reference for the KNOMI Creality firmware. It covers only what a **read-only display** needs.

**Evidence base (in priority order):**

1. **Owner captures from a K1 Max** (hostname `K1Max-BF1B`). These are 5 sessions, about 8 850 frames:

   | Capture | Firmware (DWIN sw ver) | Scenario |
   |---|---|---|
   | `Creality_WebSocket_full_print_example.txt` | 1.3.3.8 | Idle → print with calibration → completed |
   | `Creality_WebSocket_just_print_example.txt` | 1.3.3.8 | Idle (manual homing) → print without calibration → completed |
   | `Creality_WebSocket_pause_print_example.txt` | 1.3.3.8 | Print → pause → resume → stop |
   | `Creality_WebSocket_in_print_example.txt` | 1.3.3.29 | Connect at 99 % → completed → idle |
   | `Creality_WebSocket_idle_example.txt` | **1.3.5.19** (latest installed; printer offers 1.3.5.22) | Connect while idle |

   Plus owner notes in `Creality_WebSocket_API.txt`. Location: `C:\D_Drive\Linde\A5 - Private\Creality\`.
   They will be converted into test fixtures (plan, Phase 0.4).
   In this document, **VERIFIED (K1 Max)** means "seen in these captures"; **1.3.5.x** marks behaviour seen only on the latest firmware.
2. **Live receive-only sessions on the same K1 Max, FW 1.3.5.19 (2026-10-20).** The client only listened and sent no application messages. The owner operated the printer from its own screen.

   | Session | Scenario |
   |---|---|
   | A (20 min, 1 646 frames) | Connect while idle → nozzle 210 °C → bed 50 °C → homing → Auto Bed Leveling → idle |
   | B | Connect during print setup → pause → resume → completed (`Cylinder_PLA_1m50s.gcode`) |
   | C | Idle, filament unload/load ×3 |

   Results from these sessions are marked **1.3.5.19 live**.
3. **Static analysis of the offered OTA image** `CR4CU220812S11_ota_img_V1.3.5.22.img` (`ota_info`: `ota_version=1.3.5.22`).
   It covers the stock web UI source map (`/usr/share/frontend`), the startup scripts, and `/usr/bin/web-server`.
   The debug summary labels this image "V1.3.5.19", but its SHA-256 (`5765…7A42`) and `ota_info` identify 1.3.5.22.
   It shows what the **stock client** sends. It does not prove how the server handles requests.
4. Open-source projects, October 2026:

   | Project | License | Notes |
   |---|---|---|
   | [3dg1luk43/ha_creality_ws](https://github.com/3dg1luk43/ha_creality_ws) | AGPL-3.0 | Real K1C traces, simulator, internal reference doc |
   | [hurricaneb/creality_k1_api](https://github.com/hurricaneb/creality_k1_api) | GPL-3.0 | Heartbeat handling |
   | [rathlinus/ha-creality-lan](https://github.com/rathlinus/ha-creality-lan) | none | Delta merge, slicer-derived `STATE_MAP` |
   | [DaviBe92/k2-websocket-re](https://github.com/DaviBe92/k2-websocket-re) | none | K2 example frames |

> **Licensing:** implement from the protocol facts in this document. **Do not copy code** from the (A)GPL/unlicensed projects.

Items marked **UNVERIFIED** still need a capture, especially from the **K2 family** (plan, Phase 0.4).

---

## 1. Transport

| Item | Value |
|---|---|
| URL | `ws://<printer-ip>:9999/` (VERIFIED K1 Max). The stock web UI builds `ws://${location.hostname}:9999` with no path, query, auth header, or subprotocol (1.3.5.22 image). |
| Server | `/usr/bin/web-server`, started at boot by `S99start_app`. It is a native libhv program (MIPS32). |
| TLS / auth | None on port 9999. The binary contains WSS strings (`start wss websocket port`, `wssPort`) and links OpenSSL, but no WSS listener or port is known. KNOMI uses plain `ws://` only. |
| Other paths | The shipped test page `/etc/hv/html/WebSocket.html` uses `ws://127.0.0.1:9999/test`. It is UNVERIFIED whether `/test` is registered; do not use it. |
| Subprotocol / extensions | Not required. The captures used none, and the 1.3.5.19 live sessions negotiated no subprotocol and no extension (so no compression). |
| Frames | Text frames. JSON objects, plus the literal text `ok` as a command ack. The stock UI also ignores a bare `ok` and JSON-parses everything else. |
| Concurrent clients | At least 2 work (`connectionCount` = 2 in sources). Hard limit UNVERIFIED. |

## 2. Message flow

1. **On connect:** one **full status dump**, a JSON object of ≈1.5–1.7 KB.
   - FW 1.3.3.x: 1 505 bytes, 68 keys, with real values.
   - **FW 1.3.5.19, idle capture: 1 653 bytes, 76 keys, but a *placeholder* dump.** Every value is zero:
     `"connect":0`, `"nozzleTemp":"0.000000"`, `"boxTemp":0`, `"maxNozzleTemp":0`, `"maxBoxTemp":0`, `"curFeedratePct":0`, `"tfCard":0`, `"velocityLimits":0`.
     The real values arrive as **deltas within the next ~10 frames**, ending with `{"connect":1}`.
     ➜ While `connect == 0`, **do not show or interpret the data** (logical state `NOT_READY`, §5.4).
   - **FW 1.3.5.19, live sessions A–C: real dumps** (≈1 675 characters) with `connect:1`, real temperatures (≈21.8 °C), and `connectionCount:0`, followed by `{"connectionCount":1}`.
     Session B connected during print setup, and its dump already contained the job (`state:1`, `deviceState:1`, `printProgress:0`).
     ➜ The placeholder dump is **not sent on every connect**. It probably only appears shortly after the printer boots (UNVERIFIED).
2. **Then unsolicited delta frames** with only the changed keys (VERIFIED). Rates seen on the K1 Max:
   - FW 1.3.3.x: about **1 frame/s while idle** (temperature jitter: `nozzleTemp`, `bedTemp0`, `boxTemp`)
   - FW 1.3.5.19 live: about **1 frame every 5 s while idle**; **several frames/s** while heating or moving (`curPosition`)
   - About **4 frames/s while printing** (1.3.3.x)
   - Median frame 55–95 bytes; 99th percentile ≤ 1.5 KB
   Examples (1.3.5.19 live): `{"nozzleTemp":"21.790000"}`, `{"nozzleTemp":"21.780000","bedTemp0":"21.810000"}`.
   **The client must merge deltas into its own state.**
3. **Large list frames are pushed unsolicited.** They never contain status keys (VERIFIED K1 Max):

   | First key | Size seen | When |
   |---|---|---|
   | `retGcodeFileInfo2` (full file list with metadata, ≈ 740 B per file) | **≈ 78 KB for 105 files, sent twice** | **1.3.5.19: right after every connect, unrequested** (no other client connected; `connectionCount` 0 → 1) |
   | `totalJob` + `historyList` (50 entries) | **≈ 20 KB**, sent **twice** in a row | At every job start and job end |
   | `elapseVideoList` (50 entries) | **≈ 28.5 KB**, up to 4× in a row | After a job (timelapse list) |
   | `retGcodeFileInfo` | 6.7 KB for 33 files; grows with the file count | When another client (Creality Print) asks for the file list |
   | `linuxOtaInfo` | 233 B | 1.3.5.19, after connect (offered printer firmware: version and URL) |

   ➜ The client **must skip frames larger than its buffer without dropping the connection** (§4).
   On 1.3.5.x, a client that closes on oversized frames would **reconnect forever**, because the 78 KB list follows every connect.
4. **Heartbeat:** sources describe `{"ModeCode":"heart_beat"}` from the printer, answered with the bare text `ok`.
   **No heartbeat appeared in the K1 Max captures** (several minutes each, no reply sent). Keep the handler for other firmware.
   - The **stock web UI** (1.3.5.22 image) sends its own heartbeat *to* the printer: `{"ModeCode":"heart_beat","msg":"<ISO-8601 date>"}`.
     It is sent 5 s after connect, then again whenever any frame arrives within 1 s. The UI does not parse a reply and does not reconnect on a missing reply.
   - A 1.3.5.19 live client that **never sent a heartbeat stayed connected for the full 20 min**. KNOMI therefore does not send heartbeats.
5. **Plain `ok` frames** are acks to `set` commands. Ignore them; never JSON-parse them.
6. **Silence watchdog (client side):**
   - Idle traffic is at least one frame about every 5 s (1.3.5.19) or every 1 s (1.3.3.x), so a longer silence means a dead link.
   - No frame for 10 s → send `{"method":"get","params":{"ReqPrinterPara":1}}`. The stock UI sends the same request.
   - No frame for 30 s → close and reconnect.
7. **Reconnect backoff:** 1 s × 1.8 up to 60 s, ±20 % jitter. Reset after the first frame.

### Requests (client → printer)

| Request | Use in KNOMI |
|---|---|
| `{"method":"get","params":{"ReqPrinterPara":1}}` | **Only** as the silence probe |
| `reqGcodeFile`, `reqGcodeList`, `reqHistory`, `reqElapseVideoList`, `reqPrintObjects`, `reqProbedMatrix`, `boxsInfo` | **Never** (large replies) |
| any `"method":"set"` (see the owner notes and the list below) | **Forbidden.** The firmware is read-only by design. |

`set` parameters sent by the stock web UI (1.3.5.22 source map). These are for reference only, and their server-side effects are UNVERIFIED:
`pause`, `stop`, `autohome` (`"X Y Z"`), `restartKlipper`, `restartFirmware`, `excludeObjects`, `deleteHistory`, `opGcodeFile`, `gcodeCmd`, `nozzleTempControl`, `bedTempControl`, `fan`, `fanCase`, `fanAuxiliary`, `lightSw`, `setFeedratePct`.

## 3. Fields used by KNOMI

Types differ **per field**. On the K1 Max, temperature actuals are strings (`"27.050000"`); targets, `boxTemp`, `layer`, and `printProgress` are ints.
Fields may be `""` while the printer boots (per sources).
Parse every numeric field with a tolerant coercion: number → value; numeric string → value; `""`, `null`, or garbage → *unknown* (`has_value=false`).

| Field | Wire type (K1 Max) | Unit | Notes |
|---|---|---|---|
| `state` | int | enum (§5.1) | Job state |
| `deviceState` | int | 0 idle, 1 job busy, 3 bed leveling, 7 homing | VERIFIED K1 Max: `7` while idle during manual homing (also 1.3.5.19 live); `1` from job start until the job ends; `0` at stop/complete. **1.3.5.19 live: `3` during Auto Bed Leveling** started from the screen, with `state` staying 0. |
| `autoLevelResult` | presumably string (UNVERIFIED) | `<index>:<offset>` | Optional (1.3.5.19 live). During Auto Bed Leveling it advances `1:0.42` … `36:0.11` (36 points on the K1 Max) and keeps the last value afterwards. Use the index for "Leveling n/36" only while `deviceState == 3`. |
| `withSelfTest` | int | **step number** | **VERIFIED K1 Max: 1, 2, 3, 4, (5), then 100.** Not a percentage. Stays 100 for the rest of the job and resets to 1 at the next job (§5.3). |
| `printProgress` | int | 0–100 % | Becomes 100 at completion and **stays 100** until the next job. `dProgress` stays 0 on the K1 Max (ignore it). |
| `printLeftTime` | int | s | **0 until `TotalLayer` is known**, so show `--` while 0 and not printing. 0 after completion. |
| `printJobTime` | int | s | Elapsed. **Resets to 0 some time after completion while the file name is kept** (VERIFIED), so it is **not** a new-job signal. |
| `printStartTime` | int | unix epoch s | Set together with `printFileName` at job start. **New-job key.** |
| `printFileName` | string | – | Full path, e.g. `/usr/data/printer_data/gcodes/<name>.gcode`. Set at job start and **kept after the job ends**. Display the basename without `.gcode`. |
| `nozzleTemp` / `targetNozzleTemp` | string / int | °C | 1.3.5.19 live: actual may overshoot the target slightly (211.49 for 210) |
| `bedTemp0` / `targetBedTemp0` | string / int | °C | |
| `boxTemp` | int | °C | Chamber actual. `0` in the 1.3.5.19 placeholder dump, then the real value. |
| `maxBoxTemp` | int | °C | **Absent on 1.3.3.x; present on 1.3.5.19** (0 in the placeholder dump, then `60`) |
| `targetBoxTemp` | int | °C | Present (0) on K1 Max 1.3.5.19. Meaningful on K2 only; always 0 on K2 base (F021). Show it only if > 0. |
| `connect` | int | 0/1 | **Data valid flag** (1.3.5.19: 0 in the placeholder dump, then `{"connect":1}` once real values have been sent; 1.3.3.x dumps already had 1). Treat 0 as "printer not ready". |
| `layer` / `TotalLayer` | int | count | `TotalLayer` is set after the self-test. **Both reset to 0 at completion and at stop** (VERIFIED). |
| `err` | object `{errcode:int, key:int[, value:string]}` | – | `value` absent on 1.3.3.x, present (`""`) on 1.3.5.19. Stayed `{"errcode":0,"key":0,"value":""}` in all 1.3.5.19 live sessions. Errors may persist (sources). |
| `model` | string | – | VERIFIED `"K1 Max"`. Sources: `"K1"`, `"K1C"`, `F021` (K2), `F012` (K2 Pro), `F008` (K2 Plus) |
| `hostname` | string | – | e.g. `K1Max-BF1B` |
| `modelVersion` | string | – | `"printer hw ver:;printer sw ver:;DWIN hw ver:CR4CU220812S11;DWIN sw ver:1.3.5.19;"`. Use the `DWIN sw ver` when `printer sw ver` is empty. |
| `modelFanPct`, `curFeedratePct` | int | % | Optional (printing page 3) |
| `linuxOtaInfo.version` | string | – | Optional (1.3.5.x). Offered printer firmware, e.g. `1.3.5.22`; shown on the Info screen as "Printer update available". Parse it only from the small `linuxOtaInfo` frame. |

**Chamber detection (late discovery, re-evaluated on every merge):**

- if `maxBoxTemp` has been received: chamber present ⇔ `maxBoxTemp > 0`;
- otherwise (1.3.3.x): chamber present ⇔ the `boxTemp` key was received **with `connect == 1`**.

Never decide on capability from the placeholder dump.

Fields that are **not usable** on the K1 Max:

- `enableSelfTest` stays 0 even when a self-test runs.
- `materialStatus` is 1 while idle and printing normally on 1.3.3.x. On 1.3.5.19 it is 0 while idle (live) and 0 in the placeholder dump.
- `materialDetect` (1), `cfsConnect` (0), and `feedState` (14) did not change during a 1.3.5.19 filament unload/load session.
- `materialDetector1` toggled 1/0 during manual filament unload/load. However, it ended at 0 with filament loaded, so it is not a reliable filament sensor (1.3.5.19 live).
- `dProgress` stays 0.
- `connectionCount` is 0 in the dump that is sent *to* the new client and only increments afterwards.
- `autohome` (`"X:0 Y:0 Z:0"` → `"X:1 Y:1 Z:1"`, per-axis homed flags) and `curPosition` (`"X:145.46 Y:142.65 Z:6.00"`, sent very often while moving) are strings and are not needed. `deviceState` already covers homing.

ArduinoJson **filter** whitelist: the fields in the table plus `ModeCode`, `printTimeLeft`, and `connectionCount`.
The filter must use `linuxOtaInfo: {version: true}` only, never the whole object.

## 4. Frame handling on the ESP32

| Rule | Value |
|---|---|
| Receive buffer | **4 KB, static.** The largest status frame seen is ≈ 1.68 KB (1.3.5.19 dump with real values, ≈ 1 675 characters); the K2 dump is ≈ 1.8 KB plus possible CFS keys. |
| Larger frames | **Stream-discard** the payload without allocating, and keep the connection. Record the first ≈ 32 bytes (e.g. `{"totalJob":`, `{"retGcodeFileInfo2":`) for diagnostics. The largest frame seen is **78 KB**; the size grows with the number of files on the printer, so there is no upper bound. |
| Fragmented frames | Assemble continuation frames while the total is ≤ buffer; otherwise discard the whole message |
| Control frames | `ping` → `pong`; `close` → close and reconnect |

> The `links2004/WebSockets` library closes the connection (code 1009) on any frame above `WEBSOCKETS_MAX_DATA_SIZE` (15 KB default) and `malloc`s whole frames otherwise.
> On 1.3.3.x it would disconnect at **every job start and end** (20 KB `historyList`).
> On 1.3.5.x it would **never stay connected** (78 KB file list after every connect), and heap churn would compete with the GIF canvas.
> Hence the plan uses a small **custom streaming client** (plan §9.2).

## 5. State model

### 5.1 Raw `state` codes

| `state` | Meaning | Evidence |
|---|---|---|
| 0 | Idle, **or transient inside a job** (pausing, resuming, stopping on 1.3.3.x), or "job ended" with the file kept | VERIFIED K1 Max |
| 1 | Job running, **including** the self-test, heating, and preparation phases before layer 1 | VERIFIED K1 Max |
| 2 | **Completed** (`printProgress` becomes 100 in the same frame); goes to 0 about 15–60 frames later (1.3.3.x) | **VERIFIED K1 Max**, also 1.3.5.19 live (contradicts the ha_creality_ws claim; matches rathlinus) |
| 3 | Failed | UNVERIFIED (rathlinus) → treat as STOPPED, labelled "Print failed" |
| 4 | Stopped/cancelled (stop from the printer screen or the slicer). `deviceState`, `layer`, and `TotalLayer` become 0 in the same frame. | VERIFIED K1 Max |
| 5 | Paused | VERIFIED K1 Max (1.3.3.x and 1.3.5.19 live) |
| 6 | Pausing (between running and paused) | **VERIFIED K1 Max 1.3.5.19 live** (`1 → 6 → 5`) → PAUSED "Pausing…" |
| 7 | Busy while a cancel finishes its move | K1C (sources) → PROCESSING "Stopping…" |
| 8 | Resuming from pause (between paused and running; the nozzle reheats). Sources also report it while restoring after power loss. | **VERIFIED K1 Max 1.3.5.19 live** (`5 → 8 → 1`) → PROCESSING "Resuming…" |
| 9 | ~0.5 s transient before a job starts | K1C (sources); not seen on the K1 Max → IDLE |

### 5.2 Verified sequences (K1 Max FW 1.3.3.x)

The `#n` markers are frame numbers in the captures.

**Job start** (full and just print):

1. `printFileName` + `printStartTime` (#18). `historyList` ×2 is pushed.
2. `deviceState:1` → `state:1`.
3. `withSelfTest` 1 → 2 (targets 150/45) → 3 (nozzle 190 → 200 → 150) → 4 → [5, calibration run only] → **100**.
4. `TotalLayer` set → `printLeftTime` set → targets 0 → targets set to print temperature (heating).
5. `layer:1` → `printProgress` increments.

**Completion:**

1. Targets 0 at 99 %.
2. `{"state":2,"printProgress":100,"layer":0,"TotalLayer":0}` → `deviceState:0` → `historyList` ×2 (→ `elapseVideoList`).
3. About 15–60 frames later: `state:0`. The file name stays set and progress stays 100.
4. Later `printJobTime` → 0. The printer may set nozzle targets again (e.g. 220 °C for ≈ 100 frames, then 0) while idle.

**Pause/resume:**

```
1 → 0 (≈2 frames; target drops to 140) → 5 … → 0 (target back to 220, reheating ≈100 frames) → 1 (progress continues)
```

`deviceState` stays 1 throughout.

**Stop:**

1. `1 → 0` (cancel requested; targets → 0).
2. About 60 frames later: `{"state":4,"deviceState":0,"layer":0,"TotalLayer":0}`. File and progress are kept.
3. `historyList` ×2.

With the WebSocket `stop` command (owner notes), only `state:0` followed by `{"deviceState":0,"layer":0,"TotalLayer":0}` was recorded. Handle **both** shapes.

**Connect during a print:** the dump has `state:1`, `withSelfTest:100`, `layer` and `TotalLayer` set → PRINTING immediately.

**Idle manual homing:** `deviceState` 7 → 0, `state` stays 0, no file.

### 5.2a Verified sequences (K1 Max FW 1.3.5.19, live sessions)

These come from the written summary of the 2026-10-20 sessions, without frame numbers or raw logs.

**Connect during print setup** (session B): the dump already has `state:1`, `deviceState:1`, `printProgress:0` → PROCESSING.
During setup, the nozzle target went 130 → 170 → 200 → 130 °C, with the bed at 50 °C. Then `TotalLayer` was set (15), followed by `printLeftTime` (first value 110). The target then went to 220 °C and printing started.
The `withSelfTest` values were not recorded.

**Printing:** `printJobTime` counts up in seconds, `printLeftTime` counts down, `printProgress` goes 1 → 99, and `layer` goes 1 → 15, all as partial deltas.

**Pause/resume** (paused and resumed from the printer screen):

```
1 → 6 (pausing) → 5 (paused; nozzle target 140) → 8 (resuming) → 1 (target back to 220; progress continues)
```

`printProgress` and `printJobTime` stay frozen (47 %, 63 s) from 6 to 8. Unlike 1.3.3.x, **no `state:0` appears** in this sequence.

**Completion:** `printProgress:100`, `printLeftTime:0`, `printJobTime:139`, `state:2`, `deviceState:0`, `layer:0`, `TotalLayer:0`, both targets 0, and `printFileName` kept.
This matches 1.3.3.x. The delay until `state:0` was not recorded.

**Idle manual homing** (session A): `deviceState` 7 → 0, and `state` stays 0.
`autohome` goes `"X:0 Y:0 Z:0"` → `"X:1 Y:0 Z:0"` → `"X:1 Y:1 Z:0"` → `"X:1 Y:1 Z:1"`, with frequent `curPosition` deltas.

**Idle Auto Bed Leveling** (session A, started from the printer screen):

1. `deviceState:3`; `state` and `printProgress` stay 0 and no file is set.
2. The printer sets targets by itself: nozzle 140 → 180 → 240 → 140 °C, and bed 50 °C.
3. `autoLevelResult` goes `1:…` → `36:…`.
4. At the end, `deviceState:0` and both targets return to 0.

**Idle heating from the screen** (session A): only `targetNozzleTemp`/`targetBedTemp0` change. `state` and `deviceState` stay 0 → IDLE with "Heating".

### 5.3 Self-test steps (`withSelfTest`)

These names are **inferred** from temperature behaviour in the K1 Max captures. Confirm them against the printer's own screen (UNVERIFIED).

| Step | Observed | Proposed label |
|---|---|---|
| 1 | Brief, no temperature change | "Preparing" |
| 2 | Targets 150/45 set; heating | "Heating" |
| 3 | Nozzle 190 → 200 → back to 150 | "Nozzle cleaning" |
| 4 | At 150 °C / 45 °C | "Auto-leveling" |
| 5 | Only in the "with calibration" print | "Calibrating" |
| 100 | Self-test finished (stays 100 during the print) | – |

Display "Self-test · step N" with the label. **Do not** draw it as a percentage arc.

### 5.4 KNOMI derivation rules (first match wins)

Session memory kept by the deriver:

- `prev`: the previous logical state
- `latch`: COMPLETED or STOPPED, entered live
- `trans_from`: the origin of an in-job `state:0` transition
- `job_id`: `printStartTime` plus `printFileName`

A change of `job_id` (with a non-empty file) clears `latch`. Any `state != 0` clears `trans_from`.

| # | Condition | Logical state (sub-label) |
|---|---|---|
| 1 | WS not connected, or no frame for 15 s | `OFFLINE` |
| 1a | `connect == 0` (1.3.5.x placeholder dump; printer backend not ready) | `NOT_READY` ("Printer starting…"). No values are displayed. |
| 2 | `errcode != 0`, not acknowledged, and `state` ∉ {1,5} | `ERROR` |
| 3 | `state==1` and `withSelfTest` in 1..99 | `SELF_TESTING` ("Step N · label") |
| 4 | `state==1` and `layer==0` and progress == 0 | `PROCESSING` ("Heating" if a target > actual + 3, else "Preparing") |
| 5 | `state==1` | `PRINTING` |
| 6 | `state` ∈ {5,6} | `PAUSED` ("Pausing…" for 6) |
| 7 | `state==2` | `COMPLETED`; set `latch` |
| 8 | `state` ∈ {3,4} and (`prev` is busy or `latch==STOPPED`) | `STOPPED`; set `latch` ("Print failed" for 3) |
| 9 | `state` ∈ {7,8} | `PROCESSING` ("Stopping…" / "Resuming…") |
| 10 | `state==0` and `latch` is set | the latched state (until `result_timeout`, a new job, or a button click) |
| 11 | `state==0`, file set, progress < 100, `deviceState==1`, and (`trans_from` is set or `prev` is busy) | origin PRINTING → `PRINTING` ("Pausing / stopping…"); origin PAUSED → `PROCESSING` ("Resuming…") |
| 12 | `state==0`, file set, progress < 100, `deviceState==0`, `layer==0`, `TotalLayer==0`, and `prev` is busy | `STOPPED`; set `latch` (WS-stop shape) |
| 13 | `deviceState==7` | `PROCESSING` ("Homing") |
| 13a | `deviceState==3` | `PROCESSING` ("Bed leveling", optionally "n/36" from the `autoLevelResult` index). The printer heats during this, so this rule must win over idle heating. |
| 14 | otherwise | `IDLE` (state 0/3/4/9 without a live transition, stale results, idle heating) |

Additional rules:

- "Busy" means PROCESSING, SELF_TESTING, PRINTING, or PAUSED.
- If `errcode != 0` during an active job, show a red **error badge** with the code on the job screen.
- **IDLE detail:** if any target > 0, show the temperature page with "Heating" (do not show the GIF). If a file is set with progress 100 (or `state` was 4), show "Last: <file> ✓/✕" on the idle temperature page.
- **On connect:** the first *valid* data (the dump with `connect:1`, or the `{"connect":1}` delta on 1.3.5.x) has no live history (`prev` = OFFLINE/NOT_READY, both not busy).
  - `state:2` → COMPLETED only if NTP is valid and `now − (printStartTime + printJobTime) < result_timeout`; otherwise IDLE with "Last ✓".
  - `state:4` → IDLE with "Last ✕".
  - When `connect` changes 0 → 1, send **one** `ReqPrinterPara` get. This is a safety net in case job fields are not re-sent after the placeholder dump; whether it is needed is UNVERIFIED.
- **Debounce:** 700 ms before the UI switches, except `OFFLINE` and `ERROR`, which switch immediately. `NOT_READY` → IDLE does not need debouncing.

### 5.5 Prototype validation

A Python prototype of these rules ([tools/derive_prototype.py](../tools/derive_prototype.py); to be ported to `lib/creality_model` with unit tests) produces these sequences from the owner captures. They are the **expected outputs** for the fixtures:

```
full_print:  IDLE → PROCESSING(Preparing) → SELF_TESTING(1→2→3→4→5) → PROCESSING(Preparing→Heating→Preparing) → PRINTING → COMPLETED
just_print:  IDLE → PROCESSING(Homing) → IDLE → PROCESSING(Homing) → IDLE → PROCESSING(Preparing) → SELF_TESTING(1→2→3→4)
             → PROCESSING(Preparing→Heating→Preparing) → PRINTING → COMPLETED
pause_print: IDLE → PROCESSING → SELF_TESTING(1→4) → PROCESSING → PRINTING → PRINTING(Pausing/stopping) → PAUSED
             → PROCESSING(Resuming) → PRINTING → PRINTING(Pausing/stopping) → STOPPED
in_print:    PRINTING → COMPLETED (latched through state 0 and the later idle heating)
idle (1.3.5.19): NOT_READY → IDLE (at the {"connect":1} delta; the two 78 KB file-list frames are skipped)
```

For the 1.3.5.19 live sessions (§5.2a), only a written summary exists, so these are **expected** outputs. Confirm them once raw logs are captured:

```
live A (1.3.5.19): IDLE (incl. idle heating) → PROCESSING(Homing) → IDLE → PROCESSING(Bed leveling) → IDLE
live B (1.3.5.19): PROCESSING/SELF_TESTING (setup) → PRINTING → PAUSED(Pausing) → PAUSED → PROCESSING(Resuming) → PRINTING → COMPLETED
```

## 6. Example frames

Initial dump, K1 Max FW 1.3.3.8 (selected keys; 68 keys in total):

```json
{"TotalLayer":0,"bedTemp0":"24.670000","boxTemp":24,"dProgress":0,"deviceState":0,"enableSelfTest":0,
 "err":{"errcode":0,"key":0},"hostname":"K1Max-BF1B","layer":0,"materialStatus":1,"maxBedTemp":125,
 "maxNozzleTemp":320,"model":"K1 Max","modelVersion":"printer hw ver:;printer sw ver:;DWIN hw ver:CR4CU220812S11;DWIN sw ver:1.3.3.8;",
 "nozzleTemp":"27.050000","printFileName":"","printJobTime":0,"printLeftTime":0,"printProgress":0,"printStartTime":0,
 "state":0,"targetBedTemp0":0,"targetNozzleTemp":0,"withSelfTest":0}
```

Completion and stop frames (K1 Max):

```json
{"state":2,"printProgress":100,"layer":0,"TotalLayer":0, "…":"…"}
{"state":4,"deviceState":0,"layer":0,"TotalLayer":0, "…":"…"}
```

Connect sequence on K1 Max FW 1.3.5.19 (idle). Large frames are shortened.

```json
{"connect":0,"nozzleTemp":"0.000000","bedTemp0":"0.000000","boxTemp":0,"maxBoxTemp":0,"targetBoxTemp":0,"maxNozzleTemp":0,
 "cfsConnect":0,"webrtcSupport":0,"connectionCount":0,"err":{"errcode":0,"key":0,"value":""},"state":0, "…":"76 keys, 1653 B"}
{"connectionCount":1}
{"linuxOtaInfo":{"version":"1.3.5.22","url":"https://file2-cdn.creality.com/…/CR4CU220812S11_ota_img_V1.3.5.22.img","name":"…","size":142580074}}
{"tfCard":1}
{"maxNozzleTemp":300,"maxBedTemp":100,"maxBoxTemp":60}
{"retGcodeFileInfo2":[{"name":"ProGlide Case In_PLA_2h58m.gcode","path":"/usr/data/printer_data/gcodes/…","…":"…"}, "…105 files, 78 069 B"]}
{"retGcodeFileInfo2":[ "…identical second copy, 78 069 B"]}
{"connect":1}
{"pressureAdvance":"0.040000"}
{"nozzleTemp":"22.640000","bedTemp0":"22.880000","boxTemp":23}
```

Connect sequence on K1 Max FW 1.3.5.19, live session A (idle, not just after boot). The dump is shown only as a summary:

```json
{"connect":1,"connectionCount":0,"state":0,"deviceState":0,"printProgress":0,"printFileName":"","targetNozzleTemp":0,"targetBedTemp0":0,
 "nozzleTemp":"21.8…","bedTemp0":"21.8…","err":{"errcode":0,"key":0,"value":""}, "…":"≈ 1 675 characters"}
{"connectionCount":1}
{"nozzleTemp":"21.790000"}
{"nozzleTemp":"21.780000","bedTemp0":"21.810000"}
```

Keys in the 1.3.5.19 dump that KNOMI ignores (from the live summary; grouped, not a full schema):

- Identity: `tfCard`
- State: `feedState`, `powerLoss`, `upgradeStatus`
- Job: `printFileType`, `printId`, `realTimeFlow`, `realTimeSpeed`, `curFlowratePct`, `usedMaterialLength`
- Motion: `autohome`, `curPosition`, `nozzleMoveSnapshot`, `velocityLimits`, `accelerationLimits`, `accelToDecelLimits`, `cornerVelocityLimits`, `smoothTime`, `pressureAdvance`
- Heaters: `bedTemp1`, `bedTemp2`, `targetBedTemp1`, `targetBedTemp2`, `maxNozzleTemp`, `maxBedTemp`, `nozzleTempAutoPid`, `bedTempAutoPid`
- Fans: `fan`, `fanAuxiliary`, `fanCase`, `auxiliaryFanPct`, `caseFanPct`, `lightSw`
- Material: `materialStatus`, `materialDetect`, `materialDetector1`, `cfsConnect`, `repoPlrStatus`
- AI and self-test: `enableSelfTest`, `aiDetection`, `aiFirstFloor`, `aiPausePrint`, `aiSw`
- Video: `video`, `video1`, `videoElapse`, `videoElapseFrame`, `videoElapseInterval`, `webrtcSupport`

The K2 Plus dump (source: DaviBe92) has the same key set as K1 Max 1.3.5.19 (`maxBoxTemp`, `targetBoxTemp`, `cfsConnect`, `webrtcSupport`, `err.value`); the model is a board code (`F008`). The K2 and current K1 firmware therefore appear to share one server code base.

Heartbeat and error, from sources (not seen on the K1 Max):

```json
{"ModeCode":"heart_beat"}
{"err":{"errcode":500,"key":121,"value":""}}
```

Heartbeat sent **by the stock web UI** to the printer (1.3.5.22 source map; KNOMI does not send it):

```json
{"ModeCode":"heart_beat","msg":"2026-10-20T08:00:00.000Z"}
```

## 7. Open points (Phase 0.4)

- [ ] **Raw logs of the 1.3.5.19 live sessions** (JSONL, with timestamps) so they can become fixtures. At the moment only the written summary exists.
- [ ] **Remaining print scenarios on FW 1.3.5.19:**
  - Done (live): pause/resume, completion, and connect during print setup.
  - Still open: **stop**, **connect mid-print** (`layer > 0`), and the `withSelfTest` steps.
  - Are the job fields re-sent after a placeholder dump?
  - How long after completion does `state` change from 2 to 0?
- [ ] Placeholder dump (`connect:0`): the live sessions show that it is **not** sent on every connect. Confirm that it appears only shortly after boot (connect within the first minute after power-on).
- [ ] Does the 78 KB `retGcodeFileInfo2` push happen on every connect? The live summary does not record it.
- [ ] State `8`: on 1.3.5.19 it appeared while resuming from pause, before the owner confirmed the resume. Is it also used after power loss, as sources report?
- [ ] Other `deviceState` values (only 0, 1, 3, and 7 are seen). Check printhead cleaning, if the printer has it, and filament retract/load.
- [ ] **K2 family captures:** idle dump, print with self-test, pause/resume, stop, completion. Confirm `state:2`, the `withSelfTest` steps, and the chamber fields.
- [ ] K1 / K1C captures, if available (sources show `state:7`/`9`, which the K1 Max does not).
- [ ] An error capture from a natural error (e.g. filament runout): does `err` clear by itself? `err` stayed 0 in all live sessions. Do not force a fault only to test this.
- [ ] Confirm the self-test step names (§5.3) on the printer screen.
- [ ] Captures with timestamps, to measure real time spent per phase and the delay of `state 2 → 0` and of the `printJobTime` reset.
- [x] ~~Heartbeat required?~~ No. A 1.3.5.19 client that sent nothing stayed connected for 20 min. Whether the printer itself sends a heartbeat to third-party clients is still unknown for other firmware.
