"""GIF -> LVGL lv_img_dsc_t (LV_IMG_CF_RAW_CHROMA_KEYED), same layout as src/gif/*.c."""
import struct, sys, pathlib

def gif_info(d: bytes):
    if d[:6] not in (b"GIF87a", b"GIF89a"):
        raise ValueError("not a GIF")
    w, h = struct.unpack_from("<HH", d, 6)
    i, fl = 13, d[10]
    if fl & 0x80:
        i += 3 << ((fl & 7) + 1)                      # global color table
    frames = delay_cs = 0
    while d[i] != 0x3B:                              # trailer
        if d[i] == 0x21:                             # extension
            if d[i + 1] == 0xF9:                     # graphic control -> frame delay
                delay_cs += d[i + 4] | d[i + 5] << 8
            i += 2
            while d[i]: i += d[i] + 1
            i += 1
        elif d[i] == 0x2C:                           # image descriptor
            fl = d[i + 9]; i += 10
            if fl & 0x80: i += 3 << ((fl & 7) + 1)   # local color table
            i += 1                                   # LZW min code size
            while d[i]: i += d[i] + 1
            i += 1; frames += 1
        else:
            raise ValueError(f"unexpected block 0x{d[i]:02x} at {i}")
    return w, h, frames, delay_cs * 10

def main(src, dst, name):
    d = pathlib.Path(src).read_bytes()
    w, h, frames, ms = gif_info(d)
    U = name.upper()
    rows = "\n".join("    " + "".join(f"0x{b:02x}, " for b in d[k:k + 13])
                     for k in range(0, len(d), 13))
    c = f'''#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif


#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMG_{U}
#define LV_ATTRIBUTE_IMG_{U}
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_{U} uint8_t {name}_map[] = {{
{rows}
}};

const lv_img_dsc_t {name} = {{
  .header.cf = LV_IMG_CF_RAW_CHROMA_KEYED,
  .header.always_zero = 0,
  .header.reserved = 0,
  .header.w = {w},
  .header.h = {h},
  .data_size = {len(d)},
  .data = {name}_map,
}};
'''
    pathlib.Path(dst).write_text(c, encoding="utf-8")
    ram = 4 * w * h
    print(f"{name}: {w}x{h}, {frames} frames, {ms / 1000:.2f} s/loop, "
          f"flash {len(d) / 1024:.1f} KB, RAM {ram / 1024:.1f} KB"
          + ("  !! exceeds 90 KB canvas rule (plan §3.3)" if ram > 90_000 else ""))

if __name__ == "__main__":
    main(*sys.argv[1:4])