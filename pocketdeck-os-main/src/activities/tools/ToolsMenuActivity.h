#pragma once

#include "ToolsCommon.h"
#include "activities/Activity.h"

// Home > Tools launcher. Lists the productivity tools and opens each one as a
// child activity, so returning from a tool lands back here.
class ToolsMenuActivity final : public Activity {
 public:
  explicit ToolsMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Tools", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum Tool : uint8_t { POMODORO, WORLD_CLOCK, HABITS, FLASHCARDS, QUOTE, KNOWLEDGE, PANCHANGA, TODAY, TOOL_COUNT };

  static const char* toolLabel(int index);
  static const char* toolDescription(int index);
  void openTool(int index);

  tools::ToolInput input_;
  int selected_ = 0;
  bool transitionPending_ = true;
};
