#!/usr/bin/env python3
"""Convert a 3x2 PNG scene atlas into six embedded 320x240 RGB565 plates."""
import struct
import sys
import zlib

from embed_title import decode


def png_preview(path, plates):
    width, height = 960, 480
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        row, py = y // 240, y % 240
        for x in range(width):
            col, px = x // 320, x % 320
            value = plates[row * 3 + col][py * 320 + px]
            raw.extend((((value >> 11) & 31) * 255 // 31,
                        ((value >> 5) & 63) * 255 // 63,
                        (value & 31) * 255 // 31))

    def chunk(tag, body):
        return (struct.pack(">I", len(body)) + tag + body
                + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)


def main():
    src, dst = sys.argv[1:3]
    width, height, channels, rows = decode(src)
    cell_w, cell_h = width // 3, height // 2
    plates = []
    for panel in range(6):
        col, row = panel % 3, panel // 3
        # Trim a narrow gutter, then center-crop each cell to 4:3.
        margin_x, margin_y = max(2, cell_w // 80), max(2, cell_h // 80)
        avail_w, avail_h = cell_w - margin_x * 2, cell_h - margin_y * 2
        crop_w = min(avail_w, avail_h * 4 // 3)
        crop_h = min(avail_h, avail_w * 3 // 4)
        x0 = col * cell_w + margin_x + (avail_w - crop_w) // 2
        y0 = row * cell_h + margin_y + (avail_h - crop_h) // 2
        values = []
        for y in range(240):
            sy = y0 + y * crop_h // 240
            for x in range(320):
                sx = x0 + x * crop_w // 320
                off = sx * channels
                red, green, blue = rows[sy][off:off + 3]
                values.append(((red >> 3) << 11) | ((green >> 2) << 5)
                              | (blue >> 3))
        plates.append(values)

    with open(dst, "w", encoding="ascii") as out:
        out.write("#ifndef SCENE_PIXELS_H\n#define SCENE_PIXELS_H\n")
        out.write("#include <stdint.h>\n")
        out.write("static const uint16_t scene_pixels[6][320*240]={\n")
        for plate in plates:
            out.write("{\n")
            for at in range(0, len(plate), 24):
                out.write(",".join(str(v) for v in plate[at:at + 24]) + ",\n")
            out.write("},\n")
        out.write("};\n")
        out.write("static inline void scene_frame(Framebuffer*f,int scene,int shade){\n")
        out.write(" const uint16_t*s=scene_pixels[(unsigned)scene<6?scene:0];\n")
        out.write(" for(int y=0;y<480;y++)for(int x=0;x<640;x++){\n")
        out.write("  uint16_t v=s[(y>>1)*320+(x>>1)];\n")
        out.write("  uint32_t r=((v>>11)&31)*255/31,g=((v>>5)&63)*255/63,b=(v&31)*255/31;\n")
        out.write("  f->pixels[y*640+x]=((r*(unsigned)shade/255)<<16)|((g*(unsigned)shade/255)<<8)|(b*(unsigned)shade/255);\n")
        out.write(" }\n}\n#endif\n")

    preview = dst.rsplit(".", 1)[0] + ".png"
    png_preview(preview, plates)
    print(f"{src}: {width}x{height} -> six RGB565 plates in {dst}; preview {preview}")


if __name__ == "__main__":
    main()
