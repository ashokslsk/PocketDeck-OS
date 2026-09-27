#pragma once

#include "ToolsCharts.h"
#include "ToolsCommon.h"
#include "activities/Activity.h"

// Pomodoro > Left: focus history from /tools/pomodoro/log-*.txt. Sessions
// today / this week / last 30 days, focus hours, active-day streak, best day,
// usual start time, a 14-day sessions bar chart and a 30-day start-time plot.
class PomodoroStatsActivity final : public Activity {
 public:
  PomodoroStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool keepAwake)
      : Activity("PomodoroStats", renderer, mappedInput), keepAwake_(keepAwake) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  // A running timer underneath still needs the device awake.
  bool preventAutoSleep() override { return keepAwake_; }

  static constexpr int kDays = 30;
  static constexpr int kBarDays = 14;

 private:
  tools::ToolInput input_;
  bool keepAwake_ = false;
  bool clockValid_ = false;
  int32_t today_ = 0;
  int16_t sessions_[kDays] = {};
  int16_t minutes_[kDays] = {};
  int16_t firstStart_[kDays] = {};
  charts::Stat stats_[8] = {};
  bool transitionPending_ = true;
};
