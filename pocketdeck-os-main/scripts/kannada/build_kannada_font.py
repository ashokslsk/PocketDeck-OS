#!/usr/bin/env python3
"""Build the SD-card Kannada font used by the tools (Panchanga, Mantras, and
any Kannada text in quotes, flashcards or knowledge files).

The firmware cannot run an OpenType shaper, so every Kannada syllable of up to
three consonants (plus reph, vowel sign or virama) is shaped here with
HarfBuzz and stored as a fixed-size slot: the device finds a syllable's glyphs
with one small read, computed from the syllable's letters. Glyph bitmaps for
four text styles are rasterised from Noto Sans Kannada (SIL Open Font License).
Nothing here is stored in firmware flash.

usage:  build_kannada_font.py [--out sd-sample/tools/fonts/kannada.knf]
                              [--check corpus.json ...] [--golden test/tools_core/kannada_golden.inc]
requires: pip install uharfbuzz freetype-py fonttools

File layout (little-endian), see docs/file-formats.md "kannada.knf":
  header (64 bytes)
    0  magic "KNF1"          4  version u16        6  glyphCount u16
    8  consonants u8 (39)    9  slotsPerCons u8 (40)  10 matras u8 (15)  11 slotSize u8 (20)
   12  dictOffset u32       16  dictSlots u32
   20  cmapOffset u32       24  cmapCount u16     26  rephGlyph u16
   28  advOffset u32        (glyphCount x {regular u16, bold u16} font-unit advances)
   32  styleCount u8        33  reserved[3]
   36  styleOffset u32      (styleCount x 16-byte records)
   40  unitsPerEm u16       42  kernOffset u32     46  kernCount u16     48..63 reserved
  kerning: kernCount x {left u16, right u16, delta i16}, sorted by (left, right); delta is added
    to the left glyph's advance when the right glyph follows it
  cmap: cmapCount x {codepoint u32, glyph u16, pad u16}, sorted by codepoint
  style record: px u8, bold u8, ascent u8, height u8, metricsOffset u32, bitmapsOffset u32, reserved u32
  metrics: glyphCount x {bitmapOffset u32 (relative to bitmapsOffset), w u8, h u8, left i8, top i8}
  bitmaps: 1 bit per pixel, rows padded to whole bytes, MSB first, 1 = ink
  dictionary slot (20 bytes): count u8, then up to 6 x 3-byte glyphs (glyph id in the low
    10 bits, font-unit advance in the high 14 bits). count 0 = not a valid syllable.
    slot index = ((c1 * 40 + c2) * 40 + c3) * 15 + matra, c1 in 0..38, c2/c3 = 0 (none) or 1 + consonant.
"""
import argparse, json, os, re, struct, sys, random

import freetype
import uharfbuzz as hb
from fontTools.ttLib import TTFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FONT_DIR = os.path.join(ROOT, "lib", "EpdFont", "builtinFonts", "source", "NotoSansKannada")
REGULAR = os.path.join(FONT_DIR, "NotoSansKannada-Regular.ttf")
BOLD = os.path.join(FONT_DIR, "NotoSansKannada-Bold.ttf")

VIRAMA = "್"
NUKTA = "಼"
RA = "ರ"
ZWNJ, ZWJ = "‌", "‍"
# Must match KannadaText.cpp.
CONSONANTS = [chr(c) for c in range(0x0C95, 0x0CBA) if c not in (0x0CA9, 0x0CB4)] + ["ೝ", "ೞ",
                                                                                    "ಜ಼", "ಫ಼",
                                                                                    "ಕ್ಷ", "ಜ್ಞ"]
# ಕ್ಷ and ಜ್ಞ are single letters in the font (akhand ligatures), so they count as one consonant.
CONJUNCTS = ("ಕ್ಷ", "ಜ್ಞ")
MATRAS = ["", "ಾ", "ಿ", "ೀ", "ು", "ೂ", "ೃ", "ೄ", "ೆ", "ೇ", "ೈ",
          "ೊ", "ೋ", "ೌ", VIRAMA]
