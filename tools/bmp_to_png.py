#!/usr/bin/env python3
"""Convert the harness's fixed 640x480 24-bit BMP capture to an RGB PNG."""
import struct
import sys
import zlib


def chunk(tag, body):
    return (struct.pack(">I", len(body)) + tag + body
            + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF))


def main():
    source, destination = sys.argv[1:3]
    data = open(source, "rb").read()
    if data[:2] != b"BM":
        raise SystemExit("not a BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    planes, depth = struct.unpack_from("<HH", data, 26)
    if planes != 1 or depth != 24 or width <= 0 or height <= 0:
        raise SystemExit("expected a bottom-up 24-bit BMP")
    stride = (width * 3 + 3) & ~3
    raw = bytearray()
    for y in range(height):
        row = data[offset + (height - 1 - y) * stride:
                   offset + (height - y) * stride]
        raw.append(0)
        for x in range(width):
            blue, green, red = row[x * 3:x * 3 + 3]
            raw.extend((red, green, blue))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    open(destination, "wb").write(png)


if __name__ == "__main__":
    main()
