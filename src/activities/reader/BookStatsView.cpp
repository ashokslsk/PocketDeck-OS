#include "BookStatsView.h"

#include <FreeInkUICore.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "LibraryStats.h"
#include "MappedInputManager.h"
#include "ReadingDayLog.h"
#include "activities/tools/ToolsCommon.h"
#include "components/CompactHeader.h"
#include "components/TouchActionButtons.h"
#include "components/TouchHeaderBackButton.h"
#include "components/TouchRegistry.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "components/icons/listIcons.h"
#include "components/icons/readingStatsIcons.h"
#include "components/icons/toolsIcon.h"
#include "fontIds.h"

namespace {
constexpr int kStatsButtonHintTopGap = 10;
constexpr int kStandaloneNoRtcMaxTopCardHeightDivisor = 2;
constexpr int kStandaloneNoRtcMaxVerticalOffset = 32;
constexpr int kPerBookRtcTopCardMaxExtra = 84;

struct StatsLayout {
  int headerHeight;
  int headerDrawHeight;
  int topGap;
  int cardGap;
  int topCardTitleH;
  int topCardH;
  int globalCardH;
  int sectionTitleH;
  int sectionTitleFontId;
  int chartLabelFontId;
  int chartLabelW;
  int barH;
  int barGap;
  int chartTopPadding;
  int chartBottomPadding;
};

constexpr StatsLayout kDefaultLayout = {
    .headerHeight = 78,
    .headerDrawHeight = 67,
    .topGap = 8,
    .cardGap = 26,
    .topCardTitleH = 36,
    .topCardH = 214,
    .globalCardH = 154,
    .sectionTitleH = 34,
    .sectionTitleFontId = UI_10_FONT_ID,
    .chartLabelFontId = UI_10_FONT_ID,
    .chartLabelW = 88,
    .barH = 22,
    .barGap = 12,
    .chartTopPadding = 14,
    .chartBottomPadding = 14,
};

constexpr StatsLayout kCompactLayout = {
    .headerHeight = 67,
    .headerDrawHeight = 67,
    .topGap = 6,
    .cardGap = 8,
    .topCardTitleH = 30,
    .topCardH = 156,
    .globalCardH = 110,
    .sectionTitleH = 30,
    .sectionTitleFontId = UI_10_FONT_ID,
    .chartLabelFontId = SMALL_FONT_ID,
    .chartLabelW = 78,
    .barH = 16,
    .barGap = 8,
    .chartTopPadding = 8,
    .chartBottomPadding = 8,
};

constexpr std::array<StrId, READING_TIME_BUCKET_COUNT> TIME_BUCKET_LABELS = {
    StrId::STR_STATS_MORNING, StrId::STR_STATS_AFTERNOON, StrId::STR_STATS_EVENING, StrId::STR_STATS_NIGHT};
constexpr std::array<StrId, READING_DAY_OF_WEEK_COUNT> DAY_LABELS = {
    StrId::STR_STATS_MON, StrId::STR_STATS_TUE, StrId::STR_STATS_WED, StrId::STR_STATS_THU,
    StrId::STR_STATS_FRI, StrId::STR_STATS_SAT, StrId::STR_STATS_SUN};

const char* dayCountText(const uint16_t days) { return days == 1 ? tr(STR_STATS_DAY) : tr(STR_STATS_DAYS); }

int sectionCardHeight(const StatsLayout& layout, const int rowCount) {
  if (rowCount <= 0) {
    return layout.sectionTitleH + layout.chartTopPadding + layout.chartBottomPadding;
  }
  const int rowStride = layout.barH + layout.barGap;
  return layout.sectionTitleH + layout.chartTopPadding + layout.chartBottomPadding + layout.barH +
         (rowCount - 1) * rowStride;
}

bool shouldShowRtcBasedStats() { return halClock.isAvailable(); }

int noRtcCardBaseHeight(const StatsLayout& layout) { return layout.globalCardH; }

int statsHeaderHeight(const ThemeMetrics& metrics, const StatsLayout& layout, const MappedInputManager* mappedInput) {
  if (mappedInput && mappedInput->hasTouchHardware()) {
    return CompactHeader::height(metrics);
  }
  return std::min(metrics.headerHeight, layout.headerHeight);
}

int statsContentHeight(const StatsLayout& layout, const int headerHeight, const bool globalPage,
                       const bool showRtcStats) {
  const int topCardH = globalPage ? layout.globalCardH : layout.topCardH;
  if (!showRtcStats) {
    return headerHeight + layout.topGap + topCardH;
  }
  const int timeOfDayH = sectionCardHeight(layout, static_cast<int>(TIME_BUCKET_LABELS.size()));
  const int dayOfWeekH = sectionCardHeight(layout, static_cast<int>(DAY_LABELS.size()));
  return headerHeight + layout.topGap + topCardH + layout.cardGap + timeOfDayH + layout.cardGap + dayOfWeekH;
}

int noRtcCombinedContentHeight(const StatsLayout& layout, const int headerHeight, const bool showAllDevicesStats) {
  const int cardBaseH = noRtcCardBaseHeight(layout);
  return headerHeight + layout.topGap + cardBaseH + layout.cardGap + layout.globalCardH +
         (showAllDevicesStats ? layout.cardGap + layout.globalCardH : 0);
}

int statsBottomInset(const ThemeMetrics& metrics, const bool showButtonHints) {
  return metrics.verticalSpacing + (showButtonHints ? metrics.buttonHintsHeight + kStatsButtonHintTopGap : 0);
}

int perBookRtcTopCardHeight(const StatsLayout& layout, const int extraHeight) {
  return layout.topCardH + std::min(extraHeight, kPerBookRtcTopCardMaxExtra);
}

int globalRtcCardHeightForPerBookRowSpacing(const StatsLayout& layout, const int perBookExtraHeight) {
  constexpr int perBookDataRowCount = 3;
  constexpr int globalDataRowCount = 2;
  const int perBookDataRowH =
      (perBookRtcTopCardHeight(layout, perBookExtraHeight) - layout.topCardTitleH) / perBookDataRowCount;
  return std::max(layout.globalCardH, layout.topCardTitleH + perBookDataRowH * globalDataRowCount);
}

const StatsLayout& getStatsLayout(const GfxRenderer& renderer, const MappedInputManager* mappedInput,
                                  const bool globalPage, const bool showButtonHints, const bool showRtcStats) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int availableHeight =
      renderer.getScreenHeight() - metrics.topPadding - statsBottomInset(metrics, showButtonHints);
  const int defaultHeaderHeight = statsHeaderHeight(metrics, kDefaultLayout, mappedInput);
  const bool defaultFitsCurrentPage =
      statsContentHeight(kDefaultLayout, defaultHeaderHeight, globalPage, showRtcStats) <= availableHeight;
  const bool defaultMatchesPerBookCharts =
      !globalPage || !showRtcStats ||
      statsContentHeight(kDefaultLayout, defaultHeaderHeight, false, showRtcStats) <= availableHeight;
  if (defaultFitsCurrentPage && defaultMatchesPerBookCharts) {
    return kDefaultLayout;
  }
  return kCompactLayout;
}

const StatsLayout& getNoRtcCombinedLayout(const GfxRenderer& renderer, const MappedInputManager* mappedInput,
                                          const bool showButtonHints, const bool showAllDevicesStats) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int availableHeight =
      renderer.getScreenHeight() - metrics.topPadding - statsBottomInset(metrics, showButtonHints);
  if (noRtcCombinedContentHeight(kDefaultLayout, statsHeaderHeight(metrics, kDefaultLayout, mappedInput),
                                 showAllDevicesStats) <= availableHeight) {
    return kDefaultLayout;
  }
  return kCompactLayout;
}

bool fallbackEstimatedTimeLeft(const BookReadingStats& stats, const float progressPercent, uint32_t& seconds) {
  seconds = 0;
  if (progressPercent <= 0.0f || progressPercent >= 100.0f || stats.totalReadingSeconds < 120) {
    return false;
  }

  const float progress = progressPercent / 100.0f;
  const float estimate = (static_cast<float>(stats.totalReadingSeconds) * (1.0f - progress)) / progress;
  if (estimate <= 0.0f) {
    return false;
  }
  seconds = static_cast<uint32_t>(estimate + 0.5f);
  return seconds > 0;
}

bool cachedEstimatedTimeLeft(const BookReadingStats& stats, uint32_t& seconds) {
  seconds = stats.estimatedTimeLeftSeconds;
  return seconds > 0;
}

bool estimateFinishDateFromDailyPace(const BookReadingStats& stats, const ReadingStatsDateTime& today,
                                     const uint32_t estimatedReadingSeconds, ReadingStatsDate& outDate) {
  outDate = {};
  if (!today.isValid() || !stats.startDate.isValid() || estimatedReadingSeconds == 0 ||
      stats.totalReadingSeconds == 0) {
    return false;
  }

  const uint16_t elapsedDays = readingSpanDaysElapsed(stats.startDate, today.date);
  const uint16_t readingDays = std::max<uint16_t>(1, elapsedDays);

  // Convert remaining reading time into calendar time using the book's average reading seconds per calendar day.
  const uint64_t estimatedCalendarSeconds =
      (static_cast<uint64_t>(estimatedReadingSeconds) * static_cast<uint64_t>(readingDays) * 86400ULL +
       static_cast<uint64_t>(stats.totalReadingSeconds) / 2ULL) /
      static_cast<uint64_t>(stats.totalReadingSeconds);
  if (estimatedCalendarSeconds == 0) {
    return false;
  }

  ReadingStatsDateTime estimatedFinish = today;
  addSecondsToReadingStatsDateTime(estimatedFinish,
                                   static_cast<uint32_t>(std::min<uint64_t>(estimatedCalendarSeconds, UINT32_MAX)));
  outDate = estimatedFinish.date;
  return outDate.isValid();
}

float pagesPerMinute(const uint32_t totalPagesTurned, const uint32_t totalReadingSeconds) {
  if (totalReadingSeconds <= 60) {
    return 0.0f;
  }
  return static_cast<float>(totalPagesTurned) * 60.0f / static_cast<float>(totalReadingSeconds);
}

