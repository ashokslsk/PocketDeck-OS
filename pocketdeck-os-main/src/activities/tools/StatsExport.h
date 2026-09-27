#pragma once

#include <cstdint>

// Stats & export: writes every feature's statistics as JSON under /stats,
// one folder per feature, and restores per-book reading data from it.
//
//   /stats/README.txt
//   /stats/habits/habits.json          streaks, rates, usual time, 12 weeks
//   /stats/medicine/medicine.json      every course with adherence and doses
//   /stats/mood/mood.json              daily moods, notes, averages
//   /stats/pomodoro/pomodoro.json      focus sessions per day
//   /stats/today/today.json            to-dos done per day
//   /stats/flashcards/flashcards.json  study sessions per deck and day
//   /stats/reading/library.json        whole-library summary
//   /stats/reading/global.json         all-books reading totals and streaks
//   /stats/reading/books/<book>.json   one file per book: progress, time,
//                                      bookmarks, clippings, looked-up words,
//                                      plus a "restore" block of the reader's
//                                      own files for moving to another device
//
// Everything is streamed to the card; nothing is loaded whole.
namespace statsx {

enum class Feature : uint8_t { Habits, Medicine, Mood, Pomodoro, Today, Flashcards, Reading, Count };

struct FeatureResult {
  bool ok = false;
  uint16_t items = 0;  // habits, courses, days, sessions or books written
};

struct ExportReport {
  FeatureResult features[static_cast<int>(Feature::Count)];
  bool clockValid = false;
};

// Progress callback between features (feature just finished), so the UI can
// repaint; may be null.
using ProgressFn = void (*)(Feature done, void* ctx);
ExportReport exportAll(ProgressFn progress, void* ctx);

struct ImportReport {
  uint16_t books = 0;     // book files found in /stats/reading/books
  uint16_t restored = 0;  // books that got at least one file back
  uint16_t kept = 0;      // files skipped because this device already has them
  uint16_t missing = 0;   // books not found at their saved path
  uint16_t failed = 0;
};

// Restores reading data (progress, stats, bookmarks, clippings, look-ups) for
// every exported book that is on this SD card at the same path, without
// overwriting anything this device already has.
ImportReport importBooks();

const char* featureFolder(Feature f);

}  // namespace statsx
