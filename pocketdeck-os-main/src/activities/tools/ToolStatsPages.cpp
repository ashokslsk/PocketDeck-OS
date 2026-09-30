#include "ToolStatsPages.h"

#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "HabitSummary.h"
#include "ToolStatsData.h"

namespace toolstats {

namespace {
using tstats::DaySeries;
using tstats::kHistoryDays;

// Copies the newest n values of a 90-day series into a chart.
void lastDays(Chart& c, const int16_t* values, const int n) {
  c.n = std::min(n, kMaxPoints);
  for (int i = 0; i < c.n; ++i) c.values[i] = values[kHistoryDays - c.n + i];
}

void chart(Chart& c, const Chart::Kind kind, const char* title, const char* left, const char* right) {
  c.kind = kind;
  c.title = title;
  c.left = left;
  c.right = right;
}

int16_t maxOf(const int16_t* v, const int n) {
  int16_t m = 1;
  for (int i = 0; i < n; ++i) m = std::max(m, v[i]);
  return m;
}

void formatHm(char* buf, const size_t len, const int minutes) {
  if (minutes < 0) {
    snprintf(buf, len, "-");
  } else {
    charts::formatMinute(buf, static_cast<int>(len), minutes);
  }
}

void formatDuration(char* buf, const size_t len, const long minutes) {
  if (minutes < 60) {
    snprintf(buf, len, "%ldm", minutes);
  } else {
    snprintf(buf, len, "%ldh %02ldm", minutes / 60, minutes % 60);
  }
}

std::unique_ptr<DaySeries> series(const char* feature, const int32_t today, const tlog::LineFn fn,
                                  const int16_t fill = 0) {
  auto s = makeUniqueNoThrow<DaySeries>();
  if (s) tstats::scanSeries(feature, today, fn, *s, fill);
  return s;
}
}  // namespace

void buildPomodoro(Page& page, const int32_t today, const void*) {
  auto s = series("pomodoro", today, &tstats::onPomodoro);
  if (!s) return;
  const int weekday = tools::weekdayMon0(today);
  int activeDays = 0, bestIdx = -1;
  long minutes30 = 0;
  for (int i = kHistoryDays - 30; i < kHistoryDays; ++i) {
    minutes30 += s->b[i];
    if (s->a[i] > 0) ++activeDays;
    if (s->a[i] > 0 && (bestIdx < 0 || s->a[i] > s->a[bestIdx])) bestIdx = i;
  }
  const int month = tstats::sumLast(s->a, 30);
  char buf[24];
  page.tile(tr(STR_TOOLS_POMO_TODAY), "%d", s->a[kHistoryDays - 1]);
  page.tile(tr(STR_TOOLS_POMO_WEEK), "%d", tstats::sumLast(s->a, weekday + 1));
  page.tile(tr(STR_TOOLS_POMO_30_DAYS), "%d", month);
  formatDuration(buf, sizeof(buf), minutes30);
  page.tile(tr(STR_TOOLS_POMO_FOCUS_TIME), "%s", buf);
  page.tile(tr(STR_TOOLS_POMO_STREAK), "%d", tstats::streak(s->a));
  page.tile(tr(STR_TOOLS_POMO_PER_DAY), "%.1f", activeDays > 0 ? static_cast<float>(month) / activeDays : 0.0f);
  if (bestIdx >= 0) {
    tools::formatShortDate(buf, sizeof(buf), s->firstDay + bestIdx);
    page.tile(tr(STR_TOOLS_POMO_BEST_DAY), "%s (%d)", buf, s->a[bestIdx]);
  } else {
    page.tile(tr(STR_TOOLS_POMO_BEST_DAY), "-");
  }
  // The log is written when a focus ends; show when sessions start.
  formatHm(buf, sizeof(buf), tstats::averageMinute(*s) >= 25 ? tstats::averageMinute(*s) - 25 : -1);
  page.tile(tr(STR_TOOLS_POMO_USUAL_START), "%s", buf);

  Chart& bars = page.charts[0];
  chart(bars, Chart::Kind::Bar, tr(STR_TOOLS_POMO_CHART_SESSIONS), tr(STR_TOOLS_POMO_14_DAYS_AGO), tr(STR_TOOLS_TODAY));
  lastDays(bars, s->a, 14);
  bars.maxV = maxOf(bars.values, bars.n);
  snprintf(bars.yTop, sizeof(bars.yTop), "%d", bars.maxV);
  Chart& times = page.charts[1];
  chart(times, Chart::Kind::Time, tr(STR_TOOLS_POMO_CHART_START), tr(STR_TOOLS_CHART_30_DAYS_AGO), tr(STR_TOOLS_TODAY));
  lastDays(times, s->minute, 30);
  for (int i = 0; i < times.n; ++i) {
    if (times.values[i] != charts::kNoValue) times.values[i] = static_cast<int16_t>(std::max(0, times.values[i] - 25));
  }
}

void buildHabit(Page& page, const int32_t today, const void* habitName) {
  const char* name = static_cast<const char*>(habitName);
  snprintf(page.subtitle, sizeof(page.subtitle), "%s", name);
  auto h = makeUniqueNoThrow<habits::Summary>();
  if (!h) return;
  habits::summarize(name, today, *h);
  char buf[24];
  page.tile(tr(STR_TOOLS_STAT_CURRENT_STREAK), "%d", h->current);
  page.tile(tr(STR_TOOLS_STAT_BEST_STREAK), "%d", h->best);
  page.tile(tr(STR_TOOLS_STAT_LAST_30_DAYS), "%d%%", h->done30 * 100 / 30);
  page.tile(tr(STR_TOOLS_STAT_DONE_12_WEEKS), "%d / %d", h->doneWeeks, habits::Summary::kWeeks * 7);
  formatHm(buf, sizeof(buf), h->avgMinute);
  page.tile(tr(STR_TOOLS_STAT_USUAL_TIME), "%s", buf);
  if (h->spreadMinutes >= 0) {
    formatDuration(buf, sizeof(buf), h->spreadMinutes);
    page.tile(tr(STR_TOOLS_STAT_TIME_SPREAD), "+/- %s", buf);
  } else {
    page.tile(tr(STR_TOOLS_STAT_TIME_SPREAD), "-");
  }
  if (h->avgGapMinutes >= 0) {
    formatDuration(buf, sizeof(buf), h->avgGapMinutes);
    page.tile(tr(STR_TOOLS_STAT_AVG_GAP), "%s", buf);
  } else {
    page.tile(tr(STR_TOOLS_STAT_AVG_GAP), "-");
  }
  page.tile(tr(STR_TOOLS_STAT_BEST_DAY), "%s",
            h->bestWeekday >= 0 ? tools::weekdayName(static_cast<uint8_t>(h->bestWeekday)) : "-");

  Chart& weekly = page.charts[0];
  chart(weekly, Chart::Kind::Line, tr(STR_TOOLS_CHART_WEEKLY_COMPLETION), tr(STR_TOOLS_CHART_12_WEEKS_AGO),
        tr(STR_TOOLS_CHART_THIS_WEEK));
  weekly.n = habits::Summary::kWeeks;
  std::copy(h->weekPct, h->weekPct + weekly.n, weekly.values);
  snprintf(weekly.yTop, sizeof(weekly.yTop), "100%%");
  snprintf(weekly.yBottom, sizeof(weekly.yBottom), "0%%");
  Chart& times = page.charts[1];
  chart(times, Chart::Kind::Time, tr(STR_TOOLS_CHART_CHECKIN_TIME), tr(STR_TOOLS_CHART_30_DAYS_AGO),
        tr(STR_TOOLS_TODAY));
  times.n = habits::Summary::kTimeDays;
  std::copy(h->checkMinute, h->checkMinute + times.n, times.values);
}

void buildFlashcards(Page& page, const int32_t today, const void*) {
  auto s = series("flashcards", today, &tstats::onFlashcards);
  auto srs = makeUniqueNoThrow<tstats::SrsSummary>();
  if (!s || !srs) return;
  tstats::summarizeSrs(today, *srs);
  const int reviewed30 = tstats::sumLast(s->a, 30);
  const int remembered30 = tstats::sumLast(s->b, 30);
  page.tile(tr(STR_TOOLS_FC_CARDS), "%d", srs->totalCards());
  page.tile(tr(STR_TOOLS_FC_DUE), "%d", srs->totalDue());
  page.tile(tr(STR_TOOLS_FC_SEEN), "%d", srs->totalReviewed());
  page.tile(tr(STR_TOOLS_FC_MASTERED), "%d", srs->totalMastered());
  page.tile(tr(STR_TOOLS_FC_REVIEWED_TODAY), "%d", s->a[kHistoryDays - 1]);
  page.tile(tr(STR_TOOLS_FC_REVIEWED_30), "%d", reviewed30);
  page.tile(tr(STR_TOOLS_FC_RECALL), "%d%%", reviewed30 > 0 ? remembered30 * 100 / reviewed30 : 0);
  page.tile(tr(STR_TOOLS_FC_STREAK), "%d", tstats::streak(s->a));

  Chart& bars = page.charts[0];
  chart(bars, Chart::Kind::Bar, tr(STR_TOOLS_FC_CHART_REVIEWS), tr(STR_TOOLS_POMO_14_DAYS_AGO), tr(STR_TOOLS_TODAY));
  lastDays(bars, s->a, 14);
  bars.maxV = maxOf(bars.values, bars.n);
  snprintf(bars.yTop, sizeof(bars.yTop), "%d", bars.maxV);
  Chart& recall = page.charts[1];
  chart(recall, Chart::Kind::Line, tr(STR_TOOLS_FC_CHART_RECALL), tr(STR_TOOLS_CHART_30_DAYS_AGO), tr(STR_TOOLS_TODAY));
  recall.n = 30;
  for (int i = 0; i < 30; ++i) {
    const int k = kHistoryDays - 30 + i;
    recall.values[i] = s->a[k] > 0 ? static_cast<int16_t>(s->b[k] * 100 / s->a[k]) : charts::kNoValue;
  }
  snprintf(recall.yTop, sizeof(recall.yTop), "100%%");
  snprintf(recall.yBottom, sizeof(recall.yBottom), "0%%");
}

void buildKnowledge(Page& page, const int32_t today, const void*) {
  auto s = series("knowledge", today, &tstats::onKnowledge);
  auto cover = makeUniqueNoThrow<tstats::KnowledgeCoverage>();
  if (!s || !cover) return;
  tstats::summarizeKnowledge(today, *cover);
  int total = 0, seen = 0;
  for (int i = 0; i < cover->count; ++i) {
    total += cover->total[i];
    seen += cover->seen[i];
  }
  char buf[24];
  page.tile(tr(STR_TOOLS_KN_TODAY), "%d", s->a[kHistoryDays - 1]);
  page.tile(tr(STR_TOOLS_POMO_WEEK), "%d", tstats::sumLast(s->a, tools::weekdayMon0(today) + 1));
  page.tile(tr(STR_TOOLS_POMO_30_DAYS), "%d", tstats::sumLast(s->a, 30));
  page.tile(tr(STR_TOOLS_FC_STREAK), "%d", tstats::streak(s->a));
  page.tile(tr(STR_TOOLS_KN_TOPICS), "%d", cover->count);
  page.tile(tr(STR_TOOLS_KN_QUESTIONS_OPENED), "%d / %d", seen, total);
  page.tile(tr(STR_TOOLS_KN_COVERAGE), "%d%%", total > 0 ? seen * 100 / total : 0);
  formatHm(buf, sizeof(buf), tstats::averageMinute(*s));
  page.tile(tr(STR_TOOLS_STAT_USUAL_TIME), "%s", buf);

  Chart& bars = page.charts[0];
  chart(bars, Chart::Kind::Bar, tr(STR_TOOLS_KN_CHART_DAILY), tr(STR_TOOLS_POMO_14_DAYS_AGO), tr(STR_TOOLS_TODAY));
  lastDays(bars, s->a, 14);
  bars.maxV = maxOf(bars.values, bars.n);
  snprintf(bars.yTop, sizeof(bars.yTop), "%d", bars.maxV);
  if (cover->count > 0) {
    Chart& topics = page.charts[1];
    chart(topics, Chart::Kind::Bar, tr(STR_TOOLS_KN_CHART_COVERAGE), cover->topic[0],
          cover->count > 1 ? cover->topic[cover->count - 1] : nullptr);
    topics.n = std::min(cover->count, kMaxPoints);
    for (int i = 0; i < topics.n; ++i) {
      topics.values[i] = static_cast<int16_t>(cover->total[i] > 0 ? cover->seen[i] * 100 / cover->total[i] : 0);
    }
    topics.maxV = 100;
    snprintf(topics.yTop, sizeof(topics.yTop), "100%%");
  }
}

void buildQuotes(Page& page, const int32_t today, const void*) {
  auto s = series("quotes", today, &tstats::onQuotes);
  if (!s) return;
  char buf[24];
  page.tile(tr(STR_TOOLS_QT_DAYS_30), "%d / 30", tstats::sumLast(s->a, 30));
  page.tile(tr(STR_TOOLS_FC_STREAK), "%d", tstats::streak(s->a));
  page.tile(tr(STR_TOOLS_QT_FAVOURITES), "%d", tstats::countFavouriteQuotes());
  page.tile(tr(STR_TOOLS_QT_LIBRARY), "%lu",
            static_cast<unsigned long>(tstats::indexCount("/tools/.cache/quotes.idx")));
  page.tile(tr(STR_TOOLS_QT_DAYS_90), "%d", tstats::sumLast(s->a, 90));
  formatHm(buf, sizeof(buf), tstats::averageMinute(*s));
  page.tile(tr(STR_TOOLS_STAT_USUAL_TIME), "%s", buf);
  Chart& days = page.charts[0];
  chart(days, Chart::Kind::Bar, tr(STR_TOOLS_QT_CHART_DAYS), tr(STR_TOOLS_CHART_30_DAYS_AGO), tr(STR_TOOLS_TODAY));
  lastDays(days, s->a, 30);
  days.maxV = 1;
  Chart& times = page.charts[1];
  chart(times, Chart::Kind::Time, tr(STR_TOOLS_QT_CHART_TIME), tr(STR_TOOLS_CHART_30_DAYS_AGO), tr(STR_TOOLS_TODAY));
  lastDays(times, s->minute, 30);
}

void buildMantras(Page& page, const int32_t today, const void*) {
  auto s = series("mantras", today, &tstats::onMantras);
  if (!s) return;
  char buf[24];
  const int month = tstats::sumLast(s->a, 30);
  int activeDays = 0, sessions = 0;
  for (int i = kHistoryDays - 30; i < kHistoryDays; ++i) {
    if (s->a[i] > 0) ++activeDays;
    sessions += s->b[i];
  }
  page.tile(tr(STR_TOOLS_MANTRA_JAPA_TODAY), "%d", s->a[kHistoryDays - 1]);
  page.tile(tr(STR_TOOLS_POMO_WEEK), "%d", tstats::sumLast(s->a, tools::weekdayMon0(today) + 1));
  page.tile(tr(STR_TOOLS_POMO_30_DAYS), "%d", month);
  page.tile(tr(STR_TOOLS_MANTRA_MALAS_30), "%d", month / 108);
  page.tile(tr(STR_TOOLS_FC_STREAK), "%d", tstats::streak(s->a));
  page.tile(tr(STR_TOOLS_MANTRA_SESSIONS_30), "%d", sessions);
  page.tile(tr(STR_TOOLS_POMO_PER_DAY), "%d", activeDays > 0 ? month / activeDays : 0);
  formatHm(buf, sizeof(buf), tstats::averageMinute(*s));
  page.tile(tr(STR_TOOLS_STAT_USUAL_TIME), "%s", buf);
  Chart& bars = page.charts[0];
  chart(bars, Chart::Kind::Bar, tr(STR_TOOLS_MANTRA_CHART_JAPA), tr(STR_TOOLS_POMO_14_DAYS_AGO), tr(STR_TOOLS_TODAY));
  lastDays(bars, s->a, 14);
  Chart& times = page.charts[1];
  chart(times, Chart::Kind::Time, tr(STR_TOOLS_MANTRA_CHART_TIME), tr(STR_TOOLS_CHART_30_DAYS_AGO),
        tr(STR_TOOLS_TODAY));
  lastDays(times, s->minute, 30);
}

void buildToday(Page& page, const int32_t today, const void*) {
  auto s = series("today", today, &tstats::onToday);
  if (!s) return;
  int days = 0, done = 0, total = 0, perfect = 0, perfectStreak = 0;
  bool streakAlive = true;
  for (int i = kHistoryDays - 1; i >= kHistoryDays - 30; --i) {
    if (s->b[i] <= 0) continue;
    ++days;
    done += s->a[i];
    total += s->b[i];
    const bool allDone = s->a[i] >= s->b[i];
    perfect += allDone ? 1 : 0;
    if (streakAlive && allDone) {
      ++perfectStreak;
    } else {
      streakAlive = false;
    }
  }
  page.tile(tr(STR_TOOLS_TD_DAYS), "%d", days);
  page.tile(tr(STR_TOOLS_TD_DONE), "%d / %d", done, total);
  page.tile(tr(STR_TOOLS_TD_COMPLETION), "%d%%", total > 0 ? done * 100 / total : 0);
  page.tile(tr(STR_TOOLS_TD_PERFECT), "%d", perfect);
  page.tile(tr(STR_TOOLS_TD_PERFECT_STREAK), "%d", perfectStreak);
  page.tile(tr(STR_TOOLS_TD_PER_DAY), "%.1f", days > 0 ? static_cast<float>(done) / days : 0.0f);
  Chart& pct = page.charts[0];
  chart(pct, Chart::Kind::Bar, tr(STR_TOOLS_TD_CHART), tr(STR_TOOLS_POMO_14_DAYS_AGO), tr(STR_TOOLS_TODAY));
  pct.n = 14;
  for (int i = 0; i < 14; ++i) {
    const int k = kHistoryDays - 14 + i;
    pct.values[i] = s->b[k] > 0 ? static_cast<int16_t>(s->a[k] * 100 / s->b[k]) : charts::kNoValue;
  }
  pct.maxV = 100;
  snprintf(pct.yTop, sizeof(pct.yTop), "100%%");
  Chart& tasks = page.charts[1];
  chart(tasks, Chart::Kind::Bar, tr(STR_TOOLS_TD_CHART_DONE), tr(STR_TOOLS_POMO_14_DAYS_AGO), tr(STR_TOOLS_TODAY));
  lastDays(tasks, s->a, 14);
  tasks.maxV = maxOf(tasks.values, tasks.n);
  snprintf(tasks.yTop, sizeof(tasks.yTop), "%d", tasks.maxV);
}

void buildMood(Page& page, const int32_t today, const void*) {
  auto s = series("mood", today, &tstats::onMood, charts::kNoValue);
  if (!s) return;
  static constexpr StrId kNames[] = {StrId::STR_TOOLS_MOOD_AWFUL, StrId::STR_TOOLS_MOOD_LOW, StrId::STR_TOOLS_MOOD_OKAY,
                                     StrId::STR_TOOLS_MOOD_GOOD, StrId::STR_TOOLS_MOOD_GREAT};
  long sum7 = 0, sum30 = 0, sumPrev = 0;
  int n7 = 0, n30 = 0, nPrev = 0;
  int counts[6] = {};
  for (int back = 0; back < 30; ++back) {
    const int16_t m = s->a[kHistoryDays - 1 - back];
    if (m == charts::kNoValue) continue;
    sum30 += m;
    ++n30;
    ++counts[m];
    if (back < 7) {
      sum7 += m;
      ++n7;
    } else if (back < 14) {
      sumPrev += m;
      ++nPrev;
    }
  }
  int mostCommon = 1;
  for (int m = 2; m <= 5; ++m) mostCommon = counts[m] > counts[mostCommon] ? m : mostCommon;
  char buf[24];
  if (n7 > 0) {
    page.tile(tr(STR_TOOLS_MOOD_AVG_7), "%.1f", static_cast<float>(sum7) / n7);
  } else {
    page.tile(tr(STR_TOOLS_MOOD_AVG_7), "-");
  }
  if (n30 > 0) {
    page.tile(tr(STR_TOOLS_MOOD_AVG_30), "%.1f", static_cast<float>(sum30) / n30);
  } else {
    page.tile(tr(STR_TOOLS_MOOD_AVG_30), "-");
  }
  if (n7 > 0 && nPrev > 0) {
    const float delta = static_cast<float>(sum7) / n7 - static_cast<float>(sumPrev) / nPrev;
    page.tile(tr(STR_TOOLS_MOOD_TREND), "%+.1f", delta);
  } else {
    page.tile(tr(STR_TOOLS_MOOD_TREND), "-");
  }
  page.tile(tr(STR_TOOLS_MOOD_MOST_COMMON), "%s", counts[mostCommon] > 0 ? I18N.get(kNames[mostCommon - 1]) : "-");
  page.tile(tr(STR_TOOLS_MOOD_DAYS_LOGGED), "%d / 30", n30);
  int streak = 0;
  for (int i = kHistoryDays - 1; i >= 0; --i) {
    if (s->a[i] == charts::kNoValue) {
      if (i == kHistoryDays - 1) continue;
      break;
    }
    ++streak;
  }
  page.tile(tr(STR_TOOLS_MOOD_STREAK), "%d", streak);
  formatHm(buf, sizeof(buf), tstats::averageMinute(*s));
  page.tile(tr(STR_TOOLS_STAT_USUAL_TIME), "%s", buf);
  page.tile(tr(STR_TOOLS_MOOD_GOOD_DAYS), "%d%%", n30 > 0 ? (counts[4] + counts[5]) * 100 / n30 : 0);

  Chart& line = page.charts[0];
  chart(line, Chart::Kind::Line, tr(STR_TOOLS_MOOD_CHART), tr(STR_TOOLS_CHART_30_DAYS_AGO), tr(STR_TOOLS_TODAY));
  lastDays(line, s->a, 30);
  line.minV = 1;
  line.maxV = 5;
  snprintf(line.yTop, sizeof(line.yTop), "%s", tr(STR_TOOLS_MOOD_GREAT));
  snprintf(line.yBottom, sizeof(line.yBottom), "%s", tr(STR_TOOLS_MOOD_AWFUL));
  Chart& dist = page.charts[1];
  chart(dist, Chart::Kind::Bar, tr(STR_TOOLS_MOOD_CHART_MIX), tr(STR_TOOLS_MOOD_AWFUL), tr(STR_TOOLS_MOOD_GREAT));
  dist.n = 5;
  for (int m = 1; m <= 5; ++m) dist.values[m - 1] = static_cast<int16_t>(counts[m]);
  dist.maxV = maxOf(dist.values, 5);
  snprintf(dist.yTop, sizeof(dist.yTop), "%d", dist.maxV);
}

}  // namespace toolstats
