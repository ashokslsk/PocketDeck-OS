#include "PocketDeckTheme.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "RecentBooksStore.h"
#include "activities/home/RecentBookProgress.h"
#include "activities/reader/BookInsights.h"
#include "activities/reader/BookReadingStats.h"
#include "activities/reader/GlobalReadingStats.h"
#include "activities/reader/ReadingStatsUtils.h"
#include "activities/tools/ToolsCommon.h"
#include "components/TouchRegistry.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "components/icons/toolsIcon.h"
#include "fontIds.h"
#include "images/PocketDeckMark32.h"

namespace {
constexpr int kPad = 12;      // screen edge to card
constexpr int kInset = 12;    // card edge to content
constexpr int kRadius = 10;   // card corners
constexpr int kCardGap = 14;  // between cards
constexpr int kNowH = 250;    // Now Reading card
constexpr int kStatsH = 150;  // Reading Stats card
constexpr int kRecentSlots = 4;   // covers plus the View All Recents tile
constexpr int kRecentCovers = 3;  // == homeRecentBooksCount
constexpr int kRecentH = 166;     // Recent Books card
constexpr int kButtonH = 40;      // Continue Reading button
constexpr int kRecentTopPad = 8;  // heading to covers
constexpr int kRecentFootH = 26;  // progress row under the covers

void card(const GfxRenderer& r, const int x, const int y, const int w, const int h) {
  r.fillRoundedRect(x, y, w, h, kRadius, Color::White);
  r.drawRoundedRect(x, y, w, h, 1, kRadius, true);
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

// Rounded progress bar: gray track, black fill.
void progressBar(const GfxRenderer& r, const int x, const int y, const int w, const int h, const float pct) {
  r.fillRectDither(x, y, w, h, Color::LightGray);
  const int fill = std::clamp(static_cast<int>(w * pct / 100.0f), 0, w);
  if (fill > 0) r.fillRect(x, y, std::max(h, fill), h, true);
}

// Big value over a small caption, left aligned.
void valueBlock(const GfxRenderer& r, const int x, const int y, const char* value, const char* caption) {
  r.drawText(UI_12_FONT_ID, x, y, value, true, EpdFontFamily::BOLD);
  r.drawText(SMALL_FONT_ID, x, y + r.getLineHeight(UI_12_FONT_ID), caption, true);
}

// Value over caption, centred in a column.
void centredBlock(const GfxRenderer& r, const int x, const int w, const int y, const char* value, const char* caption,
                  const int valueFont = UI_12_FONT_ID) {
  const int vw = r.getTextWidth(valueFont, value, EpdFontFamily::BOLD);
  r.drawText(valueFont, x + (w - vw) / 2, y, value, true, EpdFontFamily::BOLD);
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

void drawNowReading(GfxRenderer& r, const RecentBook& book, const int x, const int y, const int w,
                    const freeink::Icon* (*iconFor)(UIIcon, uint32_t)) {
  card(r, x, y, w, kNowH);
  const int coverH = kNowH - 2 * kInset;
  drawCover(r, book, x + kInset, y + kInset, coverH * 2 / 3, coverH);
  const int tx = x + kInset + coverH * 2 / 3 + 14;
  const int tw = x + w - kInset - tx;
  const int smallH = r.getLineHeight(UI_10_FONT_ID);
  const int valueH = r.getLineHeight(UI_12_FONT_ID);
  char buf[32];

  // Bottom up: button, figures, progress; the title gets the rest.
  const int buttonY = y + kNowH - kInset - kButtonH;
  r.fillRoundedRect(tx, buttonY, tw, kButtonH, 8, Color::Black);
  const char* cont = tr(STR_CONTINUE_READING);
  const int cw = r.getTextWidth(UI_10_FONT_ID, cont, EpdFontFamily::BOLD);
  const freeink::Icon* bookIcon = iconFor(UIIcon::Book, 24);
  const int iconW = bookIcon != nullptr ? 32 : 0;
  const int cx = tx + (tw - cw - iconW) / 2;
  if (bookIcon != nullptr) drawLucideIcon(r, *bookIcon, cx, buttonY + (kButtonH - 24) / 2, false);
  r.drawText(UI_10_FONT_ID, cx + iconW, buttonY + (kButtonH - r.getLineHeight(UI_10_FONT_ID)) / 2, cont, false,
             EpdFontFamily::BOLD);
  r.drawText(UI_10_FONT_ID, tx + tw - 18, buttonY + (kButtonH - r.getLineHeight(UI_10_FONT_ID)) / 2, ">", false,
             EpdFontFamily::BOLD);

  const BookReadingStats stats = BookReadingStats::load(book_files::cachePathFor(book.path));
  const float pct = RecentBookProgress::loadPercent(book);
  const int figH = r.getLineHeight(UI_12_FONT_ID);
  const int figY = buttonY - 8 - figH - smallH;
  const int colW = tw / 3;
  formatCompactReadingDuration(stats.totalReadingSeconds, buf, sizeof(buf));
  centredBlock(r, tx, colW, figY, stats.totalReadingSeconds > 0 ? buf : "-", tr(STR_STATS_READ_SHORT));
  uint32_t left = 0;
  if (!stats.isCompleted && pct >= 0.0f && bookTimeLeftSeconds(stats, pct, left)) {
    formatCompactReadingDuration(left, buf, sizeof(buf));
  } else {
    snprintf(buf, sizeof(buf), "-");
  }
  centredBlock(r, tx + colW, colW, figY, buf, tr(STR_STATS_LEFT_SHORT));
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(stats.totalPagesTurned));
  centredBlock(r, tx + colW * 2, colW, figY, buf, tr(STR_STATS_PAGES_SHORT));
  r.drawLine(tx + colW, figY + 2, tx + colW, figY + figH + smallH - 2, true);
  r.drawLine(tx + colW * 2, figY + 2, tx + colW * 2, figY + figH + smallH - 2, true);

  // Thick bar with the percentage at its end.
  const int pctH = r.getLineHeight(UI_10_FONT_ID);
  const int pctY = figY - 10 - pctH;
  snprintf(buf, sizeof(buf), "%d%%", pct >= 0.0f ? static_cast<int>(pct + 0.5f) : 0);
  const int pw = r.getTextWidth(UI_10_FONT_ID, buf, EpdFontFamily::BOLD);
  r.drawText(UI_10_FONT_ID, tx + tw - pw, pctY, buf, true, EpdFontFamily::BOLD);
  progressBar(r, tx, pctY + (pctH - 10) / 2, tw - pw - 10, 10, pct >= 0.0f ? pct : 0.0f);

  // "Now Reading", title (up to two lines), author.
  int ty = y + kInset;
  r.drawText(UI_10_FONT_ID, tx, ty, tr(STR_NOW_READING), true);
  ty += smallH + 2;
  const int authorH = book.author.empty() ? 0 : r.getLineHeight(UI_10_FONT_ID) + 2;
  const int titleRoom = std::max(1, (pctY - 4 - authorH - ty) / valueH);
  const auto titleLines =
      r.wrappedText(UI_12_FONT_ID, book.title.c_str(), tw, std::min(2, titleRoom), EpdFontFamily::BOLD);
  for (const auto& line : titleLines) {
    r.drawText(UI_12_FONT_ID, tx, ty, line.c_str(), true, EpdFontFamily::BOLD);
    ty += valueH;
  }
  if (!book.author.empty()) {
    const std::string author = r.truncatedText(UI_10_FONT_ID, book.author.c_str(), tw);
    r.drawText(UI_10_FONT_ID, tx, ty + 2, author.c_str(), true);
  }
}

void drawReadingStats(GfxRenderer& r, const GlobalReadingStats* g, const int x, const int y, const int w,
                      const freeink::Icon* (*iconFor)(UIIcon, uint32_t)) {
  card(r, x, y, w, kStatsH);
  const int top = cardTitleSlot(y);
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
  cardTitle(r, x, y, UIIcon::Chart, title, iconFor);

  // Left: days read, one row per week (this week at the bottom), Mon..Sun
  // across; read days black, others gray, today framed. Each row is labelled
  // with its Monday.
  const int chartW = w * 2 / 3;
  const int labelH = r.getLineHeight(UI_10_FONT_ID);
  const int smallH = r.getLineHeight(SMALL_FONT_ID);
  const int rowLabelW = r.getTextWidth(SMALL_FONT_ID, "30 Sep") + 4;
  const int gridX = x + kInset + rowLabelW;
  const int slot = (x + chartW - gridX) / 7;
  const int headY = top;
  const int gridTop = headY + smallH + 2;
  const int pitch = std::max(smallH, 14);
  const int weeks = std::max(1, (y + kStatsH - kInset - gridTop) / pitch);
  for (int wd = 0; wd < 7; ++wd) {
    const char* label = I18N.get(kDayLabels[wd]);
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
        r.drawRect(cx, cy, cw, ch, true);  // later this week
        continue;
      }
      const bool read =
          static_cast<uint32_t>(back) <= todayIdx &&
          readingHistoryHasDay(s.readingHistoryAnchorDay, s.readingHistoryBits, todayIdx - static_cast<uint32_t>(back));
      if (read) {
        r.fillRect(cx, cy, cw, ch, true);
      } else {
        r.fillRectDither(cx, cy, cw, ch, Color::LightGray);
      }
      if (back == 0) r.drawRect(cx - 2, cy - 2, cw + 4, ch + 4, true);  // today
    }
  }

