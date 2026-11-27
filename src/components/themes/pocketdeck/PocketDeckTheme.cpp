#include "PocketDeckTheme.h"

#include <Arduino.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <string>

#include "CrossPointSettings.h"
#include "RecentBooksStore.h"
#include "activities/home/RecentBookProgress.h"
#include "activities/reader/BookInsights.h"
#include "activities/reader/BookReadingStats.h"
#include "activities/reader/GlobalReadingStats.h"
#include "activities/reader/ReadingStatsUtils.h"
#include "activities/tools/DailyQuoteActivity.h"
#include "activities/tools/ToolsCommon.h"
#include "components/HeaderDate.h"
#include "components/TouchRegistry.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "components/icons/deckIcons.h"
#include "components/icons/listIcons.h"
#include "components/icons/toolsIcon.h"
#include "fontIds.h"
#include "images/PocketDeckMark32.h"
#include "network/WifiUtils.h"

namespace {
constexpr int kPad = 12;          // screen edge to card
constexpr int kInset = 12;        // card edge to content
constexpr int kChamfer = 14;      // card corner cut
constexpr int kCardGap = 10;      // between cards
constexpr int kNowH = 268;        // Now Reading card
constexpr int kStatsH = 142;      // Reading Stats card
constexpr int kRecentSlots = 4;   // covers plus the View All Recents tile
constexpr int kRecentCovers = 3;  // == homeRecentBooksCount
constexpr int kRecentH = 164;     // Recent Books card
constexpr int kButtonH = 40;      // Continue Reading button
constexpr int kRecentTopPad = 8;  // heading to covers
constexpr int kRecentFootH = 26;  // progress row under the covers
constexpr int kSideW = 84;        // side menu column (Home Menu: Side)
constexpr int kSideItemW = 80;
constexpr int kSideReadH = 100;  // pitch of the side items, Read first

bool sideMenu() { return SETTINGS.homeMenuLayout == CrossPointSettings::HOME_MENU_SIDE; }

// One side-menu item: icon over label; selected is a black rounded box.
void sideItem(const GfxRenderer& r, const int x, const int y, const int w, const int h, const freeink::Icon& icon,
              const char* label, const bool selected) {
  // Inset so the black box clears the panel's outline.
  if (selected) r.fillRoundedRect(x + 7, y + 5, w - 14, h - 10, 10, Color::Black);
  const int font = SMALL_FONT_ID;  // one size for every item
  const int labelH = r.getLineHeight(font);
  const int iconY = y + (h - 32 - 2 - labelH) / 2;
  drawLucideIcon(r, icon, x + (w - 32) / 2, iconY, !selected);
  const std::string text = r.truncatedText(font, label, w - 6);
  r.drawText(font, x + (w - r.getTextWidth(font, text.c_str())) / 2, iconY + 34, text.c_str(), !selected);
}

// The PocketDeck shape: a rectangle with its corners cut at 45 degrees.
void chamferPoints(const int x, const int y, const int w, const int h, const int c, int* px, int* py) {
  const int r = x + w - 1;
  const int b = y + h - 1;
  const int xs[8] = {x + c, r - c, r, r, r - c, x + c, x, x};
  const int ys[8] = {y, y, y + c, b - c, b, b, b - c, y + c};
  for (int i = 0; i < 8; ++i) {
    px[i] = xs[i];
    py[i] = ys[i];
  }
}

void fillChamfer(const GfxRenderer& r, const int x, const int y, const int w, const int h, const int c,
                 const bool black) {
  int px[8];
  int py[8];
  chamferPoints(x, y, w, h, c, px, py);
  r.fillPolygon(px, py, 8, black);
}

// Outline `lw` px thick, drawn inwards. Each inner ring cuts its corners one
// pixel less so the diagonals stay solid.
void drawChamfer(const GfxRenderer& r, const int x, const int y, const int w, const int h, const int c, const int lw,
                 const bool black = true) {
  for (int k = 0; k < lw; ++k) {
    int px[8];
    int py[8];
    chamferPoints(x + k, y + k, w - 2 * k, h - 2 * k, std::max(1, c - k), px, py);
    for (int i = 0; i < 8; ++i) r.drawLine(px[i], py[i], px[(i + 1) % 8], py[(i + 1) % 8], black);
  }
}

// Card: white cut-corner panel, 2 px frame and a hairline inner frame.
void card(const GfxRenderer& r, const int x, const int y, const int w, const int h) {
  fillChamfer(r, x, y, w, h, kChamfer, false);
  drawChamfer(r, x, y, w, h, kChamfer, 2);
  drawChamfer(r, x + 5, y + 5, w - 10, h - 10, kChamfer - 3, 1);
}

// "/////" accent ticks, `n` of them starting at x.
void hatch(const GfxRenderer& r, const int x, const int y, const int n, const int h = 6) {
  for (int i = 0; i < n; ++i) r.drawLine(x + i * 5, y + h, x + i * 5 + h, y, 2, true);
}

// Rule with heavy bracket ends and hatch ticks, under the status bar and
// above the Home buttons.
void deckRule(const GfxRenderer& r, const int x0, const int x1, const int y) {
  constexpr int kEnd = 44;
  r.drawLine(x0 + kEnd, y, x1 - kEnd, y, true);
  r.fillRect(x0, y - 1, kEnd - 14, 3, true);
  r.fillRect(x1 - kEnd + 14, y - 1, kEnd - 14, 3, true);
  hatch(r, x0 + kEnd - 10, y - 3, 2, 5);
  hatch(r, x1 - kEnd + 2, y - 3, 2, 5);
}

// ">" drawn with lines, 2 px strokes.
void chevron(const GfxRenderer& r, const int x, const int cy, const int size, const bool black) {
  for (int d = 0; d < 2; ++d) {
    r.drawLine(x + d, cy - size, x + size + d, cy, black);
    r.drawLine(x + size + d, cy, x + d, cy + size, black);
  }
}

// "^" / "v" for the Up and Down hints.
void chevronVertical(const GfxRenderer& r, const int cx, const int cy, const int size, const bool up) {
  const int dir = up ? -1 : 1;
  for (int d = 0; d < 2; ++d) {
    r.drawLine(cx - size, cy - dir * size / 2 + d, cx, cy + dir * size / 2 + d, true);
    r.drawLine(cx, cy + dir * size / 2 + d, cx + size, cy - dir * size / 2 + d, true);
  }
}

// Section title with a 32 px icon (or none) on the left. Returns the y below it.
int cardTitle(const GfxRenderer& r, const int x, const int y, const UIIcon icon, const char* title,
              const freeink::Icon* (*iconFor)(UIIcon, uint32_t)) {
  int tx = x + kInset;
  const freeink::Icon* bmp = iconFor(icon, 32);
  if (bmp != nullptr) {
    drawLucideIcon(r, *bmp, tx - 2, y + 4);
    tx += 34;
  }
  r.drawText(UI_12_FONT_ID, tx, y + 4 + (32 - r.getLineHeight(UI_12_FONT_ID)) / 2, title, true, EpdFontFamily::BOLD);
  return y + 40;
}

// Section title with a given 32 px icon.
int cardTitleIcon(const GfxRenderer& r, const int x, const int y, const freeink::Icon& icon, const char* title) {
  drawLucideIcon(r, icon, x + kInset - 2, y + 6);
  r.drawText(UI_12_FONT_ID, x + kInset + 34, y + 6 + (32 - r.getLineHeight(UI_12_FONT_ID)) / 2, title, true,
             EpdFontFamily::BOLD);
  return y + 42;
}

// y below a card title row, for cards that draw their title later.
int cardTitleSlot(const int y) { return y + 40; }

// Draws a book's home thumbnail fitted into (x, y, w, h), bottom-centred,
// framed. Returns the drawn rect width (0 for the placeholder).
int drawCover(GfxRenderer& r, const RecentBook& book, const int x, const int y, const int w, const int h) {
  int drawnW = 0;
  int drawnH = 0;
  if (!book.coverBmpPath.empty()) {
    const int thumbH = h <= PocketDeckMetrics::kRecentCoverHeight ? PocketDeckMetrics::kRecentCoverHeight
                                                                  : PocketDeckMetrics::values.homeCoverHeight;
    const std::string thumb = UITheme::getCoverThumbPath(book.coverBmpPath, thumbH);
    HalFile file;
    if (Storage.openFileForRead("HOME", thumb, file)) {
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() == BmpReaderError::Ok && bitmap.getHeight() > 0) {
        const float scale =
            std::min(static_cast<float>(w) / bitmap.getWidth(), static_cast<float>(h) / bitmap.getHeight());
        drawnW = std::max(1, static_cast<int>(bitmap.getWidth() * std::min(1.0f, scale)));
        drawnH = std::max(1, static_cast<int>(bitmap.getHeight() * std::min(1.0f, scale)));
        r.drawBitmap(bitmap, x + (w - drawnW) / 2, y + h - drawnH, drawnW, drawnH);
      }
      file.close();
    }
  }
  if (drawnW == 0) {
    // Placeholder: a book-shaped frame with the title.
    drawnW = std::min(w, h * 2 / 3);
    drawnH = h;
    const int px = x + (w - drawnW) / 2;
    r.fillRectDither(px, y, drawnW, drawnH, Color::LightGray);
    const auto lines = r.wrappedText(SMALL_FONT_ID, book.title.c_str(), drawnW - 8, 3, EpdFontFamily::BOLD);
    int ty = y + 8;
    for (const auto& line : lines) {
      r.drawText(SMALL_FONT_ID, px + 4, ty, line.c_str(), true, EpdFontFamily::BOLD);
      ty += r.getLineHeight(SMALL_FONT_ID);
    }
  }
  r.drawRect(x + (w - drawnW) / 2, y + h - drawnH, drawnW, drawnH, true);
  return drawnW;
}

