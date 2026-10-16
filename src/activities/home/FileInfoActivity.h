#pragma once

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

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::string title_;
  std::vector<Row> rows_;
  tools::ToolInput input_;
};
