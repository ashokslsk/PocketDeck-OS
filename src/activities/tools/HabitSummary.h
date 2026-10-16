#pragma once

#include <cstdint>

#include "HabitData.h"

// Numbers behind Habits > Stats and the /stats/habits export, computed from
// the weekly tick files and the tick-time log.
namespace habits {

struct Summary {
  static constexpr int kWeeks = 12;
  static constexpr int kTimeDays = 30;

  int current = 0;                      // run of done days ending today (or yesterday)
  int best = 0;                         // longest run in the last year
  int done30 = 0;                       // done days in the last 30
  int doneWeeks = 0;                    // done days in the last kWeeks weeks
  int16_t weekPct[kWeeks] = {};         // completion per week, oldest first
  uint8_t weekBits[kWeeks] = {};        // Monday..Sunday bits, oldest first
  int16_t checkMinute[kTimeDays] = {};  // same-day tick time, oldest first
  int avgMinute = -1;                   // usual tick time, -1 = unknown
  int spreadMinutes = -1;               // mean distance from usual time
  int avgGapMinutes = -1;               // mean time between check-ins
  int bestWeekday = -1;                 // 0 = Monday
};

void summarize(const char* name, int32_t today, Summary& out);

}  // namespace habits
