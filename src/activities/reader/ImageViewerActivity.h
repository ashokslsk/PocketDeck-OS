#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"

// Reader menu > View Images: the current page's images, one at a time, fitted
// to the whole screen (small ones enlarged up to 3x) and drawn in grayscale.
// Front Right zooms in (2x, 3x of the fitted size) and Left zooms out; zoomed,
// the side buttons pan half a screen at a time in reading order (top back,
// bottom forward). Confirm moves to the next image. Back returns.
// Each view is decoded once; the grayscale passes replay its pixel cache.
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
  void viewport(Placement& out) const;
  void drawChrome(int parts) const;
  bool zoomedPlacement(const Placement& fit, Placement& out, int& parts);
  void clearChromeBands() const;
  static constexpr int kMaxZoom = 3;
  int zoom_ = 1;
  int part_ = 0;
  int parts_ = 1;  // pan positions at the current zoom (set when drawn)

  std::vector<std::string> paths_;
  int index_ = 0;
  int drawnKey_ = -1;
};
