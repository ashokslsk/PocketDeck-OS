#pragma once

#include "HabitData.h"
#include "ToolsCommon.h"
#include "activities/Activity.h"

// Up to twelve habits x seven days. Each row shows the week as blocks (filled =
// done) plus the current streak. Data is stored per ISO week under
// /tools/habits. Hold Confirm for the habit menu: stats and graphs, add,
// rename or delete a habit.
class HabitTrackerActivity final : public Activity {
 public:
  explicit HabitTrackerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("HabitTracker", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr unsigned long kSaveDelayMs = 1500;
  // Streaks look back at most a year of weekly files.
  static constexpr int kMaxStreakWeeks = 53;

  void showWeek(int32_t monday);
  void saveIfDirty();
  void computeStreaks();
  bool isFuture(int dayIndex) const { return viewMonday_ + dayIndex > today_; }
  void openMenu();
  void addHabit();
  void renameHabit();
  void deleteHabit();
  void reloadNames();

  tools::ToolInput input_;
  habits::Names names_ = {};
  uint8_t bits_[habits::kMaxHabits] = {};
  uint16_t streak_[habits::kMaxHabits] = {};
  int count_ = 0;
  int32_t today_ = 0;
  int32_t viewMonday_ = 0;
  int selHabit_ = 0;
  int selDay_ = 0;
  bool clockValid_ = false;
  bool dirty_ = false;
  unsigned long dirtySinceMs_ = 0;
  bool transitionPending_ = true;
};