SLOTS_PER = len(CONSONANTS) + 1  # 0 = none
SLOT_SIZE = 20
MAX_GLYPHS = 6
# (px, bold): 0 = labels, 1 = values, 2 = titles, 3 = body text. Must match kannada::Style.
STYLES = [(19, False), (21, True), (26, True), (24, False)]
# Extra (non-Kannada-block) codepoints drawn with this font inside Kannada text.
EXTRA_CODEPOINTS = [0x20, 0xA0, 0x0964, 0x0965, 0x2010, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026,
                    0x20B9] + list(range(0x21, 0x40)) + [0x5B, 0x5D, 0x5F, 0x7B, 0x7D]


class Shaper:
    def __init__(self, path):
        self.font = hb.Font(hb.Face(hb.Blob.from_file_path(path)))

    def shape(self, text):
        b = hb.Buffer()
        b.add_str(text)
        b.direction = "ltr"
        b.script = "Knda"
        b.language = "kn"
        hb.shape(self.font, b)
        return [(i.codepoint, p.x_advance) for i, p in zip(b.glyph_infos, b.glyph_positions)]


def slot_index(c1, c2, c3, m):
    return ((c1 * SLOTS_PER + c2) * SLOTS_PER + c3) * len(MATRAS) + m


def build_dictionary(shaper):
    n = len(CONSONANTS) * SLOTS_PER * SLOTS_PER * len(MATRAS)
    data = bytearray(n * SLOT_SIZE)
    overflow = 0
    for c1 in range(len(CONSONANTS)):
        for c2 in range(SLOTS_PER):
            for c3 in range(SLOTS_PER):
                if c2 == 0 and c3 != 0:
                    continue
                stem = CONSONANTS[c1]
                if c2:
                    stem += VIRAMA + CONSONANTS[c2 - 1]
                if c3:
                    stem += VIRAMA + CONSONANTS[c3 - 1]
                for m, matra in enumerate(MATRAS):
                    glyphs = shaper.shape(stem + matra)
                    if len(glyphs) > MAX_GLYPHS:
                        overflow += 1
                        glyphs = glyphs[:MAX_GLYPHS]
                    off = slot_index(c1, c2, c3, m) * SLOT_SIZE
                    data[off] = len(glyphs)
                    for k, (g, adv) in enumerate(glyphs):
                        v = g | (max(0, adv) << 10)
                        data[off + 1 + 3 * k: off + 4 + 3 * k] = struct.pack("<I", v)[:3]
        print(f"  dictionary: consonant {c1 + 1}/{len(CONSONANTS)}", end="\r", file=sys.stderr)
    print(file=sys.stderr)
    return data, overflow


def build_kerning(shaper, dic, cmap, reph):
    """Pair adjustments between the last glyph of one syllable and the first
    glyph of the next (GPOS 'dist' shortens subscripts before some glyphs).
    Returns {(left, right): delta} applied to the left glyph's advance."""
    # One representative string per last glyph and per first glyph.
    last_rep, first_rep = {}, {}
    combos = [(c1, c2, 0) for c1 in range(len(CONSONANTS)) for c2 in range(SLOTS_PER)]
    # Second-level subscripts (alt forms) only appear in three-consonant syllables.
    combos += [(0, c2, c3) for c2 in range(1, SLOTS_PER) for c3 in range(1, SLOTS_PER)]
    for c1, c2, c3 in combos:
        for m, matra in enumerate(MATRAS):
            if m == len(MATRAS) - 1:
                continue  # a final virama never precedes a joined glyph
            off = slot_index(c1, c2, c3, m) * SLOT_SIZE
            cnt = dic[off]
            if not cnt:
                continue
            g_last = struct.unpack("<I", bytes(dic[off + 1 + 3 * (cnt - 1): off + 4 + 3 * (cnt - 1)]) + b"\0")[0]
            g_first = struct.unpack("<I", bytes(dic[off + 1: off + 4]) + b"\0")[0]
            text = (CONSONANTS[c1] + (VIRAMA + CONSONANTS[c2 - 1] if c2 else "") +
                    (VIRAMA + CONSONANTS[c3 - 1] if c3 else "") + matra)
            last_rep.setdefault(g_last & 0x3FF, []).append(text)
            first_rep.setdefault(g_first & 0x3FF, []).append(text)
    for cp, g in cmap.items():
        first_rep.setdefault(g, []).append(chr(cp))
    last_rep.setdefault(reph, []).append(RA + VIRAMA + "\u0c95")
    kern = {}
    for lg, lreps in last_rep.items():
        lrep = lreps[0]
        base = shaper.shape(lrep)
        if not base or base[-1][0] != lg:
            continue
        alone = base[-1][1]
        if lg != reph:  # the reph follows the syllable it belongs to
            with_reph = shaper.shape(RA + VIRAMA + lrep)
            if len(with_reph) == len(base) + 1 and with_reph[len(base) - 1][0] == lg and with_reph[-1][0] == reph:
                d = with_reph[len(base) - 1][1] - alone
                if d:
                    kern[(lg, reph)] = d
        for fg, freps in first_rep.items():
            got = shaper.shape(lrep + freps[0])
            if len(got) < len(base) or got[len(base) - 1][0] != lg:
                continue
            d = got[len(base) - 1][1] - alone
            if d:
                kern[(lg, fg)] = d
    return kern


