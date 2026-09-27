import re, html, glob, json, sys
from html.parser import HTMLParser

def clean(s):
    s = html.unescape(re.sub(r'<[^>]+>', ' ', s))
    s = s.replace(' ', ' ').replace(' ',' ')
    return re.sub(r'\s+', ' ', s).strip()

# ---- Kruse: <p class="calibre37"> N <br/> lines... –Author
kruse = []
files = sorted(glob.glob('a/text/part0000_split_*.html'))
for f in files:
    t = open(f, encoding='utf-8').read()
    for p in re.findall(r'<p class="calibre\d+">(.*?)</p>', t, re.S):
        if '–' not in p: continue
        parts = [clean(x) for x in re.split(r'<br[^>]*/>', p)]
        parts = [x for x in parts if x]
        if not parts: continue
        num = None
        if re.fullmatch(r'\d+', parts[0]): num = int(parts[0]); parts = parts[1:]
        joined = ' '.join(parts)
        m = re.match(r'(.*?)\s*–\s*(.+)$', joined)
        if not m: continue
        q, a = m.group(1).strip(), m.group(2).strip()
        if q: kruse.append({'n': num, 'q': q, 'a': a, 'src': 'kruse'})
# Kruse opens with an unnumbered "best" quote; keep numbered ones ordered + dedupe
seen=set(); k2=[]
for e in kruse:
    key=e['q'].lower()
    if key in seen: continue
    seen.add(key); k2.append(e)
kruse=k2

# ---- Taoism: H1 DATE/MONTH, then QTE lines + ATT attribution
months = ['JANUARY','FEBRUARY','MARCH','APRIL','MAY','JUNE','JULY','AUGUST','SEPTEMBER','OCTOBER','NOVEMBER','DECEMBER']
tao = []
for f in sorted(glob.glob('b/OEBPS/xhtml/*chapter*.xhtml')):
    t = open(f, encoding='utf-8').read()
    body = t[t.find('<body'):]
    for blk in re.split(r'(?=<p class="H1">)', body)[1:]:
        h = re.search(r'<span class="DATE">(\d+)</span>.*?<span class="line">(\w+)</span>', blk, re.S)
        if not h: continue
        day, mon = int(h.group(1)), months.index(h.group(2).upper()) + 1
        q = [clean(x) for x in re.findall(r'<p class="QTE[^"]*">(.*?)</p>', blk, re.S)]
        att = re.search(r'<p class="ATT[^"]*">(.*?)</p>', blk, re.S)
        if not q: continue
        a = clean(att.group(1)).rstrip(',') if att else 'Taoist proverb'
        a = a.title().replace('Te Ching,', 'Te Ching,')
        tao.append({'md': (mon, day), 'q': ' '.join(q), 'a': a, 'src': 'tao'})
json.dump({'kruse': kruse, 'tao': tao}, open('../quotes_raw.json','w'), ensure_ascii=False, indent=1)
print('kruse', len(kruse), 'tao', len(tao))
print('kruse nums', [e['n'] for e in kruse][:5], '...', [e['n'] for e in kruse][-3:])
import statistics
L=[len(e['q']) for e in kruse+tao]; print('len max', max(L), 'median', statistics.median(L))
print(sorted(set(e['a'] for e in tao))[:40])
