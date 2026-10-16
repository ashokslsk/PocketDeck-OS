#include "LibraryStats.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Xtc.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "BookInsights.h"
#include "BookReadingStats.h"
#include "RecentBooksStore.h"
#include "activities/home/RecentBookProgress.h"

namespace {
constexpr int kMaxDepth = 6;
constexpr uint16_t kMaxBooks = 1000;

bool isBook(const char* name) {
  const std::string n(name);
  return FsHelpers::hasEpubExtension(n) || FsHelpers::hasXtcExtension(n) || FsHelpers::checkFileExtension(n, ".txt");
}

std::string cachePathFor(const std::string& path) { return book_files::cachePathFor(path); }

struct Totals {
  LibrarySummary* s;
  double progressSum;
  LibrarySummary::BookFn onBook;
  void* ctx;
};

void walk(const std::string& dirPath, const int depth, Totals& t) {
  if (depth > kMaxDepth || t.s->books >= kMaxBooks) return;
  FsFile dir = Storage.open(dirPath.c_str());
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return;
  }
  bool folderHasBooks = false;
  char name[128];
  // Collect subfolders first so only one directory handle is open at a time.
  // One small allocation per folder level; the scan runs once per visit.
  std::vector<std::string> subdirs;
  subdirs.reserve(8);
  for (FsFile e = dir.openNextFile(); e; e = dir.openNextFile()) {
    const bool isDir = e.isDirectory();
    e.getName(name, sizeof(name));
    e.close();
    if (name[0] == '.') continue;
    const std::string full = (dirPath == "/" ? "/" : dirPath + "/") + name;
    if (isDir) {
      if (full == "/tools" || subdirs.size() >= 32) continue;
      subdirs.push_back(full);
      continue;
    }
    if (!isBook(name) || t.s->books >= kMaxBooks) continue;
    folderHasBooks = true;
    ++t.s->books;
    const std::string cache = cachePathFor(full);
    if (cache.empty() || !Storage.exists(cache.c_str())) {
      ++t.s->notOpened;
      continue;
    }
    ++t.s->opened;
    const BookReadingStats stats = BookReadingStats::load(cache);
    RecentBook book;
    book.path = full;
    float progress = RecentBookProgress::loadPercent(book);
    if (progress < 0) progress = 0;
    t.progressSum += progress;
    if (t.onBook != nullptr) t.onBook(full.c_str(), progress, t.ctx);
    if (stats.isCompleted || progress >= 99.5f) {
      ++t.s->finished;
    } else if (progress > 0 || stats.totalPagesTurned > 0) {
      ++t.s->inProgress;
    }
  }
  dir.close();
  if (folderHasBooks) ++t.s->folders;
  for (const std::string& sub : subdirs) walk(sub, depth + 1, t);
}
}  // namespace

LibrarySummary LibrarySummary::scan() { return scan(nullptr, nullptr); }

LibrarySummary LibrarySummary::scan(const BookFn onOpenedBook, void* ctx) {
  LibrarySummary s;
  Totals t{&s, 0.0, onOpenedBook, ctx};
  walk("/", 0, t);
  s.averageProgress = s.opened > 0 ? static_cast<float>(t.progressSum / s.opened) : 0.0f;
  // "Currently reading" comes from recent books, which carry proper titles.
  for (const RecentBook& book : RECENT_BOOKS.getBooks()) {
    if (s.currentCount >= kMaxCurrent) break;
    const float progress = RecentBookProgress::loadPercent(book);
    if (progress <= 0 || progress >= 99.5f) continue;
    Current& c = s.current[s.currentCount++];
    snprintf(c.title, sizeof(c.title), "%s", book.title.empty() ? book.path.c_str() : book.title.c_str());
    c.progress = progress;
  }
  s.scanned = true;
  LOG_INF("LIB", "Library: %u books in %u folders, %u opened, %u finished", s.books, s.folders, s.opened, s.finished);
  return s;
}
