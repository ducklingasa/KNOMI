"""Validate Phase 0.5 assets; optionally compile them using the S3 build database."""

import argparse
import hashlib
import io
import json
import re
import subprocess
from pathlib import Path

from PIL import Image

from bin2c import gif_info


ROOT = Path(__file__).resolve().parents[1]
GIF_NAMES = (
    "creality", "standby_green", "wifi", "homing_green", "probing_green",
    "qgling_green", "heated_green", "print_green", "print_ok", "printed_green",
)
FONT_SIZES = (14, 18, 24, 48)
FONT_SHA256 = "78a843fade9d4612a5567302fb595b56976eb5fcebf4fea5a5912d638bafcde3"


def field(source, name):
    match = re.search(r"\." + re.escape(name) + r"\s*=\s*(\w+)", source)
    if match is None:
        raise ValueError(f"Missing descriptor field: {name}")
    return match.group(1)


def check_gifs():
    total_bytes = 0
    maximum_canvas = 0
    for name in GIF_NAMES:
        symbol = f"gif_{name}"
        source = (ROOT / "src/gif" / f"{symbol}.c").read_text(encoding="utf-8")
        array = re.search(
            r"uint8_t\s+" + re.escape(field(source, "data")) + r"\[\]\s*=\s*\{(.*?)\};",
            source, re.DOTALL,
        )
        if array is None:
            raise ValueError(f"{symbol}: missing byte array")
        data = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", array.group(1)))
        original = (ROOT / "docs/screens/gif" / f"{name}.gif").read_bytes()
        if data != original:
            raise ValueError(f"{symbol}: C array differs from the source GIF")
        width, height, frames, duration = gif_info(data)
        if (int(field(source, "header.w")), int(field(source, "header.h"))) != (width, height):
            raise ValueError(f"{symbol}: descriptor dimensions differ from GIF")
        if int(field(source, "data_size")) != len(data):
            raise ValueError(f"{symbol}: descriptor byte count differs from GIF")
        if field(source, "header.cf") != "LV_IMG_CF_RAW_CHROMA_KEYED":
            raise ValueError(f"{symbol}: unexpected LVGL color format")
        with Image.open(io.BytesIO(data)) as image:
            if image.info.get("loop") != 0:
                raise ValueError(f"{symbol}: missing infinite-loop extension")
            if image.n_frames != frames:
                raise ValueError(f"{symbol}: frame counts differ between parsers")
            decoded_duration = 0
            for frame in range(frames):
                image.seek(frame)
                image.load()
                decoded_duration += image.info.get("duration", 0)
            if decoded_duration != duration:
                raise ValueError(f"{symbol}: frame duration totals differ between parsers")
        canvas = 4 * width * height
        if canvas > 90_000:
            raise ValueError(f"{symbol}: {canvas} bytes exceeds the 90,000-byte canvas budget")
        total_bytes += len(data)
        maximum_canvas = max(maximum_canvas, canvas)
        print(f"{symbol}: {width}x{height}, {frames} frames, {duration} ms, "
              f"flash={len(data)} bytes, canvas={canvas} bytes; PASS")
    if total_bytes > 470 * 1024:
        raise ValueError(f"GIF flash total {total_bytes} bytes exceeds 470 KiB")
    print(f"PASS: {len(GIF_NAMES)} GIFs, flash={total_bytes} bytes "
          f"({total_bytes / 1024:.2f} KiB), maximum canvas={maximum_canvas} bytes")