# --- Reference implementation of the device algorithm (KannadaText.cpp). ---
def is_cons_at(text, i):
    """Returns (consonant index, length) at text[i], or (-1, 0)."""
    if i >= len(text):
        return -1, 0
    if text[i:i + 3] in CONJUNCTS:
        return CONSONANTS.index(text[i:i + 3]), 3
    if i + 1 < len(text) and text[i + 1] == NUKTA and text[i:i + 2] in CONSONANTS:
        return CONSONANTS.index(text[i:i + 2]), 2
    if text[i] in CONSONANTS:
        return CONSONANTS.index(text[i]), 1
    return -1, 0


def device_shape(text, dic, cmap, reph_glyph, adv_regular, kern=None):
    out = []
    kern = kern or {}

    def emit(g, a, boundary=True):
        # Pair kerning only applies across syllables; the dictionary already
        # holds the adjusted advances inside a syllable.
        if out and boundary:
            d = kern.get((out[-1][0], g))
            if d:
                out[-1] = (out[-1][0], out[-1][1] + d)
        out.append((g, a))

    i = 0
    n = len(text)
    while i < n:
        ch = text[i]
        c, clen = is_cons_at(text, i)
        if c >= 0:
            reph = False
            if ch == RA and i + 2 < n and text[i + 1] == VIRAMA and is_cons_at(text, i + 2)[0] >= 0:
                reph = True
                i += 2
                c, clen = is_cons_at(text, i)
            cons = [c]
            i += clen
            while len(cons) < 3 and i + 1 < n and text[i] == VIRAMA:
                c2, l2 = is_cons_at(text, i + 1)
                if c2 < 0:
                    break
                cons.append(c2)
                i += 1 + l2
            m = 0
            if i < n and text[i] in MATRAS[1:]:
                m = MATRAS.index(text[i])
                i += 1
            c2 = cons[1] + 1 if len(cons) > 1 else 0
            c3 = cons[2] + 1 if len(cons) > 2 else 0
            off = slot_index(cons[0], c2, c3, m) * SLOT_SIZE
            cnt = dic[off]
            for k in range(cnt):
                v = struct.unpack("<I", bytes(dic[off + 1 + 3 * k: off + 4 + 3 * k]) + b"\0")[0]
                emit(v & 0x3FF, v >> 10, k == 0)
            if reph:
                emit(reph_glyph, adv_regular[reph_glyph])
            continue
        if ch in (ZWJ, ZWNJ):
            i += 1
            continue
        g = cmap.get(ord(ch))
        if g is not None:
            emit(g, adv_regular[g])
        i += 1
    return out


