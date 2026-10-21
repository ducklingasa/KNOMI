"""Strip non-essential GIF blocks in place: comments, plain text and every
application extension except NETSCAPE2.0 (LVGL's gifdec needs it to loop)."""
import sys, pathlib

def skip_subblocks(d: bytes, i: int) -> int:
    while d[i]:
        i += d[i] + 1
    return i + 1

def strip(d: bytes) -> bytes:
    if d[:6] not in (b"GIF87a", b"GIF89a"):
        raise ValueError("not a GIF")
    i, fl = 13, d[10]
    if fl & 0x80:
        i += 3 << ((fl & 7) + 1)
    out = bytearray(d[:i])
    while d[i] != 0x3B:
        s = i
        if d[i] == 0x21:
            label = d[i + 1]
            i = skip_subblocks(d, i + 2)
            keep = label == 0xF9 or (label == 0xFF and d[s + 3:s + 14] == b"NETSCAPE2.0")
        elif d[i] == 0x2C:
            fl = d[i + 9]; i += 10
            if fl & 0x80:
                i += 3 << ((fl & 7) + 1)
            i = skip_subblocks(d, i + 1)
            keep = True
        else:
            raise ValueError(f"unexpected block 0x{d[i]:02x} at {i}")
        if keep:
            out += d[s:i]
    out += b"\x3B"
    return bytes(out)

if __name__ == "__main__":
    for p in map(pathlib.Path, sys.argv[1:]):
        d = p.read_bytes()
        s = strip(d)
        p.write_bytes(s)
        print(f"{p.name}: {len(d)} -> {len(s)} bytes (-{len(d) - len(s)})")
