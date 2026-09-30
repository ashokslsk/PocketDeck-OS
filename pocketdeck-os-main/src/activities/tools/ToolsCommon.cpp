#include "ToolsCommon.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalGPIO.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "CrossPointSettings.h"
#include "KannadaText.h"
#include "SilentRestart.h"
#include "ToolsLog.h"
#include "activities/Activity.h"
#include "activities/reader/ReadingStatsUtils.h"
#include "components/HeaderDate.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace tools {

// ---------------------------------------------------------------------------
// Date and time
// ---------------------------------------------------------------------------

namespace {
// The system clock reads 1970 until NTP sets it; treat anything earlier than
// this as "not set" so we never show a bogus date.
constexpr int kMinPlausibleYear = 2024;
}  // namespace

bool getUtcNow(DateTime& out) {
  out = DateTime{};
  // Same trust rule as the header date: the RTC keeps ticking from 2000-01-01
  // until the first NTP sync, so its date is meaningless before that.
  if (halClock.isAvailable() && SETTINGS.clockDateHasBeenSynced) {
    uint16_t y = 0;
    uint8_t mo = 0, d = 0, h = 0, mi = 0;
    if (halClock.getDateTime(y, mo, d, h, mi) && y >= kMinPlausibleYear) {
      out = DateTime{y, mo, d, h, mi, true};
      return true;
    }
  }
  const time_t now = time(nullptr);
  struct tm utc = {};
  if (gmtime_r(&now, &utc) != nullptr && utc.tm_year + 1900 >= kMinPlausibleYear) {
    out = DateTime{static_cast<uint16_t>(utc.tm_year + 1900), static_cast<uint8_t>(utc.tm_mon + 1),
                   static_cast<uint8_t>(utc.tm_mday),         static_cast<uint8_t>(utc.tm_hour),
                   static_cast<uint8_t>(utc.tm_min),          true};
    return true;
  }
  return false;
}

bool getTimeAtOffset(const int16_t utcOffsetMinutes, DateTime& out) {
  DateTime utc;
  if (!getUtcNow(utc)) {
    out = DateTime{};
    return false;
  }
  int32_t days = daysOf(utc);
  int32_t minutes = static_cast<int32_t>(utc.hour) * 60 + utc.minute + utcOffsetMinutes;
  while (minutes < 0) {
    minutes += 1440;
    --days;
  }
  while (minutes >= 1440) {
    minutes -= 1440;
    ++days;
  }
  out.valid = true;
  out.hour = static_cast<uint8_t>(minutes / 60);
  out.minute = static_cast<uint8_t>(minutes % 60);
  civilFromDays(days, out.year, out.month, out.day);
  return true;
}

bool getLocalNow(DateTime& out) {
  // Settings stores quarter hours biased by 48 (48 = UTC+0), clamped like HalClock.
  const uint8_t biased = SETTINGS.clockUtcOffsetQ > 104 ? 104 : SETTINGS.clockUtcOffsetQ;
  return getTimeAtOffset(static_cast<int16_t>((static_cast<int>(biased) - 48) * 15), out);
}

void formatClock(char* buf, const size_t len, const uint8_t hour, const uint8_t minute) {
  if (SETTINGS.clockFormat == 1) {
    int h12 = hour % 12;
    if (h12 == 0) h12 = 12;
    snprintf(buf, len, "%d:%02u %s", h12, static_cast<unsigned>(minute), hour >= 12 ? tr(STR_PM) : tr(STR_AM));
  } else {
    snprintf(buf, len, "%02u:%02u", static_cast<unsigned>(hour), static_cast<unsigned>(minute));
  }
}

void formatLongDate(char* buf, const size_t len, const DateTime& local) {
  // Prefer the shared header formatter so tools honour Settings > Date format.
  if (formatHeaderDateText(buf, len)) return;
  formatIsoDate(buf, len, daysOf(local));
}

const char* weekdayName(const uint8_t mon0) {
  switch (mon0) {
    case 0:
      return tr(STR_TOOLS_MONDAY);
    case 1:
      return tr(STR_TOOLS_TUESDAY);
    case 2:
      return tr(STR_TOOLS_WEDNESDAY);
    case 3:
      return tr(STR_TOOLS_THURSDAY);
    case 4:
      return tr(STR_TOOLS_FRIDAY);
    case 5:
      return tr(STR_TOOLS_SATURDAY);
    default:
      return tr(STR_TOOLS_SUNDAY);
  }
}

