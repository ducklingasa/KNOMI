import re
from pathlib import Path


TARGET_GREEN = (0x32, 0x9A, 0x40)
CHANGE_TRANSPARENCY = False
PROJECT_ROOT = Path(__file__).resolve().parent.parent
INPUT_FILES = (
    Path("src/gif/gif_heated.c"),
    Path("src/gif/gif_homing.c"),
    Path("src/gif/gif_print.c"),
    Path("src/gif/gif_printed.c"),
    Path("src/gif/gif_probing.c"),
    Path("src/gif/gif_qgling.c"),
    Path("src/gif/gif_standby.c"),
)
BYTE_PATTERN = re.compile(r"0x([0-9A-Fa-f]{2})")
ARRAY_PATTERN = re.compile(
    r"(uint8_t\s+\w+\[\]\s*=\s*\{)(.*?)(\};)",
    re.DOTALL
)


def brightness(rgb):
    return sum(rgb)


def scale_color(color, factor):
    return tuple(
        max(0, min(255, round(channel * factor)))
        for channel in color
    )


def is_red(rgb):
    red, green, blue = rgb
    return red > 80 and red > green * 1.5 and red > blue * 1.5


def gif_palettes_and_transparency(data):
    if data[:6] not in (b"GIF87a", b"GIF89a") or len(data) < 13:
        raise RuntimeError("The C array does not contain a valid GIF header.")

    logical_screen_packed = data[10]
    if not logical_screen_packed & 0x80:
        raise RuntimeError("The GIF has no global color table.")

    global_palette_start = 13
    global_palette_size = 3 * (2 ** ((logical_screen_packed & 0x07) + 1))
    global_palette_end = global_palette_start + global_palette_size
    if global_palette_end > len(data):
        raise RuntimeError("The GIF global color table is truncated.")

    palettes = [(global_palette_start, global_palette_size)]
    transparent_palette_entries = []
    transparency_flags = []
    pending_transparency = None
    offset = global_palette_end
    found_trailer = False

    while offset < len(data):
        introducer = data[offset]
        if introducer == 0x3B:
            found_trailer = True
            break

        if introducer == 0x21:
            if offset + 2 >= len(data):
                raise RuntimeError("The GIF extension is truncated.")
            label = data[offset + 1]

            if label == 0xF9:
                if offset + 7 >= len(data) or data[offset + 2] != 4:
                    raise RuntimeError("The GIF graphic control extension is invalid.")
                packed_offset = offset + 3
                transparent_index = data[offset + 6]
                if data[offset + 7] != 0:
                    raise RuntimeError("The GIF graphic control extension is unterminated.")
                transparency_flags.append(packed_offset)
                pending_transparency = (
                    packed_offset,
                    transparent_index,
                    bool(data[packed_offset] & 0x01)
                )
                offset += 8
                continue

            offset += 2
            while offset < len(data):
                block_size = data[offset]
                offset += 1
                if block_size == 0:
                    break
                offset += block_size
            continue

        if introducer != 0x2C or offset + 9 >= len(data):
            raise RuntimeError(f"Unexpected or truncated GIF block at byte {offset}.")

        image_packed = data[offset + 9]
        palette_start = global_palette_start
        palette_size = global_palette_size
        offset += 10
        if image_packed & 0x80:
            palette_size = 3 * (2 ** ((image_packed & 0x07) + 1))
            if offset + palette_size > len(data):
                raise RuntimeError("A GIF local color table is truncated.")
            palette_start = offset
            palettes.append((palette_start, palette_size))
            offset += palette_size

        if pending_transparency is not None:
            packed_offset, transparent_index, enabled = pending_transparency
            if enabled:
                if (transparent_index + 1) * 3 > palette_size:
                    raise RuntimeError("A transparent GIF palette index is out of range.")
                transparent_palette_entries.append(
                    palette_start + transparent_index * 3
                )
            pending_transparency = None

        if offset >= len(data):
            raise RuntimeError("The GIF image data is truncated.")
        offset += 1
        while offset < len(data):
            block_size = data[offset]
            offset += 1
            if block_size == 0:
                break
            offset += block_size

    if not found_trailer:
        raise RuntimeError("The GIF trailer is missing.")

    return palettes, transparency_flags, transparent_palette_entries, data[11]


