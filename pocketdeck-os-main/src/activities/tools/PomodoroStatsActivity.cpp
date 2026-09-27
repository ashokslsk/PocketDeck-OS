#include "PomodoroStatsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ToolsLog.h"
#include "fontIds.h"

namespace {
struct Scan {
  int32_t firstDay;
  int16_t* sessions;
  int16_t* minutes;
  int16_t* firstStart;
};

void onLine(const tlog::Stamp& at, char* fields, void* ctx) {
  auto* s = static_cast<Scan*>(ctx);
  char* cursor = fields;
  const char* kind = tlog::nextField(&cursor);
  const char* mins = tlog::nextField(&cursor);
  const int idx = at.day - s->firstDay;
  if (kind == nullptr || strcmp(kind, "focus") != 0 || idx < 0 || idx >= PomodoroStatsActivity::kDays) return;
  const int length = mins != nullptr ? atoi(mins) : 25;
  ++s->sessions[idx];
  s->minutes[idx] = static_cast<int16_t>(s->minutes[idx] + length);
  // The log is written when a focus ends; its start is `length` minutes earlier.
  const int start = std::max(0, at.minute - length);
  if (s->firstStart[idx] == charts::kNoValue || start < s->firstStart[idx]) {
    s->firstStart[idx] = static_cast<int16_t>(start);
  }
}
}  // namespace

void PomodoroStatsActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  tlog::Stamp now;
  clockValid_ = tlog::now(now);
  today_ = now.day;
  std::fill(firstStart_, firstStart_ + kDays, charts::kNoValue);
  if (clockValid_) {
    Scan scan{today_ - (kDays - 1), sessions_, minutes_, firstStart_};
    tlog::scan("pomodoro", scan.firstDay, today_, &onLine, &scan);
  }
  int today = sessions_[kDays - 1];
  int week = 0, month = 0, focusMinutes = 0, activeDays = 0, best = 0, bestIdx = -1;
  const int weekday = tools::weekdayMon0(today_);
  long startSum = 0;
  int startN = 0;
  for (int i = 0; i < kDays; ++i) {
    const int back = kDays - 1 - i;
    month += sessions_[i];
    focusMinutes += minutes_[i];
    if (back <= weekday) week += sessions_[i];
    if (sessions_[i] > 0) ++activeDays;
    if (sessions_[i] > best) {
      best = sessions_[i];
      bestIdx = i;
    }
    if (firstStart_[i] != charts::kNoValue) {
      startSum += firstStart_[i];
      ++startN;
    }
  }
  int streak = 0;
  for (int i = kDays - 1; i >= 0; --i) {
    if (sessions_[i] == 0) {
      if (i == kDays - 1) continue;  // no session yet today does not break it
      break;
    }
    ++streak;
  }
  int n = 0;
  auto put = [&](const char* label) { stats_[n++].label = label; };
  snprintf(stats_[n].value, sizeof(stats_[n].value), "%d", today);
  put(tr(STR_TOOLS_POMO_TODAY));
  snprintf(stats_[n].value, sizeof(stats_[n].value), "%d", week);
  put(tr(STR_TOOLS_POMO_WEEK));
  snprintf(stats_[n].value, sizeof(stats_[n].value), "%d", month);
  put(tr(STR_TOOLS_POMO_30_DAYS));
  snprintf(stats_[n].value, sizeof(stats_[n].value), "%dh %02dm", focusMinutes / 60, focusMinutes % 60);
  put(tr(STR_TOOLS_POMO_FOCUS_TIME));
  snprintf(stats_[n].value, sizeof(stats_[n].value), "%d", streak);
  put(tr(STR_TOOLS_POMO_STREAK));
  if (activeDays > 0) {
    snprintf(stats_[n].value, sizeof(stats_[n].value), "%.1f", static_cast<float>(month) / activeDays);
  } else {
    snprintf(stats_[n].value, sizeof(stats_[n].value), "-");
  }
  put(tr(STR_TOOLS_POMO_PER_DAY));
  if (bestIdx >= 0) {
    char date[16];
    tools::formatShortDate(date, sizeof(date), today_ - (kDays - 1) + bestIdx);
    snprintf(stats_[n].value, sizeof(stats_[n].value), "%s (%d)", date, best);
  } else {
    snprintf(stats_[n].value, sizeof(stats_[n].value), "-");
  }
  put(tr(STR_TOOLS_POMO_BEST_DAY));
  if (startN > 0) {
    charts::formatMinute(stats_[n].value, sizeof(stats_[n].value), static_cast<int>(startSum / startN));
  } else {
    snprintf(stats_[n].value, sizeof(stats_[n].value), "-");
  }
  put(tr(STR_TOOLS_POMO_USUAL_START));
  transitionPending_ = true;
  requestUpdate();
}

void PomodoroStatsActivity::loop() {
  input_.poll(mappedInput);
  if (input_.back || input_.backLong || input_.confirm || input_.left || input_.right) finish();
}

void PomodoroStatsActivity::render(RenderLock&&) {
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_POMO_STATS));
  if (!clockValid_) {
    renderer.drawCenteredText(UI_12_FONT_ID, content.y + content.height / 2, tr(STR_TOOLS_CLOCK_NOT_SET), true,
                              EpdFontFamily::BOLD);
  } else {
    int y = content.y;
    y += charts::statGrid(renderer, Rect{content.x, y, content.width, 0}, 2, stats_, 8) + 6;
    const int chartH = (content.y + content.height - y - 12) / 2;
    int16_t maxBar = 1;
    for (int i = kDays - kBarDays; i < kDays; ++i) maxBar = std::max(maxBar, sessions_[i]);
    char top[12];
    snprintf(top, sizeof(top), "%d", maxBar);
    const Rect bars =
        charts::drawPanel(renderer, Rect{content.x, y, content.width, chartH}, tr(STR_TOOLS_POMO_CHART_SESSIONS),
                          tr(STR_TOOLS_POMO_14_DAYS_AGO), tr(STR_TOOLS_TODAY));
    charts::barChart(renderer, bars, sessions_ + (kDays - kBarDays), kBarDays, maxBar);
    renderer.drawText(SMALL_FONT_ID, bars.x + 2, bars.y, top);
    y += chartH + 12;
    const Rect starts =
        charts::drawPanel(renderer, Rect{content.x, y, content.width, chartH}, tr(STR_TOOLS_POMO_CHART_START),
                          tr(STR_TOOLS_CHART_30_DAYS_AGO), tr(STR_TOOLS_TODAY));
    charts::timeScatter(renderer, starts, firstStart_, kDays);
  }
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
