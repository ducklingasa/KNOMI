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

   Plus owner notes in `Creality_WebSocket_API.txt`. These legacy captures are not checked in; separate, newer JSONL captures are in `test/fixtures` (see item 3).
   In this document, **VERIFIED (K1 Max)** means "seen in these captures"; **1.3.5.x** marks behaviour seen only on the latest firmware.
2. **Live receive-only sessions on the same K1 Max, FW 1.3.5.19 (2026-10-20).** The client only listened and sent no application messages. The owner operated the printer from its own screen.

   | Session | Scenario |
   |---|---|
   | A (20 min, 1 646 frames) | Connect while idle → nozzle 210 °C → bed 50 °C → homing → Auto Bed Leveling → idle |
   | B | Connect during print setup → pause → resume → completed (`Cylinder_PLA_1m50s.gcode`) |
   | C | Idle, filament unload/load ×3 |

   Results from these sessions are marked **1.3.5.19 live**.
3. **Checked-in receive-only JSONL captures** in `test/fixtures` (**14** K1 Max FW 1.3.5.19 files, captured 2026-10-15 through 2026-11-10). They cover idle/reconnect/reboot, print, connect during print or a pre-existing print-start error, pause/resume, cancel/stop, simulated recovery/runout, error recovery followed by completion/idle/menu leveling, and boot-time readiness/input shaping/leveling. Every fixture has `tx:0`. The November 10 boot capture reproduces both `connect:0` and two unsolicited **79,463-byte `retGcodeFileInfo2`** frames. All 14 revised expected sequences were owner-approved on 2026-11-10; fixture-specific findings are below. These receive-only logs do not validate client requests or universal push/error behavior.
4. **Static analysis of the offered OTA image** `CR4CU220812S11_ota_img_V1.3.5.22.img` (`ota_info`: `ota_version=1.3.5.22`).
   It covers the stock web UI source map (`/usr/share/frontend`), the startup scripts, and `/usr/bin/web-server`.
   The debug summary labels this image "V1.3.5.19", but its SHA-256 (`5765…7A42`) and `ota_info` identify 1.3.5.22.
   It shows what the **stock client** sends. It does not prove how the server handles requests.
