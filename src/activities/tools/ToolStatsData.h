#pragma once

#include <cstdint>

#include "ToolsLog.h"

// Per-day aggregates read from the tracker logs (ToolsLog.h). Shared by the
// on-device stats pages (ToolStatsPages.cpp) and the JSON export
// (StatsExport.cpp), so both always show the same numbers.
namespace tstats {

constexpr int kHistoryDays = 90;

// One value pair per day, oldest first; `minute` is a time of day or kNoValue.
struct DaySeries {
  int32_t firstDay = 0;
  int16_t a[kHistoryDays] = {};  // meaning depends on the scanner below
  int16_t b[kHistoryDays] = {};
  int16_t minute[kHistoryDays] = {};
};

void clearSeries(DaySeries& s, int32_t today, int16_t fill);
int averageMinute(const DaySeries& s);
// Fills `s` from /tools/<feature>/log-*.txt for the last kHistoryDays days.
void scanSeries(const char* feature, int32_t today, tlog::LineFn fn, DaySeries& s, int16_t fill = 0);

// Scanners (tlog::LineFn):
void onMood(const tlog::Stamp& at, char* fields, void* series);      // a = mood 1..5, minute = logged at
void onPomodoro(const tlog::Stamp& at, char* fields, void* series);  // a = sessions, b = minutes, minute = first
void onToday(const tlog::Stamp& at, char* fields, void* series);     // a = done, b = total
void onQuotes(const tlog::Stamp& at, char* fields, void* series);    // a = 1 on days the quote was opened
void onMantras(const tlog::Stamp& at, char* fields, void* series);   // a = japa count, b = sessions, minute = first
void onWorkout(const tlog::Stamp& at, char* fields, void* series);   // a = minutes, b = sessions, minute = first

// Sum of a[] over the last `days` days (0 = today only counts today).
int sumLast(const int16_t* values, int days);
// Days in a row, ending today (or yesterday when today has nothing yet), with a value > 0.
int streak(const int16_t* values);

// Number of lines in /tools/quotes/favorites.txt.
int countFavouriteQuotes();
constexpr char kFavouritesPath[] = "/tools/quotes/favorites.txt";

// Card / question count stored in a tools index header (0 when missing).
uint32_t indexCount(const char* path);

}  // namespace tstats
