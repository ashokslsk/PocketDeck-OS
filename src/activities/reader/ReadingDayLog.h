#pragma once

#include <array>
#include <cstdint>

#include "ReadingStatsUtils.h"

// PocketDeck-OS: reading time per day for the last two weeks, split by time
// of day (Morning, Afternoon, Evening, Night), plus pages turned, all books.
// Kept in its own 288-byte file, /.pocketdeck-os/reading_days.bin, written
// once per reading session and read only by the weekly sleep stats page.
namespace readingdaylog {

constexpr int kDays = 14;
struct Day {
  uint32_t seconds[READING_TIME_BUCKET_COUNT];
  uint16_t pages;
  uint16_t reserved;
};
struct Log {
  uint32_t lastDay = 0;           // readingStatsDayIndex of entry 0 (0 = empty log)
  std::array<Day, kDays> days{};  // [0] = lastDay, [1] = the day before, ...
  // The entry for a day index, or nullptr when it is outside the window.
  const Day* find(uint32_t dayIndex) const;
};

Log load();
// Adds a session (split at period edges and midnight) and saves.
void record(const ReadingStatsDateTime& localStart, uint32_t seconds, uint32_t pages);

}  // namespace readingdaylog
