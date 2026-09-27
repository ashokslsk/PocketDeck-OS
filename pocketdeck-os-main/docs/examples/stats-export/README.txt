PocketDeck-OS statistics export
===============================

Created by Tools > Stats & export > Export all. Every run replaces these files.

habits/habits.json          streaks, completion, usual check-in time, 12 weeks
medicine/medicine.json      courses: start, end, stop, doses taken and when
mood/mood.json              daily moods (1-5) with averages
pomodoro/pomodoro.json      focus sessions per day
today/today.json            to-dos done per day
flashcards/flashcards.json  study sessions per deck and day
reading/library.json        books on the card, opened, finished
reading/global.json         total reading time, sessions, streaks
reading/books/*.json        one file per book: progress, time read,
                            bookmarks, clippings, looked-up words

Moving books to another PocketDeck-OS device: copy the book to the same
folder, copy this /stats folder, then run Tools > Stats & export >
Import book data. Existing data on that device is never overwritten.
The raw history the stats come from lives in /tools/<feature>/log-*.txt.
