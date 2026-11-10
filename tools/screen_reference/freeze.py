"""Freeze only non-secret legacy visual sources before the Phase 1 cleanup."""

import hashlib
import json
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "docs/screens/original"


def main():
    files = [path for path in (ROOT / "src/ui").rglob("*") if path.is_file()]
    files += [ROOT / name for name in (
        "src/lvgl_usr.cpp", "src/ui_overlay/lv_theme_color.cpp",
        "src/ui_overlay/lv_moonraker_change_screen.cpp", "src/ui_overlay/lv_wifi_change_screen.cpp",
        "src/gif/gif_wifi.c", "src/gif/gif_voron.c",
    )]
    if not files or not (ROOT / "src/ui/ui.h").exists():
        raise ValueError("Live legacy sources are required to create the initial bundle")
    hashes = {}
    archive_path = OUTPUT / "legacy-source.zip"
    with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(set(files)):
            name = path.relative_to(ROOT).as_posix()
            data = path.read_bytes()
            entry = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, data)
            hashes[name] = hashlib.sha256(data).hexdigest()
    with zipfile.ZipFile(archive_path) as archive:
        for name, expected in hashes.items():
            if hashlib.sha256(archive.read(name)).hexdigest() != expected:
                raise ValueError(f"Reference bundle failed verification: {name}")
    manifest = {
        "kind": "legacy-visual-source-preservation", "archive": archive_path.name,
        "sha256": hashlib.sha256(archive_path.read_bytes()).hexdigest(),
        "note": "Only generated UI, selected visual overlays and two original GIF C sources. No SDK config, network credentials or compiled binaries.",
        "files": hashes,
    }
    (OUTPUT / "legacy-source.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"PASS: frozen and byte-verified {len(hashes)} visual source files; archive SHA256={manifest['sha256']}")


if __name__ == "__main__":
    main()