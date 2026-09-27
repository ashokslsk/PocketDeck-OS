REPO = os.path.join(HERE, "pocketdeck-os-main")
LIB = os.path.join(HERE, "library")
SEED = "python3 '%s/seed.py' && python3 '%s/seed_extra.py' && python3 '%s/seed_trackers.py'" % (HERE, HERE, HERE)

def settings(**kv):
    import json
    kv.setdefault("clockUtcOffsetQ", 70)  # UTC+5:30
    return ("mkdir -p .pocketdeck-os && python3 -c \"import json,os;p='.pocketdeck-os/pocketdeck-os-settings.json';"
            "d=json.load(open(p)) if os.path.exists(p) else {};d.update(%s);json.dump(d,open(p,'w'))\"" % json.dumps(kv).replace('"', "'"))

BASE = ("mkdir -p books .dictionaries && cp '%s'/*.epub books/ && cp -R '%s/dict/WordNet' .dictionaries/ && "
        "mkdir -p .pocketdeck-os && printf '/.dictionaries/WordNet/WordNet' > .pocketdeck-os/dictionary.bin" % (LIB, LIB))
PREP = {"*": BASE + " && " + SEED + " && " + settings()}
THEMES = [("classic", 0), ("lyra", 1), ("lyra-extended", 2), ("roundedraff", 3), ("lyra-carousel", 4), ("minimal", 5),
          ("dashboard", 6), ("pocketdeck", 7)]
SLEEPS = [("light-default", 1), ("dark", 0), ("page-overlay", 6), ("book-cover", 3), ("reading-stats", 7),
          ("minimal", 8), ("minimal-stats", 10), ("dashboard", 11)]
T_POMODORO, T_WORLD, T_HABITS, T_MED, T_MOOD, T_FLASH, T_QUOTE, T_KNOW, T_PANCH, T_TODAY, T_STATS = range(11)

def tools(i):
    return [("UP", None, 700), ("ENTER", None, 1300)] + [("DOWN", None, 450)] * i + [("ENTER", None, 1500)]

def openbook(k, wait=9000, pages=6):
    return ([("DOWN", None, 600), ("ENTER", None, 1200), ("ENTER", None, 1200)] + [("DOWN", None, 500)] * k +
            [("ENTER", None, wait)] + [("RIGHT", None, 3000)] * pages + [("BACK", None, 3000)])

CHAPTER_ONE = ([("ENTER", None, 1500), ("DOWN", None, 500), ("DOWN", None, 500), ("DOWN", None, 500), ("ENTER", None, 2500)] +
               [("DOWN", None, 500)] * 3 + [("ENTER", None, 6000)])

