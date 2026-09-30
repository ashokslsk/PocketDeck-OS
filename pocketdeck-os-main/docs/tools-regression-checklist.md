# Productivity Tools — Regression Checklist

The Tools feature is additive. This checklist confirms that existing CrossInk
behavior still works and that the new tools behave as documented in
`docs/productivity-tools.md`.

Status legend: **PASS (auto)** = verified by an automated build/test run in this
change; **HW** = must be confirmed on a physical Xteink X3 before release.

## 1. What was changed outside the new `src/activities/tools/` folder

| File | Change | Why it cannot regress existing features |
| --- | --- | --- |
| `src/activities/home/HomeActivity.{h,cpp}` | `Tools` appended after all menu entries; menu capacity 8→9; base item count 4→5; two dispatch `case`s; carousel cache version 5→6 | Existing entries keep order and index; the cache bump only forces one carousel rebuild after update |
| `src/activities/ActivityManager.{h,cpp}` | `goToTools()`; `HomeMenuItem::TOOLS` (appended); Home reselects Tools on return | New enum value at the end; existing values unchanged |
| `src/components/themes/BaseTheme.h`, `lyra/LyraTheme.cpp` | `UIIcon::Tools` (appended) mapped to the existing Lucide `menu` bitmap | No icon headers regenerated; existing icon values unchanged |
| `lib/I18n/translations/english.yaml` | New `STR_TOOLS_*` keys only | No existing key edited |
| `src/simulator/SimulatorSmokeTest.cpp` | Renders the Tools menu and all seven tools after Home | Simulator-only code |
| `test/CMakeLists.txt`, `test/tools_core/` | New native test target | Test-only |

Not touched: WiFi stack and credential store, EPUB engine and `/.pocketdeck-os`
cache layout, OTA updater and `partitions.csv`, SD init/SPI pins, settings
menus, sleep/wake and power management, button remapping, fonts, reading stats.

## 2. Automated checks run for this change

| Check | Command | Result |
| --- | --- | --- |
| X3/X4 firmware (ESP32-C3) | `pio run -e default` | **PASS (auto)**: 6,391,847 B, 161,753 B free (97.5%) |
| reTerminal Sticky (ESP32-S3, touch) | `pio run -e sticky` | **PASS (auto)**: 6,249,494 B, 304,106 B free |
| X4 Pro (ESP32-S3, touch, SDMMC) | `pio run -e x4-pro` | **PASS (auto)**: 6,339,733 B, 213,867 B free |
| Simulator builds | `pio run -e simulator`, `pio run -e simulator-X3` | **PASS (auto)** |
| Native unit tests | `cmake -S test -B /tmp/t && cmake --build /tmp/t --target ToolsCoreTest && /tmp/t/tools_core/ToolsCoreTest` | **PASS (auto)**: 32/32 tools_core (store, dates, SRS, Panchanga vs Drik, logs, JSON export, Kannada data) and 622/622 in the whole native suite |
| Smoke test, 7 themes | `scripts/run_simulator_smoke_test.py --theme {classic,lyra,lyra-extended,roundedraff,lyra-carousel,dashboard,pocketdeck}` | **PASS (auto)**: Home, File Browser, Recent Books, Settings, Reader Options, Reader Menu, Sleep, EPUB open + page turns, plus Tools menu and all tools |
| Scripted UI walk (X3 profile) | simulator input script + screenshots | **PASS (auto)**: every tool entered, used and exited; data files written correctly; no `.tmp`/`.bak` left behind; nothing written to `/.pocketdeck-os` |

### Size and memory (ESP32-C3 `default` build)

| | Before | After | Δ |
| --- | --- | --- | --- |
| Static RAM (`.data` + `.bss`) | 64,432 B | 64,552 B | +120 B |
| Firmware image | 6,105,600 B | 6,359,789 B | +254,189 B (all tools incl. Medicine/Mood/Stats, Panchanga, Inter digits, theme, logos) |
| OTA slot free | 448,000 B | 193,811 B | partition table unchanged |

Heap available to WiFi, EPUB parsing and OTA is unchanged: the tools have no
globals and allocate only while a tool is open.

## 3. Preserved features — on-device checks (X3)

Flash `dist/pocketdeck-os-x3.bin` (or the build output `firmware-x3-x4.bin`) over USB, or copy it to the SD card and use **Settings > System > SD Card Firmware Update**.

| # | Feature | Steps | Expected | Status |
| --- | --- | --- | --- | --- |
| P1 | WiFi connect | Home > File Transfer > join a saved network | Connects with saved credentials; no re-entry needed | HW |
| P2 | WiFi via tools | Tools > World Clock > Confirm | Same WiFi picker/credentials; "Clock synced"; Back > Back > Back reboots silently to Home | HW |
| P3 | EPUB open | Home > Browse Files > open an EPUB | Opens at last position; existing `/.pocketdeck-os/epub_*` cache reused | HW |
| P4 | Page turn | Turn 20+ pages both directions, open reader menu | No slowdowns or crashes; progress saved | HW |
| P5 | Reader after tools | Open every tool, return Home, Continue Reading | Book reopens normally; serial `maxAlloc` similar to before | HW |
| P6 | OTA check | Settings > System > Check for Updates | Version check runs; OTA flow unchanged | HW |
| P7 | Deep sleep / wake | Let the device auto-sleep in Home, in the reader, and inside a tool | Sleeps on the normal timeout; wakes correctly | HW |
| P8 | Pomodoro sleep policy | Start Pomodoro, leave it | Stays awake through focus + break only, then auto-sleeps | HW |
| P9 | SD browse | Browse Files through several folders; USB/web file transfer | Unchanged; `/tools` appears as a normal folder | HW |
| P10 | Settings | Open every Settings page; change and revert one option | Unchanged | HW |
| P11 | Home menus | Every theme (Classic, Lyra, Lyra Carousel, Rounded Raff, Minimal) | All previous items in the same order; Tools last | PASS (auto) in simulator; HW spot-check |
| P12 | Button remap | Remap front buttons in Settings, open a tool | Tools follow the remap; no global change | HW |

