# Sample SD card data for Home > Tools

Copy the `tools` folder to the **root** of the reader's SD card:

```
SD card
└── tools/
    ├── quotes.txt            461 dated quotes (2026-09-27 … 2027-12-31)
    ├── daily.json            starter to-do list
    ├── pomodoro.txt          focus/break lengths (25/5)
    ├── worldclock.txt        New York, London, Tokyo, Sydney
    ├── habits/habits.txt     habit names (up to 8)
    ├── knowledge/            3 topics: AI.json, Python.json, SQL.json (13 questions)
    └── flashcards/           3 decks: ai_concepts, python_basics, sql_basics
```

Every file is optional and can be edited in any text editor. Formats and button
controls are described in [docs/productivity-tools.md](../docs/productivity-tools.md).
To regenerate `quotes.txt`, run `scripts/tools/extract_quotes.py` and then
`scripts/tools/build_quotes.py`.