void drawCenteredLabel(const GfxRenderer& renderer, const int fontId, const int x, const int w, const int y,
                       const char* text, const bool bold = false) {
  const int textWidth = renderer.getTextWidth(fontId, text, bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  renderer.drawText(fontId, x + (w - textWidth) / 2, y, text, true,
                    bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
}

void drawStatCell(const GfxRenderer& renderer, const int x, const int w, const int y, const int h, const char* value,
                  const char* label) {
  const int valueLineH = renderer.getLineHeight(UI_12_FONT_ID);
  const int labelLineH = renderer.getLineHeight(SMALL_FONT_ID);
  const int totalTextH = valueLineH + 4 + labelLineH;
  const int textY = y + (h - totalTextH) / 2;
  drawCenteredLabel(renderer, UI_12_FONT_ID, x, w, textY, value, true);
  drawCenteredLabel(renderer, SMALL_FONT_ID, x, w, textY + valueLineH + 4, label);
}

void drawSectionCard(const GfxRenderer& renderer, const int x, const int y, const int w, const int h, const char* title,
                     const StatsLayout& layout) {
  renderer.drawRect(x, y, w, h);
  renderer.drawLine(x, y + layout.sectionTitleH, x + w, y + layout.sectionTitleH);
  drawCenteredLabel(renderer, layout.sectionTitleFontId, x, w,
                    y + (layout.sectionTitleH - renderer.getLineHeight(layout.sectionTitleFontId)) / 2, title, true);
}

void formatBucketMinutes(const uint32_t seconds, char* buf, const size_t len) {
  const uint32_t minutes = seconds / 60;
  if (minutes >= 60) {
    snprintf(buf, len, "%luh %02lum", static_cast<unsigned long>(minutes / 60),
             static_cast<unsigned long>(minutes % 60));
  } else if (minutes == 0 && seconds > 0) {
    snprintf(buf, len, "<1m");
  } else {
    snprintf(buf, len, "%lum", static_cast<unsigned long>(minutes));
  }
}

// PocketDeck-OS: time of day as a 24-hour dial. Each reading period is an arc
// (Morning 5-12, Afternoon 12-17, Evening 17-21, Night 21-5) whose thickness
// follows its share; the busiest period is solid black, the others gray. A
// legend with times and percentages sits to the right.
void drawTimeOfDayDial(GfxRenderer& renderer, const int x, const int y, const int w, const int h,
                       const std::array<uint32_t, READING_TIME_BUCKET_COUNT>& values, const StatsLayout& layout) {
  const int top = y + layout.sectionTitleH;
  const int bodyH = h - layout.sectionTitleH;
  const int ro = std::max(20, std::min(bodyH - 16, w * 2 / 5) / 2);
  const int cx = x + 14 + ro;
  const int cy = top + bodyH / 2;
  uint32_t total = 0;
  uint32_t maxValue = 0;
  size_t best = 0;
  for (size_t i = 0; i < values.size(); ++i) {
    total += values[i];
    if (values[i] > maxValue) {
      maxValue = values[i];
      best = i;
    }
  }
  const int maxThick = std::max(6, ro / 3);
  int thick[READING_TIME_BUCKET_COUNT];
  for (size_t i = 0; i < values.size(); ++i) {
    thick[i] = maxValue == 0 || values[i] == 0
                   ? 0
                   : std::max(3, static_cast<int>((static_cast<uint64_t>(maxThick) * values[i]) / maxValue));
  }
  // Hour (0-23) to period, matching ReadingStatsUtils.
  const auto bucketOf = [](const int hour) { return hour < 5 ? 3 : hour < 12 ? 0 : hour < 17 ? 1 : hour < 21 ? 2 : 3; };
  const int ro2 = ro * ro;
  const int rin = ro - maxThick;
  for (int dy = -ro; dy <= ro; ++dy) {
    for (int dx = -ro; dx <= ro; ++dx) {
      const int d2 = dx * dx + dy * dy;
      if (d2 > ro2 || d2 < (rin - 1) * (rin - 1)) continue;
      // Clockwise from 12 o'clock, 24 hours around.
      float a = atan2f(static_cast<float>(dx), static_cast<float>(-dy));
      if (a < 0) a += 2.0f * static_cast<float>(M_PI);
      const int hour = std::min(23, static_cast<int>(a * 24.0f / (2.0f * static_cast<float>(M_PI))));
      const int b = bucketOf(hour);
      const int r = static_cast<int>(sqrtf(static_cast<float>(d2)));
      if (thick[b] > 0 && r > ro - thick[b]) {
        if (static_cast<size_t>(b) == best || ((cx + dx + cy + dy) & 1) == 0)
          renderer.drawPixel(cx + dx, cy + dy, true);
      } else if (r >= ro - 1 || r == rin - 1) {
        renderer.drawPixel(cx + dx, cy + dy, true);  // track outline
      }
    }
  }
  // Centre: the period read most, and its share.
  const int smallH = renderer.getLineHeight(SMALL_FONT_ID);
  if (maxValue > 0) {
    const int nameH = renderer.getLineHeight(UI_10_FONT_ID);
    const int pct = static_cast<int>((static_cast<uint64_t>(maxValue) * 100 + total / 2) / total);
    char share[8];
    snprintf(share, sizeof(share), "%d%%", pct);
    const int innerW = (rin - 3) * 2;
    const char* bestName = I18N.get(TIME_BUCKET_LABELS[best]);
    const int nameFont =
        renderer.getTextWidth(UI_10_FONT_ID, bestName, EpdFontFamily::BOLD) <= innerW ? UI_10_FONT_ID : SMALL_FONT_ID;
    const std::string name = renderer.truncatedText(nameFont, bestName, innerW, EpdFontFamily::BOLD);
    drawCenteredLabel(renderer, nameFont, cx - innerW / 2, innerW, cy - (nameH + smallH) / 2, name.c_str(), true);
    drawCenteredLabel(renderer, SMALL_FONT_ID, cx - innerW / 2, innerW, cy - (nameH + smallH) / 2 + nameH, share);
  }

  // Legend: period, time, share, with a mini bar.
  const int lx = cx + ro + 18;
  const int lw = x + w - 12 - lx;
  const int nameH = renderer.getLineHeight(UI_10_FONT_ID);
  const int rowH = std::max(nameH, (bodyH - 8) / static_cast<int>(values.size()));
  const bool showBars = rowH >= nameH + 7;
  int ly = top + std::max(0, (bodyH - rowH * static_cast<int>(values.size())) / 2);
  for (size_t i = 0; i < values.size(); ++i) {
    const bool isBest = maxValue > 0 && i == best;
    const EpdFontFamily::Style st = isBest ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    renderer.drawText(UI_10_FONT_ID, lx, ly, I18N.get(TIME_BUCKET_LABELS[i]), true, st);
    char value[24];
    char time[16];
    formatBucketMinutes(values[i], time, sizeof(time));
    const int pct = total > 0 ? static_cast<int>((static_cast<uint64_t>(values[i]) * 100 + total / 2) / total) : 0;
    snprintf(value, sizeof(value), "%s  %d%%", time, pct);
    const int vw = renderer.getTextWidth(UI_10_FONT_ID, value, st);
    renderer.drawText(UI_10_FONT_ID, lx + lw - vw, ly, value, true, st);
    const int barY = ly + nameH + 1;
    if (showBars) renderer.fillRectDither(lx, barY, lw, 5, Color::LightGray);
    if (showBars && total > 0 && values[i] > 0) {
      renderer.fillRect(lx, barY, std::max(2, static_cast<int>((static_cast<uint64_t>(lw) * values[i]) / total)), 5,
                        true);
    }
    ly += rowH;
  }
}

// PocketDeck-OS: seven day columns. The best day is solid black, the others
// gray, with the time above each column and a dashed line at the average.
void drawDayColumns(GfxRenderer& renderer, const int x, const int top, const int w, const int colAreaH,
                    const std::array<uint32_t, READING_DAY_OF_WEEK_COUNT>& values) {
  const int labelH = renderer.getLineHeight(SMALL_FONT_ID);
  const int innerW = w - 24;
  const int left = x + 12;
  const int barTop = top + labelH + 2;
  const int barH = std::max(10, colAreaH - labelH - 2);
  uint32_t maxValue = 0;
  uint32_t total = 0;
  int readDays = 0;
  for (const uint32_t v : values) {
    maxValue = std::max(maxValue, v);
    total += v;
    if (v > 0) ++readDays;
  }
  const int slot = innerW / static_cast<int>(DAY_LABELS.size());
  const int colW = std::max(6, slot * 3 / 5);
  // Dashed average line first, so bars and their values sit on top of it.
  if (readDays > 1 && maxValue > 0) {
    const uint32_t avg = total / static_cast<uint32_t>(readDays);
    const int ay = barTop + barH - static_cast<int>((static_cast<uint64_t>(barH) * avg) / maxValue);
    for (int dx = 0; dx < innerW; dx += 8) renderer.drawLine(left + dx, ay, left + std::min(dx + 3, innerW), ay, true);
  }
  for (size_t i = 0; i < DAY_LABELS.size(); ++i) {
    const int sx = left + static_cast<int>(i) * slot;
    const int barX = sx + (slot - colW) / 2;
    const bool isBest = maxValue > 0 && values[i] == maxValue;
    renderer.drawLine(barX, barTop + barH, barX + colW - 1, barTop + barH, true);
    if (maxValue > 0 && values[i] > 0) {
      const int fillH = std::max(3, static_cast<int>((static_cast<uint64_t>(barH) * values[i]) / maxValue));
      if (isBest) {
        renderer.fillRect(barX, barTop + barH - fillH, colW, fillH, true);
      } else {
        renderer.fillRectDither(barX, barTop + barH - fillH, colW, fillH, Color::DarkGray);
        renderer.drawRect(barX, barTop + barH - fillH, colW, fillH, true);
      }
      char value[16];
      formatBucketMinutes(values[i], value, sizeof(value));
      const int vw = renderer.getTextWidth(SMALL_FONT_ID, value, isBest ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
      const int vy = barTop + barH - fillH - labelH - 1;
      renderer.fillRect(sx + (slot - vw) / 2 - 1, vy, vw + 2, labelH, false);
      drawCenteredLabel(renderer, SMALL_FONT_ID, sx, slot, vy, value, isBest);
    }
    drawCenteredLabel(renderer, SMALL_FONT_ID, sx, slot, barTop + barH + 3, I18N.get(DAY_LABELS[i]), isBest);
  }
}

// PocketDeck-OS: "Day of week" as seven columns, with a 12-week reading
// calendar beneath (one square per day, filled when you read that day).
void drawWeekdayAndCalendar(GfxRenderer& renderer, const int x, const int y, const int w, const int h,
                            const GlobalReadingStats& stats, const StatsLayout& layout) {
  const int labelH = renderer.getLineHeight(SMALL_FONT_ID);
  const int top = y + layout.sectionTitleH + 8;
  const int innerW = w - 24;
  const int left = x + 12;
  // Columns take roughly 40% of the card, the calendar the rest.
  const int colAreaH = std::max(40, (h - layout.sectionTitleH) * 2 / 5 - labelH);
  drawDayColumns(renderer, x, top, w, colAreaH, stats.dayOfWeekSeconds);

  // Calendar: 7 rows (Mon..Sun) x up to 12 week columns, today bottom-right.
  ReadingStatsDateTime now;
  if (!getCurrentLocalReadingStatsDateTime(now)) return;
  const int calTop = top + colAreaH + labelH + 12;
  const int calH = y + h - 10 - calTop - labelH;
  if (calH < 7 * 5) return;
  const int cell = std::min(calH / 7, innerW / 13);
  const int gap = cell >= 10 ? 2 : 1;
  const int weeks = std::min(12, innerW / cell - 1);
  const uint32_t todayIndex = readingStatsDayIndex(now.date);
  // Weekday of today, Monday = 0 (day index 0 is Saturday 2000-01-01).
  const int todayWd = static_cast<int>((todayIndex + 5) % 7);
  const int calW = weeks * cell;
  const int calX = left + (innerW - calW) / 2;
  int readDays = 0;
  for (int wk = 0; wk < weeks; ++wk) {
    for (int wd = 0; wd < 7; ++wd) {
      const int back = (weeks - 1 - wk) * 7 + (todayWd - wd);
      if (back < 0) continue;  // later this week
      const uint32_t day = todayIndex - static_cast<uint32_t>(back);
      const int cx = calX + wk * cell;
      const int cy = calTop + wd * cell;
      if (readingHistoryHasDay(stats.readingHistoryAnchorDay, stats.readingHistoryBits, day)) {
        renderer.fillRect(cx, cy, cell - gap, cell - gap, true);
        ++readDays;
      } else {
        renderer.drawRect(cx, cy, cell - gap, cell - gap, true);
      }
    }
  }
  char caption[48];
  snprintf(caption, sizeof(caption), "%s: %d %s", tr(STR_STATS_LAST_12_WEEKS), readDays, dayCountText(readDays));
  drawCenteredLabel(renderer, SMALL_FONT_ID, x, w, calTop + 7 * cell + 2, caption);
}

// PocketDeck-OS: the book card leads with a progress ring, six stats beside
// it, and (with a clock) a start-to-finish timeline along the bottom.
void drawPerBookStatsCard(GfxRenderer& renderer, const int x, const int y, const int w, const int h,
                          const std::string& bookTitle, const BookReadingStats& stats, const float progressPercent,
                          const bool hasEstimatedTimeLeft, const uint32_t estimatedTimeLeftSeconds,
                          const StatsLayout& layout) {
  renderer.drawRect(x, y, w, h);
  renderer.drawLine(x, y + layout.topCardTitleH, x + w, y + layout.topCardTitleH);
  const std::string visibleTitle =
      renderer.truncatedText(UI_10_FONT_ID, bookTitle.c_str(), w - 20, EpdFontFamily::BOLD);
  drawCenteredLabel(renderer, UI_10_FONT_ID, x, w,
                    y + (layout.topCardTitleH - renderer.getLineHeight(UI_10_FONT_ID)) / 2, visibleTitle.c_str(), true);

  const bool showRtcStats = shouldShowRtcBasedStats();
  const int smallH = renderer.getLineHeight(SMALL_FONT_ID);
  const int timelineH = showRtcStats ? smallH * 2 + 20 : 0;
  const int bodyTop = y + layout.topCardTitleH;
  const int bodyH = h - layout.topCardTitleH - timelineH;
  char buf[40];

  // Progress ring.
  const int ringD = std::max(40, std::min(bodyH - 16, w * 9 / 25));
  const int ringR = ringD / 2;
  const int ringCx = x + 12 + ringR;
  const int ringCy = bodyTop + bodyH / 2;
  const float fraction = progressPercent >= 0.0f ? progressPercent / 100.0f : 0.0f;
  tools::drawProgressRing(renderer, ringCx, ringCy, ringR, std::max(6, ringR / 5), fraction);
  if (progressPercent >= 0.0f) {
    snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(progressPercent + 0.5f));
  } else {
    snprintf(buf, sizeof(buf), "-");
  }
  const int valueH = renderer.getLineHeight(UI_12_FONT_ID);
  drawCenteredLabel(renderer, UI_12_FONT_ID, ringCx - ringR, ringD, ringCy - (valueH + smallH) / 2, buf, true);
  drawCenteredLabel(renderer, SMALL_FONT_ID, ringCx - ringR, ringD, ringCy - (valueH + smallH) / 2 + valueH,
                    stats.isCompleted ? tr(STR_STATS_FINISHED_DATE) : tr(STR_STATS_PROGRESS_LBL));

  // Stats grid: two columns, three rows.
  const int gridX = ringCx + ringR + 10;
  const int gridW = x + w - 6 - gridX;
  const int colW = gridW / 2;
  const int rowH = bodyH / 3;

  snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(stats.sessionCount));
  drawStatCell(renderer, gridX, colW, bodyTop, rowH, buf, tr(STR_STATS_SESSIONS_LBL));
  BookReadingStats::formatDuration(stats.totalReadingSeconds, buf, sizeof(buf));
  drawStatCell(renderer, gridX + colW, colW, bodyTop, rowH, buf, tr(STR_STATS_TIME_LBL));

  const uint32_t avgSecs = stats.sessionCount > 0 ? stats.totalReadingSeconds / stats.sessionCount : 0;
  BookReadingStats::formatDuration(avgSecs, buf, sizeof(buf));
  drawStatCell(renderer, gridX, colW, bodyTop + rowH, rowH, buf, tr(STR_STATS_AVG_SESSION_LBL));

  uint32_t fallbackEstimateSeconds = 0;
  uint32_t cachedEstimateSeconds = 0;
  const bool hasCachedEstimate = cachedEstimatedTimeLeft(stats, cachedEstimateSeconds);
  const bool hasFallbackEstimate = fallbackEstimatedTimeLeft(stats, progressPercent, fallbackEstimateSeconds);
  if (!stats.isCompleted && (hasEstimatedTimeLeft || hasCachedEstimate || hasFallbackEstimate)) {
    formatCompactReadingDuration(hasEstimatedTimeLeft ? estimatedTimeLeftSeconds
                                 : hasCachedEstimate  ? cachedEstimateSeconds
                                                      : fallbackEstimateSeconds,
                                 buf, sizeof(buf));
  } else {
    snprintf(buf, sizeof(buf), "-");
  }
  drawStatCell(renderer, gridX + colW, colW, bodyTop + rowH, rowH, buf, tr(STR_TIME_LEFT));

  snprintf(buf, sizeof(buf), "%.1f", pagesPerMinute(stats.totalPagesTurned, stats.totalReadingSeconds));
  drawStatCell(renderer, gridX, colW, bodyTop + rowH * 2, rowH, buf, tr(STR_STATS_PAGES_PER_MIN));

  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(stats.totalPagesTurned));
  drawStatCell(renderer, gridX + colW, colW, bodyTop + rowH * 2, rowH, buf, tr(STR_STATS_PAGES_LBL));

  if (!showRtcStats) {
    return;
  }

  ReadingStatsDateTime today;
  const bool hasToday = getCurrentLocalReadingStatsDateTime(today);
  const ReadingStatsDate endDate = stats.isCompleted && stats.finishedDate.isValid()
                                       ? stats.finishedDate
                                       : (hasToday ? today.date : ReadingStatsDate{});
  const bool hasDaySpan = stats.startDate.isValid() && endDate.isValid();
  const uint16_t daysReading = hasDaySpan ? readingSpanDaysElapsed(stats.startDate, endDate) : 0;

  ReadingStatsDate finishDisplayDate;
  const bool finished = stats.isCompleted;
  if (finished) {
    finishDisplayDate = stats.finishedDate;
  } else if (hasToday && (hasEstimatedTimeLeft || hasCachedEstimate || hasFallbackEstimate)) {
    const uint32_t remainingReadingSeconds = hasEstimatedTimeLeft ? estimatedTimeLeftSeconds
                                             : hasCachedEstimate  ? cachedEstimateSeconds
                                                                  : fallbackEstimateSeconds;
    if (!estimateFinishDateFromDailyPace(stats, today, remainingReadingSeconds, finishDisplayDate)) {
      ReadingStatsDateTime estimatedFinish = today;
      addSecondsToReadingStatsDateTime(estimatedFinish, remainingReadingSeconds);
      finishDisplayDate = estimatedFinish.date;
    }
  }

  // Timeline: started ●━━━━━━○ finish, filled up to the current progress.
  const int ty = y + h - timelineH;
  renderer.drawLine(x + 8, ty, x + w - 8, ty, true);
  const int startedY = ty;
  TouchRegistry::getInstance().add(Rect(x, startedY, w / 2, timelineH), BookStatsTouchTarget::StartedDaysStat,
                                   TouchRegistry::Item);
  char dateBuf[24];
  char label[48];
  formatReadingStatsShortDate(stats.startDate, dateBuf, sizeof(dateBuf));
  snprintf(label, sizeof(label), "%s %s", tr(STR_STATS_STARTED), dateBuf);
  const int lineY = ty + 10 + smallH + 6;
  renderer.drawText(SMALL_FONT_ID, x + 14, ty + 8, label, true, EpdFontFamily::BOLD);
  formatReadingStatsShortDate(finishDisplayDate, dateBuf, sizeof(dateBuf));
  snprintf(label, sizeof(label), "%s %s", finished ? tr(STR_STATS_FINISHED_DATE) : tr(STR_STATS_EST_FINISH_DATE),
           dateBuf);
  const int fw = renderer.getTextWidth(SMALL_FONT_ID, label, EpdFontFamily::BOLD);
  renderer.drawText(SMALL_FONT_ID, x + w - 14 - fw, ty + 8, label, true, EpdFontFamily::BOLD);
  if (hasDaySpan) {
    snprintf(label, sizeof(label), "%u %s", static_cast<unsigned>(daysReading), dayCountText(daysReading));
    drawCenteredLabel(renderer, SMALL_FONT_ID, x, w, ty + 8, label);
  }
  const int lx0 = x + 20;
  const int lx1 = x + w - 20;
  const int fillX = lx0 + static_cast<int>(static_cast<float>(lx1 - lx0) * std::min(1.0f, std::max(0.0f, fraction)));
  renderer.fillRectDither(lx0, lineY - 2, lx1 - lx0, 5, Color::LightGray);
  renderer.fillRect(lx0, lineY - 2, fillX - lx0, 5, true);
  renderer.fillRect(lx0 - 5, lineY - 5, 11, 11, true);  // start dot
  renderer.fillRect(lx1 - 5, lineY - 5, 11, 11, finished);
  renderer.drawRect(lx1 - 5, lineY - 5, 11, 11, true);   // finish dot
  renderer.fillRect(fillX - 2, lineY - 8, 5, 17, true);  // "you are here"
}