// Rounded progress bar: gray track, black fill, round ends.
void progressBar(const GfxRenderer& r, const int x, const int y, const int w, const int h, const float pct) {
  r.fillRoundedRect(x, y, w, h, h / 2, Color::LightGray);
  const int fill = std::clamp(static_cast<int>(w * pct / 100.0f), 0, w);
  if (fill > 0) r.fillRoundedRect(x, y, std::max(h, fill), h, h / 2, Color::Black);
}

// Big value over a small caption, left aligned.
void valueBlock(const GfxRenderer& r, const int x, const int y, const char* value, const char* caption) {
  r.drawText(UI_12_FONT_ID, x, y, value, true, EpdFontFamily::BOLD);
  r.drawText(SMALL_FONT_ID, x, y + r.getLineHeight(UI_12_FONT_ID), caption, true);
}

// Value over caption, centred in a column.
void centredBlock(const GfxRenderer& r, const int x, const int w, const int y, const char* value, const char* caption,
                  const int valueFont = UI_12_FONT_ID) {
  // A narrow column (side menu layout) drops a long value to 10 pt.
  const int font = r.getTextWidth(valueFont, value, EpdFontFamily::BOLD) > w - 4 ? UI_10_FONT_ID : valueFont;
  const int vw = r.getTextWidth(font, value, EpdFontFamily::BOLD);
  r.drawText(font, x + (w - vw) / 2, y + (r.getLineHeight(valueFont) - r.getLineHeight(font)), value, true,
             EpdFontFamily::BOLD);
  const std::string cap = r.truncatedText(UI_10_FONT_ID, caption, w - 4);
  const int cw = r.getTextWidth(UI_10_FONT_ID, cap.c_str());
  r.drawText(UI_10_FONT_ID, x + (w - cw) / 2, y + r.getLineHeight(valueFont), cap.c_str(), true);
}

// "35h" from 10 hours up, "2h 5m" / "18m" below: fits a narrow column.
void shortDuration(const uint32_t seconds, char* buf, const size_t len) {
  const uint32_t minutes = seconds / 60;
  if (minutes >= 600) {
    snprintf(buf, len, "%luh", static_cast<unsigned long>(minutes / 60));
  } else {
    formatCompactReadingDuration(seconds, buf, len);
  }
}

constexpr StrId kDayLabels[7] = {StrId::STR_STATS_MON, StrId::STR_STATS_TUE, StrId::STR_STATS_WED, StrId::STR_STATS_THU,
                                 StrId::STR_STATS_FRI, StrId::STR_STATS_SAT, StrId::STR_STATS_SUN};

// Black "Now Reading" tag: book icon and label, slanted right end.
int nowTag(const GfxRenderer& r, const int x, const int y) {
  const char* label = tr(STR_NOW_READING);
  const int h = r.getLineHeight(SMALL_FONT_ID) + 8;
  const int w = 10 + 16 + 6 + r.getTextWidth(SMALL_FONT_ID, label) + 8;
  r.fillRoundedRect(x, y, w, h, 4, true, false, true, false, Color::Black);
  for (int row = 0; row < h; ++row) r.drawLine(x + w, y + row, x + w + (h - row) / 2, y + row, true);
  drawLucideIcon(r, icon_deck_book_16, x + 10, y + (h - 16) / 2, false);
  r.drawText(SMALL_FONT_ID, x + 32, y + 4, label, false);
  return h;
}

// Icon over a big value over a caption, centred in a column.
void figure(const GfxRenderer& r, const int x, const int w, const int y, const freeink::Icon& icon, const char* value,
            const char* caption) {
  drawLucideIcon(r, icon, x + (w - 20) / 2, y);
  centredBlock(r, x, w, y + 22, value, caption);
}

void drawNowReading(GfxRenderer& r, const RecentBook& book, const int x, const int y, const int w, const int h,
                    const freeink::Icon* (*iconFor)(UIIcon, uint32_t)) {
  (void)iconFor;
  card(r, x, y, w, h);
  hatch(r, x + w - 64, y + 9, 6);
  // No taller than its thumbnail; centred when the card is taller.
  const int coverH = std::min(h - 2 * kInset - 4, PocketDeckMetrics::values.homeCoverHeight + 8);
  // Narrower beside the side menu, so the text column keeps its width.
  const int coverW = std::min(coverH * 2 / 3, w * 36 / 100);
  drawCover(r, book, x + kInset + 2, y + (h - coverH) / 2, coverW, coverH);
  const int tx = x + kInset + coverW + 16;
  const int tw = x + w - kInset - 4 - tx;
  const int smallH = r.getLineHeight(UI_10_FONT_ID);
  const int valueH = r.getLineHeight(UI_12_FONT_ID);
  char buf[32];

  // Bottom up: button, figures, progress; the title gets the rest.
  const int buttonY = y + h - kInset - kButtonH;
  fillChamfer(r, tx, buttonY, tw, kButtonH, 10, true);
  const char* cont = tr(STR_CONTINUE_READING);
  const int cw = r.getTextWidth(UI_10_FONT_ID, cont, EpdFontFamily::BOLD);
  constexpr int kPlayW = 30;
  // The ">>>" arrows only when they clear the text (not beside the side menu).
  // Centred group shifted left 8 px; the arrows take the last 36 px plus 8 px clear.
  const bool arrows = tw - (cw + kPlayW) >= 72;
  const int cx = tx + (tw - cw - kPlayW) / 2 - (arrows ? 8 : 0);
  drawLucideIcon(r, icon_deck_play_20, cx, buttonY + (kButtonH - 20) / 2, false);
  r.drawText(UI_10_FONT_ID, cx + kPlayW, buttonY + (kButtonH - smallH) / 2, cont, false, EpdFontFamily::BOLD);
  if (arrows) {
    for (int i = 0; i < 3; ++i) chevron(r, tx + tw - 36 + i * 7, buttonY + kButtonH / 2, 6, false);
  }

  const BookReadingStats stats = BookReadingStats::load(book_files::cachePathFor(book.path));
  const float pct = RecentBookProgress::loadPercent(book);
  const int figTop = buttonY - 6 - (22 + valueH + smallH);
  const int colW = tw / 3;
  formatCompactReadingDuration(stats.totalReadingSeconds, buf, sizeof(buf));
  figure(r, tx, colW, figTop, icon_deck_book_20, stats.totalReadingSeconds > 0 ? buf : "-", tr(STR_STATS_READ_SHORT));
  uint32_t left = 0;
  if (!stats.isCompleted && pct >= 0.0f && bookTimeLeftSeconds(stats, pct, left)) {
    formatCompactReadingDuration(left, buf, sizeof(buf));
  } else {
    snprintf(buf, sizeof(buf), "-");
  }
  figure(r, tx + colW, colW, figTop, icon_deck_hourglass_20, buf, tr(STR_STATS_LEFT_SHORT));
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(stats.totalPagesTurned));
  figure(r, tx + colW * 2, colW, figTop, icon_deck_page_20, buf, tr(STR_STATS_PAGES_SHORT));
  r.drawLine(tx + colW, figTop + 6, tx + colW, figTop + 22 + valueH + smallH - 4, true);
  r.drawLine(tx + colW * 2, figTop + 6, tx + colW * 2, figTop + 22 + valueH + smallH - 4, true);

  // Rounded bar with the percentage at its end.
  const int pctY = figTop - 8 - smallH;
  snprintf(buf, sizeof(buf), "%d%%", pct >= 0.0f ? static_cast<int>(pct + 0.5f) : 0);
  const int pw = r.getTextWidth(UI_10_FONT_ID, buf, EpdFontFamily::BOLD);
  r.drawText(UI_10_FONT_ID, tx + tw - pw, pctY, buf, true, EpdFontFamily::BOLD);
  progressBar(r, tx, pctY + (smallH - 10) / 2, tw - pw - 12, 10, pct >= 0.0f ? pct : 0.0f);

  // Tag, title (up to two lines), author.
  int ty = y + kInset;
  ty += nowTag(r, tx, ty) + 3;
  const int authorH = book.author.empty() ? 0 : smallH;
  const int titleRoom = std::max(1, (pctY - 2 - authorH - ty) / valueH);
  const auto titleLines =
      r.wrappedText(UI_12_FONT_ID, book.title.c_str(), tw, std::min(2, titleRoom), EpdFontFamily::BOLD);
  for (const auto& line : titleLines) {
    r.drawText(UI_12_FONT_ID, tx, ty, line.c_str(), true, EpdFontFamily::BOLD);
    ty += valueH;
  }
  if (!book.author.empty()) {
    const std::string author = r.truncatedText(UI_10_FONT_ID, book.author.c_str(), tw);
    r.drawText(UI_10_FONT_ID, tx, ty, author.c_str(), true);
  }
}

