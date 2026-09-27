#pragma once

#include <I18n.h>

#include "activities/Activity.h"
#include "activities/tools/ToolsCommon.h"

// A titled, paged help page (Markdown subset) for settings that need an
// explanation, such as Sleep > How to add wallpapers. Up/Down page, Back returns.
class HelpTextActivity final : public Activity {
 public:
  HelpTextActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, StrId title, StrId body)
      : Activity("HelpText", renderer, mappedInput), title_(title), body_(body) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  StrId title_;
  StrId body_;
  tools::ToolInput input_;
  int page_ = 0;
  int pageCount_ = 1;
};