void drawGlobalStatsCard(GfxRenderer& renderer, const int x, const int y, const int w, const int h, const char* title,
                         const GlobalReadingStats& stats, const StatsLayout& layout) {
  renderer.drawRect(x, y, w, h);
  renderer.drawLine(x, y + layout.topCardTitleH, x + w, y + layout.topCardTitleH);
  const bool showRtcStats = shouldShowRtcBasedStats();
  drawCenteredLabel(renderer, UI_10_FONT_ID, x, w,
                    y + (layout.topCardTitleH - renderer.getLineHeight(UI_10_FONT_ID)) / 2, title, true);

  const int thirdW = w / 3;
  const int halfW = w / 2;
  const int rowH = (h - layout.topCardTitleH) / 2;
  char buf[40];

  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(stats.totalSessions));
  drawStatCell(renderer, x, thirdW, y + layout.topCardTitleH, rowH, buf, tr(STR_STATS_SESSIONS_LBL));

  BookReadingStats::formatDuration(stats.totalReadingSeconds, buf, sizeof(buf));
  drawStatCell(renderer, x + thirdW, thirdW, y + layout.topCardTitleH, rowH, buf, tr(STR_STATS_TIME_LBL));

  snprintf(buf, sizeof(buf), "%.1f", pagesPerMinute(stats.totalPagesTurned, stats.totalReadingSeconds));
  drawStatCell(renderer, x + thirdW * 2, thirdW, y + layout.topCardTitleH, rowH, buf, tr(STR_STATS_PAGES_PER_MIN));

  const uint32_t avgSecs = stats.totalSessions > 0 ? stats.totalReadingSeconds / stats.totalSessions : 0;
  BookReadingStats::formatDuration(avgSecs, buf, sizeof(buf));
  if (showRtcStats) {
    drawStatCell(renderer, x, thirdW, y + layout.topCardTitleH + rowH, rowH, buf, tr(STR_STATS_AVG_SESSION_LBL));
  } else {
    drawStatCell(renderer, x, halfW, y + layout.topCardTitleH + rowH, rowH, buf, tr(STR_STATS_AVG_SESSION_LBL));
  }

  if (showRtcStats) {
    ReadingStatsDateTime today;
    const bool hasToday = getCurrentLocalReadingStatsDateTime(today);
    const uint16_t currentStreak = hasToday ? stats.currentReadingStreak(&today.date) : 0;
    if (currentStreak > 0) {
      snprintf(buf, sizeof(buf), "%u %s", static_cast<unsigned>(currentStreak), dayCountText(currentStreak));
    } else {
      snprintf(buf, sizeof(buf), "-");
    }
    drawStatCell(renderer, x + thirdW, thirdW, y + layout.topCardTitleH + rowH, rowH, buf,
                 tr(STR_STATS_READING_STREAK_LBL));
  }

  if (stats.completedBooks > 0) {
    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(stats.completedBooks));
  } else {
    snprintf(buf, sizeof(buf), "-");
  }
  drawStatCell(renderer, showRtcStats ? x + thirdW * 2 : x + halfW, showRtcStats ? thirdW : halfW,
               y + layout.topCardTitleH + rowH, rowH, buf, tr(STR_STATS_COMPLETED_LBL));
}

