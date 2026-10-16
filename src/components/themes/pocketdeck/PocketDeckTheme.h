#pragma once

#include "components/themes/lyra/LyraTheme.h"

// PocketDeck: Lyra's layout with the PocketDeck-OS mark in every header, a
// heavy header rule and high-contrast (inverted) selection rows. Home is a
// card dashboard: Now Reading (cover, progress, three figures and a Continue
// Reading button), Reading Stats (weekday columns, streak, days read this
// week), Recent Books (four covers with their progress) and a bottom bar with
// the Home menu as icons.
namespace PocketDeckMetrics {
constexpr ThemeMetrics makeValues() {
  ThemeMetrics v = LyraMetrics::values;
  v.listSelectionStyle = 0;  // InvertFill: black row, white text
  v.listTitleBold = true;
  v.headerUnderlineSize = 4;
  // Home: the cards above Quick Actions are one cover tile (drawn once, then
  // restored from the cover snapshot on every selection change).
  v.homeCoverHeight = 226;  // Now Reading cover, drawn at thumbnail size (Lyra's too)
  v.homeCoverTileHeight = 604;
  v.homeRecentBooksCount = 4;
  v.homeMenuTopOffset = 14;
  return v;
}
constexpr ThemeMetrics values = makeValues();
// Recent Reads covers get their own thumbnails at this height (a dithered
// thumbnail scaled down again turns to noise).
constexpr int kRecentCoverHeight = 90;
constexpr int kMarkSize = 32;
constexpr int kMarkGap = 8;
}  // namespace PocketDeckMetrics

class PocketDeckTheme : public LyraTheme {
 public:
  void drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle = nullptr,
                  bool readerContext = false) const override;
  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           int selectorIndex, bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                           const std::function<bool()>& storeCoverBuffer, const BookReadingStats* stats = nullptr,
                           float progressPercent = -1.0f, const GlobalReadingStats* globalStats = nullptr,
                           const char* currentChapterTitle = nullptr) const override;
  void drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                      const std::function<const char*(int index)>& buttonLabel,
                      const std::function<UIIcon(int index)>& rowIcon) const override;
};
