#include "HabitStatsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "fontIds.h"

namespace {
void formatHoursMinutes(char* buf, const size_t len, const long minutes) {
  if (minutes < 60) {
    snprintf(buf, len, "%ldm", minutes);
  } else {
    snprintf(buf, len, "%ldh %02ldm", minutes / 60, minutes % 60);
  }
}
}  // namespace

HabitStatsActivity::HabitStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* habitName)
    : Activity("HabitStats", renderer, mappedInput) {
  snprintf(name_, sizeof(name_), "%s", habitName);
}

void HabitStatsActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  tools::DateTime local;
  clockValid_ = tools::getLocalNow(local);
  if (clockValid_) {
    today_ = tools::daysOf(local);
    compute();
  }
  transitionPending_ = true;
  requestUpdate();
}

void HabitStatsActivity::compute() {
  habits::summarize(name_, today_, summary_);
  const habits::Summary& h = summary_;
  int i = 0;
  auto put = [&](const char* label) { stats_[i++].label = label; };
  snprintf(stats_[i].value, sizeof(stats_[i].value), "%d", h.current);
  put(tr(STR_TOOLS_STAT_CURRENT_STREAK));
  snprintf(stats_[i].value, sizeof(stats_[i].value), "%d", h.best);
  put(tr(STR_TOOLS_STAT_BEST_STREAK));
  snprintf(stats_[i].value, sizeof(stats_[i].value), "%d%%", h.done30 * 100 / 30);
  put(tr(STR_TOOLS_STAT_LAST_30_DAYS));
  snprintf(stats_[i].value, sizeof(stats_[i].value), "%d / %d", h.doneWeeks, habits::Summary::kWeeks * 7);
  put(tr(STR_TOOLS_STAT_DONE_12_WEEKS));
  if (h.avgMinute >= 0) {
    charts::formatMinute(stats_[i].value, sizeof(stats_[i].value), h.avgMinute);
  } else {
    snprintf(stats_[i].value, sizeof(stats_[i].value), "-");
  }
  put(tr(STR_TOOLS_STAT_USUAL_TIME));
  if (h.spreadMinutes >= 0) {
    char spread[16];
    formatHoursMinutes(spread, sizeof(spread), h.spreadMinutes);
    snprintf(stats_[i].value, sizeof(stats_[i].value), "+/- %s", spread);
  } else {
    snprintf(stats_[i].value, sizeof(stats_[i].value), "-");
  }
  put(tr(STR_TOOLS_STAT_TIME_SPREAD));
  if (h.avgGapMinutes >= 0) {
    formatHoursMinutes(stats_[i].value, sizeof(stats_[i].value), h.avgGapMinutes);
  } else {
    snprintf(stats_[i].value, sizeof(stats_[i].value), "-");
  }
  put(tr(STR_TOOLS_STAT_AVG_GAP));
  snprintf(stats_[i].value, sizeof(stats_[i].value), "%s",
           h.bestWeekday >= 0 ? tools::weekdayName(static_cast<uint8_t>(h.bestWeekday)) : "-");
  put(tr(STR_TOOLS_STAT_BEST_DAY));
}

void HabitStatsActivity::loop() {
  input_.poll(mappedInput);
  if (input_.backLong) {
    tools::exitToHome();
    return;
  }
  if (input_.back || input_.confirm) finish();
}

void HabitStatsActivity::render(RenderLock&&) {
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_HABIT_STATS), name_);
  if (!clockValid_) {
    renderer.drawCenteredText(UI_12_FONT_ID, content.y + content.height / 2, tr(STR_TOOLS_CLOCK_NOT_SET), true,
                              EpdFontFamily::BOLD);
  } else {
    int y = content.y;
    y += charts::statGrid(renderer, Rect{content.x, y, content.width, 0}, 2, stats_, 8) + 6;
    const int chartH = (content.y + content.height - y - 12) / 2;
    const Rect weekly =
        charts::drawPanel(renderer, Rect{content.x, y, content.width, chartH}, tr(STR_TOOLS_CHART_WEEKLY_COMPLETION),
                          tr(STR_TOOLS_CHART_12_WEEKS_AGO), tr(STR_TOOLS_CHART_THIS_WEEK));
    charts::lineChart(renderer, weekly, summary_.weekPct, habits::Summary::kWeeks, 0, 100, "100%", "0%");
    y += chartH + 12;
    const Rect times =
        charts::drawPanel(renderer, Rect{content.x, y, content.width, chartH}, tr(STR_TOOLS_CHART_CHECKIN_TIME),
                          tr(STR_TOOLS_CHART_30_DAYS_AGO), tr(STR_TOOLS_TODAY));
    charts::timeScatter(renderer, times, summary_.checkMinute, habits::Summary::kTimeDays);
  }
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
