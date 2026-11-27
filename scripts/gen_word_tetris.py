#!/usr/bin/env python3
"""Generates src/activities/tools/WordTetrisPuzzles.h for Tools > Word Tetris.

Each puzzle is a 4x4 board whose four rows read four common English words.
The board is cut into four tetrominoes (pieces never rotate). A puzzle is kept
only when it has exactly one solution: no other placement of its pieces makes
every row a word from WORDS. Pieces are numbered in random order so the tray
order gives nothing away.

Usage: python3 scripts/gen_word_tetris.py [--count 240] [--seed 7]
"""

import argparse
import itertools
import random
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "src/activities/tools/WordTetrisPuzzles.h"

# Common, family-friendly four-letter words. Every word here is accepted as a
# row on the device, and every puzzle row comes from this list.
WORDS = """
able acid aged also area army away baby back bake ball band bank barn base bath bead beam bean bear beat
bell belt bend best bike bird bite blue boat body bold bone book boot born boss both bowl bulb bump burn
bush busy cake calm came camp card care cart case cash cast cave chat chef chin chip city clap clay clip
coal coat code coin cold come cook cool copy corn cost cozy crab crew crop crow cube cups cure curl cute
dark dash data date dawn days deal dear deck deep deer desk dial dice diet dirt dish dive dock does doll
door dove down draw drop drum duck dust duty each earn ears east easy echo edge else epic even ever exit
face fact fair fall farm fast fate fear feed feel feet fell felt fern file fill film find fine fire firm
fish fist five flag flat flew flip flow foam fold folk fond food foot fork form fort four free frog from
fuel full fund gain game gate gave gear gift girl give glad glow glue goal goat gold golf gone good gown
grab gray grew grid grin grip grow gulf hail hair half hall hand hang hard harp hate have hawk head heal
heap hear heat held helm help herb here hero hide high hike hill hint hive hold hole home hood hook hope
horn hose host hour huge hunt idea inch into iron item jade jazz join joke jump just keen keep kept kick
kind king kite knee knit knot know lace lady lake lamb lamp land lane last late lawn lazy lead leaf lean
left lend lens less life lift like lime line link lion list live load loaf loan lock loft long look loop
lord lose lost loud love luck lump lung made mail main make mall many maps mark mask mast math meal mean
meat meet melt menu mild milk mill mind mine mint miss mist mode mole mood moon more moss most moth move
much mule must name navy near neat neck need nest news next nice nine node none noon nose note oaks oath
odds once only open oven over pace pack page paid pail pain pair palm park part pass past path peak pear
peas pick pier pile pine pink pipe plan play plot plug plum poem poet pole pond pony pool poor port pose
post pour pray pull pump pure push race rack rain rake ramp rang rare rate read real rest rice rich ride
ring rise risk road roam rock role roll roof room root rope rose ruby rule rush rust safe sage said sail
salt same sand save seal seat seed seek seem seen self sell send ship shoe shop shot show shut sick side
sign silk sing sink size skip slip slow snow soap sock soft soil sold sole some song soon sort soup sour
spin spot star stay stem step stew stir stop such suit sure swan swim tail take tale talk tall tame tank
tape task team tear tell tend tent term test text than that them then they thin this tide tidy tile time
tiny tire toad told tone tool toys tree trim trip true tube tune turn twin type unit upon used user vase
vast very view vine vote wade wage wait wake walk wall want warm wash wave weak wear week well went were
west what when whom wide wife wild will wind wine wing wire wise wish with wolf wood wool word wore work
worm wrap yard yarn year yell your zero zone zoom
""".split()

N = 4


def tetromino_tilings():
    """Every way to cut the 4x4 board into four connected 4-cell pieces."""
    tilings = []
    grid = [[-1] * N for _ in range(N)]

    def grow(cells, piece_id):
        # All connected 4-cell sets that include the first free cell, grown so
        # that the first free cell is the piece's top-left-most cell.
        results = set()

        def extend(shape):
            if len(shape) == 4:
                results.add(frozenset(shape))
                return
            for r, c in list(shape):
                for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nr, nc = r + dr, c + dc
                    if 0 <= nr < N and 0 <= nc < N and grid[nr][nc] == -1 and (nr, nc) not in shape:
                        if (nr, nc) < cells:  # keep `cells` the smallest cell
                            continue
                        extend(shape | {(nr, nc)})

        extend(frozenset([cells]))
        return results

    def place(piece_id):
        free = [(r, c) for r in range(N) for c in range(N) if grid[r][c] == -1]
        if not free:
            tilings.append([row[:] for row in grid])
            return
        for shape in grow(free[0], piece_id):
            for r, c in shape:
                grid[r][c] = piece_id
            place(piece_id + 1)
            for r, c in shape:
                grid[r][c] = -1

    place(0)
    return tilings


