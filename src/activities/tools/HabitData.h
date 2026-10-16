#pragma once

#include <cstddef>
#include <cstdint>

#include "ToolsCommon.h"

// Habit tracker storage, split by ISO week so each file stays tiny:
//   /tools/habits/habits.txt      one habit name per line (1 to 12 lines)
//   /tools/habits/2026-W39.txt    "Name|1010000" per habit, Monday..Sunday
//   /tools/habits/log-2026-09.txt "2026-09-27 07:42|2026-09-27|Yoga|1" per tick
// Week files are matched to habits by name, so adding, removing or
// reordering lines in habits.txt never shifts anyone's history. The log
// records *when* each box was ticked (see ToolsLog.h) for time-of-day stats.
namespace habits {

constexpr int kMaxHabits = 12;
constexpr size_t kNameCap = 24;

using Names = char[kMaxHabits][kNameCap];

// Loads names (creating the file with defaults on first use); returns the count.
int loadNames(Names& names);
// bit d (0 = Monday) set when the habit was done that day. Missing file = all clear.
void loadWeek(int32_t mondayDay, const Names& names, int count, uint8_t (&bits)[kMaxHabits]);
bool saveWeek(int32_t mondayDay, const Names& names, int count, const uint8_t (&bits)[kMaxHabits]);

// Rewrites habits.txt with the given names (atomic).
bool saveNames(const Names& names, int count);
// Renames a habit in the last `weeks` week files so its history follows it.
void renameInHistory(int32_t today, const char* oldName, const char* newName, int weeks = 53);
// Records a tick (done=true) or untick for `day` in the monthly log.
void logToggle(int32_t day, const char* name, bool done);

inline int32_t mondayOf(const int32_t day) { return day - tools::weekdayMon0(day); }
// How many habits are ticked on `day`, and how many habits exist.
int countDoneOn(int32_t day, int* habitCount = nullptr);

}  // namespace habits
