#pragma once

#include <cstdint>

#include "components/themes/BaseTheme.h"

class GfxRenderer;

// Small e-ink friendly charts for the tracker stats screens. Everything is
// drawn straight into the framebuffer from caller-owned arrays; nothing is
// allocated. Missing samples are passed as kNoValue and leave a gap.
namespace charts {

constexpr int16_t kNoValue = INT16_MIN;

// Draws a titled frame and returns the plot area inside it (below the title,
// above the x-axis labels).
Rect drawPanel(const GfxRenderer& r, const Rect& box, const char* title, const char* leftLabel, const char* rightLabel);

// Line chart of n values scaled to [minV, maxV], with light guides at the
// quarter marks, a 2 px line between consecutive samples and a dot on each.
// yTop / yBottom are optional axis captions drawn inside the plot.
void lineChart(const GfxRenderer& r, const Rect& plot, const int16_t* values, int n, int16_t minV, int16_t maxV,
               const char* yTop = nullptr, const char* yBottom = nullptr);

// Vertical bars (value per slot, kNoValue = empty slot).
void barChart(const GfxRenderer& r, const Rect& plot, const int16_t* values, int n, int16_t maxV);

// Time-of-day scatter: one square per day at the minute it happened. The y
// range is fitted to the data (at least 3 hours) and labelled with clock
// times; a dashed line marks the average.
void timeScatter(const GfxRenderer& r, const Rect& plot, const int16_t* minutes, int n);

// Grid of "big value / small label" tiles, `cols` per row, starting at
// (box.x, box.y). Returns the height used.
struct Stat {
  char value[24];
  const char* label;
};
int statGrid(const GfxRenderer& r, const Rect& box, int cols, const Stat* stats, int count);

// "HH:MM" (24 h) for a minute of the day.
void formatMinute(char* buf, int len, int minute);

}  // namespace charts
