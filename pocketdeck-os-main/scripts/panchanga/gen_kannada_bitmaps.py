#!/usr/bin/env python3
"""Pre-render the Panchanga's Kannada strings as 1-bit bitmaps.

Kannada needs complex text shaping (conjuncts, reordered vowel signs), which
the firmware's text renderer does not do. Every string the Panchanga shows is
therefore shaped here with HarfBuzz and rasterised with FreeType, using the
open-source Noto Sans Kannada font (SIL Open Font License,
lib/EpdFont/builtinFonts/source/NotoSansKannada/), and stored in flash.

usage: gen_kannada_bitmaps.py [--preview preview.png]
writes: src/activities/tools/PanchangaKannada.h
requires: pip install uharfbuzz freetype-py pillow
"""
import argparse, os, sys
import freetype
import uharfbuzz as hb
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kn_rle  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FONT_DIR = os.path.join(ROOT, "lib", "EpdFont", "builtinFonts", "source", "NotoSansKannada")
OUT = os.path.join(ROOT, "src", "activities", "tools", "PanchangaKannada.h")

# (style, pixel size, font file)
STYLES = {"label": (19, "NotoSansKannada-Regular.ttf"), "value": (21, "NotoSansKannada-Bold.ttf"),
          "title": (26, "NotoSansKannada-Bold.ttf")}