void drawDateField(const GfxRenderer& renderer, const int x, const int y, const int w, const char* text,
                   const bool selected, const int touchTarget = -1) {
  const int h = renderer.getLineHeight(UI_12_FONT_ID) + 10;
  if (touchTarget >= 0) {
    TouchRegistry::getInstance().add(Rect(x, y, w, h), touchTarget, TouchRegistry::Item);
  }
  renderer.fillRectDither(x, y, w, h, selected ? Color::LightGray : Color::White);
  renderer.drawRect(x, y, w, h, true);
  if (selected) {
    renderer.drawRect(x + 1, y + 1, w - 2, h - 2, true);
  }
  drawCenteredLabel(renderer, UI_12_FONT_ID, x, w, y + 5, text);
}

void drawDateAdjustButton(const GfxRenderer& renderer, const int x, const int y, const int size,
                          const freeink::Icon& icon, const int touchTarget) {
  TouchRegistry::getInstance().add(Rect(x, y, size, size), touchTarget, TouchRegistry::Item);
  renderer.drawRect(x, y, size, size, true);
  const freeink::ui::BitmapRef bitmap{icon.bits, icon.w, icon.h, freeink::ui::BitmapFormat::Mask1, true};
  freeink::ui::forEachBitmapPixel(
      freeink::ui::Rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(size),
                        static_cast<int16_t>(size)},
      bitmap, freeink::ui::BitmapMode::Center,
      [&renderer](const int16_t px, const int16_t py) { renderer.drawPixel(px, py, true); });
}
}  // namespace

void renderPerBookStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const std::string& bookTitle,
                            const BookReadingStats& stats, const float progressPercent, const bool hasEstimatedTimeLeft,
                            const uint32_t estimatedTimeLeftSeconds, const bool showButtonHints,
                            const bool showEditButton, const bool showMoreButton) {
  renderer.clearScreen();
  const bool showRtcStats = shouldShowRtcBasedStats();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& layout = getStatsLayout(renderer, mappedInput, false, showButtonHints, showRtcStats);
  if (mappedInput && mappedInput->hasTouchHardware()) {
    TouchHeaderBackButton::drawCompact(renderer, tr(STR_READING_STATS), false, true);
  } else {
    CompactHeader::drawTitle(renderer, tr(STR_READING_STATS), true);
  }
  const int screenW = renderer.getScreenWidth();
  const int cardX = metrics.contentSidePadding;
  const int cardW = screenW - metrics.contentSidePadding * 2;
  const int availableHeight =
      renderer.getScreenHeight() - metrics.topPadding - statsBottomInset(metrics, showButtonHints);
  int topCardH = layout.topCardH;
  const int headerHeight = statsHeaderHeight(metrics, layout, mappedInput);
  int y = metrics.topPadding + headerHeight + layout.topGap;

  if (showRtcStats) {
    const int timeOfDayH = sectionCardHeight(layout, static_cast<int>(TIME_BUCKET_LABELS.size()));
    const int dayOfWeekH = sectionCardHeight(layout, static_cast<int>(DAY_LABELS.size()));
    const int compactContentHeight =
        headerHeight + layout.topGap + layout.topCardH + layout.cardGap + timeOfDayH + layout.cardGap + dayOfWeekH;
    const int extraHeight = std::max(0, availableHeight - compactContentHeight);
    const int extraTopCardHeight = std::min(extraHeight, kPerBookRtcTopCardMaxExtra);
    const int remainingExtraHeight = extraHeight - extraTopCardHeight;
    const int timeOfDayExtraHeight = (remainingExtraHeight * 4) / 11;
    const int dayOfWeekExtraHeight = remainingExtraHeight - timeOfDayExtraHeight;
    const int timeOfDayCardH = timeOfDayH + timeOfDayExtraHeight;
    int dayOfWeekCardH = dayOfWeekH + dayOfWeekExtraHeight;
    topCardH += extraTopCardHeight;
    // The ring card needs three full stat rows plus its timeline; take the
    // difference from the day-of-week columns, which stretch freely.
    const int cellH = renderer.getLineHeight(UI_12_FONT_ID) + renderer.getLineHeight(SMALL_FONT_ID) + 10;
    const int minTopCardH = layout.topCardTitleH + cellH * 3 + renderer.getLineHeight(SMALL_FONT_ID) * 2 + 20;
    if (topCardH < minTopCardH) {
      dayOfWeekCardH -= minTopCardH - topCardH;
      topCardH = minTopCardH;
    }

    drawPerBookStatsCard(renderer, cardX, y, cardW, topCardH, bookTitle, stats, progressPercent, hasEstimatedTimeLeft,
                         estimatedTimeLeftSeconds, layout);
    y += topCardH + layout.cardGap;

    drawSectionCard(renderer, cardX, y, cardW, timeOfDayCardH, tr(STR_STATS_TIME_OF_DAY), layout);
    drawTimeOfDayDial(renderer, cardX, y, cardW, timeOfDayCardH, stats.timeOfDaySeconds, layout);
    y += timeOfDayCardH + layout.cardGap;

    drawSectionCard(renderer, cardX, y, cardW, dayOfWeekCardH, tr(STR_STATS_DAY_OF_WEEK), layout);
    drawDayColumns(renderer, cardX, y + layout.sectionTitleH + 8, cardW,
                   dayOfWeekCardH - layout.sectionTitleH - 16 - renderer.getLineHeight(SMALL_FONT_ID),
                   stats.dayOfWeekSeconds);
  } else {
    const int compactContentHeight = headerHeight + layout.topGap + layout.topCardH;
    const int extraHeight = std::max(0, availableHeight - compactContentHeight);
    if (showButtonHints) {
      topCardH += extraHeight;
    } else {
      // The sleep-screen variant has no footer controls, so on tall portrait displays the
      // single card can balloon and create huge internal gaps between the two stat rows.
      // Cap the card growth and spend the rest as outer margin instead.
      const int maxStandaloneCardHeight =
          std::max(layout.topCardH, renderer.getScreenHeight() / kStandaloneNoRtcMaxTopCardHeightDivisor);
      topCardH = std::min(layout.topCardH + extraHeight, maxStandaloneCardHeight);
      const int unusedExtraHeight = extraHeight - (topCardH - layout.topCardH);
      y += std::min(unusedExtraHeight / 3, kStandaloneNoRtcMaxVerticalOffset);
    }
    drawPerBookStatsCard(renderer, cardX, y, cardW, topCardH, bookTitle, stats, progressPercent, hasEstimatedTimeLeft,
                         estimatedTimeLeftSeconds, layout);
  }

  if (showButtonHints && mappedInput) {
    const auto labels =
        mappedInput->mapLabels(mappedInput->withBackArrow(tr(STR_BACK)), showEditButton ? tr(STR_EDIT) : "", "",
                               showMoreButton ? tr(STR_MORE) : "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, true);
  }
}

void renderGlobalStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const char* screenTitle,
                           const GlobalReadingStats& stats, const bool showButtonHints, const bool showMoreButton) {
  renderer.clearScreen();
  const bool showRtcStats = shouldShowRtcBasedStats();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& layout = getStatsLayout(renderer, mappedInput, true, showButtonHints, showRtcStats);
  if (mappedInput && mappedInput->hasTouchHardware()) {
    TouchHeaderBackButton::drawCompact(renderer, screenTitle, false);
  } else {
    CompactHeader::drawTitle(renderer, screenTitle);
  }
  const int screenW = renderer.getScreenWidth();
  const int cardX = metrics.contentSidePadding;
  const int cardW = screenW - metrics.contentSidePadding * 2;
  const int availableHeight =
      renderer.getScreenHeight() - metrics.topPadding - statsBottomInset(metrics, showButtonHints);
  int globalCardH = layout.globalCardH;
  const int headerHeight = statsHeaderHeight(metrics, layout, mappedInput);
  int y = metrics.topPadding + headerHeight + layout.topGap;

  if (showRtcStats) {
    const int timeOfDayH = sectionCardHeight(layout, static_cast<int>(TIME_BUCKET_LABELS.size()));
    const int dayOfWeekH = sectionCardHeight(layout, static_cast<int>(DAY_LABELS.size()));
    const int compactContentHeight =
        headerHeight + layout.topGap + layout.globalCardH + layout.cardGap + timeOfDayH + layout.cardGap + dayOfWeekH;
    const int extraHeight = std::max(0, availableHeight - compactContentHeight);
    const int perBookCompactContentHeight =
        headerHeight + layout.topGap + layout.topCardH + layout.cardGap + timeOfDayH + layout.cardGap + dayOfWeekH;
    const int perBookExtraHeight = std::max(0, availableHeight - perBookCompactContentHeight);
    const int targetGlobalCardH = globalRtcCardHeightForPerBookRowSpacing(layout, perBookExtraHeight);
    const int extraTopCardHeight = std::min(extraHeight, std::max(0, targetGlobalCardH - layout.globalCardH));
    const int remainingExtraHeight = extraHeight - extraTopCardHeight;
    const int timeOfDayExtraHeight = (remainingExtraHeight * 4) / 11;
    const int dayOfWeekExtraHeight = remainingExtraHeight - timeOfDayExtraHeight;
    const int timeOfDayCardH = timeOfDayH + timeOfDayExtraHeight;
    const int dayOfWeekCardH = dayOfWeekH + dayOfWeekExtraHeight;
    globalCardH += extraTopCardHeight;

    drawGlobalStatsCard(renderer, cardX, y, cardW, globalCardH, tr(STR_STATS_ALL_TIME), stats, layout);
    y += globalCardH + layout.cardGap;

    drawSectionCard(renderer, cardX, y, cardW, timeOfDayCardH, tr(STR_STATS_TIME_OF_DAY), layout);
    drawTimeOfDayDial(renderer, cardX, y, cardW, timeOfDayCardH, stats.timeOfDaySeconds, layout);
    y += timeOfDayCardH + layout.cardGap;

    drawSectionCard(renderer, cardX, y, cardW, dayOfWeekCardH, tr(STR_STATS_DAY_AND_CALENDAR), layout);
    drawWeekdayAndCalendar(renderer, cardX, y, cardW, dayOfWeekCardH, stats, layout);
  } else {
    const int compactContentHeight = headerHeight + layout.topGap + layout.globalCardH;
    globalCardH += std::max(0, availableHeight - compactContentHeight);
    drawGlobalStatsCard(renderer, cardX, y, cardW, globalCardH, tr(STR_STATS_ALL_TIME), stats, layout);
  }

  if (showButtonHints && mappedInput) {
    const auto labels =
        mappedInput->mapLabels(mappedInput->withBackArrow(tr(STR_EXIT)), "", mappedInput->withBackArrow(tr(STR_BACK)),
                               showMoreButton ? tr(STR_MORE) : "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, true);
  }
}

namespace {
// Fill patterns for the four reading periods (Morning, Afternoon, Evening,
// Night): solid, dark dots, light dots, empty. Used by the stacked bars, the
// period ring and the legend so they read alike in black and white.
bool periodInk(const int bucket, const int x, const int y) {
  switch (bucket) {
    case 0:
      return true;
    case 1:
      return ((x + y) & 1) == 0;
    case 2:
      return (x & 1) == 0 && (y & 1) == 0;
    default:
      return false;
  }
}

void fillPeriod(const GfxRenderer& r, const int bucket, const int x, const int y, const int w, const int h) {
  for (int yy = y; yy < y + h; ++yy) {
    for (int xx = x; xx < x + w; ++xx) {
      if (periodInk(bucket, xx, yy)) r.drawPixel(xx, yy, true);
    }
  }
}

// A cell of the overview grid: icon, value, label, centred.
void iconStat(const GfxRenderer& r, const int x, const int w, const int y, const int h, const freeink::Icon& icon,
              const char* value, const char* label) {
  const int valueH = r.getLineHeight(UI_12_FONT_ID);
  const int labelH = r.getLineHeight(SMALL_FONT_ID);
  const int top = y + (h - (icon.h + 2 + valueH + labelH)) / 2;
  drawLucideIcon(r, icon, x + (w - icon.w) / 2, top);
  drawCenteredLabel(r, UI_12_FONT_ID, x, w, top + icon.h + 2, value, true);
  drawCenteredLabel(r, SMALL_FONT_ID, x, w, top + icon.h + 2 + valueH, label);
}

void cardWithTitle(const GfxRenderer& r, const int x, const int y, const int w, const int h, const int titleH,
                   const char* title, const char* right) {
  r.drawRect(x, y, w, h);
  r.drawLine(x, y + titleH, x + w, y + titleH);
  const int th = r.getLineHeight(UI_10_FONT_ID);
  r.drawText(UI_10_FONT_ID, x + 12, y + (titleH - th) / 2, title, true, EpdFontFamily::BOLD);
  if (right != nullptr) {
    const int sh = r.getLineHeight(SMALL_FONT_ID);
    r.drawText(SMALL_FONT_ID, x + w - 12 - r.getTextWidth(SMALL_FONT_ID, right), y + (titleH - sh) / 2, right, true);
  }
}
}  // namespace

void renderWeeklySleepStatsPage(GfxRenderer& renderer, const GlobalReadingStats& stats, const WeeklyBook* books,
                                const int bookCount) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& layout = getStatsLayout(renderer, nullptr, true, false, true);
  CompactHeader::drawTitle(renderer, tr(STR_READING_STATS), true);
  const int screenW = renderer.getScreenWidth();
  const int cardX = metrics.contentSidePadding;
  const int cardW = screenW - metrics.contentSidePadding * 2;
  int y = metrics.topPadding + statsHeaderHeight(metrics, layout, nullptr) + layout.topGap;
  const int bottom = renderer.getScreenHeight() - statsBottomInset(metrics, false);
  constexpr int kGap = 7;
  const int avail = bottom - y - 3 * kGap;
  const int overH = avail * 30 / 100;
  const int dailyH = avail * 21 / 100;
  const int booksH = avail * 25 / 100;
  const int patternH = avail - overH - dailyH - booksH;
  const int titleH = 30;
  const int valueH = renderer.getLineHeight(UI_12_FONT_ID);
  const int smallH = renderer.getLineHeight(SMALL_FONT_ID);
  const int bodyH10 = renderer.getLineHeight(UI_10_FONT_ID);

  // This week (Monday to Sunday) from the day log.
  ReadingStatsDateTime now;
  const bool hasNow = getCurrentLocalReadingStatsDateTime(now);
  const uint32_t todayIdx = hasNow ? readingStatsDayIndex(now.date) : 0;
  const int todayWd = hasNow ? readingStatsDayOfWeekIndex(now.date) : 0;
  const uint32_t mondayIdx = todayIdx >= static_cast<uint32_t>(todayWd) ? todayIdx - todayWd : 0;
  const readingdaylog::Log log = readingdaylog::load();
  uint32_t dayBucket[7][READING_TIME_BUCKET_COUNT] = {};
  uint32_t daySeconds[7] = {};
  std::array<uint32_t, READING_TIME_BUCKET_COUNT> weekBuckets{};
  uint32_t weekSeconds = 0;
  uint32_t weekPages = 0;
  int daysRead = 0;
  for (int d = 0; d < 7 && hasNow; ++d) {
    const readingdaylog::Day* day = log.find(mondayIdx + d);
    if (day == nullptr) continue;
    for (int b = 0; b < READING_TIME_BUCKET_COUNT; ++b) {
      dayBucket[d][b] = day->seconds[b];
      daySeconds[d] += day->seconds[b];
      weekBuckets[b] += day->seconds[b];
    }
    weekSeconds += daySeconds[d];
    weekPages += day->pages;
  }
  // Days read come from the reading history (kept since long before the day
  // log), so the count is right for the whole week.
  for (int d = 0; d < 7 && hasNow; ++d) {
    const uint32_t idx = mondayIdx + d;
    if (idx <= todayIdx &&
        (daySeconds[d] > 0 || readingHistoryHasDay(stats.readingHistoryAnchorDay, stats.readingHistoryBits, idx))) {
      ++daysRead;
    }
  }
  // The day log may have started partway through this week: from which day
  // are the minutes complete?
  uint32_t logStartIdx = 0;
  if (log.lastDay != 0) {
    for (int back = readingdaylog::kDays - 1; back >= 0; --back) {
      const readingdaylog::Day& day = log.days[back];
      if (day.seconds[0] || day.seconds[1] || day.seconds[2] || day.seconds[3] || day.pages) {
        logStartIdx = log.lastDay - static_cast<uint32_t>(back);
        break;
      }
    }
  }
  char buf[48];
  char weekRange[40] = "";
  {
    ReadingStatsDate mon;
    ReadingStatsDate sun;
    if (hasNow && readingStatsDateFromDayIndex(mondayIdx, mon) && readingStatsDateFromDayIndex(mondayIdx + 6, sun)) {
      char m1[8];
      char m2[8];
      formatReadingStatsMonthToken(mon, m1, sizeof(m1));
      formatReadingStatsMonthToken(sun, m2, sizeof(m2));
      snprintf(weekRange, sizeof(weekRange), "%s %u \xE2\x80\x93 %s %u, %u", m1, static_cast<unsigned>(mon.day), m2,
               static_cast<unsigned>(sun.day), static_cast<unsigned>(sun.year));
    }
  }

  // 1. Overview (this week): total in a ring of days read, six figures.
  {
    cardWithTitle(renderer, cardX, y, cardW, overH, titleH, tr(STR_STATS_WEEK_OVERVIEW), weekRange);
    const int bodyTop = y + titleH;
    const int bodyH = overH - titleH;
    const int ringD = std::max(40, std::min(bodyH - 12, cardW * 8 / 25));
    const int ringR = ringD / 2;
    const int ringCx = cardX + 10 + ringR;
    const int ringCy = bodyTop + bodyH / 2;
    tools::drawProgressRing(renderer, ringCx, ringCy, ringR, std::max(8, ringR / 6), daysRead / 7.0f);
    // The ring is the days read this week, and says so.
    snprintf(buf, sizeof(buf), "%d/7", daysRead);
    drawCenteredLabel(renderer, UI_12_FONT_ID, ringCx - ringR, ringD, ringCy - (valueH + smallH) / 2, buf, true);
    drawCenteredLabel(renderer, SMALL_FONT_ID, ringCx - ringR, ringD, ringCy - (valueH + smallH) / 2 + valueH,
                      tr(STR_STATS_DAYS_READ));
    const int gridX = ringCx + ringR + 8;
    const int gridW = cardX + cardW - 6 - gridX;
    const int colW = gridW / 3;
    const int rowH = bodyH / 2;
    snprintf(buf, sizeof(buf), "%d", bookCount);
    iconStat(renderer, gridX, colW, bodyTop, rowH, icon_book_open_32, buf, tr(STR_STATS_IN_PROGRESS));
    formatCompactReadingDuration(weekSeconds, buf, sizeof(buf));
    iconStat(renderer, gridX + colW, colW, bodyTop, rowH, icon_history_32, weekSeconds > 0 ? buf : "0m",
             tr(STR_STATS_TOTAL_SHORT));
    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(weekPages));
    iconStat(renderer, gridX + colW * 2, colW, bodyTop, rowH, icon_file_text_32, buf, tr(STR_STATS_PAGES_READ));
    int loggedDays = 0;
    for (const uint32_t v : daySeconds) loggedDays += v > 0 ? 1 : 0;
    formatCompactReadingDuration(loggedDays > 0 ? weekSeconds / loggedDays : 0, buf, sizeof(buf));
    iconStat(renderer, gridX, colW, bodyTop + rowH, rowH, icon_reading_stats_32, loggedDays > 0 ? buf : "-",
             tr(STR_STATS_DAILY_AVG_LBL));
    const uint16_t streak = hasNow ? stats.currentReadingStreak(&now.date) : 0;
    snprintf(buf, sizeof(buf), "%u %s", static_cast<unsigned>(streak), dayCountText(streak));
    iconStat(renderer, gridX + colW, colW, bodyTop + rowH, rowH, icon_flame_32, buf, tr(STR_STATS_STREAK_SHORT));
    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(stats.completedBooks));
    iconStat(renderer, gridX + colW * 2, colW, bodyTop + rowH, rowH, icon_book_marked_32, buf,
             tr(STR_STATS_COMPLETED_LBL));
    renderer.drawLine(gridX + 4, bodyTop + rowH, cardX + cardW - 10, bodyTop + rowH, true);
    for (int c = 1; c < 3; ++c) {
      renderer.drawLine(gridX + colW * c, bodyTop + 10, gridX + colW * c, bodyTop + rowH - 10, true);
      renderer.drawLine(gridX + colW * c, bodyTop + rowH + 10, gridX + colW * c, bodyTop + bodyH - 10, true);
    }
    y += overH + kGap;
  }

  // 2. Daily reading time (minutes), Monday to Sunday with dates.
  {
    char unit[40];
    snprintf(unit, sizeof(unit), "%s", tr(STR_STATS_MINUTES));
    ReadingStatsDate since;
    if (hasNow && (logStartIdx == 0 || logStartIdx > mondayIdx)) {
      // Minutes are counted from the first logged session (today if none yet).
      const uint32_t from = logStartIdx == 0 ? todayIdx : logStartIdx;
      if (readingStatsDateFromDayIndex(from, since)) {
        char m[8];
        formatReadingStatsMonthToken(since, m, sizeof(m));
        snprintf(unit, sizeof(unit), "(%s %u %s)", tr(STR_STATS_MINUTES_SINCE), static_cast<unsigned>(since.day), m);
      }
    }
    cardWithTitle(renderer, cardX, y, cardW, dailyH, titleH, tr(STR_STATS_DAILY_TIME), unit);
    uint32_t maxV = 1;
    for (const uint32_t v : daySeconds) maxV = std::max(maxV, v);
    const int left = cardX + 12;
    const int slot = (cardW - 24) / 7;
    const int colW = slot * 3 / 5;
    const int barBottom = y + dailyH - 6 - smallH * 2 - 3;
    const int barTop = y + titleH + 4 + smallH;
    const int barMax = std::max(6, barBottom - barTop);
    renderer.drawLine(left, barBottom, left + slot * 7, barBottom, true);
    for (int d = 0; d < 7; ++d) {
      const int sx = left + d * slot;
      const bool isToday = hasNow && d == todayWd;
      const int bh = daySeconds[d] > 0
                         ? std::max(3, static_cast<int>(static_cast<uint64_t>(barMax) * daySeconds[d] / maxV))
                         : (hasNow && d > todayWd ? 0 : 1);
      renderer.fillRect(sx + (slot - colW) / 2, barBottom - bh, colW, bh, true);
      if (!hasNow || d <= todayWd) {
        // Days before the log started have no minutes: "–", not 0.
        const uint32_t from = logStartIdx == 0 ? todayIdx : logStartIdx;
        if (hasNow && mondayIdx + d < from) {
          snprintf(buf, sizeof(buf), "\xE2\x80\x93");
        } else {
          snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(daySeconds[d] / 60));
        }
        drawCenteredLabel(renderer, SMALL_FONT_ID, sx, slot, barBottom - bh - smallH - 1, buf, isToday);
      }
      drawCenteredLabel(renderer, SMALL_FONT_ID, sx, slot, barBottom + 2, I18N.get(DAY_LABELS[d]), isToday);
      ReadingStatsDate date;
      if (hasNow && readingStatsDateFromDayIndex(mondayIdx + d, date)) {
        char m[8];
        formatReadingStatsMonthToken(date, m, sizeof(m));
        snprintf(buf, sizeof(buf), "%s %u", m, static_cast<unsigned>(date.day));
        drawCenteredLabel(renderer, SMALL_FONT_ID, sx, slot, barBottom + 2 + smallH, buf, isToday);
      }
    }
    y += dailyH + kGap;
  }

  // 3. Books this week: icon, title, progress bar, percentage, time.
  {
    cardWithTitle(renderer, cardX, y, cardW, booksH, titleH, tr(STR_STATS_BOOKS_THIS_WEEK), nullptr);
    const int timeColW = renderer.getTextWidth(UI_10_FONT_ID, "00h 00m", EpdFontFamily::BOLD);
    const int pctColW = renderer.getTextWidth(UI_10_FONT_ID, "100%") + 8;
    const int timeX = cardX + cardW - 12 - timeColW;
    const int pctX = timeX - 10 - pctColW;
    const char* hp = tr(STR_STATS_PROGRESS_LBL);
    const char* ht = tr(STR_STATS_TIME_SHORT);
    renderer.drawText(SMALL_FONT_ID, pctX + pctColW - renderer.getTextWidth(SMALL_FONT_ID, hp),
                      y + (titleH - smallH) / 2, hp, true);
    renderer.drawText(SMALL_FONT_ID, timeX + timeColW - renderer.getTextWidth(SMALL_FONT_ID, ht),
                      y + (titleH - smallH) / 2, ht, true);
    constexpr int kRows = 5;
    const int listTop = y + titleH + 2;
    const int rowH = (booksH - titleH - 4) / kRows;
    if (bookCount == 0) {
      drawCenteredLabel(renderer, UI_10_FONT_ID, cardX, cardW, listTop + 10, tr(STR_STATS_NOTHING_IN_PROGRESS));
    }
    for (int i = 0; i < std::min(bookCount, kRows); ++i) {
      const int ry = listTop + i * rowH;
      const WeeklyBook& b = books[i];
      drawLucideIcon(renderer, icon_book_24, cardX + 10, ry + (rowH - 24) / 2);
      const int tx = cardX + 42;
      const int textW = pctX - 12 - tx;
      // 10 pt titles when the row has room for title + bar, small otherwise.
      const int titleFont = rowH >= bodyH10 + 11 ? UI_10_FONT_ID : SMALL_FONT_ID;
      const int titleLineH = renderer.getLineHeight(titleFont);
      const std::string title = renderer.truncatedText(titleFont, b.title.c_str(), textW);
      const int contentTop = ry + (rowH - (titleLineH + 7)) / 2;
      renderer.drawText(titleFont, tx, contentTop, title.c_str(), true);
      const int barY = contentTop + titleLineH + 2;
      renderer.drawRect(tx, barY, textW, 5, true);
      renderer.fillRect(tx, barY, std::max(0, static_cast<int>(textW * b.progress / 100.0f)), 5, true);
      snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(b.progress + 0.5f));
      renderer.drawText(UI_10_FONT_ID, pctX + pctColW - renderer.getTextWidth(UI_10_FONT_ID, buf),
                        ry + (rowH - bodyH10) / 2, buf, true);
      formatCompactReadingDuration(b.seconds, buf, sizeof(buf));
      renderer.drawText(UI_10_FONT_ID,
                        timeX + timeColW - renderer.getTextWidth(UI_10_FONT_ID, buf, EpdFontFamily::BOLD),
                        ry + (rowH - bodyH10) / 2, buf, true, EpdFontFamily::BOLD);
      if (i + 1 < std::min(bookCount, kRows)) {
        renderer.drawLine(cardX + 10, ry + rowH - 1, cardX + cardW - 10, ry + rowH - 1, true);
      }
    }
    y += booksH + kGap;
  }

  // 4. Reading pattern: weekday bars stacked by period, and this week's
  // period split as a ring with a legend.
  {
    cardWithTitle(renderer, cardX, y, cardW, patternH, titleH, tr(STR_STATS_READING_PATTERN),
                  tr(STR_STATS_TIME_OF_DAY));
    const int bodyTop = y + titleH;
    const int bodyH = patternH - titleH;
    // Left: stacked bars.
    const int leftW = cardW * 40 / 100;
    const int left = cardX + 10;
    const int slot = (leftW - 14) / 7;
    const int colW = slot * 3 / 5;
    const int barBottom = bodyTop + bodyH - 6 - smallH - 2;
    const int barMax = std::max(6, barBottom - bodyTop - 8);
    uint32_t maxV = 1;
    for (const uint32_t v : daySeconds) maxV = std::max(maxV, v);
    renderer.drawLine(left, barBottom, left + slot * 7, barBottom, true);
    for (int d = 0; d < 7; ++d) {
      const int bx = left + d * slot + (slot - colW) / 2;
      int top = barBottom;
      for (int b = 0; b < READING_TIME_BUCKET_COUNT; ++b) {
        const int h = static_cast<int>(static_cast<uint64_t>(barMax) * dayBucket[d][b] / maxV);
        if (h <= 0) continue;
        fillPeriod(renderer, b, bx, top - h, colW, h);
        top -= h;
      }
      if (top < barBottom) renderer.drawRect(bx, top, colW, barBottom - top, true);
      const char initial[2] = {I18N.get(DAY_LABELS[d])[0], '\0'};
      drawCenteredLabel(renderer, SMALL_FONT_ID, left + d * slot, slot, barBottom + 2, initial, hasNow && d == todayWd);
    }
    // Dotted divider.
    const int divX = cardX + leftW + 4;
    for (int yy = bodyTop + 8; yy < y + patternH - 8; yy += 4) renderer.drawPixel(divX, yy, true);
    // Right: ring of the period split with the total inside, legend beside.
    const int ringR = std::max(20, std::min(bodyH - 16, 92) / 2);
    const int ringCx = divX + 10 + ringR;
    const int ringCy = bodyTop + bodyH / 2;
    const int rin = ringR - std::max(8, ringR / 4);
    const uint32_t total = weekBuckets[0] + weekBuckets[1] + weekBuckets[2] + weekBuckets[3];
    for (int dy = -ringR; dy <= ringR; ++dy) {
      for (int dx = -ringR; dx <= ringR; ++dx) {
        const int d2 = dx * dx + dy * dy;
        if (d2 > ringR * ringR || d2 < (rin - 1) * (rin - 1)) continue;
        const int rr = static_cast<int>(sqrtf(static_cast<float>(d2)));
        if (rr >= ringR - 1 || rr <= rin) {
          renderer.drawPixel(ringCx + dx, ringCy + dy, true);  // outlines
          continue;
        }
        if (total == 0) continue;
        float a = atan2f(static_cast<float>(dx), static_cast<float>(-dy));
        if (a < 0) a += 2.0f * static_cast<float>(M_PI);
        const uint32_t at = static_cast<uint32_t>(a / (2.0f * static_cast<float>(M_PI)) * total);
        uint32_t acc = 0;
        int bucket = 3;
        for (int b = 0; b < READING_TIME_BUCKET_COUNT; ++b) {
          acc += weekBuckets[b];
          if (at < acc) {
            bucket = b;
            break;
          }
        }
        if (periodInk(bucket, ringCx + dx, ringCy + dy)) renderer.drawPixel(ringCx + dx, ringCy + dy, true);
      }
    }
    formatCompactReadingDuration(total, buf, sizeof(buf));
    const int innerW = (rin - 2) * 2;
    drawCenteredLabel(renderer, SMALL_FONT_ID, ringCx - innerW / 2, innerW, ringCy - smallH, total > 0 ? buf : "0m",
                      true);
    drawCenteredLabel(renderer, SMALL_FONT_ID, ringCx - innerW / 2, innerW, ringCy, tr(STR_STATS_TOTAL_SHORT));
    const int lx = ringCx + ringR + 10;
    const int lw = cardX + cardW - 10 - lx;
    const int legendRowH = std::min(smallH + 6, (bodyH - 8) / static_cast<int>(READING_TIME_BUCKET_COUNT));
    int ly = bodyTop + (bodyH - legendRowH * static_cast<int>(READING_TIME_BUCKET_COUNT)) / 2;
    for (int b = 0; b < READING_TIME_BUCKET_COUNT; ++b) {
      fillPeriod(renderer, b, lx, ly + 2, 10, 10);
      renderer.drawRect(lx, ly + 2, 10, 10, true);
      renderer.drawText(SMALL_FONT_ID, lx + 14, ly, I18N.get(TIME_BUCKET_LABELS[b]), true);
      const int pct =
          total > 0 ? static_cast<int>((static_cast<uint64_t>(weekBuckets[b]) * 100 + total / 2) / total) : 0;
      char line[24];
      snprintf(line, sizeof(line), "%d%%", pct);
      renderer.drawText(SMALL_FONT_ID, lx + lw - renderer.getTextWidth(SMALL_FONT_ID, line), ly, line, true);
      ly += legendRowH;
    }
  }
}

void renderNoRtcCombinedStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput,
                                  const std::string& bookTitle, const BookReadingStats& bookStats,
                                  const float progressPercent, const bool hasEstimatedTimeLeft,
                                  const uint32_t estimatedTimeLeftSeconds, const GlobalReadingStats& deviceStats,
                                  const GlobalReadingStats* allDevicesStats, const bool showButtonHints) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& layout = getNoRtcCombinedLayout(renderer, mappedInput, showButtonHints, allDevicesStats != nullptr);
  if (mappedInput && mappedInput->hasTouchHardware()) {
    TouchHeaderBackButton::drawCompact(renderer, tr(STR_READING_STATS), false);
  } else {
    CompactHeader::drawTitle(renderer, tr(STR_READING_STATS));
  }
  const int screenW = renderer.getScreenWidth();
  const int cardX = metrics.contentSidePadding;
  const int cardW = screenW - metrics.contentSidePadding * 2;
  const int availableHeight =
      renderer.getScreenHeight() - metrics.topPadding - statsBottomInset(metrics, showButtonHints);
  const int headerHeight = statsHeaderHeight(metrics, layout, mappedInput);
  const int compactContentHeight = noRtcCombinedContentHeight(layout, headerHeight, allDevicesStats != nullptr);
  const int extraHeight = std::max(0, availableHeight - compactContentHeight);
  const int visibleCardCount = allDevicesStats ? 3 : 2;
  const int extraPerCard = visibleCardCount > 0 ? extraHeight / visibleCardCount : 0;
  const int extraRemainder = visibleCardCount > 0 ? extraHeight % visibleCardCount : 0;
  const int perBookExtraHeight = extraPerCard + (extraRemainder > 0 ? 1 : 0);
  const int deviceExtraHeight = extraPerCard + (extraRemainder > 1 ? 1 : 0);
  const int allDevicesExtraHeight = allDevicesStats ? extraPerCard : 0;
  const int perBookCardH = noRtcCardBaseHeight(layout) + perBookExtraHeight;
  const int deviceCardH = layout.globalCardH + deviceExtraHeight;
  const int allDevicesCardH = layout.globalCardH + allDevicesExtraHeight;

  int y = metrics.topPadding + headerHeight + layout.topGap;
  drawPerBookStatsCard(renderer, cardX, y, cardW, perBookCardH, bookTitle, bookStats, progressPercent,
                       hasEstimatedTimeLeft, estimatedTimeLeftSeconds, layout);
  y += perBookCardH + layout.cardGap;

  drawGlobalStatsCard(renderer, cardX, y, cardW, deviceCardH, tr(STR_STATS_THIS_DEVICE_SCREEN), deviceStats, layout);
  y += deviceCardH;

  if (allDevicesStats) {
    y += layout.cardGap;
    drawGlobalStatsCard(renderer, cardX, y, cardW, allDevicesCardH, tr(STR_STATS_ALL_DEVICES_SCREEN), *allDevicesStats,
                        layout);
  }

  if (showButtonHints && mappedInput) {
    const auto labels = mappedInput->mapLabels(mappedInput->withBackArrow(tr(STR_BACK)), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, true);
  }
}

void renderEditBookDatesPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const std::string& bookTitle,
                             const BookReadingStats& stats, const int selectedField, const bool showButtonHints) {
  renderer.clearScreen();
  if (mappedInput && mappedInput->hasTouchHardware()) {
    TouchHeaderBackButton::drawCompact(renderer, tr(STR_READING_STATS), false);
  } else {
    CompactHeader::drawTitle(renderer, tr(STR_READING_STATS));
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int cardW = pageWidth - 120;
  const int cardH = 250;
  const int cardX = (pageWidth - cardW) / 2;
  const int cardY = 138;

  const std::string visibleTitle =
      renderer.truncatedText(UI_12_FONT_ID, bookTitle.c_str(), pageWidth - 80, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_12_FONT_ID, 96, visibleTitle.c_str(), true, EpdFontFamily::BOLD);
  renderer.drawRect(cardX, cardY, cardW, cardH);

  const int sectionGap = 104;
  const int row1Y = cardY + 66;
  const int row2Y = row1Y + sectionGap;
  const int monthW = 52;
  const int dayW = 46;
  const int yearW = 68;
  const int gap = 14;
  const int totalFieldW = monthW + gap + dayW + gap + yearW;
#if CROSSINK_APP_CAP_TOUCH
  const bool showTouchControls = mappedInput && mappedInput->hasTouch();
  constexpr int adjustButtonSize = 60;
  constexpr int adjustButtonRightPadding = 34;
  constexpr int adjustButtonGap = 24;
  const int adjustButtonX = cardX + cardW - adjustButtonRightPadding - adjustButtonSize;
  const int fieldAreaW = showTouchControls ? adjustButtonX - cardX - adjustButtonGap : cardW;
#else
  const int fieldAreaW = cardW;
#endif
  const int fieldStartX = cardX + (std::max(totalFieldW, fieldAreaW) - totalFieldW) / 2;

  char monthBuf[8];
  char dayBuf[8];
  char yearBuf[8];

  drawCenteredLabel(renderer, UI_10_FONT_ID, cardX, cardW, cardY + 24, tr(STR_STATS_START_DATE), true);
  formatReadingStatsMonthToken(stats.startDate, monthBuf, sizeof(monthBuf));
  snprintf(dayBuf, sizeof(dayBuf), "%s", stats.startDate.isValid() ? "" : "-");
  if (stats.startDate.isValid()) {
    snprintf(dayBuf, sizeof(dayBuf), "%02u", static_cast<unsigned>(stats.startDate.day));
    snprintf(yearBuf, sizeof(yearBuf), "%u", static_cast<unsigned>(stats.startDate.year));
  } else {
    snprintf(dayBuf, sizeof(dayBuf), "-");
    snprintf(yearBuf, sizeof(yearBuf), "-");
  }
  drawDateField(renderer, fieldStartX, row1Y, monthW, monthBuf, selectedField == 0, BookStatsTouchTarget::dateField(0));
  drawDateField(renderer, fieldStartX + monthW + gap, row1Y, dayW, dayBuf, selectedField == 1,
                BookStatsTouchTarget::dateField(1));
  drawDateField(renderer, fieldStartX + monthW + gap + dayW + gap, row1Y, yearW, yearBuf, selectedField == 2,
                BookStatsTouchTarget::dateField(2));

  drawCenteredLabel(renderer, UI_10_FONT_ID, cardX, cardW, cardY + 24 + sectionGap, tr(STR_STATS_FINISHED_DATE), true);
  const bool showFinishedFields = stats.isCompleted && stats.finishedDate.isValid();
  formatReadingStatsMonthToken(showFinishedFields ? stats.finishedDate : ReadingStatsDate{}, monthBuf,
                               sizeof(monthBuf));
  if (showFinishedFields) {
    snprintf(dayBuf, sizeof(dayBuf), "%02u", static_cast<unsigned>(stats.finishedDate.day));
    snprintf(yearBuf, sizeof(yearBuf), "%u", static_cast<unsigned>(stats.finishedDate.year));
  } else {
    snprintf(dayBuf, sizeof(dayBuf), "-");
    snprintf(yearBuf, sizeof(yearBuf), "-");
  }
  drawDateField(renderer, fieldStartX, row2Y, monthW, monthBuf, selectedField == 3, BookStatsTouchTarget::dateField(3));
  drawDateField(renderer, fieldStartX + monthW + gap, row2Y, dayW, dayBuf, selectedField == 4,
                BookStatsTouchTarget::dateField(4));
  drawDateField(renderer, fieldStartX + monthW + gap + dayW + gap, row2Y, yearW, yearBuf, selectedField == 5,
                BookStatsTouchTarget::dateField(5));

#if CROSSINK_APP_CAP_TOUCH
  if (showTouchControls) {
    const int fieldH = renderer.getLineHeight(UI_12_FONT_ID) + 10;
    drawDateAdjustButton(renderer, adjustButtonX, row1Y + (fieldH - adjustButtonSize) / 2, adjustButtonSize,
                         icon_chevron_up_32, BookStatsTouchTarget::DateAdjustUp);
    drawDateAdjustButton(renderer, adjustButtonX, row2Y + (fieldH - adjustButtonSize) / 2, adjustButtonSize,
                         icon_chevron_down_32, BookStatsTouchTarget::DateAdjustDown);

    constexpr int actionHeight = TouchActionButtons::kDefaultHeight;
    constexpr int actionGap = TouchActionButtons::kDefaultGap;
    constexpr int actionTotalHeight = actionHeight * 2 + actionGap;
    const Rect safeArea = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
    const Rect actionArea{safeArea.x + metrics.contentSidePadding,
                          safeArea.y + safeArea.height - metrics.verticalSpacing - actionTotalHeight,
                          safeArea.width - metrics.contentSidePadding * 2, actionTotalHeight};
    const auto actions = TouchActionButtons::vertical(actionArea, 2);
    const char* labels[] = {tr(STR_SAVE), tr(STR_CANCEL)};
    TouchActionButtons::draw(renderer, actions, labels, 0, -1, UI_10_FONT_ID);
    TouchRegistry::getInstance().add(actions.buttons[0], BookStatsTouchTarget::DateSave, TouchRegistry::Item);
    TouchRegistry::getInstance().add(actions.buttons[1], BookStatsTouchTarget::DateCancel, TouchRegistry::Item);
  }
#endif

  if (showButtonHints && mappedInput) {
    const auto labels = mappedInput->mapLabels(mappedInput->withBackArrow(tr(STR_BACK)), tr(STR_NEXT_FIELD),
                                               tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, true);
  }
}

void renderLibraryStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const LibrarySummary& library,
                            const GlobalReadingStats& stats, const bool showButtonHints) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& layout = getStatsLayout(renderer, mappedInput, true, showButtonHints, shouldShowRtcBasedStats());
  if (mappedInput && mappedInput->hasTouchHardware()) {
    TouchHeaderBackButton::drawCompact(renderer, tr(STR_STATS_LIBRARY), false);
  } else {
    CompactHeader::drawTitle(renderer, tr(STR_STATS_LIBRARY));
  }
  const int cardX = metrics.contentSidePadding;
  const int cardW = renderer.getScreenWidth() - metrics.contentSidePadding * 2;
  const int thirdW = cardW / 3;
  const int rowH = renderer.getLineHeight(UI_12_FONT_ID) + renderer.getLineHeight(SMALL_FONT_ID) + 18;
  int y = metrics.topPadding + statsHeaderHeight(metrics, layout, mappedInput) + layout.topGap;
  char buf[32];

  // Books on the card, with a stacked bar: finished / in progress / not opened.
  const int stackH = 14 + renderer.getLineHeight(SMALL_FONT_ID) + 16;
  const int card1H = layout.sectionTitleH + rowH * 2 + stackH;
  drawSectionCard(renderer, cardX, y, cardW, card1H, tr(STR_STATS_ON_THIS_CARD), layout);
  int ry = y + layout.sectionTitleH;
  snprintf(buf, sizeof(buf), "%u", library.books);
  drawStatCell(renderer, cardX, thirdW, ry, rowH, buf, tr(STR_STATS_BOOKS));
  snprintf(buf, sizeof(buf), "%u", library.folders);
  drawStatCell(renderer, cardX + thirdW, thirdW, ry, rowH, buf, tr(STR_STATS_FOLDERS));
  snprintf(buf, sizeof(buf), "%u", library.opened);
  drawStatCell(renderer, cardX + thirdW * 2, thirdW, ry, rowH, buf, tr(STR_STATS_OPENED));
  ry += rowH;
  snprintf(buf, sizeof(buf), "%u", library.notOpened);
  drawStatCell(renderer, cardX, thirdW, ry, rowH, buf, tr(STR_STATS_NOT_OPENED));
  snprintf(buf, sizeof(buf), "%u", library.inProgress);
  drawStatCell(renderer, cardX + thirdW, thirdW, ry, rowH, buf, tr(STR_STATS_IN_PROGRESS));
  snprintf(buf, sizeof(buf), "%u", library.finished);
  drawStatCell(renderer, cardX + thirdW * 2, thirdW, ry, rowH, buf, tr(STR_STATS_FINISHED));
  ry += rowH;
  {
    const int barX = cardX + 12;
    const int barW = cardW - 24;
    const int total = std::max<int>(1, library.books);
    const int finishedW = barW * library.finished / total;
    const int progressW = barW * library.inProgress / total;
    renderer.drawRect(barX, ry, barW, 14);
    renderer.fillRect(barX, ry, finishedW, 14, true);
    renderer.fillRectDither(barX + finishedW, ry, progressW, 14, Color::DarkGray);
    // Legend: filled = finished, grey = in progress, empty = not opened.
    const int ly = ry + 18;
    int lx = barX;
    const auto legend = [&](const int kind, const char* text) {
      if (kind == 0) renderer.fillRect(lx, ly + 3, 10, 10, true);
      if (kind == 1) renderer.fillRectDither(lx, ly + 3, 10, 10, Color::DarkGray);
      if (kind == 2) renderer.drawRect(lx, ly + 3, 10, 10);
      renderer.drawText(SMALL_FONT_ID, lx + 14, ly, text);
      lx += 14 + renderer.getTextWidth(SMALL_FONT_ID, text) + 16;
    };
    legend(0, tr(STR_STATS_FINISHED));
    legend(1, tr(STR_STATS_IN_PROGRESS));
    legend(2, tr(STR_STATS_NOT_OPENED));
  }
  y += card1H + layout.cardGap;

  // Reading totals.
  const int card2H = layout.sectionTitleH + rowH * 2;
  drawSectionCard(renderer, cardX, y, cardW, card2H, tr(STR_STATS_READING), layout);
  ry = y + layout.sectionTitleH;
  BookReadingStats::formatDuration(stats.totalReadingSeconds, buf, sizeof(buf));
  drawStatCell(renderer, cardX, thirdW, ry, rowH, buf, tr(STR_STATS_TIME_LBL));
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(stats.totalSessions));
  drawStatCell(renderer, cardX + thirdW, thirdW, ry, rowH, buf, tr(STR_STATS_SESSIONS_LBL));
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(stats.totalPagesTurned));
  drawStatCell(renderer, cardX + thirdW * 2, thirdW, ry, rowH, buf, tr(STR_STATS_PAGES_READ));
  ry += rowH;
  snprintf(buf, sizeof(buf), "%.0f%%", library.averageProgress);
  drawStatCell(renderer, cardX, thirdW, ry, rowH, buf, tr(STR_STATS_AVG_PROGRESS));
  ReadingStatsDateTime today;
  const bool hasToday = getCurrentLocalReadingStatsDateTime(today);
  const uint16_t streak = hasToday ? stats.currentReadingStreak(&today.date) : 0;
  snprintf(buf, sizeof(buf), "%u %s", static_cast<unsigned>(streak), dayCountText(streak));
  drawStatCell(renderer, cardX + thirdW, thirdW, ry, rowH, buf, tr(STR_STATS_READING_STREAK_LBL));
  const uint16_t longest = stats.displayLongestReadingStreak();
  snprintf(buf, sizeof(buf), "%u %s", static_cast<unsigned>(longest), dayCountText(longest));
  drawStatCell(renderer, cardX + thirdW * 2, thirdW, ry, rowH, buf, tr(STR_STATS_LONGEST_STREAK_LBL));
  y += card2H + layout.cardGap;

  // Currently reading, with progress bars.
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const int itemH = lineH + 16;
  const int rows = std::max(1, library.currentCount);
  const int card3H = layout.sectionTitleH + 8 + rows * itemH;
  drawSectionCard(renderer, cardX, y, cardW, card3H, tr(STR_STATS_CURRENTLY_READING), layout);
  ry = y + layout.sectionTitleH + 8;
  if (library.currentCount == 0) {
    drawCenteredLabel(renderer, UI_10_FONT_ID, cardX, cardW, ry, tr(STR_STATS_NOTHING_IN_PROGRESS));
  }
  for (int i = 0; i < library.currentCount; ++i) {
    const auto& c = library.current[i];
    snprintf(buf, sizeof(buf), "%.0f%%", c.progress);
    const int pctW = renderer.getTextWidth(UI_10_FONT_ID, buf, EpdFontFamily::BOLD);
    const std::string title = renderer.truncatedText(UI_10_FONT_ID, c.title, cardW - 24 - pctW - 12);
    renderer.drawText(UI_10_FONT_ID, cardX + 12, ry, title.c_str());
    renderer.drawText(UI_10_FONT_ID, cardX + cardW - 12 - pctW, ry, buf, true, EpdFontFamily::BOLD);
    const int barY = ry + lineH + 2;
    const int barW = cardW - 24;
    renderer.drawRect(cardX + 12, barY, barW, 6);
    renderer.fillRect(cardX + 12, barY, static_cast<int>(barW * std::min(100.0f, c.progress) / 100.0f), 6);
    ry += itemH;
  }

  if (showButtonHints && mappedInput) {
    const auto labels = mappedInput->mapLabels(mappedInput->withBackArrow(tr(STR_EXIT)), "",
                                               mappedInput->withBackArrow(tr(STR_BACK)), "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, true);
  }
}
