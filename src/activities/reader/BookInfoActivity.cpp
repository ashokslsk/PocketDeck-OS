#include "BookInfoActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "ReadingStatsUtils.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/InputReleaseGuard.h"

namespace {
// "Oct 12, 2026"; "-" when unknown.
void formatDateWithYear(const ReadingStatsDate& date, char* buf, const size_t len) {
  if (!date.isValid()) {
    snprintf(buf, len, "-");
    return;
  }
  char shortDate[16];
  formatReadingStatsShortDate(date, shortDate, sizeof(shortDate));
  snprintf(buf, len, "%s, %u", shortDate, static_cast<unsigned>(date.year));
}

void formatDays(const uint32_t days, char* buf, const size_t len) {
  snprintf(buf, len, days == 1 ? tr(STR_BOOK_INFO_DAY_FORMAT) : tr(STR_BOOK_INFO_DAYS_FORMAT),
           static_cast<unsigned>(days));
}
}  // namespace

void BookInfoActivity::onEnter() {
  Activity::onEnter();
  // The menu row was chosen with Confirm (or Power); don't let its release close us.
  ignoreInitialBackRelease_ = mappedInput.isPressed(MappedInputManager::Button::Back);
  ignoreInitialConfirmRelease_ = mappedInput.isPressed(MappedInputManager::Button::Confirm);
  ignoreInitialPowerRelease_ = mappedInput.isPressed(MappedInputManager::Button::Power);
  buildRows();
  requestUpdate();
}

void BookInfoActivity::buildRows() {
  rowCount_ = 0;
  const auto add = [this](const char* label) -> char* {
    if (rowCount_ >= kMaxRows) return nullptr;
    Row& row = rows_[rowCount_++];
    row.label = label;
    row.value[0] = '\0';
    return row.value;
  };
  const BookReadingStats& stats = data_.stats;
  const GlobalReadingStats& global = data_.globalStats;

  ReadingStatsDateTime now;
  const bool haveToday = getCurrentLocalReadingStatsDateTime(now) && now.date.isValid();
  const ReadingStatsDate* today = haveToday ? &now.date : nullptr;
  const bool haveStart = stats.startDate.isValid();

  if (data_.publisher[0] != '\0') {
    if (char* v = add(tr(STR_BOOK_INFO_PUBLISHER))) snprintf(v, sizeof(Row::value), "%s", data_.publisher);
  }
  if (char* v = add(tr(STR_BOOK_INFO_STARTED))) formatDateWithYear(stats.startDate, v, sizeof(Row::value));

  // Calendar days since the first reading day, counting today.
  uint32_t spanDays = 0;
  if (haveStart && haveToday && compareReadingStatsDate(stats.startDate, now.date) <= 0) {
    spanDays = readingSpanDaysInclusive(stats.startDate, now.date);
  }
  if (spanDays > 0) {
    if (char* v = add(tr(STR_BOOK_INFO_READING_FOR))) formatDays(spanDays, v, sizeof(Row::value));
  }

  if (char* v = add(tr(STR_BOOK_INFO_TIME_READ))) {
    char duration[24];
    BookReadingStats::formatDuration(stats.totalReadingSeconds, duration, sizeof(duration));
    snprintf(v, sizeof(Row::value), tr(STR_BOOK_INFO_TIME_SESSIONS_FORMAT), duration,
             static_cast<unsigned>(stats.sessionCount));
  }
  if (char* v = add(tr(STR_BOOK_INFO_PAGES_READ))) {
    snprintf(v, sizeof(Row::value), "%u", static_cast<unsigned>(stats.totalPagesTurned));
  }

  if (stats.isCompleted) {
    if (char* v = add(tr(STR_BOOK_INFO_FINISHED))) formatDateWithYear(stats.finishedDate, v, sizeof(Row::value));
  } else if (data_.hasTimeLeft && data_.timeLeftSeconds > 0) {
    if (char* v = add(tr(STR_BOOK_INFO_TIME_LEFT))) {
      BookReadingStats::formatDuration(data_.timeLeftSeconds, v, sizeof(Row::value));
    }
    // At the pace kept so far (reading time spread over every calendar day
    // since the start, rest days included), when would the book be done?
    if (spanDays > 0 && stats.totalReadingSeconds > 0) {
      const uint32_t perDay = std::max<uint32_t>(1, stats.totalReadingSeconds / spanDays);
      const uint32_t daysNeeded = (data_.timeLeftSeconds + perDay - 1) / perDay;
      ReadingStatsDate finish = now.date;
      addDaysToReadingStatsDate(finish, static_cast<int>(std::min<uint32_t>(daysNeeded, 3650)));
      if (char* v = add(tr(STR_BOOK_INFO_EST_FINISH))) formatDateWithYear(finish, v, sizeof(Row::value));
    }
  }

  // Streaks and days off come from the device's day-by-day reading history
  // (any book), which keeps about the last two years.
  if (char* v = add(tr(STR_BOOK_INFO_STREAK))) {
    snprintf(v, sizeof(Row::value), tr(STR_BOOK_INFO_STREAK_FORMAT),
             static_cast<unsigned>(global.currentReadingStreak(today)),
             static_cast<unsigned>(global.displayLongestReadingStreak()));
  }
  if (spanDays > 0) {
    const uint32_t todayIndex = readingStatsDayIndex(now.date);
    const uint32_t startIndex = readingStatsDayIndex(stats.startDate);
    const uint32_t oldestKept = todayIndex + 1 > READING_HISTORY_DAYS ? todayIndex + 1 - READING_HISTORY_DAYS : 0;
    uint32_t daysOff = 0;
    for (uint32_t day = std::max(startIndex, oldestKept); day <= todayIndex; ++day) {
      if (!readingHistoryHasDay(global.readingHistoryAnchorDay, global.readingHistoryBits, day)) ++daysOff;
    }
    if (char* v = add(tr(STR_BOOK_INFO_DAYS_OFF))) formatDays(daysOff, v, sizeof(Row::value));
  }
}