void drawReadingStats(GfxRenderer& r, const GlobalReadingStats* g, const int x, const int y, const int w, const int h,
                      const freeink::Icon* (*iconFor)(UIIcon, uint32_t)) {
  (void)iconFor;
  card(r, x, y, w, h);
  const int top = cardTitleSlot(y) + 2;
  const GlobalReadingStats empty{};
  const GlobalReadingStats& s = g != nullptr ? *g : empty;

  ReadingStatsDateTime now;
  const bool hasNow = getCurrentLocalReadingStatsDateTime(now);
  const int todayWd = hasNow ? readingStatsDayOfWeekIndex(now.date) : -1;

  // Title: this week, Monday to Sunday ("12 Oct – 18 Oct").
  char title[40];
  snprintf(title, sizeof(title), "%s", tr(STR_READING_STATS));
  const uint32_t todayIdx = hasNow ? readingStatsDayIndex(now.date) : 0;
  if (hasNow && todayIdx >= static_cast<uint32_t>(todayWd)) {
    ReadingStatsDate mon;
    ReadingStatsDate sun;
    if (readingStatsDateFromDayIndex(todayIdx - todayWd, mon) &&
        readingStatsDateFromDayIndex(todayIdx - todayWd + 6, sun)) {
      char m1[8];
      char m2[8];
      formatReadingStatsMonthToken(mon, m1, sizeof(m1));
      formatReadingStatsMonthToken(sun, m2, sizeof(m2));
      snprintf(title, sizeof(title), "%u %s \xE2\x80\x93 %u %s", static_cast<unsigned>(mon.day), m1,
               static_cast<unsigned>(sun.day), m2);
    }
  }
  cardTitleIcon(r, x, y, icon_deck_calendar_32, title);

  // Left: days read, one row per week (this week at the bottom), Mon..Sun
  // across; read days black, others gray, today framed. Each row is labelled
  // with its Monday.
  const int chartW = w >= 480 ? w * 2 / 3 : w * 60 / 100;
  const int smallH = r.getLineHeight(SMALL_FONT_ID);
  const int rowLabelW = r.getTextWidth(SMALL_FONT_ID, "30 Sep") + 4;
  const int gridX = x + kInset + rowLabelW;
  const int slot = (x + chartW - gridX) / 7;
  const int headY = top;
  const int gridTop = headY + smallH + 2;
  const int pitch = std::max(smallH, 14);
  const int weeks = std::max(1, (y + h - kInset - gridTop) / pitch);
  // Initials for every day ("M T W") when any name would touch its neighbour.
  bool initials = false;
  for (int wd = 0; wd < 7; ++wd) initials |= r.getTextWidth(SMALL_FONT_ID, I18N.get(kDayLabels[wd])) > slot - 3;
  for (int wd = 0; wd < 7; ++wd) {
    const char* full = I18N.get(kDayLabels[wd]);
    char initial[5] = {};
    const char* label = full;
    if (initials) {
      size_t n = 1;
      while (full[n] != '\0' && (static_cast<uint8_t>(full[n]) & 0xC0) == 0x80 && n < 4) ++n;
      memcpy(initial, full, n);
      label = initial;
    }
    const EpdFontFamily::Style st = wd == todayWd ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    const int lw = r.getTextWidth(SMALL_FONT_ID, label, st);
    r.drawText(SMALL_FONT_ID, gridX + wd * slot + (slot - lw) / 2, headY, label, true, st);
  }
  for (int wk = 0; wk < weeks; ++wk) {
    const int cy = gridTop + wk * pitch;
    const int weeksBack = weeks - 1 - wk;
    if (hasNow) {
      ReadingStatsDate monday;
      const uint32_t back = static_cast<uint32_t>(weeksBack * 7 + todayWd);
      if (back <= todayIdx && readingStatsDateFromDayIndex(todayIdx - back, monday)) {
        char m[8];
        char rowLabel[16];
        formatReadingStatsMonthToken(monday, m, sizeof(m));
        snprintf(rowLabel, sizeof(rowLabel), "%u %s", static_cast<unsigned>(monday.day), m);
        r.drawText(SMALL_FONT_ID, x + kInset, cy + (pitch - 2 - smallH) / 2, rowLabel, true,
                   weeksBack == 0 ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
      }
    }
    for (int wd = 0; wd < 7; ++wd) {
      const int cx = gridX + wd * slot + 2;
      const int cw = slot - 4;
      const int ch = pitch - 3;
      const int back = weeksBack * 7 + (todayWd - wd);
      if (!hasNow || back < 0) {
        r.drawRoundedRect(cx, cy, cw, ch, 1, 2, true);  // later this week
        continue;
      }
      const bool read =
          static_cast<uint32_t>(back) <= todayIdx &&
          readingHistoryHasDay(s.readingHistoryAnchorDay, s.readingHistoryBits, todayIdx - static_cast<uint32_t>(back));
      r.fillRoundedRect(cx, cy, cw, ch, 2, read ? Color::Black : Color::LightGray);
      if (back == 0) r.drawRoundedRect(cx - 2, cy - 2, cw + 4, ch + 4, 1, 3, true);  // today
    }
  }

  // Right: total time in the title row, then streak and a ring of the days
  // read in the last seven.
  char buf[24];
  shortDuration(s.totalReadingSeconds, buf, sizeof(buf));
  {
    // Total time on a black pill with a book icon.
    const int tw = r.getTextWidth(UI_10_FONT_ID, buf, EpdFontFamily::BOLD);
    const int ph = r.getLineHeight(UI_10_FONT_ID) + 6;
    const int pw = 12 + 16 + 6 + tw + 14;
    const int px = x + w - kInset - 4 - pw;
    const int py = y + 6 + (32 - ph) / 2;
    r.fillRoundedRect(px, py, pw, ph, ph / 2, Color::Black);
    drawLucideIcon(r, icon_deck_book_16, px + 12, py + (ph - 16) / 2, false);
    r.drawText(UI_10_FONT_ID, px + 34, py + 3, buf, false, EpdFontFamily::BOLD);
  }
  const int divX = x + chartW + 4;
  r.drawLine(divX, top, divX, y + h - kInset, true);
  const int valueH = r.getLineHeight(UI_12_FONT_ID);
  const int rowH = (y + h - kInset - top) / 2;
  const uint16_t streak = hasNow ? s.currentReadingStreak(&now.date) : 0;
  snprintf(buf, sizeof(buf), "%u %s", static_cast<unsigned>(streak),
           streak == 1 ? tr(STR_STATS_DAY) : tr(STR_STATS_DAYS));
  int week = 0;
  if (hasNow) {
    const uint32_t today = readingStatsDayIndex(now.date);
    for (uint32_t k = 0; k < 7 && k <= today; ++k) {
      if (readingHistoryHasDay(s.readingHistoryAnchorDay, s.readingHistoryBits, today - k)) ++week;
    }
  }
  char weekBuf[8];
  snprintf(weekBuf, sizeof(weekBuf), "%d/7", week);
  // Icon, then value over caption, left aligned after it.
  const int ix = divX + 12;
  const int textX = ix + 40;
  const auto statRow = [&](const freeink::Icon& icon, const int rowTop, const char* value, const char* caption) {
    // Caption tucked under the value's descent so two rows fit the card.
    const int capH = r.getLineHeight(SMALL_FONT_ID);
    const int blockY = rowTop + (rowH - (valueH - 4 + capH)) / 2;
    drawLucideIcon(r, icon, ix, rowTop + (rowH - 32) / 2);
    r.drawText(UI_12_FONT_ID, textX, blockY, value, true, EpdFontFamily::BOLD);
    const std::string cap = r.truncatedText(SMALL_FONT_ID, caption, x + w - kInset - textX);
    r.drawText(SMALL_FONT_ID, textX, blockY + valueH - 4, cap.c_str(), true);
  };
  statRow(icon_flame_32, top, buf, tr(STR_STATS_STREAK_SHORT));
  statRow(icon_deck_target_32, top + rowH, weekBuf, tr(STR_STATS_LAST_7_DAYS));
}

// Recent books past 10%, for the View All tile's heading. Counting reads one
// progress file per book (up to 100), which takes seconds on the SD card, so
// Home counts one book per loop pass after it is on screen; -1 until done.
int sRecentAboveTenCount = -1;
struct RecentCounter {
  std::vector<RecentBook> page;  // the current page of up to kPage books
  size_t offset = 0;             // books loaded so far
  size_t next = 0;               // next book in `page`
  bool more = true;
  bool active = false;
  int count = 0;
};
RecentCounter gRecentCounter;

int recentSlotWidth(const int w) { return (w - 2 * kInset) / kRecentSlots; }

void drawRecentReads(GfxRenderer& r, const std::vector<RecentBook>& books, const int x, const int y, const int w,
                     const int h, const freeink::Icon* (*iconFor)(UIIcon, uint32_t)) {
  (void)iconFor;
  card(r, x, y, w, h);
  const int top = cardTitleIcon(r, x, y, icon_deck_stack_32, tr(STR_MENU_RECENT_BOOKS)) + kRecentTopPad;
  const int slot = recentSlotWidth(w);
  const int coverH = std::min(PocketDeckMetrics::kRecentCoverHeight, y + h - kInset - kRecentFootH - top);
  const int count = std::min(static_cast<int>(books.size()), kRecentCovers);
  for (int i = 0; i < count; ++i) {
    const int sx = x + kInset + i * slot;
    const int cw = drawCover(r, books[i], sx + 6, top, slot - 12, coverH);
    const int bx = sx + (slot - cw) / 2;
    const float pct = RecentBookProgress::loadPercent(books[i]);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct >= 0.0f ? static_cast<int>(pct + 0.5f) : 0);
    const int pw = r.getTextWidth(UI_10_FONT_ID, buf);
    const int footY = top + coverH + 4;
    progressBar(r, bx, footY + 8, std::max(6, cw - pw - 6), 6, pct >= 0.0f ? pct : 0.0f);
    r.drawText(UI_10_FONT_ID, bx + cw - pw, footY, buf, true);
  }
  if (count == 0) {
    r.drawText(UI_10_FONT_ID, x + kInset, top + 10, tr(STR_NO_RECENT_BOOKS), true);
  }

  // Last slot: View All Recents (the full Recent Books list), set off from the
  // covers by a divider like the Reading Stats card's.
  const int divX = x + kInset + kRecentCovers * slot;
  r.drawLine(divX, top - 2, divX, y + h - kInset, true);
  const int tx = divX + 6;
  const int tw = slot - 10;
  const int areaTop = top - 6;
  const int areaBottom = y + h - kInset + 4;
  // The tile: a rounded frame around a grid icon and the label.
  r.drawRoundedRect(tx, areaTop, tw, areaBottom - areaTop, 1, 8, true);
  const int tileFont = tw < 110 ? SMALL_FONT_ID : UI_10_FONT_ID;  // narrow beside the side menu
  const int lineH = r.getLineHeight(tileFont);
  const auto lines = r.wrappedText(tileFont, tr(STR_VIEW_ALL_RECENTS), tw - 14, 2, EpdFontFamily::BOLD);
  const int textH = static_cast<int>(lines.size()) * lineH;
  constexpr int kIcon = 32;
  constexpr int kIconGap = 8;
  int ty = areaTop + std::max(0, (areaBottom - areaTop - kIcon - kIconGap - textH) / 2);
  drawLucideIcon(r, icon_deck_grid_32, tx + (tw - kIcon) / 2, ty);
  ty += kIcon + kIconGap;
  constexpr int kChevronW = 5;
  constexpr int kChevronGap = 5;
  for (size_t n = 0; n < lines.size(); ++n) {
    const bool last = n + 1 == lines.size();
    const int lw = r.getTextWidth(tileFont, lines[n].c_str(), EpdFontFamily::BOLD);
    // The last line carries the ">" so text and arrow centre as one group.
    const int groupW = lw + (last ? kChevronGap + kChevronW : 0);
    const int lx = tx + (tw - groupW) / 2;
    r.drawText(tileFont, lx, ty, lines[n].c_str(), true, EpdFontFamily::BOLD);
    if (last) {
      const int cx = lx + lw + kChevronGap;
      const int cy = ty + lineH / 2 + 1;
      for (int d = 0; d < 2; ++d) {
        r.drawLine(cx + d, cy - 5, cx + 4 + d, cy);
        r.drawLine(cx + 4 + d, cy, cx + d, cy + 5);
      }
    }
    ty += lineH;
  }
}
}  // namespace

