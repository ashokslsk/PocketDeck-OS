#!/usr/bin/env python3
"""Small EPUB 3 writer for sample books, plus the Kannada sample.

    python3 scripts/samples/make_epub.py            # writes the sample books below to sd-sample/books

Surya_Namaskara_Kannada.epub and four short language test books in test/:
Kannada, Japanese, Russian and Spanish (original text written for testing).

The Kannada sample is built from scripts/workout/surya_namaskara_mantras.json
(Kannada verses, transliteration and meanings), with language "kn" so the
reader shapes it with the SD card Kannada font.
"""
import html
import json
import os
import uuid
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def write_epub(path, title, author, language, chapters):
    """chapters: list of (title, body_html)."""
    book_id = "urn:uuid:" + str(uuid.uuid5(uuid.NAMESPACE_URL, title))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with zipfile.ZipFile(path, "w") as z:
        z.writestr(zipfile.ZipInfo("mimetype"), "application/epub+zip", compress_type=zipfile.ZIP_STORED)
        z.writestr("META-INF/container.xml",
                   '<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">'
                   '<rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>'
                   '</rootfiles></container>', compress_type=zipfile.ZIP_DEFLATED)
        items, spine, nav = [], [], []
        for i, (ctitle, body) in enumerate(chapters, 1):
            name = f"ch{i:02d}.xhtml"
            items.append(f'<item id="c{i}" href="{name}" media-type="application/xhtml+xml"/>')
            spine.append(f'<itemref idref="c{i}"/>')
            nav.append(f'<li><a href="{name}">{html.escape(ctitle)}</a></li>')
            z.writestr("OEBPS/" + name,
                       f'<?xml version="1.0" encoding="utf-8"?><!DOCTYPE html><html xmlns="http://www.w3.org/1999/xhtml" '
                       f'xml:lang="{language}" lang="{language}"><head><title>{html.escape(ctitle)}</title></head>'
                       f'<body><h2>{html.escape(ctitle)}</h2>{body}</body></html>', compress_type=zipfile.ZIP_DEFLATED)
        z.writestr("OEBPS/nav.xhtml",
                   '<?xml version="1.0" encoding="utf-8"?><!DOCTYPE html><html xmlns="http://www.w3.org/1999/xhtml" '
                   'xmlns:epub="http://www.idpf.org/2007/ops"><head><title>Contents</title></head><body>'
                   f'<nav epub:type="toc"><ol>{"".join(nav)}</ol></nav></body></html>', compress_type=zipfile.ZIP_DEFLATED)
        z.writestr("OEBPS/content.opf",
                   '<?xml version="1.0" encoding="utf-8"?><package xmlns="http://www.idpf.org/2007/opf" version="3.0" '
                   'unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/">'
                   f'<dc:identifier id="id">{book_id}</dc:identifier><dc:title>{html.escape(title)}</dc:title>'
                   f'<dc:creator>{html.escape(author)}</dc:creator><dc:language>{language}</dc:language>'
                   '<meta property="dcterms:modified">2026-10-04T00:00:00Z</meta></metadata><manifest>'
                   '<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>'
                   f'{"".join(items)}</manifest><spine>{"".join(spine)}</spine></package>',
                   compress_type=zipfile.ZIP_DEFLATED)


def p(text, cls=None):
    return f"<p>{html.escape(text)}</p>" if text else ""


def verse(v, kn="kannada", en="english", meaning="meaning_english"):
    out = p(v.get(kn, ""))
    if v.get(en):
        out += f"<p><i>{html.escape(v[en])}</i></p>"
    if v.get(meaning):
        out += p(v[meaning])
    return out