void BookInfoActivity::loop() {
  if (InputReleaseGuard::consumeInitialRelease(mappedInput, MappedInputManager::Button::Back,
                                               ignoreInitialBackRelease_) ||
      InputReleaseGuard::consumeInitialRelease(mappedInput, MappedInputManager::Button::Power,
                                               ignoreInitialPowerRelease_) ||
      InputReleaseGuard::consumeInitialRelease(mappedInput, MappedInputManager::Button::Confirm,
                                               ignoreInitialConfirmRelease_)) {
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    mappedInput.suppressNextBackRelease();
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
      mappedInput.wasReleased(MappedInputManager::Button::Power)) {
    mappedInput.suppressNextConfirmRelease();
    finish();
  }
}

void BookInfoActivity::render(RenderLock&&) {
  renderer.clearScreen();
  int top, right, bottom, left;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const int hintsH = metrics.buttonHintsHeight;

  // The card: a rounded popup inside the bezel-safe area, above the hints.
  const int cardX = left + 12;
  const int cardY = top + 12;
  const int cardW = screenW - left - right - 24;
  const int cardH = screenH - top - bottom - hintsH - 24;
  renderer.drawRoundedRect(cardX, cardY, cardW, cardH, 3, 14, true);

  constexpr int pad = 20;
  const int x = cardX + pad;
  const int w = cardW - pad * 2;
  int y = cardY + pad - 4;

  renderer.drawText(SMALL_FONT_ID, x, y, tr(STR_BOOK_INFO), true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(SMALL_FONT_ID) + 6;

  // Title (up to two lines) and author.
  const int titleH = renderer.getLineHeight(UI_12_FONT_ID);
  const auto titleLines = renderer.wrappedText(UI_12_FONT_ID, data_.title.c_str(), w, 2, EpdFontFamily::BOLD);
  for (const auto& line : titleLines) {
    renderer.drawText(UI_12_FONT_ID, x, y, line.c_str(), true, EpdFontFamily::BOLD);
    y += titleH;
  }
  if (!data_.author.empty()) {
    const std::string author = renderer.truncatedText(UI_10_FONT_ID, data_.author.c_str(), w);
    renderer.drawText(UI_10_FONT_ID, x, y + 2, author.c_str());
    y += renderer.getLineHeight(UI_10_FONT_ID) + 2;
  }
  y += 14;

  // Progress bar with the percentage to its right.
  const float pct = data_.progressPercent < 0.0f ? 0.0f : std::min(100.0f, data_.progressPercent);
  char pctText[8];
  snprintf(pctText, sizeof(pctText), "%d%%", static_cast<int>(pct + 0.5f));
  const int pctW = renderer.getTextWidth(UI_12_FONT_ID, pctText, EpdFontFamily::BOLD);
  const int barH = 14;
  const int barW = w - pctW - 12;
  const int barY = y + (titleH - barH) / 2;
  renderer.drawRoundedRect(x, barY, barW, barH, 2, barH / 2, true);
  const int fillW = static_cast<int>((barW - 6) * pct / 100.0f);
  if (fillW > 0) renderer.fillRoundedRect(x + 3, barY + 3, fillW, barH - 6, (barH - 6) / 2, Color::Black);
  renderer.drawText(UI_12_FONT_ID, x + w - pctW, y, pctText, true, EpdFontFamily::BOLD);
  y += titleH + 12;
  renderer.drawLine(x, y, x + w, y);
  y += 10;

  // "Label .......... value" rows, as many as fit.
  const int rowH = renderer.getLineHeight(UI_10_FONT_ID) + 10;
  const int rowsBottom = cardY + cardH - pad + 4;
  for (int i = 0; i < rowCount_ && y + rowH <= rowsBottom; ++i) {
    const Row& row = rows_[i];
    const int valueW = std::min(w * 3 / 5, renderer.getTextWidth(UI_10_FONT_ID, row.value, EpdFontFamily::BOLD));
    const std::string value = renderer.truncatedText(UI_10_FONT_ID, row.value, valueW, EpdFontFamily::BOLD);
    const std::string label = renderer.truncatedText(UI_10_FONT_ID, row.label, w - valueW - 12);
    renderer.drawText(UI_10_FONT_ID, x, y, label.c_str());
    renderer.drawText(UI_10_FONT_ID, x + w - renderer.getTextWidth(UI_10_FONT_ID, value.c_str(), EpdFontFamily::BOLD),
                      y, value.c_str(), true, EpdFontFamily::BOLD);
    y += rowH;
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_DONE), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