GROUPS = [  # (C++ array name, style, strings)
  ("kWeekday", "value", ["ಭಾನುವಾರ", "ಸೋಮವಾರ", "ಮಂಗಳವಾರ", "ಬುಧವಾರ", "ಗುರುವಾರ", "ಶುಕ್ರವಾರ", "ಶನಿವಾರ"]),
  ("kGregorianMonth", "value", ["ಜನವರಿ", "ಫೆಬ್ರವರಿ", "ಮಾರ್ಚ್", "ಏಪ್ರಿಲ್", "ಮೇ", "ಜೂನ್", "ಜುಲೈ", "ಆಗಸ್ಟ್", "ಸೆಪ್ಟೆಂಬರ್",
                                "ಅಕ್ಟೋಬರ್", "ನವೆಂಬರ್", "ಡಿಸೆಂಬರ್"]),
  ("kMasa", "value", ["ಚೈತ್ರ", "ವೈಶಾಖ", "ಜ್ಯೇಷ್ಠ", "ಆಷಾಢ", "ಶ್ರಾವಣ", "ಭಾದ್ರಪದ", "ಆಶ್ವಯುಜ", "ಕಾರ್ತಿಕ", "ಮಾರ್ಗಶಿರ",
                      "ಪುಷ್ಯ", "ಮಾಘ", "ಫಾಲ್ಗುಣ"]),
  ("kSamvatsara", "value", ["ಪ್ರಭವ", "ವಿಭವ", "ಶುಕ್ಲ", "ಪ್ರಮೋದೂತ", "ಪ್ರಜೋತ್ಪತ್ತಿ", "ಆಂಗೀರಸ", "ಶ್ರೀಮುಖ", "ಭಾವ", "ಯುವ",
     "ಧಾತು", "ಈಶ್ವರ", "ಬಹುಧಾನ್ಯ", "ಪ್ರಮಾಥಿ", "ವಿಕ್ರಮ", "ವೃಷ", "ಚಿತ್ರಭಾನು", "ಸ್ವಭಾನು", "ತಾರಣ", "ಪಾರ್ಥಿವ", "ವ್ಯಯ",
     "ಸರ್ವಜಿತ್", "ಸರ್ವಧಾರಿ", "ವಿರೋಧಿ", "ವಿಕೃತಿ", "ಖರ", "ನಂದನ", "ವಿಜಯ", "ಜಯ", "ಮನ್ಮಥ", "ದುರ್ಮುಖಿ", "ಹೇವಿಳಂಬಿ",
     "ವಿಳಂಬಿ", "ವಿಕಾರಿ", "ಶಾರ್ವರಿ", "ಪ್ಲವ", "ಶುಭಕೃತ್", "ಶೋಭಕೃತ್", "ಕ್ರೋಧಿ", "ವಿಶ್ವಾವಸು", "ಪರಾಭವ", "ಪ್ಲವಂಗ", "ಕೀಲಕ",
     "ಸೌಮ್ಯ", "ಸಾಧಾರಣ", "ವಿರೋಧಿಕೃತ್", "ಪರಿಧಾವಿ", "ಪ್ರಮಾದೀಚ", "ಆನಂದ", "ರಾಕ್ಷಸ", "ನಳ", "ಪಿಂಗಳ", "ಕಾಳಯುಕ್ತಿ",
     "ಸಿದ್ಧಾರ್ಥಿ", "ರೌದ್ರಿ", "ದುರ್ಮತಿ", "ದುಂದುಭಿ", "ರುಧಿರೋದ್ಗಾರಿ", "ರಕ್ತಾಕ್ಷಿ", "ಕ್ರೋಧನ", "ಅಕ್ಷಯ"]),
  # 0..13 = pratipada..chaturdashi, 14 = purnima, 15 = amavasya
  ("kTithi", "value", ["ಪಾಡ್ಯ", "ಬಿದಿಗೆ", "ತದಿಗೆ", "ಚತುರ್ಥಿ", "ಪಂಚಮಿ", "ಷಷ್ಠಿ", "ಸಪ್ತಮಿ", "ಅಷ್ಟಮಿ", "ನವಮಿ", "ದಶಮಿ",
                       "ಏಕಾದಶಿ", "ದ್ವಾದಶಿ", "ತ್ರಯೋದಶಿ", "ಚತುರ್ದಶಿ", "ಹುಣ್ಣಿಮೆ", "ಅಮಾವಾಸ್ಯೆ"]),
  ("kPaksha", "value", ["ಶುಕ್ಲ ಪಕ್ಷ", "ಕೃಷ್ಣ ಪಕ್ಷ"]),
  ("kPakshaShort", "label", ["ಶುಕ್ಲ", "ಕೃಷ್ಣ"]),
  ("kNakshatra", "value", ["ಅಶ್ವಿನಿ", "ಭರಣಿ", "ಕೃತ್ತಿಕಾ", "ರೋಹಿಣಿ", "ಮೃಗಶಿರಾ", "ಆರ್ದ್ರಾ", "ಪುನರ್ವಸು", "ಪುಷ್ಯ", "ಆಶ್ಲೇಷಾ",
     "ಮಘಾ", "ಪೂರ್ವ ಫಲ್ಗುಣಿ", "ಉತ್ತರ ಫಲ್ಗುಣಿ", "ಹಸ್ತ", "ಚಿತ್ತಾ", "ಸ್ವಾತಿ", "ವಿಶಾಖ", "ಅನುರಾಧಾ", "ಜ್ಯೇಷ್ಠ", "ಮೂಲ",
     "ಪೂರ್ವಾಷಾಢ", "ಉತ್ತರಾಷಾಢ", "ಶ್ರವಣ", "ಧನಿಷ್ಠ", "ಶತಭಿಷ", "ಪೂರ್ವಾಭಾದ್ರ", "ಉತ್ತರಾಭಾದ್ರ", "ರೇವತಿ"]),
  ("kNakshatraLabel", "label", ["ಅಶ್ವಿನಿ", "ಭರಣಿ", "ಕೃತ್ತಿಕಾ", "ರೋಹಿಣಿ", "ಮೃಗಶಿರಾ", "ಆರ್ದ್ರಾ", "ಪುನರ್ವಸು", "ಪುಷ್ಯ", "ಆಶ್ಲೇಷಾ",
     "ಮಘಾ", "ಪೂರ್ವ ಫಲ್ಗುಣಿ", "ಉತ್ತರ ಫಲ್ಗುಣಿ", "ಹಸ್ತ", "ಚಿತ್ತಾ", "ಸ್ವಾತಿ", "ವಿಶಾಖ", "ಅನುರಾಧಾ", "ಜ್ಯೇಷ್ಠ", "ಮೂಲ",
     "ಪೂರ್ವಾಷಾಢ", "ಉತ್ತರಾಷಾಢ", "ಶ್ರವಣ", "ಧನಿಷ್ಠ", "ಶತಭಿಷ", "ಪೂರ್ವಾಭಾದ್ರ", "ಉತ್ತರಾಭಾದ್ರ", "ರೇವತಿ"]),
  ("kYoga", "value", ["ವಿಷ್ಕಂಭ", "ಪ್ರೀತಿ", "ಆಯುಷ್ಮಾನ್", "ಸೌಭಾಗ್ಯ", "ಶೋಭನ", "ಅತಿಗಂಡ", "ಸುಕರ್ಮ", "ಧೃತಿ", "ಶೂಲ", "ಗಂಡ",
     "ವೃದ್ಧಿ", "ಧ್ರುವ", "ವ್ಯಾಘಾತ", "ಹರ್ಷಣ", "ವಜ್ರ", "ಸಿದ್ಧಿ", "ವ್ಯತೀಪಾತ", "ವರೀಯಾನ್", "ಪರಿಘ", "ಶಿವ", "ಸಿದ್ಧ", "ಸಾಧ್ಯ",
     "ಶುಭ", "ಶುಕ್ಲ", "ಬ್ರಹ್ಮ", "ಐಂದ್ರ", "ವೈಧೃತಿ"]),
  ("kKarana", "value", ["ಬವ", "ಬಾಲವ", "ಕೌಲವ", "ತೈತಿಲ", "ಗರಜ", "ವಣಿಜ", "ವಿಷ್ಟಿ", "ಶಕುನಿ", "ಚತುಷ್ಪಾದ", "ನಾಗ", "ಕಿಂಸ್ತುಘ್ನ"]),
  # Order matches panchanga::Special (index 0 = None is unused).
  ("kSpecial", "value", ["", "ಯುಗಾದಿ", "ಶ್ರೀರಾಮ ನವಮಿ", "ಅಕ್ಷಯ ತೃತೀಯಾ", "ನಾಗರ ಪಂಚಮಿ", "ಕೃಷ್ಣ ಜನ್ಮಾಷ್ಟಮಿ", "ಗಣೇಶ ಚತುರ್ಥಿ",
     "ಮಹಾಲಯ ಅಮಾವಾಸ್ಯೆ", "ನವರಾತ್ರಿ ಆರಂಭ", "ವಿಜಯದಶಮಿ", "ನರಕ ಚತುರ್ದಶಿ", "ದೀಪಾವಳಿ", "ಬಲಿಪಾಡ್ಯಮಿ", "ಮಹಾಶಿವರಾತ್ರಿ",
     "ಹೋಳಿ ಹುಣ್ಣಿಮೆ", "ಮಕರ ಸಂಕ್ರಾಂತಿ", "ಸಂಕ್ರಮಣ", "ಏಕಾದಶಿ", "ಹುಣ್ಣಿಮೆ", "ಅಮಾವಾಸ್ಯೆ", "ಸಂಕಷ್ಟ ಚತುರ್ಥಿ"]),
  ("kLabel", "label", ["ಸಂವತ್ಸರ", "ಮಾಸ", "ತಿಥಿ", "ನಕ್ಷತ್ರ", "ಯೋಗ", "ಕರಣ", "ವಾರ", "ಸೂರ್ಯೋದಯ", "ಸೂರ್ಯಾಸ್ತ", "ರಾಹು ಕಾಲ",
     "ಯಮಗಂಡ ಕಾಲ", "ಗುಳಿಕ ಕಾಲ", "ಅಭಿಜಿತ್ ಮುಹೂರ್ತ", "ದಿನದ ವಿಶೇಷ", "ಚಂದ್ರ", "ಬೆಳಕು", "ಚಂದ್ರ ನಕ್ಷತ್ರ", "ವರೆಗೆ", "ಇಂದು",
     "ಬೆಂಗಳೂರು", "ಅಧಿಕ", "ಗಡಿಯಾರ ಹೊಂದಿಸಿಲ್ಲ", "ವಿಶೇಷ ದಿನವಿಲ್ಲ", "ನಾಳೆ", "ಪೂರ್ಣ ರಾತ್ರಿ"]),
  ("kTitle", "title", ["ಪಂಚಾಂಗ"]),
]
LABEL_NAMES = ["Samvatsara", "Masa", "Tithi", "Nakshatra", "Yoga", "Karana", "Vara", "Sunrise", "Sunset", "Rahu",
               "Yamaganda", "Gulika", "Abhijit", "Special", "Moon", "Light", "MoonNakshatra", "Until", "Today",
               "Bengaluru", "Adhika", "ClockNotSet", "NoSpecial", "Tomorrow", "FullNight"]


