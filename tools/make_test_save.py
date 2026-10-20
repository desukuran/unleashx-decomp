#!/usr/bin/env python3
"""Makes a made-up game save in the Xbox's on-disk format, for testing the
game saves manager (docs/GAME_SAVES.md) without real game data.

  make_test_save.py <out dir> [title id] [title name] [save name...]

writes <out>/UDATA/<title id>/ with TitleMeta.xbx, TitleImage.xbx (128x128)
and SaveImage.xbx, and one folder per save with SaveMeta.xbx, SaveImage.xbx
(64x64) and a small data file; plus <out>/TDATA/<title id>/ for the title's
own data. Images are DXT1 textures in XPR0 files, like the dashboard's. Copy
the tree to E:\\ (tools/xemu_ftp.py putdir <out> /E). Needs Pillow.
"""
import hashlib
import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFont


def rgb565(c):
    return ((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3)


def unpack565(v):
    return ((v >> 11) << 3, ((v >> 5) & 0x3F) << 2, (v & 0x1F) << 3)


def dxt1(image):
    """DXT1 blocks (4x4, left to right, top to bottom; not swizzled)."""
    w, h = image.size
    px = image.convert("RGB").load()
    out = bytearray()
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            block = [px[bx + x, by + y] for y in range(4) for x in range(4)]
            # The two colours furthest apart along the brightest axis.
            lum = [c[0] * 3 + c[1] * 6 + c[2] for c in block]
            c0 = rgb565(block[lum.index(max(lum))])
            c1 = rgb565(block[lum.index(min(lum))])
            if c0 < c1:
                c0, c1 = c1, c0
            if c0 == c1:
                out += struct.pack("<HHI", c0, c1, 0)
                continue
            a, b = unpack565(c0), unpack565(c1)
            palette = [a, b, tuple((2 * a[i] + b[i]) // 3 for i in range(3)),
                       tuple((a[i] + 2 * b[i]) // 3 for i in range(3))]
            bits = 0
            for i, c in enumerate(block):
                d = [sum((c[k] - p[k]) ** 2 for k in range(3)) for p in palette]
                bits |= d.index(min(d)) << (2 * i)
            out += struct.pack("<HHI", c0, c1, bits)
    return bytes(out)


def xpr(image):
    """An XPR0 file holding one DXT1 texture (a power-of-two square)."""
    size = image.size[0]
    log2 = size.bit_length() - 1
    # D3DTexture: Common (1 reference, type texture), Data, Lock, Format, Size.
    # Format: DMA channel A, 2D, D3DFMT_DXT1 (0x0C), 1 level, log2 sizes.
    fmt = 0x29 | (0x0C << 8) | (1 << 16) | (log2 << 20) | (log2 << 24)
    header = b"XPR0" + struct.pack("<II", 0, 0x800)
    header += struct.pack("<IIIII", 0x00040001, 0, 0, fmt, 0) + b"\xff\xff\xff\xff"
    header = header.ljust(0x800, b"\xad")
    data = dxt1(image)
    blob = header + data
    blob = blob.ljust((len(blob) + 0x7FF) & ~0x7FF, b"\xad")
    return blob[:4] + struct.pack("<I", len(blob)) + blob[8:]


def picture(size, text, top, bottom):
    image = Image.new("RGB", (size, size))
    draw = ImageDraw.Draw(image)
    for y in range(size):
        t = y / (size - 1)
        draw.line([(0, y), (size, y)], fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)))
    draw.rectangle([1, 1, size - 2, size - 2], outline=(255, 255, 255), width=max(1, size // 32))
    try:
        font = ImageFont.truetype("DejaVuSans-Bold.ttf", size // 3)
    except OSError:
        font = ImageFont.load_default()
    box = draw.textbbox((0, 0), text, font=font)
    draw.text(((size - (box[2] - box[0])) / 2 - box[0], (size - (box[3] - box[1])) / 2 - box[1]), text,
              font=font, fill=(255, 255, 255))
    return image


def meta(path, key, value):
    with open(path, "wb") as f:
        f.write(("\ufeff%s=%s\r\n" % (key, value)).encode("utf-16-le"))


def main(argv):
    if len(argv) < 2:
        print(__doc__.strip())
        return 2
    out = argv[1]
    title_id = (argv[2] if len(argv) > 2 else "decade01").lower()
    title = argv[3] if len(argv) > 3 else "UnleashX Test Game"
    saves = argv[4:] or ["Level 1 - Start", "Level 7 - Boss", "Autosave"]
    colours = [((40, 90, 200), (10, 20, 60)), ((200, 60, 40), (60, 10, 10)), ((40, 160, 70), (10, 50, 20)),
               ((180, 140, 30), (60, 40, 5))]

    game = os.path.join(out, "UDATA", title_id)
    os.makedirs(game, exist_ok=True)
    meta(os.path.join(game, "TitleMeta.xbx"), "TitleName", title)
    initials = "".join(w[0] for w in title.split()[:3]).upper()
    with open(os.path.join(game, "TitleImage.xbx"), "wb") as f:
        f.write(xpr(picture(128, initials, (90, 40, 160), (20, 10, 40))))
    with open(os.path.join(game, "SaveImage.xbx"), "wb") as f:
        f.write(xpr(picture(64, initials, (90, 40, 160), (20, 10, 40))))
    for i, name in enumerate(saves):
        # Save folders are 12 hex digits (the XAPI makes them from the name).
        folder = os.path.join(game, hashlib.sha1(name.encode()).hexdigest()[:12].upper())
        os.makedirs(folder, exist_ok=True)
        meta(os.path.join(folder, "SaveMeta.xbx"), "Name", name)
        top, bottom = colours[i % len(colours)]
        with open(os.path.join(folder, "SaveImage.xbx"), "wb") as f:
            f.write(xpr(picture(64, str(i + 1), top, bottom)))
        with open(os.path.join(folder, "save.dat"), "wb") as f:
            f.write((name + "\n").encode() * (200 * (i + 1)))
    tdata = os.path.join(out, "TDATA", title_id)
    os.makedirs(tdata, exist_ok=True)
    with open(os.path.join(tdata, "settings.dat"), "wb") as f:
        f.write(b"\x00" * 4096)
    print("wrote %s: %s, %d saves" % (game, title, len(saves)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