def font_codepoints(source):
    cmaps = re.search(r"\bcmaps\[\]\s*=\s*\{(.*?)\};", source, re.DOTALL)
    if cmaps is None:
        raise ValueError("Missing LVGL font character maps")
    rows = re.findall(r"\{([^{}]*)\}", cmaps.group(1), re.DOTALL)
    if len(rows) != int(field(source, "cmap_num")):
        raise ValueError("Font character map count mismatch")
    points = set()
    for row in rows:
        start = int(field(row, "range_start"))
        length = int(field(row, "range_length"))
        kind = field(row, "type")
        if int(field(row, "glyph_id_start")) == 0 or field(row, "glyph_id_ofs_list") != "NULL":
            raise ValueError("Unexpected font glyph indexing")
        if kind == "LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY":
            offsets = list(range(length))
        elif kind == "LV_FONT_FMT_TXT_CMAP_SPARSE_TINY":
            name = field(row, "unicode_list")
            array = re.search(r"\b" + re.escape(name) + r"\[\]\s*=\s*\{(.*?)\};", source, re.DOTALL)
            if array is None:
                raise ValueError(f"Missing sparse font map: {name}")
            offsets = [int(value, 0) for value in re.findall(r"0x[0-9a-fA-F]+|\d+", array.group(1))]
            if len(offsets) != int(field(row, "list_length")):
                raise ValueError("Sparse font map length mismatch")
        else:
            raise ValueError(f"Unsupported font character map type: {kind}")
        if any(offset < 0 or offset >= length for offset in offsets):
            raise ValueError("Font character map offset outside its range")
        points.update(start + offset for offset in offsets)
    return points


def check_fonts():
    font = ROOT / "tools/fonts/Inter-SemiBold.ttf"
    if hashlib.sha256(font.read_bytes()).hexdigest() != FONT_SHA256:
        raise ValueError("Inter SemiBold source differs from the pinned 4.1 font")
    for size in FONT_SIZES:
        symbol = f"ui_font_InterSemiBold{size}"
        source = (ROOT / "src/ui/fonts" / f"{symbol}.c").read_text(encoding="utf-8")
        if not re.search(r"\.bpp\s*=\s*4\b", source):
            raise ValueError(f"{symbol}: not 4 bpp")
        if not re.search(r"\blv_font_t\s+" + symbol + r"\s*=", source):
            raise ValueError(f"{symbol}: public font symbol missing")
        actual = font_codepoints(source)
        annotated = {int(value, 16) for value in re.findall(r"U\+([0-9A-Fa-f]{4,6})", source)}
        if annotated != actual:
            raise ValueError(f"{symbol}: glyph annotations differ from LVGL character maps")
        if size in (14, 18):
            expected = (set(range(0x20, 0x7F)) | set(range(0xA0, 0x180)) | {0x2103}) - {0xAD, 0x149}
        elif size == 24:
            expected = set(range(0x20, 0x7F)) | {0xB0, 0x2103}
        else:
            expected = set(range(0x30, 0x3B)) | {0x20, 0x25, 0x43, 0xB0, 0x2103}
        if actual != expected:
            missing = ", ".join(f"U+{point:04X}" for point in sorted(expected - actual))
            extra = ", ".join(f"U+{point:04X}" for point in sorted(actual - expected))
            raise ValueError(f"{symbol}: glyph coverage mismatch; missing={missing}; extra={extra}")
        print(f"{symbol}: 4 bpp, {len(actual)} glyphs; PASS")


def compile_assets(build_dir):
    entries = json.loads((build_dir / "compile_commands.json").read_text(encoding="utf-8"))
    entry = next(item for item in entries if item["file"].endswith("gif_creality.c"))
    sources = [ROOT / "src/gif" / f"gif_{name}.c" for name in GIF_NAMES]
    sources += [ROOT / "src/ui/fonts" / f"ui_font_InterSemiBold{size}.c" for size in FONT_SIZES]
    for source in sources:
        command = re.sub(r' -(?:o|MF|MT) (?:"[^"]*"|\S+)', "", entry["command"])
        command = re.sub(r" -M(?:M)?D\b", "", command)
        command, count = re.subn(
            r' -c (?:"[^"]*"|\S+)', f' -fsyntax-only -c "{source.as_posix()}"', command,
        )
        if count != 1:
            raise ValueError("Could not identify source in the S3 compile command")
        result = subprocess.run(
            command, cwd=entry["directory"], capture_output=True, text=True,
            encoding="utf-8", errors="replace",
        )
        if result.returncode:
            raise RuntimeError(f"{source.name}: compile failed\n{result.stdout}{result.stderr}")
        print(f"PASS ESP32-S3/LVGL compile: {source.name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compile", action="store_true", help="Use the configured Windows S3 compile database")
    parser.add_argument("--build-dir", type=Path, default=ROOT / "spikes/display/build/s3")
    args = parser.parse_args()
    check_gifs()
    check_fonts()
    if args.compile:
        compile_assets(args.build_dir)


if __name__ == "__main__":
    main()