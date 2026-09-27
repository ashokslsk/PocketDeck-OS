#pragma once

#include "components/themes/lyra/LyraTheme.h"

// PocketDeck: Lyra's layout with the PocketDeck-OS mark in every header, a
// heavy header rule and high-contrast (inverted) selection rows.
namespace PocketDeckMetrics {
constexpr ThemeMetrics makeValues() {
  ThemeMetrics v = LyraMetrics::values;
  v.listSelectionStyle = 0;  // InvertFill: black row, white text
  v.listTitleBold = true;
  v.headerUnderlineSize = 4;
  return v;
}
constexpr ThemeMetrics values = makeValues();
constexpr int kMarkSize = 32;
constexpr int kMarkGap = 8;
}  // namespace PocketDeckMetrics

class PocketDeckTheme : public LyraTheme {
 public:
  void drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle = nullptr,
                  bool readerContext = false) const override;
};
