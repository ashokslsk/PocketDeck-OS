#include "ReadingDayLog.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

namespace readingdaylog {
namespace {
constexpr char kPath[] = "/.pocketdeck-os/reading_days.bin";
constexpr uint32_t kMagic = 0x31445052;  // "RPD1"

Day* entryFor(Log& log, const uint32_t dayIndex) {
  if (log.lastDay == 0 || dayIndex > log.lastDay) {
    // Newer day: shift the window so dayIndex becomes entry 0.
    const uint32_t shift = log.lastDay == 0 ? kDays : dayIndex - log.lastDay;
    if (shift >= static_cast<uint32_t>(kDays)) {
      log.days.fill(Day{});
    } else {
      for (int i = kDays - 1; i >= 0; --i) {
        log.days[i] = i >= static_cast<int>(shift) ? log.days[i - shift] : Day{};
      }
    }
    log.lastDay = dayIndex;
  }
  const uint32_t back = log.lastDay - dayIndex;
  return back < static_cast<uint32_t>(kDays) ? &log.days[back] : nullptr;
}
}  // namespace

const Day* Log::find(const uint32_t dayIndex) const {
  if (lastDay == 0 || dayIndex > lastDay || lastDay - dayIndex >= static_cast<uint32_t>(kDays)) return nullptr;
  return &days[lastDay - dayIndex];
}

Log load() {
  Log log;
  HalFile f;
  if (!Storage.exists(kPath) || !Storage.openFileForRead("RDAY", kPath, f)) return log;
  uint32_t magic = 0;
  const bool ok = f.read(&magic, sizeof(magic)) == sizeof(magic) && magic == kMagic &&
                  f.read(&log.lastDay, sizeof(log.lastDay)) == sizeof(log.lastDay) &&
                  f.read(log.days.data(), sizeof(log.days)) == static_cast<int>(sizeof(log.days));
  f.close();
  if (!ok) {
    LOG_ERR("RDAY", "Ignoring unreadable %s", kPath);
    return Log{};
  }
  return log;
}

void record(const ReadingStatsDateTime& localStart, const uint32_t seconds, const uint32_t pages) {
  if (!localStart.isValid() || (seconds == 0 && pages == 0)) return;
  Log log = load();
  if (Day* first = entryFor(log, readingStatsDayIndex(localStart.date))) {
    const uint32_t sum = first->pages + pages;
    first->pages = static_cast<uint16_t>(sum > 0xFFFF ? 0xFFFF : sum);
  }
  ReadingStatsDateTime cursor = localStart;
  uint32_t remaining = seconds;
  while (remaining > 0) {
    const int hour = cursor.hour;
    const int bucket = hour < 5 ? 3 : hour < 12 ? 0 : hour < 17 ? 1 : hour < 21 ? 2 : 3;
    const uint32_t nextEdge = hour < 5 ? 5 : hour < 12 ? 12 : hour < 17 ? 17 : hour < 21 ? 21 : 24;
    const uint32_t now = static_cast<uint32_t>(hour) * 3600u + cursor.minute * 60u + cursor.second;
    const uint32_t toEdge = nextEdge * 3600u > now ? nextEdge * 3600u - now : 1;
    const uint32_t span = remaining < toEdge ? remaining : toEdge;
    if (Day* day = entryFor(log, readingStatsDayIndex(cursor.date))) day->seconds[bucket] += span;
    remaining -= span;
    addSecondsToReadingStatsDateTime(cursor, span);
  }
  Storage.mkdir("/.pocketdeck-os");
  HalFile f;
  if (!Storage.openFileForWrite("RDAY", kPath, f)) {
    LOG_ERR("RDAY", "Cannot write %s", kPath);
    return;
  }
  f.write(&kMagic, sizeof(kMagic));
  f.write(&log.lastDay, sizeof(log.lastDay));
  f.write(log.days.data(), sizeof(log.days));
  f.close();
}

}  // namespace readingdaylog
