#pragma once

#include "PanchangaMath.h"
#include "ToolsCommon.h"
#include "activities/Activity.h"

// Offline Kannada panchanga: tithi, vara, nakshatra, yoga, karana, masa,
// samvatsara, sunrise/sunset, Rahu/Yamaganda/Gulika kaala, Abhijit muhurta,
// festivals and a Moon-phase widget, for any date within 5 years of today.
//
// All Kannada text is pre-shaped (HarfBuzz + Noto Sans Kannada) into bitmaps
// at build time; see scripts/panchanga/gen_kannada_bitmaps.py. Location comes
// from /tools/panchanga.txt (default: Bengaluru, UTC+5:30).
class PanchangaActivity final : public Activity {
 public:
  explicit PanchangaActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Panchanga", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr char kConfigPath[] = "/tools/panchanga.txt";
  static constexpr int32_t kRangeDays = 1826;  // five years each way
  static constexpr int kAnimFrames = 5;
  static constexpr unsigned long kAnimFrameMs = 220;

  void loadConfig();
  static bool writeDefaults(FsFile& out, void* ctx);
  void selectDay(int32_t day);
  void startAnimation();

  tools::ToolInput input_;
  double lat_ = 12.9716, lon_ = 77.5946, tz_ = 5.5;
  bool defaultPlace_ = true;
  bool animate_ = true;
  bool clockValid_ = false;
  int32_t today_ = 0;
  int32_t selected_ = 0;
  panchanga::Day day_;
  double moonElongation_ = 0.0;  // at noon (or now, for today)
  int moonNakshatra_ = 0;
  int animFrame_ = kAnimFrames;  // == kAnimFrames when idle
  unsigned long animLastMs_ = 0;
  bool transitionPending_ = true;
};
