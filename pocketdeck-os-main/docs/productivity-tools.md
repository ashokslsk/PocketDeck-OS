# Productivity Tools

PocketDeck-OS adds eleven tools under **Home > Tools**, the last item on the
Home menu. Every existing Home item keeps its place.

| Tool | What it does |
| --- | --- |
| Pomodoro | 25/5 focus and break timer inside a shrinking ring; **Left** opens focus stats (sessions, focus hours, streak, 14-day chart, usual start time) |
| World Clock | Local time plus up to four cities with automatic daylight saving; hold Confirm to pick cities from a built-in list of 79 |
| Habit Tracker | Up to 12 habits in a weekly grid with streaks; hold Confirm for stats and graphs, add, rename, delete |
| Medicine | Time-bound medicine or supplement courses (1-4 doses a day, 1-90 days) with adherence and dose timing |
| Mood | One tap a day on a five-point scale, optional note, 30-day graph and trends |
| Flashcards | Spaced-repetition review of JSON decks (one file per subject) |
| Daily Quote | A new quote each day from `quotes.txt`; long quotes page with Up/Down |
| Knowledge | Question-and-answer topics with detailed, paged answers |
| Panchanga | Offline panchanga in Kannada or English, with Moon phase, for any date within 5 years |
| Today | Date, time, today's habit and focus counts, and a checkable to-do list |
| Stats & export | Writes every statistic to `/stats/<feature>/` as JSON; imports book data |

## Getting started

1. Copy the `sd-sample/tools` folder to the **root** of the SD card, so the card
   has `/tools/quotes.txt`, `/tools/knowledge/…` and so on. The tools also work
   without it; each one explains which file it needs.
2. Set your time zone in **Settings > System > Clock UTC Offset**.
3. Open **Tools > World Clock** and press **Confirm** to sync the time over WiFi.
   This reuses the normal WiFi picker and saved networks. After one sync the
   RTC keeps time on its own.

## Buttons

The tools use CrossInk's logical buttons, so any remapping you set in
**Settings > Controls** still applies. The hint bar at the bottom of each screen
shows what the front buttons do. Buttons are only interpreted this way inside
Tools; nothing is remapped globally.

| Button | In every tool |
| --- | --- |
| Up / Left / Page Back | Previous item |
| Down / Right / Page Forward | Next item |
| Confirm (press) | Select / toggle |
| Confirm (hold ~0.7 s, then release) | Secondary action (see below); it happens on release |
| Back (press) | Back one level (tool > Tools menu > Home) |
| Back (hold ~0.7 s) | Leave Tools and go straight Home |

On touch devices, a tap acts as Confirm and a horizontal swipe as previous/next.

| Tool | Confirm | Confirm (hold) | Left / Right | Up / Down |
| --- | --- | --- | --- | --- |
| Pomodoro | Start / pause | Reset the current phase | Left: **stats**. Right: skip to the other phase | Up / Down: skip phase |
| World Clock | Sync the clock over WiFi | Choose cities | – | – |
| Habit Tracker | Tick / untick the selected day | Menu: stats and graphs, add, rename, delete | Previous / next day (crosses into other weeks) | Previous / next habit |
| Medicine | Tick / untick the selected dose for today (or add the first course) | Menu: add a course, details and stats, stop, delete | Previous / next dose | Previous / next course |
| Mood | Save the selected mood for the shown day | Add or edit a note | Choose a face | Up: earlier day, Down: later day (last 30 days) |
| Stats & export | Export everything to `/stats` | Import book data | – | – |
| Flashcards | Deck list: study. Question: reveal. Answer: **Got it** | – | Answer: Left = **Forgot**, Right = **Got it** | Deck list: move |
| Daily Quote | Random quote | Back to today's quote | Previous / next day's quote | Previous / next page of a long quote |
| Knowledge | Topic list: open. Question/answer: switch between them | Back to the question of the day | Question: Left = previous question, Right = answer. Answer: previous / next page | Previous / next question |
| Panchanga | Jump to today (replays the Moon animation) | Choose Kannada or English | Previous / next day | Previous / next month (30 days) |
| Today | Toggle task, or run the "+ Add task" / "Clear completed" rows | Delete the selected task | Move selection | Move selection |

