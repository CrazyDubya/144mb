#!/usr/bin/env python3
"""Turn the harness's frame dumps into web-ready media.

The harness writes PNGs with stored (uncompressed) deflate so the C side needs
no zlib. That is fine on disk but far too large to embed in a page, so this
recompresses them properly and also assembles an animated GIF of the run.

No ffmpeg, ImageMagick or PIL on this box -- GIF/LZW is implemented here.
"""
import glob
import os
import struct
import sys
import zlib


# ---------------------------------------------------------------- png read
def read_png(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "not a png"
    pos, idat, w, h = 8, b"", 0, 0
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        ctype = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        if ctype == b"IHDR":
            w, h, depth, colour = struct.unpack(">IIBB", chunk[:10])
            assert depth == 8 and colour == 2, "expected 8-bit RGB"
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"IEND":
            break
        pos += 12 + length

    raw = zlib.decompress(idat)
    stride = w * 3 + 1
    rows = []
    for y in range(h):
        off = y * stride
        assert raw[off] == 0, "harness only ever writes filter 0"
        rows.append(raw[off + 1:off + stride])
    return w, h, rows


# ---------------------------------------------------------------- png write
def write_png(path, w, h, rows):
    raw = b"".join(b"\x00" + r for r in rows)
    comp = zlib.compress(raw, 9)

    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload
                + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", comp)
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)
    return len(png)


# ---------------------------------------------------------------- gif
class BitWriter:
    """LSB-first bit packer, as GIF's LZW stream requires."""

    def __init__(self):
        self.out = bytearray()
        self.acc = 0
        self.n = 0

    def write(self, code, width):
        self.acc |= code << self.n
        self.n += width
        while self.n >= 8:
            self.out.append(self.acc & 0xFF)
            self.acc >>= 8
            self.n -= 8

    def flush(self):
        if self.n:
            self.out.append(self.acc & 0xFF)
            self.acc = 0
            self.n = 0
        return bytes(self.out)


def lzw(indices, min_code_size):
    clear, eoi = 1 << min_code_size, (1 << min_code_size) + 1
    table = {bytes([i]): i for i in range(1 << min_code_size)}
    nxt, width = eoi + 1, min_code_size + 1

    bw = BitWriter()
    bw.write(clear, width)

    prefix = b""
    for value in indices:
        cur = prefix + bytes([value])
        if cur in table:
            prefix = cur
            continue
        bw.write(table[prefix], width)
        table[cur] = nxt
        nxt += 1
        if nxt > 4095:
            bw.write(clear, width)
            table = {bytes([i]): i for i in range(1 << min_code_size)}
            nxt, width = eoi + 1, min_code_size + 1
        elif nxt > (1 << width):
            width += 1
        prefix = bytes([value])

    if prefix:
        bw.write(table[prefix], width)
    bw.write(eoi, width)
    return bw.flush()


def blockify(data):
    out = bytearray()
    for i in range(0, len(data), 255):
        part = data[i:i + 255]
        out.append(len(part))
        out += part
    out.append(0)
    return bytes(out)


def write_gif(path, w, h, frames, palette, delay_cs, last_delay_cs):
    bits = max(1, (len(palette) - 1).bit_length())
    size = 1 << bits
    gct = bytearray()
    for i in range(size):
        r, g, b = palette[i] if i < len(palette) else (0, 0, 0)
        gct += bytes((r, g, b))

    out = bytearray(b"GIF89a")
    out += struct.pack("<HHBBB", w, h, 0xF0 | (bits - 1), 0, 0)
    out += gct
    # Loop forever.
    out += b"\x21\xFF\x0BNETSCAPE2.0\x03\x01\x00\x00\x00"

    for i, idx in enumerate(frames):
        d = last_delay_cs if i == len(frames) - 1 else delay_cs
        out += b"\x21\xF9" + struct.pack("<BBHBB", 4, 0, d, 0, 0)
        out += b"\x2C" + struct.pack("<HHHHB", 0, 0, w, h, 0)
        mcs = max(2, bits)
        out += bytes((mcs,)) + blockify(lzw(idx, mcs))

    out += b"\x3B"
    open(path, "wb").write(bytes(out))
    return len(out)


# ---------------------------------------------------------------- main
def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "run"
    dst = sys.argv[2] if len(sys.argv) > 2 else "media"
    os.makedirs(dst, exist_ok=True)

    paths = sorted(glob.glob(os.path.join(src, "*.png")))
    if not paths:
        print("no frames found in", src)
        return 1

    palette, lut, frames, dims = [], {}, [], None
    for p in paths:
        w, h, rows = read_png(p)
        dims = (w, h)
        idx = bytearray()
        for row in rows:
            for x in range(0, len(row), 3):
                key = row[x:x + 3]
                v = lut.get(key)
                if v is None:
                    v = len(palette)
                    if v > 255:
                        raise SystemExit("more than 256 colours; needs quantising")
                    lut[key] = v
                    palette.append((key[0], key[1], key[2]))
                idx.append(v)
        frames.append(bytes(idx))

        out_png = os.path.join(dst, os.path.basename(p))
        write_png(out_png, w, h, rows)

    w, h = dims
    n = write_gif(os.path.join(dst, "run.gif"), w, h, frames, palette, 90, 300)
    print("frames        : %d  (%dx%d)" % (len(frames), w, h))
    print("distinct cols : %d" % len(palette))
    print("run.gif       : %d bytes (%.0f KB)" % (n, n / 1024.0))
    total = sum(os.path.getsize(os.path.join(dst, os.path.basename(p))) for p in paths)
    print("pngs          : %d bytes total, avg %.0f KB"
          % (total, total / len(paths) / 1024.0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
