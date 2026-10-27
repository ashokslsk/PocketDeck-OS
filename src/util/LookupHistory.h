#pragma once

#include <string>
#include <vector>

// Per-book lookup history. Stored as <cachePath>/dictionary_history.txt.
// Format: one entry per line, "word|STATUS\n" where STATUS is a single char.
// Oldest entry at top; newest at bottom. Append-only, with no deduplication or
// automatic truncation. UI readers retain only the newest visible entries.
class LookupHistory {
 public:
  static constexpr int MAX_VISIBLE_ENTRIES = 50;

  enum class Status { Direct = 'D', Stem = 'T', AltForm = 'Y', Suggestion = 'S', NotFound = 'X' };

  struct Entry {
    std::string word;
    Status status = Status::NotFound;
  };

  // Append word+status without reading or rewriting existing history.
  static bool addWord(const std::string& cachePath, const std::string& word, Status status);

  // Conditional addWord: short-circuits if disabled, word empty, or cachePath empty.
  // Single guarded entry point used by all dictionary lookup recording sites.
  static void addWordIf(const std::string& cachePath, const std::string& word, Status status, bool enabled);

  // Load up to MAX_VISIBLE_ENTRIES in most-recent-first order.
  static std::vector<Entry> load(const std::string& cachePath);

  // Remove one of the visible entries by distance from newest (0 = newest).
  static bool removeRecentAt(const std::string& cachePath, int indexFromNewest);

  // Every lookup made while reading also lands in one global log,
  // /.pocketdeck-os/lookup_history_all.txt ("<book cache>|<YYYY-MM-DD HH:MM>|
  // <word>|S" per line, oldest first, oldest half dropped past 64 KB), so the
  // Dictionary tool can show words across all books with when they were looked
  // up. Only book caches are recorded; each book keeps its own history too.
  struct BookLookups {
    std::string cachePath;
    std::string title;  // from the cache's source.path (file name without extension)
    uint16_t words = 0;
  };
  // Books with lookups, most recently used first, plus the total word count.
  static std::vector<BookLookups> loadAllBooks(uint32_t& totalWords);
  struct TimedWord {
    std::string word;
    std::string when;  // "2026-10-23 14:03", empty if the clock was not set
  };
  // One book's looked-up words, newest first (at most `max`).
  static std::vector<TimedWord> loadBookWords(const std::string& cachePath, size_t max);

 private:
  static std::string filePath(const std::string& cachePath);
  static void appendGlobal(const std::string& cachePath, const std::string& word, Status status);
  static std::vector<Entry> readRecent(const std::string& path);
};
