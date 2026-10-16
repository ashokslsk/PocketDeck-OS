#include "ToolStatsData.h"

#include <HalStorage.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ToolsCharts.h"
#include "ToolsDate.h"
#include "ToolsStore.h"

namespace tstats {

void clearSeries(DaySeries& s, const int32_t today, const int16_t fill) {
  s.firstDay = today - (kHistoryDays - 1);
  std::fill(s.a, s.a + kHistoryDays, fill);
  std::fill(s.b, s.b + kHistoryDays, fill);
  std::fill(s.minute, s.minute + kHistoryDays, charts::kNoValue);
}

// mood: a = mood 1..5 (latest wins), minute = time logged (same day)
void onMood(const tlog::Stamp& at, char* fields, void* p) {
  auto* s = static_cast<DaySeries*>(p);
  char* cursor = fields;
  const char* date = tlog::nextField(&cursor);
  const char* mood = tlog::nextField(&cursor);
  int32_t day = 0;
  if (date == nullptr || mood == nullptr || !tools::parseIsoDate(date, day)) return;
  const int idx = day - s->firstDay;
  const int m = atoi(mood);
  if (idx < 0 || idx >= kHistoryDays || m < 1 || m > 5) return;
  s->a[idx] = static_cast<int16_t>(m);
  s->minute[idx] = day == at.day ? at.minute : charts::kNoValue;
}

// pomodoro: a = sessions, b = focus minutes, minute = first session time
void onPomodoro(const tlog::Stamp& at, char* fields, void* p) {
  auto* s = static_cast<DaySeries*>(p);
  char* cursor = fields;
  const char* kind = tlog::nextField(&cursor);
  const char* minutes = tlog::nextField(&cursor);
  const int idx = at.day - s->firstDay;
  if (kind == nullptr || strcmp(kind, "focus") != 0 || idx < 0 || idx >= kHistoryDays) return;
  ++s->a[idx];
  s->b[idx] = static_cast<int16_t>(s->b[idx] + (minutes != nullptr ? atoi(minutes) : 0));
  if (s->minute[idx] == charts::kNoValue) s->minute[idx] = at.minute;
}

// today: a = done, b = total (for the day the list belonged to)
void onToday(const tlog::Stamp&, char* fields, void* p) {
  auto* s = static_cast<DaySeries*>(p);
  char* cursor = fields;
  const char* date = tlog::nextField(&cursor);
  const char* done = tlog::nextField(&cursor);
  const char* total = tlog::nextField(&cursor);
  int32_t day = 0;
  if (date == nullptr || done == nullptr || total == nullptr || !tools::parseIsoDate(date, day)) return;
  const int idx = day - s->firstDay;
  if (idx < 0 || idx >= kHistoryDays) return;
  s->a[idx] = static_cast<int16_t>(atoi(done));
  s->b[idx] = static_cast<int16_t>(atoi(total));
}

int averageMinute(const DaySeries& s) {
  long sum = 0;
  int n = 0;
  for (const int16_t m : s.minute) {
    if (m == charts::kNoValue) continue;
    sum += m;
    ++n;
  }
  return n > 0 ? static_cast<int>(sum / n) : -1;
}

void scanSeries(const char* feature, const int32_t today, const tlog::LineFn fn, DaySeries& s, const int16_t fill) {
  clearSeries(s, today, fill);
  tlog::scan(feature, s.firstDay, today, fn, &s);
}

// quotes: "open|YYYY-MM-DD" when the tool is opened
void onQuotes(const tlog::Stamp& at, char* fields, void* p) {
  auto* s = static_cast<DaySeries*>(p);
  char* cursor = fields;
  const char* kind = tlog::nextField(&cursor);
  const int idx = at.day - s->firstDay;
  if (kind == nullptr || strcmp(kind, "open") != 0 || idx < 0 || idx >= kHistoryDays) return;
  s->a[idx] = 1;
  if (s->minute[idx] == charts::kNoValue) s->minute[idx] = at.minute;
}

// mantras: "count|category|mantra|text" for each japa session
void onMantras(const tlog::Stamp& at, char* fields, void* p) {
  auto* s = static_cast<DaySeries*>(p);
  char* cursor = fields;
  const char* count = tlog::nextField(&cursor);
  const int idx = at.day - s->firstDay;
  if (count == nullptr || idx < 0 || idx >= kHistoryDays) return;
  const int n = atoi(count);
  if (n <= 0) return;
  s->a[idx] = static_cast<int16_t>(std::min(32000, s->a[idx] + n));
  ++s->b[idx];
  if (s->minute[idx] == charts::kNoValue) s->minute[idx] = at.minute;
}

// workout: "category|id|seconds|count|reps|level|done" for each session
void onWorkout(const tlog::Stamp& at, char* fields, void* p) {
  auto* s = static_cast<DaySeries*>(p);
  char* cursor = fields;
  tlog::nextField(&cursor);
  tlog::nextField(&cursor);
  const char* seconds = tlog::nextField(&cursor);
  const int idx = at.day - s->firstDay;
  if (seconds == nullptr || idx < 0 || idx >= kHistoryDays) return;
  const int minutes = (std::max(0, atoi(seconds)) + 30) / 60;
  s->a[idx] = static_cast<int16_t>(std::min(32000, s->a[idx] + minutes));
  ++s->b[idx];
  if (s->minute[idx] == charts::kNoValue) s->minute[idx] = at.minute;
}

int sumLast(const int16_t* values, const int days) {
  int sum = 0;
  for (int i = kHistoryDays - days; i < kHistoryDays; ++i) {
    if (i >= 0 && values[i] > 0) sum += values[i];
  }
  return sum;
}

int streak(const int16_t* values) {
  int n = 0;
  for (int i = kHistoryDays - 1; i >= 0; --i) {
    if (values[i] <= 0) {
      if (i == kHistoryDays - 1) continue;  // nothing yet today does not break it
      break;
    }
    ++n;
  }
  return n;
}

uint32_t indexCount(const char* path) {
  FsFile f;
  if (!Storage.exists(path) || !Storage.openFileForRead("TST", path, f)) return 0;
  uint32_t header[3] = {};
  const bool ok = f.read(header, sizeof(header)) == sizeof(header);
  f.close();
  return ok ? header[2] : 0;
}

int countFavouriteQuotes() {
  FsFile f;
  if (!Storage.exists(kFavouritesPath) || !Storage.openFileForRead("TST", kFavouritesPath, f)) return 0;
  int n = 0;
  char line[8];
  while (tools::readLine(f, line, sizeof(line)) >= 0) n += line[0] != '\0' && line[0] != '#' ? 1 : 0;
  f.close();
  return n;
}

}  // namespace tstats
