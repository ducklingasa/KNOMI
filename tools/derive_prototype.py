"""Reference prototype of the KNOMI state derivation (docs/CREALITY_WS_PROTOCOL.md section 5.4).

Replays printer captures and prints the derived logical state sequence.
Accepted inputs:
  - knomi-ws-capture JSONL (plan section 4.2; written by tools/WebSocketLogger.html and tools/convert_capture.py).
    Only "rx" events are replayed; a "close" event emits OFFLINE and restarts derivation, as a reconnecting KNOMI does.
  - Legacy text lines "MESSAGE <- {json}" or "> RECEIVED: {json}", or raw JSONL ({json} per line).
Frames are numbered #n in order of received JSON objects (text starting with "{").
Frames larger than 4096 bytes are skipped, as the firmware client does.

Usage: python tools/derive_prototype.py <capture> [<capture> ...]
"""
import json
import sys

BUSY = {"PRINTING", "PAUSED", "PROCESSING", "SELF_TESTING"}
MAX_FRAME = 4096
CAPTURE_FORMAT = "knomi-ws-capture"


def num(v):
    if isinstance(v, (int, float)):
        return v
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


class Deriver:
    def __init__(self):
        self.m = {}
        self.prev = "OFFLINE"
        self.latch = None
        self.job = None
        self.sub = ""
        self.trans = None
        self.heaters = {}

    def heating(self):
        active = []
        for name, actual_key, target_key in (
            ("Nozzle", "nozzleTemp", "targetNozzleTemp"),
            ("Bed", "bedTemp0", "targetBedTemp0"),
        ):
            target = num(self.m.get(target_key))
            actual = num(self.m.get(actual_key))
            if target is None or target <= 0:
                self.heaters.pop(name, None)
                continue
            previous_target, reached = self.heaters.get(name, (None, False))
            if target != previous_target:
                reached = False
            if actual is not None and actual >= target:
                reached = True
            self.heaters[name] = (target, reached)
            if actual is not None and not reached:
                active.append(name)
        return f"{active[0]} heating" if active else ""

    def derive(self):
        m = self.m
        st = m.get("state")
        dev = m.get("deviceState", 0)
        wst = m.get("withSelfTest", 0)
        f = m.get("printFileName", "")
        prog = m.get("printProgress", 0)
        lay = m.get("layer", 0)
        tot = m.get("TotalLayer", 0)
        err = (m.get("err") or {}).get("errcode", 0)
        job_id = (m.get("printStartTime"), f)
        if f and job_id != self.job:
            if self.job is not None:
                self.latch = None
                self.heaters.clear()
            self.job = job_id
        self.sub = ""
        if st != 0:
            self.trans = None
        if m.get("connect", 1) == 0:
            return "NOT_READY"
        heating = self.heating()
        if err:
            return "ERROR"
        if st == 1 and 1 <= wst <= 99:
            self.sub = f"step {wst}" + (f"; {heating}" if heating else "")
            return "SELF_TESTING"
        if st == 1:
            self.latch = None
            if lay == 0 and prog == 0:
                self.sub = heating or "Preparing"
                return "PROCESSING"
            self.sub = heating
            return "PRINTING"
        if st in (5, 6):
            self.latch = None
            if st == 6:
                self.sub = "Pausing"
            return "PAUSED"
        if st == 2:
            self.latch = "COMPLETED"
            return "COMPLETED"
        if st in (3, 4):
            if self.prev in BUSY or self.latch == "STOPPED":
                self.latch = "STOPPED"
                return "STOPPED"
            return "IDLE"
        if st in (7, 8):
            self.sub = "Stopping" if st == 7 else "Resuming"
            if st == 8 and heating:
                self.sub += f"; {heating}"
            return "PROCESSING"
        if st == 0 and dev in (3, 4, 7):
            self.latch = None
            self.sub = {3: "Bed leveling", 4: "Input shaping", 7: "Homing"}[dev]
            if heating:
                self.sub += f"; {heating}"
            return "PROCESSING"
        if self.latch:
            return self.latch
        if f and prog < 100 and dev == 1 and (self.trans or self.prev in BUSY):
            if self.trans is None:
                self.trans = "PAUSED" if self.prev == "PAUSED" else "PRINTING"
            if self.trans == "PAUSED":
                self.sub = "Resuming"
                return "PROCESSING"
            self.sub = "Pausing/stopping"
            return "PRINTING"
        if f and prog < 100 and dev == 0 and lay == 0 and tot == 0 and self.prev in BUSY:
            self.latch = "STOPPED"
            return "STOPPED"
        if dev == 7:
            self.sub = "Homing"
            return "PROCESSING"
        if dev == 3:
            self.sub = "Bed leveling"
            return "PROCESSING"
        self.sub = heating
        return "IDLE"


def is_capture_header(line):
    try:
        head = json.loads(line)
    except json.JSONDecodeError:
        return False
    return isinstance(head, dict) and head.get("type") == "meta" and head.get("format") == CAPTURE_FORMAT


def events(path):
    """Yield ("rx", text) for each received JSON-object frame and ("close", None) at each connection end."""
    capture = None
    with open(path, encoding="utf-8-sig", errors="replace") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            if capture is None:
                capture = is_capture_header(line)
                if capture:
                    continue
            if capture:
                try:
                    ev = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if ev.get("type") == "rx" and isinstance(ev.get("data"), str):
                    body = ev["data"].strip()
                    if body.startswith("{"):
                        yield "rx", body
                elif ev.get("type") == "close":
                    yield "close", None
                continue
            for prefix in ("MESSAGE <- ", "> RECEIVED: "):
                if line.startswith(prefix):
                    line = line[len(prefix):]
                    break
            if line.startswith("{"):
                yield "rx", line


def main(paths):
    for path in paths:
        d = Deriver()
        last = None
        seq = []
        n = 0
        for kind, body in events(path):
            if kind == "close":
                if last is not None and last[0] != "OFFLINE":
                    seq.append(f"#{n}:OFFLINE")
                    last = ("OFFLINE", "")
                d = Deriver()
                continue
            n += 1
            if len(body.encode("utf-8")) > MAX_FRAME:
                continue
            try:
                obj = json.loads(body)
            except json.JSONDecodeError:
                continue
            if not isinstance(obj, dict) or "ModeCode" in obj:
                continue
            d.m.update(obj)
            state = d.derive()
            key = (state, d.sub)
            if key != last:
                seq.append(f"#{n}:{state}" + (f"({d.sub})" if d.sub else ""))
                last = key
            d.prev = state
        print(f"{path}:\n  " + " -> ".join(seq))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    main(sys.argv[1:])