  // Right: total time in the title row, then streak and a ring of the days
  // read in the last seven.
  char buf[24];
  shortDuration(s.totalReadingSeconds, buf, sizeof(buf));
  const int tw = r.getTextWidth(UI_12_FONT_ID, buf, EpdFontFamily::BOLD);
  r.drawText(UI_12_FONT_ID, x + w - kInset - tw, y + 4 + (32 - r.getLineHeight(UI_12_FONT_ID)) / 2, buf, true,
             EpdFontFamily::BOLD);
  const int divX = x + chartW + 4;
  r.drawLine(divX, top, divX, y + kStatsH - kInset, true);
  const int valueH = r.getLineHeight(UI_12_FONT_ID);
  const int rowH = (y + kStatsH - kInset - top) / 2;
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
  // The block (marks + text) sits against the right edge, leaving space
  // after the divider.
  const int ringR = std::min(17, rowH / 2 - 2);
  const int textW = std::max({r.getTextWidth(UI_12_FONT_ID, buf, EpdFontFamily::BOLD),
                              r.getTextWidth(UI_10_FONT_ID, tr(STR_STATS_STREAK_SHORT)),
                              r.getTextWidth(UI_12_FONT_ID, weekBuf, EpdFontFamily::BOLD),
                              r.getTextWidth(UI_10_FONT_ID, tr(STR_STATS_LAST_7_DAYS))});
  const int rx = std::max(divX + 8, x + w - kInset - (ringR * 2 + 8 + textW));
  // Text lines end flush with the card's right edge.
  const int textRight = x + w - kInset;
  const auto rightText = [&](const int font, const int ty, const char* t, const EpdFontFamily::Style st) {
    r.drawText(font, textRight - r.getTextWidth(font, t, st), ty, t, true, st);
  };
  rightText(UI_12_FONT_ID, top + (rowH - valueH - labelH) / 2, buf, EpdFontFamily::BOLD);
  rightText(UI_10_FONT_ID, top + (rowH - valueH - labelH) / 2 + valueH, tr(STR_STATS_STREAK_SHORT),
            EpdFontFamily::REGULAR);
  // Streak mark: a flame.
  drawLucideIcon(r, icon_flame_32, rx + ringR - 16, top + rowH / 2 - 16);
  const int ringCy = top + rowH + rowH / 2;
  tools::drawProgressRing(r, rx + ringR, ringCy, ringR, 8, week / 7.0f);
  rightText(UI_12_FONT_ID, ringCy - (valueH + labelH) / 2, weekBuf, EpdFontFamily::BOLD);
  rightText(UI_10_FONT_ID, ringCy - (valueH + labelH) / 2 + valueH, tr(STR_STATS_LAST_7_DAYS), EpdFontFamily::REGULAR);
}