namespace {
// FNV-1a, enough to tell whether anything drawn on the cards changed.
struct CardsHash {
  uint32_t h = 2166136261u;
  void add(const void* data, const size_t len) {
    const auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) h = (h ^ p[i]) * 16777619u;
  }
  template <typename T>
  void value(const T v) {
    add(&v, sizeof(v));
  }
  void text(const std::string& t) {
    add(t.data(), t.size());
    value<uint8_t>(0);
  }
};
}  // namespace

uint32_t pocketDeckCardsKey(const GfxRenderer& renderer, const std::vector<RecentBook>& books,
                            const GlobalReadingStats& globalStats) {
  CardsHash k;
  k.value<uint8_t>(1);  // bump when the card drawing changes
  k.value(renderer.getScreenWidth());
  k.value(renderer.getScreenHeight());
  k.value(pocketDeckHomeTileHeight(renderer));
  k.value(SETTINGS.homeMenuLayout);
  k.value(SETTINGS.screenInverted);
  k.value(SETTINGS.language);
  // Now Reading and the Recent Books covers: books 0..2.
  const int count = std::min(static_cast<int>(books.size()), kRecentCovers);
  k.value(count);
  for (int i = 0; i < count; ++i) {
    const RecentBook& b = books[i];
    k.text(b.path);
    k.text(b.title);
    k.text(b.author);
    k.text(b.coverBmpPath);
    k.value(RecentBookProgress::loadPercent(b));
    if (!b.coverBmpPath.empty()) {
      // A thumbnail appearing replaces the placeholder.
      k.value(
          Storage.exists(UITheme::getCoverThumbPath(b.coverBmpPath, PocketDeckMetrics::kRecentCoverHeight).c_str()));
      if (i == 0) {
        k.value(Storage.exists(
            UITheme::getCoverThumbPath(b.coverBmpPath, PocketDeckMetrics::values.homeCoverHeight).c_str()));
      }
    }
  }
  if (count > 0) {
    const BookReadingStats st = BookReadingStats::load(book_files::cachePathFor(books[0].path));
    k.value(st.totalReadingSeconds);
    k.value(st.totalPagesTurned);
    k.value(st.isCompleted);
    k.value(st.avgSecondsPerForwardPage);
    k.value(st.estimatedTimeLeftSeconds);
  }
  // Reading Stats: totals, the day history and today's place in the week.
  k.value(globalStats.totalReadingSeconds);
  k.value(globalStats.readingHistoryAnchorDay);
  k.add(globalStats.readingHistoryBits.data(), globalStats.readingHistoryBits.size());
  k.value(globalStats.longestReadingStreak);
  ReadingStatsDateTime now;
  const bool hasNow = getCurrentLocalReadingStatsDateTime(now);
  k.value(hasNow ? readingStatsDayIndex(now.date) : 0u);
  return k.h;
}

