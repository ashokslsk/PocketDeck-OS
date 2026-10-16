#!/usr/bin/env python3
"""Draw the PocketDeck-OS logo and emit every derived asset.

Outputs (paths relative to the repo root):
  assets/branding/pocketdeck-os-logo.png     full lockup, pocket + wordmark
  assets/branding/pocketdeck-os-icon.png     1024x1024 pocket mark
  assets/branding/pocketdeck-os-icon-1bit.png 120x120 device preview
  src/images/Logo120.h, PocketDeckLogo240.h   boot/sleep bitmaps (1 = white,
                                              pre-rotated to panel orientation)
  web/assets/logo.png                         web portal header mark (white)

The artwork is drawn at 8x and downsampled, so edges stay clean. The only
colour is black on white, which suits e-ink.
"""
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
S = 8  # supersampling factor
INK = 0
PAPER = 255


def font(names, size, index=0):
    for n in names:
        for d in ("/System/Library/Fonts", "/Library/Fonts", "/usr/share/fonts/truetype/dejavu"):
            p = os.path.join(d, n)
            if os.path.exists(p):
                try:
                    return ImageFont.truetype(p, size, index=index)
                except OSError:
                    pass
    return ImageFont.load_default()


def rounded(draw, box, r, width):
    draw.rounded_rectangle(box, radius=r, fill=PAPER, outline=INK, width=width)


