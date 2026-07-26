#!/usr/bin/env python3
"""Convert a three-column PNG triptych into three embedded RGB332 plates."""
import struct
import sys
import zlib

from embed_title import decode


def write_preview(path, plates):
    raw = bytearray()
    for y in range(240):
        raw.append(0)
        for panel in range(3):
            for value in plates[panel][y * 320:(y + 1) * 320]:
                raw.extend((value & 0xE0, (value & 0x1C) << 3, (value & 3) << 6))

    def chunk(tag, body):
        return (struct.pack(">I", len(body)) + tag + body
                + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", 960, 240, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)


def main():
    src, dst = sys.argv[1:3]
    width, height, channels, rows = decode(src)
    cell_w = width // 3
    plates = []
    for panel in range(3):
        margin_x, margin_y = max(2, cell_w // 100), max(2, height // 100)
        avail_w, avail_h = cell_w - margin_x * 2, height - margin_y * 2
        crop_w = min(avail_w, avail_h * 4 // 3)
        crop_h = min(avail_h, avail_w * 3 // 4)
        x0 = panel * cell_w + margin_x + (avail_w - crop_w) // 2
        y0 = margin_y + (avail_h - crop_h) // 2
        values = []
        for y in range(240):
            sy = y0 + y * crop_h // 240
            for x in range(320):
                sx = x0 + x * crop_w // 320
                off = sx * channels
                red, green, blue = rows[sy][off:off + 3]
                values.append((red & 0xE0) | ((green >> 3) & 0x1C) | (blue >> 6))
        plates.append(values)

    with open(dst, "w", encoding="ascii") as out:
        out.write("#ifndef STORY_PIXELS_H\n#define STORY_PIXELS_H\n")
        out.write("static const unsigned char story_pixels[3][320*240]={\n")
        for plate in plates:
            out.write("{\n")
            for at in range(0, len(plate), 32):
                out.write(",".join(str(v) for v in plate[at:at + 32]) + ",\n")
            out.write("},\n")
        out.write("};\n")
        out.write("static inline void story_frame(Framebuffer*f,int story,int shade){\n")
        out.write(" const unsigned char*s=story_pixels[(unsigned)story<3?story:0];\n")
        out.write(" for(int y=0;y<480;y++)for(int x=0;x<640;x++){\n")
        out.write("  unsigned v=s[(y>>1)*320+(x>>1)],r=v&224,g=(v&28)<<3,b=(v&3)<<6;\n")
        out.write("  f->pixels[y*640+x]=((r*(unsigned)shade/255)<<16)|((g*(unsigned)shade/255)<<8)|(b*(unsigned)shade/255);\n")
        out.write(" }\n}\n#endif\n")
    preview = dst.rsplit(".", 1)[0] + ".png"
    write_preview(preview, plates)
    print(f"{src}: {width}x{height} -> three RGB332 plates in {dst}; preview {preview}")


if __name__ == "__main__":
    main()
