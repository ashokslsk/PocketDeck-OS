#pragma once

#include "activities/Activity.h"

// Hold Power for 4 seconds: a popup with two icon buttons, Shut down and
// Restart. Left/Right (or Up/Down) choose, Confirm acts, Back cancels.
class PowerMenuActivity final : public Activity {
 public:
  PowerMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("PowerMenu", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool allowPowerAsConfirmInReaderMode() const override { return false; }

 private:
  int selected = 0;                  // 0 = Shut down, 1 = Restart
  bool waitForPowerRelease = false;  // the hold that opened the menu is still down
};
