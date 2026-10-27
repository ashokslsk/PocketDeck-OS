#pragma once

#include "activities/Activity.h"

// Last screen before Shut down: the PocketDeck mark, "Device is off now", a
// reading quote and how to turn the device on. E-ink keeps it with no power,
// so it is drawn once with a clean (non-differential) refresh.
class PoweredOffActivity final : public Activity {
 public:
  PoweredOffActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("PoweredOff", renderer, mappedInput) {}
  void onEnter() override;
};
