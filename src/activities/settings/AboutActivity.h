#pragma once

#include "activities/Activity.h"
#include "activities/tools/ToolsCommon.h"

// Settings > System > About: PocketDeck-OS identity, version, credits and a
// short feature overview. Up/Down pages through the text; Back returns.
class AboutActivity final : public Activity {
 public:
  explicit AboutActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("About", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  tools::ToolInput input_;
  int page_ = 0;
  int pageCount_ = 1;
};
