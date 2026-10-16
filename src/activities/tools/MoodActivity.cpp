#include "MoodActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "ToolsLog.h"
#include "MoodHistoryActivity.h"
#include "ToolStatsPages.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "activities/util/OptionSelectionActivity.h"
#include "fontIds.h"

namespace {
constexpr char kFeature[] = "mood";

const char* moodName(const int mood) {
  switch (mood) {
    case 1:
      return tr(STR_TOOLS_MOOD_AWFUL);
    case 2:
      return tr(STR_TOOLS_MOOD_LOW);
    case 3:
      return tr(STR_TOOLS_MOOD_OKAY);
    case 4:
      return tr(STR_TOOLS_MOOD_GOOD);
    default:
      return tr(STR_TOOLS_MOOD_GREAT);
  }
}

// Line-art face: circle, two eyes and a mouth whose curve follows the mood
// (deep frown for 1 through a wide smile for 5).
void drawFace(const GfxRenderer& r, const int cx, const int cy, const int radius, const int mood) {
  for (int a = 0; a < 720; ++a) {
    const float t = static_cast<float>(a) * 3.14159265f / 360.0f;
    for (int k = 0; k < 2; ++k) {
      r.drawPixel(cx + static_cast<int>(std::lround((radius - k) * std::cos(t))),
                  cy + static_cast<int>(std::lround((radius - k) * std::sin(t))), true);
    }
  }
  const int eyeDx = radius * 35 / 100;
  const int eyeY = cy - radius * 25 / 100;
  r.fillRect(cx - eyeDx - 2, eyeY - 2, 5, 5);
  r.fillRect(cx + eyeDx - 2, eyeY - 2, 5, 5);
  const float curve = (mood - 3) * 0.28f;  // + smile, - frown
  const int half = radius / 2;
  const int baseY = cy + radius * 35 / 100 - static_cast<int>(curve * half * 0.5f);
  int prevX = cx - half;
  int prevY = baseY;
  for (int x = -half; x <= half; x += 2) {
    const float f = 1.0f - static_cast<float>(x * x) / static_cast<float>(half * half);
    const int y = baseY + static_cast<int>(curve * half * f);
    r.drawLine(prevX, prevY, cx + x, y, 2, true);
    prevX = cx + x;
    prevY = y;
  }
}

struct LoadCtx {
  int32_t firstDay;
  int16_t* mood;
  int16_t* minute;
  int32_t noteDay;
  char* note;
  size_t noteCap;
};

void onLogLine(const tlog::Stamp& at, char* fields, void* ctx) {
  auto* l = static_cast<LoadCtx*>(ctx);
  char* cursor = fields;
  const char* date = tlog::nextField(&cursor);
  const char* mood = tlog::nextField(&cursor);
  const char* note = cursor;  // rest of the line (may contain '|')
  int32_t day = 0;
  if (date == nullptr || mood == nullptr || !tools::parseIsoDate(date, day)) return;
  const int idx = day - l->firstDay;
  const int m = atoi(mood);
  if (idx < 0 || idx >= MoodActivity::kDays || m < 1 || m > 5) return;
  l->mood[idx] = static_cast<int16_t>(m);
  l->minute[idx] = day == at.day ? at.minute : charts::kNoValue;
  if (day == l->noteDay) snprintf(l->note, l->noteCap, "%s", note != nullptr ? note : "");
}
}  // namespace

void MoodActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  tlog::Stamp now;
  clockValid_ = tlog::now(now);
  today_ = now.day;
  if (clockValid_) load();
  transitionPending_ = true;
  requestUpdate();
}

void MoodActivity::load() {
  std::fill(mood_, mood_ + kDays, charts::kNoValue);
  std::fill(minute_, minute_ + kDays, charts::kNoValue);
  note_[0] = '\0';
  LoadCtx ctx{today_ - (kDays - 1), mood_, minute_, today_ - offset_, note_, sizeof(note_)};
  tlog::scan(kFeature, ctx.firstDay, today_, &onLogLine, &ctx);
  const int16_t current = mood_[kDays - 1 - offset_];
  cursor_ = current != charts::kNoValue ? current - 1 : 3;
  computeStats();
}

