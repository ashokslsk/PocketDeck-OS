#include "LibraryStats.h"

#include <Arduino.h>
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
constexpr uint16_t kMaxBooks = 1000;

bool isBook(const char* name) {
  const std::string n(name);
  return FsHelpers::hasEpubExtension(n) || FsHelpers::hasXtcExtension(n) || FsHelpers::checkFileExtension(n, ".txt");
}

std::string cachePathFor(const std::string& path) { return book_files::cachePathFor(path); }

// Saved progress 0..100, or -1 when the book has never been read on this
// device. EPUBs read the small cached percentage (or progress.bin), never the
// whole book: a prepared library has a cache folder for every book.
float savedProgress(const std::string& path) {
  RecentBook book;
  book.path = path;
  if (FsHelpers::hasEpubExtension(path)) return RecentBookProgress::loadCachedEpubPercent(book);
  // XTC/TXT: loading the book to compute a percentage is slow, so only books
  // with saved progress get that far; an unread one costs one exists() check.
  const std::string cache = cachePathFor(path);
  if (cache.empty() || !Storage.exists((cache + "/progress.bin").c_str())) return -1.0f;
  return RecentBookProgress::loadPercent(book);
}
}  // namespace

LibrarySummary LibrarySummary::scan() { return scan(nullptr, nullptr); }

LibrarySummary LibrarySummary::scan(const BookFn onOpenedBook, void* ctx) {
  LibraryScanner scanner;
  scanner.begin(onOpenedBook, ctx);
  while (!scanner.step(1000)) {
    yield();
  }
  return scanner.summary();
}

void LibraryScanner::begin(const LibrarySummary::BookFn onOpenedBook, void* ctx) {
  cancel();
  summary_ = LibrarySummary{};
  progressSum_ = 0;
  onBook_ = onOpenedBook;
  ctx_ = ctx;
  levels_[0].dir = Storage.open("/");
  levels_[0].path = "/";
  levels_[0].hasBooks = false;
  if (levels_[0].dir && levels_[0].dir.isDirectory()) {
    depth_ = 1;
    done_ = false;
  } else {
    if (levels_[0].dir) levels_[0].dir.close();
    LOG_ERR("LIB", "Library scan: cannot open card root");
    finish();
  }
}

void LibraryScanner::cancel() {
  while (depth_ > 0) {
    levels_[depth_ - 1].dir.close();
    --depth_;
  }
  done_ = true;
}

void LibraryScanner::popLevel() {
  Level& level = levels_[depth_ - 1];
  level.dir.close();
  if (level.hasBooks) ++summary_.folders;
  --depth_;
}

void LibraryScanner::visitBook(const std::string& path) {
  ++summary_.books;
  const float progress = savedProgress(path);
  if (progress < 0) {
    ++summary_.notOpened;
    return;
  }
  ++summary_.opened;
  progressSum_ += progress;
  if (onBook_ != nullptr) onBook_(path.c_str(), progress, ctx_);
  const std::string cache = cachePathFor(path);
  const BookReadingStats stats = cache.empty() ? BookReadingStats{} : BookReadingStats::load(cache);
  if (stats.isCompleted || progress >= 99.5f) {
    ++summary_.finished;
  } else if (progress > 0 || stats.totalPagesTurned > 0) {
    ++summary_.inProgress;
  }
}

bool LibraryScanner::step(const unsigned long budgetMs) {
  if (done_) return true;
  char name[128];
  const unsigned long started = millis();
  while (millis() - started < budgetMs) {
    if (depth_ == 0 || summary_.books >= kMaxBooks) {
      finish();
      return true;
    }
    Level& level = levels_[depth_ - 1];
    HalFile entry = level.dir.openNextFile();
    if (!entry) {
      popLevel();
      continue;
    }
    const bool isDir = entry.isDirectory();
    entry.getName(name, sizeof(name));
    if (name[0] == '.') {
      entry.close();
      continue;
    }
    std::string full = (level.path == "/" ? "/" : level.path + "/") + name;
    if (isDir) {
      if (full == "/tools" || depth_ > kMaxDepth) {
        entry.close();
        continue;
      }
      // Descend: the entry handle becomes the next level.
      Level& next = levels_[depth_++];
      next.dir = std::move(entry);
      next.path = std::move(full);
      next.hasBooks = false;
      continue;
    }
    entry.close();
    if (!isBook(name)) continue;
    level.hasBooks = true;
    visitBook(full);
  }
  return false;
}

void LibraryScanner::finish() {
  while (depth_ > 0) popLevel();
  done_ = true;
  LibrarySummary& s = summary_;
  s.averageProgress = s.opened > 0 ? static_cast<float>(progressSum_ / s.opened) : 0.0f;
  // "Currently reading" comes from recent books, which carry proper titles.
  s.currentCount = 0;
  for (const RecentBook& book : RECENT_BOOKS.getBooks()) {
    if (s.currentCount >= LibrarySummary::kMaxCurrent) break;
    const float progress = savedProgress(book.path);
    if (progress <= 0 || progress >= 99.5f) continue;
    LibrarySummary::Current& c = s.current[s.currentCount++];
    snprintf(c.title, sizeof(c.title), "%s", book.title.empty() ? book.path.c_str() : book.title.c_str());
    c.progress = progress;
  }
  s.scanned = true;
  LOG_INF("LIB", "Library: %u books in %u folders, %u opened, %u finished", s.books, s.folders, s.opened, s.finished);
}
