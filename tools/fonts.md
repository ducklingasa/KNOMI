# Phase 0.5 Asset Generation

Run commands from the repository root. This prepares assets only; it does not migrate the full firmware, change the display workload, flash COM8, or validate the future screens visually.

## Pinned Inputs

- Inter **4.1**, standard **Inter SemiBold** face, from https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip (`extras/ttf/Inter-SemiBold.ttf`, not Inter Display).
- Unmodified source: [Inter-SemiBold.ttf](fonts/Inter-SemiBold.ttf), SHA-256 `78a843fade9d4612a5567302fb595b56976eb5fcebf4fea5a5912d638bafcde3`.
- Upstream archive SHA-256: `9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e`. These hashes identify the downloaded inputs; they are not an upstream signature verification.
- SIL Open Font License 1.1: [OFL.txt](fonts/OFL.txt). Retain it when distributing the font source or generated font assets.
- Font converter: npm `lv_font_conv@1.5.3`; package URL https://registry.npmjs.org/lv_font_conv/-/lv_font_conv-1.5.3.tgz.
- Asset validation: Python with `Pillow==12.0.0` (`python -m pip install Pillow==12.0.0`). Validated with Python 3.14.6 on Windows.

The upstream font and license are included, so font regeneration does not need another font download. npm may download the pinned converter and its dependencies. Use normal retries after network processing; after several failures report package name, version, attempted URL and error, without bypassing TLS/network controls.

## Generate Fonts

PowerShell:

```powershell
$jobs = @(
    @{ Size = 14; Range = '0x20-0x7E,0xA0-0x17F,0x2103' },
    @{ Size = 18; Range = '0x20-0x7E,0xA0-0x17F,0x2103' },
    @{ Size = 24; Range = '0x20-0x7E,0xB0,0x2103' },
    @{ Size = 48; Range = '0x20,0x25,0x30-0x3A,0x43,0xB0,0x2103' }
)
foreach ($job in $jobs) {
    $name = 'ui_font_InterSemiBold' + $job.Size
    npm.cmd exec --yes --package=lv_font_conv@1.5.3 -- lv_font_conv `
        --size $job.Size --bpp 4 --format lvgl `
        --font tools/fonts/Inter-SemiBold.ttf --range $job.Range `
        --no-compress --no-prefilter --lv-include lvgl.h `
        --lv-font-name $name --output ('src/ui/fonts/' + $name + '.c')
    if ($LASTEXITCODE -ne 0) { throw "Conversion failed: $name" }
}
```

| Size | Generated Symbol | Coverage | Glyphs |
|---|---|---|---|
| 14 / 18 px | `ui_font_InterSemiBold14` / `ui_font_InterSemiBold18` | Printable ASCII, Latin-1 / Latin Extended-A, Celsius sign; exclusions below | 318 each |
| 24 px | `ui_font_InterSemiBold24` | Printable ASCII, degree and Celsius signs | 97 |
| 48 px | `ui_font_InterSemiBold48` | Space, percent, digits, colon, C, degree and Celsius signs | 16 |

All four use **4 bpp**, uncompressed grayscale bitmaps and normal kerning. U+00B0 is the degree sign; U+2103 is the Celsius sign retained for compatibility. Inter 4.1 does not provide U+00AD (soft hyphen) or U+0149 (deprecated Latin small letter n preceded by apostrophe); those two are explicitly excluded from the extended ranges. Other missing characters still need a later UI fallback policy. The fonts include the Hungarian accented letters needed by the filename acceptance example.

Keep LVGL Montserrat 16 for `LV_SYMBOL_*` icons. The legacy 16/20/32 px Inter sources are not part of the target font set and were not regenerated; removing unused legacy files belongs to Phase 1. Existing public symbols of the four regenerated fonts are unchanged. Generated sources include `lvgl.h` directly rather than the legacy generated UI header.

## GIF Sources

The selected GIF files in `docs/screens/gif/` already match the embedded arrays in `src/gif/` byte-for-byte, so Phase 0.5 did not rewrite or resize them. Symbols and filenames remain unchanged. After deliberately editing a source GIF, regenerate its C array with:

```powershell
python tools/bin2c.py docs/screens/gif/creality.gif src/gif/gif_creality.c gif_creality
```

Use the same pattern for `standby_green`, `wifi`, `homing_green`, `probing_green`, `qgling_green`, `heated_green`, `print_green`, `print_ok` and `printed_green`. Preserve the infinite-loop NETSCAPE extension; do not strip it as incidental metadata. The Wi-Fi descriptor currently references the legacy internal array name `gif_welcome_map`; its public descriptor remains `gif_wifi`.

## Verification

```powershell
python tools/check_assets.py
python tools/check_assets.py --compile --build-dir spikes/display/build/s3
```

The first command checks source/C byte equality, descriptors, GIF dimensions/frame counts/durations, complete Pillow frame decoding, infinite looping, budgets, source-font hash, 4-bpp settings and exact generated glyph coverage. It prints each asset's measured values.

The second also syntax-compiles all **10 GIFs and 4 fonts** using the existing accepted Windows ESP32-S3 `compile_commands.json` entry for `gif_creality.c`, preserving its compiler/includes/defines and substituting each asset path. Configure/build the isolated IDF spike first if this database is absent. Use the `--build-dir` option for a different configured S3 build directory. This check creates no firmware or object files and does not overwrite the spike's build objects.

2026-11-09 results: **14/14 C assets compile-check successfully** against ESP-IDF 5.5.5 / LVGL 8.4.0. GIF payload total **458,197 bytes (447.46 KiB)**, below the **470 KiB** limit. Maximum estimated decoder canvas/frame allocation **90,000 bytes**, at the planned limit; the estimate does not include the transient LZW table or other LVGL/network allocations. The separate S3 network-loaded PSRAM margin was measured in Phase 0.3. Target UI integration, all-asset device playback and the 24-hour soak remain later checks, not results of this offline asset gate.