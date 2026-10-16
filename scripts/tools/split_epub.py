#!/usr/bin/env python3
"""Split oversized EPUB chapters into smaller files for the reader.

Some EPUBs put a whole book in one XHTML file (Malegalalli Madumagalu: 3.3 MB,
2,000+ pages in a single "chapter"). The X3 then lays out one enormous
chapter. This script rewrites such files into parts:

  * it splits at headings (h1-h3), scene breaks ("* * *", "***", "⁂", "~ ~ ~")
    and <hr>, once a part has at least --min-kb of text;
  * a part that reaches --max-kb without a natural break is split at the next
    paragraph;
  * the table of contents (toc.ncx and/or nav.xhtml) gets one entry per part
    (the part's first heading, or "Part N: first words"), the OPF manifest and
    spine list the parts in order, and links to ids inside the old file are
    pointed at the part that now holds them.

Nothing else in the book changes. Usage:

  python3 scripts/tools/split_epub.py book.epub                 # writes book (split).epub
  python3 scripts/tools/split_epub.py book.epub -o out.epub --max-kb 120
"""
import argparse
import html
import os
import posixpath
import re
import sys
import zipfile

BLOCK_TAGS = {"p", "div", "h1", "h2", "h3", "h4", "h5", "h6", "hr", "blockquote", "ul", "ol", "table", "pre",
              "section", "article", "figure", "dl", "center", "img", "br"}
VOID_TAGS = {"hr", "br", "img", "meta", "link", "input", "area", "base", "col", "embed", "source", "wbr"}
SCENE_BREAK = re.compile(r"^\s*(?:[*•·⁂~#\-–—]\s*){1,7}$")
TAG = re.compile(r"<(/?)([a-zA-Z][a-zA-Z0-9:]*)([^>]*?)(/?)>", re.S)


def text_of(fragment):
    return html.unescape(re.sub(r"<[^>]+>", " ", fragment)).strip()


def top_level_blocks(body):
    """Splits body inner HTML into top-level pieces (offset ranges)."""
    blocks, depth, start, pos = [], 0, 0, 0
    for m in TAG.finditer(body):
        closing, name, _attrs, selfclose = m.group(1), m.group(2).lower(), m.group(3), m.group(4)
        if name in VOID_TAGS or selfclose:
            if depth == 0:
                blocks.append((start, m.end()))
                start = m.end()
            continue
        if closing:
            depth = max(0, depth - 1)
            if depth == 0:
                blocks.append((start, m.end()))
                start = m.end()
        else:
            depth += 1
    if body[start:].strip():
        blocks.append((start, len(body)))
    return [body[a:b] for a, b in blocks]


def first_tag(block):
    m = re.search(r"<([a-zA-Z][a-zA-Z0-9]*)", block)
    return m.group(1).lower() if m else ""


def is_break(block):
    tag = first_tag(block)
    if tag in ("h1", "h2", "h3"):
        return "heading"
    if tag == "hr":
        return "rule"
    t = text_of(block)
    if t and len(t) <= 12 and SCENE_BREAK.match(t):
        return "scene"
    return ""


def split_body(body, min_bytes, max_bytes):
    parts, cur, size = [], [], 0
    for block in top_level_blocks(body):
        kind = is_break(block)
        b = len(block.encode("utf-8"))
        if cur and ((kind and size >= min_bytes) or size + b > max_bytes):
            if kind == "scene" or kind == "rule":
                # The break marker ends the part it closes.
                cur.append(block)
                parts.append(cur)
                cur, size = [], 0
                continue
            parts.append(cur)
            cur, size = [], 0
        cur.append(block)
        size += b
    if cur:
        if parts and len("".join(cur).encode("utf-8")) < min_bytes // 4:
            parts[-1].extend(cur)  # don't leave a tiny tail part
        else:
            parts.append(cur)
    return ["".join(p) for p in parts]


