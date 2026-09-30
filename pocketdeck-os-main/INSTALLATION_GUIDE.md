# PocketDeck-OS installation guide

PocketDeck-OS 1.0.0 · a personal project by Ashok Kumar Srinivas

This guide has four parts:

1. [Install PocketDeck-OS on a reader that already runs CrossInk](#1-install-over-crossink)
2. [First-time setup](#2-first-time-setup-about-10-minutes)
3. [The content routine: adding quotes, flashcards and knowledge](#3-the-content-routine)
4. [Backups, moving to a new card, and going back to CrossInk](#4-backups-new-cards-and-going-back)

---

## 1. Install over CrossInk

You need:

- The reader, charged above 50% (keep it on USB power while installing if you can).
- Its microSD card and a way to copy files to it: a card reader, or the reader's own **File Transfer** (WiFi) page.
- The firmware file for your device, from the `dist/` folder:

| Device | File |
| --- | --- |
| Xteink X3 or X4 | `pocketdeck-os-x3.bin` |
| Seeed reTerminal Sticky | `pocketdeck-os-sticky.bin` |
| Xteink X4 Pro | `pocketdeck-os-x4-pro.bin` |

### What is kept

Everything on the SD card stays: books, reading progress, bookmarks, clippings,
reading stats, settings, WiFi networks, dictionaries and fonts.

PocketDeck-OS keeps its reader data in its own hidden folder,
**`/.pocketdeck-os`**. On the first start it **moves** CrossInk's
`/.crosspoint` folder there in one step, so all of the above comes along; it
also renames `crossink-settings.json` to `pocketdeck-os-settings.json` and
`/.crossink-stats-backup` to `/.pocketdeck-os-stats-backup`. If a
`/.pocketdeck-os` folder already exists, nothing is moved or overwritten. The
tools write under `/tools`, `/stats` and `/sleep`.

### Steps (SD card method, no computer tools needed)

1. **Copy the firmware to the card.** Put `pocketdeck-os-x3.bin` anywhere on
   the SD card, for example in the root folder. Either:
   - take the card out, copy the file with a card reader, and put the card back, or
   - on the reader open **Home > File Transfer**, connect to WiFi, open the
     address it shows in a browser on your phone or computer, and upload the file.
2. **Open the updater.** On the reader: **Settings > System > SD Card Firmware Update**.
3. **Pick the file.** Browse to `pocketdeck-os-x3.bin` and press **Select**.
4. **Confirm.** The reader checks the image (size, chip and board type) before
   writing anything. If the check fails, nothing is changed.
5. **Wait.** Writing takes about a minute. Do not remove the card or press
   buttons. The reader restarts by itself.
6. **Check.** The PocketDeck-OS boot screen appears. **Settings** shows
   "PocketDeck-OS 1.0.0" at the bottom, and **Settings > System > About**
   shows the version and credits.

### Alternative: USB with esptool

Use this if the reader will not start, or it runs stock Xteink firmware.

```bash
pip3 install esptool
```

Find the port (`ls /dev/cu.*` on a Mac, for example `/dev/cu.usbmodem2101`),
then flash app slot 0 and clear the OTA selector:

```bash
esptool.py --chip esp32c3 --port /dev/cu.usbmodem2101 --baud 921600 write_flash 0x10000 dist/pocketdeck-os-x3.bin
```

```bash
esptool.py --chip esp32c3 --port /dev/cu.usbmodem2101 erase_region 0xe000 0x2000
```

Use `--chip esp32s3` and the matching file for the Sticky and X4 Pro.

### Important: do not use "Check for Updates"

**Settings > System > Check for Updates** still looks at the CrossInk release
feed (PocketDeck-OS keeps CrossInk's version number, 1.6.0, for that check).
Accepting an update there replaces PocketDeck-OS with stock CrossInk. To update
PocketDeck-OS, install a newer PocketDeck-OS `.bin` with the steps above.

---

## 2. First-time setup (about 10 minutes)

### 2.1 Copy the starter pack

From `sd-sample/` copy these folders to the **root** of the SD card:

```
SD card (root)
├── tools/            sample quotes, flashcards, knowledge, habits, settings
│   ├── fonts/        kannada.knf (21.8 MB): Kannada text for Panchanga, Mantras and your files
│   ├── panchanga/festivals/  karnataka-1976-2075.json (festival and holiday calendar)
│   ├── mantras/      mantras.json (35 deities, daily/ritual mantras, kavacha)
│   ├── quotes.txt
│   ├── flashcards/   ai_concepts.json, python_basics.json, sql_basics.json
│   ├── knowledge/    AI.json, Python.json, SQL.json, Panchanga_Kannada.json
│   ├── habits/       habits.txt
│   ├── worldclock.txt, pomodoro.txt, daily.json
├── sleep/            example wallpapers (.bmp and .jpg)
└── (your books, anywhere, e.g. /Books)
```

Nothing breaks if you skip this. Each tool creates its own files and folders
the first time you open it, but Quote, Flashcards, Knowledge and Mantras have
nothing to show until they have content, and Kannada text needs
`tools/fonts/kannada.knf` (without it the Panchanga shows English).

> **The Kannada font is on the card, not in the firmware.** Copy
> `tools/fonts/kannada.knf` with a card reader (it is 21.8 MB, which is slow
> over WiFi). Keep the name and folder exactly as shown. The first time you
> open Panchanga and Mantras they spend a few seconds preparing an index of
> the festival and mantra files; after that they open at once.

**Folders PocketDeck-OS creates for you:** `/tools`, `/tools/.cache`,
`/tools/habits`, `/tools/medicine`, `/tools/mood`, `/tools/pomodoro`,
`/tools/today`, `/tools/flashcards`, `/tools/knowledge` and `/stats`.

**Folders you create yourself:** `/sleep`, for wallpapers.

### 2.2 Set the clock

Habits, Medicine, Mood, Pomodoro stats, Quote-of-the-day and Panchanga need the date.

1. **Settings > System > Device > Clock UTC Offset**: for India choose +5:30.
2. **Home > Tools > World Clock**, press **Confirm** and connect to WiFi once. The clock syncs over the internet.

### 2.3 Personalise (all optional)

| What | Where |
| --- | --- |
| Carousel with book stats | Settings > Display > UI Theme = **Lyra Carousel**; **Carousel book stats** on (default) or off for big covers |
| Tilt page turn | Inside a book: Menu > **Book Options** > **Tilt Page Turn** (also Menu > gear > Controls, and Settings > Controls) |
| Rotating wallpaper | Put pictures in `/sleep`, then Settings > Display > Sleep Screen: **Wallpaper = Custom**, **Change wallpaper = Every 30 min**. Help: **How to add wallpapers** in the same menu |
| Panchanga language | Tools > Panchanga (second in the list), **Confirm (Menu)** > Switch to English / Kannada |
| Pick any date 1976-2075 | Tools > Panchanga > **Menu > Calendar** (Left/Right day, Up/Down month, Confirm opens it) or **Menu > Go to month and year** (decade, year, month) |
| Mantras | Tools > Mantras: **Left (Settings)** for meanings (Kannada and English, Kannada only, English only), mala size (108/54/27) and the daily ritual |
| Pomodoro seconds | Shown by default (MM:SS). To save battery, add `seconds=0` to `/tools/pomodoro.txt` for whole minutes |
| World Clock cities | Tools > World Clock, **Left (Cities)**, pick a city slot, pick a city |
| Habits | Tools > Habit Tracker, **Left (Menu)** > Add a new habit (e.g. "Tender coconut water") |
| Medicine course | Tools > Medicine, **Confirm** (or **Left (Menu)** > Add a course): name, doses per day, before/after food, length, start, then check the summary |
| Favourite quotes | Tools > Daily Quote, **Confirm (Menu)** > Save as favourite |

Every tool's hint bar names each button, and no two buttons share a name.
**Left** opens a tool's **Menu** or **Stats** page; **Right** is always
**Next**. Each stats page has an **Export** button for that tool.

---

## 3. The content routine

Make this a weekly or monthly habit. All the tools read plain files in
`/tools`, which you can prepare on your computer or phone.

### 3.1 The routine in five steps

1. **Write or update** the file on your computer, using the templates below.
2. **Check the JSON.** Run the command below (it prints the file if it is
   valid and shows the line with the mistake if not), or paste the file into any JSON validator.

   ```bash
   python3 -m json.tool Python.json
   ```

3. **Copy it to the card**, in the folder named below: with a card reader, or
   over WiFi with **Home > File Transfer** (upload into that folder).
4. **Open the tool on the reader.** Indexes rebuild by themselves when a file
   changes. There is no restart and no cache to clear.
5. **Check it once**: the new deck, topic or quote should be there. If a tool
   says it has nothing to show, the JSON has a mistake (go back to step 2).

Ready-to-copy templates for every file below are in [docs/examples/templates](docs/examples/templates).

### 3.2 Daily quotes → `/tools/quotes.txt`

One quote per line, UTF-8 text:

```
2026-12-25|Peace begins with a smile. — Mother Teresa
01-01|Every new year is a fresh page. — Anonymous
*|Only used for random picks. — Author
```

- `YYYY-MM-DD|…` shows on that exact date.
- `MM-DD|…` shows on that day every year.
- `*|…` has no date: it only appears as a random pick. Random picks (**Menu > Random quote**, or a day with no dated quote) come from the whole file.
- Text after ` — ` (space, em dash, space) is shown as the author.
- Keep one line per quote. Up to 2,048 bytes per line; long quotes page with Up/Down.

**Monthly habit:** paste next month's 30 dated lines at the end of the file.
Order does not matter. To build a year of quotes from an EPUB anthology, see
`scripts/tools/extract_quotes.py` and `scripts/tools/build_quotes.py`.

### 3.3 Flashcards → `/tools/flashcards/<deck name>.json`

The file name becomes the deck name (`python_basics.json` shows as
"python_basics"). One file per subject; up to 16 decks and 400 cards each.

```json
{
  "cards": [
    {"question": "What does len() return for a dict?", "answer": "The number of **keys**."},
    {"question": "Which SQL clause filters groups?", "answer": "**HAVING** (WHERE filters rows)."}
  ]
}
```

- Answers may use `**bold**`, `- bullets` and `1. numbered` lines; use `\n` for a new line.
- Review history is kept per question text, so you can add, remove or reorder
  cards at any time. Changing a question's wording makes it a new card.
- A plain list `[{"question": …, "answer": …}]` also works.

### 3.4 Knowledge → `/tools/knowledge/<topic>.json`

Questions with longer, paged answers. The topic list shows file names.

```json
{
  "topic": "Python",
  "cards": [
    {
      "question": "What are list comprehensions?",
      "answer": "A **list comprehension** builds a list in one expression.\n\n# Filtering\n- `[x for x in nums if x > 0]`\n\n# When not to use them\n1. The body needs several statements.\n2. You only need a loop, not a list."
    }
  ]
}
```

- Formatting: `# Heading`, `**bold**`, `- bullet`, `1. numbered`, `` `code` ``.
- Up to 500 questions per topic and about 4 KB per answer; long answers are paged ("Page 2 / 4").
- The "question of the day" changes daily by itself.

### 3.5 Habits, medicine, world clock (usually done on the device)

| File | Format |
| --- | --- |
| `/tools/habits/habits.txt` | one habit name per line, up to 12 |
| `/tools/medicine/courses.txt` | `id\|name\|doses per day\|days\|start YYYY-MM-DD\|stopped or -\|times\|food` e.g. `c1\|Paracetamol\|3\|4\|2026-09-27\|-\|08:00,13:00,19:00\|after` (food: `before`, `after`, `with` or `-`; older 7-field lines still work) |
| `/tools/quotes/favorites.txt` | favourite quotes, one per line (written by the device) |
| `/tools/worldclock.txt` | `Name\|+5:30\|NONE` (DST rules: US, EU, AU, NZ, NONE), max 4 lines |
| `/tools/panchanga.txt` | `lat=`, `lon=`, `tz=`, `animate=1`, `lang=kn` or `lang=en` |

### 3.6 Festival calendars → `/tools/panchanga/festivals/*.json`

Every `.json` file in this folder is one **layer**, and all layers show on a date:

| Layer (example file) | What it holds |
| --- | --- |
| `karnataka-1976-2075.json` (included) | Master calendar: festivals, jayantis, vratas and state holidays |
| `mysuru.json` (your own) | Venue dates, e.g. "Mysuru Dasara" when it differs from Vijayadashami |
| `holidays-2027.json` (your own) | This year's government holiday notification |

Format: `{"festivals": [ {...}, ... ]}` (or just the list). Each entry needs a
`"date": "YYYY-MM-DD"` and a name; the rest is shown when present:

```json
{"festivals": [
  {"date": "2020-10-26", "kannada_name": "ಮೈಸೂರು ದಸರಾ", "english_name": "Mysuru Dasara",
   "type": "festival", "is_public_holiday": true, "location_scope": "Mysuru",
   "date_note": "Procession date; differs from the general Vijayadashami date."}
]}
```

- Up to 8 files and 6,000 entries in total. A file is re-read automatically
  when it is added, removed or changes size.
- For Ugadi, Janmashtami, Ganesh Chaturthi, Navaratri, Vijayadashami, Deepavali,
  Shivaratri and Makara Sankranti, a year that a layer lists uses **the layer's
  date**; years it does not list (and the monthly Ekadashi, Hunnime, Amavasya
  and Sankashti) come from the Panchanga's own calculation. "Mysuru Dasara"
  counts as its own event, so it never hides Vijayadashami.
- **Menu > About this day** shows every entry for a date with its type,
  holiday status, place, note, meaning and the file it came from.

### 3.7 Mantras → `/tools/mantras/mantras.json`

The included file has 35 deities (20 mantras each), 100 daily and ritual
mantras in 14 categories, and the Sri Vishwakarma Kavacham. To use your own,
keep the same structure:

```json
{"vedic_mantras_collection": {
   "vishnu": {"deity_kannada": "ವಿಷ್ಣು", "deity_english": "Vishnu",
              "mantras": [{"kannada": "ಓಂ ನಮೋ ನಾರಾಯಣಾಯ", "english": "Om Namo Narayanaya",
                           "meaning_english": "...", "meaning_kannada": "..."}]},
   "additional_ritualistic_mantras": {"mantras": [
     {"kannada": "...", "english": "...", "category": "Morning routine", "sequence": 1,
      "use": "Waking in the morning"}]}},
 "kavacha_collection": [{"name_kannada": "...", "name_english": "...",
   "rishi": {"kannada": "...", "english": "..."},
   "sections": {"dhyana": {"title_kannada": "...", "title_english": "...", "mantras": [...]}}}]}
```

- **Deities** come from `vedic_mantras_collection`; **daily and ritual
  mantras** are grouped by `"category"` in order of `"sequence"`; each
  **kavacha** lists its sections, an **About** page (rishi, chandas, devata,
  beeja, shakti, keelaka, phalashruti) and **Read** (the whole kavacha).
- **Today's mantra** is the weekday's deity (Sunday Surya, Monday Shiva, Tuesday
  Hanuman, Wednesday Vishnu, Thursday Dattatreya, Friday Lakshmi, Saturday
  Venkateshwara) with a new mantra each week. Change the deities with the
  `days=` line in `/tools/mantras/settings.txt`.
- **Start daily ritual** plays one ritual category (default "Morning routine");
  choose another with **Left (Change)**.
- **Japa counter:** Confirm or the side buttons count; Left undoes; Right
  resets (after asking). Every session is logged in
  `/tools/mantras/log-YYYY-MM.txt`; each full mala is saved at once.

### 3.8 Kannada in your own files

Quotes, flashcards and knowledge files can be written in Kannada (UTF-8), or
mix Kannada and English; they are drawn with the Kannada font. Formatting
(`# heading`, `- bullet`, `1.` lists, `**bold**`) works the same. The sample
`knowledge/Panchanga_Kannada.json` shows the format.

### 3.9 Wallpapers → `/sleep/`

- `.bmp` shows at once; `.jpg` is converted once into a `.bmp` copy next to it (two per sleep).
- The best size for the X3 is 528 × 792 pixels, portrait. Grayscale with good contrast looks best on e-ink.
- A pinned favourite wallpaper overrides rotation; unpin it to rotate.

---

## 4. Backups, new cards and going back

### Export your stats (monthly)

**Tools > Stats & export > Confirm** writes JSON files you can open on any computer:

```
/stats/habits/habits.json         /stats/pomodoro/pomodoro.json
/stats/medicine/medicine.json     /stats/today/today.json
/stats/mood/mood.json             /stats/flashcards/flashcards.json
/stats/knowledge/knowledge.json   /stats/quotes/quotes.json
/stats/mantras/mantras.json
/stats/reading/library.json       /stats/reading/global.json
/stats/reading/books/<book>.json  (one per book)
```

Each tool's own stats page also has **Export** (Confirm) for just that tool.
Copy `/stats` and `/tools` to your computer as a backup.

### Moving books to another card or device

1. On the old card: **Tools > Stats & export > Confirm**.
2. Copy the books (in the **same folders**) and the `/stats` folder to the new card.
3. On the new card: **Tools > Stats & export**, press **Left (Import)**, then confirm.
   Progress, reading time, bookmarks, clippings and looked-up words come back.
   Anything already on the new card is never overwritten.

### Going back to CrossInk

1. Install CrossInk's `firmware-x3-x4.bin` with the same **SD Card Firmware
   Update** steps.
2. To keep your progress, bookmarks and settings, rename two things on the
   card with a computer (hidden files must be shown): the folder
   `/.pocketdeck-os` to `/.crosspoint`, and inside it
   `pocketdeck-os-settings.json` to `crossink-settings.json`.
   Without this, CrossInk starts with fresh settings; your books are not affected.
3. `/tools`, `/stats` and `/sleep` are simply ignored by CrossInk.

### If something goes wrong

- **The reader does not start after flashing:** reflash it with the USB method
  above (esptool works whatever firmware is on the reader).
- **A tool shows nothing:** check the JSON (step 3.1) and the folder name.
- **The clock is wrong:** check the UTC offset, then sync again from World Clock.
- **Wallpaper does not change while asleep:** the X4 powers down completely in
  sleep, so it changes on each sleep instead. The X3 wakes itself on the timer.
