#pragma once

#include <string>

#include "BookReadingStats.h"
#include "GlobalReadingStats.h"

class GfxRenderer;
class MappedInputManager;

namespace BookStatsTouchTarget {
constexpr int StartedDaysStat = 1;
constexpr int DateFieldBase = 10;
constexpr int DateFieldCount = 6;
constexpr int DateAdjustUp = 20;
constexpr int DateAdjustDown = 21;
constexpr int DateSave = 22;
constexpr int DateCancel = 23;

constexpr int dateField(const int index) { return DateFieldBase + index; }
}  // namespace BookStatsTouchTarget

void renderPerBookStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const std::string& bookTitle,
                            const BookReadingStats& stats, float progressPercent, bool hasEstimatedTimeLeft,
                            uint32_t estimatedTimeLeftSeconds, bool showButtonHints, bool showEditButton,
                            bool showMoreButton, uint32_t bookPagesDone = 0, uint32_t bookPagesTotal = 0);

void renderGlobalStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const char* screenTitle,
                           const GlobalReadingStats& stats, bool showButtonHints, bool showMoreButton);

// PocketDeck-OS sleep screen, this week (Monday to Sunday), all books: an
// overview (total time in a ring of days read, pages, daily average, streak),
// reading time per day, the books in progress and the week's time of day.
struct WeeklyBook {
  std::string title;
  float progress = 0;    // 0..100
  uint32_t seconds = 0;  // all-time reading time of the book
};
void renderWeeklySleepStatsPage(GfxRenderer& renderer, const GlobalReadingStats& stats, const WeeklyBook* books,
                                int bookCount);

void renderNoRtcCombinedStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput,
                                  const std::string& bookTitle, const BookReadingStats& bookStats,
                                  float progressPercent, bool hasEstimatedTimeLeft, uint32_t estimatedTimeLeftSeconds,
                                  const GlobalReadingStats& deviceStats, const GlobalReadingStats* allDevicesStats,
                                  bool showButtonHints, uint32_t bookPagesDone = 0, uint32_t bookPagesTotal = 0);

struct LibrarySummary;
// Library dashboard: books and folders on the card, opened vs not, progress,
// pages read, reading time, sessions and streaks, plus what is being read now.
void renderLibraryStatsPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const LibrarySummary& library,
                            const GlobalReadingStats& stats, bool showButtonHints);

void renderEditBookDatesPage(GfxRenderer& renderer, const MappedInputManager* mappedInput, const std::string& bookTitle,
                             const BookReadingStats& stats, int selectedField, bool showButtonHints);