5. Open-source projects, October 2026:

   | Project | License | Notes |
   |---|---|---|
   | [3dg1luk43/ha_creality_ws](https://github.com/3dg1luk43/ha_creality_ws) | AGPL-3.0 | Real K1C traces, simulator, internal reference doc |
   | [hurricaneb/creality_k1_api](https://github.com/hurricaneb/creality_k1_api) | GPL-3.0 | Heartbeat handling |
   | [rathlinus/ha-creality-lan](https://github.com/rathlinus/ha-creality-lan) | none | Delta merge, slicer-derived `STATE_MAP` |
   | [DaviBe92/k2-websocket-re](https://github.com/DaviBe92/k2-websocket-re) | none | K2 example frames |

> **Licensing:** implement from the protocol facts in this document. **Do not copy code** from the (A)GPL/unlicensed projects.

Items marked **UNVERIFIED** still need evidence, especially from the **K2 family**; these additional captures are deferred beyond the completed Phase 0.4 fixture-baseline scope.

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
   - **FW 1.3.5.19, live sessions A–C: real dumps** (≈1 675 characters) with `connect:1`, real temperatures (≈21.8 °C), and `connectionCount:0`, followed by `{"connectionCount":1}`. Most checked-in fixtures also have real dumps (`connect:1`), 1 675–1 786 bytes. The November 10 boot fixture instead starts with a **1,426-byte / 76-field placeholder**, `connect:0`, empty temperature strings and `err:{}`; real deltas arrive before `connect:1` at #13, **18,191 ms after socket open**.
     Session B connected during print setup, and its dump already contained the job (`state:1`, `deviceState:1`, `printProgress:0`).
   ➜ The placeholder dump is **not sent on every connect**. The owner started the November 10 logger before printer boot; this reproduces the startup case, but does not prove that boot is the only trigger.
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
   | `retGcodeFileInfo2` (full file list with metadata, ≈ 740 B per file) | Earlier: ≈ 78 KB for 105 files. November 10 fixture: **79,463 bytes per frame, twice** | Unsolicited at boot #11/#12, before `connect:1`; other checked-in connects do not show it |
   | `totalJob` + `historyList` (50 entries) | **≈ 23.4 KB in checked-in fixtures**, sent twice in a row | At job start and job end |
   | `elapseVideoList` (50 entries) | **≈ 28.7 KB in checked-in fixtures**, sent twice in a row | After a job (timelapse list) |
   | `retGcodeFileInfo` | 6.7 KB for 33 files; grows with the file count | When another client (Creality Print) asks for the file list |
   | `linuxOtaInfo` | 233 B | 1.3.5.19, after connect (offered printer firmware: version and URL) |

   ➜ The client **must skip frames larger than its buffer without dropping the connection** (§4).
   The November 10 boot fixture reproduces the large file-list push without any client requests. Do not assume it follows every connect: retain streaming discard regardless of its trigger.
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
Fields may be `""` while the printer boots (verified in the November 10 placeholder fixture).
Parse every numeric field with a tolerant coercion: number → value; numeric string → value; `""`, `null`, or garbage → *unknown* (`has_value=false`).

| Field | Wire type (K1 Max) | Unit | Notes |
|---|---|---|---|
| `state` | int | enum (§5.1) | Job state |
| `deviceState` | int | 0 idle, 1 job busy, 3 bed leveling, 4 input shaping, 7 homing | VERIFIED K1 Max: `7` while idle during manual homing; `1` from job start until the job ends; `0` at stop/complete. November 10 fixtures associate `3` with screen-started Auto Bed Leveling and `4` with Input Shaping, with `state` staying 0. These mappings are not established for other models/firmware. |
| `autoLevelResult` | string | `<index>:<offset>` | November 10 fixtures verify 36 string updates from point 1 to 36 during menu-started leveling, including signed offsets. The last result remains afterwards. Use the index for "Leveling n/36" only while `deviceState == 3`, not as a new-job or active-leveling signal on its own. |
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
| `err` | object `{errcode:int, key:int[, value:string]}` | – | `value` absent on 1.3.3.x, present (`""`) on 1.3.5.19; boot placeholder may contain `{}`. November 9 records code 101/key 104 while paused, clearing during stopping. November 10 starts with the same nonzero payload already in the first valid dump and clears it before resume without stopping (§5.2b). Every nonzero error in valid data takes precedence over all job states, including PAUSED/PRINTING. |
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
| Receive buffer | **4 KB, static.** The largest status frame seen is ≈ 1.79 KB (1 786 bytes in the checked-in fixtures; the live-session dump was ≈ 1 675 characters); the K2 dump is ≈ 1.8 KB plus possible CFS keys. |
| Larger frames | **Stream-discard** the payload without allocating, and keep the connection. Record the first ≈ 32 bytes (e.g. `{"totalJob":`, `{"retGcodeFileInfo2":`) for diagnostics. Checked-in fixtures reach **79,463 bytes** in the November 10 boot file list, as well as the earlier 23.4–28.7 KB history/timelapse frames. The file-list size grows with the file count; do not treat the largest observation as an upper bound. |
| Fragmented frames | Assemble continuation frames while the total is ≤ buffer; otherwise discard the whole message |
| Control frames | `ping` → `pong`; `close` → close and reconnect |

> The `links2004/WebSockets` library closes the connection (code 1009) on any frame above `WEBSOCKETS_MAX_DATA_SIZE` (15 KB default) and `malloc`s whole frames otherwise.
> On 1.3.3.x it would disconnect at **every job start and end** (20 KB `historyList`).
> On 1.3.5.19 the checked-in fixtures show oversized `historyList` and `elapseVideoList` frames around job events and two 79,463-byte file lists during boot. The latter is not present on every connection. Heap churn would also compete with the GIF canvas.
> Hence the plan uses a small **custom streaming client** (plan §9.2).

## 5. State model

### 5.1 Raw `state` codes

| `state` | Meaning | Evidence |
|---|---|---|
| 0 | Idle, **or transient inside a job** (pausing, resuming, stopping on 1.3.3.x), or "job ended" with the file kept | VERIFIED K1 Max |
| 1 | Job running, **including** the self-test, heating, and preparation phases before layer 1 | VERIFIED K1 Max |
| 2 | **Completed** (`printProgress` becomes 100 in the same frame); later returns to 0 | **VERIFIED K1 Max**. About 15–60 later frames on 1.3.3.x; 1.3.5.19 fixtures include roughly 63 s and 733.479 s intervals (§5.2b), not a universal timeout. |
| 3 | Failed | UNVERIFIED (rathlinus) → treat as STOPPED, labelled "Print failed" |
| 4 | Stopped/cancelled (stop from the printer screen or the slicer). `deviceState`, `layer`, and `TotalLayer` become 0 in the same frame. | VERIFIED K1 Max |
| 5 | Paused | VERIFIED K1 Max (1.3.3.x and 1.3.5.19 live) |
| 6 | Pausing (between running and paused) | **VERIFIED K1 Max 1.3.5.19 live** (`1 → 6 → 5`) → PAUSED "Pausing…" |
| 7 | Busy while a cancel finishes its move | **VERIFIED K1 Max 1.3.5.19 fixtures**: appears before `state:4` on stop/cancel → PROCESSING "Stopping…" |
| 8 | Resuming from pause (between paused and running; the nozzle reheats). Sources also report it while restoring after power loss. | **VERIFIED K1 Max 1.3.5.19 live** (`5 → 8 → 1`) → PROCESSING "Resuming…" |
| 9 | ~0.5 s transient before a job starts | **VERIFIED K1 Max 1.3.5.19 fixtures**: repeatedly appears about 0.4–0.6 s before `state:1` → IDLE |

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
This matches 1.3.3.x. The live-session summary did not record the delay until `state:0`; a checked-in fixture records `state:0` about 63 s after `state:2` (§5.2b).

**Idle manual homing** (session A): `deviceState` 7 → 0, and `state` stays 0.
`autohome` goes `"X:0 Y:0 Z:0"` → `"X:1 Y:0 Z:0"` → `"X:1 Y:1 Z:0"` → `"X:1 Y:1 Z:1"`, with frequent `curPosition` deltas.

**Idle Auto Bed Leveling** (session A, started from the printer screen):

1. `deviceState:3`; `state` and `printProgress` stay 0 and no file is set.
2. The printer sets targets by itself: nozzle 140 → 180 → 240 → 140 °C, and bed 50 °C.
3. `autoLevelResult` goes `1:…` → `36:…`.
4. At the end, `deviceState:0` and both targets return to 0.

**Idle heating from the screen** (session A): only `targetNozzleTemp`/`targetBedTemp0` change. `state` and `deviceState` stay 0 -> IDLE; the revised sub-label distinguishes nozzle/bed heating until the selected heater first reaches its target (§5.4).

### 5.2b Checked-in fixtures (K1 Max FW 1.3.5.19)

The 14 receive-only JSONL captures in `test/fixtures` verify these additional details:

- `state:9` precedes `state:1` by about 0.4–0.6 s at job start. `state:7` appears during stop/cancel, followed by `state:4`.
- Print captures show `withSelfTest` steps 1–5 then 100 when step 5 is part of the run. Pause/resume includes `1 → 6 → 5 → 8 → 1`; stop/cancel includes `1 → 7 → 4`.
- A connect-during-print capture begins at layer 13 (`state:1`) and reaches `state:2`; `state:0` follows about 63 s later.
- Large captured frames include `totalJob`/`historyList` at 23.4 KB, `elapseVideoList` up to 28.7 KB, and two 79,463-byte `retGcodeFileInfo2` frames in the November 10 boot capture. That capture also contains the `connect:0` placeholder.
- The October 30 `error` fixture includes simulated power-loss and filament-runout flows, but `errcode` stays 0. It is not evidence of a nonzero `err` payload.
- The November 9 `error` fixture (`k1max_1.3.5.19_error_20261109-1120.jsonl`) records `errcode:101`, `key:104`, `value:""` while paused. An operator note reads "Foreign object detected on printing plate, printing paused"; this associates that payload with this observed condition, not a universal error-code mapping. One connection carries 1,205 JSON messages, three over 4,096 bytes (maximum 23,402 bytes); the final close is client-initiated at 697,768 ms.

November 9 error/stop sequence (times are capture-relative, not timing assertions in the expected file):

| Frame | Time (ms) | Received update | Prototype state |
|---|---|---|---|
| #1114 | 587,582 | `state:5` | `PAUSED` |
| #1115 | 587,638 | `err:{errcode:101,key:104,value:""}` | `ERROR` |
| #1159 | 657,861 | `state:7`, previous error still merged | `ERROR` (already active) |
| #1160 | 657,912 | `err:{errcode:0,key:0,value:""}` | `PROCESSING(Stopping)` |
| #1164 | 659,655 | `state:4`, `deviceState:0` | `STOPPED` |

The stop update precedes error clearing by **51 ms**, but the owner-approved November 10 rule reports `ERROR` from #1115 onward, including while paused. The earlier prototype excluded states 1/5 and therefore delayed `ERROR` until #1159; that baseline is superseded. The operator later noted "I have selected stop printing"; notes are not exact button timestamps. This capture proves clearing during a stop flow, not spontaneous clearing or acknowledgement/reconnect behavior.

**November 10: connect during print-start error, recover, finish, idle, menu leveling** (`k1max_1.3.5.19_error-resume-finish-idle-auto-leveling_20261110-0842.jsonl`): one connection, 2,833 JSON messages, one oversized 23,382-byte history frame, `tx:0`, no operator notes. The owner reports solving the fault manually and resuming.

| Frame | Time (ms) | Received/merged evidence | Revised prototype |
|---|---|---|---|
| #1 | 0 | First valid dump: `connect:1`, `state:5`, `deviceState:1`, `err:{errcode:101,key:104,value:""}`, self-test step 1, job already set | `ERROR` |
| #25 | 24,233 | Error clears to code/key 0; still paused | `PAUSED` |
| #26 | 24,280 | `state:1`, self-test step 1 | `SELF_TESTING(step 1)` |
| #738 | 370,017 | First derived printing state after self-test/preparation | `PRINTING` |
| #1313 | 500,923 | `state:2`, progress 100, `deviceState:0`, layers reset, targets 0, `printJobTime:137`; filename retained | `COMPLETED` |
| #1745 | 1,234,402 | `state:0`, progress still 100, old job retained | `COMPLETED` (logical result latch) |
| #1749 | 1,245,539 | `deviceState:3`, raw state remains 0; old filename/progress retained, elapsed time now 0 | `PROCESSING(Bed leveling)`; clear result latch |
| #2447 -> #2783 | 1,559,208 -> 1,755,004 | `autoLevelResult` strings `1:0.42` through `36:0.09` | Bed leveling, with separate heater sub-labels during preparation |
| #2791 | 1,756,964 | `deviceState:0`, both targets 0 | `IDLE` |
| #2833 / close | 1,804,760 (close) | Client closes the one uninterrupted connection | `OFFLINE` |

The initial dump proves that **this pre-existing error is kept and reported to a newly connecting client**; it is not lost merely because the error occurred before this connection. The error clears **47 ms before resume**, without stopping the job. Do not generalize this one code/flow to persistence over a printer reboot or every fault.

The observed raw `state:2 -> 0` interval is **733,479 ms = 12 min 13.479 s**; menu leveling starts **11,137 ms** after that raw idle update. The owner confirms the printer showed the print-complete screen and **OK was pressed soon after completion, not after twelve minutes**. There is no timestamped button marker. Do not attribute the late state:0 to a late OK press, claim an immediate flag change from OK, or call this interval a universal automatic timeout. The effect of that button on other status flags remains unresolved.

**November 10: boot, idle and complete calibration** (`k1max_1.3.5.19_boot-idle-complete-calibration_20261110-1011.jsonl`): logger started before printer boot; one connection, 1,797 JSON messages, `tx:0`, no operator notes. The owner associates idle `deviceState:4` with UI-started Input Shaping and `3` with Auto Bed Leveling; the reviewed baseline uses those labels for this K1 Max firmware.

| Frame | Time (ms) | Evidence / revised prototype |
|---|---|---|
| #1 | 52 | 1,426-byte / 76-field placeholder: `connect:0`, temperatures `""`, `err:{}` -> `NOT_READY` |
| #4/#5/#6 | 1,520 / 1,672 / 1,865 | Identity/version, `boxConfig`, then temperatures/motion/zero-error deltas; data still not ready |
| #8/#9/#10 | 7,844 / 12,810 / 12,878 | `linuxOtaInfo`, `tfCard`, then max-temperature capabilities |
| #11/#12 | 15,314 / 16,412 | Two unsolicited **79,463-byte** `retGcodeFileInfo2` frames; skipped by the 4 KiB replay limit |
| #13 | 18,191 | `connect:1` -> `IDLE` |
| #25 | 46,760 | `deviceState:4`, raw state stays 0 -> `PROCESSING(Input shaping)` |
| #822 | 296,657 | `deviceState:0` -> `IDLE` |
| #824 | 296,776 | `deviceState:3` -> `PROCESSING(Bed leveling)` |
| #1490 -> #1769 | 586,531 -> 781,950 | 36 `autoLevelResult` string updates, `1:0.45` through `36:0.10` |
| #1771 | 782,614 | `deviceState:0`, both targets 0 -> `IDLE` |
| #1797 / close | 796,334 (close) | Client closes -> `OFFLINE` |

The two large lists do not close this connection or prevent readiness. `boxConfig`, file-list metadata and other unused keys are not additions to the display whitelist. This idle boot capture does not establish whether an active job's fields are all re-sent after a placeholder; the need for a follow-up get remains unverified.

Replay command: `python -c "import glob; from tools.derive_prototype import main; main(sorted(glob.glob('test/fixtures/*.jsonl')))"`. Each fixture has a sibling `*.expected.txt` containing its reviewed frame-numbered logical-state sequence (prototype output with the printed source-path prefix removed). **All 14 match fresh replay and were owner-approved on 2026-11-10** after heater, calibration-latch and error-priority revisions. Five prior files remain identical; the other existing sequences were regenerated and the boot baseline added. `OFFLINE` appears at recorded close/reconnect boundaries. These baselines do not assert timing, validate free-text notes, model printer-button actions, or test a production C++ implementation. The timing tables above are separately extracted evidence. Run the **16 focused Python regression tests** with `python -m unittest discover -s test -p test_derive_prototype.py -v`.

### 5.3 Self-test steps (`withSelfTest`)

The owner confirmed these K1 Max self-test labels on **2026-11-09**, replacing the earlier temperature-based guesses. This confirmation does not establish labels for other printer models/firmware.

| Step | Telemetry observation | Owner-confirmed label |
|---|---|---|
| 1 | Brief, no temperature change | "Foreign object detection" |
| 2 | Targets 150/45 set; heating | "Homing" |
| 3 | Nozzle 190 → 200 → back to 150 | "Nozzle cleaning in progress" |
| 4 | At 150 °C / 45 °C | "Probing" |
| 5 | Only in the "with calibration" print | "Bed leveling" |
| 100 | Self-test finished (stays 100 during the print) | – |

Display "Self-test · step N" with the label. **Do not** draw it as a percentage arc.

### 5.4 KNOMI derivation rules (first match wins)

Session memory kept by the deriver:

- `prev`: the previous logical state
- `latch`: COMPLETED or STOPPED, entered live
- `trans_from`: the origin of an in-job `state:0` transition
- `job_id`: `printStartTime` plus `printFileName`
- Per-heater target and first-target-reach flags (nozzle and bed)

A change of `job_id` (with a non-empty file) clears `latch` and re-arms the heater flags. Any `state != 0` clears `trans_from`. On valid data, track each heater independently: a positive target is heating until its first observed actual temperature **>= target**, with no tolerance band. Once reached, later dips do not restart that heater. A changed target or target off/on re-arms it; if the actual is already above the new target it is immediately reached. Unknown temperatures do not prove completion. Nozzle heating wins over bed heating when both are pending, matching the legacy screen priority. Reconnect starts fresh observation history, not knowledge of a heater's earlier physical crossings.

| # | Condition | Logical state (sub-label) |
|---|---|---|
| 1 | WS not connected, or no frame for 15 s | `OFFLINE` |
| 1a | `connect == 0` (1.3.5.x placeholder dump; printer backend not ready) | `NOT_READY` ("Printer starting…"). No values are displayed. |
| 2 | Valid printer data with `errcode != 0`, regardless of raw job state or local acknowledgement | `ERROR`; always report it, including while PAUSED/PRINTING/self-testing |
| 3 | `state==1` and `withSelfTest` in 1..99 | `SELF_TESTING` ("Step N · label"; append selected nozzle/bed heating sub-label while pending) |
| 4 | `state==1` and `layer==0` and progress == 0 | `PROCESSING` ("Nozzle heating" / "Bed heating" / "Preparing") |
| 5 | `state==1` | `PRINTING` (append selected heater while pending) |
| 6 | `state` ∈ {5,6} | `PAUSED` ("Pausing…" for 6) |
| 7 | `state==2` | `COMPLETED`; set `latch` |
| 8 | `state` ∈ {3,4} and (`prev` is busy or `latch==STOPPED`) | `STOPPED`; set `latch` ("Print failed" for 3) |
| 9 | `state` ∈ {7,8} | `PROCESSING` ("Stopping…" / "Resuming…"; append selected heater for Resuming) |
| 9a | `state==0` and `deviceState` is 3, 4 or 7 | Clear any result latch; `PROCESSING` ("Bed leveling" / "Input shaping" / "Homing", with selected heater appended). This must precede the stale-result latch. |
| 10 | `state==0` and `latch` is set | the latched state (until `result_timeout`, a new job, or a button click) |
| 11 | `state==0`, file set, progress < 100, `deviceState==1`, and (`trans_from` is set or `prev` is busy) | origin PRINTING → `PRINTING` ("Pausing / stopping…"); origin PAUSED → `PROCESSING` ("Resuming…") |
| 12 | `state==0`, file set, progress < 100, `deviceState==0`, `layer==0`, `TotalLayer==0`, and `prev` is busy | `STOPPED`; set `latch` (WS-stop shape) |
| 13 | `deviceState==7` | `PROCESSING` ("Homing") |
| 13a | `deviceState==3` | `PROCESSING` ("Bed leveling", optionally "n/36" from the `autoLevelResult` index). Normal idle calibration is handled earlier by 9a. |
| 14 | otherwise | `IDLE` (state 0/3/4/9 without a live transition, stale results; append selected heater during idle heating) |

Additional rules:

- "Busy" means PROCESSING, SELF_TESTING, PRINTING, or PAUSED.
- **Nonzero errors always win** once data is ready. Do not replace ERROR with a paused/printing job page or suppress it after a local acknowledgement; a badge alone does not meet the owner's requirement. OFFLINE and invalid `connect:0` data still take precedence over stale/error values.
- **Printer error versus local acknowledgement:** the owner expects naturally occurring errors to require printer-side interaction. Captures prove clearing during the November 9 stop flow and November 10 manual recovery/resume, not a universal clearing rule. A KNOMI acknowledgement may mark the error seen locally but sends no command, does not clear `err`, and must not dismiss the active logical ERROR. Automatic clearing, power-cycle persistence and other faults remain unverified.
- **Heater labels:** keep self-test/calibration/resume as the primary operation and append the selected heater, e.g. `SELF_TESTING(step 3; Nozzle heating)` or `PROCESSING(Bed leveling; Bed heating)`. Calibration does not become an ordinary print merely because the old filename and progress 100 remain. The Python prototype models these labels, not the heated GIF timing.
- **IDLE detail:** if any target > 0, show the temperature page (do not show the GIF). Label "Nozzle heating" or "Bed heating" only while that first-reach phase is pending; after both reach target show temperatures without falsely restarting heating on jitter. If a file is set with progress 100 (or `state` was 4), show "Last: <file> ✓/✕" on the idle temperature page.
- **On connect:** the first *valid* data (the dump with `connect:1`, or the `{"connect":1}` delta on 1.3.5.x) has no live history (`prev` = OFFLINE/NOT_READY, both not busy).
  - `state:2` → COMPLETED only if NTP is valid and `now − (printStartTime + printJobTime) < result_timeout`; otherwise IDLE with "Last ✓".
  - `state:4` → IDLE with "Last ✕".
  - When `connect` changes 0 → 1, send **one** `ReqPrinterPara` get. This is a safety net in case job fields are not re-sent after the placeholder dump; whether it is needed is UNVERIFIED.
- **Debounce:** 700 ms before the UI switches, except `OFFLINE` and `ERROR`, which switch immediately. `NOT_READY` → IDLE does not need debouncing.

### 5.5 Prototype validation

A Python prototype ([tools/derive_prototype.py](../tools/derive_prototype.py); to be ported to `lib/creality_model` with unit tests) originally produced the following **historical/coarse summaries** from the legacy owner captures listed in §1. They predate the November 10 separate heater labels, calibration-latch and unconditional error-priority refinements. The legacy raw files are not checked in, so these summaries were not regenerated and are **not** current fixture replay baselines. All checked-in fixtures are FW 1.3.5.19; see §5.2b for their reviewed outputs/evidence.

```
full_print:  IDLE → PROCESSING(Preparing) → SELF_TESTING(1→2→3→4→5) → PROCESSING(Preparing→Heating→Preparing) → PRINTING → COMPLETED
just_print:  IDLE → PROCESSING(Homing) → IDLE → PROCESSING(Homing) → IDLE → PROCESSING(Preparing) → SELF_TESTING(1→2→3→4)
             → PROCESSING(Preparing→Heating→Preparing) → PRINTING → COMPLETED
pause_print: IDLE → PROCESSING → SELF_TESTING(1→4) → PROCESSING → PRINTING → PRINTING(Pausing/stopping) → PAUSED
             → PROCESSING(Resuming) → PRINTING → PRINTING(Pausing/stopping) → STOPPED
in_print:    PRINTING → COMPLETED (latched through state 0 and the later idle heating)
idle (1.3.5.19): NOT_READY → IDLE (at the {"connect":1} delta; the two 78 KB file-list frames are skipped)
```

For the 2026-10-20 1.3.5.19 live sessions (§5.2a), only a written summary exists, so these are **expected** outputs. The later checked-in fixtures provide separate raw traces:

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

Connect sequence on K1 Max FW 1.3.5.19 (idle), from an earlier capture. November 10 independently reproduces the placeholder/list/readiness pattern with different values and sizes (§5.2b); this remains the earlier example, not a verbatim November 10 dump. Large frames are shortened.

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

## 7. Open points (deferred pending additional evidence)

Phase 0.4 fixture review was extended to **14 K1 Max FW 1.3.5.19 captures** on 2026-11-10. The owner approved all regenerated expected sequences and the revised heater/calibration/error rules. The two new captures resolve several earlier gaps below; remaining unchecked points are deferred, not blockers to this fixture-review scope and not claims of production firmware validation.

- [x] Startup placeholder and readiness captured: November 10 boot #1 `connect:0`, then #13 `connect:1`, with identity/temperature/capability deltas in between. The initial temperatures are empty strings, not necessarily numeric zero.
- [ ] **Active-job fields after a placeholder / need for a follow-up get:** the captured boot is idle. It does not settle completeness of job-field resend or server handling of a client request.
- [x] Unsolicited `retGcodeFileInfo2` reproduced: two 79,463-byte frames in the boot fixture before readiness, with `tx:0`.
- [ ] Exact file-list/placeholder triggers on other connects/firmware and future list sizes. Keep discard handling independent of the largest observed size.
- [x] Menu calibration represented: idle `deviceState:4` associated with Input Shaping and `3` with Auto Bed Leveling; 36 string point updates captured. Mapping is specific to the reviewed K1 Max firmware.
- [ ] State `8` after power loss and unobserved `deviceState` values/models. Preserve tolerant unknown-state behavior beyond observed 0/1/3/4/7.
- [ ] **K2 family and K1/K1C captures.** **Impact:** cross-model state, self-test and chamber-field compatibility is unverified; current fixture evidence is K1 Max only.
- [x] A nonzero `err` payload is captured: November 9 #1115 records `errcode:101`, `key:104` while paused, with an operator-reported foreign-object condition. #1160 clears it during stopping; see §5.2b.
- [x] A pre-existing nonzero error is reported to a new connection and clears without stopping: November 10 #1 code 101/key 104, #25 clears, #26 resumes after owner-reported manual recovery. Always surface ERROR even if paused/printing.
- [ ] Other errors, spontaneous clearing, exact printer-acknowledgement effects and power-cycle persistence. KNOMI local acknowledgement never clears/suppresses the logical fault.
- [x] Self-test step names supplied/revised by the owner; the current labels are maintained in §5.3 rather than duplicated here.
- [x] Completion-to-raw-idle interval measured in the new capture: **733,479 ms**, alongside the earlier roughly 63 s case. Owner pressed completion OK soon after finishing, not at the late state:0 transition.
- [ ] Exact completion-screen OK effects, causes/variability of the `state:2 -> 0` delay, elapsed-time reset timing, and more stop/phase scenarios across firmware/models. There is no exact OK marker; expected files assert frame-indexed states only, not timing or causality.
- [x] ~~Heartbeat required?~~ No. A 1.3.5.19 client that sent nothing stayed connected for 20 min. Whether the printer itself sends a heartbeat to third-party clients is still unknown for other firmware.