def card(size, icon, w, h, line):
    """One card as its own RGBA layer so it can be rotated."""
    im = Image.new("RGBA", (w + 4 * line, h + 4 * line), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    o = 2 * line
    d.rounded_rectangle((o, o, o + w, o + h), radius=w // 7, fill=(255, 255, 255, 255), outline=(0, 0, 0, 255),
                        width=line)
    cx, top = o + w / 2, o + h * 0.12
    k = w / 100.0  # icon unit
    blk = (0, 0, 0, 255)
    if icon == "book":
        # open book: two pages with a spine and text lines
        y0, y1 = top + 6 * k, top + 42 * k
        for side in (-1, 1):
            xs = cx + side * 40 * k
            d.polygon([(cx, y0 + 4 * k), (xs, y0), (xs, y1), (cx, y1 + 4 * k)], fill=(255, 255, 255, 255), outline=blk)
            d.line([(cx, y0 + 4 * k), (xs, y0), (xs, y1), (cx, y1 + 4 * k)], fill=blk, width=line)
            for i in range(3):
                yy = y0 + (11 + 9 * i) * k
                d.line([(cx + side * 8 * k, yy + 2 * k), (cx + side * 32 * k, yy)], fill=blk, width=max(1, line * 2 // 3))
        d.line([(cx, y0 + 4 * k), (cx, y1 + 4 * k)], fill=blk, width=line)
        for i in range(3):  # lines under the book
            yy = top + (62 + 11 * i) * k
            d.line([(o + 14 * k, yy), (o + w - (14 + 18 * (i % 2)) * k, yy)], fill=blk, width=line)
    elif icon == "clock":
        r = 30 * k
        cy = top + 34 * k
        d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=blk, width=line)
        for a in range(0, 360, 90):
            x1, y1 = cx + math.sin(math.radians(a)) * r * 0.78, cy - math.cos(math.radians(a)) * r * 0.78
            x2, y2 = cx + math.sin(math.radians(a)) * r * 0.92, cy - math.cos(math.radians(a)) * r * 0.92
            d.line([(x1, y1), (x2, y2)], fill=blk, width=line)
        d.line([(cx, cy), (cx, cy - r * 0.62)], fill=blk, width=line)
        d.line([(cx, cy), (cx + r * 0.45, cy + r * 0.12)], fill=blk, width=line)
    elif icon == "check":
        cy = top + 30 * k
        d.line([(cx - 26 * k, cy), (cx - 8 * k, cy + 18 * k), (cx + 28 * k, cy - 20 * k)], fill=blk, width=line * 2,
               joint="curve")
        for i in range(3):
            yy = top + (66 + 11 * i) * k
            d.line([(o + 14 * k, yy), (o + w - 14 * k, yy)], fill=blk, width=line)
    elif icon == "chart":
        # Bars sit in the upper half so they show above the pocket's rim.
        base = o + h * 0.66
        bw = 13 * k
        for i, hh in enumerate((14, 26, 40, 56)):
            x = o + (18 + i * 18) * k
            d.rectangle((x, base - hh * k, x + bw, base), fill=blk)
        d.line([(o + 12 * k, base), (o + w - 12 * k, base)], fill=blk, width=line)
    return im


def draw_mark(size):
    """The pocket with four cards, square canvas of `size` px (drawn at S x)."""
    W = size * S
    line = max(S, int(W * 0.018))
    im = Image.new("RGBA", (W, W), (255, 255, 255, 255))

    # Cards behind the pocket: book (left), clock (back), checklist (right).
    cw, ch = int(W * 0.25), int(W * 0.33)
    layout = [("book", -10, 0.30, 0.10), ("clock", 0, 0.50, 0.04), ("check", 12, 0.70, 0.12)]
    for icon, angle, fx, fy in layout:
        c = card(W, icon, cw, ch, line).rotate(-angle, resample=Image.BICUBIC, expand=True)
        im.alpha_composite(c, (int(W * fx - c.width / 2), int(W * fy)))

    d = ImageDraw.Draw(im)
    # Pocket: straight sides narrowing into a shield point.
    px0, px1 = W * 0.18, W * 0.82
    ptop, pmid, pbot = W * 0.43, W * 0.72, W * 0.93
    pocket = [(px0, ptop), (px1, ptop), (px1, pmid), (W * 0.5, pbot), (px0, pmid)]
    d.polygon(pocket, fill=(255, 255, 255, 255))

    # Front card (chart) peeks out of the pocket, drawn before the pocket band.
    fc = card(W, "chart", int(W * 0.22), int(W * 0.26), line)
    im.alpha_composite(fc, (int(W * 0.5 - fc.width / 2), int(W * 0.235)))
    d = ImageDraw.Draw(im)
    d.polygon(pocket, fill=(255, 255, 255, 255))

    # Halftone band along the pocket's top edge, like printed shading.
    band = W * 0.075
    step = max(3, int(W * 0.022))
    for yy in range(int(ptop + line), int(ptop + band), step):
        t = (yy - ptop) / band
        rr = step * 0.42 * (1 - t) + 1
        off = (yy // step) % 2 * step / 2
        for xx in range(int(px0 + line * 2 + off), int(px1 - line * 2), step):
            d.ellipse((xx - rr, yy - rr, xx + rr, yy + rr), fill=(0, 0, 0, 255))

    d.line(pocket + [pocket[0]], fill=(0, 0, 0, 255), width=int(line * 1.4), joint="curve")
    # Pocket hem and stitching.
    d.line([(px0, ptop + band), (px1, ptop + band)], fill=(0, 0, 0, 255), width=line)
    inset = W * 0.035
    stitch = [(px0 + inset, ptop + band + inset), (px0 + inset, pmid - inset * 0.3), (W * 0.5, pbot - inset * 1.4),
              (px1 - inset, pmid - inset * 0.3), (px1 - inset, ptop + band + inset)]
    dash = W * 0.025
    for (x1, y1), (x2, y2) in zip(stitch, stitch[1:]):
        L = math.hypot(x2 - x1, y2 - y1)
        n = int(L / dash)
        for i in range(0, n, 2):
            a, b = i / n, min(1, (i + 1) / n)
            d.line([(x1 + (x2 - x1) * a, y1 + (y2 - y1) * a), (x1 + (x2 - x1) * b, y1 + (y2 - y1) * b)],
                   fill=(0, 0, 0, 255), width=max(1, line * 2 // 3))
    return im.convert("L").resize((size, size), Image.LANCZOS)


def lockup():
    mark = draw_mark(900)
    # Trim the empty band above the cards so the lockup is balanced.
    top = next(y for y in range(900) if min(mark.crop((0, y, 900, y + 1)).getdata()) < 128)
    mark = mark.crop((0, max(0, top - 10), 900, 900))
    W, H = 2400, 1290
    im = Image.new("L", (W, H), PAPER)
    im.paste(mark, ((W - 900) // 2, 950 - mark.height))
    bold = font(["Avenir Next.ttc"], 210, index=0)   # Avenir Next Bold
    reg = font(["Avenir Next.ttc"], 210, index=7)    # Avenir Next Regular
    d = ImageDraw.Draw(im)
    parts = [("pocket", bold), ("deck", reg), ("·", reg), ("os", reg)]
    widths = [d.textlength(t, font=f) for t, f in parts]
    x = (W - sum(widths)) / 2
    y = 930
    for (t, f), w in zip(parts, widths):
        d.text((x, y), t, font=f, fill=INK)
        x += w
    tag = font(["Avenir Next.ttc"], 54, index=7)
    sub = "a personal project by Ashok Kumar Srinivas"
    d.text(((W - d.textlength(sub, font=tag)) / 2, 1172), sub, font=tag, fill=110)
    return im


def to_1bit(img, size):
    small = img.resize((size, size), Image.LANCZOS)
    return small.point(lambda v: 255 if v > 150 else 0, mode="1")


def write_logo_header(bw, path, symbol="Logo120", size=120, rotate=True):
    # Stored in the panel's native orientation: the upright artwork is rotated
    # 90 degrees counter-clockwise, 1 bit per pixel, MSB first, 1 = white.
    rot = bw.rotate(90, expand=True) if rotate else bw
    px = rot.load()
    data = []
    for y in range(size):
        for xb in range(0, size, 8):
            v = 0
            for bit in range(8):
                v = (v << 1) | (1 if px[xb + bit, y] else 0)
            data.append(v)
    rows = []
    for i in range(0, len(data), 19):
        rows.append("    " + ", ".join(f"0x{b:02x}" for b in data[i:i + 19]) + ",")
    rows[-1] = rows[-1].rstrip(",") + "};"
    with open(path, "w") as f:
        f.write("#pragma once\n#include <cstdint>\n\n")
        f.write(f"// 'pocketdeck-os', {size}x{size}px. Generated by scripts/branding/make_logo.py.\n")
        f.write(f"inline constexpr uint8_t {symbol}[] = {{\n" + "\n".join(rows) + "\n")
        n = size * size // 8
        f.write(f'static_assert(sizeof({symbol}) == {n}, "{symbol} must be exactly {size}x{size} / 8 bytes");\n')


def main():
    out = os.path.join(ROOT, "assets", "branding")
    os.makedirs(out, exist_ok=True)
    lockup().save(os.path.join(out, "pocketdeck-os-logo.png"))
    icon = draw_mark(1024)
    icon.save(os.path.join(out, "pocketdeck-os-icon.png"))
    bw = to_1bit(draw_mark(960), 120)
    bw.convert("L").resize((480, 480), Image.NEAREST).save(os.path.join(out, "pocketdeck-os-icon-1bit.png"))
    write_logo_header(bw, os.path.join(ROOT, "src", "images", "Logo120.h"))
    mark = to_1bit(draw_mark(512), 32)
    write_logo_header(mark, os.path.join(ROOT, "src", "images", "PocketDeckMark32.h"), "PocketDeckMark32", 32, rotate=False)
    big = to_1bit(draw_mark(1440), 240)
    write_logo_header(big, os.path.join(ROOT, "src", "images", "PocketDeckLogo240.h"), "PocketDeckLogo240", 240)
    # Web portal header mark: white on transparent for the dark header bar.
    m = draw_mark(320).resize((80, 80), Image.LANCZOS)
    web = Image.merge("LA", (Image.new("L", m.size, 255), m.point(lambda v: 255 - v)))
    web.save(os.path.join(ROOT, "web", "assets", "logo.png"))
    print("PocketDeck-OS assets written")


if __name__ == "__main__":
    sys.exit(main())