const char* weekdayShortName(const uint8_t mon0) {
  switch (mon0) {
    case 0:
      return tr(STR_TOOLS_MON);
    case 1:
      return tr(STR_TOOLS_TUE);
    case 2:
      return tr(STR_TOOLS_WED);
    case 3:
      return tr(STR_TOOLS_THU);
    case 4:
      return tr(STR_TOOLS_FRI);
    case 5:
      return tr(STR_TOOLS_SAT);
    default:
      return tr(STR_TOOLS_SUN);
  }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void ToolInput::reset(const MappedInputManager& input) {
  // A button still held from the previous screen must be released and pressed
  // again before it counts here (same idea as HomeActivity::backPressSeen).
  backArmed_ = false;
  confirmArmed_ = false;
  leftArmed_ = false;
  backLongFired_ = input.isPressed(MappedInputManager::Button::Back);
  confirmLongFired_ = input.isPressed(MappedInputManager::Button::Confirm);
}

void ToolInput::poll(const MappedInputManager& input) {
  using B = MappedInputManager::Button;
  back = backLong = confirm = confirmLong = leftUp = false;
  up = input.wasPressed(B::Up);
  down = input.wasPressed(B::Down);
  left = input.wasPressed(B::Left);
  right = input.wasPressed(B::Right);
  pageBack = input.wasPressed(B::PageBack);
  pageForward = input.wasPressed(B::PageForward);

  if (left) leftArmed_ = true;
  if (input.wasReleased(B::Left)) {
    leftUp = leftArmed_;
    leftArmed_ = false;
  }

  if (input.wasPressed(B::Back)) {
    backArmed_ = true;
    backLongFired_ = false;
  }
  if (backArmed_ && !backLongFired_ && input.isPressed(B::Back) && input.getHeldTime() >= kLongPressMs) {
    backLong = true;
    backLongFired_ = true;
  }
  if (input.wasReleased(B::Back)) {
    back = backArmed_ && !backLongFired_;
    backArmed_ = false;
  }

  if (input.wasPressed(B::Confirm)) {
    confirmArmed_ = true;
    confirmLongFired_ = false;
  }
  // A long Confirm is reported on release, not while held: tools open menus
  // and pickers on it, and firing earlier would let this same release select
  // the first row of the picker that just opened.
  if (confirmArmed_ && !confirmLongFired_ && input.isPressed(B::Confirm) && input.getHeldTime() >= kLongPressMs) {
    confirmLongFired_ = true;
  }
  if (input.wasReleased(B::Confirm)) {
    confirm = confirmArmed_ && !confirmLongFired_;
    confirmLong = confirmArmed_ && confirmLongFired_;
    confirmArmed_ = false;
  }

  // Touch devices: tap = confirm, horizontal swipe = previous/next.
  if (input.hasTouch()) {
    int x = 0, y = 0;
    if (input.wasScreenTapped(x, y)) confirm = true;
    const auto swipe = input.wasSwipe();
    if (swipe == MappedInputManager::SwipeDir::Left) right = true;
    if (swipe == MappedInputManager::SwipeDir::Right) left = true;
  }

  any = back || backLong || confirm || confirmLong || up || down || left || right || pageBack || pageForward || leftUp;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

Rect drawFrame(GfxRenderer& renderer, const char* title, const char* subtitle) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, title, subtitle);
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int bottom = height - metrics.buttonHintsHeight - metrics.verticalSpacing;
  return Rect{metrics.contentSidePadding, top, width - 2 * metrics.contentSidePadding, bottom - top};
}