def kannada_sample():
    src = json.load(open(os.path.join(ROOT, "scripts", "workout", "surya_namaskara_mantras.json"), encoding="utf-8"))
    sn = src["surya_namaskara"]
    chapters = [("ಧ್ಯಾನ ಶ್ಲೋಕ (Dhyana shloka)", verse(sn["opening_verse"]))]
    steps = ""
    for s in sn["steps"]:
        steps += (f"<h3>{s['step']}. {html.escape(s['posture_kannada'])} — {html.escape(s['posture_english'])}</h3>"
                  f"<p><b>{html.escape(s['mantra_kannada'])}</b></p><p><i>{html.escape(s['mantra_english'])}</i></p>"
                  f"{p(s['meaning_english'])}{p('Breath: ' + s['breath'] + '. ' + s['movement_english'] + '.')}")
    chapters.append(("ಸೂರ್ಯ ನಮಸ್ಕಾರ ಮಂತ್ರಗಳು (The twelve mantras)", steps))
    chapters.append(("ಸಮಾಪ್ತಿ ಶ್ಲೋಕ (Closing verse)", verse(sn["closing_verse"])))
    for key in ("bhaskara_stotram", "surya_ashtakam", "guru_namaskara"):
        sec = src[key]
        body = "".join(f"<h3>{v['verse_number']}</h3>" + verse(v) for v in sec["verses"])
        if "phalashruti" in sec:
            body += "<h3>ಫಲಶ್ರುತಿ (Phalashruti)</h3>" + verse(sec["phalashruti"])
        chapters.append((f"{sec['title_kannada']} ({sec['title_english']})", body))
    dd = src["dheerga_danda_namaskara"]
    chapters.append((f"{dd['title_kannada']}",
                     p(dd["description_kannada"]) + p(dd["description_english"]) +
                     f"<p><b>{html.escape(dd['mantra_kannada'])}</b></p>" + verse(dd, "kannada_shloka", "english_shloka")))
    out = os.path.join(ROOT, "sd-sample", "books", "Surya_Namaskara_Kannada.epub")
    write_epub(out, "ಸೂರ್ಯ ನಮಸ್ಕಾರ ಮಂತ್ರಗಳು", "PocketDeck-OS sample", "kn", chapters)
    print("wrote", out)


