#include "HabitSummary.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ToolsCharts.h"
#include "ToolsLog.h"

namespace habits {

namespace {
struct TimeScan {
  const char* name;
  int32_t firstDay;
  int16_t* minutes;  // `days` entries, oldest first
  int days;
};

void onLogLine(const tlog::Stamp& at, char* fields, void* ctx) {
  auto* t = static_cast<TimeScan*>(ctx);
  char* cursor = fields;
  const char* target = tlog::nextField(&cursor);
  const char* name = tlog::nextField(&cursor);
  const char* value = tlog::nextField(&cursor);
  if (target == nullptr || name == nullptr || value == nullptr || strcmp(name, t->name) != 0) return;
  int32_t day = 0;
  if (!tools::parseIsoDate(target, day) || day != at.day) return;  // catch-up ticks carry no time of day
  const int idx = day - t->firstDay;
  if (idx < 0 || idx >= t->days) return;
  t->minutes[idx] = value[0] == '1' ? at.minute : charts::kNoValue;
}
}  // namespace

void summarize(const char* name, const int32_t today, Summary& out) {
  out = Summary{};
  Names one = {};
  snprintf(one[0], kNameCap, "%s", name);
  constexpr int kStreakWeeks = 53;
  const int32_t thisMonday = mondayOf(today);
  int run = 0;
  int weekdayDone[7] = {};
  // Oldest week first so runs accumulate in time order.
  for (int w = kStreakWeeks - 1; w >= 0; --w) {
    const int32_t monday = thisMonday - 7 * w;
    uint8_t bits[kMaxHabits] = {};
    loadWeek(monday, one, 1, bits);
    int done = 0;
    int elapsed = 0;
    for (int d = 0; d < 7; ++d) {
      const int32_t day = monday + d;
      if (day > today) break;
      ++elapsed;
      if ((bits[0] >> d) & 1) {
        ++done;
        ++run;
        out.best = std::max(out.best, run);
        if (today - day < 30) ++out.done30;
        if (w < Summary::kWeeks) ++weekdayDone[d];
      } else if (day != today) {
        run = 0;  // today still in progress does not break a streak
      }
    }
    if (w < Summary::kWeeks) {
      const int slot = Summary::kWeeks - 1 - w;
      out.weekPct[slot] = static_cast<int16_t>(elapsed > 0 ? done * 100 / elapsed : 0);
      out.weekBits[slot] = bits[0];
      out.doneWeeks += done;
    }
  }
  out.current = run;

  std::fill(out.checkMinute, out.checkMinute + Summary::kTimeDays, charts::kNoValue);
  TimeScan scan{name, today - (Summary::kTimeDays - 1), out.checkMinute, Summary::kTimeDays};
  tlog::scan("habits", scan.firstDay, today, &onLogLine, &scan);
  long sum = 0;
  int n = 0;
  for (const int16_t m : out.checkMinute) {
    if (m == charts::kNoValue) continue;
    sum += m;
    ++n;
  }
  if (n > 0) out.avgMinute = static_cast<int>(sum / n);
  long dev = 0;
  long gapSum = 0;
  int gaps = 0;
  long prevAbs = -1;
  for (int i = 0; i < Summary::kTimeDays; ++i) {
    const int16_t m = out.checkMinute[i];
    if (m == charts::kNoValue) continue;
    dev += std::labs(m - out.avgMinute);
    const long abs = static_cast<long>(i) * 1440 + m;
    if (prevAbs >= 0) {
      gapSum += abs - prevAbs;
      ++gaps;
    }
    prevAbs = abs;
  }
  if (n >= 2) out.spreadMinutes = static_cast<int>(dev / n);
  if (gaps > 0) out.avgGapMinutes = static_cast<int>(gapSum / gaps);
  const int bestDay = static_cast<int>(std::max_element(weekdayDone, weekdayDone + 7) - weekdayDone);
  if (weekdayDone[bestDay] > 0) out.bestWeekday = bestDay;
}

}  // namespace habits
