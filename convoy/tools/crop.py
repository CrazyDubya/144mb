#!/usr/bin/env python3
"""Decode, crop and re-encode a PNG.

The CI screenshot is a full 1024x768 desktop written by .NET, so unlike the
harness's own dumps it uses real filters and an alpha channel. This implements
the five PNG reconstruction filters so the game window can be cut out of it.

  crop.py in.png out.png X Y W H
"""
import struct
import sys
import zlib


def decode(path):
    d = open(path, "rb").read()
    assert d[:8] == b"\x89PNG\r\n\x1a\n"
    pos, idat = 8, b""
    w = h = depth = colour = 0
    while pos < len(d):
        (ln,) = struct.unpack(">I", d[pos:pos + 4])
        tag = d[pos + 4:pos + 8]
        body = d[pos + 8:pos + 8 + ln]
        if tag == b"IHDR":
            w, h, depth, colour = struct.unpack(">IIBB", body[:10])
        elif tag == b"IDAT":
            idat += body
        elif tag == b"IEND":
            break
        pos += 12 + ln

    assert depth == 8, "only 8-bit supported"
    channels = {0: 1, 2: 3, 4: 2, 6: 4}[colour]
    raw = zlib.decompress(idat)
    stride = w * channels

    def paeth(a, b, c):
        p = a + b - c
        pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
        if pa <= pb and pa <= pc:
            return a
        return b if pb <= pc else c

    rows, prev, pos = [], bytearray(stride), 0
    for _ in range(h):
        ft = raw[pos]
        pos += 1
        line = bytearray(raw[pos:pos + stride])
        pos += stride
        if ft == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 0xFF
        elif ft == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ft == 3:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif ft == 4:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                c = prev[i - channels] if i >= channels else 0
                line[i] = (line[i] + paeth(a, prev[i], c)) & 0xFF
        elif ft != 0:
            raise SystemExit("bad filter %d" % ft)
        rows.append(line)
        prev = line
    return w, h, channels, rows


def encode(path, w, h, rows):
    raw = b"".join(b"\x00" + bytes(r) for r in rows)

    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload
                + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    out = b"\x89PNG\r\n\x1a\n"
    out += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    out += chunk(b"IDAT", zlib.compress(raw, 9))
    out += chunk(b"IEND", b"")
    open(path, "wb").write(out)
    return len(out)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    x, y, cw, ch = (int(v) for v in sys.argv[3:7])
    w, h, ch_n, rows = decode(src)

    out = []
    for row in rows[y:y + ch]:
        line = bytearray()
        for px in range(x, x + cw):
            off = px * ch_n
            line += row[off:off + 3]          # drop alpha
        out.append(line)

    n = encode(dst, cw, ch, out)
    print("%s %dx%d -> %s %dx%d  %.0f KB" % (src, w, h, dst, cw, ch, n / 1024.0))


if __name__ == "__main__":
    main()
