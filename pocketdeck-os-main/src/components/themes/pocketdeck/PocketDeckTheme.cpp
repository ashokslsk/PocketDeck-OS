#include "PocketDeckTheme.h"

#include <GfxRenderer.h>

#include "components/UITheme.h"
#include "images/PocketDeckMark32.h"

void PocketDeckTheme::drawHeader(const GfxRenderer& renderer, const Rect rect, const char* title, const char* subtitle,
                                 const bool readerContext) const {
  using namespace PocketDeckMetrics;
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int markArea = metrics.headerSidePadding + kMarkSize + kMarkGap - metrics.headerSidePadding / 2;
  // Standard Lyra header shifted right to make room for the mark.
  LyraTheme::drawHeader(renderer, Rect{rect.x + markArea, rect.y, rect.width - markArea, rect.height}, title, subtitle,
                        readerContext);
  // Carry the heavy rule under the mark too.
  const int ruleY = rect.y + rect.height - metrics.headerUnderlineSize;
  renderer.fillRect(rect.x, ruleY, markArea, metrics.headerUnderlineSize);
  // Upright 1-bit mark (1 = white), drawn per pixel so it follows orientation.
  const int mx = rect.x + metrics.headerSidePadding / 2;
  const int my = rect.y + (rect.height - metrics.headerUnderlineSize - kMarkSize) / 2;
  for (int y = 0; y < kMarkSize; ++y) {
    for (int x = 0; x < kMarkSize; ++x) {
      const uint8_t byte = PocketDeckMark32[y * (kMarkSize / 8) + x / 8];
      if (!(byte & (0x80 >> (x & 7)))) renderer.drawPixel(mx + x, my + y, true);
    }
  }
}
