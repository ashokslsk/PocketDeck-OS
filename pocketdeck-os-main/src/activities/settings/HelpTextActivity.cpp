#include "HelpTextActivity.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <cstdio>

#include "components/UITheme.h"
#include "fontIds.h"

void HelpTextActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  page_ = 0;
  requestUpdate();
}

void HelpTextActivity::loop() {
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

void HelpTextActivity::render(RenderLock&&) {
  const Rect content = tools::drawFrame(renderer, I18N.get(title_));
  const int footerH = renderer.getLineHeight(SMALL_FONT_ID) + 6;
  const Rect body{content.x, content.y, content.width, content.height - footerH};
  const int linesPerPage = std::max(1, body.height / renderer.getLineHeight(UI_10_FONT_ID));
  tools::WrapOptions opt;
  opt.fontId = UI_10_FONT_ID;
  opt.markdown = true;
  opt.draw = false;
  const int totalLines = tools::drawWrappedText(renderer, body, I18N.get(body_), opt);
  pageCount_ = std::max(1, (totalLines + linesPerPage - 1) / linesPerPage);
  page_ = std::min(page_, pageCount_ - 1);
  opt.draw = true;
  opt.skipLines = page_ * linesPerPage;
  opt.maxLines = linesPerPage;
  tools::drawWrappedText(renderer, body, I18N.get(body_), opt);
  if (pageCount_ > 1) {
    char footer[32];
    snprintf(footer, sizeof(footer), "%s %d/%d", tr(STR_TOOLS_PAGE), page_ + 1, pageCount_);
    renderer.drawText(SMALL_FONT_ID, content.x, content.y + content.height - footerH + 6, footer);
  }
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  renderer.displayBuffer();
}
