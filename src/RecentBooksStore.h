#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>
#include <vector>

struct RecentBook {
  // Unknown failures stay retryable; Missing is set only after EPUB metadata has no cover image.
  enum class CoverState : uint8_t { Unknown = 0, Missing = 1 };

  std::string path;
  std::string title;
  std::string author;
  std::string coverBmpPath;
  CoverState coverState = CoverState::Unknown;

  bool operator==(const RecentBook& other) const { return path == other.path; }
};

class RecentBooksStore : public PersistableStore<RecentBooksStore> {
 private:
  std::vector<RecentBook> recentBooks;

  // The most recent books stay in RAM (Home, sleep screens, quick lookups);
  // older ones, up to MAX_TOTAL_BOOKS in all, live one per line in
  // getHistoryPath() on the SD card and are read only by the Recent Books list.
 public:
  static constexpr int MAX_RECENT_BOOKS = 10;
  static constexpr int MAX_TOTAL_BOOKS = 100;
  static constexpr int MAX_HISTORY_BOOKS = MAX_TOTAL_BOOKS - MAX_RECENT_BOOKS;

 private:
  // Streams the history file into a new copy: `prepend` (if any) first, the
  // entry for `dropPath` left out, the entry for `renameFrom` repointed via
  // `renameTo`, and (if `popFirst` is set) the first remaining entry moved out
  // into it. Returns true when dropPath / renameFrom was found or popFirst filled.
  bool rewriteHistory(const RecentBook* prepend, const std::string& dropPath, RecentBook* popFirst,
                      const std::string& renameFrom = std::string(), const RecentBook* renameTo = nullptr);

  RecentBooksStore() = default;
  ~RecentBooksStore() = default;
  bool loadFromBinaryFile();
  // Entries past MAX_RECENT_BOOKS found in recent.json (older builds kept
  // more in RAM); moved to the SD history after loading.
  std::vector<RecentBook> overflow_;

  friend class PersistableStore<RecentBooksStore>;

 public:
  static const char* getFilePath() { return "/.pocketdeck-os/recent.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);
  bool saveToFile() const;
  bool loadFromFile();

  // Add a new book to the front, or refresh an existing entry and promote it
  // to the front.
  void addOrUpdateBook(const std::string& path, const std::string& title, const std::string& author,
                       const std::string& coverBmpPath,
                       RecentBook::CoverState coverState = RecentBook::CoverState::Unknown);

  // updateBook updates metadata for an existing book only and must not change
  // recent-books ordering. Use addOrUpdateBook when the touched book should
  // become most recent. Returns false if the book does not exist.
  [[nodiscard]] bool updateBook(const std::string& path, const std::string& title, const std::string& author,
                                const std::string& coverBmpPath, RecentBook::CoverState coverState);

  // Remove the entry whose path matches (used when a book is removed from recents or finished/read).
  // Returns true if an entry was found and removed (no-op + false otherwise).
  // Persistence is best-effort: a failed save is logged, not reflected in the return.
  bool removeByPath(const std::string& path);

  // Repoint an entry's path (and coverBmpPath, if it lived under the old cache dir) after the
  // backing file and cache dir were moved on disk. No-op if no entry matches oldPath.
  // Persists on success. Keeps the entry's list position (does not reorder).
  [[nodiscard]] bool updatePath(const std::string& oldPath, const std::string& newPath, const std::string& oldCachePath,
                                const std::string& newCachePath);

  // True if the book's backing file is no longer present on the SD card.
  static bool isMissing(const RecentBook& book);

  // Remove entries whose backing file is no longer on the SD card.
  // Returns true if any entry was removed. Does not persist — caller decides.
  bool pruneMissing();

  static const char* getHistoryPath() { return "/.pocketdeck-os/recent_more.txt"; }

  // One page of the full list, newest first: entries [offset, offset+count)
  // across the RAM entries and then the SD history. Returns true when more
  // entries follow the page. Allocates only the page; covers of history
  // entries are kept only when `keepCovers` is set.
  bool loadPage(std::vector<RecentBook>& out, size_t offset, size_t count, bool keepCovers = false) const;

  // Get the list of recent books (most recent first)
  const std::vector<RecentBook>& getBooks() const {
    ensureLoaded();
    return recentBooks;
  }

  // Get the count of recent books
  int getCount() const {
    ensureLoaded();
    return static_cast<int>(recentBooks.size());
  }

  RecentBook getDataFromBook(std::string path) const;
};

// Helper macro to access recent books store
#define RECENT_BOOKS RecentBooksStore::getInstance()