## Files on the SD card

Tool data lives under `/tools`; exports go to `/stats` and wallpapers to
`/sleep`. The tools only read reader data in `/.pocketdeck-os` for Stats & export,
and only write there when you import book data. Every save writes
`<file>.tmp` first, moves the old file to `<file>.bak`, renames the new file
into place, then deletes the backup. If power is lost mid-save, the `.bak`
copy is restored the next time the file is read. Log appends use the same
sequence, so history is never half-written.

```
/tools
  quotes.txt                  Daily Quote library (you provide; sample included)
  daily.json                  Today's to-do list (created automatically)
  pomodoro.txt                Focus/break lengths and today's completed count
  worldclock.txt              World Clock cities (created with defaults)
  panchanga.txt               Place, time zone, animation and language
  srs_state.json              Flashcard review history (created automatically)
  habits/habits.txt           Habit names, one per line (up to 12)
  habits/2026-W39.txt         Ticks, one file per ISO week
  medicine/courses.txt        Medicine and supplement courses
  knowledge/<topic>.json      Knowledge topics
  flashcards/<deck>.json      Flashcard decks
  <feature>/log-YYYY-MM.txt   History behind the stats (habits, medicine,
                              mood, pomodoro, today, flashcards), one line
                              per event: "YYYY-MM-DD HH:MM|fields"
  .cache/                     Rebuildable indexes; safe to delete at any time
/stats                        Written by Tools > Stats & export (see below)
/sleep                        Wallpapers (.bmp, .jpg) for the Custom sleep screen
```

### quotes.txt

One quote per line, UTF-8:

```
2026-09-27|Your time is limited, so don't waste it living someone else's life. — Steve Jobs
09-27|A quote for 27 September every year — Author
*|A quote that is only used for random picks — Author
```

* `YYYY-MM-DD|…` shows on that exact date. `MM-DD|…` repeats every year.
* A line without a date prefix, or with `*|`, only joins the random pool.
* If no line matches today, a random quote is chosen. It stays the same for the
  whole day, and **Confirm** picks another.
* Text after the last ` — ` (space, em dash, space) or ` -- ` is shown as the author.
* After midnight the screen moves to the new day's quote by itself.
* A quote too long for one screen, even at the smallest size, pages with Up/Down (each line holds up to 2,048 bytes)
  and shows "1 / 2"; the author appears on the last page.
* `/tools/.cache/quotes.idx` stores 8 bytes per line (date key + byte offset) and
  is rebuilt whenever `quotes.txt` changes size. Only the one matching line is
  ever read into memory.

The sample `quotes.txt` covers every day from 2026-09-27 to 2027-12-31. It
contains 365 motivational quotes from *365 Best Inspirational Quotes* (K. E. Kruse)
and 60 classical Taoist quotations (Tao Te Ching, Chuang Tzu) from *A Year of
Taoism* (E. Reninger), kept on the calendar dates that book assigns them. It is
generated by `scripts/tools/build_quotes.py`.

### daily.json

```json
{
  "date": "2026-09-27",
  "todos": [
    {"text": "Review PR", "done": false},
    {"text": "Run one Pomodoro", "done": true}
  ]
}
```

Up to 20 tasks, 79 bytes each. When the date changes, completed tasks from
earlier days are removed and unfinished ones carry over. New tasks are added
on the device with **+ Add task**, which opens the on-screen keyboard.

### habits/

`habits.txt` holds up to twelve names, one per line (defaults: Water, Reading,
Exercise, Meditation, Study, Sleep). Add habits such as `Yoga` or `Tender
coconut water` on the device (hold Confirm > Add a new habit) or as new lines. Week files are matched by name, so adding, removing or reordering
habits keeps their history. Each week gets its own small file named by ISO week:

```
Water|1101100
Reading|1111110
...
```

