#include "PoweredOffActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "fontIds.h"
#include "images/PocketDeckLogo240.h"

void PoweredOffActivity::onEnter() {
  Activity::onEnter();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  constexpr int kLogo = 240;

  renderer.clearScreen();
  // Logo a little above centre, the words under it.
  int y = pageHeight / 2 - kLogo / 2 - 90;
  renderer.drawImage(PocketDeckLogo240, (pageWidth - kLogo) / 2, y, kLogo, kLogo);
  y += kLogo + 22;
  renderer.drawCenteredText(LEXENDDECA_16_FONT_ID, y, tr(STR_POWERED_OFF_TITLE), true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(LEXENDDECA_16_FONT_ID) + 14;

  // Short rule between the status and the quote.
  renderer.fillRect(pageWidth / 2 - 24, y, 48, 2, true);
  y += 22;

  const int quoteW = pageWidth - 2 * 48;
  const int quoteH = renderer.getLineHeight(LEXENDDECA_12_FONT_ID);
  for (const auto& line : renderer.wrappedText(LEXENDDECA_12_FONT_ID, tr(STR_POWERED_OFF_QUOTE), quoteW, 4,
                                               EpdFontFamily::ITALIC)) {
    renderer.drawCenteredText(LEXENDDECA_12_FONT_ID, y, line.c_str(), true, EpdFontFamily::ITALIC);
    y += quoteH;
  }
  y += 6;
  renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_POWERED_OFF_QUOTE_BY));

  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 48, tr(STR_POWERED_OFF_HINT));
  // A full-waveform refresh: the panel holds this image while unpowered.
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}
