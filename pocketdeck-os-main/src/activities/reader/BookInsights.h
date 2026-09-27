#pragma once

#include <cstdint>
#include <string>

// PocketDeck-OS: small per-book facts that live outside BookReadingStats —
// how many bookmarks, clippings (saved highlights) and dictionary look-ups a
// book has. Every value is read from a file header or by counting lines, so
// nothing large is loaded; Home caches the result for the selected book.
struct BookInsights {
  uint16_t bookmarks = 0;
  uint16_t clippings = 0;
  uint16_t lookups = 0;
  bool valid = false;

  static BookInsights load(const std::string& bookPath);
};

struct BookReadingStats;
struct ReadingStatsDate;

// Reading time still needed: the reader's live estimate when it has one,
// otherwise time read so far scaled by the unread fraction.
bool bookTimeLeftSeconds(const BookReadingStats& stats, float progressPercent, uint32_t& seconds);
// Expected finish date at the current daily pace (reading time per calendar
// day since the start date); the finished date for finished books.
bool bookFinishDate(const BookReadingStats& stats, float progressPercent, ReadingStatsDate& out);

namespace book_files {
// "epub", "xtc" or "txt" (the stores' book type), or "" for other files.
const char* typeFor(const std::string& bookPath);
// The book's reader cache folder under /.pocketdeck-os (may not exist yet).
std::string cachePathFor(const std::string& bookPath);
}  // namespace book_files