def part_title(fragment, n):
    m = re.search(r"<h[1-3][^>]*>(.*?)</h[1-3]>", fragment, re.S)
    if m and text_of(m.group(1)):
        return text_of(m.group(1))[:80]
    for p in re.findall(r"<p[^>]*>(.*?)</p>", fragment, re.S):
        t = text_of(p)
        if len(t) > 12 and not SCENE_BREAK.match(t):
            words = t.split()
            return f"Part {n}: " + " ".join(words[:5]) + ("…" if len(words) > 5 else "")
    return f"Part {n}"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("epub")
    ap.add_argument("-o", "--output")
    ap.add_argument("--max-kb", type=int, default=150, help="split files larger than this; part size cap (default 150)")
    ap.add_argument("--min-kb", type=int, default=40, help="smallest part before a natural break is used (default 40)")
    args = ap.parse_args()
    out_path = args.output or os.path.splitext(args.epub)[0] + " (split).epub"
    max_bytes, min_bytes = args.max_kb * 1024, args.min_kb * 1024

    zin = zipfile.ZipFile(args.epub)
    files = {i.filename: zin.read(i.filename) for i in zin.infolist() if not i.is_dir()}
    container = files["META-INF/container.xml"].decode("utf-8")
    opf_path = re.search(r'full-path="([^"]+)"', container).group(1)
    opf_dir = posixpath.dirname(opf_path)
    opf = files[opf_path].decode("utf-8")

    def full(href):
        return posixpath.normpath(posixpath.join(opf_dir, href)) if opf_dir else posixpath.normpath(href)

    items = {}  # id -> (href, media-type, whole tag)
    for m in re.finditer(r"<item\b[^>]*/?>", opf):
        tag = m.group(0)
        iid = re.search(r'\bid="([^"]+)"', tag)
        href = re.search(r'\bhref="([^"]+)"', tag)
        if iid and href:
            items[iid.group(1)] = (html.unescape(href.group(1)), tag)
    spine_ids = re.findall(r'<itemref\b[^>]*\bidref="([^"]+)"', opf)

    splits = {}  # full path of old file -> list of (new full path, new href, title, ids)
    for sid in spine_ids:
        href, tag = items.get(sid, (None, None))
        if href is None:
            continue
        path = full(href)
        data = files.get(path)
        if data is None or len(data) <= max_bytes:
            continue
        doc = data.decode("utf-8", "ignore")
        bm = re.search(r"(<body\b[^>]*>)(.*)(</body>)", doc, re.S | re.I)
        if not bm:
            continue
        pieces = split_body(bm.group(2), min_bytes, max_bytes)
        if len(pieces) < 2:
            continue
        head, tail = doc[:bm.start(2)], doc[bm.end(2):]
        stem, ext = posixpath.splitext(href)
        parts = []
        for n, piece in enumerate(pieces, 1):
            new_href = f"{stem}_part{n:03d}{ext}"
            ids = set(re.findall(r'\bid="([^"]+)"', piece))
            files[full(new_href)] = (head + piece + tail).encode("utf-8")
            parts.append((full(new_href), new_href, part_title(piece, n), ids, sid))
        del files[path]
        splits[path] = parts
        print(f"{href}: {len(data) // 1024} KB -> {len(parts)} parts")

    if not splits:
        print("Nothing to split: every chapter is under", args.max_kb, "KB")
        return 0

    # OPF: replace each split item with its parts in the manifest and spine.
    for path, parts in splits.items():
        sid = parts[0][4]
        _href, tag = items[sid]
        new_items = []
        for n, (_full, new_href, _title, _ids, _sid) in enumerate(parts, 1):
            t = re.sub(r'\bid="[^"]+"', f'id="{sid}_p{n:03d}"', tag)
            t = re.sub(r'\bhref="[^"]+"', f'href="{html.escape(new_href, quote=True)}"', t)
            new_items.append(t)
        opf = opf.replace(tag, "\n    ".join(new_items), 1)
        ref = re.search(rf'<itemref\b[^>]*\bidref="{re.escape(sid)}"[^>]*/?>', opf).group(0)
        opf = opf.replace(ref, "\n    ".join(ref.replace(f'idref="{sid}"', f'idref="{sid}_p{n:03d}"')
                                           for n in range(1, len(parts) + 1)), 1)
    files[opf_path] = opf.encode("utf-8")

    # Links: old.xhtml#id -> the part holding id; old.xhtml -> part 1.
    def retarget(text, base_dir):
        def fix(m):
            attr, quote, target = m.group(1), m.group(2), m.group(3)
            path_part, _, frag = target.partition("#")
            if not path_part or re.match(r"^[a-z]+:", path_part):
                return m.group(0)
            resolved = posixpath.normpath(posixpath.join(base_dir, html.unescape(path_part)))
            if resolved not in splits:
                return m.group(0)
            parts = splits[resolved]
            dest = parts[0]
            if frag:
                dest = next((p for p in parts if frag in p[3]), parts[0])
            rel = posixpath.relpath(dest[0], base_dir or ".")
            return f'{attr}={quote}{html.escape(rel, quote=True)}{"#" + frag if frag else ""}{quote}'
        return re.sub(r'\b(href|src)=(["\'])([^"\']+)\2', fix, text)

    for name in list(files):
        if name.endswith((".xhtml", ".html", ".htm", ".ncx")):
            base = posixpath.dirname(name)
            text = files[name].decode("utf-8", "ignore")
            new = retarget(text, base)
            files[name] = new.encode("utf-8")

    # TOC: one entry per part, after the entry that pointed at the old file.
    for name in list(files):
        base = posixpath.dirname(name)
        if name.endswith(".ncx"):
            ncx = files[name].decode("utf-8")
            for path, parts in splits.items():
                first_rel = posixpath.relpath(parts[0][0], base or ".")
                m = re.search(r'(<navPoint\b[^>]*>\s*<navLabel>\s*<text>)(.*?)(</text>\s*</navLabel>\s*<content\s+src="'
                              + re.escape(first_rel) + r'(?:#[^"]*)?"\s*/>\s*</navPoint>)', ncx, re.S)
                extra = "".join(
                    f'<navPoint id="split_{i}_{n}" playOrder="0"><navLabel><text>{html.escape(t)}</text></navLabel>'
                    f'<content src="{html.escape(posixpath.relpath(f, base or "."), quote=True)}"/></navPoint>'
                    for i, (f, _h, t, _ids, _s) in enumerate(parts[1:], 2) for n in [len(parts)])
                if m:
                    ncx = ncx[:m.end()] + extra + ncx[m.end():]
                else:
                    ncx = ncx.replace("</navMap>", extra + "</navMap>", 1)
            order = iter(range(1, 100000))
            ncx = re.sub(r'playOrder="\d+"', lambda _m: f'playOrder="{next(order)}"', ncx)
            files[name] = ncx.encode("utf-8")
        elif name.endswith((".xhtml", ".html")) and b'epub:type="toc"' in files[name]:
            nav = files[name].decode("utf-8")
            for path, parts in splits.items():
                first_rel = posixpath.relpath(parts[0][0], base or ".")
                m = re.search(r'<li>\s*<a\s+href="' + re.escape(first_rel) + r'(?:#[^"]*)?"[^>]*>.*?</a>\s*</li>', nav,
                              re.S)
                extra = "".join(f'<li><a href="{html.escape(posixpath.relpath(f, base or "."), quote=True)}">'
                                f'{html.escape(t)}</a></li>' for f, _h, t, _ids, _s in parts[1:])
                if m:
                    nav = nav[:m.end()] + extra + nav[m.end():]
            files[name] = nav.encode("utf-8")

    with zipfile.ZipFile(out_path, "w") as zout:
        zout.writestr(zipfile.ZipInfo("mimetype"), files.pop("mimetype", b"application/epub+zip"),
                      compress_type=zipfile.ZIP_STORED)
        for name, data in files.items():
            zout.writestr(name, data, compress_type=zipfile.ZIP_DEFLATED)
    print("wrote", out_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
