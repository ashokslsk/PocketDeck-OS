#pragma once

#include "ToolsCharts.h"
#include "ToolsCommon.h"
#include "activities/Activity.h"

// Daily mood check-in on a five-point scale (Awful, Low, Okay, Good, Great)
// with an optional note. Entries are appended to /tools/mood/log-YYYY-MM.txt
// as "YYYY-MM-DD HH:MM|day|mood 1-5|note"; the latest entry for a day wins.
// The screen shows a 30-day mood line and stats: 7- and 30-day averages, the
// week-on-week trend, most frequent mood, check-in streak, best weekday and
// the usual check-in time.
class MoodActivity final : public Activity {
 public:
  explicit MoodActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Mood", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

  static constexpr int kDays = 60;
  static constexpr int kChartDays = 30;
  static constexpr size_t kNoteCap = 64;

 private:
  void load();
  void computeStats();
  void save(const char* note);
  void editNote();
  void openMenu();

  tools::ToolInput input_;
  bool clockValid_ = false;
  int32_t today_ = 0;
  int offset_ = 0;              // 0 = today, 1 = yesterday, ...
  int cursor_ = 3;              // 0..4 face under the cursor
  int16_t mood_[kDays] = {};    // oldest first, kNoValue = no entry
  int16_t minute_[kDays] = {};  // time the entry was made (same-day only)
  char note_[kNoteCap] = {};    // note for the selected day
  charts::Stat stats_[8] = {};
  bool transitionPending_ = true;
};