def is_interesting(tiling):
    # A flat 1x4 piece hands the player a whole row; leave those out.
    for p in range(4):
        rows = {r for r in range(N) for c in range(N) if tiling[r][c] == p}
        if len(rows) == 1:
            return False
    return True


def pieces_of(tiling, letters):
    """Pieces as (cells relative to their bounding box, letters)."""
    out = []
    for p in range(4):
        cells = [(r, c) for r in range(N) for c in range(N) if tiling[r][c] == p]
        r0 = min(r for r, _ in cells)
        c0 = min(c for _, c in cells)
        out.append([((r - r0, c - c0), letters[r][c]) for r, c in cells])
    return out


def count_solutions(pieces, words, limit=2):
    """Distinct boards (as letters) where every row is a word."""
    word_set = set(words)
    prefixes = {w[:k] for w in words for k in range(1, 5)}
    found = set()
    board = [[None] * N for _ in range(N)]

    def rows_ok():
        for r in range(N):
            row = board[r]
            prefix = ""
            for ch in row:
                if ch is None:
                    break
                prefix += ch
            if prefix and prefix not in prefixes:
                return False
        return True

    def solve(i):
        if len(found) >= limit:
            return
        if i == len(pieces):
            text = "".join("".join(row) for row in board)
            if all(text[r * 4:r * 4 + 4] in word_set for r in range(N)):
                found.add(text)
            return
        piece = pieces[i]
        h = max(r for (r, _), _ in piece) + 1
        w = max(c for (_, c), _ in piece) + 1
        for dr in range(N - h + 1):
            for dc in range(N - w + 1):
                if any(board[r + dr][c + dc] is not None for (r, c), _ in piece):
                    continue
                for (r, c), ch in piece:
                    board[r + dr][c + dc] = ch
                if rows_ok():
                    solve(i + 1)
                for (r, c), _ in piece:
                    board[r + dr][c + dc] = None

    solve(0)
    return len(found)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--count", type=int, default=240)
    ap.add_argument("--seed", type=int, default=7)
    args = ap.parse_args()

    words = sorted(set(WORDS))
    assert all(len(w) == 4 and w.isalpha() for w in words)
    tilings = [t for t in tetromino_tilings() if is_interesting(t)]
    rng = random.Random(args.seed)
    uses = {w: 0 for w in words}
    puzzles = []
    seen = set()
    attempts = 0
    while len(puzzles) < args.count:
        attempts += 1
        # Prefer words used least so far, so the set stays varied.
        pool = sorted(words, key=lambda w: (uses[w], rng.random()))[:120]
        rows = rng.sample(pool, 4)
        key = tuple(sorted(rows))
        if key in seen:
            continue
        tiling = [row[:] for row in rng.choice(tilings)]
        # Number the pieces in a random order (the tray order).
        perm = list(range(4))
        rng.shuffle(perm)
        tiling = [[perm[v] for v in row] for row in tiling]
        letters = [list(w) for w in rows]
        if count_solutions(pieces_of(tiling, letters), words) != 1:
            continue
        seen.add(key)
        for w in rows:
            uses[w] += 1
        puzzles.append((rows, tiling))

    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("// Generated by scripts/gen_word_tetris.py. Do not edit by hand.")
    lines.append(f"// {len(puzzles)} puzzles, seed {args.seed}; each has exactly one solution.")
    lines.append("")
    lines.append("#include <cstdint>")
    lines.append("")
    lines.append("namespace wordtetris {")
    lines.append("")
    lines.append("struct Puzzle {")
    lines.append("  char letters[16];  // solution, row by row, upper case")
    lines.append("  uint32_t pieces;   // 2 bits per cell (cell 0 lowest): which piece owns it")
    lines.append("};")
    lines.append("")
    lines.append(f"constexpr int kPuzzleCount = {len(puzzles)};")
    lines.append("")
    lines.append("static const Puzzle kPuzzles[kPuzzleCount] = {")
    for rows, tiling in puzzles:
        packed = 0
        for i in range(16):
            packed |= tiling[i // 4][i % 4] << (2 * i)
        text = "".join(rows).upper()
        chars = ", ".join(f"'{ch}'" for ch in text)
        lines.append(f"    {{{{{chars}}}, 0x{packed:08X}u}},  // {' '.join(rows)}")
    lines.append("};")
    lines.append("")
    lines.append("// Every word accepted as a row, sorted, four letters each, no separators.")
    lines.append(f"constexpr int kWordCount = {len(words)};")
    lines.append("static const char kWords[] =")
    upper = [w.upper() for w in words]
    for i in range(0, len(upper), 16):
        lines.append('    "' + "".join(upper[i:i + 16]) + '"')
    lines[-1] += ";"
    lines.append("")
    lines.append("}  // namespace wordtetris")
    lines.append("")
    OUT.write_text("\n".join(lines))
    print(f"wrote {OUT} ({len(puzzles)} puzzles, {len(words)} words, {len(tilings)} tilings, {attempts} attempts)")


if __name__ == "__main__":
    main()