class Renderer:
    def __init__(self, size, fontfile):
        path = os.path.join(FONT_DIR, fontfile)
        self.face = hb.Face(hb.Blob.from_file_path(path))
        self.font = hb.Font(self.face)
        self.upem = self.face.upem
        self.ft = freetype.Face(path)
        self.ft.set_pixel_sizes(0, size)
        self.scale = size / self.upem
        # Fixed line box from the font's vertical metrics so all strings align.
        self.ascent = int(round(self.ft.ascender * self.scale)) + 1
        self.height = self.ascent + int(round(-self.ft.descender * self.scale)) + 1

    def render(self, text):
        if not text:
            return Image.new("1", (1, self.height), 1)
        buf = hb.Buffer()
        buf.add_str(text)
        buf.guess_segment_properties()
        hb.shape(self.font, buf, {"kern": True, "liga": True})
        width = int(sum(p.x_advance for p in buf.glyph_positions) * self.scale) + 4
        img = Image.new("L", (width, self.height), 0)
        x = 1.0
        for info, pos in zip(buf.glyph_infos, buf.glyph_positions):
            self.ft.load_glyph(info.codepoint, freetype.FT_LOAD_DEFAULT)
            self.ft.glyph.render(freetype.FT_RENDER_MODE_NORMAL)
            bm = self.ft.glyph.bitmap
            if bm.width and bm.rows:
                g = Image.frombytes("L", (bm.width, bm.rows), bytes(bm.buffer), "raw", "L", bm.pitch)
                gx = int(round(x + pos.x_offset * self.scale)) + self.ft.glyph.bitmap_left
                gy = self.ascent - int(round(pos.y_offset * self.scale)) - self.ft.glyph.bitmap_top
                layer = Image.new("L", img.size, 0)
                layer.paste(g, (gx, gy))
                img = Image.fromarray(__import__("numpy").maximum(__import__("numpy").array(img), __import__("numpy").array(layer)))
            x += pos.x_advance * self.scale
        # Trim horizontal slack, keep the fixed line box.
        bbox = img.getbbox()
        if bbox:
            img = img.crop((max(0, bbox[0] - 1), 0, min(img.width, bbox[2] + 1), self.height))
        return img.point(lambda v: 0 if v >= 110 else 255, mode="1")  # 0 = ink


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview")
    args = ap.parse_args()
    renderers = {k: Renderer(*v) for k, v in STYLES.items()}
    blob = bytearray()
    groups_cpp = []
    previews = []
    for name, style, strings in GROUPS:
        entries = []
        for s in strings:
            img = renderers[style].render(s)
            w, h = img.size
            px = img.load()
            offset = len(blob)
            blob.extend(kn_rle.encode(px[xx, y] == 0 for y in range(h) for xx in range(w)))
            entries.append(f"    {{{w}, {h}, {offset}}}")
            previews.append(img)
        groups_cpp.append(f"inline constexpr KnText {name}[] = {{\n" + ",\n".join(entries) + "\n};")
    label_enum = "enum class Label : uint8_t {\n" + ",\n".join(f"  {n}" for n in LABEL_NAMES) + "\n};"
    rows = [", ".join(f"0x{b:02x}" for b in blob[i:i + 24]) for i in range(0, len(blob), 24)]
    with open(OUT, "w") as f:
        f.write("#pragma once\n\n#include <cstdint>\n\n")
        f.write("// Generated by scripts/panchanga/gen_kannada_bitmaps.py. Do not edit.\n")
        f.write("// Kannada strings shaped with HarfBuzz and rendered from Noto Sans Kannada\n")
        f.write("// (SIL Open Font License 1.1). Pixels are nibble run-length encoded, see\n")
        f.write("// scripts/panchanga/kn_rle.py: row-major runs alternating blank/ink from\n")
        f.write("// blank; nibble 15 continues a run, 0..14 ends it; high nibble first.\n")
        f.write("namespace kn {\n\n")
        f.write("struct KnText {\n  uint16_t w;\n  uint16_t h;\n  uint32_t offset;  // into kBits: first byte of this string's runs\n};\n\n")
        f.write(label_enum + "\n\n")
        f.write(f"inline constexpr uint8_t kBits[{len(blob)}] = {{\n    " + ",\n    ".join(rows) + "};\n\n")
        f.write("\n\n".join(groups_cpp) + "\n\n}  // namespace kn\n")
    print(f"{sum(len(g[2]) for g in GROUPS)} strings, {len(blob)} bytes")
    if args.preview:
        W = 560
        y = 0
        x = 0
        rowh = 0
        placed = []
        for im in previews:
            if x + im.width > W:
                x = 0
                y += rowh + 4
                rowh = 0
            placed.append((im, x, y))
            x += im.width + 12
            rowh = max(rowh, im.height)
        sheet = Image.new("1", (W, y + rowh + 4), 1)
        for im, px_, py in placed:
            sheet.paste(im, (px_, py))
        sheet.save(args.preview)


if __name__ == "__main__":
    main()