int pocketDeckHomeTileHeight(const GfxRenderer& renderer) {
  const auto& m = PocketDeckMetrics::values;
  if (SETTINGS.homeMenuLayout != CrossPointSettings::HOME_MENU_SIDE) return m.homeCoverTileHeight;
  return std::max(m.homeCoverTileHeight, renderer.getScreenHeight() - m.buttonHintsHeight - m.homeTopPadding - 6);
}

int PocketDeckTheme::headerLeftInset() const {
  using namespace PocketDeckMetrics;
  const auto& metrics = UITheme::getInstance().getMetrics();
  return metrics.headerSidePadding + kMarkSize + kMarkGap - metrics.headerSidePadding / 2;
}

void PocketDeckTheme::drawHeader(const GfxRenderer& renderer, const Rect rect, const char* title, const char* subtitle,
                                 const bool readerContext) const {
  using namespace PocketDeckMetrics;
  if (title == nullptr && !readerContext) {
    drawHomeStatusBar(renderer, rect);
    return;
  }
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int markArea = headerLeftInset();
  // Standard Lyra header shifted right to make room for the mark.
  LyraTheme::drawHeader(renderer, Rect{rect.x + markArea, rect.y, rect.width - markArea, rect.height}, title, subtitle,
                        readerContext);
  // Carry the heavy rule under the mark too, except on Home (no title at all;
  // compact headers pass "" and draw their own title text).
  if (title != nullptr) {
    const int ruleY = rect.y + rect.height - metrics.headerUnderlineSize;
    renderer.fillRect(rect.x, ruleY, markArea, metrics.headerUnderlineSize);
  }
  // Upright 1-bit mark (1 = white), drawn per pixel so it follows orientation.
  const int mx = rect.x + metrics.headerSidePadding / 2;
  const int my = rect.y + (rect.height - metrics.headerUnderlineSize - kMarkSize) / 2;
  for (int y = 0; y < kMarkSize; ++y) {
    for (int x = 0; x < kMarkSize; ++x) {
      const uint8_t byte = PocketDeckMark32[y * (kMarkSize / 8) + x / 8];
      if (!(byte & (0x80 >> (x & 7)))) renderer.drawPixel(mx + x, my + y, true);
    }
  }
}

