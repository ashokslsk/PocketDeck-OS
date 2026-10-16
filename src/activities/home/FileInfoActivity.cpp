#include "FileInfoActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "components/UITheme.h"
#include "fontIds.h"

void FileInfoActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  requestUpdate();
}

void FileInfoActivity::loop() {
  input_.poll(mappedInput);
  // Back only: the Confirm release that chose Info in the Options sheet
  // arrives here and must not close the page straight away.
  if (input_.back || input_.backLong) {
    finish();
  }
}

void FileInfoActivity::render(RenderLock&&) {
  const Rect content = tools::drawFrame(renderer, tr(STR_FILE_INFO));
  const int labelH = renderer.getLineHeight(SMALL_FONT_ID);
  const int valueH = renderer.getLineHeight(UI_12_FONT_ID);
  int y = content.y + 4;

  // The item's own name, wrapped over up to two lines.
  tools::WrapOptions opt;
  opt.fontId = UI_12_FONT_ID;
  opt.maxLines = 2;
  const int titleLines =
      tools::drawWrappedText(renderer, Rect{content.x, y, content.width, valueH * 2}, title_.c_str(), opt);
  y += valueH * std::max(1, titleLines) + 14;
  renderer.drawLine(content.x, y - 7, content.x + content.width, y - 7, true);

  for (const auto& row : rows_) {
    if (y + labelH + valueH > content.y + content.height) break;
    renderer.drawText(SMALL_FONT_ID, content.x, y, row.first.c_str());
    y += labelH + 2;
    const std::string value = renderer.truncatedText(UI_12_FONT_ID, row.second.c_str(), content.width);
    renderer.drawText(UI_12_FONT_ID, content.x, y, value.c_str());
    y += valueH + 12;
  }

  tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
  renderer.displayBuffer();
}
