#!/usr/bin/env python3
"""Build sd-sample/tools/quotes.txt for the Daily Quote tool.

Input is a JSON file of extracted quotes ({"kruse": [...], "tao": [...]}).
Taoist quotes keep the calendar day their source book assigns them; every
other day takes the next motivational quote in book order. Output lines are
`YYYY-MM-DD|quote — Author`, one per day, UTF-8, sorted by date so the
firmware can binary-search or index the file by line offset.

usage: build_quotes.py quotes_raw.json out.txt [--start 2026-09-27] [--end 2027-12-31]
"""
import argparse, datetime as dt, json, re

ap = argparse.ArgumentParser()
ap.add_argument("raw"); ap.add_argument("out")
ap.add_argument("--start", default="2026-09-27"); ap.add_argument("--end", default="2027-12-31")
args = ap.parse_args()
d = json.load(open(args.raw, encoding="utf-8"))

def tidy(s):
    s = s.replace("|", "/").replace("’", "'").replace("‘", "'")
    s = s.replace("“", '"').replace("”", '"').replace("…", "...")
    return re.sub(r"\s+", " ", s).strip(" -–—")

kruse = []
for e in d["kruse"]:
    if e["n"] is None and not e["a"].startswith("Steve Jobs"):
        continue  # front-matter noise, not a quote
    kruse.append((e["n"] or 1, tidy(e["q"]), tidy(e["a"])))
kruse.sort()
tao = {}
for e in d["tao"]:
    tao.setdefault(tuple(e["md"]), (tidy(e["q"]), tidy(e["a"])))

start = dt.date.fromisoformat(args.start); end = dt.date.fromisoformat(args.end)
used_tao = set(); ki = 0; lines = []
day = start
while day <= end:
    key = (day.month, day.day)
    if key in tao and key not in used_tao:
        used_tao.add(key); q, a = tao[key]
    else:
        _, q, a = kruse[ki % len(kruse)]; ki += 1
    lines.append(f"{day.isoformat()}|{q} — {a}")
    day += dt.timedelta(days=1)
open(args.out, "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")
print(f"{len(lines)} days, {len(kruse)} motivational + {len(tao)} taoist quotes, max line {max(map(len, lines))} chars")
