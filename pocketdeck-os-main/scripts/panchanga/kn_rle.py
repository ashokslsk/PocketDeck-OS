"""Nibble run-length codec shared by gen_kannada_bitmaps.py (and its tests).

A string's pixels are walked row-major. Runs alternate blank, ink, blank, ...
starting with blank. Each run is a chain of 4-bit nibbles (high nibble
first): 15 means "15 more pixels, run continues", 0..14 ends the run. Every
string starts on a byte boundary.
"""


def encode(pixels):
    """pixels: iterable of bools (True = ink) in row-major order -> bytes."""
    nibbles = []
    ink = False
    run = 0

    def flush(n):
        while n >= 15:
            nibbles.append(15)
            n -= 15
        nibbles.append(n)

    for p in pixels:
        if p != ink:
            flush(run)
            ink = p
            run = 0
        run += 1
    flush(run)
    if len(nibbles) % 2:
        nibbles.append(0)
    return bytes((nibbles[i] << 4) | nibbles[i + 1] for i in range(0, len(nibbles), 2))


def decode(data, count):
    """Inverse of encode for `count` pixels."""
    out = []
    ink = False
    run = 0
    for byte in data:
        for nib in (byte >> 4, byte & 15):
            run += nib
            if nib == 15:
                continue
            out.extend([ink] * run)
            ink = not ink
            run = 0
            if len(out) >= count:
                return out[:count]
    return out[:count]
