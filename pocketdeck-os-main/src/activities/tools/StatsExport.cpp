#include "StatsExport.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "BookmarkStore.h"
#include "ClippingStore.h"
#include "HabitSummary.h"
#include "MedicineData.h"
#include "RecentBooksStore.h"
#include "ToolStatsData.h"
#include "ToolsCharts.h"
#include "ToolsCommon.h"
#include "ToolsJson.h"
#include "ToolsLog.h"
#include "activities/reader/BookInsights.h"
#include "activities/reader/BookReadingStats.h"
#include "activities/reader/GlobalReadingStats.h"
#include "activities/reader/LibraryStats.h"
#include "util/LookupHistory.h"

namespace statsx {

using tstats::averageMinute;
using tstats::clearSeries;
using tstats::DaySeries;
using tstats::DeckTotals;
using tstats::kHistoryDays;
using tstats::onDeck;

namespace {
constexpr char kRoot[] = "/stats";
constexpr size_t kHexChunkBytes = 128;  // 256 hex characters per JSON string

struct Clock {
  bool valid = false;
  int32_t today = 0;
  int16_t minute = 0;
  char stamp[20] = "";  // "YYYY-MM-DD HH:MM"
};

Clock readClock() {
  Clock c;
  tlog::Stamp now;
  c.valid = tlog::now(now);
  if (!c.valid) return c;
  c.today = now.day;
  c.minute = now.minute;
  char date[12];
  tools::formatIsoDate(date, sizeof(date), now.day);
  snprintf(c.stamp, sizeof(c.stamp), "%s %02d:%02d", date, now.minute / 60, now.minute % 60);
  return c;
}

bool ensureFolder(const char* sub) {
  char dir[48];
  snprintf(dir, sizeof(dir), "%s/%s", kRoot, sub);
  return Storage.ensureDirectoryExists(kRoot) && Storage.ensureDirectoryExists(dir);
}

void header(tools::JsonOut& j, const Clock& c, const char* feature) {
  j.str("feature", feature).str("exported", c.stamp).str("generator", "PocketDeck-OS " POCKETDECK_VERSION);
}

void dayKey(char (&buf)[12], const int32_t day) { tools::formatIsoDate(buf, sizeof(buf), day); }

void minuteValue(tools::JsonOut& j, const char* key, const int minute) {
  if (minute < 0 || minute == charts::kNoValue) {
    j.null(key);
    return;
  }
  char hm[8];
  charts::formatMinute(hm, sizeof(hm), minute);
  j.str(key, hm);
}

// ---------------------------------------------------------------------------
// Habits
// ---------------------------------------------------------------------------
struct HabitsCtx {
  const Clock* clock;
  uint16_t items;
};

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool writeHabits(FsFile& out, void* p) {
  auto* ctx = static_cast<HabitsCtx*>(p);
  habits::Names names;
  const int count = habits::loadNames(names);
  tools::JsonOut j(out);
  j.beginObject();
  header(j, *ctx->clock, "habits");
  j.beginArray("habits");
  for (int h = 0; h < count; ++h) {
    habits::Summary s;
    habits::summarize(names[h], ctx->clock->today, s);
    j.beginObject();
    j.str("name", names[h]).num("current_streak_days", s.current).num("best_streak_days", s.best);
    j.num("done_last_30_days", s.done30).num("rate_last_30_days_percent", s.done30 * 100 / 30);
    j.num("done_last_12_weeks", s.doneWeeks);
    minuteValue(j, "usual_time", s.avgMinute);
    if (s.spreadMinutes >= 0) {
      j.num("time_spread_minutes", s.spreadMinutes);
    } else {
      j.null("time_spread_minutes");
    }
    if (s.avgGapMinutes >= 0) {
      j.num("average_gap_minutes", s.avgGapMinutes);
    } else {
      j.null("average_gap_minutes");
    }
    j.str("best_weekday", s.bestWeekday >= 0 ? tools::weekdayName(static_cast<uint8_t>(s.bestWeekday)) : "");
    j.beginArray("weeks");
    const int32_t thisMonday = habits::mondayOf(ctx->clock->today);
    for (int w = 0; w < habits::Summary::kWeeks; ++w) {
      const int32_t monday = thisMonday - 7 * (habits::Summary::kWeeks - 1 - w);
      char date[12];
      dayKey(date, monday);
      char days[8];
      for (int d = 0; d < 7; ++d) days[d] = (s.weekBits[w] >> d) & 1 ? '1' : '0';
      days[7] = '\0';
      j.beginObject().str("week_of", date).str("mon_to_sun", days).num("percent", s.weekPct[w]).endObject();
    }
    j.endArray();
    j.beginArray("check_in_times_last_30_days");
    for (int i = 0; i < habits::Summary::kTimeDays; ++i) {
      if (s.checkMinute[i] == charts::kNoValue) continue;
      char date[12];
      dayKey(date, ctx->clock->today - (habits::Summary::kTimeDays - 1) + i);
      j.beginObject().str("date", date);
      minuteValue(j, "time", s.checkMinute[i]);
      j.endObject();
    }
    j.endArray();
    j.endObject();
    ++ctx->items;
  }
  j.endArray();
  j.endObject();
  return j.ok();
}

// ---------------------------------------------------------------------------
// Medicine
// ---------------------------------------------------------------------------
struct MedsCtx {
  const Clock* clock;
  uint16_t items;
  meds::Course* courses;  // heap scratch, kMaxCourses
  int16_t* doseMinute;    // heap scratch, kMaxDays * kMaxDoses
};

const char* statusName(const meds::Status s) {
  switch (s) {
    case meds::Status::Upcoming:
      return "upcoming";
    case meds::Status::Active:
      return "active";
    case meds::Status::Completed:
      return "completed";
    default:
      return "stopped";
  }
}

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool writeMeds(FsFile& out, void* p) {
  auto* ctx = static_cast<MedsCtx*>(p);
  auto& courses = *reinterpret_cast<meds::Course(*)[meds::kMaxCourses]>(ctx->courses);
  const int count = meds::load(courses);
  tools::JsonOut j(out);
  j.beginObject();
  header(j, *ctx->clock, "medicine");
  j.beginArray("courses");
  for (int i = 0; i < count; ++i) {
    const meds::Course& c = courses[i];
    meds::CourseSummary s;
    meds::summarize(c, ctx->clock->today, ctx->clock->minute, ctx->doseMinute, s);
    char start[12], end[12];
    dayKey(start, c.start);
    dayKey(end, c.lastDay());
    j.beginObject();
    j.str("name", c.name).str("id", c.id).str("status", statusName(s.status));
    j.num("doses_per_day", c.doses).num("days", c.days).str("started", start).str("planned_end", end);
    if (c.stopped != 0) {
      char stopped[12];
      dayKey(stopped, c.stopped);
      j.str("stopped_on", stopped);
    } else {
      j.null("stopped_on");
    }
    if (s.status == meds::Status::Completed) {
      char done[12];
      dayKey(done, s.lastTakenDay != 0 ? s.lastTakenDay : c.lastDay());
      j.str("completed_on", done);
    } else {
      j.null("completed_on");
    }
    j.num("doses_planned", s.planned).num("doses_due_so_far", s.due).num("doses_taken", s.taken);
    j.num("doses_missed", s.missed);
    j.num("adherence_percent", s.due > 0 ? std::min(100, s.taken * 100 / s.due) : 100);
    if (s.timed > 0) {
      j.num("on_time_percent", s.onTime * 100 / s.timed).num("average_delay_minutes", s.avgDelay);
    } else {
      j.null("on_time_percent").null("average_delay_minutes");
    }
    j.num("days_with_every_dose", s.fullDays);
    j.beginArray("dose_times");
    for (int d = 0; d < c.doses; ++d) minuteValue(j, nullptr, c.slotMinute[d]);
    j.endArray();
    j.beginArray("days");
    for (int d = 0; d < c.days; ++d) {
      char date[12];
      dayKey(date, c.start + d);
      j.beginObject().str("date", date);
      j.beginArray("taken_at");
      for (int sl = 0; sl < c.doses; ++sl) {
        const int16_t m = ctx->doseMinute[d * meds::kMaxDoses + sl];
        if (!c.isTaken(d, sl)) {
          j.raw(nullptr, "false");
        } else if (m == charts::kNoValue) {
          j.raw(nullptr, "true");
        } else {
          minuteValue(j, nullptr, m);
        }
      }
      j.endArray();
      j.endObject();
    }
    j.endArray();
    j.endObject();
    ++ctx->items;
  }
  j.endArray();
  j.endObject();
  return j.ok();
}

// ---------------------------------------------------------------------------
// Mood, Pomodoro, Today, Flashcards: per-day series from the logs
// ---------------------------------------------------------------------------
struct SeriesCtx {
  const Clock* clock;
  DaySeries* series;
  uint16_t items;
};

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool writeMood(FsFile& out, void* p) {
  auto* ctx = static_cast<SeriesCtx*>(p);
  const DaySeries& s = *ctx->series;
  static constexpr const char* kNames[] = {"", "awful", "low", "okay", "good", "great"};
  int counts[6] = {};
  long sum7 = 0, sum30 = 0, sum90 = 0;
  int n7 = 0, n30 = 0, n90 = 0;
  for (int i = 0; i < kHistoryDays; ++i) {
    const int16_t m = s.a[i];
    if (m == charts::kNoValue) continue;
    const int back = kHistoryDays - 1 - i;
    ++counts[m];
    sum90 += m;
    ++n90;
    if (back < 30) {
      sum30 += m;
      ++n30;
    }
    if (back < 7) {
      sum7 += m;
      ++n7;
    }
  }
  tools::JsonOut j(out);
  j.beginObject();
  header(j, *ctx->clock, "mood");
  j.str("scale", "1 awful, 2 low, 3 okay, 4 good, 5 great");
  if (n7 > 0) {
    j.dec("average_last_7_days", static_cast<double>(sum7) / n7);
  } else {
    j.null("average_last_7_days");
  }
  if (n30 > 0) {
    j.dec("average_last_30_days", static_cast<double>(sum30) / n30);
  } else {
    j.null("average_last_30_days");
  }
  if (n90 > 0) {
    j.dec("average_last_90_days", static_cast<double>(sum90) / n90);
  } else {
    j.null("average_last_90_days");
  }
  j.num("days_logged_last_30", n30).num("days_logged_last_90", n90);
  minuteValue(j, "usual_check_in_time", averageMinute(s));
  j.beginObject("counts_last_90_days");
  for (int m = 1; m <= 5; ++m) j.num(kNames[m], counts[m]);
  j.endObject();
  j.beginArray("days");
  for (int i = 0; i < kHistoryDays; ++i) {
    if (s.a[i] == charts::kNoValue) continue;
    char date[12];
    dayKey(date, s.firstDay + i);
    j.beginObject().str("date", date).num("mood", s.a[i]).str("label", kNames[s.a[i]]);
    minuteValue(j, "time", s.minute[i]);
    j.endObject();
    ++ctx->items;
  }
  j.endArray();
  j.endObject();
  return j.ok();
}

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool writePomodoro(FsFile& out, void* p) {
  auto* ctx = static_cast<SeriesCtx*>(p);
  const DaySeries& s = *ctx->series;
  long sessions = 0, minutes = 0, sessions7 = 0;
  int activeDays = 0, bestIdx = -1;
  for (int i = 0; i < kHistoryDays; ++i) {
    if (s.a[i] <= 0) continue;
    sessions += s.a[i];
    minutes += s.b[i];
    ++activeDays;
    if (kHistoryDays - 1 - i < 7) sessions7 += s.a[i];
    if (bestIdx < 0 || s.a[i] > s.a[bestIdx]) bestIdx = i;
  }
  tools::JsonOut j(out);
  j.beginObject();
  header(j, *ctx->clock, "pomodoro");
  j.num("focus_sessions_last_90_days", sessions).num("focus_minutes_last_90_days", minutes);
  j.num("focus_sessions_last_7_days", sessions7).num("active_days_last_90", activeDays);
  if (activeDays > 0) {
    j.dec("sessions_per_active_day", static_cast<double>(sessions) / activeDays);
  } else {
    j.null("sessions_per_active_day");
  }
  if (bestIdx >= 0) {
    char date[12];
    dayKey(date, s.firstDay + bestIdx);
    j.str("best_day", date).num("best_day_sessions", s.a[bestIdx]);
  }
  minuteValue(j, "usual_first_session_time", averageMinute(s));
  j.beginArray("days");
  for (int i = 0; i < kHistoryDays; ++i) {
    if (s.a[i] <= 0) continue;
    char date[12];
    dayKey(date, s.firstDay + i);
    j.beginObject().str("date", date).num("sessions", s.a[i]).num("focus_minutes", s.b[i]);
    minuteValue(j, "first_session", s.minute[i]);
    j.endObject();
    ++ctx->items;
  }
  j.endArray();
  j.endObject();
  return j.ok();
}

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool writeToday(FsFile& out, void* p) {
  auto* ctx = static_cast<SeriesCtx*>(p);
  const DaySeries& s = *ctx->series;
  long done = 0, total = 0;
  int days = 0, perfect = 0;
  for (int i = 0; i < kHistoryDays; ++i) {
    if (s.b[i] <= 0) continue;
    done += s.a[i];
    total += s.b[i];
    ++days;
    if (s.a[i] == s.b[i]) ++perfect;
  }
  tools::JsonOut j(out);
  j.beginObject();
  header(j, *ctx->clock, "today");
  j.str("note", "A day is recorded when Today is opened on a later day.");
  j.num("days_recorded", days).num("tasks_done", done).num("tasks_total", total);
  j.num("completion_percent", total > 0 ? done * 100 / total : 0).num("days_with_everything_done", perfect);
  j.beginArray("days");
  for (int i = 0; i < kHistoryDays; ++i) {
    if (s.b[i] <= 0) continue;
    char date[12];
    dayKey(date, s.firstDay + i);
    j.beginObject().str("date", date).num("done", s.a[i]).num("total", s.b[i]).endObject();
    ++ctx->items;
  }
  j.endArray();
  j.endObject();
  return j.ok();
}

// Per-deck totals need a second pass over the log.
struct FlashCtx {
  const Clock* clock;
  DaySeries* series;
  DeckTotals* decks;
  tstats::SrsSummary* srs;
  uint16_t items;
};

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool writeFlashcards(FsFile& out, void* p) {
  auto* ctx = static_cast<FlashCtx*>(p);
  const DaySeries& s = *ctx->series;
  const DeckTotals& d = *ctx->decks;
  tools::JsonOut j(out);
  j.beginObject();
  header(j, *ctx->clock, "flashcards");
  long reviewed = 0, remembered = 0;
  int studyDays = 0;
  for (int i = 0; i < kHistoryDays; ++i) {
    if (s.a[i] <= 0) continue;
    reviewed += s.a[i];
    remembered += s.b[i];
    ++studyDays;
  }
  j.num("cards_reviewed_last_90_days", reviewed).num("cards_remembered_last_90_days", remembered);
  j.num("recall_percent", reviewed > 0 ? remembered * 100 / reviewed : 0).num("study_days_last_90", studyDays);
  minuteValue(j, "usual_study_time", averageMinute(s));
  const tstats::SrsSummary& m = *ctx->srs;
  j.num("cards", m.totalCards()).num("cards_seen", m.totalReviewed()).num("cards_mastered_21_days", m.totalMastered());
  j.num("cards_due_today", m.totalDue());
  j.beginArray("mastery");
  for (int i = 0; i < m.count; ++i) {
    j.beginObject().str("deck", m.name[i]).num("cards", m.cards[i]).num("seen", m.reviewed[i]);
    j.num("mastered", m.mastered[i]).num("due_today", m.due[i] + std::max(0, m.cards[i] - m.reviewed[i])).endObject();
  }
  j.endArray();
  j.beginArray("decks");
  for (int i = 0; i < d.count; ++i) {
    j.beginObject().str("deck", d.name[i]).num("sessions", d.sessions[i]).num("reviewed", d.reviewed[i]);
    j.num("remembered", d.remembered[i]).num("forgot", d.forgot[i]);
    j.num("recall_percent", d.reviewed[i] > 0 ? d.remembered[i] * 100 / d.reviewed[i] : 0).endObject();
    ++ctx->items;
  }
  j.endArray();
  j.beginArray("days");
  for (int i = 0; i < kHistoryDays; ++i) {
    if (s.a[i] <= 0) continue;
    char date[12];
    dayKey(date, s.firstDay + i);
    j.beginObject().str("date", date).num("reviewed", s.a[i]).num("remembered", s.b[i]).endObject();
  }
  j.endArray();
  j.endObject();
  return j.ok();
}

struct KnowledgeCtx {
  const Clock* clock;
  DaySeries* series;
  tstats::KnowledgeCoverage* cover;
  uint16_t items;
};

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool writeKnowledge(FsFile& out, void* p) {
  auto* ctx = static_cast<KnowledgeCtx*>(p);
  const DaySeries& s = *ctx->series;
  const tstats::KnowledgeCoverage& c = *ctx->cover;
  tools::JsonOut j(out);
  j.beginObject();
  header(j, *ctx->clock, "knowledge");
  j.num("answers_opened_today", s.a[kHistoryDays - 1]).num("answers_opened_last_7_days", tstats::sumLast(s.a, 7));
  j.num("answers_opened_last_30_days", tstats::sumLast(s.a, 30)).num("study_streak_days", tstats::streak(s.a));
  minuteValue(j, "usual_study_time", averageMinute(s));
  j.beginArray("topics");
  for (int i = 0; i < c.count; ++i) {
    j.beginObject().str("topic", c.topic[i]).num("questions", c.total[i]).num("questions_opened", c.seen[i]);
    j.num("coverage_percent", c.total[i] > 0 ? c.seen[i] * 100 / c.total[i] : 0).endObject();
    ++ctx->items;
  }
  j.endArray();
  j.beginArray("days");
  for (int i = 0; i < kHistoryDays; ++i) {
    if (s.a[i] <= 0) continue;
    char date[12];
    dayKey(date, s.firstDay + i);
    j.beginObject().str("date", date).num("answers_opened", s.a[i]).endObject();
  }
  j.endArray();
  j.endObject();
  return j.ok();
}

struct QuotesCtx {
  const Clock* clock;
  DaySeries* series;
  uint16_t items;
};

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool writeQuotes(FsFile& out, void* p) {
  auto* ctx = static_cast<QuotesCtx*>(p);
  const DaySeries& s = *ctx->series;
  tools::JsonOut j(out);
  j.beginObject();
  header(j, *ctx->clock, "quotes");
  j.num("days_opened_last_30", tstats::sumLast(s.a, 30)).num("days_opened_last_90", tstats::sumLast(s.a, 90));
  j.num("streak_days", tstats::streak(s.a));
  minuteValue(j, "usual_time", averageMinute(s));
  j.beginArray("favourites");
  FsFile f;
  if (Storage.exists(tstats::kFavouritesPath) && Storage.openFileForRead("STX", tstats::kFavouritesPath, f)) {
    char line[512];
    while (tools::readLine(f, line, sizeof(line)) >= 0) {
      if (line[0] == '\0' || line[0] == '#') continue;
      char* bar = strchr(line, '|');
      const char* text = bar != nullptr ? bar + 1 : line;
      if (bar != nullptr) *bar = '\0';
      j.beginObject().str("date", bar != nullptr ? line : "").str("quote", text).endObject();
      ++ctx->items;
    }
    f.close();
  }
  j.endArray();
  j.endObject();
  return j.ok();
}

// ---------------------------------------------------------------------------
// Mantras (japa counter)
// ---------------------------------------------------------------------------
struct JapaTotals {
  static constexpr int kMax = 48;
  char key[kMax][24] = {};
  uint32_t count[kMax] = {};
  uint16_t sessions[kMax] = {};
  int n = 0;
};

// "count|category|mantra|text": totals per deity or ritual category.
void onJapaTotals(const tlog::Stamp&, char* fields, void* p) {
  auto* t = static_cast<JapaTotals*>(p);
  char* cursor = fields;
  const char* count = tlog::nextField(&cursor);
  const char* category = tlog::nextField(&cursor);
  if (count == nullptr || category == nullptr) return;
  int i = 0;
  while (i < t->n && strcmp(t->key[i], category) != 0) ++i;
  if (i == t->n) {
    if (t->n >= JapaTotals::kMax) return;
    snprintf(t->key[t->n++], sizeof(t->key[0]), "%s", category);
  }
  t->count[i] += static_cast<uint32_t>(std::max(0, atoi(count)));
  ++t->sessions[i];
}

struct MantrasCtx {
  const Clock* clock;
  DaySeries* series;
  const JapaTotals* totals;
  uint16_t items;
};

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool writeMantras(FsFile& out, void* p) {
  auto* ctx = static_cast<MantrasCtx*>(p);
  const DaySeries& s = *ctx->series;
  tools::JsonOut j(out);
  j.beginObject();
  header(j, *ctx->clock, "mantras");
  const int month = tstats::sumLast(s.a, 30);
  j.num("japa_today", s.a[tstats::kHistoryDays - 1]).num("japa_last_30_days", month);
  j.num("japa_last_90_days", tstats::sumLast(s.a, 90)).num("malas_of_108_last_30_days", month / 108);
  j.num("streak_days", tstats::streak(s.a));
  minuteValue(j, "usual_time", averageMinute(s));
  j.beginArray("by_deity_or_ritual");
  for (int i = 0; i < ctx->totals->n; ++i) {
    j.beginObject()
        .str("category", ctx->totals->key[i])
        .num("japa", static_cast<int>(ctx->totals->count[i]))
        .num("sessions", ctx->totals->sessions[i])
        .endObject();
  }
  j.endArray();
  j.beginArray("days");
  for (int i = 0; i < tstats::kHistoryDays; ++i) {
    if (s.a[i] <= 0) continue;
    char date[12];
    dayKey(date, s.firstDay + i);
    j.beginObject().str("date", date).num("japa", s.a[i]).num("sessions", s.b[i]).endObject();
    ++ctx->items;
  }
  j.endArray();
  j.endObject();
  return j.ok();
}

// ---------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------
struct LibraryCtx {
  const Clock* clock;
  const LibrarySummary* summary;
};

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool writeLibrary(FsFile& out, void* p) {
  const auto* ctx = static_cast<const LibraryCtx*>(p);
  const LibrarySummary& s = *ctx->summary;
  tools::JsonOut j(out);
  j.beginObject();
  header(j, *ctx->clock, "reading-library");
  j.num("books", s.books).num("folders_with_books", s.folders).num("opened", s.opened).num("not_opened", s.notOpened);
  j.num("in_progress", s.inProgress).num("finished", s.finished).dec("average_progress_percent", s.averageProgress);
  j.beginArray("currently_reading");
  for (int i = 0; i < s.currentCount; ++i) {
    j.beginObject().str("title", s.current[i].title).dec("progress_percent", s.current[i].progress).endObject();
  }
  j.endArray();
  j.endObject();
  return j.ok();
}

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool writeGlobal(FsFile& out, void* p) {
  const auto* clock = static_cast<const Clock*>(p);
  const GlobalReadingStats g = GlobalReadingStats::load();
  static constexpr const char* kBuckets[] = {"morning", "afternoon", "evening", "night"};
  tools::JsonOut j(out);
  j.beginObject();
  header(j, *clock, "reading-global");
  j.num("sessions", g.totalSessions).num("reading_seconds", g.totalReadingSeconds);
  j.dec("reading_hours", g.totalReadingSeconds / 3600.0).num("pages_turned", g.totalPagesTurned);
  j.num("books_finished", g.completedBooks).num("longest_streak_days", g.displayLongestReadingStreak());
  ReadingStatsDateTime now;
  if (getCurrentLocalReadingStatsDateTime(now)) {
    j.num("current_streak_days", g.currentReadingStreak(&now.date));
  }
  j.beginObject("seconds_by_time_of_day");
  for (size_t i = 0; i < READING_TIME_BUCKET_COUNT; ++i) j.num(kBuckets[i], g.timeOfDaySeconds[i]);
  j.endObject();
  j.beginArray("seconds_by_weekday");
  for (size_t i = 0; i < READING_DAY_OF_WEEK_COUNT; ++i) j.num(nullptr, g.dayOfWeekSeconds[i]);
  j.endArray();
  j.endObject();
  return j.ok();
}

// Streams a small binary/text file as an array of hex strings.
void hexFile(tools::JsonOut& j, const char* key, const std::string& path) {
  FsFile f;
  if (!Storage.exists(path.c_str()) || !Storage.openFileForRead("STX", path, f)) return;
  j.beginArray(key);
  uint8_t buf[kHexChunkBytes];
  char hex[kHexChunkBytes * 2 + 1];
  int n = 0;
  while ((n = f.read(buf, sizeof(buf))) > 0) {
    for (int i = 0; i < n; ++i) snprintf(hex + i * 2, 3, "%02x", buf[i]);
    hex[n * 2] = '\0';
    j.str(nullptr, hex);
  }
  f.close();
  j.endArray();
}

bool isRestorableCacheFile(const char* name) {
  const size_t n = strlen(name);
  if (n > 4 && (strcmp(name + n - 4, ".tmp") == 0 || strcmp(name + n - 4, ".bak") == 0)) return false;
  return strcmp(name, "progress.bin") == 0 || strcmp(name, "progress_percent.bin") == 0 ||
         strcmp(name, "dictionary_history.txt") == 0 ||
         (strncmp(name, "stats", 5) == 0 && n > 4 && strcmp(name + n - 4, ".bin") == 0);
}

struct BookCtx {
  const Clock* clock;
  const char* path;
  float progress;
};

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool writeBook(FsFile& out, void* p) {
  const auto* ctx = static_cast<const BookCtx*>(p);
  const std::string path = ctx->path;
  const std::string type = book_files::typeFor(path);
  const std::string cache = book_files::cachePathFor(path);
  std::string title;
  std::string author;
  for (const RecentBook& b : RECENT_BOOKS.getBooks()) {
    if (b.path == path) {
      title = b.title;
      author = b.author;
      break;
    }
  }
  const size_t slash = path.rfind('/');
  const std::string file = slash == std::string::npos ? path : path.substr(slash + 1);
  const BookReadingStats st = BookReadingStats::load(cache);

  tools::JsonOut j(out);
  j.beginObject();
  header(j, *ctx->clock, "reading-book");
  j.num("pocketdeck_book_data", 1).str("path", path.c_str()).str("file", file.c_str());
  j.str("title", title.empty() ? file.c_str() : title.c_str()).str("author", author.c_str());
  j.str("format", type.c_str()).dec("progress_percent", ctx->progress);

  j.beginObject("reading");
  char buf[40];
  j.num("sessions", st.sessionCount).num("time_read_seconds", st.totalReadingSeconds);
  BookReadingStats::formatDuration(st.totalReadingSeconds, buf, sizeof(buf));
  j.str("time_read", buf).num("pages_turned", st.totalPagesTurned).boolean("finished", st.isCompleted);
  j.num("average_seconds_per_page", st.avgSecondsPerForwardPage);
  if (st.estimatedTimeLeftSeconds > 0) {
    j.num("time_left_seconds", st.estimatedTimeLeftSeconds);
  } else {
    j.null("time_left_seconds");
  }
  if (st.startDate.isValid()) {
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u", st.startDate.year, st.startDate.month, st.startDate.day);
    j.str("started", buf);
  } else {
    j.null("started");
  }
  if (st.finishedDate.isValid()) {
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u", st.finishedDate.year, st.finishedDate.month, st.finishedDate.day);
    j.str("finished_on", buf);
  } else {
    j.null("finished_on");
  }
  static constexpr const char* kBuckets[] = {"morning", "afternoon", "evening", "night"};
  j.beginObject("seconds_by_time_of_day");
  for (size_t i = 0; i < READING_TIME_BUCKET_COUNT; ++i) j.num(kBuckets[i], st.timeOfDaySeconds[i]);
  j.endObject();
  j.beginArray("seconds_by_weekday");
  for (size_t i = 0; i < READING_DAY_OF_WEEK_COUNT; ++i) j.num(nullptr, st.dayOfWeekSeconds[i]);
  j.endArray();
  j.endObject();

  // Bookmarks and clippings through their stores (loaded, copied out, unloaded).
  j.beginArray("bookmarks");
  if (!type.empty() && BookmarkStore::countForBook(path, type) > 0) {
    auto& store = BookmarkStore::getInstance();
    if (store.loadForBook(path, title, author, type)) {
      for (const Bookmark& b : store.getBookmarks()) {
        j.beginObject().str("chapter", b.chapterTitle).dec("chapter_progress", b.progress, 3);
        j.num("spine_index", b.spineIndex).str("snippet", b.snippet).endObject();
      }
      store.unload();
    }
  }
  j.endArray();
  j.beginArray("clippings");
  if (!type.empty() && ClippingStore::countForBook(path, type) > 0) {
    auto& store = ClippingStore::getInstance();
    if (store.loadForBook(path, title, author, type)) {
      std::string text;
      for (size_t i = 0; i < store.clippingCount(); ++i) {
        const Clipping* c = store.clippingAt(i);
        if (c == nullptr) continue;
        text.clear();
        store.readClippingText(i, text);
        j.beginObject().str("chapter", c->chapterTitle).num("spine_index", c->spineIndex);
        j.num("words", c->wordCount).str("text", text.c_str()).endObject();
      }
      store.unload();
    }
  }
  j.endArray();
  j.beginArray("looked_up_words");
  {
    FsFile f;
    const std::string history = cache + "/dictionary_history.txt";
    if (Storage.exists(history.c_str()) && Storage.openFileForRead("STX", history, f)) {
      char line[64];
      while (tools::readLine(f, line, sizeof(line)) >= 0) {
        char* bar = strrchr(line, '|');
        if (bar != nullptr) *bar = '\0';
        if (line[0] != '\0') j.str(nullptr, line);
      }
      f.close();
    }
  }
  j.endArray();

  // Exact copies of the reader's own files, for Stats > Import on another
  // card or device.
  j.beginObject("restore");
  j.num("format", 1);
  j.beginObject("cache");
  if (!cache.empty()) {
    FsFile dir = Storage.open(cache.c_str());
    if (dir && dir.isDirectory()) {
      char names[8][32] = {};
      int nameCount = 0;
      for (FsFile e = dir.openNextFile(); e && nameCount < 8; e = dir.openNextFile()) {
        char name[48];
        e.getName(name, sizeof(name));
        const bool keep = !e.isDirectory() && e.size() <= 64 * 1024 && isRestorableCacheFile(name);
        e.close();
        if (keep && strlen(name) < sizeof(names[0])) snprintf(names[nameCount++], sizeof(names[0]), "%s", name);
      }
      dir.close();
      for (int i = 0; i < nameCount; ++i) hexFile(j, names[i], cache + "/" + names[i]);
    } else if (dir) {
      dir.close();
    }
  }
  j.endObject();
  if (!type.empty()) {
    hexFile(j, "bookmarks", BookmarkStore::storeFilePathFor(path, type));
    hexFile(j, "clippings", ClippingStore::storeFilePathFor(path, type));
  }
  j.endObject();
  j.endObject();
  return j.ok();
}

struct BooksWalk {
  const Clock* clock;
  uint16_t written;
  uint16_t failed;
};

void bookFileName(char* buf, const size_t len, const char* path) {
  const char* slash = strrchr(path, '/');
  const char* name = slash != nullptr ? slash + 1 : path;
  char stem[48];
  size_t n = 0;
  for (const char* c = name; *c != '\0' && n + 1 < sizeof(stem); ++c) {
    const auto u = static_cast<uint8_t>(*c);
    stem[n++] = (u < 0x20 || strchr("\\/:*?\"<>|", *c) != nullptr) ? '_' : *c;
  }
  stem[n] = '\0';
  char* dot = strrchr(stem, '.');
  if (dot != nullptr && dot != stem) *dot = '\0';
  uint32_t h = 2166136261u;
  for (const char* c = path; *c != '\0'; ++c) h = (h ^ static_cast<uint8_t>(*c)) * 16777619u;
  snprintf(buf, len, "%s/reading/books/%s_%04lx.json", kRoot, stem, static_cast<unsigned long>(h & 0xFFFF));
}

void onBook(const char* path, const float progress, void* p) {
  auto* w = static_cast<BooksWalk*>(p);
  char out[160];
  bookFileName(out, sizeof(out), path);
  BookCtx ctx{w->clock, path, progress};
  if (tools::writeFileAtomic(out, &writeBook, &ctx)) {
    ++w->written;
  } else {
    ++w->failed;
    LOG_ERR("STX", "Book export failed: %s", path);
  }
}

bool writeReadme(FsFile& out, void*) {
  return tools::writeText(out,
                          "PocketDeck-OS statistics export\n"
                          "===============================\n\n"
                          "Created by Tools > Stats & export > Export all. Every run replaces these files.\n\n"
                          "habits/habits.json          streaks, completion, usual check-in time, 12 weeks\n"
                          "medicine/medicine.json      courses: start, end, stop, doses taken and when\n"
                          "mood/mood.json              daily moods (1-5) with averages\n"
                          "pomodoro/pomodoro.json      focus sessions per day\n"
                          "today/today.json            to-dos done per day\n"
                          "flashcards/flashcards.json  study sessions per deck and day, mastery\n"
                          "knowledge/knowledge.json    answers opened per day, topic coverage\n"
                          "quotes/quotes.json          days opened, favourite quotes\n"
                          "mantras/mantras.json        japa counts per day and per deity or ritual\n"
                          "reading/library.json        books on the card, opened, finished\n"
                          "reading/global.json         total reading time, sessions, streaks\n"
                          "reading/books/*.json        one file per book: progress, time read,\n"
                          "                            bookmarks, clippings, looked-up words\n\n"
                          "Moving books to another PocketDeck-OS device: copy the book to the same\n"
                          "folder, copy this /stats folder, then run Tools > Stats & export >\n"
                          "Import book data. Existing data on that device is never overwritten.\n"
                          "The raw history the stats come from lives in /tools/<feature>/log-*.txt.\n");
}

bool writeJson(const char* folder, const char* file, tools::WriteFn fn, const void* ctx) {
  if (!ensureFolder(folder)) return false;
  char path[80];
  snprintf(path, sizeof(path), "%s/%s/%s", kRoot, folder, file);
  // Writers only read through ctx; WriteFn's signature needs a mutable pointer.
  const bool ok = tools::writeFileAtomic(path, fn, const_cast<void*>(ctx));
  if (!ok) LOG_ERR("STX", "Export failed: %s", path);
  return ok;
}
}  // namespace

const char* featureFolder(const Feature f) {
  switch (f) {
    case Feature::Habits:
      return "habits";
    case Feature::Medicine:
      return "medicine";
    case Feature::Mood:
      return "mood";
    case Feature::Pomodoro:
      return "pomodoro";
    case Feature::Today:
      return "today";
    case Feature::Flashcards:
      return "flashcards";
    case Feature::Knowledge:
      return "knowledge";
    case Feature::Quotes:
      return "quotes";
    case Feature::Mantras:
      return "mantras";
    default:
      return "reading";
  }
}

namespace {
FeatureResult exportOne(const Feature f, const Clock& clock) {
  FeatureResult r;
  if (f != Feature::Reading && !clock.valid) return r;  // tool stats need dates
  if (f == Feature::Habits) {
    HabitsCtx h{&clock, 0};
    return {writeJson("habits", "habits.json", &writeHabits, &h), h.items};
  }
  if (f == Feature::Medicine) {
    // Course and dose scratch live on the heap only for this export.
    auto courses = makeUniqueNoThrow<meds::Course[]>(meds::kMaxCourses);
    auto doses = makeUniqueNoThrow<int16_t[]>(meds::kMaxDays * meds::kMaxDoses);
    if (!courses || !doses) return r;
    MedsCtx m{&clock, 0, courses.get(), doses.get()};
    return {writeJson("medicine", "medicine.json", &writeMeds, &m), m.items};
  }
  if (f == Feature::Reading) {
    bool ok = ensureFolder("reading") && ensureFolder("reading/books");
    BooksWalk walk{&clock, 0, 0};
    const LibrarySummary summary = LibrarySummary::scan(&onBook, &walk);
    LibraryCtx lib{&clock, &summary};
    ok = writeJson("reading", "library.json", &writeLibrary, &lib) && ok;
    ok = writeJson("reading", "global.json", &writeGlobal, &clock) && ok;
    return {ok && walk.failed == 0, walk.written};
  }
  auto series = makeUniqueNoThrow<DaySeries>();
  if (!series) return r;
  switch (f) {
    case Feature::Mood: {
      tstats::scanSeries("mood", clock.today, &tstats::onMood, *series, charts::kNoValue);
      SeriesCtx c{&clock, series.get(), 0};
      return {writeJson("mood", "mood.json", &writeMood, &c), c.items};
    }
    case Feature::Pomodoro: {
      tstats::scanSeries("pomodoro", clock.today, &tstats::onPomodoro, *series);
      SeriesCtx c{&clock, series.get(), 0};
      return {writeJson("pomodoro", "pomodoro.json", &writePomodoro, &c), c.items};
    }
    case Feature::Today: {
      tstats::scanSeries("today", clock.today, &tstats::onToday, *series);
      SeriesCtx c{&clock, series.get(), 0};
      return {writeJson("today", "today.json", &writeToday, &c), c.items};
    }
    case Feature::Flashcards: {
      tstats::scanSeries("flashcards", clock.today, &tstats::onFlashcards, *series);
      auto decks = makeUniqueNoThrow<DeckTotals>();
      auto srs = makeUniqueNoThrow<tstats::SrsSummary>();
      if (!decks || !srs) return r;
      tlog::scan("flashcards", series->firstDay, clock.today, &tstats::onDeck, decks.get());
      tstats::summarizeSrs(clock.today, *srs);
      FlashCtx c{&clock, series.get(), decks.get(), srs.get(), 0};
      return {writeJson("flashcards", "flashcards.json", &writeFlashcards, &c), c.items};
    }
    case Feature::Knowledge: {
      tstats::scanSeries("knowledge", clock.today, &tstats::onKnowledge, *series);
      auto cover = makeUniqueNoThrow<tstats::KnowledgeCoverage>();
      if (!cover) return r;
      tstats::summarizeKnowledge(clock.today, *cover);
      KnowledgeCtx c{&clock, series.get(), cover.get(), 0};
      return {writeJson("knowledge", "knowledge.json", &writeKnowledge, &c), c.items};
    }
    case Feature::Quotes: {
      tstats::scanSeries("quotes", clock.today, &tstats::onQuotes, *series);
      QuotesCtx c{&clock, series.get(), 0};
      return {writeJson("quotes", "quotes.json", &writeQuotes, &c), c.items};
    }
    case Feature::Mantras: {
      tstats::scanSeries("mantras", clock.today, &tstats::onMantras, *series);
      auto totals = makeUniqueNoThrow<JapaTotals>();
      if (!totals) return r;
      tlog::scan("mantras", series->firstDay, clock.today, &onJapaTotals, totals.get());
      MantrasCtx c{&clock, series.get(), totals.get(), 0};
      return {writeJson("mantras", "mantras.json", &writeMantras, &c), c.items};
    }
    default:
      return r;
  }
}

bool prepareRoot() {
  if (!Storage.ensureDirectoryExists(kRoot)) {
    LOG_ERR("STX", "Cannot create %s", kRoot);
    return false;
  }
  tools::writeFileAtomic("/stats/README.txt", &writeReadme, nullptr);
  return true;
}
}  // namespace

FeatureResult exportFeature(const Feature f) {
  if (!prepareRoot()) return {};
  return exportOne(f, readClock());
}

ExportReport exportAll(const ProgressFn progress, void* pctx) {
  ExportReport report;
  const Clock clock = readClock();
  report.clockValid = clock.valid;
  if (!prepareRoot()) return report;
  for (int i = 0; i < static_cast<int>(Feature::Count); ++i) {
    const auto f = static_cast<Feature>(i);
    report.features[i] = exportOne(f, clock);
    if (progress != nullptr) progress(f, pctx);
  }
  return report;
}

// ---------------------------------------------------------------------------
// Import
// ---------------------------------------------------------------------------
namespace {
using Tok = tools::JsonReader::Token;

int hexNibble(const char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

struct HexWrite {
  tools::JsonReader* reader;
};

// Streams the hex-string array the reader is positioned in (after '[').
// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool writeFromHex(FsFile& out, void* p) {
  auto* h = static_cast<HexWrite*>(p);
  char chunk[kHexChunkBytes * 2 + 4];
  uint8_t bytes[kHexChunkBytes];
  while (true) {
    const Tok t = h->reader->next(chunk, sizeof(chunk));
    if (t == Tok::ArrayEnd) return true;
    if (t == Tok::Comma) continue;
    if (t != Tok::String) return false;
    const size_t len = strlen(chunk);
    if (len % 2 != 0 || len / 2 > sizeof(bytes)) return false;
    for (size_t i = 0; i < len / 2; ++i) {
      const int hi = hexNibble(chunk[2 * i]);
      const int lo = hexNibble(chunk[2 * i + 1]);
      if (hi < 0 || lo < 0) return false;
      bytes[i] = static_cast<uint8_t>(hi << 4 | lo);
    }
    if (out.write(bytes, len / 2) != len / 2) return false;
  }
}

// Consumes one value; if it is a hex array and `target` does not exist yet,
// writes it there. Returns false only on a malformed file.
bool restoreArray(tools::JsonReader& r, const std::string& target, ImportReport& rep, bool& restoredAny) {
  char scratch[8];
  const Tok first = r.next(scratch, sizeof(scratch));
  if (first != Tok::ArrayStart) return r.skipValue(first);
  if (Storage.exists(target.c_str())) {
    ++rep.kept;
    return r.skipValue(first);
  }
  HexWrite h{&r};
  if (tools::writeFileAtomic(target.c_str(), &writeFromHex, &h)) {
    restoredAny = true;
    return true;
  }
  ++rep.failed;
  return false;
}

bool importOne(const char* jsonPath, ImportReport& rep) {
  FsFile f;
  if (!Storage.openFileForRead("STX", jsonPath, f)) return false;
  tools::JsonReader r(f);
  char text[160];
  std::string bookPath;
  std::string type;
  std::string cache;
  bool restoredAny = false;
  bool ok = r.next(text, sizeof(text)) == Tok::ObjectStart;
  while (ok) {
    Tok t = r.next(text, sizeof(text));
    if (t == Tok::ObjectEnd || t == Tok::End) break;
    if (t == Tok::Comma) continue;
    if (t != Tok::String) {
      ok = false;
      break;
    }
    const std::string key = text;
    if (r.next(text, sizeof(text)) != Tok::Colon) {
      ok = false;
      break;
    }
    if (key == "path") {
      t = r.next(text, sizeof(text));
      if (t != Tok::String) {
        ok = false;
        break;
      }
      bookPath = text;
      if (!Storage.exists(bookPath.c_str())) {
        ++rep.missing;
        break;
      }
      type = book_files::typeFor(bookPath);
      cache = book_files::cachePathFor(bookPath);
      continue;
    }
    if (key != "restore" || bookPath.empty() || type.empty()) {
      t = r.next(text, sizeof(text));
      ok = r.skipValue(t);
      continue;
    }
    // "restore": { "format": 1, "cache": {name: [hex]}, "bookmarks": [hex], "clippings": [hex] }
    if (r.next(text, sizeof(text)) != Tok::ObjectStart) {
      ok = false;
      break;
    }
    while (ok) {
      t = r.next(text, sizeof(text));
      if (t == Tok::ObjectEnd) break;
      if (t == Tok::Comma) continue;
      if (t != Tok::String || r.next(nullptr, 0) != Tok::Colon) {
        ok = false;
        break;
      }
      const std::string part = text;
      if (part == "cache") {
        ok = r.next(text, sizeof(text)) == Tok::ObjectStart && !cache.empty() &&
             Storage.ensureDirectoryExists(cache.c_str());
        while (ok) {
          t = r.next(text, sizeof(text));
          if (t == Tok::ObjectEnd) break;
          if (t == Tok::Comma) continue;
          if (t != Tok::String || r.next(nullptr, 0) != Tok::Colon || !isRestorableCacheFile(text)) {
            ok = false;
            break;
          }
          ok = restoreArray(r, cache + "/" + text, rep, restoredAny);
        }
      } else if (part == "bookmarks") {
        ok = Storage.ensureDirectoryExists("/.pocketdeck-os/bookmarks") &&
             restoreArray(r, BookmarkStore::storeFilePathFor(bookPath, type), rep, restoredAny);
      } else if (part == "clippings") {
        ok = Storage.ensureDirectoryExists("/.pocketdeck-os/clippings") &&
             restoreArray(r, ClippingStore::storeFilePathFor(bookPath, type), rep, restoredAny);
      } else {
        t = r.next(text, sizeof(text));
        ok = r.skipValue(t);
      }
    }
  }
  f.close();
  if (restoredAny) ++rep.restored;
  if (!ok) {
    ++rep.failed;
    LOG_ERR("STX", "Import stopped in %s", jsonPath);
  }
  return ok;
}
}  // namespace

ImportReport importBooks() {
  ImportReport rep;
  static constexpr char kBooks[] = "/stats/reading/books";
  FsFile dir = Storage.open(kBooks);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return rep;
  }
  // Collect names first so only one directory handle is open while importing.
  constexpr int kMaxFiles = 64;
  auto names = makeUniqueNoThrow<char[]>(kMaxFiles * 64);
  if (!names) {
    dir.close();
    return rep;
  }
  int count = 0;
  for (FsFile e = dir.openNextFile(); e && count < kMaxFiles; e = dir.openNextFile()) {
    char name[64];
    e.getName(name, sizeof(name));
    const bool isJson = !e.isDirectory() && FsHelpers::checkFileExtension(std::string_view(name), ".json");
    e.close();
    if (isJson) snprintf(&names[count++ * 64], 64, "%s", name);
  }
  dir.close();
  for (int i = 0; i < count; ++i) {
    char path[96];
    snprintf(path, sizeof(path), "%s/%s", kBooks, &names[i * 64]);
    ++rep.books;
    importOne(path, rep);
  }
  return rep;
}

}  // namespace statsx