void MoodActivity::computeStats() {
  auto average = [this](const int fromBack, const int count, int& n) {
    long sum = 0;
    n = 0;
    for (int i = 0; i < count; ++i) {
      const int idx = kDays - 1 - fromBack - i;
      if (idx < 0 || mood_[idx] == charts::kNoValue) continue;
      sum += mood_[idx];
      ++n;
    }
    return n > 0 ? static_cast<float>(sum) / n : 0.0f;
  };
  int n7 = 0, n30 = 0, nPrev = 0;
  const float avg7 = average(0, 7, n7);
  const float avg30 = average(0, 30, n30);
  const float prev7 = average(7, 7, nPrev);
  int counts[6] = {};
  long minuteSum = 0;
  int minuteN = 0;
  float weekdaySum[7] = {};
  int weekdayN[7] = {};
  for (int i = 0; i < kDays; ++i) {
    if (mood_[i] == charts::kNoValue) continue;
    const int32_t day = today_ - (kDays - 1) + i;
    if (i >= kDays - 30) ++counts[mood_[i]];
    const uint8_t wd = tools::weekdayMon0(day);
    weekdaySum[wd] += mood_[i];
    ++weekdayN[wd];
    if (minute_[i] != charts::kNoValue) {
      minuteSum += minute_[i];
      ++minuteN;
    }
  }
  int streak = 0;
  for (int i = kDays - 1; i >= 0; --i) {
    if (mood_[i] == charts::kNoValue) {
      if (i == kDays - 1) continue;  // today not logged yet does not break it
      break;
    }
    ++streak;
  }
  const int common = static_cast<int>(std::max_element(counts + 1, counts + 6) - counts);
  int bestWd = -1;
  float bestAvg = 0.0f;
  for (int d = 0; d < 7; ++d) {
    if (weekdayN[d] == 0) continue;
    const float a = weekdaySum[d] / weekdayN[d];
    if (bestWd < 0 || a > bestAvg) {
      bestWd = d;
      bestAvg = a;
    }
  }

  int i = 0;
  auto put = [&](const char* label) { stats_[i++].label = label; };
  if (n7 > 0) {
    snprintf(stats_[i].value, sizeof(stats_[i].value), "%.1f %s", avg7, moodName(static_cast<int>(avg7 + 0.5f)));
  } else {
    snprintf(stats_[i].value, sizeof(stats_[i].value), "-");
  }
  put(tr(STR_TOOLS_MOOD_AVG_7));
  if (n30 > 0) {
    snprintf(stats_[i].value, sizeof(stats_[i].value), "%.1f", avg30);
  } else {
    snprintf(stats_[i].value, sizeof(stats_[i].value), "-");
  }
  put(tr(STR_TOOLS_MOOD_AVG_30));
  if (n7 > 0 && nPrev > 0) {
    const float delta = avg7 - prev7;
    snprintf(stats_[i].value, sizeof(stats_[i].value), "%s %+.1f",
             delta > 0.05f    ? tr(STR_TOOLS_TREND_UP)
             : delta < -0.05f ? tr(STR_TOOLS_TREND_DOWN)
                              : tr(STR_TOOLS_TREND_STEADY),
             delta);
  } else {
    snprintf(stats_[i].value, sizeof(stats_[i].value), "-");
  }
  put(tr(STR_TOOLS_MOOD_TREND));
  if (counts[common] > 0) {
    snprintf(stats_[i].value, sizeof(stats_[i].value), "%s (%d)", moodName(common), counts[common]);
  } else {
    snprintf(stats_[i].value, sizeof(stats_[i].value), "-");
  }
  put(tr(STR_TOOLS_MOOD_MOST_COMMON));
  snprintf(stats_[i].value, sizeof(stats_[i].value), "%d / 30", n30);
  put(tr(STR_TOOLS_MOOD_DAYS_LOGGED));
  snprintf(stats_[i].value, sizeof(stats_[i].value), "%d", streak);
  put(tr(STR_TOOLS_MOOD_STREAK));
  snprintf(stats_[i].value, sizeof(stats_[i].value), "%s",
           bestWd >= 0 ? tools::weekdayName(static_cast<uint8_t>(bestWd)) : "-");
  put(tr(STR_TOOLS_STAT_BEST_DAY));
  if (minuteN > 0) {
    charts::formatMinute(stats_[i].value, sizeof(stats_[i].value), static_cast<int>(minuteSum / minuteN));
  } else {
    snprintf(stats_[i].value, sizeof(stats_[i].value), "-");
  }
  put(tr(STR_TOOLS_STAT_USUAL_TIME));
}

