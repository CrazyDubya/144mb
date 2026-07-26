#!/usr/bin/env python3
"""Convert an 8-bit PNG to a 320x240 RGB332 C array for a standalone game."""
import struct
import sys
import zlib


def decode(path):
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("not a PNG")
    pos, packed = 8, b""
    width = height = depth = colour = 0
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        tag, body = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + length]
        if tag == b"IHDR":
            width, height, depth, colour = struct.unpack(">IIBB", body[:10])
        elif tag == b"IDAT":
            packed += body
        elif tag == b"IEND":
            break
        pos += 12 + length
    if depth != 8 or colour not in (2, 6):
        raise SystemExit("expected 8-bit RGB or RGBA PNG")
    channels = 3 if colour == 2 else 4
    raw, stride = zlib.decompress(packed), width * channels

    def paeth(a, b, c):
        p = a + b - c
        pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
        if pa <= pb and pa <= pc:
            return a
        return b if pb <= pc else c

    rows, prev, at = [], bytearray(stride), 0
    for _ in range(height):
        kind, at = raw[at], at + 1
        row = bytearray(raw[at:at + stride])
        at += stride
        for i in range(stride):
            left = row[i - channels] if i >= channels else 0
            above = prev[i]
            upper_left = prev[i - channels] if i >= channels else 0
            if kind == 1:
                row[i] = (row[i] + left) & 255
            elif kind == 2:
                row[i] = (row[i] + above) & 255
            elif kind == 3:
                row[i] = (row[i] + ((left + above) >> 1)) & 255
            elif kind == 4:
                row[i] = (row[i] + paeth(left, above, upper_left)) & 255
            elif kind != 0:
                raise SystemExit("unsupported PNG filter")
        rows.append(row)
        prev = row
    return width, height, channels, rows


def main():
    src, dst = sys.argv[1:3]
    width, height, channels, rows = decode(src)
    crop_w, crop_h = min(width, height * 4 // 3), min(height, width * 3 // 4)
    x0, y0 = (width - crop_w) // 2, (height - crop_h) // 2
    values = []
    for y in range(240):
        sy = y0 + y * crop_h // 240
        for x in range(320):
            sx = x0 + x * crop_w // 320
            off = sx * channels
            r, g, b = rows[sy][off:off + 3]
            values.append((r & 0xE0) | ((g >> 3) & 0x1C) | (b >> 6))
    with open(dst, "w", encoding="ascii") as out:
        out.write("#ifndef TITLE_PIXELS_H\n#define TITLE_PIXELS_H\n")
        out.write("static const unsigned char title_pixels[320*240]={\n")
        for at in range(0, len(values), 32):
            out.write(",".join(str(v) for v in values[at:at + 32]) + ",\n")
        out.write("};\n#endif\n")
    preview = dst.rsplit(".", 1)[0] + ".png"
    raw = bytearray()
    for y in range(240):
        raw.append(0)
        for value in values[y * 320:(y + 1) * 320]:
            raw.extend((value & 0xE0, (value & 0x1C) << 3, (value & 3) << 6))

    def chunk(tag, body):
        return (struct.pack(">I", len(body)) + tag + body
                + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", 320, 240, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    open(preview, "wb").write(png)
    print(f"{src}: {width}x{height} -> {dst}: 320x240 RGB332; preview {preview}")


if __name__ == "__main__":
    main()