The seven digits are Monday to Sunday (`1` = done). A streak counts consecutive
done days up to today; if today isn't ticked yet, it counts up to yesterday.
Streaks look back at most a year.

### worldclock.txt

```
# Name|UTC offset in standard time|DST rule
New York|-5|US
London|0|EU
Tokyo|+9|NONE
Sydney|+10|AU
```

Up to four cities. The offset can be in hours (`+9`, `-5`, `+5:30`) or minutes
(`330`). DST rules: `US`, `EU`, `AU` (south-east Australia), `NZ`, or `NONE`.
Other regions can use `NONE` with a manually adjusted offset.

### pomodoro.txt

```
focus=25
break=5
date=2026-09-27
completed=3
```

Edit `focus` (1–120) and `break` (1–60) to change the lengths. A finished focus
session starts its break automatically. When the break ends, the timer stops
and waits for **Confirm**.

### knowledge/

```
/tools/knowledge/Python.json
/tools/knowledge/SQL.json
```

Each file is one topic (the file name is shown in the topic list):

```json
{
  "topic": "Python",
  "cards": [
    {"question": "What are list comprehensions?",
     "answer": "A **list comprehension** builds a list in one expression.\n\n# Filtering\n- `[x for x in nums if x > 0]`"}
  ]
}
```

A bare array of `{"question", "answer"}` objects also works. Answers support
`# Heading`, `- bullet`, `1. numbered`, `**bold**` and `` `code` ``; use `\n`
for new lines. Long answers are split into pages with a "Page 2 / 4" footer and
page dots. Each topic opens on its question of the day (days since 1970 mod the
number of questions); up to 500 questions per topic, about 4 KB per answer.

### flashcards/

A deck is a JSON array (or an object with a `"cards"` array):

```json
[
  {"question": "What does len() return for a dict?", "answer": "The number of **keys**."},
  {"question": "Which clause filters groups?", "answer": "HAVING"}
]
```

Up to 16 decks and 400 cards per deck. Questions up to 399 bytes and answers up
to 799 bytes are shown in full; longer text is cut off. Answers support the same
Markdown subset as knowledge cards.

**Scheduling** (simplified, two numbers per card):

* **Got it**: interval becomes 1 day the first time, then doubles (max 180 days).
* **Forgot**: interval becomes 0, and the card comes back at the end of the session.
* A card is due when `today >= lastReviewed + interval`. New cards are always due.

`srs_state.json` stores one entry per reviewed card:

```json
{"version": 1, "cards": [
  {"deck": "python_basics", "id": "160dff45", "lastReviewed": "2026-09-27", "interval": 4}
]}
```

`id` is a hash of the question text, so reordering a deck or adding cards keeps
the history of unchanged cards. Editing a question's text resets that card.

## Panchanga

An offline Kannada panchanga for the configured place (default Bengaluru,
UTC+5:30). Left/Right browse days and Up/Down browse months, up to five years
either side of today.

It shows:
- **Vāra, date, samvatsara and māsa.** Months follow the Karnataka amānta
  calendar, and an adhika māsa is marked ಅಧಿಕ.
- **Tithi (with pakṣa), nakṣatra, yoga and karaṇa** current at sunrise, each
  with its end time. The end time is marked ನಾಳೆ when it falls after midnight,
  and the element reads ಪೂರ್ಣ ರಾತ್ರಿ when it lasts past the next sunrise.
- **Sunrise and sunset.**
- **Rāhu, Yamagaṇḍa and Guḷika kāla, and Abhijit muhūrta.**
- **The day's festivals or observances.**
- **A Moon-phase disc** with the illumination percentage and the Moon's current nakṣatra.

**Accuracy.** Checked against NASA JPL's DE421 ephemeris over 2021–2031
(4,017 days): sunrise and sunset agree within 2 seconds, and tithi and nakṣatra
end times within 30 seconds. Spot checks against Drik Panchang for Bengaluru
agree to the minute on every element, and every kāla window matches.
- **Sun:** VSOP87.
- **Moon:** Meeus ch. 47 (ELP-2000/82).
- **Ayanāṃśa:** Lahiri, calibrated to Drik's published values.
- **Tests:** these checks live in `test/tools_core`.

