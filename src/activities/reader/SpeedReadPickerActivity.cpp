#include "SpeedReadPickerActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "MappedInputManager.h"
#include "activities/ActivityResult.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "components/icons/listIcons.h"
#include "components/icons/speedIcons.h"
#include "fontIds.h"

namespace {
constexpr uint16_t kEnglishWpm[] = {125, 200, 275, 350};
constexpr uint16_t kKannadaWpm[] = {60, 100, 150, 200, 250, 350};
static_assert(sizeof(kKannadaWpm) / sizeof(kKannadaWpm[0]) == SpeedReadPickerActivity::kMaxPresetCount, "presets");
}  // namespace

uint16_t SpeedReadPickerActivity::wpmFor(const bool kannada, int index) {
  if (index < 0) index = 0;
  if (index >= presetCount(kannada)) index = presetCount(kannada) - 1;
  return kannada ? kKannadaWpm[index] : kEnglishWpm[index];
}

void SpeedReadPickerActivity::onEnter() {
  Activity::onEnter();
  if (selected_ < 0 || selected_ >= presetCount(kannada_)) selected_ = 1;
  requestUpdate();
}

void SpeedReadPickerActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    setResult(OptionSelectionResult{static_cast<uint8_t>(selected_)});
    finish();
    return;
  }
  buttonNavigator_.onNextRelease([this] {
    selected_ = ButtonNavigator::nextIndex(selected_, presetCount(kannada_));
    requestUpdate();
  });
  buttonNavigator_.onPreviousRelease([this] {
    selected_ = ButtonNavigator::previousIndex(selected_, presetCount(kannada_));
    requestUpdate();
  });
}

void SpeedReadPickerActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, screenW, metrics.headerHeight}, tr(STR_SPEED_READ),
                 kannada_ ? tr(STR_SPEED_READ_KANNADA) : tr(STR_SPEED_READ_ENGLISH));

  // A rounded card of rows: icon, "125 WPM", what the pace is for.
  static const freeink::Icon* const kIcons[kMaxPresetCount] = {&icon_book_open_32, &icon_book_32,
                                                               &icon_zap_32,       &icon_gauge_32,
                                                               &icon_fast_forward_32, &icon_rocket_32};
  static constexpr StrId kCaptions[kMaxPresetCount] = {
      StrId::STR_SPEED_READ_CAREFUL, StrId::STR_SPEED_READ_COMFORTABLE, StrId::STR_SPEED_READ_FLUENT,
      StrId::STR_SPEED_READ_FAST,    StrId::STR_SPEED_READ_RAPID,       StrId::STR_SPEED_READ_SPRINT};
  const int count = presetCount(kannada_);
  const int x = metrics.contentSidePadding;
  const int w = screenW - 2 * metrics.contentSidePadding;
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int available = screenH - top - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int rowH = std::min(96, std::max(60, available / count));
  const int cardH = rowH * count;
  renderer.drawRoundedRect(x, top, w, cardH, 1, 10, true);
  const int valueH = renderer.getLineHeight(UI_12_FONT_ID);
  const int captionH = renderer.getLineHeight(UI_10_FONT_ID);
  char value[24];
  for (int i = 0; i < count; ++i) {
    const int y = top + i * rowH;
    const bool selected = i == selected_;
    if (selected) {
      renderer.fillRoundedRect(x + 4, y + 4, w - 8, rowH - 8, 8, Color::Black);
    } else if (i > 0) {
      renderer.drawLine(x + 16, y, x + w - 16, y, true);
    }
    drawLucideIcon(renderer, *kIcons[i], x + 20, y + (rowH - 32) / 2, !selected);
    const int tx = x + 20 + 32 + 20;
    const int textTop = y + (rowH - valueH - captionH) / 2;
    snprintf(value, sizeof(value), "%u WPM", static_cast<unsigned>(wpmFor(kannada_, i)));
    renderer.drawText(UI_12_FONT_ID, tx, textTop, value, !selected, EpdFontFamily::BOLD);
    const std::string caption = renderer.truncatedText(UI_10_FONT_ID, I18N.get(kCaptions[i]), x + w - 12 - tx);
    renderer.drawText(UI_10_FONT_ID, tx, textTop + valueH, caption.c_str(), !selected);
  }

  const auto labels =
      mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), tr(STR_START), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
