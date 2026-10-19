#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Reader menu > Speed Read: pick a pace. Presets per language, chosen for the
// book's script (Kannada words are longer, so the pace is lower):
//   English 125 / 200 / 275 / 350 words per minute
//   Kannada  60 / 100 / 150 / 200 / 250 / 350 words per minute
// Returns OptionSelectionResult{index}; wpmFor() maps it to words per minute.
class SpeedReadPickerActivity final : public Activity {
 public:
  static constexpr int kMaxPresetCount = 6;
  static int presetCount(bool kannada) { return kannada ? 6 : 4; }
  static uint16_t wpmFor(bool kannada, int index);

  SpeedReadPickerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool kannada, int selected)
      : Activity("SpeedReadPicker", renderer, mappedInput), kannada_(kannada), selected_(selected) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool allowPowerAsConfirmInReaderMode() const override { return true; }

 private:
  bool kannada_;
  int selected_;
  ButtonNavigator buttonNavigator_;
};