void MoodActivity::save(const char* note) {
  tlog::Stamp at;
  if (!tlog::now(at)) return;
  char date[12];
  tools::formatIsoDate(date, sizeof(date), today_ - offset_);
  char fields[tlog::kLineCap - 20];
  snprintf(fields, sizeof(fields), "%s|%d|%s", date, cursor_ + 1, note);
  if (tlog::append(kFeature, at, fields)) {
    RenderLock lock(*this);
    snprintf(note_, sizeof(note_), "%s", note);
    mood_[kDays - 1 - offset_] = static_cast<int16_t>(cursor_ + 1);
    minute_[kDays - 1 - offset_] = offset_ == 0 ? at.minute : charts::kNoValue;
    computeStats();
  }
}

void MoodActivity::editNote() {
  auto kb =
      makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_TOOLS_MOOD_NOTE), note_, kNoteCap - 1);
  if (!kb) return;
  startActivityForResult(std::move(kb), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    const auto* text = std::get_if<KeyboardResult>(&result.data);
    if (!result.isCancelled && text != nullptr) {
      std::string clean = text->text;
      std::replace(clean.begin(), clean.end(), '\n', ' ');
      save(clean.c_str());
    }
    requestUpdate();
  });
}

void MoodActivity::openMenu() {
  std::vector<std::string> options;
  options.reserve(3);
  options.emplace_back(note_[0] != '\0' ? tr(STR_TOOLS_MOOD_EDIT_NOTE) : tr(STR_TOOLS_MOOD_ADD_NOTE));
  options.emplace_back(tr(STR_TOOLS_MOOD_HISTORY));
  options.emplace_back(tr(STR_TOOLS_HABIT_STATS));
  auto picker = makeUniqueNoThrow<OptionSelectionActivity>(renderer, mappedInput, "MoodMenu", StrId::STR_TOOLS_MOOD,
                                                           std::move(options), 0);
  if (!picker) return;
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    const auto* sel = std::get_if<OptionSelectionResult>(&result.data);
    if (result.isCancelled || sel == nullptr) {
      requestUpdate();
      return;
    }
    std::unique_ptr<Activity> next;
    if (sel->index == 0) {
      editNote();
      return;
    }
    if (sel->index == 1) {
      next = makeUniqueNoThrow<MoodHistoryActivity>(renderer, mappedInput);
    } else {
      next = makeUniqueNoThrow<ToolStatsActivity>(renderer, mappedInput, tr(STR_TOOLS_MOOD_STATS),
                                                  &toolstats::buildMood, nullptr, statsx::Feature::Mood);
    }
    if (!next) return;
    startActivityForResult(std::move(next), [this](const ActivityResult&) {
      input_.reset(mappedInput);
      transitionPending_ = true;
      requestUpdate();
    });
  });
}

