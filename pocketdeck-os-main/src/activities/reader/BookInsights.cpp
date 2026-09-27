#include "BookInsights.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Txt.h>
#include <Xtc.h>

#include <algorithm>

#include "BookReadingStats.h"
#include "BookmarkStore.h"
#include "ClippingStore.h"

namespace book_files {

const char* typeFor(const std::string& bookPath) {
  if (FsHelpers::hasEpubExtension(bookPath)) return "epub";
  if (FsHelpers::hasXtcExtension(bookPath)) return "xtc";
  if (FsHelpers::checkFileExtension(bookPath, ".txt")) return "txt";
  return "";
}

std::string cachePathFor(const std::string& bookPath) {
  if (FsHelpers::hasEpubExtension(bookPath)) return Epub::cachePathForFilePath(bookPath, "/.pocketdeck-os");
  if (FsHelpers::hasXtcExtension(bookPath)) return Xtc(bookPath, "/.pocketdeck-os").getCachePath();
  if (FsHelpers::checkFileExtension(bookPath, ".txt")) return Txt(bookPath, "/.pocketdeck-os").getCachePath();
  return {};
}

}  // namespace book_files

BookInsights BookInsights::load(const std::string& bookPath) {
  BookInsights out;
  const std::string type = book_files::typeFor(bookPath);
  if (type.empty()) return out;
  out.bookmarks = BookmarkStore::countForBook(bookPath, type);
  out.clippings = ClippingStore::countForBook(bookPath, type);
  // Look-ups: one line per word in <cache>/dictionary_history.txt.
  const std::string history = book_files::cachePathFor(bookPath) + "/dictionary_history.txt";
  FsFile f;
  if (Storage.exists(history.c_str()) && Storage.openFileForRead("INS", history, f)) {
    uint8_t buf[64];
    int n = 0;
    uint32_t lines = 0;
    while ((n = f.read(buf, sizeof(buf))) > 0) {
      for (int i = 0; i < n; ++i) lines += buf[i] == '\n' ? 1 : 0;
    }
    f.close();
    out.lookups = static_cast<uint16_t>(lines > 0xFFFF ? 0xFFFF : lines);
  }
  out.valid = true;
  return out;
}

bool bookTimeLeftSeconds(const BookReadingStats& stats, const float progressPercent, uint32_t& seconds) {
  seconds = 0;
  if (stats.estimatedTimeLeftSeconds > 0) {
    seconds = stats.estimatedTimeLeftSeconds;
    return true;
  }
  if (progressPercent <= 0.0f || progressPercent >= 100.0f || stats.totalReadingSeconds < 120) return false;
  const float progress = progressPercent / 100.0f;
  seconds = static_cast<uint32_t>(static_cast<float>(stats.totalReadingSeconds) * (1.0f - progress) / progress + 0.5f);
  return seconds > 0;
}

bool bookFinishDate(const BookReadingStats& stats, const float progressPercent, ReadingStatsDate& out) {
  out = {};
  if (stats.isCompleted) {
    out = stats.finishedDate;
    return out.isValid();
  }
  uint32_t left = 0;
  ReadingStatsDateTime today;
  if (!bookTimeLeftSeconds(stats, progressPercent, left) || !getCurrentLocalReadingStatsDateTime(today)) return false;
  ReadingStatsDateTime finish = today;
  if (stats.startDate.isValid() && stats.totalReadingSeconds > 0) {
    // Calendar time = reading time left x (calendar days so far / reading time so far).
    const uint16_t days = std::max<uint16_t>(1, readingSpanDaysElapsed(stats.startDate, today.date));
    const uint64_t calendar = (static_cast<uint64_t>(left) * days * 86400ULL) / stats.totalReadingSeconds;
    addSecondsToReadingStatsDateTime(finish, static_cast<uint32_t>(std::min<uint64_t>(calendar, UINT32_MAX)));
  } else {
    addSecondsToReadingStatsDateTime(finish, left);
  }
  out = finish.date;
  return out.isValid();
}
