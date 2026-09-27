REPO = os.path.join(HERE, "pocketdeck-os-main")
LIB = os.path.join(HERE, "library")
SEED = "python3 '%s/seed.py' && python3 '%s/seed_extra.py'" % (HERE, HERE)

def settings(**kv):
    import json
    kv.setdefault("clockUtcOffsetQ", 70)  # UTC+5:30
    return ("mkdir -p .crosspoint && python3 -c \"import json,os;p='.crosspoint/crossink-settings.json';"
            "d=json.load(open(p)) if os.path.exists(p) else {};d.update(%s);json.dump(d,open(p,'w'))\"" % json.dumps(kv).replace('"', "'"))

BASE = ("mkdir -p books .dictionaries && cp '%s'/*.epub books/ && cp -R '%s/dict/WordNet' .dictionaries/ && "
        "mkdir -p .crosspoint && printf '/.dictionaries/WordNet/WordNet' > .crosspoint/dictionary.bin" % (LIB, LIB))
PREP = {"*": BASE + " && " + SEED + " && " + settings()}
THEMES = [("classic", 0), ("lyra", 1), ("lyra-extended", 2), ("roundedraff", 3), ("lyra-carousel", 4), ("minimal", 5),
          ("dashboard", 6), ("pocketdeck", 7)]
SLEEPS = [("light-default", 1), ("dark", 0), ("page-overlay", 6), ("book-cover", 3), ("reading-stats", 7),
          ("minimal", 8), ("minimal-stats", 10), ("dashboard", 11)]
T_POMODORO, T_WORLD, T_HABITS, T_FLASH, T_QUOTE, T_KNOW, T_PANCH, T_TODAY = range(8)

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
                ("RIGHT", "06-break-phase", 1000)],
  "pomodoro_ring": tools(T_POMODORO) + [("ENTER", None, 30500), ("WAIT", "07-ring-quarter", 300),
                ("WAIT", None, 29700), ("WAIT", "08-ring-half", 300), ("WAIT", None, 29700), ("WAIT", "09-ring-three-quarters", 300)],
  "worldclock": tools(T_WORLD) + [("WAIT", "01-world-clock", 0), ("ENTER", "02-sync-wifi", 2500)],
  "habits": tools(T_HABITS) + [("WAIT", "01-eight-habits", 0), ("ENTER", "02-ticked-today", 900)] +
            [("DOWN", None, 400)] * 6 + [("ENTER", "03-yoga-ticked", 900), ("DOWN", None, 400), ("ENTER", "04-workout-ticked", 900)] +
            [("LEFT", None, 400)] * 8 + [("WAIT", "05-previous-week", 500)],
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
START["boot"] = "boot"; START["settings"] = "settings"; START["tools_menu"] = "tools"

for name, n in THEMES:
    for part in ("home", "tools", "settings"):
        key = f"theme_{name}_{part}"
        SECTIONS[key] = [("WAIT", part, 1500)]
        REUSE[key] = "lib3"
        PREP[key] = settings(uiTheme=n)
        if part != "home":
            START[key] = part

for name, n in SLEEPS:
    key = f"sleep_{name}"
    SECTIONS[key] = [("ENTER", None, 7000), ("SLEEP", "sleep", 1600)]
    REUSE[key] = "lib3"
    PREP[key] = settings(sleepScreen=n)