void drawHints(GfxRenderer& renderer, const MappedInputManager& input, const char* back, const char* confirm,
               const char* previous, const char* next) {
  const auto labels = input.mapLabels(input.withBackArrow(back), confirm, previous, next);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

int digitHeight(const GfxRenderer& renderer, const int fontId) {
  // Inter's lining figures are about three quarters of its ascender.
  return renderer.getFontAscenderSize(fontId) * 3 / 4;
}

int drawDigits(const GfxRenderer& renderer, const int fontId, const int x, const int top, const char* text) {
  const int ascender = renderer.getFontAscenderSize(fontId);
  const int height = digitHeight(renderer, fontId);
  renderer.drawText(fontId, x, top + height - ascender, text);
  return height;
}

void drawProgressRing(const GfxRenderer& renderer, const int cx, const int cy, const int outerRadius,
                      const int thickness, float fraction) {
  if (fraction < 0.0f) fraction = 0.0f;
  if (fraction > 1.0f) fraction = 1.0f;
  const int ro = outerRadius;
  const int ri = outerRadius - thickness;
  const int trackOuter2 = (ro - 2) * (ro - 2);
  const int trackInner2 = (ri + 2) * (ri + 2);

  // The ESP32-C3 has no FPU, so avoid per-pixel trigonometry: compute the
  // end-of-arc direction once, then classify each pixel with an integer cross
  // product. Coordinates use y pointing up; angles run clockwise from 12.
  const float sweep = fraction * 2.0f * static_cast<float>(M_PI);
  const int32_t bx = static_cast<int32_t>(sinf(sweep) * 4096.0f);
  const int32_t by = static_cast<int32_t>(cosf(sweep) * 4096.0f);
  const bool full = fraction >= 0.9999f;
  const bool empty = fraction <= 0.0001f;
  const bool overHalf = fraction > 0.5f;

  for (int dy = -ro; dy <= ro; ++dy) {
    // Scanline extents: the ring covers |dx| in (inner, outer] on this row.
    const int xo = static_cast<int>(sqrtf(static_cast<float>(ro * ro - dy * dy)));
    const int xi = (dy > -ri && dy < ri) ? static_cast<int>(sqrtf(static_cast<float>(ri * ri - dy * dy))) : -1;
    const int32_t y = -dy;  // y up
    for (int side = -1; side <= 1; side += 2) {
      for (int adx = xi + 1; adx <= xo; ++adx) {
        const int32_t x = side * adx;
        bool filled;
        if (full) {
          filled = true;
        } else if (empty) {
          filled = false;
        } else {
          // Clockwise angle(p) < sweep  <=>  cross(p, b) < 0 within a half-plane.
          const bool beforeEnd = x * by - y * bx < 0;
          const bool rightHalf = x > 0 || (x == 0 && y > 0);
          filled = overHalf ? (rightHalf || beforeEnd) : (rightHalf && beforeEnd);
        }
        if (filled) {
          renderer.drawPixel(cx + x, cy + dy, true);
        } else {
          // Unfilled track: thin inner and outer outlines, chosen by radius so
          // the circles stay continuous at the top and bottom.
          const int d2 = adx * adx + dy * dy;
          if (d2 >= trackOuter2 || d2 <= trackInner2) renderer.drawPixel(cx + x, cy + dy, true);
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Word wrap with a Markdown subset
// ---------------------------------------------------------------------------

namespace {
constexpr size_t kSegCap = 72;

bool isBoldMarker(const char* p, const char* end) { return p + 1 < end && p[0] == '*' && p[1] == '*'; }

// Measures (and optionally draws) one space-free word, honouring **bold**
// toggles inside it. Returns its advance width and updates `bold`.
int layoutWord(const GfxRenderer& renderer, const int fontId, const char* p, const char* end, bool& bold,
               const bool markdown, const bool draw, const int x, const int y) {
  char seg[kSegCap];
  int w = 0;
  while (p < end) {
    if (markdown && isBoldMarker(p, end)) {
      bold = !bold;
      p += 2;
      continue;
    }
    if (markdown && *p == '`') {  // inline code marks are hidden, text kept
      ++p;
      continue;
    }
    const char* s = p;
    while (p < end && !(markdown && (isBoldMarker(p, end) || *p == '`'))) ++p;
    size_t n = static_cast<size_t>(p - s);
    if (n >= kSegCap) n = kSegCap - 1;
    memcpy(seg, s, n);
    seg[n] = '\0';
    const auto style = bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    if (draw) renderer.drawText(fontId, x + w, y, seg, true, style);
    w += renderer.getTextWidth(fontId, seg, style);
  }
  return w;
}

const char* skipSpaces(const char* p, const char* end) {
  while (p < end && (*p == ' ' || *p == '\t')) ++p;
  return p;
}

const char* wordEnd(const char* p, const char* end) {
  while (p < end && *p != ' ' && *p != '\t') ++p;
  return p;
}
}  // namespace

// Kannada text (quotes, flashcards, knowledge files) with the SD card font:
// same paragraphs, Markdown prefixes, paging and centring as below.
namespace {
kannada::Style kannadaStyleFor(const int fontId) {
  return fontId == UI_10_FONT_ID || fontId == SMALL_FONT_ID ? kannada::Style::Label : kannada::Style::Body;
}

int drawWrappedKannada(const GfxRenderer& renderer, const Rect& box, const char* text, const WrapOptions& opt) {
  const auto base = kannadaStyleFor(opt.fontId);
  const int lineH = kannada::lineHeight(renderer, base) + 2;
  // One scratch copy for the current paragraph (NUL-terminated, Markdown marks removed).
  auto buf = makeUniqueNoThrow<char[]>(strlen(text) + 1);
  if (!buf) return 0;
  int lineIndex = 0;
  const char* para = text;
  while (*para != '\0') {
    const char* paraEnd = strchr(para, '\n');
    if (paraEnd == nullptr) paraEnd = para + strlen(para);
    const char* p = para;
    auto style = base;
    bool bullet = false;
    char number[8] = {};
    if (opt.markdown) {
      if (*p == '#') {
        while (p < paraEnd && *p == '#') ++p;
        style = kannada::Style::Value;
      } else if ((p[0] == '-' || p[0] == '*') && p + 1 < paraEnd && p[1] == ' ') {
        bullet = true;
        p += 2;
      } else if (p[0] >= '0' && p[0] <= '9') {
        const char* q = p;
        while (q < paraEnd && q - p < 4 && *q >= '0' && *q <= '9') ++q;
        if (q + 1 < paraEnd && q[0] == '.' && q[1] == ' ') {
          snprintf(number, sizeof(number), "%.*s", static_cast<int>(q + 1 - p), p);
          p = q + 2;
        }
      }
    }
    while (p < paraEnd && *p == ' ') ++p;
    size_t n = 0;
    for (const char* r = p; r < paraEnd; ++r) {
      if (opt.markdown && (*r == '`' || (r[0] == '*' && r + 1 < paraEnd && r[1] == '*'))) {
        if (*r == '*') ++r;
        continue;
      }
      buf[n++] = *r;
    }
    buf[n] = '\0';
    if (n == 0) ++lineIndex;  // blank line keeps paragraph spacing
    const int indent = bullet ? 18 : number[0] ? kannada::width(renderer, number, style) + 8 : 0;
    kannada::Line lines[48];
    const size_t count = kannada::wrap(renderer, buf.get(), style, box.width - indent, lines, 48);
    for (size_t i = 0; i < count; ++i, ++lineIndex) {
      const int visible = lineIndex - opt.skipLines;
      const int y = box.y + visible * lineH;
      if (!opt.draw || i >= 48 || visible < 0 || visible >= opt.maxLines || y + lineH > box.y + box.height) continue;
      const char* s = buf.get() + lines[i].start;
      int x = box.x + indent;
      if (opt.centered) x = box.x + (box.width - kannada::width(renderer, s, style, lines[i].length)) / 2;
      if (i == 0 && bullet) renderer.fillRect(box.x + 5, y + lineH / 2 - 2, 5, 5);
      if (i == 0 && number[0]) kannada::draw(renderer, box.x, y, number, style);
      kannada::draw(renderer, x, y, s, style, true, lines[i].length);
    }
    para = *paraEnd == '\n' ? paraEnd + 1 : paraEnd;
  }
  return lineIndex;
}
}  // namespace

int wrapLineHeight(const GfxRenderer& renderer, const int fontId, const char* text) {
  if (text != nullptr && kannada::ready() && kannada::hasKannada(text)) {
    return kannada::lineHeight(renderer, kannadaStyleFor(fontId)) + 2;
  }
  return renderer.getLineHeight(fontId);
}

int drawWrappedText(const GfxRenderer& renderer, const Rect& box, const char* text, const WrapOptions& opt) {
  if (text == nullptr) return 0;
  if (kannada::ready() && kannada::hasKannada(text)) return drawWrappedKannada(renderer, box, text, opt);
  const int lineH = renderer.getLineHeight(opt.fontId);
  const int spaceW = renderer.getSpaceWidth(opt.fontId);
  const int bulletIndent = spaceW * 3;
  int lineIndex = 0;

  const char* para = text;
  while (*para != '\0') {
    const char* paraEnd = strchr(para, '\n');
    if (paraEnd == nullptr) paraEnd = para + strlen(para);

    const char* p = para;
    bool bold = false;
    int indent = 0;
    bool bullet = false;
    const char* numberStart = nullptr;
    const char* numberEnd = nullptr;
    if (opt.markdown) {
      if (*p == '#') {
        while (p < paraEnd && *p == '#') ++p;
        p = skipSpaces(p, paraEnd);
        bold = true;
      } else if ((p[0] == '-' || p[0] == '*') && p + 1 < paraEnd && p[1] == ' ') {
        bullet = true;
        indent = bulletIndent;
        p += 2;
      } else if (p[0] >= '0' && p[0] <= '9') {
        const char* q = p;
        while (q < paraEnd && *q >= '0' && *q <= '9') ++q;
        if (q + 1 < paraEnd && q[0] == '.' && q[1] == ' ') {
          numberStart = p;
          numberEnd = q + 1;
          bool dummy = false;
          indent = layoutWord(renderer, opt.fontId, numberStart, numberEnd, dummy, false, false, 0, 0) + spaceW;
          p = q + 2;
        }
      }
    }

    p = skipSpaces(p, paraEnd);
    if (p >= paraEnd) {
      ++lineIndex;  // blank line keeps paragraph spacing
    }
    bool firstLine = true;
    while (p < paraEnd) {
      const int avail = box.width - indent;
      // Pass 1: find how many words fit on this line.
      const char* q = p;
      bool boldScan = bold;
      int lineW = 0;
      int words = 0;
      while (q < paraEnd) {
        const char* ws = skipSpaces(q, paraEnd);
        if (ws >= paraEnd) {
          q = ws;
          break;
        }
        const char* we = wordEnd(ws, paraEnd);
        bool boldAfter = boldScan;
        const int ww = layoutWord(renderer, opt.fontId, ws, we, boldAfter, opt.markdown, false, 0, 0);
        const int needed = (words > 0 ? spaceW : 0) + ww;
        if (words > 0 && lineW + needed > avail) break;
        lineW += needed;
        ++words;
        boldScan = boldAfter;
        q = we;
      }

      // Pass 2: draw the line if it falls inside the visible window.
      const int visibleIndex = lineIndex - opt.skipLines;
      const int y = box.y + visibleIndex * lineH;
      if (opt.draw && visibleIndex >= 0 && visibleIndex < opt.maxLines && y + lineH <= box.y + box.height) {
        int x = opt.centered ? box.x + (box.width - lineW) / 2 : box.x + indent;
        if (firstLine && bullet) {
          const int dot = spaceW > 4 ? spaceW - 1 : 3;
          renderer.fillRect(box.x + spaceW / 2, y + renderer.getFontAscenderSize(opt.fontId) / 2, dot, dot);
        }
        if (firstLine && numberStart != nullptr) {
          bool dummy = false;
          layoutWord(renderer, opt.fontId, numberStart, numberEnd, dummy, false, true, box.x, y);
        }
        const char* r = p;
        bool boldDraw = bold;
        int drawn = 0;
        while (drawn < words) {
          const char* ws = skipSpaces(r, paraEnd);
          const char* we = wordEnd(ws, paraEnd);
          if (drawn > 0) x += spaceW;
          x += layoutWord(renderer, opt.fontId, ws, we, boldDraw, opt.markdown, true, x, y);
          r = we;
          ++drawn;
        }
      }
      bold = boldScan;
      p = q;
      firstLine = false;
      ++lineIndex;
    }
    para = *paraEnd == '\n' ? paraEnd + 1 : paraEnd;
  }
  return lineIndex;
}

// ---------------------------------------------------------------------------
// Session flags
// ---------------------------------------------------------------------------

namespace {
bool networkUsed = false;
// Below this largest-free-block size the next EPUB section build is likely
// to fail, so leaving Tools reboots silently instead of returning to Home.
constexpr uint32_t kRestartMaxAllocThreshold = 40U * 1024U;
}  // namespace

void markNetworkUsed() { networkUsed = true; }

bool shouldRestartOnExit() {
  if (networkUsed) return true;
#ifndef SIMULATOR
  return ESP.getMaxAllocHeap() < kRestartMaxAllocThreshold;
#else
  return false;
#endif
}

void clearSessionFlags() { networkUsed = false; }

void exitToHome() {
  if (shouldRestartOnExit()) {
    LOG_INF("TOOLS", "Leaving Tools via silent restart (network used or heap fragmented)");
    clearSessionFlags();
    silentRestart();
    return;
  }
  clearSessionFlags();
  activityManager.goHome(HomeMenuItem::TOOLS, transitionRefresh());
}

HalDisplay::RefreshMode transitionRefresh() {
  return gpio.deviceIsX3() ? HalDisplay::FULL_REFRESH : HalDisplay::HALF_REFRESH;
}

}  // namespace tools

namespace tools {
void formatShortDate(char* buf, const size_t len, const int32_t day) {
  ReadingStatsDate date;
  civilFromDays(day, date.year, date.month, date.day);
  formatReadingStatsShortDate(date, buf, len);
}
}  // namespace tools

namespace tlog {
bool now(Stamp& out) {
  tools::DateTime local;
  if (!tools::getLocalNow(local)) return false;
  out.day = tools::daysOf(local);
  out.minute = static_cast<int16_t>(local.hour * 60 + local.minute);
  return true;
}
}  // namespace tlog