KN_TEST = [
    ("ಮೊದಲ ಅಧ್ಯಾಯ: ಬೆಳಗು", [
        "ಬೆಳಗಿನ ಜಾವ ಊರಿನ ಕೆರೆಯ ಮೇಲೆ ಮಂಜು ಹರಡಿತ್ತು. ಹಕ್ಕಿಗಳ ಹಾಡು ಕೇಳಿ ಪುಟ್ಟ ಹುಡುಗಿ ಕಿಟಕಿಯಿಂದ ಹೊರಗೆ ನೋಡಿದಳು.",
        "ಅಜ್ಜಿ ಅಂಗಳದಲ್ಲಿ ರಂಗೋಲಿ ಬಿಡಿಸುತ್ತಿದ್ದರು. \"ಇಂದು ಸಂಕ್ರಾಂತಿ, ಎಳ್ಳು ಬೆಲ್ಲ ಹಂಚೋಣ\" ಎಂದು ನಗುತ್ತಾ ಹೇಳಿದರು.",
        "ಶಾಲೆಗೆ ಹೊರಡುವ ಮುನ್ನ ಅವಳು ತನ್ನ ಪುಸ್ತಕದಲ್ಲಿ ಮೂರು ಹೊಸ ಪದಗಳನ್ನು ಬರೆದಳು: ಸ್ನೇಹ, ಪ್ರಕೃತಿ, ಕೃತಜ್ಞತೆ.",
        "This line mixes English with ಕನ್ನಡ words, numbers like 2026, and punctuation (commas, quotes) to test both fonts.",
    ]),
    ("ಎರಡನೆಯ ಅಧ್ಯಾಯ: ಸಂಜೆ", [
        "ಸಂಜೆ ಸೂರ್ಯ ಮುಳುಗುವಾಗ ಆಕಾಶ ಕಿತ್ತಳೆ ಬಣ್ಣಕ್ಕೆ ತಿರುಗಿತು. ದೇವಸ್ಥಾನದ ಗಂಟೆ ಮೆಲ್ಲನೆ ಮೊಳಗಿತು.",
        "ತಾತ ಕಥೆ ಹೇಳಲು ಕುಳಿತರು: ಒಂದು ಕಾಲದಲ್ಲಿ ಕಾಡಿನ ಅಂಚಿನಲ್ಲಿ ಒಬ್ಬ ಬುದ್ಧಿವಂತ ಆನೆ ವಾಸವಾಗಿತ್ತು.",
        "ಮಕ್ಕಳು ಕಣ್ಣರಳಿಸಿ ಕೇಳುತ್ತಿದ್ದರು. ಕಥೆ ಮುಗಿದಾಗ ಎಲ್ಲರೂ ಚಪ್ಪಾಳೆ ತಟ್ಟಿದರು.",
        "ಒತ್ತಕ್ಷರಗಳು: ಕ್ಷೇತ್ರ, ಶ್ರೀ, ಸ್ತ್ರೀ, ಅರ್ಥ, ಕರ್ನಾಟಕ, ವಿದ್ಯಾರ್ಥಿ, ಸ್ವಾತಂತ್ರ್ಯ.",
    ]),
]
JA_TEST = [
    ("第一章　朝", [
        "朝早く、町の小さな駅に一番電車が着いた。ホームには、まだ眠そうな人々が静かに並んでいた。",
        "「おはようございます」と駅員さんが笑顔で言うと、少女は小さく頭を下げた。",
        "窓の外では、桜の花びらが風に舞い、川面にゆっくりと落ちていった。",
        "彼女はノートを開き、今日やりたいことを三つ書いた。読書、散歩、そして友だちへの手紙。",
    ]),
    ("第二章　夜", [
        "夜になると、空には数えきれないほどの星が輝いた。遠くで犬が一度だけ吠えた。",
        "祖母は温かいお茶を入れながら、昔の話をしてくれた。カタカナの言葉も、ひらがなの言葉も、やさしく響いた。",
        "This sentence mixes English with 日本語 to test the Latin letters in the CJK font.",
    ]),
]
RU_TEST = [
    ("Глава первая. Утро", [
        "Солнце медленно поднималось над горами. Кто мог бы забыть то утро? «Завтра будет новый день», — сказала девочка и улыбнулась.",
        "Он шёл по берегу реки; холодная и чистая вода отражала деревья. Год за годом маленькая деревня хранила свои песни.",
        "Ёжик, съешь же ещё этих мягких французских булок да выпей чаю.",
    ]),
    ("Глава вторая. Вечер", [
        "Вечером в окнах зажглись огни. Бабушка рассказывала сказку о храбром зайце и мудрой сове.",
        "Дети слушали, затаив дыхание, а за окном тихо падал первый снег.",
    ]),
]
ES_TEST = [
    ("Capítulo uno. La mañana", [
        "El sol salía despacio sobre las montañas. ¿Quién podría olvidar aquella mañana? «Mañana será otro día», dijo la niña, y sonrió.",
        "Él caminó por la orilla del río; el agua fría y clara reflejaba los árboles. Año tras año, la pequeña aldea guardaba sus canciones.",
        "¡Qué alegría! El pingüino Wenceslao hizo kilómetros bajo exhaustiva lluvia y frío, añoraba a su querido cachorro.",
    ]),
    ("Capítulo dos. La noche", [
        "Por la noche, la abuela contó una historia sobre un zorro valiente y una lechuza sabia.",
        "Los niños escuchaban en silencio mientras, afuera, caía la primera nieve del invierno.",
    ]),
]


def test_books():
    out = os.path.join(ROOT, "sd-sample", "books", "test")
    for name, title, author, lang, chapters in [
            ("Kannada_test.epub", "ಕನ್ನಡ ಪರೀಕ್ಷಾ ಪುಸ್ತಕ", "PocketDeck-OS", "kn", KN_TEST),
            ("Japanese_test.epub", "日本語テスト", "PocketDeck-OS", "ja", JA_TEST),
            ("Russian_test.epub", "Проверка на русском", "PocketDeck-OS", "ru", RU_TEST),
            ("Spanish_test.epub", "Prueba en español", "PocketDeck-OS", "es", ES_TEST)]:
        body = [(t, "".join(p(x) for x in paras) * 3) for t, paras in chapters]
        write_epub(os.path.join(out, name), title, author, lang, body)
        print("wrote", name)


if __name__ == "__main__":
    kannada_sample()
    test_books()
