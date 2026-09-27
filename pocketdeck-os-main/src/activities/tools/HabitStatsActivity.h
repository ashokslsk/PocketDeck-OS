#pragma once

#include "HabitData.h"
#include "HabitSummary.h"
#include "ToolsCharts.h"
#include "ToolsCommon.h"
#include "activities/Activity.h"

// Stats and graphs for one habit: streaks, completion rates, the usual time
// of day it gets ticked and how consistent that time is, the average gap
// between check-ins, a 12-week completion line and a 30-day time scatter.
// Ticks come from the weekly files; times come from /tools/habits/log-*.txt.
class HabitStatsActivity final : public Activity {
 public:
  HabitStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* habitName);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void compute();

  tools::ToolInput input_;
  char name_[habits::kNameCap] = {};
  bool clockValid_ = false;
  int32_t today_ = 0;
  habits::Summary summary_;
  charts::Stat stats_[8] = {};
  bool transitionPending_ = true;
};
