# Sample SD card data for Home > Tools

Copy the `tools` folder (and, if you like, `sleep`) to the **root** of the reader's SD card:

```
SD card
└── tools/
    ├── fonts/kannada.knf     Kannada font for Panchanga, Mantras, Kannada books and your files (22.0 MB)
    ├── panchanga/festivals/  karnataka-1976-2075.json festival and holiday calendar
    ├── mantras/mantras.json  mantras: 35 deities, daily and ritual, kavacha
    ├── workout/              Workout: manifests/ (4 categories, 40 practices) and 67 figures
    ├── quotes.txt            461 dated quotes (2026-09-27 … 2027-12-31)
    ├── daily.json            starter to-do list
    ├── pomodoro.txt          focus/break lengths (25/5)
    ├── worldclock.txt        New York, London, Tokyo, Sydney
    ├── habits/habits.txt     habit names (up to 12; add more on the device)
    ├── knowledge/            4 topics: AI, Python, SQL, Panchanga_Kannada (a Kannada example)
    └── flashcards/           3 decks: ai_concepts, python_basics, sql_basics
fonts/                        NotoSansCJK reading font for Japanese/Chinese/Korean books (copy to /fonts)
books/                        Surya_Namaskara_Kannada.epub and test/ language test books
└── sleep/                    3 example wallpapers (2 BMP, 1 JPG) for the
                              Custom sleep screen and "Change wallpaper"
```

Medicine, Mood, Pomodoro, Today, Flashcards, Knowledge, Daily Quote and Workout create their own history files
under `/tools/<feature>/` as you use them; **Tools > Stats & export** writes
the statistics to `/stats`. The full routine for adding your own content is in
[INSTALLATION_GUIDE.md](../INSTALLATION_GUIDE.md).

Every file is optional and can be edited in any text editor. Formats and button
controls are described in [docs/productivity-tools.md](../docs/productivity-tools.md).
To regenerate `quotes.txt`, run `scripts/tools/extract_quotes.py` and then
`scripts/tools/build_quotes.py`.
