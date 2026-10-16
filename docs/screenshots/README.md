# PocketDeck-OS screenshot walkthrough

Every screen of PocketDeck-OS 1.0.0, captured from the CrossInk simulator with the **Xteink X3** profile (792x528 panel, portrait). The library is public-domain books from Project Gutenberg, the dictionary is built from Princeton WordNet 3.0, and tool data comes from `sd-sample/tools` plus a few seeded days of habits and to-dos. Clocks are set to UTC+5:30 (Bengaluru).

Captured headlessly with the simulator's `CROSSPOINT_SIM_INPUT_SCRIPT` / `CROSSPOINT_SIM_SCREENSHOTS` variables (`SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software`). The simulator-only `CROSSINK_SIMULATOR_START_SCREEN=boot|tools|settings` opens a screen directly.

![Overview](overview.png)

## Contents

- [01. Boot and sleep screens](#01-boot-and-sleep-screens) (11)
- [02. Home and library](#02-home-and-library) (6)
- [03. Reader and dictionary](#03-reader-and-dictionary) (8)
- [04. Bookmarks and clippings](#04-bookmarks-and-clippings) (14)
- [05. Reading stats and library dashboard](#05-reading-stats-and-library-dashboard) (3)
- [06. File transfer](#06-file-transfer) (1)
- [07. Settings and About](#07-settings-and-about) (14)
- [08. UI themes](#08-ui-themes) (24)
- [09. Lyra Carousel with book stats](#09-lyra-carousel-with-book-stats) (4)
- [10. Tilt page turn and PDF / MOBI](#10-tilt-page-turn-and-pdf-mobi) (6)
- [11. Tools launcher](#11-tools-launcher) (1)
- [12. Pomodoro](#12-pomodoro) (11)
- [13. World Clock](#13-world-clock) (6)
- [14. Habit Tracker](#14-habit-tracker) (10)
- [15. Medicine and supplements](#15-medicine-and-supplements) (17)
- [16. Mood](#16-mood) (8)
- [17. Flashcards](#17-flashcards) (12)
- [18. Daily Quote](#18-daily-quote) (11)
- [19. Knowledge](#19-knowledge) (10)
- [20. Panchanga: calendar 1976-2075, festivals, Kannada and English](#20-panchanga-calendar-1976-2075-festivals-kannada-and-english) (21)
- [21. Mantras and japa counter](#21-mantras-and-japa-counter) (18)
- [22. Kannada in your own JSON files](#22-kannada-in-your-own-json-files) (3)
- [23. Today](#23-today) (5)
- [24. Stats & export](#24-stats-export) (6)
- [25. Upgrade from CrossInk (data folder moved)](#25-upgrade-from-crossink-data-folder-moved) (2)


## 01. Boot and sleep screens

| ![Boot splash](01-boot-and-sleep/01-boot-splash.png) | ![Sleep: PocketDeck-OS (default)](01-boot-and-sleep/02-sleep-pocketdeck-os-default.png) | ![Sleep: PocketDeck-OS dark](01-boot-and-sleep/03-sleep-pocketdeck-os-dark.png) | ![Sleep: Page with PocketDeck-OS card](01-boot-and-sleep/04-sleep-page-with-pocketdeck-os-card.png) |
|---|---|---|---|
| **01** Boot splash | **02** Sleep: PocketDeck-OS (default) | **03** Sleep: PocketDeck-OS dark | **04** Sleep: Page with PocketDeck-OS card |

| ![Sleep: Book cover](01-boot-and-sleep/05-sleep-book-cover.png) | ![Sleep: Reading stats](01-boot-and-sleep/06-sleep-reading-stats.png) | ![Sleep: Minimal](01-boot-and-sleep/07-sleep-minimal.png) | ![Sleep: Minimal stats](01-boot-and-sleep/08-sleep-minimal-stats.png) |
|---|---|---|---|
| **05** Sleep: Book cover | **06** Sleep: Reading stats | **07** Sleep: Minimal | **08** Sleep: Minimal stats |

| ![Sleep: Dashboard](01-boot-and-sleep/09-sleep-dashboard.png) | ![Sleep: custom wallpaper (JPG converted)](01-boot-and-sleep/10-sleep-custom-wallpaper-jpg-converted.png) | ![Sleep: custom wallpaper (BMP)](01-boot-and-sleep/11-sleep-custom-wallpaper-bmp.png) |
|---|---|---|
| **09** Sleep: Dashboard | **10** Sleep: custom wallpaper (JPG converted) | **11** Sleep: custom wallpaper (BMP) |

## 02. Home and library

| ![Home before reading](02-home-and-library/01-home-before-reading.png) | ![Browse files](02-home-and-library/02-browse-files.png) | ![Books folder](02-home-and-library/03-books-folder.png) | ![Home continue reading](02-home-and-library/04-home-continue-reading.png) |
|---|---|---|---|
| **01** Home before reading | **02** Browse files | **03** Books folder | **04** Home continue reading |

| ![Home with library](02-home-and-library/05-home-with-library.png) | ![Recent books](02-home-and-library/06-recent-books.png) |
|---|---|
| **05** Home with library | **06** Recent books |

## 03. Reader and dictionary

| ![Book cover page](03-reader-and-dictionary/01-book-cover-page.png) | ![Reader menu](03-reader-and-dictionary/02-reader-menu.png) | ![Select chapter](03-reader-and-dictionary/03-select-chapter.png) | ![Chapter page](03-reader-and-dictionary/04-chapter-page.png) |
|---|---|---|---|
| **01** Book cover page | **02** Reader menu | **03** Select chapter | **04** Chapter page |

| ![Next page](03-reader-and-dictionary/05-next-page.png) | ![Look up word menu](03-reader-and-dictionary/06-look-up-word-menu.png) | ![Word select](03-reader-and-dictionary/07-word-select.png) | ![Dictionary definition](03-reader-and-dictionary/08-dictionary-definition.png) |
|---|---|---|---|
| **05** Next page | **06** Look up word menu | **07** Word select | **08** Dictionary definition |

## 04. Bookmarks and clippings

| ![Chapter page](04-bookmarks-and-clippings/01-chapter-page.png) | ![Add bookmark](04-bookmarks-and-clippings/02-add-bookmark.png) | ![Bookmarked page](04-bookmarks-and-clippings/03-bookmarked-page.png) | ![Create clipping](04-bookmarks-and-clippings/04-create-clipping.png) |
|---|---|---|---|
| **01** Chapter page | **02** Add bookmark | **03** Bookmarked page | **04** Create clipping |

| ![Clipping start](04-bookmarks-and-clippings/05-clipping-start.png) | ![Clipping first word](04-bookmarks-and-clippings/06-clipping-first-word.png) | ![Clipping range](04-bookmarks-and-clippings/07-clipping-range.png) | ![Clipping saved](04-bookmarks-and-clippings/08-clipping-saved.png) |
|---|---|---|---|
| **05** Clipping start | **06** Clipping first word | **07** Clipping range | **08** Clipping saved |

| ![Menu with clippings](04-bookmarks-and-clippings/09-menu-with-clippings.png) | ![Clippings list](04-bookmarks-and-clippings/10-clippings-list.png) | ![Clipping preview](04-bookmarks-and-clippings/11-clipping-preview.png) | ![Clipping opened in the book (highlighted)](04-bookmarks-and-clippings/12-clipping-opened-in-the-book-highlighted.png) |
|---|---|---|---|
| **09** Menu with clippings | **10** Clippings list | **11** Clipping preview | **12** Clipping opened in the book (highlighted) |

| ![Home with saved items](04-bookmarks-and-clippings/13-home-with-saved-items.png) | ![Saved items](04-bookmarks-and-clippings/14-saved-items.png) |
|---|---|
| **13** Home with saved items | **14** Saved items |

## 05. Reading stats and library dashboard

| ![Reading stats this book](05-reading-stats/01-reading-stats-this-book.png) | ![Reading stats this device](05-reading-stats/02-reading-stats-this-device.png) | ![Reading stats library](05-reading-stats/03-reading-stats-library.png) |
|---|---|---|
| **01** Reading stats this book | **02** Reading stats this device | **03** Reading stats library |

## 06. File transfer

| ![File transfer](06-file-transfer/01-file-transfer.png) |
|---|
| **01** File transfer |

## 07. Settings and About

| ![Display](07-settings-and-about/01-display.png) | ![Reader](07-settings-and-about/02-reader.png) | ![Controls](07-settings-and-about/03-controls.png) | ![System](07-settings-and-about/04-system.png) |
|---|---|---|---|
| **01** Display | **02** Reader | **03** Controls | **04** System |

| ![System about row](07-settings-and-about/05-system-about-row.png) | ![About page 1](07-settings-and-about/06-about-page-1.png) | ![About page 2](07-settings-and-about/07-about-page-2.png) | ![About page 3](07-settings-and-about/08-about-page-3.png) |
|---|---|---|---|
| **05** System about row | **06** About page 1 | **07** About page 2 | **08** About page 3 |

| ![Carousel book stats row](07-settings-and-about/09-carousel-book-stats-row.png) | ![Sleep settings](07-settings-and-about/10-sleep-settings.png) | ![Change wallpaper row](07-settings-and-about/11-change-wallpaper-row.png) | ![Change wallpaper options](07-settings-and-about/12-change-wallpaper-options.png) |
|---|---|---|---|
| **09** Carousel book stats row | **10** Sleep settings | **11** Change wallpaper row | **12** Change wallpaper options |

| ![How to add wallpapers](07-settings-and-about/13-how-to-add-wallpapers.png) | ![Help page 2](07-settings-and-about/14-help-page-2.png) |
|---|---|
| **13** How to add wallpapers | **14** Help page 2 |

## 08. UI themes

| ![PocketDeck (new): home](08-themes/01-pocketdeck-new-home.png) | ![PocketDeck (new): tools](08-themes/02-pocketdeck-new-tools.png) | ![PocketDeck (new): settings](08-themes/03-pocketdeck-new-settings.png) | ![Lyra (default): home](08-themes/04-lyra-default-home.png) |
|---|---|---|---|
| **01** PocketDeck (new): home | **02** PocketDeck (new): tools | **03** PocketDeck (new): settings | **04** Lyra (default): home |

| ![Lyra (default): tools](08-themes/05-lyra-default-tools.png) | ![Lyra (default): settings](08-themes/06-lyra-default-settings.png) | ![Classic: home](08-themes/07-classic-home.png) | ![Classic: tools](08-themes/08-classic-tools.png) |
|---|---|---|---|
| **05** Lyra (default): tools | **06** Lyra (default): settings | **07** Classic: home | **08** Classic: tools |

| ![Classic: settings](08-themes/09-classic-settings.png) | ![Lyra Extended: home](08-themes/10-lyra-extended-home.png) | ![Lyra Extended: tools](08-themes/11-lyra-extended-tools.png) | ![Lyra Extended: settings](08-themes/12-lyra-extended-settings.png) |
|---|---|---|---|
| **09** Classic: settings | **10** Lyra Extended: home | **11** Lyra Extended: tools | **12** Lyra Extended: settings |

| ![RoundedRaff: home](08-themes/13-roundedraff-home.png) | ![RoundedRaff: tools](08-themes/14-roundedraff-tools.png) | ![RoundedRaff: settings](08-themes/15-roundedraff-settings.png) | ![Lyra Carousel (book stats): home](08-themes/16-lyra-carousel-book-stats-home.png) |
|---|---|---|---|
| **13** RoundedRaff: home | **14** RoundedRaff: tools | **15** RoundedRaff: settings | **16** Lyra Carousel (book stats): home |

| ![Lyra Carousel (book stats): tools](08-themes/17-lyra-carousel-book-stats-tools.png) | ![Lyra Carousel (book stats): settings](08-themes/18-lyra-carousel-book-stats-settings.png) | ![Minimal: home](08-themes/19-minimal-home.png) | ![Minimal: tools](08-themes/20-minimal-tools.png) |
|---|---|---|---|
| **17** Lyra Carousel (book stats): tools | **18** Lyra Carousel (book stats): settings | **19** Minimal: home | **20** Minimal: tools |

| ![Minimal: settings](08-themes/21-minimal-settings.png) | ![Dashboard: home](08-themes/22-dashboard-home.png) | ![Dashboard: tools](08-themes/23-dashboard-tools.png) | ![Dashboard: settings](08-themes/24-dashboard-settings.png) |
|---|---|---|---|
| **21** Minimal: settings | **22** Dashboard: home | **23** Dashboard: tools | **24** Dashboard: settings |

## 09. Lyra Carousel with book stats

| ![Book stats: bookmarks, highlights, look-ups](09-carousel-book-stats/01-book-stats-bookmarks-highlights-look-ups.png) | ![Second book](09-carousel-book-stats/02-second-book.png) | ![Third book](09-carousel-book-stats/03-third-book.png) | ![Carousel book stats off (classic)](09-carousel-book-stats/04-carousel-book-stats-off-classic.png) |
|---|---|---|---|
| **01** Book stats: bookmarks, highlights, look-ups | **02** Second book | **03** Third book | **04** Carousel book stats off (classic) |

## 10. Tilt page turn and PDF / MOBI

| ![Book options](10-tilt-and-formats/01-book-options.png) | ![Tilt row](10-tilt-and-formats/02-tilt-row.png) | ![Tilt on](10-tilt-and-formats/03-tilt-on.png) | ![Books with pdf and mobi](10-tilt-and-formats/04-books-with-pdf-and-mobi.png) |
|---|---|---|---|
| **01** Book options | **02** Tilt row | **03** Tilt on | **04** Books with pdf and mobi |

| ![Pdf selected](10-tilt-and-formats/05-pdf-selected.png) | ![Convert help](10-tilt-and-formats/06-convert-help.png) |
|---|---|
| **05** Pdf selected | **06** Convert help |

## 11. Tools launcher

| ![Tools menu](11-tools-launcher/01-tools-menu.png) |
|---|
| **01** Tools menu |

## 12. Pomodoro

| ![Ready](12-pomodoro/01-ready.png) | ![Running](12-pomodoro/02-running.png) | ![Running seconds](12-pomodoro/03-running-seconds.png) | ![Paused](12-pomodoro/04-paused.png) |
|---|---|---|---|
| **01** Ready | **02** Running | **03** Running seconds | **04** Paused |

| ![Reset](12-pomodoro/05-reset.png) | ![Break phase](12-pomodoro/06-break-phase.png) | ![Pomodoro stats](12-pomodoro/07-pomodoro-stats.png) | ![Stats exported](12-pomodoro/08-stats-exported.png) |
|---|---|---|---|
| **05** Reset | **06** Break phase | **07** Pomodoro stats | **08** Stats exported |

| ![Ring quarter](12-pomodoro/09-ring-quarter.png) | ![Ring half](12-pomodoro/10-ring-half.png) | ![Ring three quarters](12-pomodoro/11-ring-three-quarters.png) |
|---|---|---|
| **09** Ring quarter | **10** Ring half | **11** Ring three quarters |

## 13. World Clock

| ![World clock](13-world-clock/01-world-clock.png) | ![Sync wifi](13-world-clock/02-sync-wifi.png) | ![Choose city slot](13-world-clock/03-choose-city-slot.png) | ![City catalogue](13-world-clock/04-city-catalogue.png) |
|---|---|---|---|
| **01** World clock | **02** Sync wifi | **03** Choose city slot | **04** City catalogue |

| ![Pick mumbai](13-world-clock/05-pick-mumbai.png) | ![Mumbai added](13-world-clock/06-mumbai-added.png) |
|---|---|
| **05** Pick mumbai | **06** Mumbai added |

## 14. Habit Tracker

| ![Nine habits](14-habit-tracker/01-nine-habits.png) | ![Ticked today](14-habit-tracker/02-ticked-today.png) | ![Yoga ticked](14-habit-tracker/03-yoga-ticked.png) | ![Coconut water ticked](14-habit-tracker/04-coconut-water-ticked.png) |
|---|---|---|---|
| **01** Nine habits | **02** Ticked today | **03** Yoga ticked | **04** Coconut water ticked |

| ![Habit menu](14-habit-tracker/05-habit-menu.png) | ![Coconut water stats](14-habit-tracker/06-coconut-water-stats.png) | ![Stats exported](14-habit-tracker/07-stats-exported.png) | ![Yoga stats](14-habit-tracker/08-yoga-stats.png) |
|---|---|---|---|
| **05** Habit menu | **06** Coconut water stats | **07** Stats exported | **08** Yoga stats |

| ![Add habit keyboard](14-habit-tracker/09-add-habit-keyboard.png) | ![Previous week](14-habit-tracker/10-previous-week.png) |
|---|---|
| **09** Add habit keyboard | **10** Previous week |

## 15. Medicine and supplements

| ![Courses](15-medicine/01-courses.png) | ![Evening dose ticked](15-medicine/02-evening-dose-ticked.png) | ![Course menu](15-medicine/03-course-menu.png) | ![Paracetamol details](15-medicine/04-paracetamol-details.png) |
|---|---|---|---|
| **01** Courses | **02** Evening dose ticked | **03** Course menu | **04** Paracetamol details |

| ![Details exported](15-medicine/05-details-exported.png) | ![Vitamin d details](15-medicine/06-vitamin-d-details.png) | ![Completed course](15-medicine/07-completed-course.png) | ![Stopped course](15-medicine/08-stopped-course.png) |
|---|---|---|---|
| **05** Details exported | **06** Vitamin d details | **07** Completed course | **08** Stopped course |

| ![Add course name](15-medicine/09-add-course-name.png) | ![Name typed](15-medicine/10-name-typed.png) | ![Ok key](15-medicine/11-ok-key.png) | ![Doses per day](15-medicine/12-doses-per-day.png) |
|---|---|---|---|
| **09** Add course name | **10** Name typed | **11** Ok key | **12** Doses per day |

| ![Food](15-medicine/13-food.png) | ![Length](15-medicine/14-length.png) | ![Starts](15-medicine/15-starts.png) | ![Course summary](15-medicine/16-course-summary.png) |
|---|---|---|---|
| **13** Food | **14** Length | **15** Starts | **16** Course summary |

| ![Course added](15-medicine/17-course-added.png) |
|---|
| **17** Course added |

## 16. Mood

| ![Mood today](16-mood/01-mood-today.png) | ![Choose great](16-mood/02-choose-great.png) | ![Mood saved](16-mood/03-mood-saved.png) | ![Yesterday with note](16-mood/04-yesterday-with-note.png) |
|---|---|---|---|
| **01** Mood today | **02** Choose great | **03** Mood saved | **04** Yesterday with note |

| ![Mood menu](16-mood/05-mood-menu.png) | ![History and notes](16-mood/06-history-and-notes.png) | ![Mood stats](16-mood/07-mood-stats.png) | ![Note keyboard](16-mood/08-note-keyboard.png) |
|---|---|---|---|
| **05** Mood menu | **06** History and notes | **07** Mood stats | **08** Note keyboard |

## 17. Flashcards

| ![Decks](17-flashcards/01-decks.png) | ![Flashcard stats](17-flashcards/02-flashcard-stats.png) | ![Question](17-flashcards/03-question.png) | ![Answer](17-flashcards/04-answer.png) |
|---|---|---|---|
| **01** Decks | **02** Flashcard stats | **03** Question | **04** Answer |

| ![Next question](17-flashcards/05-next-question.png) | ![Answer](17-flashcards/06-answer.png) | ![Forgot card returns](17-flashcards/07-forgot-card-returns.png) | ![Session complete](17-flashcards/08-session-complete.png) |
|---|---|---|---|
| **05** Next question | **06** Answer | **07** Forgot card returns | **08** Session complete |

| ![Stats after session](17-flashcards/09-stats-after-session.png) | ![Deck list again](17-flashcards/10-deck-list-again.png) | ![Sql question](17-flashcards/11-sql-question.png) | ![Sql answer](17-flashcards/12-sql-answer.png) |
|---|---|---|---|
| **09** Stats after session | **10** Deck list again | **11** Sql question | **12** Sql answer |

## 18. Daily Quote

| ![Todays quote](18-daily-quote/01-todays-quote.png) | ![Long quote page 1](18-daily-quote/02-long-quote-page-1.png) | ![Long quote page 2](18-daily-quote/03-long-quote-page-2.png) | ![Next day](18-daily-quote/04-next-day.png) |
|---|---|---|---|
| **01** Todays quote | **02** Long quote page 1 | **03** Long quote page 2 | **04** Next day |

| ![Quote menu](18-daily-quote/05-quote-menu.png) | ![Saved to favourites](18-daily-quote/06-saved-to-favourites.png) | ![Taoist quote](18-daily-quote/07-taoist-quote.png) | ![My favourites](18-daily-quote/08-my-favourites.png) |
|---|---|---|---|
| **05** Quote menu | **06** Saved to favourites | **07** Taoist quote | **08** My favourites |

| ![Next favourite](18-daily-quote/09-next-favourite.png) | ![Back to daily](18-daily-quote/10-back-to-daily.png) | ![Quote stats](18-daily-quote/11-quote-stats.png) |
|---|---|---|
| **09** Next favourite | **10** Back to daily | **11** Quote stats |

## 19. Knowledge

| ![Topics](19-knowledge/01-topics.png) | ![Knowledge stats](19-knowledge/02-knowledge-stats.png) | ![Question of the day](19-knowledge/03-question-of-the-day.png) | ![Question](19-knowledge/04-question.png) |
|---|---|---|---|
| **01** Topics | **02** Knowledge stats | **03** Question of the day | **04** Question |

| ![Answer page 1](19-knowledge/05-answer-page-1.png) | ![Answer page 2](19-knowledge/06-answer-page-2.png) | ![Answer page 3](19-knowledge/07-answer-page-3.png) | ![Next question](19-knowledge/08-next-question.png) |
|---|---|---|---|
| **05** Answer page 1 | **06** Answer page 2 | **07** Answer page 3 | **08** Next question |

| ![Next question right](19-knowledge/09-next-question-right.png) | ![Topics again](19-knowledge/10-topics-again.png) |
|---|---|
| **09** Next question right | **10** Topics again |

## 20. Panchanga: calendar 1976-2075, festivals, Kannada and English

| ![Today](20-panchanga/01-today.png) | ![Next day](20-panchanga/02-next-day.png) | ![Menu](20-panchanga/03-menu.png) | ![Calendar](20-panchanga/04-calendar.png) |
|---|---|---|---|
| **01** Today | **02** Next day | **03** Menu | **04** Calendar |

| ![Calendar next month](20-panchanga/05-calendar-next-month.png) | ![Calendar day selected](20-panchanga/06-calendar-day-selected.png) | ![Opened from calendar](20-panchanga/07-opened-from-calendar.png) | ![About this day](20-panchanga/08-about-this-day.png) |
|---|---|---|---|
| **05** Calendar next month | **06** Calendar day selected | **07** Opened from calendar | **08** About this day |

| ![Go to month and year](20-panchanga/09-go-to-month-and-year.png) | ![Decade row](20-panchanga/10-decade-row.png) | ![1970s](20-panchanga/11-1970s.png) | ![Month row](20-panchanga/12-month-row.png) |
|---|---|---|---|
| **09** Go to month and year | **10** Decade row | **11** 1970s | **12** Month row |

| ![Calendar 1980](20-panchanga/13-calendar-1980.png) | ![Panchanga 1980](20-panchanga/14-panchanga-1980.png) | ![Menu](20-panchanga/15-menu.png) | ![English today](20-panchanga/16-english-today.png) |
|---|---|---|---|
| **13** Calendar 1980 | **14** Panchanga 1980 | **15** Menu | **16** English today |

| ![English calendar](20-panchanga/17-english-calendar.png) | ![English calendar next month](20-panchanga/18-english-calendar-next-month.png) | ![English calendar day](20-panchanga/19-english-calendar-day.png) | ![English opened day](20-panchanga/20-english-opened-day.png) |
|---|---|---|---|
| **17** English calendar | **18** English calendar next month | **19** English calendar day | **20** English opened day |

| ![English about this day](20-panchanga/21-english-about-this-day.png) |
|---|
| **21** English about this day |

## 21. Mantras and japa counter

| ![Home](21-mantras/01-home.png) | ![Todays mantra](21-mantras/02-todays-mantra.png) | ![Page 2](21-mantras/03-page-2.png) | ![Japa](21-mantras/04-japa.png) |
|---|---|---|---|
| **01** Home | **02** Todays mantra | **03** Page 2 | **04** Japa |

| ![Japa counting](21-mantras/05-japa-counting.png) | ![Home after japa](21-mantras/06-home-after-japa.png) | ![Daily ritual](21-mantras/07-daily-ritual.png) | ![Ritual step 1](21-mantras/08-ritual-step-1.png) |
|---|---|---|---|
| **05** Japa counting | **06** Home after japa | **07** Daily ritual | **08** Ritual step 1 |

| ![Ritual step 2](21-mantras/09-ritual-step-2.png) | ![Deities](21-mantras/10-deities.png) | ![Vishnu mantras](21-mantras/11-vishnu-mantras.png) | ![Mantra](21-mantras/12-mantra.png) |
|---|---|---|---|
| **09** Ritual step 2 | **10** Deities | **11** Vishnu mantras | **12** Mantra |

| ![Ritual categories](21-mantras/13-ritual-categories.png) | ![Kavacha](21-mantras/14-kavacha.png) | ![Kavacha about](21-mantras/15-kavacha-about.png) | ![Kavacha reading](21-mantras/16-kavacha-reading.png) |
|---|---|---|---|
| **13** Ritual categories | **14** Kavacha | **15** Kavacha about | **16** Kavacha reading |

| ![Japa stats](21-mantras/17-japa-stats.png) | ![Settings](21-mantras/18-settings.png) |
|---|---|
| **17** Japa stats | **18** Settings |

## 22. Kannada in your own JSON files

| ![Kannada question](22-kannada-in-your-files/01-kannada-question.png) | ![Kannada answer](22-kannada-in-your-files/02-kannada-answer.png) | ![Kannada answer list](22-kannada-in-your-files/03-kannada-answer-list.png) |
|---|---|---|
| **01** Kannada question | **02** Kannada answer | **03** Kannada answer list |

## 23. Today

| ![Today](23-today/01-today.png) | ![Task completed](23-today/02-task-completed.png) | ![Today stats](23-today/03-today-stats.png) | ![Add task row](23-today/04-add-task-row.png) |
|---|---|---|---|
| **01** Today | **02** Task completed | **03** Today stats | **04** Add task row |

| ![New task keyboard](23-today/05-new-task-keyboard.png) |
|---|
| **05** New task keyboard |

## 24. Stats & export

| ![Stats and export](24-stats-and-export/01-stats-and-export.png) | ![Export finished](24-stats-and-export/02-export-finished.png) | ![Import confirm](24-stats-and-export/03-import-confirm.png) | ![Import on the same card (nothing overwritten)](24-stats-and-export/04-import-on-the-same-card-nothing-overwritten.png) |
|---|---|---|---|
| **01** Stats and export | **02** Export finished | **03** Import confirm | **04** Import on the same card (nothing overwritten) |

| ![Import on a fresh card](24-stats-and-export/05-import-on-a-fresh-card.png) | ![Home after import (progress restored)](24-stats-and-export/06-home-after-import-progress-restored.png) |
|---|---|
| **05** Import on a fresh card | **06** Home after import (progress restored) |

## 25. Upgrade from CrossInk (data folder moved)

| ![Home after migration](25-upgrade-from-crossink/01-home-after-migration.png) | ![Saved items after migration](25-upgrade-from-crossink/02-saved-items-after-migration.png) |
|---|---|
| **01** Home after migration | **02** Saved items after migration |

