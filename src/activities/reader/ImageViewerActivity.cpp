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
constexpr char kViewCache[] = "/.pocketdeck-os/system_cache/image_view.pxc";
constexpr float kMaxUpscale = 3.0f;
}  // namespace

void ImageViewerActivity::onEnter() {
  Activity::onEnter();
  index_ = 0;
  drawnKey_ = -1;
  zoom_ = 1;
  part_ = 0;
  requestUpdate();
}

void ImageViewerActivity::onExit() {
  if (Storage.exists(kViewCache)) Storage.remove(kViewCache);
  Activity::onExit();
}

void ImageViewerActivity::viewport(Placement& out) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  out.y = metrics.topPadding + renderer.getLineHeight(SMALL_FONT_ID) + 6;
  out.h = renderer.getScreenHeight() - metrics.buttonHintsHeight - 4 - out.y;
  out.x = 4;
  out.w = renderer.getScreenWidth() - 8;
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

void ImageViewerActivity::drawChrome(const int parts) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  char title[80];
  const int count = static_cast<int>(paths_.size());
  if (zoom_ > 1) {
    snprintf(title, sizeof(title), tr(STR_IMAGE_ZOOM_TITLE_FORMAT), index_ + 1, count, zoom_, part_ + 1, parts);
  } else {
    snprintf(title, sizeof(title), tr(STR_IMAGE_N_OF_M_FORMAT), index_ + 1, count);
  }
  renderer.drawCenteredText(SMALL_FONT_ID, metrics.topPadding, title, true, EpdFontFamily::BOLD);
  const char* confirm = count > 1 ? tr(STR_IMAGE_NEXT_IMAGE) : "";
  const auto hints = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), confirm,
                                           zoom_ > 1 ? tr(STR_IMAGE_ZOOM_OUT) : "",
                                           zoom_ < kMaxZoom ? tr(STR_IMAGE_ZOOM_IN) : "");
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
}

bool ImageViewerActivity::zoomedPlacement(const Placement& fit, Placement& out, int& parts) {
  Placement view{};
  viewport(view);
  const int zw = fit.w * zoom_;
  const int zh = fit.h * zoom_;
  // Panning moves half a screen at a time in reading order (across, then down)
  // so each view overlaps the last; the last step is aligned to the image edge.
  const int stepX = std::max(1, view.w / 2);
  const int stepY = std::max(1, view.h / 2);
  const int cols = zw > view.w ? (zw - view.w + stepX - 1) / stepX + 1 : 1;
  const int rows = zh > view.h ? (zh - view.h + stepY - 1) / stepY + 1 : 1;
  parts = cols * rows;
  if (part_ >= parts) part_ = parts - 1;
  if (part_ < 0) part_ = 0;
  const int col = part_ % cols;
  const int row = part_ / cols;
  const int ox = zw > view.w ? std::min(col * stepX, zw - view.w) : 0;
  const int oy = zh > view.h ? std::min(row * stepY, zh - view.h) : 0;
  out.x = zw > view.w ? view.x - ox : view.x + (view.w - zw) / 2;
  out.y = zh > view.h ? view.y - oy : view.y + (view.h - zh) / 2;
  out.w = zw;
  out.h = zh;
  return true;
}

void ImageViewerActivity::clearChromeBands() const {
  // A zoomed image runs past the viewport; keep the title and hint bands clear.
  Placement view{};
  viewport(view);
  renderer.fillRect(0, 0, renderer.getScreenWidth(), view.y, false);
  renderer.fillRect(0, view.y + view.h, renderer.getScreenWidth(), renderer.getScreenHeight() - view.y - view.h, false);
}

void ImageViewerActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  // Front Left/Right zoom; the side buttons pan (top: back, bottom: forward);
  // Confirm moves to the next image.
  const bool zoomOut = mappedInput.wasReleased(MappedInputManager::Button::Left);
  const bool zoomIn = mappedInput.wasReleased(MappedInputManager::Button::Right);
  const bool panBack = mappedInput.wasReleased(MappedInputManager::Button::Up);
  const bool panForward = mappedInput.wasReleased(MappedInputManager::Button::Down);
  const bool nextImage = mappedInput.wasReleased(MappedInputManager::Button::Confirm);
  if (zoomIn && zoom_ < kMaxZoom) {
    ++zoom_;
    part_ = 0;
    requestUpdate();
  } else if (zoomOut && zoom_ > 1) {
    --zoom_;
    part_ = 0;
    requestUpdate();
  } else if (zoom_ > 1 && panForward && part_ + 1 < parts_) {
    ++part_;
    requestUpdate();
  } else if (zoom_ > 1 && panBack && part_ > 0) {
    --part_;
    requestUpdate();
  } else if (nextImage && paths_.size() > 1) {
    index_ = (index_ + 1) % static_cast<int>(paths_.size());
    zoom_ = 1;
    part_ = 0;
    requestUpdate();
  }
}

void ImageViewerActivity::render(RenderLock&&) {
  if (paths_.empty()) {
    finish();
    return;
  }
  const std::string& path = paths_[index_];
  // Decode afresh when the view changes; the grayscale passes replay the
  // pixel cache (it clips to the screen, so zoomed views work the same way).
  const int viewKey = (index_ << 16) | (zoom_ << 8) | part_;
  const bool fresh = drawnKey_ != viewKey;
  if (fresh) {
    GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  }
  Placement fit{};
  Placement p{};
  int parts = 1;
  bool ok = placementFor(path, fit);
  if (ok) {
    p = fit;
    if (zoom_ > 1) zoomedPlacement(fit, p, parts);
  }
  parts_ = parts;
  renderer.clearScreen();
  ok = ok && ImageBlock::renderScaled(renderer, path, kViewCache, p.x, p.y, p.w, p.h, fresh);
  drawnKey_ = viewKey;
  if (zoom_ > 1) clearChromeBands();
  drawChrome(parts);
  if (!ok) {
    drawnKey_ = -1;
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
    if (zoom_ > 1) clearChromeBands();
    renderer.copyGrayscaleLsbBuffers();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    ok = ImageBlock::renderScaled(renderer, path, kViewCache, p.x, p.y, p.w, p.h, false);
    if (ok) {
      if (zoom_ > 1) clearChromeBands();
      renderer.copyGrayscaleMsbBuffers();
      renderer.displayGrayBuffer();
    }
  }
  renderer.setRenderMode(GfxRenderer::BW);
  renderer.restoreBwBuffer();
}
