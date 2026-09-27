#pragma once

#include "ToolsCommon.h"
#include "activities/Activity.h"

// Local time plus up to four configurable cities.
//
// Time comes from the existing HalClock (RTC) after an NTP sync; Confirm
// opens the existing Settings clock-sync flow, which reuses the normal WiFi
// selection/credential stack. City offsets live in /tools/worldclock.txt as
// "Name|UTC offset|DST rule" so no network is needed to show them.
class WorldClockActivity final : public Activity {
 public:
  explicit WorldClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("WorldClock", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr char kConfigPath[] = "/tools/worldclock.txt";
  static constexpr int kMaxCities = 4;
  static constexpr size_t kNameCap = 24;

  struct City {
    char name[kNameCap];
    int16_t standardOffset;  // minutes east of UTC, standard time
    tools::DstRule rule;
  };

  void loadCities();
  static bool writeDefaults(FsFile& out, void* ctx);
  void syncClock();

  tools::ToolInput input_;
  City cities_[kMaxCities] = {};
  int cityCount_ = 0;
  int lastMinute_ = -1;
  unsigned long lastPollMs_ = 0;
  bool transitionPending_ = true;
};
