# Productivity Tools

PocketDeck-OS adds eleven tools under **Home > Tools**, the last item on the
Home menu. Every existing Home item keeps its place.

The Tools menu lists them in this order:

| Tool | What it does |
| --- | --- |
| Pomodoro | 25/5 focus and break timer inside a shrinking ring, with focus stats |
| Panchanga | Offline panchanga in Kannada or English, with Moon phase, festivals and holidays, for any date from 1976 to 2075, with a calendar picker |
| Mantras | Today's mantra, daily ritual sequence, 35 deities, ritual mantras, kavacha, and a japa counter with stats |
| World Clock | Local time plus up to four cities with automatic daylight saving, picked from a list of 79 |
| Habit Tracker | Up to 12 habits in a weekly grid with streaks, stats and graphs |
| Medicine | Time-bound medicine or supplement courses (1-4 doses a day, 1-90 days, before/after food) with adherence |
| Mood | One tap a day on a five-point scale, notes with time stamps, history, stats and graphs |
| Flashcards | Spaced-repetition review of JSON decks (one file per subject), with study stats |
| Daily Quote | A new quote each day from `quotes.txt`, favourites, and reading stats |
| Knowledge | Question-and-answer topics with detailed, paged answers and coverage stats |
| Today | Date, time, today's habit and focus counts, a checkable to-do list, and task stats |
| Stats & export | Writes every statistic to `/stats/<feature>/` as JSON; imports book data |

Every tool that records something has its own **stats page** with charts, and
**Confirm** on that page exports just that tool to `/stats/<feature>/`.

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
| Up / Down | Previous / next item |
| Confirm | The action named in the hint bar |
| Left | The action named in the hint bar: **Menu** or **Stats** (it opens when you let go) |
| Right | **Next** (next day, dose, face, deck or question) |
| Back (press) | Back one level (tool > Tools menu > Home) |
| Back (hold ~0.7 s) | Leave Tools and go straight Home |