def replacement_text(match, output_stem):
    if match.group(0).isupper():
        return output_stem.upper()
    return output_stem


def convert_file(input_file, output_file):
    input_file = Path(input_file)
    output_file = Path(output_file)
    content = input_file.read_text(encoding="utf-8")

    array_match = ARRAY_PATTERN.search(content)
    if array_match is None:
        raise RuntimeError(f"No uint8_t GIF array found in {input_file}.")

    initializer = array_match.group(2)
    byte_matches = list(BYTE_PATTERN.finditer(initializer))
    data = bytearray(int(match.group(1), 16) for match in byte_matches)
    palettes, transparency_flags, transparent_entries, background_index = (
        gif_palettes_and_transparency(data)
    )

    red_colors = {
        tuple(data[start + offset:start + offset + 3])
        for start, size in palettes
        for offset in range(0, size, 3)
        if is_red(tuple(data[start + offset:start + offset + 3]))
    }
    if not red_colors:
        raise RuntimeError(f"No red palette entries found in {input_file}.")

    brightest_red = max(red_colors, key=brightness)
    brightest_score = brightness(brightest_red)
    mapping = {
        red: scale_color(TARGET_GREEN, brightness(red) / brightest_score)
        for red in red_colors
    }

    replacements = {}
    for start, size in palettes:
        for offset in range(0, size, 3):
            color_start = start + offset
            color = tuple(data[color_start:color_start + 3])
            if color in mapping:
                replacements.update(
                    (color_start + channel, value)
                    for channel, value in enumerate(mapping[color])
                )

    if CHANGE_TRANSPARENCY:
        global_palette_start, global_palette_size = palettes[0]
        if (background_index + 1) * 3 <= global_palette_size:
            background_entry = global_palette_start + background_index * 3
            replacements.update({
                background_entry: 0,
                background_entry + 1: 0,
                background_entry + 2: 0
            })
        else:
            raise RuntimeError("The GIF background color index is out of range.")

        for palette_entry in transparent_entries:
            replacements.update({
                palette_entry: 0,
                palette_entry + 1: 0,
                palette_entry + 2: 0
            })
        for packed_offset in transparency_flags:
            replacements[packed_offset] = data[packed_offset] & ~0x01

    changed_initializer = initializer
    for offset, value in sorted(
        replacements.items(),
        key=lambda item: byte_matches[item[0]].start(),
        reverse=True
    ):
        token = byte_matches[offset]
        changed_initializer = (
            changed_initializer[:token.start()]
            + f"0x{value:02X}"
            + changed_initializer[token.end():]
        )

    old_stem = input_file.stem
    new_stem = output_file.stem
    escaped_old_stem = re.escape(old_stem)
    new_content = (
        content[:array_match.start(2)]
        + changed_initializer
        + content[array_match.end(2):]
    )
    new_content = re.sub(
        escaped_old_stem,
        lambda match: replacement_text(match, new_stem),
        new_content,
        flags=re.IGNORECASE
    )

    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text(new_content, encoding="utf-8")
    print(
        f"{input_file.name}: brightest red {brightest_red} -> "
        f"#{TARGET_GREEN[0]:02X}{TARGET_GREEN[1]:02X}{TARGET_GREEN[2]:02X}; "
        f"{len(red_colors)} red shades; wrote {output_file}"
    )


if __name__ == "__main__":
    for relative_path in INPUT_FILES:
        input_file = PROJECT_ROOT / relative_path
        output_file = input_file.with_name(f"{input_file.stem}_green{input_file.suffix}")
        convert_file(input_file, output_file)