**Festivals.** Each festival is decided by the tithi at its traditional time of
day, and kṣaya tithis are handled, as in the major panchangas:
- Ugadi at sunrise
- Vijayadaśamī in the afternoon (aparāhṇa)
- Deepavali in the evening (pradoṣa)
- Janmāṣṭamī and Śivarātri at midnight (niśītha)
- Makara Saṅkrānti on the day whose sunset follows the transit

**Known variance.** Where Drik applies extra sect-specific rules (Smārta or
Vaiṣṇava Janmāṣṭamī), the date can differ by a day.

**Kannada text.** The firmware cannot shape Kannada (conjuncts, reordered vowel
signs), so every string is shaped at build time with HarfBuzz and rendered from
the open-source **Noto Sans Kannada** font (SIL Open Font License). The result
is about 72 KB of flash. Regenerate with
`scripts/panchanga/gen_kannada_bitmaps.py`; the font and its licence are in
`lib/EpdFont/builtinFonts/source/NotoSansKannada/`.

**Moon animation.** When the date changes, the terminator sweeps into place
over a few frames and then stops. On e-ink a continuous animation would cause
ghosting and drain the battery. Turn it off with `animate=0` in
`/tools/panchanga.txt`.

## Fonts

Large clock digits (Today, World Clock, Pomodoro) use **Inter**, an open-source
typeface (SIL Open Font License) already bundled for the UI. Only the digits,
colon, hyphen and space are embedded at 44 pt and 30 pt, which adds about 9 KB
of flash. See `lib/EpdFont/scripts/convert-builtin-fonts.sh`.

## How it stays out of the way

* **RAM.** A tool's buffers are fixed-size arrays inside its Activity object. The
  object is allocated once when the tool opens and freed when it closes. There are
  no globals (static RAM grew by 8 bytes), and nothing is allocated per frame.
  Large files (quotes, decks, SRS state) are streamed from the SD card, never
  loaded whole. Rough per-tool sizes: Flashcards about 6 KB, Knowledge Card about
  4 KB, Today about 2 KB, the rest under 1 KB.
* **Heap hygiene.** Leaving Tools triggers CrossInk's silent restart to Home
  (the same defragmentation used after WiFi sessions) if the World Clock used WiFi,
  or if the largest free heap block is below 40 KB.
* **Display.** Entering or leaving a tool does one clean refresh (full on X3,
  half on X4-class panels). Everything else is a fast partial refresh. Clocks redraw once a
  minute; a running Pomodoro redraws its MM:SS readout every second (fast
  refresh) with one clean refresh every five minutes to clear ghosting.
* **Sleep.** Only a running Pomodoro phase holds off auto-sleep. It stops after
  its break, so it can delay sleep by at most one focus + break.
* **Firmware and OTA.** The partition table is unchanged, so OTA works exactly
  as before. The tools add about 64 KB to the image, which leaves 384 KB
  free in each OTA slot.

## Limitations

* Pomodoro keeps time only while its screen is open. There is no sound, so a
  finished phase is marked with a clean full-screen refresh.
* Date-based tools need a real-time clock. The X3 has one (DS3231), so after one
  sync the time survives deep sleep. Devices without an RTC (for example the X4)
  show "Clock not set", just like the existing header clock.
* World Clock DST covers US, EU, AU and NZ rules. Other zones need a manual offset.
* The Knowledge Card order follows the directory order within each category.
* Tool screens are in English for now. All strings go through the i18n system
  (`STR_TOOLS_*` in `lib/I18n/translations/english.yaml`), so other languages
  fall back to English until they are translated.


## Habit stats and graphs

In **Habit Tracker**, hold Confirm on a habit and choose **Stats and graphs**:

