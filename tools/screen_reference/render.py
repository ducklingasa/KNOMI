"""Export native LVGL captures and check the preserved screen references."""

import argparse
import hashlib
import json
import re
import subprocess
import sys
import zipfile
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFont, __version__ as pillow_version


ROOT = Path(__file__).resolve().parents[2]
LVGL_SOURCE = ROOT / "managed_components/lvgl__lvgl"
if not LVGL_SOURCE.exists():
    LVGL_SOURCE = ROOT / "spikes/display/managed_components/lvgl__lvgl"


def scenarios():
    result = []
    for screen in ("nozzle", "bed"):
        result.extend((f"{screen}_{value:03d}", screen, value, 0) for value in (0, 25, 50, 100, 125))
        result.append((f"{screen}_zero_target", screen + "_zero", 0, 0))
    result.extend((f"print_v1_{value:03d}", "print_v1", value, 0) for value in (0, 57, 100))
    result.append(("print_v2_057_reference_only", "print_v2", 57, 0))
    for screen in ("main_gif", "welcome", "wifi_connecting"):
        times = {"main_gif": (600, 1200), "welcome": (40, 1240), "wifi_connecting": (0, 1200)}[screen]
        result.extend((f"{screen}_{time:04d}ms", screen, 0, time) for time in times)
    result.extend((screen, screen, 0, 0) for screen in ("temperatures", "wifi_lost", "qr", "info", "popup", "dialog"))
    return result


def pixels(image):
    raw = image.convert("RGB").tobytes()
    return list(zip(raw[0::3], raw[1::3], raw[2::3]))


def source_hashes():
    archive_path = ROOT / "docs/screens/original/legacy-source.zip"
    if archive_path.exists():
        manifest = json.loads(archive_path.with_suffix(".json").read_text(encoding="utf-8"))
        if hashlib.sha256(archive_path.read_bytes()).hexdigest() != manifest["sha256"]:
            raise ValueError("Legacy source archive hash mismatch")
        with zipfile.ZipFile(archive_path) as archive:
            for name, expected in manifest["files"].items():
                if hashlib.sha256(archive.read(name)).hexdigest() != expected:
                    raise ValueError(f"Legacy source mismatch: {name}")
        hashes = dict(manifest["files"])
    else:
        paths = list((ROOT / "src/ui/screens").glob("*.c"))
        paths += list((ROOT / "src/ui/images").glob("*.c"))
        paths += list((ROOT / "src/ui/fonts").glob("*.c"))
        hashes = {path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in sorted(set(paths))}
    paths = [ROOT / path for path in (
        "tools/screen_reference/main.c", "tools/screen_reference/lv_conf.h",
        "tools/screen_reference/CMakeLists.txt", "tools/screen_reference/render.py", "tools/screen_reference/freeze.py",
    )]
    hashes.update({path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                   for path in paths})
    return hashes


def contact_sheet(output, captures):
    width, height = 272, 280
    rows = (len(captures) + 3) // 4
    sheet = Image.new("RGB", (4 * width, rows * height + 40), "#202020")
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype(str(ROOT / "tools/fonts/Inter-SemiBold.ttf"), 13)
    draw.text((16, 10), "Native LVGL references - current legacy source, not device photos", font=font, fill="white")
    for index, capture in enumerate(captures):
        left = (index % 4) * width + 16
        top = (index // 4) * height + 40
        with Image.open(output / capture["round_png"]) as image:
            sheet.paste(image, (left, top), image)
        draw.text((left, top + 244), capture["name"], font=font, fill="white")
    sheet.save(output / "contact-sheet.png")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=Path(__file__).parent / "build/zig/screen_reference.exe")
    parser.add_argument("--output", type=Path, default=ROOT / "docs/screens/original")
    parser.add_argument("--lvgl-source", type=Path, default=LVGL_SOURCE)
    args = parser.parse_args()
    versions = dict(re.findall(r"#define\s+LVGL_VERSION_(MAJOR|MINOR|PATCH)\s+(\d+)",
                              (args.lvgl_source / "lvgl.h").read_text(encoding="utf-8")))
    version = ".".join(versions.get(part, "?") for part in ("MAJOR", "MINOR", "PATCH"))
    if version != "8.4.0":
        raise ValueError(f"Reference LVGL version must be 8.4.0, found {version}")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    mask = Image.new("L", (240, 240))
    ImageDraw.Draw(mask).ellipse((0, 0, 239, 239), fill=255)
    captures = []
    for name, screen, value, time in scenarios():
        ppm = args.executable.resolve().parent / "capture.ppm"
        result = subprocess.run([str(args.executable.resolve()), screen, str(value), str(time), str(ppm)],
                                check=True, cwd=ROOT, capture_output=True, text=True)
        if result.stdout.strip() or result.stderr.strip():
            raise RuntimeError(f"Unexpected renderer diagnostics for {name}: {result.stdout}{result.stderr}")
        with Image.open(ppm) as original:
            image = original.convert("RGB")
        if image.size != (240, 240) or len(set(pixels(image))) < 2:
            raise ValueError(f"Blank or invalid capture: {name}")
        png = name + ".png"
        round_png = name + ".round.png"
        image.save(output / png)
        rounded = image.convert("RGBA")
        rounded.putalpha(mask)
        rounded.save(output / round_png)
        red = sum(red > 80 and red > green * 1.5 and red > blue * 1.5 for red, green, blue in pixels(image))
        captures.append({"name": name, "screen": screen, "value": value, "time_ms": time,
                         "png": png, "round_png": round_png, "red_pixels": red,
                         "png_sha256": hashlib.sha256((output / png).read_bytes()).hexdigest()})
    by_name = {capture["name"]: capture for capture in captures}
    for screen in ("nozzle", "bed"):
        levels = [by_name[f"{screen}_{value:03d}"]["red_pixels"] for value in (0, 25, 50, 100)]
        if levels != sorted(levels) or levels[-1] < levels[0] + 100:
            raise ValueError(f"Thermal red does not increase with fill for {screen}: {levels}")
    for screen in ("main_gif", "welcome", "wifi_connecting"):
        frames = [capture for capture in captures if capture["screen"] == screen]
        with Image.open(output / frames[0]["png"]) as before, Image.open(output / frames[1]["png"]) as after:
            if ImageChops.difference(before, after).getbbox() is None:
                raise ValueError(f"Animation did not advance: {screen}")
    manifest = {
        "kind": "native-lvgl-legacy-source-reference", "lvgl": version,
        "python": sys.version.split()[0], "pillow": pillow_version,
        "resolution": [240, 240], "color_depth": 16, "color_16_swap": 0,
        "theme": "legacy default dark, blue primary/red secondary; no target recoloring",
        "note": "Current source with regenerated fonts; no input devices, inert event callbacks, synthetic values. Not device photos or the new UI.",
        "executable_sha256": hashlib.sha256(args.executable.read_bytes()).hexdigest(),
        "source_sha256": source_hashes(), "captures": captures,
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    contact_sheet(output, captures)
    print(f"PASS: {len(captures)} native captures, dimensions/content, increasing red heater fill, GIF/spinner motion and source manifest")
    print(f"Gallery: {(output / 'contact-sheet.png').relative_to(ROOT) if output.is_relative_to(ROOT) else output / 'contact-sheet.png'}")


if __name__ == "__main__":
    main()