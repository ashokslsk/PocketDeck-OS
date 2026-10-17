#include "ImageViewerActivity.h"

#include <Epub/blocks/ImageBlock.h>
#include <Epub/converters/ImageDecoderFactory.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Scaled pixels of the image on screen, reused by the grayscale passes.
constexpr char kViewCache[] = "/.pocketdeck-os/image_view.pxc";
constexpr float kMaxUpscale = 3.0f;
}  // namespace

void ImageViewerActivity::onEnter() {
  Activity::onEnter();
  index_ = 0;
  drawnIndex_ = -1;
  requestUpdate();
}

void ImageViewerActivity::onExit() {
  if (Storage.exists(kViewCache)) Storage.remove(kViewCache);
  Activity::onExit();
}

bool ImageViewerActivity::placementFor(const std::string& path, Placement& out) const {
  ImageToFramebufferDecoder* decoder = ImageDecoderFactory::getDecoder(path);
  ImageDimensions dims{0, 0};
  if (!decoder || !decoder->getDimensions(path, dims) || dims.width <= 0 || dims.height <= 0) {
    LOG_ERR("IMGV", "Cannot read image size: %s", path.c_str());
    return false;
  }
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int top = metrics.topPadding + renderer.getLineHeight(SMALL_FONT_ID) + 6;
  const int bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - 4;
  const int availW = renderer.getScreenWidth() - 8;
  const int availH = bottom - top;
  const float scale =
      std::min({static_cast<float>(availW) / dims.width, static_cast<float>(availH) / dims.height, kMaxUpscale});
  out.w = std::max(1, static_cast<int>(dims.width * scale));
  out.h = std::max(1, static_cast<int>(dims.height * scale));
  out.x = (renderer.getScreenWidth() - out.w) / 2;
  out.y = top + (availH - out.h) / 2;
  return true;
}

void ImageViewerActivity::drawChrome() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  char title[48];
  snprintf(title, sizeof(title), tr(STR_IMAGE_N_OF_M_FORMAT), index_ + 1, static_cast<int>(paths_.size()));
  renderer.drawCenteredText(SMALL_FONT_ID, metrics.topPadding, title, true, EpdFontFamily::BOLD);
  const bool several = paths_.size() > 1;
  const auto hints =
      mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), "", several && index_ > 0 ? "<" : "",
                            several && index_ + 1 < static_cast<int>(paths_.size()) ? ">" : "");
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
}

void ImageViewerActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  const bool previous = mappedInput.wasReleased(MappedInputManager::Button::Left) ||
                        mappedInput.wasReleased(MappedInputManager::Button::Up) ||
                        mappedInput.wasReleased(MappedInputManager::Button::PageBack);
  const bool next = mappedInput.wasReleased(MappedInputManager::Button::Right) ||
                    mappedInput.wasReleased(MappedInputManager::Button::Down) ||
                    mappedInput.wasReleased(MappedInputManager::Button::PageForward);
  if (previous && index_ > 0) {
    --index_;
    requestUpdate();
  } else if (next && index_ + 1 < static_cast<int>(paths_.size())) {
    ++index_;
    requestUpdate();
  }
}

void ImageViewerActivity::render(RenderLock&&) {
  if (paths_.empty()) {
    finish();
    return;
  }
  const std::string& path = paths_[index_];
  const bool fresh = drawnIndex_ != index_;
  if (fresh) {
    GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  }
  Placement p{};
  renderer.clearScreen();
  bool ok = placementFor(path, p) && ImageBlock::renderScaled(renderer, path, kViewCache, p.x, p.y, p.w, p.h, fresh);
  drawnIndex_ = index_;
  drawChrome();
  if (!ok) {
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2, tr(STR_IMAGE_LOAD_FAILED));
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    return;
  }
  // Same sequence as a reader page with an image: the B/W frame on a
  // grayscale-aware base, then the two gray planes, then the B/W frame back.
  renderer.displayGrayscaleBase(HalDisplay::HALF_REFRESH);
  if (!renderer.storeBwBuffer()) return;
  renderer.clearScreen(0x00);
  renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
  ok = ImageBlock::renderScaled(renderer, path, kViewCache, p.x, p.y, p.w, p.h, false);
  if (ok) {
    renderer.copyGrayscaleLsbBuffers();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    ok = ImageBlock::renderScaled(renderer, path, kViewCache, p.x, p.y, p.w, p.h, false);
    if (ok) {
      renderer.copyGrayscaleMsbBuffers();
      renderer.displayGrayBuffer();
    }
  }
  renderer.setRenderMode(GfxRenderer::BW);
  renderer.restoreBwBuffer();
}