void PocketDeckTheme::drawRecentBookCover(GfxRenderer& renderer, const Rect rect,
                                          const std::vector<RecentBook>& recentBooks, const int selectorIndex,
                                          bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                                          const std::function<bool()>& storeCoverBuffer,
                                          const BookReadingStats* /*stats*/, float /*progressPercent*/,
                                          const GlobalReadingStats* globalStats,
                                          const char* /*currentChapterTitle*/) const {
  (void)bufferRestored;
  const bool side = sideMenu();
  const int x = rect.x + kPad + (side ? kSideW : 0);
  const int w = rect.width - 2 * kPad - (side ? kSideW : 0);
  const int nowY = rect.y + 10;
  const int sideX = rect.x + kPad / 2;
  // Now Reading, then Recent Books, then Reading Stats. A taller area (the
  // side menu frees the bottom bar's rows) grows the cards, mostly Now Reading.
  const int extra = std::max(0, rect.height - PocketDeckMetrics::values.homeCoverTileHeight);
  const int nowH = kNowH + extra * 58 / 100;
  const int recentH = kRecentH + extra * 12 / 100;
  const int statsH = kStatsH + extra - (nowH - kNowH) - (recentH - kRecentH);
  const int recentY = nowY + nowH + kCardGap;
  const int statsY = recentY + recentH + kCardGap;

  // Everything here is static while Home is open: draw it once, snapshot it,
  // and let later renders restore the snapshot and add only the selection.
  if (!coverRendered) {
    if (!recentBooks.empty()) {
      drawNowReading(renderer, recentBooks[0], x, nowY, w, nowH, &LyraTheme::iconForName);
    } else {
      card(renderer, x, nowY, w, nowH);
      drawEmptyRecents(renderer, Rect{x, nowY, w, nowH});
    }
    drawReadingStats(renderer, globalStats, x, statsY, w, statsH, &LyraTheme::iconForName);
    drawRecentReads(renderer, recentBooks, x, recentY, w, recentH, &LyraTheme::iconForName);
    if (side) {
      // The side panel: a white rounded column with a 2 px outline, as tall as the cards.
      renderer.drawRoundedRect(sideX, nowY, kSideItemW, rect.y + rect.height - nowY, 2, 18, true);
      sideItem(renderer, sideX, nowY + 4, kSideItemW, kSideReadH, icon_deck_book_32, tr(STR_READ), false);
    }
    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  }

  // Selection and touch targets: the Now Reading card is book 0; each cover
  // in Recent Reads is its book.
  if (!recentBooks.empty()) {
    TouchRegistry::getInstance().add(Rect{x, nowY, w, nowH}, 0, TouchRegistry::Cover);
    if (side) TouchRegistry::getInstance().add(Rect{sideX, nowY, kSideItemW, kSideReadH}, 0, TouchRegistry::Cover);
  }
  const int slot = recentSlotWidth(w);
  const int count = std::min(static_cast<int>(recentBooks.size()), kRecentCovers);
  for (int i = 1; i < count; ++i) {
    TouchRegistry::getInstance().add(Rect{x + kInset + i * slot, recentY, slot, recentH}, i, TouchRegistry::Cover);
  }
  // The View All tile is Home menu entry 0 (Recent Books): it follows the
  // covers in the selection order, so it is selected at index `count`.
  const int tileX = x + kInset + kRecentCovers * slot;
  TouchRegistry::getInstance().add(Rect{tileX, recentY, slot, recentH}, 0, TouchRegistry::Item);
  if (selectorIndex == count && sRecentAboveTenCount >= 0) {
    // "Recent Books – 9 books above 10%" while the tile is selected.
    char summary[48];
    snprintf(summary, sizeof(summary),
             sRecentAboveTenCount == 1 ? tr(STR_RECENT_ONE_ABOVE_10) : tr(STR_RECENT_ABOVE_10_FORMAT),
             sRecentAboveTenCount);
    const int headX = x + kInset + 34;
    const int headY = recentY + 6 + (32 - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
    const int afterHead = headX + renderer.getTextWidth(UI_12_FONT_ID, tr(STR_MENU_RECENT_BOOKS), EpdFontFamily::BOLD);
    const int space = renderer.getTextWidth(UI_12_FONT_ID, "n");
    const char* dash = "\xE2\x80\x93";  // en dash
    const int dashX = afterHead + space;
    const int textX = dashX + renderer.getTextWidth(UI_12_FONT_ID, dash) + space;
    if (x + w - kInset - textX > 40) {
      const std::string text =
          renderer.truncatedText(UI_12_FONT_ID, summary, x + w - kInset - textX, EpdFontFamily::REGULAR);
      renderer.drawText(UI_12_FONT_ID, dashX, headY, dash, true, EpdFontFamily::REGULAR);
      renderer.drawText(UI_12_FONT_ID, textX, headY, text.c_str(), true, EpdFontFamily::REGULAR);
    }
  }
  if (selectorIndex == count) {
    const int top = recentY + 42 + kRecentTopPad;
    renderer.drawRoundedRect(tileX + 6, top - 6, slot - 10, recentY + recentH - kInset + 4 - (top - 6), 3, 8, true);
  }
  if (selectorIndex >= 0 && selectorIndex < count) {
    // Heading names the selected book: "Recent Books – <title>".
    const char* heading = tr(STR_MENU_RECENT_BOOKS);
    const int headX = x + kInset + 34;
    const int headY = recentY + 6 + (32 - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
    const int afterHead = headX + renderer.getTextWidth(UI_12_FONT_ID, heading, EpdFontFamily::BOLD);
    const int room = x + w - kInset - afterHead;
    if (room > 40) {
      // Dash and title drawn apart, so a Kannada title keeps the spacing.
      const char* dash = "\xE2\x80\x93";  // en dash
      const int space = renderer.getTextWidth(UI_12_FONT_ID, "n");
      const int dashX = afterHead + space;
      const int titleX = dashX + renderer.getTextWidth(UI_12_FONT_ID, dash) + space;
      const std::string title = renderer.truncatedText(UI_12_FONT_ID, recentBooks[selectorIndex].title.c_str(),
                                                       x + w - kInset - titleX, EpdFontFamily::REGULAR);
      renderer.drawText(UI_12_FONT_ID, dashX, headY, dash, true, EpdFontFamily::REGULAR);
      renderer.drawText(UI_12_FONT_ID, titleX, headY, title.c_str(), true, EpdFontFamily::REGULAR);
    }
    if (selectorIndex == 0) {
      drawChamfer(renderer, x, nowY, w, nowH, kChamfer, 4);
      if (side) sideItem(renderer, sideX, nowY + 4, kSideItemW, kSideReadH, icon_deck_book_32, tr(STR_READ), true);
    }
    const int sx = x + kInset + selectorIndex * slot;
    const int top = recentY + 42 + kRecentTopPad;
    const int boxBottom = recentY + recentH - kInset + 4;
    renderer.drawRoundedRect(sx + 2, top - 6, slot - 4, boxBottom - (top - 6), 3, 6, true);
  }
}

void PocketDeckTheme::drawButtonMenu(GfxRenderer& renderer, const Rect rect, const int buttonCount,
                                     const int selectedIndex, const std::function<const char*(int index)>& buttonLabel,
                                     const std::function<UIIcon(int index)>& rowIcon) const {
  // Bottom bar: one cut-corner box per item under a bracketed rule, icon
  // over label; the selected box is filled black with white content.
  if (buttonCount <= 0) return;
  // Recent Books lives in the Recent Books card (View All Recents), not here.
  // Entries keep their menu index for touch and selection; only slots shift.
  const auto shownInBar = [&rowIcon](const int i) { return rowIcon == nullptr || rowIcon(i) != UIIcon::Recent; };
  int shownCount = 0;
  for (int i = 0; i < buttonCount; ++i) shownCount += shownInBar(i) ? 1 : 0;
  if (shownCount <= 0) return;
  if (sideMenu()) {
    // Left column under Read (drawn with the cards), same height as the cards.
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int sx = kPad / 2;
    const int top = metrics.homeTopPadding + 10 + 4 + kSideReadH;
    const int bottom = metrics.homeTopPadding + pocketDeckHomeTileHeight(renderer) - 4;
    const int itemH = std::min(kSideReadH, (bottom - top) / shownCount);  // shrinks only with many items
    int slot = 0;
    for (int i = 0; i < buttonCount; ++i) {
      if (!shownInBar(i)) continue;
      const int iy = top + (slot++) * itemH;
      const UIIcon icon = rowIcon != nullptr ? rowIcon(i) : UIIcon::File;
      const freeink::Icon* bmp = icon == UIIcon::Tools      ? &icon_layout_grid_32
                                 : icon == UIIcon::Settings ? &icon_cog_32
                                                            : iconForName(icon, 32);
      if (bmp == nullptr) bmp = iconForName(UIIcon::Book, 32);
      const char* label = buttonLabel != nullptr ? buttonLabel(i) : "";
      if (icon == UIIcon::Folder) label = tr(STR_TAB_LIBRARY);
      if (icon == UIIcon::Chart) label = tr(STR_TAB_STATS);
      if (icon == UIIcon::Transfer) label = tr(STR_TAB_TRANSFER);
      if (icon == UIIcon::BookmarkIcon) label = tr(STR_TAB_SAVED);
      TouchRegistry::getInstance().add(Rect{sx, iy, kSideItemW, itemH}, i, TouchRegistry::Item);
      if (bmp != nullptr) sideItem(renderer, sx, iy, kSideItemW, itemH, *bmp, label ? label : "", i == selectedIndex);
    }
    return;
  }
  const int x = rect.x + kPad;
  const int w = rect.width - 2 * kPad;
  const int cols = std::min(shownCount, 7);  // one row, even with OPDS
  const int rows = (shownCount + cols - 1) / cols;
  const int rowH = std::min(76, std::max(30, (rect.height - 6) / rows));
  // Sit on the bottom of the area (just above the button hints).
  const int barTop = rect.y + std::max(0, rect.height - 4 - rows * rowH);
  const bool withIcons = rowH >= 58;
  // Equal columns; the pixels integer division leaves over go to both ends.
  const int cellW = w / cols;
  const int barX = x + (w - cols * cellW) / 2;
  // The rule separating the bar from the cards above.
  deckRule(renderer, rect.x + kPad, rect.x + rect.width - kPad - 1, barTop - 5);
  // Short tab names for the common items; others keep their menu label.
  const auto tabLabel = [&](const int i) -> const char* {
    switch (rowIcon != nullptr ? rowIcon(i) : UIIcon::File) {
      case UIIcon::Folder:
        return tr(STR_TAB_LIBRARY);
      case UIIcon::Recent:
        return tr(STR_TAB_RECENT);
      case UIIcon::Chart:
        return tr(STR_TAB_STATS);
      case UIIcon::Transfer:
        return tr(STR_TAB_TRANSFER);
      case UIIcon::BookmarkIcon:
        return tr(STR_TAB_SAVED);
      default: {
        const char* l = buttonLabel != nullptr ? buttonLabel(i) : "";
        return l ? l : "";
      }
    }
  };
  // One label size for the whole bar: 10 pt only if every label keeps at
  // least 12 px clear of its box's sides; otherwise the small size for all.
  int labelFont = UI_10_FONT_ID;
  for (int i = 0; i < buttonCount; ++i) {
    if (!shownInBar(i)) continue;
    if (renderer.getTextWidth(UI_10_FONT_ID, tabLabel(i)) > cellW - 8 - 24) labelFont = SMALL_FONT_ID;
  }
  const int labelH = renderer.getLineHeight(labelFont);
  constexpr int kBoxGap = 4;  // each side of a box
  constexpr int kBoxCut = 9;
  int slotIndex = 0;
  for (int i = 0; i < buttonCount; ++i) {
    if (!shownInBar(i)) continue;
    const int slotPos = slotIndex++;
    const int cx = barX + (slotPos % cols) * cellW;
    const int cy = barTop + (slotPos / cols) * rowH;
    const int ch = rowH - 4;
    TouchRegistry::getInstance().add(Rect{cx, cy, cellW, ch}, i, TouchRegistry::Item);
    const UIIcon icon = rowIcon != nullptr ? rowIcon(i) : UIIcon::File;
    const bool selected = i == selectedIndex;
    const int bx = cx + kBoxGap;
    const int bw = cellW - 2 * kBoxGap;
    if (selected) {
      fillChamfer(renderer, bx, cy, bw, ch, kBoxCut, true);
    } else {
      fillChamfer(renderer, bx, cy, bw, ch, kBoxCut, false);
      drawChamfer(renderer, bx, cy, bw, ch, kBoxCut, 2);
    }
    if (withIcons) {
      const freeink::Icon* bmp = icon == UIIcon::Tools      ? &icon_layout_grid_32
                                 : icon == UIIcon::Settings ? &icon_cog_32
                                                            : iconForName(icon, 32);
      if (bmp == nullptr) bmp = iconForName(UIIcon::Book, 32);
      if (bmp != nullptr)
        drawLucideIcon(renderer, *bmp, cx + (cellW - 32) / 2, cy + (ch - 32 - 4 - labelH) / 2, !selected);
    }
    const char* label = tabLabel(i);
    const std::string text = renderer.truncatedText(labelFont, label ? label : "", bw - 16);
    const int lw = renderer.getTextWidth(labelFont, text.c_str());
    // Icon and label centred as one group in the box.
    const int ly = withIcons ? cy + (ch - 32 - 4 - labelH) / 2 + 36 : cy + (ch - labelH) / 2;
    renderer.drawText(labelFont, cx + (cellW - lw) / 2, ly, text.c_str(), !selected);
  }
}

namespace {
// Quote of the day on Home's second status row. Longer than the row, it steps
// a word at a time: a short hold at the start, a step every kStepMs, a hold at
// the end, then back to the start, for kTickerPasses (5) passes per Home visit. Only
// that line changes, so each step is a small differential refresh.
constexpr unsigned long kTickerStepMs = 2000;
constexpr unsigned long kTickerHoldStartMs = 4000;
constexpr unsigned long kTickerHoldEndMs = 3000;
constexpr int kTickerPasses = 5;

struct QuoteTicker {
  std::string text;  // “quote” — author
  int32_t day = INT32_MIN;
  size_t offset = 0;  // byte offset of the first shown word
  bool overflow = false;
  bool atEnd = false;
  int passes = 0;
  unsigned long nextMs = 0;
};
QuoteTicker gTicker;

// Today's quote from Tools > Daily Quote, read once per day.
void refreshTickerQuote() {
  tools::DateTime local;
  if (!tools::getLocalNow(local)) return;  // no date, no "of the day"
  const int32_t day = tools::daysOf(local);
  if (day == gTicker.day) return;
  gTicker.day = day;
  gTicker.offset = 0;
  gTicker.nextMs = 0;
  gTicker.passes = 0;
  std::string line;
  gTicker.text.clear();
  if (!DailyQuoteActivity::quoteForDay(day, line)) return;
  // “text” — author: split at the last em dash.
  const size_t dash = line.rfind(" \xE2\x80\x94 ");
  gTicker.text = "\xE2\x80\x9C";
  gTicker.text += line.substr(0, dash);
  gTicker.text += "\xE2\x80\x9D";
  if (dash != std::string::npos) gTicker.text += line.substr(dash);
}

// Draws the ticker in [x, x + w) at y; updates overflow / atEnd.
void drawTicker(const GfxRenderer& r, const int x, const int y, const int w) {
  const std::string& t = gTicker.text;
  gTicker.overflow = r.getTextWidth(SMALL_FONT_ID, t.c_str()) > w;
  if (!gTicker.overflow) gTicker.offset = 0;
  if (gTicker.offset >= t.size()) gTicker.offset = 0;
  const std::string rest = t.substr(gTicker.offset);
  // Whole words that fit; never a cut-off word.
  size_t fit = 0;
  for (size_t pos = 0;;) {
    const size_t sp = rest.find(' ', pos);
    const size_t end = sp == std::string::npos ? rest.size() : sp;
    if (r.getTextWidth(SMALL_FONT_ID, rest.substr(0, end).c_str()) > w) break;
    fit = end;
    if (sp == std::string::npos) break;
    pos = sp + 1;
  }
  const bool wasAtEnd = gTicker.atEnd;
  gTicker.atEnd = fit >= rest.size();
  if (gTicker.overflow && gTicker.atEnd && !wasAtEnd) gTicker.nextMs = millis() + kTickerHoldEndMs;
  if (fit > 0) r.drawText(SMALL_FONT_ID, x, y, rest.substr(0, fit).c_str(), true);
}
}  // namespace

void pocketDeckRecentCountReset() {
  sRecentAboveTenCount = -1;
  gRecentCounter.page.clear();
  gRecentCounter.offset = 0;
  gRecentCounter.next = 0;
  gRecentCounter.more = true;
  gRecentCounter.count = 0;
  gRecentCounter.active = true;
}

bool pocketDeckRecentCountActive() { return gRecentCounter.active; }

void pocketDeckRecentCountStop() {
  gRecentCounter.active = false;
  std::vector<RecentBook>().swap(gRecentCounter.page);
}

bool pocketDeckRecentCountStep() {
  constexpr size_t kPage = 10;
  RecentCounter& c = gRecentCounter;
  if (!c.active) return false;
  if (c.next >= c.page.size()) {
    if (!c.more || c.offset >= static_cast<size_t>(RecentBooksStore::MAX_TOTAL_BOOKS)) {
      // Done: publish the count and give the page's memory back.
      c.active = false;
      std::vector<RecentBook>().swap(c.page);
      sRecentAboveTenCount = c.count;
      return true;
    }
    c.page.clear();
    c.page.reserve(kPage);
    c.more = RECENT_BOOKS.loadPage(c.page, c.offset, kPage);
    c.offset += c.page.size();
    c.next = 0;
    if (c.page.empty()) c.more = false;
    return false;
  }
  const RecentBook& book = c.page[c.next++];
  const float pct = FsHelpers::hasEpubExtension(book.path) ? RecentBookProgress::loadCachedEpubPercent(book)
                                                           : RecentBookProgress::loadPercent(book);
  if (RecentBookProgress::hasPercent(pct) && pct > 10.0f) ++c.count;
  return false;
}

void pocketDeckQuoteTickerReset() {
  gTicker.offset = 0;
  gTicker.nextMs = 0;
  gTicker.passes = 0;
  gTicker.atEnd = false;
}

bool pocketDeckQuoteTick(const unsigned long nowMs) {
  if (!gTicker.overflow || gTicker.passes >= kTickerPasses) return false;
  if (gTicker.nextMs == 0) {
    gTicker.nextMs = nowMs + kTickerHoldStartMs;
    return false;
  }
  if (static_cast<long>(nowMs - gTicker.nextMs) < 0) return false;
  if (gTicker.atEnd) {
    gTicker.offset = 0;
    gTicker.atEnd = false;
    gTicker.nextMs = nowMs + kTickerHoldStartMs;
    ++gTicker.passes;
  } else {
    const size_t sp = gTicker.text.find(' ', gTicker.offset);
    gTicker.offset = sp == std::string::npos ? 0 : sp + 1;
    gTicker.nextMs = nowMs + kTickerStepMs;
  }
  return true;
}

void PocketDeckTheme::drawHomeStatusBar(const GfxRenderer& renderer, const Rect rect) const {
  using namespace PocketDeckMetrics;
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int left = rect.x + kPad;
  const int right = rect.x + rect.width - kPad;
  const int rowTop = rect.y + UITheme::getTopStatusBarInset(renderer) + 2;
  const int smallH = renderer.getLineHeight(SMALL_FONT_ID);
  const int nameX = left + kMarkSize + 6;

  refreshTickerQuote();
  const std::string owner = renderer.truncatedText(SMALL_FONT_ID, SETTINGS.ownerName, 150);
  // Two rows when there is an owner name or a quote: brand, date, time and
  // battery on top; owner name and the quote below. Otherwise one row.
  const bool twoRows = !owner.empty() || !gTicker.text.empty();
  const int midY = twoRows ? rowTop + 8 : rowTop + kMarkSize / 2;
  const int textY = midY - smallH / 2;

  // Top row segments after the brand, spread evenly with "|" separators.
  struct Segment {
    const freeink::Icon* icon;
    char text[20];
    int width;
  };
  Segment segs[3];
  int count = 0;
  const auto add = [&](const freeink::Icon* icon, const char* text) {
    Segment& sg = segs[count++];
    sg.icon = icon;
    snprintf(sg.text, sizeof(sg.text), "%s", text ? text : "");
    sg.width = (icon ? 16 : 0) + (icon && sg.text[0] ? 5 : 0) + renderer.getTextWidth(SMALL_FONT_ID, sg.text);
  };
  char timeBuf[12];
  if (halClock.isAvailable() && SETTINGS.shouldShowClockOutsideReader() &&
      halClock.formatTime(timeBuf, sizeof(timeBuf), SETTINGS.clockUtcOffsetQ, SETTINGS.clockFormat == 1)) {
    char dateBuf[16];
    if (formatHeaderDateText(dateBuf, sizeof(dateBuf))) add(&icon_deck_calendar_16, dateBuf);
    add(&icon_deck_clock_16, timeBuf);
  }
  if (hasActiveStationWifiConnection()) add(&icon_deck_wifi_16, "");

  const char* brand = tr(STR_HOME_BRAND);
  const int brandW = kMarkSize + 6 + renderer.getTextWidth(UI_10_FONT_ID, brand, EpdFontFamily::BOLD);
  const bool showPct = SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_ALWAYS;
  char pctBuf[8];
  snprintf(pctBuf, sizeof(pctBuf), "%u%%", static_cast<unsigned>(powerManager.getBatteryPercentage()));
  const int batteryW =
      metrics.batteryWidth + 2 + (showPct ? batteryPercentSpacing + renderer.getTextWidth(SMALL_FONT_ID, pctBuf) : 0);
  int total = brandW + batteryW;
  for (int i = 0; i < count; ++i) total += segs[i].width;
  const int gap = std::max(10, (right - left - total) / (count + 1));

  // The mark spans both rows.
  for (int y = 0; y < kMarkSize; ++y) {
    for (int x = 0; x < kMarkSize; ++x) {
      const uint8_t byte = PocketDeckMark32[y * (kMarkSize / 8) + x / 8];
      if (!(byte & (0x80 >> (x & 7)))) renderer.drawPixel(left + x, rowTop + y, true);
    }
  }
  renderer.drawText(UI_10_FONT_ID, nameX, midY - renderer.getLineHeight(UI_10_FONT_ID) / 2, brand, true,
                    EpdFontFamily::BOLD);
  int cx = left + brandW;
  const auto separator = [&] {
    renderer.drawLine(cx + gap / 2, midY - 7, cx + gap / 2, midY + 7, true);
    cx += gap;
  };
  for (int i = 0; i < count; ++i) {
    separator();
    int ix = cx;
    if (segs[i].icon) {
      drawLucideIcon(renderer, *segs[i].icon, ix, midY - 8);
      ix += 16 + 5;
    }
    if (segs[i].text[0]) renderer.drawText(SMALL_FONT_ID, ix, textY, segs[i].text, true);
    cx += segs[i].width;
  }
  separator();
  const int iconX = right - metrics.batteryWidth - 2;
  drawBatteryRight(renderer, Rect{iconX, textY, metrics.batteryWidth, metrics.batteryHeight}, showPct);

  if (twoRows) {
    // A 2 px line splits the rows; below it the owner name, a short divider,
    // then the quote to the right edge.
    const int lineY = rowTop + 23;
    renderer.fillRect(nameX, lineY, right - nameX, 2, true);
    const int row2Y = lineY + 4;
    int qx = nameX;
    if (!owner.empty()) {
      renderer.drawText(SMALL_FONT_ID, nameX, row2Y, owner.c_str(), true);
      qx = nameX + renderer.getTextWidth(SMALL_FONT_ID, owner.c_str());
      if (!gTicker.text.empty()) {
        const int divY = row2Y + smallH / 2;
        renderer.drawLine(qx + 8, divY - 6, qx + 8, divY + 6, true);
        qx += 16;
      }
    }
    if (!gTicker.text.empty()) drawTicker(renderer, qx, row2Y, right - qx);
  }

  deckRule(renderer, left - kPad / 2, right + kPad / 2 - 1, rect.y + rect.height - 4);
}

void PocketDeckTheme::drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                                      const char* btn4, const bool allowInvertedText) const {
  // Lyra's positions (they sit over the physical buttons), drawn as cut-corner
  // pills; Read, Select, Up and Down carry a small icon.
  if (gpio.hasTouch()) return;
  const GfxRenderer::Orientation orig = renderer.getOrientation();
  const bool invertText = allowInvertedText && orig == GfxRenderer::Orientation::PortraitInverted;
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  const int pageHeight = renderer.getScreenHeight();
  constexpr int kW = 80;
  constexpr int kH = LyraMetrics::values.buttonHintsHeight - 4;
  constexpr int kStubH = 13;
  constexpr int kCut = 7;
  constexpr int narrow[] = {58, 146, 254, 342};
  constexpr int wide[] = {65, 157, 291, 383};
  const int* pos = renderer.getScreenWidth() >= 528 ? wide : narrow;
  const char* labels[] = {btn1, btn2, btn3, btn4};
  const int top = pageHeight - kH - 2;

  for (int i = 0; i < 4; ++i) {
    const int x = pos[i];
    if (labels[i] == nullptr) continue;
    // Clear the full slot first so an old label never shows through a stub.
    renderer.fillRect(x, pageHeight - LyraMetrics::values.buttonHintsHeight, kW, LyraMetrics::values.buttonHintsHeight,
                      false);
    if (labels[i][0] != '\0') {
      TouchRegistry::getInstance().add(Rect{x, top, kW, kH}, i, TouchRegistry::Button);
      drawChamfer(renderer, x, top, kW, kH, kCut, 2);
    } else {
      drawChamfer(renderer, x, pageHeight - kStubH - 1, kW, kStubH, 4, 1);
    }
  }

  renderer.setOrientation(invertText ? GfxRenderer::Orientation::PortraitInverted : GfxRenderer::Orientation::Portrait);
  const int lineH = renderer.getLineHeight(SMALL_FONT_ID);
  const int textTop = invertText ? 2 + (kH - lineH) / 2 : top + (kH - lineH) / 2;
  for (int i = 0; i < 4; ++i) {
    if (labels[i] == nullptr || labels[i][0] == '\0') continue;
    const int x = pos[invertText ? 3 - i : i];
    // A small mark for the common actions.
    enum class Mark { None, Read, Select, Up, Down } mark = Mark::None;
    if (strcmp(labels[i], tr(STR_READ)) == 0)
      mark = Mark::Read;
    else if (strcmp(labels[i], tr(STR_SELECT)) == 0)
      mark = Mark::Select;
    else if (strcmp(labels[i], tr(STR_DIR_UP)) == 0)
      mark = Mark::Up;
    else if (strcmp(labels[i], tr(STR_DIR_DOWN)) == 0)
      mark = Mark::Down;
    // Icon width plus gap; the Select check and the arrows are narrower than the book.
    int markW = mark == Mark::None ? 0 : (mark == Mark::Read ? 16 + 4 : (mark == Mark::Select ? 10 + 4 : 12 + 6));
    // Keep at least 8 px clear on each side; a label too wide for its mark drops it.
    if (markW > 0 && renderer.getTextWidth(SMALL_FONT_ID, labels[i]) + markW > kW - 16) {
      mark = Mark::None;
      markW = 0;
    }
    const std::string label = renderer.truncatedText(SMALL_FONT_ID, labels[i], kW - 10 - markW);
    const int tw = renderer.getTextWidth(SMALL_FONT_ID, label.c_str());
    const int gx = x + (kW - 1 - tw - markW) / 2;
    const int midY = textTop + lineH / 2;
    switch (mark) {
      case Mark::Read:
        drawLucideIcon(renderer, icon_deck_book_16, gx, midY - 8);
        break;
      case Mark::Select:
        // A 2 px check mark: crisp at this size, unlike a dashed square.
        // Bold: three offset strokes.
        for (int d = 0; d < 3; ++d) {
          const int dx = d == 2 ? 1 : 0;
          const int dy = d == 1 ? 1 : 0;
          renderer.drawLine(gx + dx, midY + dy, gx + 3 + dx, midY + 3 + dy, true);
          renderer.drawLine(gx + 3 + dx, midY + 3 + dy, gx + 9 + dx, midY - 4 + dy, true);
        }
        break;
      case Mark::Up:
      case Mark::Down:
        chevronVertical(renderer, gx + 6, midY, 6, mark == Mark::Up);
        break;
      case Mark::None:
        break;
    }
    renderer.drawText(SMALL_FONT_ID, gx + markW, textTop, label.c_str(), true);
  }
  renderer.setOrientation(orig);
}