No two buttons on a screen share a name, and every hint bar label does
something. The three hold actions left each show a hint line on screen when
they apply (Pomodoro reset, Today delete, Knowledge back to today's question).
On touch devices, a tap acts as Confirm and a horizontal swipe as previous/next.

| Tool | Confirm | Left | Right | Up / Down |
| --- | --- | --- | --- | --- |
| Pomodoro | Start / pause (hold when paused: reset) | **Stats** | Skip to the other phase | – (side buttons do nothing, so a stray press never ends a session) |
| Panchanga | **Menu**: today, calendar, go to month and year, about this day, Kannada/English, month back/forward, Moon animation | Previous day | Next day | Previous / next month |
| Panchanga calendar | Open the chosen day | Previous day | Next day | Previous / next month |
| Mantras | Open the selected row (Japa on a mantra: count) | **Settings** (mantra: previous; japa: undo) | **Japa** (mantra: next; japa: reset) | Move (mantra: page; japa: count) |
| World Clock | Sync the clock over WiFi | **Cities** | – | – |
| Habit Tracker | Tick / untick the selected day | **Menu**: stats and graphs, add, rename, delete, previous / next week | Next day | Previous / next habit |
| Medicine | Tick / untick the selected dose (or add the first course) | **Menu**: details and stats, stop, add, delete | Next dose | Previous / next course |
| Mood | Save the selected face for the shown day | **Menu**: add/edit note, history and notes, stats and graphs | Next face | Earlier / later day (last 30 days) |
| Flashcards | Deck list: study. Question: reveal. Answer: **Got it** | Deck list: **Stats**. Answer: **Forgot** | Deck list: next deck | Deck list: move (never grades a card) |
| Daily Quote | **Menu**: save/remove favourite, random quote, today's quote, my favourites, stats and graphs | Previous day | Next day | Previous / next page of a long quote |
| Knowledge | Topic list: open. Question: **Answer**. Answer: back to the question | Topic list: **Stats**. Question: previous question. Answer: previous page | Topic list: next topic. Question: next question. Answer: next page | Previous / next question |
| Today | Toggle task, or run "+ Add task" / "Clear completed" (hold on a task: delete) | **Stats** | Next row | Move selection |
| Stats & export | Export everything to `/stats` | **Import** book data | – | – |

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
  quotes/favorites.txt        Favourite quotes, one per line
  knowledge/<topic>.json      Knowledge topics
  flashcards/<deck>.json      Flashcard decks
  <feature>/log-YYYY-MM.txt   History behind the stats (habits, medicine,
                              mood, pomodoro, today, flashcards, knowledge,
                              quotes), one line
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
  whole day; **Menu > Random quote** picks another.
* The screen leads with the date in large type (for example "28 September
  2026") and the weekday, then the quote.
* **Menu > Save as favourite** adds the quote to `/tools/quotes/favorites.txt`;
  **My favourites** pages through them with Left/Right.
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
coconut water` on the device (Left > Menu > Add a new habit) or as new lines. Week files are matched by name, so adding, removing or reordering
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

An offline Kannada (or English) panchanga for the configured place (default
Bengaluru, UTC+5:30). It is second in the Tools menu. Left/Right browse days
and Up/Down browse months, up to five years either side of today.

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

**Kannada text.** The firmware has no OpenType shaper, so Kannada comes from an
SD card font, `/tools/fonts/kannada.knf` (21.8 MB, built by
`scripts/kannada/build_kannada_font.py` from the open-source **Noto Sans
Kannada**, SIL Open Font License). Every syllable of up to three consonants
(with reph, vowel sign or virama; ಕ್ಷ and ಜ್ಞ count as one) was shaped with
HarfBuzz and stored in a fixed slot, so the device finds a syllable's glyphs
with one small read. Pair kerning between syllables is applied from a table.
Checked against HarfBuzz on every Kannada word in the mantra collection, the
festival calendars and the Panchanga (3,509 words): all identical, glyphs and
spacing. Nothing is stored in flash (the old pre-rendered strings, 38.5 KB, are
gone); the Panchanga's own Kannada strings are kept one byte per letter.
Without the file, the Panchanga shows English.

**Range and accuracy, 1976-2075.** Delta T follows the NASA (Espenak and
Meeus) polynomials. Against JPL DE421: 1976 and 2048-2052 agree on every
element except five sunrise edge cases in 1,827 days, with end times within
0.7 min and sunrise/sunset within 3.5 s (DE421 ends in 2053).

**Festivals.** Dates come from the JSON layers in
`/tools/panchanga/festivals` (the included 1976-2075 Karnataka calendar, plus
any you add, such as a Mysuru or government-holiday layer; see
INSTALLATION_GUIDE.md section 3.6). The calculated festivals above fill years
a layer does not list, and the monthly observances come from the calculation.

**Moon animation.** When the date changes, the terminator sweeps into place
over a few frames and then stops. On e-ink a continuous animation would cause
ghosting and drain the battery. Turn it off in Panchanga's **Menu** (saved as
`animate=0` in `/tools/panchanga.txt`).

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
* **Display and battery.** Entering or leaving a tool does one clean refresh
  (full on X3, half on X4-class panels). Everything else is a fast partial
  refresh. Clocks check the time every 5 seconds and redraw only when the
  minute changes. A running Pomodoro shows whole minutes and redraws once a
  minute, then counts seconds only in its last minute (about 85 refreshes per
  25-minute session instead of 1,500), with a clean refresh every 30 updates.
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

In **Habit Tracker**, press **Left (Menu)** on a habit and choose **Stats and graphs**:

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

A **course** is one medicine or supplement taken 1 to 4 times a day for 1 to
90 days. Add one with **Confirm** (first course) or **Left (Menu) > Add a
course**. The wizard asks, in order:

1. **Name** (on-screen keyboard).
2. **Doses a day** (1 to 4), each with a fixed time: 1 = 08:00; 2 = 08:00 and
   20:00; 3 = 08:00, 13:00 and 19:00; 4 = 08:00, 13:00, 18:00 and 22:00.
   Times can be changed in `courses.txt`.
3. **Food**: before food, after food, with food, or any time.
4. **Length**: 1 to 90 days.
5. **Start**: today or tomorrow.
6. **Summary**, for example *"Starts Mon Sep 28, ends Thu Oct 1. 3 a day:
   Morning 08:00, Noon 13:00, Evening 19:00. After food. 12 doses in all."*
   Confirm saves it; Back goes back a step.

Tick doses with **Right (Next dose)** and **Confirm**. The list shows each
course's food rule. **Details and stats** show the schedule line (for example
"3 a day: 08:00, 13:00, 19:00 - After food"), started, ends / planned end,
status (Upcoming, Active, Completed, Stopped), day x of y, doses taken of
planned, adherence (taken ÷ due so far), missed doses, on time (within ±1 hour
of the planned time), average delay, days with every dose, and a grid of every
dose (filled = taken, outline = missed, grey = still to come). **Confirm** on
this page exports medicine stats.

A course completes when its last day has passed or every dose is taken.
**Stop course** ends it early and records the stop date. Dose times and the
food rule can be edited in `courses.txt` (8th field: `before`, `after`,
`with` or `-`).

## Mood

Choose a face (Awful, Low, Okay, Good, Great) with **Right (Next face)** and
press **Confirm**. Up/Down move to earlier days so a missed day can be filled
in. **Left (Menu)** has:

- **Add / edit note** for the shown day.
- **History and notes**: the last 90 days, newest first, each with date,
  time, mood (bar and name) and the note.
- **Stats and graphs**: 7- and 30-day averages, week-on-week trend, most
  common mood, good days, days logged, check-in streak, best weekday, usual
  check-in time, a 30-day line and the mood mix.

## Flashcards, Knowledge, Daily Quote and Today stats

| Tool | Where | What it shows |
| --- | --- | --- |
| Pomodoro | **Left (Stats)** | Sessions today / this week / 30 days, focus time, streak, sessions per active day, best day, usual start; 14-day sessions and 30-day start-time charts |
| Flashcards | Deck list: **Left (Stats)**, or **Confirm** after a session | Cards, due, seen, mastered, reviewed today / 30 days, recall rate, streak; 14-day reviews and 30-day recall charts |
| Knowledge | Topic list: **Left (Stats)** | Answers opened today / this week / 30 days, streak, topics, questions opened of all questions, coverage, usual time; 14-day chart and per-topic coverage |
| Daily Quote | **Menu > Stats and graphs** | Days opened (30 and 90), streak, favourites, quotes in the library, usual time; 30-day days and time-of-day charts |
| Today | **Left (Stats)** | Days tracked, tasks done of total, completion, perfect days and streak, tasks per day; 14-day completion and tasks-done charts |

## Panchanga language and menu

Press **Confirm (Menu)** in Panchanga for:

- **Go to today**
- **Calendar**: a month grid; Left/Right move a day, Up/Down a month; a dot
  marks a festival, a square a public holiday; the month's special days are
  listed under the grid; Confirm opens that day.
- **Go to month and year**: Decade, Year and Month rows (Up/Down choose a row,
  Left/Right change it), so any month from 1976 to 2075 is a few presses away.
- **About this day**: every festival and holiday for the date, with type,
  holiday status, place, note, meaning and the file it came from.
- **Switch to English / Kannada**, **Back / Forward one month**, **Moon animation**.

Choices are saved in `/tools/panchanga.txt` (`lang=`, `animate=`).

## Mantras

**Home** greets you by time of day, shows **today's mantra** (the weekday's
deity, a new mantra each week) and rows for **Start daily ritual**,
**Deities**, **Daily and ritual mantras**, **Kavacha**, **Japa counter** and
**Stats and graphs**. Each mantra shows its Kannada text, transliteration,
meanings (Kannada and English by default; Settings cycles to Kannada only
or English only) and when to chant it; long
text pages with Up/Down. **Japa** counts with Confirm or the side buttons
around a mala ring (108, 54 or 27), Left undoes, Right starts a new count.
The ring is the live count; "Malas today" and "Completed today" change only
when a mala completes (it is saved at once) or the session ends;
sessions go to `/tools/mantras/log-YYYY-MM.txt` and **Stats** (japa today,
week, 30 days, malas, streak, sessions, usual time, 14-day and time-of-day
charts, Export to `/stats/mantras/`). The file format is in
INSTALLATION_GUIDE.md section 3.7.

## Stats & export

**Confirm** writes one JSON file per feature (each tool's stats page can also
export just its own file):

| File | Contents |
| --- | --- |
| `/stats/habits/habits.json` | Every habit: streaks, rates, usual time, consistency, 12 weeks of ticks, 30 days of tick times |
| `/stats/medicine/medicine.json` | Every course: dates, status, doses planned/due/taken/missed, adherence, on-time rate, delay, and each day's doses with times |
| `/stats/mood/mood.json` | Averages, counts per mood and every logged day (90 days) |
| `/stats/pomodoro/pomodoro.json` | Sessions and focus minutes per day, best day, usual start (90 days) |
| `/stats/today/today.json` | Tasks done and total per day |
| `/stats/flashcards/flashcards.json` | Cards reviewed and remembered per deck and per day, recall rate, mastery (mastered / learning / new) |
| `/stats/knowledge/knowledge.json` | Answers opened per day, streak, per-topic coverage |
| `/stats/quotes/quotes.json` | Days opened, streak, favourite quotes |
| `/stats/reading/library.json` | Books, folders, opened, finished, in progress, average progress |
| `/stats/reading/global.json` | Total reading time, sessions, pages, streaks, time-of-day and weekday split |
| `/stats/reading/books/<book>.json` | One per opened book: progress, time read, sessions, pace, start/finish dates, bookmarks (chapter and snippet), clippings (full text), looked-up words, and a `restore` block |

**Left (Import)** imports: for each file in `/stats/reading/books` whose book
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
