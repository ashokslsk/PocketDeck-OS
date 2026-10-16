#pragma once

#include <atomic>

#include "StatsExport.h"
#include "ToolsCommon.h"
#include "activities/Activity.h"

// Tools > Stats & export. Confirm writes every feature's statistics to
// /stats/<feature>/ as JSON (see StatsExport.h); Left (Import) restores book
// data (progress, reading time, bookmarks, clippings, look-ups) from
// /stats/reading/books for books on this card.
class StatsActivity final : public Activity {
 public:
  explicit StatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Stats", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return pending_ != Pending::None; }

 private:
  enum class Pending : uint8_t { None, Export, Import };
  enum class Shown : uint8_t { Intro, Exported, Imported };

  void confirmImport();

  tools::ToolInput input_;
  Pending pending_ = Pending::None;
  std::atomic<bool> busyShown_{false};  // set by render(), read by loop()
  Shown shown_ = Shown::Intro;
  statsx::ExportReport exportReport_ = {};
  statsx::ImportReport importReport_ = {};
  bool transitionPending_ = true;
};
