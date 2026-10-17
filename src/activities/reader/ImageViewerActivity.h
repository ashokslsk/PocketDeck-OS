#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"

// Reader menu > View Images: the current page's images, one at a time, fitted
// to the whole screen (small ones enlarged up to 3x). Left/Right or the side
// buttons move between them when the page has several; Back returns to the
// page. Drawn in grayscale where the panel supports it.
class ImageViewerActivity final : public Activity {
 public:
  ImageViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::vector<std::string> imagePaths)
      : Activity("ImageViewer", renderer, mappedInput), paths_(std::move(imagePaths)) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }

 private:
  struct Placement {
    int x, y, w, h;
  };
  bool placementFor(const std::string& path, Placement& out) const;
  void drawChrome() const;

  std::vector<std::string> paths_;
  int index_ = 0;
  int drawnIndex_ = -1;
};