## 4. New tools — on-device checks

| # | Tool | Steps | Expected |
| --- | --- | --- | --- |
| T1 | Launcher | Home > Tools; Up/Down; Confirm; Back; hold Back inside a tool | Clean refresh on enter/exit; hold Back goes straight Home |
| T2 | Today | Toggle, hold Confirm to delete, "+ Add task", "Clear completed" | `daily.json` updated about 1.5 s later; a new day keeps only unfinished tasks |
| T3 | Pomodoro | Start, pause, skip, finish a phase | Ring depletes each minute (partial refresh); phase end does a clean refresh; count increments |
| T4 | World Clock | View before and after sync | Before sync: "Clock not set"; after: correct times and DST |
| T5 | Habits | Tick days; cross week boundaries; check streaks | Week file per ISO week; future days not toggleable |
| T6 | Daily Quote | Open on a dated day, Prev/Next day, Shuffle | Matches `quotes.txt`; index rebuilt after the file changes |
| T7 | Knowledge Card | Browse cards; page through a long card | Markdown rendered; the day's card marked `*` |
| T8 | Flashcards | Study a deck; mix Got it / Forgot; exit mid-session | Forgotten cards return; `srs_state.json` keeps other decks' entries |
| T9 | Power loss | Pull power during a save (or leave a `.bak`) | Next open restores from `.bak` |
| T10 | Missing data | Remove `/tools` entirely | Every tool shows a helpful empty state; nothing crashes |
| T11 | Habits menu | Hold Confirm > Add "Tender coconut water"; rename it; delete another | Keyboard opens; list updates; renamed habit keeps its ticks and streak; `habits.txt` rewritten atomically |
| T12 | Habit stats | Tick a habit on several days at different times; open Stats and graphs | Streaks match the grid; usual time and consistency follow the tick times; both graphs drawn |
| T13 | Medicine | Add "Paracetamol, 3 a day, 4 days, today"; tick doses; open Details; Stop course | Doses show filled; adherence, missed, on-time and delay update; status Stopped with the stop date |
| T14 | Mood | Pick a face and Confirm; hold Confirm to add a note; Up to yesterday and log it | Saved bar under the face; note shown; chart and averages update |
| T15 | Pomodoro stats | Finish a focus session; press Left | Session counted today; bar for today; timer keeps running underneath |
| T16 | World Clock cities | Hold Confirm; replace a city; remove one | `worldclock.txt` rewritten; times correct with DST |
| T17 | Panchanga language | Hold Confirm > English; reopen the tool | English names, no overlaps; `lang=en` kept; Kannada restores the bitmaps |
| T18 | Stats & export | Confirm; open the files on a computer | Nine feature files plus one per opened book; valid JSON |
| T19 | Book data import | Export; copy books and `/stats` to a fresh card; hold Confirm | Progress, bookmarks, clippings, look-ups and stats restored; nothing overwritten on a second run |
| T20 | Carousel book stats | Lyra Carousel theme; toggle Settings > Display > Carousel book stats | Half-height carousel with 10 stats, or the original large covers |
| T21 | Tilt in book | Book menu > Book Options > Tilt Page Turn on; tilt the X3 | Pages turn on tilt; the Settings > Controls toggle shows the same state |
| T22 | Rotating wallpaper | Put 3 images (one JPG) in `/sleep`; Custom + Every 15 min; sleep for 45 min on battery | The picture changes about every 15 min; power button still wakes; JPG gets a `.jpg.bmp` copy |
| T23 | PDF / MOBI | Copy a .pdf and a .mobi; open them | Listed in Browse Files; a conversion help page opens instead of the reader |

## 5. Final verification run (2026-09-27, final enhancements)

`default`, `sticky`, `x4-pro`, `simulator` and `simulator-X3` builds passed;
32/32 tools_core tests and all 622 native tests passed; the smoke test passed in
7 themes (Tools and every tool rendered); the scripted X3 walk captured all 176
screens (`docs/screenshots`); book-data import restored 10/10 reader files
byte-for-byte; a card with CrossInk's `/.crosspoint` folder was migrated to
`/.pocketdeck-os` with progress, bookmarks, clippings and covers intact.
Touched files formatted with clang-format 21.

## 5a. Earlier verification run (2026-09-27)

All run sequentially on the final sources: `default`, `sticky`, `x4-pro`,
`simulator` and `simulator-X3` builds all passed; 22/22 unit tests passed; the
smoke test passed in classic, lyra, lyra-carousel and roundedraff (8 tool
screens rendered in each); the scripted X3 UI walk completed. cppcheck (repo
flags) and clang-format 21 are clean on all new and touched files.
Screenshots: `docs/images/tools-x3-simulator.png`.
