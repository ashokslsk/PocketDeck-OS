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
void onMood(const tlog::Stamp& at, char* fields, void* series);        // a = mood 1..5, minute = logged at
void onPomodoro(const tlog::Stamp& at, char* fields, void* series);    // a = sessions, b = minutes, minute = first
void onToday(const tlog::Stamp& at, char* fields, void* series);       // a = done, b = total
void onFlashcards(const tlog::Stamp& at, char* fields, void* series);  // a = reviewed, b = remembered, minute = first
void onKnowledge(const tlog::Stamp& at, char* fields, void* series);   // a = answers opened, minute = first
void onQuotes(const tlog::Stamp& at, char* fields, void* series);      // a = 1 on days the quote was opened
void onMantras(const tlog::Stamp& at, char* fields, void* series);     // a = japa count, b = sessions, minute = first

// Sum of a[] over the last `days` days (0 = today only counts today).
int sumLast(const int16_t* values, int days);
// Days in a row, ending today (or yesterday when today has nothing yet), with a value > 0.
int streak(const int16_t* values);

struct DeckTotals {
  static constexpr int kMaxDecks = 16;
  char name[kMaxDecks][32] = {};
  uint16_t sessions[kMaxDecks] = {};
  uint32_t reviewed[kMaxDecks] = {};
  uint32_t remembered[kMaxDecks] = {};
  uint32_t forgot[kMaxDecks] = {};
  int count = 0;
};
void onDeck(const tlog::Stamp& at, char* fields, void* totals);

// Spaced-repetition state per deck from /tools/srs_state.json plus card
// counts from the deck indexes.
struct SrsSummary {
  static constexpr int kMaxDecks = 16;
  char name[kMaxDecks][32] = {};
  uint16_t cards[kMaxDecks] = {};     // cards in the deck (0 = deck not indexed yet)
  uint16_t reviewed[kMaxDecks] = {};  // cards seen at least once
  uint16_t mastered[kMaxDecks] = {};  // interval of 21 days or more
  uint16_t due[kMaxDecks] = {};       // reviewed cards due today
  int count = 0;
  int totalCards() const;
  int totalReviewed() const;
  int totalMastered() const;
  int totalDue() const;  // includes never-reviewed cards
};
void summarizeSrs(int32_t today, SrsSummary& out);  // ~1 KB: allocate on the heap

// Knowledge: distinct questions opened per topic, and question counts.
struct KnowledgeCoverage {
  static constexpr int kMaxTopics = 16;
  static constexpr int kMaxQuestions = 512;
  char topic[kMaxTopics][32] = {};
  uint16_t total[kMaxTopics] = {};
  uint16_t seen[kMaxTopics] = {};
  uint8_t bits[kMaxTopics][kMaxQuestions / 8] = {};
  int count = 0;
};
void summarizeKnowledge(int32_t today, KnowledgeCoverage& out);  // ~1.6 KB: allocate on the heap

// Number of lines in /tools/quotes/favorites.txt.
int countFavouriteQuotes();
constexpr char kFavouritesPath[] = "/tools/quotes/favorites.txt";

// Card / question count stored in a tools index header (0 when missing).
uint32_t indexCount(const char* path);

}  // namespace tstats
