#include "ToolsCharts.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <cstdio>

#include "fontIds.h"

namespace charts {

namespace {
constexpr int kDot = 5;

void dashedH(const GfxRenderer& r, const int x0, const int x1, const int y) {
  for (int x = x0; x < x1; x += 6) r.drawLine(x, y, std::min(x + 2, x1), y);
}

int scaleY(const Rect& plot, const int v, const int minV, const int maxV) {
  const int span = std::max(1, maxV - minV);
  const int clamped = std::clamp(v, minV, maxV);
  return plot.y + plot.height - 1 - (clamped - minV) * (plot.height - 1) / span;
}
}  // namespace

int statGrid(const GfxRenderer& r, const Rect& box, const int cols, const Stat* stats, const int count) {
  const int valueH = r.getLineHeight(UI_12_FONT_ID);
  const int labelH = r.getLineHeight(SMALL_FONT_ID);
  const int tileH = valueH + labelH + 10;
  const int colW = box.width / cols;
  const int rows = (count + cols - 1) / cols;
  for (int i = 0; i < count; ++i) {
    const int x = box.x + (i % cols) * colW;
    const int y = box.y + (i / cols) * tileH;
    const auto value = r.truncatedText(UI_12_FONT_ID, stats[i].value, colW - 8, EpdFontFamily::BOLD);
    r.drawText(UI_12_FONT_ID, x, y, value.c_str(), true, EpdFontFamily::BOLD);
    const auto label = r.truncatedText(SMALL_FONT_ID, stats[i].label, colW - 8);
    r.drawText(SMALL_FONT_ID, x, y + valueH + 1, label.c_str());
  }
  // Thin separators between rows.
  for (int row = 1; row < rows; ++row) {
    const int y = box.y + row * tileH - 5;
    for (int x = box.x; x < box.x + box.width; x += 3) r.drawPixel(x, y);
  }
  return rows * tileH;
}

void formatMinute(char* buf, const int len, const int minute) {
  const int m = ((minute % 1440) + 1440) % 1440;
  snprintf(buf, static_cast<size_t>(len), "%02d:%02d", m / 60, m % 60);
}

Rect drawPanel(const GfxRenderer& r, const Rect& box, const char* title, const char* leftLabel,
               const char* rightLabel) {
  const int titleH = r.getLineHeight(UI_10_FONT_ID);
  const int axisH = r.getLineHeight(SMALL_FONT_ID);
  r.drawText(UI_10_FONT_ID, box.x, box.y, title, true, EpdFontFamily::BOLD);
  const Rect plot{box.x + 2, box.y + titleH + 4, box.width - 4, box.height - titleH - axisH - 8};
  // Axis lines: left and bottom.
  r.drawLine(plot.x, plot.y, plot.x, plot.y + plot.height);
  r.drawLine(plot.x, plot.y + plot.height, plot.x + plot.width, plot.y + plot.height);
  if (leftLabel != nullptr) r.drawText(SMALL_FONT_ID, plot.x, plot.y + plot.height + 3, leftLabel);
  if (rightLabel != nullptr) {
    const int w = r.getTextWidth(SMALL_FONT_ID, rightLabel);
    r.drawText(SMALL_FONT_ID, plot.x + plot.width - w, plot.y + plot.height + 3, rightLabel);
  }
  return Rect{plot.x + 4, plot.y + 2, plot.width - 8, plot.height - 4};
}

void lineChart(const GfxRenderer& r, const Rect& plot, const int16_t* values, const int n, const int16_t minV,
               const int16_t maxV, const char* yTop, const char* yBottom) {
  if (n <= 0) return;
  for (int q = 1; q < 4; ++q) dashedH(r, plot.x, plot.x + plot.width, plot.y + plot.height * q / 4);
  if (yTop != nullptr) r.drawText(SMALL_FONT_ID, plot.x + 2, plot.y, yTop);
  if (yBottom != nullptr) {
    r.drawText(SMALL_FONT_ID, plot.x + 2, plot.y + plot.height - r.getLineHeight(SMALL_FONT_ID), yBottom);
  }
  int prevX = -1;
  int prevY = -1;
  for (int i = 0; i < n; ++i) {
    const int x = n == 1 ? plot.x + plot.width / 2 : plot.x + i * (plot.width - 1) / (n - 1);
    if (values[i] == kNoValue) {
      prevX = -1;
      continue;
    }
    const int y = scaleY(plot, values[i], minV, maxV);
    if (prevX >= 0) r.drawLine(prevX, prevY, x, y, 2, true);
    r.fillRect(x - kDot / 2, y - kDot / 2, kDot, kDot);
    prevX = x;
    prevY = y;
  }
}

void barChart(const GfxRenderer& r, const Rect& plot, const int16_t* values, const int n, const int16_t maxV) {
  if (n <= 0) return;
  const int slot = plot.width / n;
  const int barW = std::max(2, slot * 2 / 3);
  for (int i = 0; i < n; ++i) {
    if (values[i] == kNoValue || values[i] <= 0) continue;
    const int top = scaleY(plot, values[i], 0, std::max<int16_t>(1, maxV));
    const int x = plot.x + i * slot + (slot - barW) / 2;
    r.fillRect(x, top, barW, plot.y + plot.height - top);
  }
}

void timeScatter(const GfxRenderer& r, const Rect& plot, const int16_t* minutes, const int n) {
  int lo = 1440;
  int hi = -1;
  long sum = 0;
  int count = 0;
  for (int i = 0; i < n; ++i) {
    if (minutes[i] == kNoValue) continue;
    lo = std::min<int>(lo, minutes[i]);
    hi = std::max<int>(hi, minutes[i]);
    sum += minutes[i];
    ++count;
  }
  if (count == 0) {
    r.drawCenteredText(SMALL_FONT_ID, plot.y + plot.height / 2 - 6, "-");
    return;
  }
  // Fit the range to whole hours around the data, at least three hours tall.
  lo = std::max(0, (lo / 60) * 60 - 30);
  hi = std::min(1440, ((hi + 59) / 60) * 60 + 30);
  while (hi - lo < 180) {
    lo = std::max(0, lo - 30);
    hi = std::min(1440, hi + 30);
  }
  char label[8];
  formatMinute(label, sizeof(label), hi);
  r.drawText(SMALL_FONT_ID, plot.x + 2, plot.y, label);
  formatMinute(label, sizeof(label), lo);
  r.drawText(SMALL_FONT_ID, plot.x + 2, plot.y + plot.height - r.getLineHeight(SMALL_FONT_ID), label);
  const int avgY = scaleY(plot, static_cast<int>(sum / count), lo, hi);
  dashedH(r, plot.x, plot.x + plot.width, avgY);
  for (int i = 0; i < n; ++i) {
    if (minutes[i] == kNoValue) continue;
    const int x = n == 1 ? plot.x + plot.width / 2 : plot.x + i * (plot.width - 1) / (n - 1);
    const int y = scaleY(plot, minutes[i], lo, hi);
    r.fillRect(x - 3, y - 3, 7, 7);
  }
}

}  // namespace charts