SECTIONS = {
  "lib": [("WAIT", "01-home-before-reading", 0), ("ENTER", "02-browse-files", 1000), ("ENTER", "03-books-folder", 1200),
          ("ENTER", "04-book-cover-page", 9000), ("ENTER", "05-reader-menu", 1500),
          ("DOWN", None, 500), ("DOWN", None, 500), ("DOWN", None, 500), ("ENTER", "06-select-chapter", 2500),
          ("DOWN", None, 500), ("DOWN", None, 500), ("DOWN", None, 500), ("ENTER", "07-chapter-page", 6000),
          ("RIGHT", "08-next-page", 3200), ("LEFT", None, 3000),
          ("ENTER", None, 1500), ("DOWN", "09-look-up-word-menu", 700), ("ENTER", "10-word-select", 2500),
          ("ENTER", "11-dictionary-definition", 6000)],
  "lib2": [("WAIT", "01-home-continue-reading", 0)] + openbook(1) + openbook(3) + openbook(2, 20000) +
          [("WAIT", "02-home-with-library", 500)],
  "lib3": [("DOWN", None, 600), ("DOWN", None, 600), ("ENTER", "01-recent-books", 1500), ("BACK", None, 2000),
           ("DOWN", None, 600), ("ENTER", "02-reading-stats-this-book", 3000), ("RIGHT", "03-reading-stats-this-device", 1800),
           ("RIGHT", "04-reading-stats-library", 4000)],
  "bookmarks": [("ENTER", None, 6000)] + CHAPTER_ONE + [("WAIT", "01-chapter-page", 0),
           ("ENTER", None, 1500), ("ENTER", None, 1200), ("DOWN", None, 500), ("DOWN", "02-add-bookmark", 600),
           ("ENTER", "03-bookmarked-page", 2000),
           ("ENTER", None, 1500), ("ENTER", None, 1200), ("DOWN", "04-create-clipping", 600), ("ENTER", "05-clipping-start", 2500),
           ("ENTER", "06-clipping-first-word", 1200), ("DOWN", None, 500), ("DOWN", "07-clipping-range", 800),
           ("ENTER", "08-clipping-saved", 3000),
           ("ENTER", None, 1500), ("ENTER", None, 1200), ("DOWN", None, 500), ("WAIT", "09-menu-with-clippings", 400),
           ("DOWN", None, 500), ("ENTER", "10-clippings-list", 2500), ("BACK", None, 1500),
           ("ENTER", None, 1500), ("ENTER", None, 1200)] + [("DOWN", None, 500)] * 3 + [("WAIT", "11-view-bookmarks-row", 300),
           ("ENTER", "12-bookmarks-list", 2500), ("BACK", None, 1500), ("BACK", None, 1500), ("BACK", None, 3000),
           ("WAIT", "13-home-with-saved-items", 500)] + [("DOWN", None, 500)] * 4 + [("ENTER", "14-saved-items", 2500)],
  "boot": [("WAIT", "01-boot-splash", 1500)],
  "transfer": [("DOWN", None, 600)] * 2 + [("ENTER", "01-file-transfer", 1500)],
  "settings": [("WAIT", "01-display", 1200), ("ENTER", "02-reader", 1000), ("ENTER", "03-controls", 1000),
               ("ENTER", "04-system", 1000)] + [("DOWN", None, 450)] * 9 +
              [("WAIT", "05-system-about-row", 300), ("ENTER", "06-about-page-1", 1500), ("DOWN", "07-about-page-2", 1000),
               ("DOWN", "08-about-page-3", 1000)],
  "tools_menu": [("WAIT", "01-tools-menu", 1500)],
  "pomodoro": tools(T_POMODORO) + [("WAIT", "01-ready", 0), ("ENTER", "02-running", 3200), ("WAIT", "03-running-seconds", 3000),
                ("ENTER", "04-paused", 900), ("HOLD:ENTER:900", None, 900), ("WAIT", "05-reset", 300),
                ("RIGHT", "06-break-phase", 1000), ("LEFT", "07-pomodoro-stats", 2500)],
  "pomodoro_ring": tools(T_POMODORO) + [("ENTER", None, 30500), ("WAIT", "07-ring-quarter", 300),
                ("WAIT", None, 29700), ("WAIT", "08-ring-half", 300), ("WAIT", None, 29700), ("WAIT", "09-ring-three-quarters", 300)],
  "worldclock": tools(T_WORLD) + [("WAIT", "01-world-clock", 0), ("ENTER", "02-sync-wifi", 2500)],
  "worldclock_cities": tools(T_WORLD) + [("HOLD:ENTER:900", "03-choose-city-slot", 1200), ("ENTER", "04-city-catalogue", 1500),
             ("UP", None, 450), ("UP", None, 450), ("UP", "05-pick-mumbai", 600), ("ENTER", "06-mumbai-added", 1800)],
  "habits": tools(T_HABITS) + [("WAIT", "01-nine-habits", 0), ("ENTER", "02-ticked-today", 900)] +
            [("DOWN", None, 400)] * 6 + [("ENTER", "03-yoga-ticked", 900), ("DOWN", None, 400), ("DOWN", None, 400),
            ("ENTER", "04-coconut-water-ticked", 900), ("HOLD:ENTER:900", "05-habit-menu", 1200),
            ("ENTER", "06-coconut-water-stats", 3500), ("BACK", None, 1500), ("UP", None, 400), ("UP", None, 400),
            ("HOLD:ENTER:900", None, 1200), ("ENTER", "07-yoga-stats", 3500), ("BACK", None, 1500),
            ("HOLD:ENTER:900", None, 1200), ("DOWN", None, 450), ("ENTER", "08-add-habit-keyboard", 1500), ("BACK", None, 1200)] +
            [("LEFT", None, 400)] * 8 + [("WAIT", "09-previous-week", 500)],
  "medicine": tools(T_MED) + [("WAIT", "01-courses", 500), ("RIGHT", None, 400), ("RIGHT", None, 400),
            ("ENTER", "02-evening-dose-ticked", 1000),
            ("HOLD:ENTER:900", "03-course-menu", 1200), ("DOWN", None, 450), ("ENTER", "04-paracetamol-details", 2500),
            ("BACK", None, 1200), ("DOWN", None, 400), ("HOLD:ENTER:900", None, 1200), ("DOWN", None, 450),
            ("ENTER", "05-vitamin-d-details", 2500), ("BACK", None, 1200), ("DOWN", None, 400), ("HOLD:ENTER:900", None, 1200),
            ("DOWN", None, 450), ("ENTER", "06-completed-course", 2500), ("BACK", None, 1200), ("DOWN", None, 400),
            ("HOLD:ENTER:900", None, 1200), ("DOWN", None, 450), ("ENTER", "07-stopped-course", 2500), ("BACK", None, 1200),
            ("HOLD:ENTER:900", None, 1200), ("ENTER", "08-add-course-name", 1500), ("BACK", None, 1200)],
  "mood": tools(T_MOOD) + [("WAIT", "01-mood-today", 1500), ("RIGHT", "02-choose-great", 900), ("ENTER", "03-mood-saved", 1500),
            ("UP", "04-yesterday-with-note", 1500), ("HOLD:ENTER:900", "05-note-keyboard", 1500), ("BACK", None, 1200)],
  "stats": tools(T_STATS) + [("WAIT", "01-stats-and-export", 800), ("ENTER", "02-export-finished", 25000),
            ("HOLD:ENTER:900", "03-import-confirm", 1500), ("DOWN", None, 500), ("ENTER", "04-import-nothing-to-do", 12000)],
  "stats_import": tools(T_STATS) + [("HOLD:ENTER:900", None, 1500), ("DOWN", None, 500),
            ("ENTER", "01-import-restored", 15000), ("BACK", None, 1500), ("BACK", "02-home-after-import", 9000)],
  "panchanga_en": tools(T_PANCH) + [("WAIT", None, 1800), ("HOLD:ENTER:900", "01-language-picker", 1500), ("DOWN", None, 500),
            ("ENTER", "02-english-today", 2500), ("RIGHT", "03-english-next-day", 2200)] +
            [("DOWN", None, 700)] + [("RIGHT", None, 600)] * 11 + [("WAIT", "04-english-deepavali-2026", 2200)],
  "flashcards": tools(T_FLASH) + [("WAIT", "01-decks", 0), ("DOWN", None, 500), ("DOWN", None, 500), ("ENTER", "02-question", 1500),
             ("ENTER", "03-answer", 900), ("RIGHT", "04-next-question", 900), ("ENTER", "05-answer", 900),
             ("LEFT", "06-forgot-card-returns", 900), ("ENTER", None, 700), ("RIGHT", "07-session-complete", 1000),
             ("BACK", None, 900), ("DOWN", "08-deck-list-again", 600), ("ENTER", "09-sql-question", 1500),
             ("ENTER", "10-sql-answer", 900)],
  "quote": tools(T_QUOTE) + [("WAIT", "01-todays-quote", 800), ("RIGHT", "02-long-quote-page-1", 1200),
             ("DOWN", "03-long-quote-page-2", 1200), ("RIGHT", "04-next-day", 1000), ("RIGHT", "05-taoist-quote", 1000),
             ("ENTER", "06-shuffle", 1000)],
  "knowledge": tools(T_KNOW) + [("WAIT", "01-topics", 0), ("DOWN", None, 500), ("ENTER", "02-question-of-the-day", 1500)] +
             [("UP", None, 700)] * 3 + [("WAIT", "03-question", 300),
             ("RIGHT", "04-answer-page-1", 1200), ("RIGHT", "05-answer-page-2", 1000), ("RIGHT", "06-answer-page-3", 1000),
             ("DOWN", "07-next-question", 1000), ("RIGHT", "08-answer", 1000), ("BACK", "09-topics-again", 1000)],
  "panchanga": tools(T_PANCH) + [("WAIT", "01-today", 1800), ("RIGHT", "02-next-day", 2200)] +
             [("DOWN", None, 700)] + [("RIGHT", None, 600)] * 11 + [("WAIT", "03-deepavali-2026", 2200)] +
             [("DOWN", None, 700)] * 5 + [("RIGHT", None, 600)] * 11 + [("WAIT", "04-ugadi-2027", 2200),
             ("ENTER", "05-back-to-today", 2200)],
  "today": tools(T_TODAY) + [("WAIT", "01-today", 0), ("DOWN", None, 500), ("ENTER", "02-task-completed", 900),
             ("DOWN", None, 400), ("DOWN", None, 400), ("DOWN", None, 400), ("DOWN", "03-add-task-row", 700),
             ("ENTER", "04-new-task-keyboard", 1500)],
}
PREP["flashcards"] = PREP["*"] + " && cp '%s/quick_review.json' tools/flashcards/" % HERE
PREP["pomodoro_ring"] = PREP["*"] + " && python3 -c \"open('tools/pomodoro.txt','w').write('focus=2\\\\nbreak=1\\\\ndate=1970-01-01\\\\ncompleted=3\\\\n')\""
REUSE["lib2"] = "lib"; REUSE["lib3"] = "lib2"; REUSE["bookmarks"] = "lib"
PREP["lib2"] = ""; PREP["lib3"] = ""; PREP["bookmarks"] = ""
REUSE["stats"] = "lib3"; PREP["stats"] = ""
SECTIONS["probe_readermenu"] = [("ENTER", None, 7000), ("ENTER", None, 1500), ("ENTER", None, 900), ("ENTER", "00-gear", 1200)] + [("DOWN", "%02d-down" % i, 600) for i in range(1, 16)]
REUSE["probe_readermenu"] = "lib3"; PREP["probe_readermenu"] = ""
SECTIONS["reader_tilt"] = ([("ENTER", None, 7000), ("ENTER", None, 1500)] + [("DOWN", None, 450)] * 7 +
    [("ENTER", "01-book-options", 1500)] + [("DOWN", None, 400)] * 10 + [("WAIT", "02-tilt-row", 500),
    ("ENTER", "03-tilt-on", 1200)])
