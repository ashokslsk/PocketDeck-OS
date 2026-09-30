#include "StatsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Memory.h>

#include <cstdio>

#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
const char* featureLabel(const statsx::Feature f) {
  switch (f) {
    case statsx::Feature::Habits:
      return tr(STR_TOOLS_HABITS);
    case statsx::Feature::Medicine:
      return tr(STR_TOOLS_MEDICINE);
    case statsx::Feature::Mood:
      return tr(STR_TOOLS_MOOD);
    case statsx::Feature::Pomodoro:
      return tr(STR_TOOLS_POMODORO);
    case statsx::Feature::Today:
      return tr(STR_TOOLS_TODAY);
    case statsx::Feature::Flashcards:
      return tr(STR_TOOLS_FLASHCARDS);
    case statsx::Feature::Knowledge:
      return tr(STR_TOOLS_KNOWLEDGE);
    case statsx::Feature::Quotes:
      return tr(STR_TOOLS_DAILY_QUOTE);
    case statsx::Feature::Mantras:
      return tr(STR_TOOLS_MANTRAS);
    default:
      return tr(STR_TOOLS_STATS_READING);
  }
}
}  // namespace

void StatsActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  tools::ensureToolsDirs();
  transitionPending_ = true;
  requestUpdate();
}

void StatsActivity::confirmImport() {
  auto confirm = makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_TOOLS_STATS_IMPORT),
                                                         tr(STR_TOOLS_STATS_IMPORT_BODY));
  if (!confirm) return;
  startActivityForResult(std::move(confirm), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    if (!result.isCancelled) {
      pending_ = Pending::Import;
      busyShown_ = false;
    }
    requestUpdate();
  });
}

void StatsActivity::loop() {
  if (pending_ != Pending::None) {
    // Paint the "working" screen first, then do the SD work on the next pass.
    if (!busyShown_) return;
    if (pending_ == Pending::Export) {
      const statsx::ExportReport report = statsx::exportAll(nullptr, nullptr);
      RenderLock lock(*this);
      exportReport_ = report;
      shown_ = Shown::Exported;
    } else {
      const statsx::ImportReport report = statsx::importBooks();
      RenderLock lock(*this);
      importReport_ = report;
      shown_ = Shown::Imported;
    }
    pending_ = Pending::None;
    input_.reset(mappedInput);
    requestUpdate();
    return;
  }
  input_.poll(mappedInput);
  if (input_.backLong) {
    tools::exitToHome();
    return;
  }
  if (input_.back) {
    finish();
    return;
  }
  if (input_.leftUp) {
    confirmImport();
  } else if (input_.confirm) {
    pending_ = Pending::Export;
    busyShown_ = false;
    requestUpdate();
  }
}

void StatsActivity::render(RenderLock&&) {
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_STATS));
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  int y = content.y;
  if (pending_ != Pending::None) {
    renderer.drawCenteredText(
        UI_12_FONT_ID, content.y + content.height / 2 - 20,
        pending_ == Pending::Export ? tr(STR_TOOLS_STATS_EXPORTING) : tr(STR_TOOLS_STATS_IMPORTING), true,
        EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, content.y + content.height / 2 + 10, tr(STR_TOOLS_STATS_WAIT));
    tools::drawHints(renderer, mappedInput, "", "", "", "");
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    busyShown_ = true;
    return;
  }

  tools::WrapOptions opt;
  opt.fontId = UI_10_FONT_ID;
  opt.markdown = true;
  if (shown_ == Shown::Exported) {
    renderer.drawText(UI_12_FONT_ID, content.x, y, tr(STR_TOOLS_STATS_DONE), true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_12_FONT_ID) + 8;
    if (!exportReport_.clockValid) {
      renderer.drawText(UI_10_FONT_ID, content.x, y, tr(STR_TOOLS_STATS_NO_CLOCK));
      y += lineH + 6;
    }
    for (int i = 0; i < static_cast<int>(statsx::Feature::Count); ++i) {
      const auto f = static_cast<statsx::Feature>(i);
      const statsx::FeatureResult& r = exportReport_.features[i];
      char left[64];
      snprintf(left, sizeof(left), "%s", featureLabel(f));
      char right[48];
      if (r.ok) {
        snprintf(right, sizeof(right), "/stats/%s  (%u)", statsx::featureFolder(f), static_cast<unsigned>(r.items));
      } else {
        snprintf(right, sizeof(right), "%s", tr(STR_TOOLS_STATS_SKIPPED));
      }
      renderer.drawText(UI_10_FONT_ID, content.x, y, left, true, EpdFontFamily::BOLD);
      const int w = renderer.getTextWidth(UI_10_FONT_ID, right);
      renderer.drawText(UI_10_FONT_ID, content.x + content.width - w, y, right);
      y += lineH + 8;
    }
    y += 8;
  } else if (shown_ == Shown::Imported) {
    renderer.drawText(UI_12_FONT_ID, content.x, y, tr(STR_TOOLS_STATS_IMPORT_DONE), true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_12_FONT_ID) + 8;
    char line[80];
    const struct {
      StrId label;
      uint16_t value;
    } rows[] = {{StrId::STR_TOOLS_STATS_BOOK_FILES, importReport_.books},
                {StrId::STR_TOOLS_STATS_RESTORED, importReport_.restored},
                {StrId::STR_TOOLS_STATS_KEPT, importReport_.kept},
                {StrId::STR_TOOLS_STATS_MISSING, importReport_.missing},
                {StrId::STR_TOOLS_STATS_FAILED, importReport_.failed}};
    for (const auto& row : rows) {
      snprintf(line, sizeof(line), "%u", static_cast<unsigned>(row.value));
      renderer.drawText(UI_10_FONT_ID, content.x, y, I18N.get(row.label));
      const int w = renderer.getTextWidth(UI_10_FONT_ID, line, EpdFontFamily::BOLD);
      renderer.drawText(UI_10_FONT_ID, content.x + content.width - w, y, line, true, EpdFontFamily::BOLD);
      y += lineH + 8;
    }
    y += 8;
  }
  tools::drawWrappedText(renderer, Rect{content.x, y, content.width, content.y + content.height - y},
                         tr(STR_TOOLS_STATS_INTRO), opt);
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_STATS_EXPORT), tr(STR_TOOLS_STATS_IMPORT_SHORT),
                   "");
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
