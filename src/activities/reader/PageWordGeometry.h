#pragma once

#include <Epub/Page.h>
#include <GfxRenderer.h>

#include <algorithm>

struct PageWordGeometry {
  int xOffset = 0;
  int width = 0;
};

inline PageWordGeometry clipPageTextRange(const PageTextLine& line, const int xOffset, const int width) {
  if (line.clipWidth <= 0) return {xOffset, width};
  const int visibleLeft = std::max(0, xOffset);
  const int visibleRight = std::min(line.clipWidth, xOffset + width);
  return {visibleLeft, std::max(0, visibleRight - visibleLeft)};
}

// The font a block's lines are laid out in: <pre> code lines use the monospace font.
inline int pageLineFontId(const GfxRenderer& renderer, const int fontId, const TextBlock& block) {
  return block.getBlockStyle().preformatted ? renderer.monospaceFontFor(fontId) : fontId;
}

inline PageWordGeometry pageWordGeometry(const GfxRenderer& renderer, const int fontId, const PageTextLine& line,
                                         const TextBlock& block, const uint16_t wordIndex) {
  const auto style = static_cast<EpdFontFamily::Style>(block.wordStyle(wordIndex) & ~EpdFontFamily::UNDERLINE);
  const int measureFontId = pageLineFontId(renderer, fontId, block);
  int width = renderer.getTextAdvanceX(measureFontId, block.wordText(wordIndex), style);
  if (wordIndex + 1 < block.wordCount() && block.wordXpos(wordIndex + 1) > block.wordXpos(wordIndex)) {
    width = std::min(width, static_cast<int>(block.wordXpos(wordIndex + 1) - block.wordXpos(wordIndex)));
  }
  return clipPageTextRange(line, block.wordXpos(wordIndex), width);
}
