#pragma once

#include <cstdint>

#include "ToolsCommon.h"
#include "activities/Activity.h"

// Mood > Menu > History and notes: every logged day of the last 90, newest
// first, with the time it was logged, the mood and the note. Left/Right page.
class MoodHistoryActivity final : public Activity {
 public:
  explicit MoodHistoryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("MoodHistory", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

  static constexpr int kMaxEntries = 90;
  static constexpr size_t kNoteCap = 64;

  struct Entry {
    int32_t day = 0;
    int16_t minute = -1;  // -1 = logged on a later day
    uint8_t mood = 0;
    char note[kNoteCap] = {};
  };

 private:
  tools::ToolInput input_;
  Entry entries_[kMaxEntries];
  int count_ = 0;
  int page_ = 0;
  int perPage_ = 1;
  bool transitionPending_ = true;
};
