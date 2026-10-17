#include "PowerMenuActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "PowerActions.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "components/icons/powerIcons.h"
#include "fontIds.h"

void PowerMenuActivity::onEnter() {
  Activity::onEnter();
  selected = 0;
  waitForPowerRelease = mappedInput.isPressed(MappedInputManager::Button::Power);
  requestUpdate();
}

void PowerMenuActivity::loop() {
  if (waitForPowerRelease) {
    // Swallow the release of the hold that opened this menu.
    if (!mappedInput.isPressed(MappedInputManager::Button::Power)) waitForPowerRelease = false;
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  const bool previous = mappedInput.wasReleased(MappedInputManager::Button::Left) ||
                        mappedInput.wasReleased(MappedInputManager::Button::Up);
  const bool next = mappedInput.wasReleased(MappedInputManager::Button::Right) ||
                    mappedInput.wasReleased(MappedInputManager::Button::Down);
  if (previous || next) {
    selected = 1 - selected;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selected == 0) {
      poweractions::requestShutdown();
    } else {
      poweractions::requestRestart();
    }
    finish();
  }
}

void PowerMenuActivity::render(RenderLock&&) {
  // A popup over whatever was on screen.
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const int titleH = renderer.getLineHeight(UI_12_FONT_ID);
  const int labelH = renderer.getLineHeight(UI_10_FONT_ID);
  constexpr int pad = 20;
  constexpr int gap = 16;
  constexpr int iconSize = 48;
  const int cardW = std::min(screenW - 40, 420);
  const int tileW = (cardW - pad * 2 - gap) / 2;
  const int tileH = iconSize + labelH + 36;
  const int cardH = pad + titleH + 14 + tileH + pad;
  const int cardX = (screenW - cardW) / 2;
  const int cardY = (screenH - cardH) / 2;

  renderer.fillRoundedRect(cardX, cardY, cardW, cardH, 14, Color::White);
  renderer.drawRoundedRect(cardX, cardY, cardW, cardH, 3, 14, true);
  const char* title = tr(STR_POWER_MENU_TITLE);
  renderer.drawText(UI_12_FONT_ID,
                    cardX + (cardW - renderer.getTextWidth(UI_12_FONT_ID, title, EpdFontFamily::BOLD)) / 2, cardY + pad,
                    title, true, EpdFontFamily::BOLD);

  const freeink::Icon* icons[2] = {&icon_power_48, &icon_rotate_ccw_48};
  const char* labels[2] = {tr(STR_POWER_SHUT_DOWN), tr(STR_POWER_RESTART)};
  const int tileY = cardY + pad + titleH + 14;
  for (int i = 0; i < 2; ++i) {
    const int tileX = cardX + pad + i * (tileW + gap);
    const bool isSelected = i == selected;
    if (isSelected) {
      renderer.fillRoundedRect(tileX, tileY, tileW, tileH, 10, Color::Black);
    } else {
      renderer.drawRoundedRect(tileX, tileY, tileW, tileH, 2, 10, true);
    }
    drawLucideIcon(renderer, *icons[i], tileX + (tileW - iconSize) / 2, tileY + 12, !isSelected);
    const int lw = renderer.getTextWidth(UI_10_FONT_ID, labels[i], EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, tileX + (tileW - lw) / 2, tileY + 12 + iconSize + 10, labels[i], !isSelected,
                      EpdFontFamily::BOLD);
  }

  const auto hints = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_CANCEL)), tr(STR_SELECT), "<", ">");
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
