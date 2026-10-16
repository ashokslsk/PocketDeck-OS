#!/usr/bin/env python3
"""Extract attributed quotes from the two source EPUBs used for quotes.txt.

  extract_quotes.py --kruse "365 Best Inspirational Quotes.epub" \
                    --taoism "A Year of Taoism.epub" -o quotes_raw.json

Only attributed quotations are taken: Kruse's numbered quotes, and the
classical quotations (Tao Te Ching, Chuang Tzu, ...) that "A Year of Taoism"
places on specific calendar days. The Taoism author's own reflections and
meditation practices are deliberately skipped. The parsing matches these two
books' markup; other books need their own rules. Feed the output to
build_quotes.py.
"""
import argparse, html, json, re, zipfile

MONTHS = ["JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER", "OCTOBER",
          "NOVEMBER", "DECEMBER"]


def clean(s):
    s = html.unescape(re.sub(r"<[^>]+>", " ", s)).replace(" ", " ")
    return re.sub(r"\s+", " ", s).strip()


def kruse_quotes(path):
    out, seen = [], set()
    with zipfile.ZipFile(path) as z:
        for name in sorted(n for n in z.namelist() if re.search(r"text/part\d+_split_\d+\.html$", n)):
            text = z.read(name).decode("utf-8")
            for p in re.findall(r'<p class="calibre\d+">(.*?)</p>', text, re.S):
                if "–" not in p:
                    continue
                parts = [x for x in (clean(x) for x in re.split(r"<br[^>]*/>", p)) if x]
                if not parts:
                    continue
                num = int(parts[0]) if re.fullmatch(r"\d+", parts[0]) else None
                if num is not None:
                    parts = parts[1:]
                m = re.match(r"(.*?)\s*–\s*(.+)$", " ".join(parts))
                if not m or not m.group(1).strip():
                    continue
                q, a = m.group(1).strip(), m.group(2).strip()
                if num is None and not a.startswith("Steve Jobs"):
                    continue  # front matter; the unnumbered Steve Jobs quote is #1
                if q.lower() in seen:
                    continue
                seen.add(q.lower())
                out.append({"n": num or 1, "q": q, "a": a, "src": "kruse"})
    return out


def taoism_quotes(path):
    out = []
    with zipfile.ZipFile(path) as z:
        for name in sorted(n for n in z.namelist() if "chapter" in n and n.endswith(".xhtml")):
            body = z.read(name).decode("utf-8")
            body = body[body.find("<body"):]
            for blk in re.split(r'(?=<p class="H1">)', body)[1:]:
                h = re.search(r'<span class="DATE">(\d+)</span>.*?<span class="line">(\w+)</span>', blk, re.S)
                q = [clean(x) for x in re.findall(r'<p class="QTE[^"]*">(.*?)</p>', blk, re.S)]
                if not h or not q:
                    continue
                att = re.search(r'<p class="ATT[^"]*">(.*?)</p>', blk, re.S)
                a = clean(att.group(1)).rstrip(",").lstrip("—").strip().title() if att else "Taoist proverb"
                out.append({"md": [MONTHS.index(h.group(2).upper()) + 1, int(h.group(1))], "q": " ".join(q),
                            "a": a, "src": "tao"})
    return out


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--kruse", required=True)
    ap.add_argument("--taoism", required=True)
    ap.add_argument("-o", "--out", default="quotes_raw.json")
    args = ap.parse_args()
    data = {"kruse": kruse_quotes(args.kruse), "tao": taoism_quotes(args.taoism)}
    json.dump(data, open(args.out, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print(f"{len(data['kruse'])} motivational + {len(data['tao'])} taoist quotes -> {args.out}")