void MoodActivity::loop() {
  input_.poll(mappedInput);
  if (input_.backLong) {
    tools::exitToHome();
    return;
  }
  if (input_.back) {
    finish();
    return;
  }
  if (!clockValid_) return;
  if (input_.leftUp) {
    openMenu();
    return;
  } else if (input_.right) {
    cursor_ = (cursor_ + 1) % 5;
    requestUpdate();
  } else if ((input_.up || input_.pageBack) && offset_ < kChartDays - 1) {
    RenderLock lock(*this);
    ++offset_;
    load();
    requestUpdate();
  } else if ((input_.down || input_.pageForward) && offset_ > 0) {
    RenderLock lock(*this);
    --offset_;
    load();
    requestUpdate();
  } else if (input_.confirm) {
    save(note_);
    requestUpdate();
  }
}

void MoodActivity::render(RenderLock&&) {
  char subtitle[32] = "";
  if (clockValid_) {
    if (offset_ == 0) {
      snprintf(subtitle, sizeof(subtitle), "%s", tr(STR_TOOLS_TODAY));
    } else {
      tools::formatShortDate(subtitle, sizeof(subtitle), today_ - offset_);
    }
  }
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_MOOD), clockValid_ ? subtitle : nullptr);
  if (!clockValid_) {
    renderer.drawCenteredText(UI_12_FONT_ID, content.y + content.height / 2 - 20, tr(STR_TOOLS_CLOCK_NOT_SET), true,
                              EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, content.y + content.height / 2 + 10, tr(STR_TOOLS_SYNC_FROM_WORLD_CLOCK));
    tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
    renderer.displayBuffer(tools::transitionRefresh());
    transitionPending_ = false;
    return;
  }

  // --- Faces.
  const int16_t saved = mood_[kDays - 1 - offset_];
  const int cellW = content.width / 5;
  const int radius = std::min(30, cellW / 2 - 8);
  int y = content.y + 4;
  for (int m = 0; m < 5; ++m) {
    const int cx = content.x + m * cellW + cellW / 2;
    const int cy = y + radius + 2;
    drawFace(renderer, cx, cy, radius, m + 1);
    if (saved == m + 1) {
      // Saved mood: a solid bar under the face.
      renderer.fillRect(cx - radius, cy + radius + 26, radius * 2, 4);
    }
    if (cursor_ == m) renderer.drawRect(cx - radius - 6, cy - radius - 6, radius * 2 + 12, radius * 2 + 34, 2, true);
    const char* name = moodName(m + 1);
    const int w = renderer.getTextWidth(SMALL_FONT_ID, name, EpdFontFamily::BOLD);
    renderer.drawText(SMALL_FONT_ID, cx - w / 2, cy + radius + 6, name, true, EpdFontFamily::BOLD);
  }
  y += radius * 2 + 44;
  {
    char line[kNoteCap + 16];
    if (note_[0] != '\0') {
      snprintf(line, sizeof(line), "%s: %s", tr(STR_TOOLS_MOOD_NOTE), note_);
    } else {
      snprintf(line, sizeof(line), "%s", tr(STR_TOOLS_MOOD_NOTE_HINT));
    }
    const auto shown = renderer.truncatedText(UI_10_FONT_ID, line, content.width);
    renderer.drawText(UI_10_FONT_ID, content.x, y, shown.c_str());
    y += renderer.getLineHeight(UI_10_FONT_ID) + 8;
  }

  // --- 30-day line.
  const int chartH = 170;
  const Rect plot = charts::drawPanel(renderer, Rect{content.x, y, content.width, chartH}, tr(STR_TOOLS_MOOD_CHART),
                                      tr(STR_TOOLS_CHART_30_DAYS_AGO), tr(STR_TOOLS_TODAY));
  charts::lineChart(renderer, plot, mood_ + (kDays - kChartDays), kChartDays, 1, 5, tr(STR_TOOLS_MOOD_GREAT),
                    tr(STR_TOOLS_MOOD_AWFUL));
  y += chartH + 8;

  // --- Stats.
  charts::statGrid(renderer, Rect{content.x, y, content.width, 0}, 2, stats_, 8);

  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_SAVE), tr(STR_TOOLS_MENU),
                   tr(STR_TOOLS_MOOD_NEXT_FACE));
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