| Stat | Meaning |
| --- | --- |
| Current streak | Days in a row up to today (an unticked today does not break it) |
| Best streak | Longest run in the last year |
| Done, last 30 days | Share of the last 30 days that were ticked |
| Days done, 12 weeks | Ticked days out of 84 |
| Usual time | Average time of day the habit was ticked on the day itself |
| Time consistency | How far, on average, a tick is from the usual time (smaller = more regular) |
| Average gap | Average time between two check-ins (for example 24 h for daily coconut water) |
| Best weekday | The weekday it is done most often |

Graphs: weekly completion for 12 weeks (line) and the time of day of each tick
for 30 days (dots, with a dashed line at the usual time). Ticks for earlier
days count for streaks but not for time-of-day stats.

## Medicine

A **course** is a medicine or supplement taken 1 to 4 times a day for 1 to 90
days. Add one with **Confirm** (first course) or hold Confirm > **Add a
course**: type the name, choose doses per day (Morning / Noon / Evening /
Night), the length and whether it starts today or tomorrow. Tick today's doses
with Left/Right and Confirm.

**Details and stats** show: started, ends / planned end, status (Upcoming,
Active, Completed, Stopped), day x of y, completed on / stopped on, doses taken
of planned, adherence (taken ÷ due so far), missed doses, on time (within
±1 hour of the planned time), average delay, days with every dose, and a grid
of every dose (filled = taken, outline = missed, grey = still to come).

A course completes when its last day has passed or every dose is taken.
**Stop course** ends it early and records the stop date. Dose times can be
edited in `courses.txt`.

## Mood

Choose a face (Awful, Low, Okay, Good, Great) and press Confirm; hold Confirm
to add a note. Up/Down move to earlier days so a missed day can be filled in.
Stats: 7- and 30-day averages, week-on-week trend, most common mood, days
logged, check-in streak, best weekday and usual check-in time, with a 30-day line.

## Panchanga language

Hold Confirm in Panchanga to switch between **Kannada** (Noto Sans Kannada,
pre-shaped at build time) and **English** (Drik-style transliteration). The
choice is saved as `lang=` in `/tools/panchanga.txt`.

## Stats & export

**Confirm** writes one JSON file per feature:

| File | Contents |
| --- | --- |
| `/stats/habits/habits.json` | Every habit: streaks, rates, usual time, consistency, 12 weeks of ticks, 30 days of tick times |
| `/stats/medicine/medicine.json` | Every course: dates, status, doses planned/due/taken/missed, adherence, on-time rate, delay, and each day's doses with times |
| `/stats/mood/mood.json` | Averages, counts per mood and every logged day (90 days) |
| `/stats/pomodoro/pomodoro.json` | Sessions and focus minutes per day, best day, usual start (90 days) |
| `/stats/today/today.json` | Tasks done and total per day |
| `/stats/flashcards/flashcards.json` | Cards reviewed and remembered per deck and per day, recall rate |
| `/stats/reading/library.json` | Books, folders, opened, finished, in progress, average progress |
| `/stats/reading/global.json` | Total reading time, sessions, pages, streaks, time-of-day and weekday split |
| `/stats/reading/books/<book>.json` | One per opened book: progress, time read, sessions, pace, start/finish dates, bookmarks (chapter and snippet), clippings (full text), looked-up words, and a `restore` block |

**Hold Confirm** imports: for each file in `/stats/reading/books` whose book
is on the card at the same path, it restores the reader's progress, stats,
bookmarks, clippings and look-up history, skipping anything the device
already has. This is how a book continues on another card or device.

## Reading additions

- **Carousel book stats** (Settings > Display, Lyra Carousel theme): the
  carousel is drawn at half height and the lower half shows the selected
  book's progress bar, time read, time left, estimated finish date (at your
  daily pace), pages per minute, sessions, start date, bookmarks, highlights
  (clippings), words looked up and pages read.
- **Tilt page turn** is also in each book's **Book Options** (and in the
  book menu's Controls page), next to Tilt Direction.
- **Rotating wallpapers:** Sleep Screen > Change wallpaper (every 15 min to
  2 hours) with Wallpaper = Custom. See *How to add wallpapers* in that menu.
- **PDF / MOBI / AZW3** files are listed in the file browser and explain how
  to convert them to EPUB or XTC.