def rasterise(path, px, glyph_count):
    face = freetype.Face(path)
    face.set_pixel_sizes(0, px)
    scale = px / face.units_per_EM
    ascent = int(round(face.ascender * scale)) + 1
    height = ascent + int(round(-face.descender * scale)) + 1
    metrics, blob = [], bytearray()
    for g in range(glyph_count):
        face.load_glyph(g, freetype.FT_LOAD_DEFAULT)
        face.glyph.render(freetype.FT_RENDER_MODE_NORMAL)
        bm = face.glyph.bitmap
        w, h = bm.width, bm.rows
        if w == 0 or h == 0:
            metrics.append((len(blob), 0, 0, 0, 0))
            continue
        stride = (w + 7) // 8
        start = len(blob)
        buf = bytes(bm.buffer)
        for y in range(h):
            row = bytearray(stride)
            for x in range(w):
                if buf[y * bm.pitch + x] >= 110:  # same threshold as the old pre-rendered strings
                    row[x >> 3] |= 0x80 >> (x & 7)
            blob.extend(row)
        metrics.append((start, w, h, face.glyph.bitmap_left, face.glyph.bitmap_top))
    return ascent, height, metrics, blob


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "sd-sample", "tools", "fonts", "kannada.knf"))
    ap.add_argument("--check", nargs="*", default=[], help="JSON/text files whose Kannada words are verified")
    ap.add_argument("--golden", help="write C++ test vectors (string -> glyphs) for test/tools_core")
    args = ap.parse_args()

    reg, bold = TTFont(REGULAR), TTFont(BOLD)
    assert reg.getGlyphOrder() == bold.getGlyphOrder(), "Regular and Bold must share glyph ids"
    order = reg.getGlyphOrder()
    glyph_count = len(order)
    assert glyph_count < 1024
    adv_r = [reg["hmtx"][n][0] for n in order]
    adv_b = [bold["hmtx"][n][0] for n in order]
    cmap_src = reg.getBestCmap()
    cmap = {cp: order.index(name) for cp, name in cmap_src.items()
            if 0x0C80 <= cp <= 0x0CFF or cp in EXTRA_CODEPOINTS}
    reph = order.index("rephknda")

    shaper = Shaper(REGULAR)
    print("shaping every syllable of up to three consonants...", file=sys.stderr)
    dic, overflow = build_dictionary(shaper)
    print(f"  {len(dic) // SLOT_SIZE} slots, {overflow} longer than {MAX_GLYPHS} glyphs (truncated)", file=sys.stderr)

    # Verify the device algorithm against HarfBuzz on real text.
    words = set()
    for path in args.check:
        words.update(re.findall("[ಀ-೿‌‍।॥]+", open(path, encoding="utf-8").read()))
    corpus = set(words)
    random.seed(7)
    for _ in range(20000):  # random syllable soup, including reph and modifiers
        s = ""
        for _ in range(random.randint(1, 4)):
            k = random.choice([1, 1, 1, 2, 2, 3])
            cl = [random.choice(CONSONANTS) for _ in range(k)]
            s += ("ರ್" if random.random() < 0.1 else "") + VIRAMA.join(cl) + random.choice(MATRAS)
            s += random.choice(["", "", "", "ಂ", "ಃ"])
        words.add(s)
    print("deriving pair kerning...", file=sys.stderr)
    kern = build_kerning(shaper, dic, cmap, reph)
    print(f"  {len(kern)} kerning pairs", file=sys.stderr)
    space = order.index("space")
    bad = []
    for w in sorted(words):
        want = [(g, a) for g, a in shaper.shape(w) if not (g == space and a == 0)]
        got = device_shape(w, dic, cmap, reph, adv_r, kern)
        if [g for g, _ in want] != [g for g, _ in got]:
            bad.append(w)
        elif sum(a for _, a in want) != sum(a for _, a in got):
            bad.append(w)
    bad_corpus = [w for w in bad if w in corpus]
    print(f"verified {len(corpus)} real words against HarfBuzz: {len(corpus) - len(bad_corpus)} identical, "
          f"{len(bad_corpus)} differ", file=sys.stderr)
    print(f"verified {len(words) - len(corpus)} random syllable strings: {len(bad) - len(bad_corpus)} differ "
          "(four-consonant clusters are not in the dictionary)", file=sys.stderr)
    bad = bad_corpus + [w for w in bad if w not in corpus]
    for w in bad[:12]:
        print("   differs:", w, [order[g] for g, _ in shaper.shape(w)],
              [(order[g], a) for g, a in device_shape(w, dic, cmap, reph, adv_r, kern)],
              [(order[g], a) for g, a in shaper.shape(w)], file=sys.stderr)

    styles = [rasterise(BOLD if b else REGULAR, px, glyph_count) for px, b in STYLES]

    out = bytearray(64)
    cmap_items = sorted(cmap.items())
    cmap_off = len(out)
    for cp, g in cmap_items:
        out += struct.pack("<IHH", cp, g, 0)
    kern_off = len(out)
    for (l, r), d in sorted(kern.items()):
        out += struct.pack("<HHh", l, r, d)
    while len(out) % 4:
        out.append(0)
    adv_off = len(out)
    for a, b in zip(adv_r, adv_b):
        out += struct.pack("<HH", a, b)
    style_off = len(out)
    out += bytes(16 * len(STYLES))
    records = []
    for (px, b), (ascent, height, metrics, blob) in zip(STYLES, styles):
        m_off = len(out)
        for (boff, w, h, left, top) in metrics:
            out += struct.pack("<IBBbb", boff, w, h, max(-128, min(127, left)), max(-128, min(127, top)))
        b_off = len(out)
        out += blob
        records.append(struct.pack("<BBBBIII", px, 1 if b else 0, ascent, height, m_off, b_off, 0))
    for k, r in enumerate(records):
        out[style_off + 16 * k: style_off + 16 * (k + 1)] = r
    while len(out) % 16:
        out.append(0)
    dict_off = len(out)
    out += dic
    header = struct.pack("<4sHHBBBBIIIHHIB3xIH", b"KNF1", 1, glyph_count, len(CONSONANTS), SLOTS_PER, len(MATRAS),
                         SLOT_SIZE, dict_off, len(dic) // SLOT_SIZE, cmap_off, len(cmap_items), reph, adv_off,
                         len(STYLES), style_off, reg["head"].unitsPerEm)
    out[0:len(header)] = header
    out[42:48] = struct.pack("<IH", kern_off, len(kern))
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    tmp = args.out + ".tmp"
    with open(tmp, "wb") as f:
        f.write(out)
    os.replace(tmp, args.out)
    print(f"wrote {args.out}: {len(out):,} bytes ({len(dic):,} dictionary, {len(out) - len(dic):,} glyphs/tables)",
          file=sys.stderr)

    if args.golden:
        samples = ["ಪಂಚಾಂಗ", "ಶ್ರೀ", "ಲಕ್ಷ್ಮೀಃ", "ಸ್ತ್ರೀ", "ಕಾರ್ತಿಕ", "ಮಂತ್ರ", "ತತ್ತ್ವ", "ಕ್ರಿಸ್‌ಮಸ್", "ಓಂ ಗಂ ಗಣಪತಯೇ ನಮಃ",
                   "ಜ್ಞಾನ", "ರ್ಧ್ಯಾ", "ಸೌಮ್ಯ", "॥೧॥", "ಅಕ್ಷಯ", "ಹೃದಯಾಯ", "ಗ್ರೌ", "ಕೈ", "ಕೊ", "ಶುಭಕೃತ್"]
        lines = ["// Generated by scripts/kannada/build_kannada_font.py --golden. Do not edit.",
                 "// UTF-8 input and the glyph ids + font-unit advances HarfBuzz produces."]
        for s in samples:
            want = [(g, a) for g, a in shaper.shape(s) if not (g == space and a == 0)]
            assert want == device_shape(s, dic, cmap, reph, adv_r, kern), s
            esc = "".join(f"\\x{b:02x}" for b in s.encode("utf-8"))
            body = ", ".join(f"{{{g}, {a}}}" for g, a in want)
            lines.append(f'{{"{esc}", {len(want)}, {{{body}}}}},')
        with open(args.golden, "w") as f:
            f.write("\n".join(lines) + "\n")
    return 0 if not bad_corpus else 1


if __name__ == "__main__":
    sys.exit(main())
