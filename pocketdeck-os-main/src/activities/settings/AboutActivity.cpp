#include "AboutActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "AppVersion.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "images/Logo120.h"

void AboutActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  page_ = 0;
  requestUpdate();
}

void AboutActivity::loop() {
  input_.poll(mappedInput);
  if (input_.back || input_.backLong) {
    finish();
    return;
  }
  if (input_.next() && page_ + 1 < pageCount_) {
    ++page_;
    requestUpdate();
  } else if (input_.prev() && page_ > 0) {
    --page_;
    requestUpdate();
  }
}

void AboutActivity::render(RenderLock&&) {
  const Rect content = tools::drawFrame(renderer, tr(STR_ABOUT));
  int y = content.y;

  // Identity block: logo beside name, version and credit.
  renderer.drawImage(Logo120, content.x, y, 120, 120);
  const int textX = content.x + 136;
  int ty = y + 14;
  renderer.drawText(LEXENDDECA_16_FONT_ID, textX, ty, tr(STR_CROSSINK), true, EpdFontFamily::BOLD);
  ty += renderer.getLineHeight(LEXENDDECA_16_FONT_ID) + 2;
  char version[64];
  snprintf(version, sizeof(version), "%s %s", tr(STR_ABOUT_VERSION), POCKETDECK_VERSION);
  renderer.drawText(UI_10_FONT_ID, textX, ty, version);
  ty += renderer.getLineHeight(UI_10_FONT_ID) + 2;
  const std::string credit =
      renderer.truncatedText(UI_10_FONT_ID, tr(STR_POCKETDECK_DEVELOPED_BY), content.x + content.width - textX);
  renderer.drawText(UI_10_FONT_ID, textX, ty, credit.c_str(), true, EpdFontFamily::BOLD);
  y += 132;
  renderer.drawLine(content.x, y, content.x + content.width, y);
  y += 12;

  // Paged body text (Markdown subset).
  const int footerH = renderer.getLineHeight(SMALL_FONT_ID) + 6;
  const Rect body{content.x, y, content.width, content.y + content.height - y - footerH};
  const int linesPerPage = std::max(1, body.height / renderer.getLineHeight(UI_10_FONT_ID));
  tools::WrapOptions measure;
  measure.fontId = UI_10_FONT_ID;
  measure.markdown = true;
  measure.draw = false;
  const int totalLines = tools::drawWrappedText(renderer, body, tr(STR_ABOUT_BODY), measure);
  pageCount_ = std::max(1, (totalLines + linesPerPage - 1) / linesPerPage);
  page_ = std::min(page_, pageCount_ - 1);
  tools::WrapOptions opt = measure;
  opt.draw = true;
  opt.skipLines = page_ * linesPerPage;
  opt.maxLines = linesPerPage;
  tools::drawWrappedText(renderer, body, tr(STR_ABOUT_BODY), opt);

  char footer[48];
  snprintf(footer, sizeof(footer), "%s %d/%d   Based on CrossInk %s", tr(STR_TOOLS_PAGE), page_ + 1, pageCount_,
           CROSSINK_VERSION);
  renderer.drawText(SMALL_FONT_ID, content.x, content.y + content.height - footerH + 6, footer);

  tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  renderer.displayBuffer();
}
