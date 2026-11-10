# Native KNOMI Screen References

This host-only harness renders the existing generated screen constructors through **LVGL 8.4.0**. It does not approximate their layout with HTML/Pillow drawing, run the firmware, send printer commands, register input devices or change the ESP32 display spike.

The output is a reference of the **current legacy source**, not a released stock binary or a device photograph. Four Inter fonts were already regenerated at 4 bpp; other legacy fonts remain unchanged. Theme colors are the original default dark/blue/red, not the future green adaptation. Visible controls and old branding are historical references only. UI event functions are generated as inert stubs; no control callbacks are linked from the firmware.

## Build

Requirements: CMake 3.20+, Ninja and a native C99 compiler. The root `managed_components/lvgl__lvgl` is preferred, with the old spike cache as a fallback; pass `-DLVGL_SOURCE_DIR=<path>` for another LVGL 8.4.0 checkout. No download is performed by this host CMake project. Configure the root IDF project first on a fresh checkout to obtain LVGL.

The Espressif Clang installed here lacked a usable Windows host linker/runtime configuration. The validated portable alternative is **Zig 0.14.1**, using its bundled C compiler and Windows runtime, without a system SDK installation:

- Archive: https://ziglang.org/download/0.14.1/zig-x86_64-windows-0.14.1.zip
- SHA-256 from the official release index: `554f5378228923ffd558eac35e21af020c73789d87afeabf4bfd16f2e6feed2c`.
- Downloaded normally, verified before extraction, and unpacked under the user's temporary `knomi-host-tools` directory. No TLS/network bypass or elevated installer was used. A matching hash identifies the downloaded archive; it is not signature verification.

From the repository root in PowerShell, with CMake/Ninja on PATH:

```powershell
$zig = Join-Path $env:TEMP 'knomi-host-tools/zig-x86_64-windows-0.14.1/zig.exe'
cmake -S tools/screen_reference -B tools/screen_reference/build/zig -G Ninja `
    "-DCMAKE_C_COMPILER=$zig" -DCMAKE_C_COMPILER_ARG1=cc
cmake --build tools/screen_reference/build/zig --parallel 4
python tools/screen_reference/render.py
```

Fresh VS Code task terminals do not inherit an activated IDF PATH. If needed, use absolute CMake/Ninja paths and `-DCMAKE_MAKE_PROGRAM=<ninja.exe>`. On this machine those tools are under `C:/D_Drive/Espressif/tools/cmake/3.30.2/bin/` and `C:/D_Drive/Espressif/tools/ninja/1.12.1/`. A regular native GCC/Clang installation can be selected instead; then omit the Zig-specific `CMAKE_C_COMPILER_ARG1` option and use a separate build directory.

Export requires Python with Pillow. The captured run used **Python 3.14.6 / Pillow 12.3.0**; actual versions are recorded in the manifest, not inferred from PATH. On this machine the explicit interpreter is `C:/Users/F5RY38/AppData/Local/Python/pythoncore-3.14-64/python.exe`.

## Output and Checks

See [docs/screens/original/contact-sheet.png](../../docs/screens/original/contact-sheet.png) and [docs/screens/original/manifest.json](../../docs/screens/original/manifest.json).

- **28 scenarios** across all 12 adaptable screen constructors: nozzle/bed 0/25/50/100/125 percent and target-zero guard; v1 progress 0/57/100; one v2 comparison; temperature panels; two phases of each GIF/spinner; disconnect, QR, info, popup and dialog.
- Each scenario has an unmasked 240 x 240 RGB PNG and a `.round.png` with an illustrative round-display alpha mask. The contact sheet labels previews outside the screen area. These masks do not replace physical panel verification.
- The exporter rejects blank/wrong-sized captures, checks that native heater red increases with fill, and verifies that the two GIF/spinner phases differ. Source hashes, executable hash, PNG hashes, synthetic values, simulated times and tool versions are recorded.
- The original Voron GIF begins with eight black source frames. The main-GIF snapshots deliberately use later visible phases at 600/1,200 simulated ms. The timer is advanced in 20 ms steps; these snapshots are not FPS or timing acceptance tests. The welcome asset uses its real `gif_welcome` descriptor from `gif_wifi.c`.
- Native output confirms automatic vertical heater fill and clamping above 100. The nozzle screen also renders a horizontal strip below the icon; it is preserved as observed output, not silently removed in the reference tool.
- Info values use reserved documentation IPs. The QR retains the original public BTT manual URL solely as reference. No credentials or live printer status are read.

Only off-screen host code uses an 8 MiB LVGL pool and a full-frame output array. They are **not firmware allocation recommendations**; the ESP32 retains its two internal DMA strips and explicit PSRAM policy.

The **75 original visual source files** were frozen in [docs/screens/original/legacy-source.zip](../../docs/screens/original/legacy-source.zip), with per-file hashes in [docs/screens/original/legacy-source.json](../../docs/screens/original/legacy-source.json). When the live generated UI is absent, CMake verifies the bundle hash and extracts it into the ignored native build directory. The archive-backed build was tested before cleanup: **all 28 PNGs were byte-identical** to live-source captures. Export/verification still passes after removal. `freeze.py` creates the initial bundle from live originals and must not be rerun against the cleaned tree.

Missing/unknown printer values, paused styling, new timing/chamber fields and green target layouts must be validated in the new UI, not fabricated as original-screen behavior. The root firmware and host renderer have separate LVGL configuration: retained source images preserve original bytes, so their eventual swapped-RGB565 target rendering still needs a color/byte-order check before hardware acceptance.