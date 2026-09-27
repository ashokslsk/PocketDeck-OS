# Sample SD card data for Home > Tools

Copy the `tools` folder (and, if you like, `sleep`) to the **root** of the reader's SD card:

```
SD card
└── tools/
    ├── quotes.txt            461 dated quotes (2026-09-27 … 2027-12-31)
    ├── daily.json            starter to-do list
    ├── pomodoro.txt          focus/break lengths (25/5)
    ├── worldclock.txt        New York, London, Tokyo, Sydney
    ├── habits/habits.txt     habit names (up to 12; add more on the device)
    ├── knowledge/            3 topics: AI.json, Python.json, SQL.json (13 questions)
    └── flashcards/           3 decks: ai_concepts, python_basics, sql_basics
└── sleep/                    3 example wallpapers (2 BMP, 1 JPG) for the
                              Custom sleep screen and "Change wallpaper"
```

Medicine, Mood, Pomodoro, Today and Flashcards create their own history files
under `/tools/<feature>/` as you use them; **Tools > Stats & export** writes
the statistics to `/stats`. The full routine for adding your own content is in
[INSTALLATION_GUIDE.md](../INSTALLATION_GUIDE.md).

Every file is optional and can be edited in any text editor. Formats and button
controls are described in [docs/productivity-tools.md](../docs/productivity-tools.md).
To regenerate `quotes.txt`, run `scripts/tools/extract_quotes.py` and then
`scripts/tools/build_quotes.py`.
