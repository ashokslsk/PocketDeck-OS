#pragma once

#include "ToolStatsActivity.h"

// Builders for ToolStatsActivity, one per tool. Numbers come from the same
// aggregates as the JSON export (ToolStatsData.h).
namespace toolstats {

void buildPomodoro(Page& page, int32_t today, const void* ctx);
void buildHabit(Page& page, int32_t today, const void* habitName);
void buildQuotes(Page& page, int32_t today, const void* ctx);
void buildToday(Page& page, int32_t today, const void* ctx);
void buildMood(Page& page, int32_t today, const void* ctx);
void buildMantras(Page& page, int32_t today, const void* ctx);
void buildWorkout(Page& page, int32_t today, const void* ctx);

}  // namespace toolstats