REUSE["reader_tilt"] = "lib3"; PREP["reader_tilt"] = ""
REUSE["carousel_nostats"] = "lib3"; PREP["carousel_nostats"] = settings(uiTheme=4, carouselBookStats=0)
SECTIONS["carousel_nostats"] = [("WAIT", "home", 9000)]
SECTIONS["carousel_browse"] = [("WAIT", None, 9000), ("RIGHT", "01-second-book", 2500), ("RIGHT", "02-third-book", 2500)]
REUSE["carousel_browse"] = "lib3"; PREP["carousel_browse"] = settings(uiTheme=4)
SECTIONS["sleep_settings"] = [("WAIT", None, 1200), ("DOWN", None, 500), ("ENTER", "01-sleep-settings", 1500)] + \
    [("DOWN", None, 450)] * 4 + [("WAIT", "02-change-wallpaper-row", 400), ("ENTER", "03-change-wallpaper-options", 1500),
    ("BACK", None, 1200), ("DOWN", None, 450), ("ENTER", "04-how-to-add-wallpapers", 1500), ("DOWN", "05-help-page-2", 1200)]
START["sleep_settings"] = "settings"
SECTIONS["display_settings"] = [("WAIT", None, 1200), ("DOWN", None, 500)] + [("DOWN", None, 450)] * 6 + \
    [("WAIT", "01-carousel-book-stats-row", 500)]
