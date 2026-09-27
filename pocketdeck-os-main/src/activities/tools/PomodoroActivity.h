#pragma once

#include "ToolsCommon.h"
#include "activities/Activity.h"

// 25/5 Pomodoro timer drawn as a circular progress ring.
//
// Timing uses millis(). While running, the MM:SS readout redraws once a second
// with a fast partial refresh, plus a clean refresh every five minutes so
// e-ink ghosting cannot build up. The timer keeps the device awake only while a
// phase is actually running; when a break ends it stops and waits for the
// user, so auto-sleep can never be held off for more than one focus + break.
class PomodoroActivity final : public Activity {
 public:
  explicit PomodoroActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Pomodoro", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return running_; }

  // Completed focus sessions recorded for `day` (read-only, for Today view).
  static uint16_t completedOn(int32_t day);

 private:
  enum class Phase : uint8_t { Focus, Break };

  static constexpr char kStatePath[] = "/tools/pomodoro.txt";

  void loadState();
  void saveState();
  static bool writeState(FsFile& out, void* ctx);
  uint32_t phaseDurationMs() const;
  uint32_t elapsedMs() const;
  uint32_t remainingSeconds() const;
  void startOrPause();
  void resetPhase();
  void switchPhase(bool completedFocus);

  tools::ToolInput input_;
  Phase phase_ = Phase::Focus;
  bool running_ = false;
  uint32_t phaseStartMs_ = 0;   // millis() when the current run segment began
  uint32_t accumulatedMs_ = 0;  // elapsed time from earlier segments (pauses)
  uint32_t lastShownSeconds_ = UINT32_MAX;
  uint16_t fastRefreshCount_ = 0;  // partial refreshes since the last clean one
  uint8_t focusMinutes_ = 25;
  uint8_t breakMinutes_ = 5;
  uint16_t completedToday_ = 0;
  int32_t statsDay_ = 0;  // days since epoch the counter belongs to
  bool transitionPending_ = true;
  bool phaseJustEnded_ = false;  // flash a clean refresh to draw the eye
};
