# PocketDeck-OS screenshot walkthrough

Every screen of PocketDeck-OS 1.0.0, captured from the CrossInk simulator with the **Xteink X3** profile (792x528 panel, portrait). The library is public-domain books from Project Gutenberg, the dictionary is built from Princeton WordNet 3.0, and tool data comes from `sd-sample/tools` plus a few seeded days of habits and to-dos. Clocks are set to UTC+5:30 (Bengaluru).

Captured headlessly with the simulator's `CROSSPOINT_SIM_INPUT_SCRIPT` / `CROSSPOINT_SIM_SCREENSHOTS` variables (`SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software`). The simulator-only `CROSSINK_SIMULATOR_START_SCREEN=boot|tools|settings` opens a screen directly.

![Overview](overview.png)

## Contents

- [01. Boot and sleep screens](#01-boot-and-sleep-screens) (9)
- [02. Home and library](#02-home-and-library) (6)
- [03. Reader and dictionary](#03-reader-and-dictionary) (8)
- [04. Bookmarks and clippings](#04-bookmarks-and-clippings) (14)
- [05. Reading stats and library dashboard](#05-reading-stats-and-library-dashboard) (3)
- [06. File transfer](#06-file-transfer) (1)
- [07. Settings and About](#07-settings-and-about) (8)
- [08. UI themes](#08-ui-themes) (24)
- [09. Tools launcher](#09-tools-launcher) (1)
- [10. Pomodoro](#10-pomodoro) (9)
- [11. World Clock](#11-world-clock) (2)
- [12. Habit Tracker](#12-habit-tracker) (5)
- [13. Flashcards](#13-flashcards) (10)
- [14. Daily Quote](#14-daily-quote) (6)
- [15. Knowledge](#15-knowledge) (9)
- [16. Panchanga (Kannada)](#16-panchanga-kannada) (5)
- [17. Today](#17-today) (4)


## 01. Boot and sleep screens

| ![Boot splash](01-boot-and-sleep/01-boot-splash.png) | ![Sleep: PocketDeck-OS (default)](01-boot-and-sleep/02-sleep-pocketdeck-os-default.png) | ![Sleep: PocketDeck-OS dark](01-boot-and-sleep/03-sleep-pocketdeck-os-dark.png) | ![Sleep: Page with PocketDeck-OS card](01-boot-and-sleep/04-sleep-page-with-pocketdeck-os-card.png) |
|---|---|---|---|
| **01** Boot splash | **02** Sleep: PocketDeck-OS (default) | **03** Sleep: PocketDeck-OS dark | **04** Sleep: Page with PocketDeck-OS card |

| ![Sleep: Book cover](01-boot-and-sleep/05-sleep-book-cover.png) | ![Sleep: Reading stats](01-boot-and-sleep/06-sleep-reading-stats.png) | ![Sleep: Minimal](01-boot-and-sleep/07-sleep-minimal.png) | ![Sleep: Minimal stats](01-boot-and-sleep/08-sleep-minimal-stats.png) |
|---|---|---|---|
| **05** Sleep: Book cover | **06** Sleep: Reading stats | **07** Sleep: Minimal | **08** Sleep: Minimal stats |

| ![Sleep: Dashboard](01-boot-and-sleep/09-sleep-dashboard.png) |
|---|
| **09** Sleep: Dashboard |

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

| ![RoundedRaff: home](08-themes/13-roundedraff-home.png) | ![RoundedRaff: tools](08-themes/14-roundedraff-tools.png) | ![RoundedRaff: settings](08-themes/15-roundedraff-settings.png) | ![Lyra Carousel: home](08-themes/16-lyra-carousel-home.png) |
|---|---|---|---|
| **13** RoundedRaff: home | **14** RoundedRaff: tools | **15** RoundedRaff: settings | **16** Lyra Carousel: home |

| ![Lyra Carousel: tools](08-themes/17-lyra-carousel-tools.png) | ![Lyra Carousel: settings](08-themes/18-lyra-carousel-settings.png) | ![Minimal: home](08-themes/19-minimal-home.png) | ![Minimal: tools](08-themes/20-minimal-tools.png) |
|---|---|---|---|
| **17** Lyra Carousel: tools | **18** Lyra Carousel: settings | **19** Minimal: home | **20** Minimal: tools |

| ![Minimal: settings](08-themes/21-minimal-settings.png) | ![Dashboard: home](08-themes/22-dashboard-home.png) | ![Dashboard: tools](08-themes/23-dashboard-tools.png) | ![Dashboard: settings](08-themes/24-dashboard-settings.png) |
|---|---|---|---|
| **21** Minimal: settings | **22** Dashboard: home | **23** Dashboard: tools | **24** Dashboard: settings |

## 09. Tools launcher

| ![Tools menu](09-tools-launcher/01-tools-menu.png) |
|---|
| **01** Tools menu |

## 10. Pomodoro

| ![Ready](10-pomodoro/01-ready.png) | ![Running](10-pomodoro/02-running.png) | ![Running seconds](10-pomodoro/03-running-seconds.png) | ![Paused](10-pomodoro/04-paused.png) |
|---|---|---|---|
| **01** Ready | **02** Running | **03** Running seconds | **04** Paused |

| ![Reset](10-pomodoro/05-reset.png) | ![Break phase](10-pomodoro/06-break-phase.png) | ![Ring quarter](10-pomodoro/07-ring-quarter.png) | ![Ring half](10-pomodoro/08-ring-half.png) |
|---|---|---|---|
| **05** Reset | **06** Break phase | **07** Ring quarter | **08** Ring half |

| ![Ring three quarters](10-pomodoro/09-ring-three-quarters.png) |
|---|
| **09** Ring three quarters |

## 11. World Clock

| ![World clock](11-world-clock/01-world-clock.png) | ![Sync wifi](11-world-clock/02-sync-wifi.png) |
|---|---|
| **01** World clock | **02** Sync wifi |

## 12. Habit Tracker

| ![Eight habits](12-habit-tracker/01-eight-habits.png) | ![Ticked today](12-habit-tracker/02-ticked-today.png) | ![Yoga ticked](12-habit-tracker/03-yoga-ticked.png) | ![Workout ticked](12-habit-tracker/04-workout-ticked.png) |
|---|---|---|---|
| **01** Eight habits | **02** Ticked today | **03** Yoga ticked | **04** Workout ticked |

| ![Previous week](12-habit-tracker/05-previous-week.png) |
|---|
| **05** Previous week |

## 13. Flashcards

| ![Decks](13-flashcards/01-decks.png) | ![Question](13-flashcards/02-question.png) | ![Answer](13-flashcards/03-answer.png) | ![Next question](13-flashcards/04-next-question.png) |
|---|---|---|---|
| **01** Decks | **02** Question | **03** Answer | **04** Next question |

| ![Answer](13-flashcards/05-answer.png) | ![Forgot card returns](13-flashcards/06-forgot-card-returns.png) | ![Session complete](13-flashcards/07-session-complete.png) | ![Deck list again](13-flashcards/08-deck-list-again.png) |
|---|---|---|---|
| **05** Answer | **06** Forgot card returns | **07** Session complete | **08** Deck list again |

| ![Sql question](13-flashcards/09-sql-question.png) | ![Sql answer](13-flashcards/10-sql-answer.png) |
|---|---|
| **09** Sql question | **10** Sql answer |

## 14. Daily Quote

| ![Todays quote](14-daily-quote/01-todays-quote.png) | ![Long quote page 1](14-daily-quote/02-long-quote-page-1.png) | ![Long quote page 2](14-daily-quote/03-long-quote-page-2.png) | ![Next day](14-daily-quote/04-next-day.png) |
|---|---|---|---|
| **01** Todays quote | **02** Long quote page 1 | **03** Long quote page 2 | **04** Next day |

| ![Taoist quote](14-daily-quote/05-taoist-quote.png) | ![Shuffle](14-daily-quote/06-shuffle.png) |
|---|---|
| **05** Taoist quote | **06** Shuffle |

## 15. Knowledge

| ![Topics](15-knowledge/01-topics.png) | ![Question of the day](15-knowledge/02-question-of-the-day.png) | ![Question](15-knowledge/03-question.png) | ![Answer page 1](15-knowledge/04-answer-page-1.png) |
|---|---|---|---|
| **01** Topics | **02** Question of the day | **03** Question | **04** Answer page 1 |

| ![Answer page 2](15-knowledge/05-answer-page-2.png) | ![Answer page 3](15-knowledge/06-answer-page-3.png) | ![Next question](15-knowledge/07-next-question.png) | ![Answer](15-knowledge/08-answer.png) |
|---|---|---|---|
| **05** Answer page 2 | **06** Answer page 3 | **07** Next question | **08** Answer |

| ![Topics again](15-knowledge/09-topics-again.png) |
|---|
| **09** Topics again |

## 16. Panchanga (Kannada)

| ![Today](16-panchanga/01-today.png) | ![Next day](16-panchanga/02-next-day.png) | ![Deepavali 2026](16-panchanga/03-deepavali-2026.png) | ![Ugadi 2027](16-panchanga/04-ugadi-2027.png) |
|---|---|---|---|
| **01** Today | **02** Next day | **03** Deepavali 2026 | **04** Ugadi 2027 |

| ![Back to today](16-panchanga/05-back-to-today.png) |
|---|
| **05** Back to today |

## 17. Today

| ![Today](17-today/01-today.png) | ![Task completed](17-today/02-task-completed.png) | ![Add task row](17-today/03-add-task-row.png) | ![New task keyboard](17-today/04-new-task-keyboard.png) |
|---|---|---|---|
| **01** Today | **02** Task completed | **03** Add task row | **04** New task keyboard |