// Recent books past 10%, counted when the Recent Books card is drawn (once
// per Home visit); -1 until then.
int sRecentAboveTenCount = -1;

int recentSlotWidth(const int w) { return (w - 2 * kInset) / kRecentSlots; }

void drawRecentReads(GfxRenderer& r, const std::vector<RecentBook>& books, const int x, const int y, const int w,
                     const int h, const freeink::Icon* (*iconFor)(UIIcon, uint32_t)) {
  card(r, x, y, w, h);
  const int top = cardTitle(r, x, y, UIIcon::Recent, tr(STR_MENU_RECENT_BOOKS), iconFor) + kRecentTopPad;
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
    progressBar(r, bx, footY + 8, std::max(4, cw - pw - 6), 5, pct >= 0.0f ? pct : 0.0f);
    r.drawText(UI_10_FONT_ID, bx + cw - pw, footY, buf, true);
  }
  if (count == 0) {
    r.drawText(UI_10_FONT_ID, x + kInset, top + 10, tr(STR_NO_RECENT_BOOKS), true);
  }

  // Last slot: View All Recents (the full Recent Books list), set off from the
  // covers by a divider like the Reading Stats card's.
  const int divX = x + kInset + kRecentCovers * slot;
  r.drawLine(divX, top - 2, divX, y + h - kInset, true);
  // How many recent books are past 10%, for the heading when the tile is
  // selected. Counted here: this part is drawn once per Home visit.
  sRecentAboveTenCount = RecentBookProgress::countRecentBooksAbove(10.0f);

  const int tx = divX + 6;
  const int tw = slot - 10;
  const int areaTop = top;
  const int areaBottom = y + h - kInset;
  const int lineH = r.getLineHeight(UI_10_FONT_ID);
  const auto lines = r.wrappedText(UI_10_FONT_ID, tr(STR_VIEW_ALL_RECENTS), tw - 14, 2, EpdFontFamily::BOLD);
  const int textH = static_cast<int>(lines.size()) * lineH;
  constexpr int kIcon = 32;
  constexpr int kIconGap = 8;
  int ty = areaTop + std::max(0, (areaBottom - areaTop - kIcon - kIconGap - textH) / 2);
  if (const freeink::Icon* bmp = iconFor(UIIcon::Library, kIcon)) {
    drawLucideIcon(r, *bmp, tx + (tw - kIcon) / 2, ty);
  }
  ty += kIcon + kIconGap;
  constexpr int kChevronW = 5;
  constexpr int kChevronGap = 5;
  for (size_t n = 0; n < lines.size(); ++n) {
    const bool last = n + 1 == lines.size();
    const int lw = r.getTextWidth(UI_10_FONT_ID, lines[n].c_str(), EpdFontFamily::BOLD);
    // The last line carries the ">" so text and arrow centre as one group.
    const int groupW = lw + (last ? kChevronGap + kChevronW : 0);
    const int lx = tx + (tw - groupW) / 2;
    r.drawText(UI_10_FONT_ID, lx, ty, lines[n].c_str(), true, EpdFontFamily::BOLD);
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

int PocketDeckTheme::headerLeftInset() const {
  using namespace PocketDeckMetrics;
  const auto& metrics = UITheme::getInstance().getMetrics();
  return metrics.headerSidePadding + kMarkSize + kMarkGap - metrics.headerSidePadding / 2;
}

void PocketDeckTheme::drawHeader(const GfxRenderer& renderer, const Rect rect, const char* title, const char* subtitle,
                                 const bool readerContext) const {
  using namespace PocketDeckMetrics;
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
  const int x = rect.x + kPad;
  const int w = rect.width - 2 * kPad;
  const int nowY = rect.y + 10;
  // Now Reading, then Recent Books, then Reading Stats.
  const int recentY = nowY + kNowH + kCardGap;
  const int recentH = kRecentH;
  const int statsY = recentY + recentH + kCardGap;

  // Everything here is static while Home is open: draw it once, snapshot it,
  // and let later renders restore the snapshot and add only the selection.
  if (!coverRendered) {
    if (!recentBooks.empty()) {
      drawNowReading(renderer, recentBooks[0], x, nowY, w, &LyraTheme::iconForName);
    } else {
      card(renderer, x, nowY, w, kNowH);
      drawEmptyRecents(renderer, Rect{x, nowY, w, kNowH});
    }
    drawReadingStats(renderer, globalStats, x, statsY, w, &LyraTheme::iconForName);
    drawRecentReads(renderer, recentBooks, x, recentY, w, recentH, &LyraTheme::iconForName);
    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  }

  // Selection and touch targets: the Now Reading card is book 0; each cover
  // in Recent Reads is its book.
  if (!recentBooks.empty()) {
    TouchRegistry::getInstance().add(Rect{x, nowY, w, kNowH}, 0, TouchRegistry::Cover);
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
    const int headY = recentY + 4 + (32 - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
    const int afterHead =
        headX + renderer.getTextWidth(UI_12_FONT_ID, tr(STR_MENU_RECENT_BOOKS), EpdFontFamily::BOLD);
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
    const int top = recentY + 40 + kRecentTopPad;
    renderer.drawRoundedRect(tileX + 2, top - 6, slot - 4, recentY + recentH - kInset + 4 - (top - 6), 3, 6, true);
  }
  if (selectorIndex >= 0 && selectorIndex < count) {
    // Heading names the selected book: "Recent Books – <title>".
    const char* heading = tr(STR_MENU_RECENT_BOOKS);
    const int headX = x + kInset + 34;
    const int headY = recentY + 4 + (32 - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
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
      renderer.drawRoundedRect(x, nowY, w, kNowH, 3, kRadius, true);
    }
    const int sx = x + kInset + selectorIndex * slot;
    const int top = recentY + 40 + kRecentTopPad;
    const int boxBottom = recentY + recentH - kInset + 4;
    renderer.drawRoundedRect(sx + 2, top - 6, slot - 4, boxBottom - (top - 6), 3, 6, true);
  }
}

void PocketDeckTheme::drawButtonMenu(GfxRenderer& renderer, const Rect rect, const int buttonCount,
                                     const int selectedIndex, const std::function<const char*(int index)>& buttonLabel,
                                     const std::function<UIIcon(int index)>& rowIcon) const {
  // Bottom bar: a tab bar under one hairline rule. Items share the width
  // equally with no frames, icon over label; the selected item sits on a
  // light-gray rounded pill.
  if (buttonCount <= 0) return;
  // Recent Books lives in the Recent Books card (View All Recents), not here.
  // Entries keep their menu index for touch and selection; only slots shift.
  const auto shownInBar = [&rowIcon](const int i) { return rowIcon == nullptr || rowIcon(i) != UIIcon::Recent; };
  int shownCount = 0;
  for (int i = 0; i < buttonCount; ++i) shownCount += shownInBar(i) ? 1 : 0;
  if (shownCount <= 0) return;
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
  renderer.drawLine(rect.x + kPad / 2, barTop - 6, rect.x + rect.width - kPad / 2 - 1, barTop - 6, true);
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
  // One label size for the whole bar: 10 pt if every label fits.
  int labelFont = UI_10_FONT_ID;
  for (int i = 0; i < buttonCount; ++i) {
    if (!shownInBar(i)) continue;
    if (renderer.getTextWidth(UI_10_FONT_ID, tabLabel(i)) > cellW - 12) labelFont = SMALL_FONT_ID;
  }
  const int labelH = renderer.getLineHeight(labelFont);
  // Selection pill: soft 4 px corners, inset so neighbouring pills never touch.
  constexpr int kPillInsetX = 8;
  constexpr int kPillInsetY = 2;
  constexpr int kPillRadius = 4;
  int slotIndex = 0;
  for (int i = 0; i < buttonCount; ++i) {
    if (!shownInBar(i)) continue;
    const int slotPos = slotIndex++;
    const int cx = barX + (slotPos % cols) * cellW;
    const int cy = barTop + (slotPos / cols) * rowH;
    const int ch = rowH - 4;
    TouchRegistry::getInstance().add(Rect{cx, cy, cellW, ch}, i, TouchRegistry::Item);
    const UIIcon icon = rowIcon != nullptr ? rowIcon(i) : UIIcon::File;
    if (i == selectedIndex) {
      renderer.fillRoundedRect(cx + kPillInsetX, cy + kPillInsetY, cellW - 2 * kPillInsetX, ch - 2 * kPillInsetY,
                               kPillRadius, Color::LightGray);
    }
    if (withIcons) {
      const freeink::Icon* bmp = icon == UIIcon::Tools ? &icon_layout_grid_32 : iconForName(icon, 32);
      if (bmp == nullptr) bmp = iconForName(UIIcon::Book, 32);
      if (bmp != nullptr) drawLucideIcon(renderer, *bmp, cx + (cellW - 32) / 2, cy + 8, true);
    }
    const char* label = tabLabel(i);
    const std::string text = renderer.truncatedText(labelFont, label ? label : "", cellW - 12);
    const int lw = renderer.getTextWidth(labelFont, text.c_str());
    const int ly = withIcons ? cy + ch - labelH - 6 : cy + (ch - labelH) / 2;
    renderer.drawText(labelFont, cx + (cellW - lw) / 2, ly, text.c_str(), true);
  }
}