START["display_settings"] = "settings"
START["boot"] = "boot"; START["settings"] = "settings"; START["tools_menu"] = "tools"

for name, n in THEMES:
    for part in ("home", "tools", "settings"):
        key = f"theme_{name}_{part}"
        SECTIONS[key] = [("WAIT", part, 9000 if (name == "lyra-carousel" and part == "home") else 1500)]
        REUSE[key] = "lib3"
        PREP[key] = settings(uiTheme=n)
        if part != "home":
            START[key] = part

for name, n in SLEEPS:
    key = f"sleep_{name}"
    SECTIONS[key] = [("ENTER", None, 7000), ("SLEEP", "sleep", 1600)]
    REUSE[key] = "lib3"
    PREP[key] = settings(sleepScreen=n)
# Carousel shots start from the run that added a bookmark, a clipping and a
# dictionary look-up, so the stats panel has something to count.
REUSE["theme_lyra-carousel_home"] = "bookmarks"
REUSE["carousel_browse"] = "lib3"
REUSE["carousel_nostats"] = "bookmarks"

# Import test: a card with the /stats export but no reader data for the books.
REUSE["stats_import"] = "stats"
PREP["stats_import"] = ("mkdir -p ../fs_before && cp -R .pocketdeck-os ../fs_before/ && "
                        "rm -rf .pocketdeck-os/epub_* .pocketdeck-os/bookmarks .pocketdeck-os/clippings")

# Custom wallpapers from /sleep: a JPG (converted on the device) and a BMP.
for key, pic in (("sleep_custom_jpg", "lao-tzu.jpg"), ("sleep_custom_bmp", "moon-phases.bmp")):
    SECTIONS[key] = [("ENTER", None, 7000), ("SLEEP", "sleep", 9000)]
    REUSE[key] = "lib3"
    PREP[key] = settings(sleepScreen=2, wallpaperRotation=2) + " && mkdir -p sleep && cp '%s/sd-sample/sleep/%s' sleep/" % (REPO, pic)

# PDF / MOBI files are listed and explain how to convert them.
SECTIONS["convert_pdf"] = [("DOWN", None, 600), ("ENTER", None, 1500), ("ENTER", "01-books-with-pdf-and-mobi", 1500),
                           ("DOWN", None, 450), ("WAIT", "02-pdf-selected", 400), ("ENTER", "03-convert-help", 1500)]
REUSE["convert_pdf"] = "lib3"
PREP["convert_pdf"] = "echo PDF > 'books/Field manual.pdf' && echo BOOKMOBI > 'books/Kindle novel.mobi'"

# Migration: boot on a card that still has CrossInk's /.crosspoint folder.
SECTIONS["migration"] = [("WAIT", "01-home-after-migration", 9000), ("DOWN", None, 500), ("DOWN", None, 500),
                         ("DOWN", None, 500), ("DOWN", None, 500), ("ENTER", "02-saved-items-after-migration", 2500)]
REUSE["migration"] = "bookmarks"
PREP["migration"] = ""
