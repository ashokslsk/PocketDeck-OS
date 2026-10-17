#pragma once

#include <HalStorage.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "activities/Activity.h"
#include "activities/tools/ToolsCommon.h"

// Browse Files > Options > Info: a titled list of label/value rows (name,
// type, size, location, reading progress or folder contents). Back returns.
class FileInfoActivity final : public Activity {
 public:
  using Row = std::pair<std::string, std::string>;

  FileInfoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string title, std::vector<Row> rows)
      : Activity("FileInfo", renderer, mappedInput), title_(std::move(title)), rows_(std::move(rows)) {}

  // Draws a used-space bar under row `row` (permille: 0-1000 of the card used)
  // and counts the card's files beneath it.
  void setUsageBar(const int row, const int usedPermille) {
    usageRow_ = row;
    usedPermille_ = usedPermille;
  }

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::string title_;
  std::vector<Row> rows_;
  tools::ToolInput input_;
  int usageRow_ = -1;
  int usedPermille_ = 0;

  // Card statistics, gathered a slice per loop() so Back stays responsive on a
  // card with tens of thousands of books. Hidden folders (app caches, system
  // data) are skipped. The last complete count is kept on the card and shown
  // straight away while a fresh count runs.
  struct CardStats {
    uint32_t files = 0;
    uint32_t folders = 0;
    uint32_t books = 0;
    uint32_t images = 0;
    uint64_t largestSize = 0;
    std::string largestName;
  };
  static constexpr int kMaxScanDepth = 12;  // deeper folders are counted, not entered
  void scanSlice();
  void finishScan();
  bool loadSavedStats();
  void saveStats() const;
  void drawCardStats(int y, int bottom, const Rect& content) const;
  // Depth-first walk: one open handle per level, so memory stays fixed however
  // many files a folder holds.
  HalFile dirStack_[kMaxScanDepth];
  int dirDepth_ = 0;
  bool scanning_ = false;
  bool scanDone_ = false;
  bool haveSaved_ = false;
  CardStats counting_;  // the count in progress
  CardStats shown_;     // what the page shows: the saved count until the new one finishes
  unsigned long lastDrawMs_ = 0;
  bool drawnOnce_ = false;
};
