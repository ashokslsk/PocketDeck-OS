#pragma once

#include <HalDisplay.h>
#include <HalStorage.h>

#include <cstddef>
#include <cstdint>

#include "MappedInputManager.h"
#include "ToolsDate.h"
#include "ToolsStore.h"
#include "components/themes/BaseTheme.h"

class GfxRenderer;

// Shared helpers for the productivity tools (Pomodoro, World Clock, Daily
// Command Center, Habits, Daily Quote, Knowledge Card, Flashcards).
//
// Memory policy: nothing here allocates on the heap. Every buffer is either a
// small stack local or a fixed-size array owned by the calling Activity, so a
// tool's whole footprint is one allocation that disappears when it exits.
// Tool data lives only under /tools on the SD card; /.pocketdeck-os is never
// touched.
namespace tools {

// ---------------------------------------------------------------------------
// Date and time
// ---------------------------------------------------------------------------

struct DateTime {
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  bool valid = false;
};

// UTC wall time from the RTC (only once the date has been NTP-synced, matching
// the header date policy), falling back to an NTP-set system clock.
bool getUtcNow(DateTime& out);
// Local time: UTC plus the user's Settings > Clock offset.
bool getLocalNow(DateTime& out);
// Local time for an arbitrary UTC offset in minutes (World Clock cities).
bool getTimeAtOffset(int16_t utcOffsetMinutes, DateTime& out);

inline int32_t daysOf(const DateTime& dt) { return daysFromCivil(dt.year, dt.month, dt.day); }
// "HH:MM" or "H:MM AM" following Settings > Clock format.
void formatClock(char* buf, size_t len, uint8_t hour, uint8_t minute);
// Localized long date using Settings > Date format; ISO fallback.
void formatLongDate(char* buf, size_t len, const DateTime& local);
// "Sep 27" style short date for a tools day number.
void formatShortDate(char* buf, size_t len, int32_t day);
// Translated weekday name, 0 = Monday.
const char* weekdayName(uint8_t mon0);
const char* weekdayShortName(uint8_t mon0);

// ---------------------------------------------------------------------------
// Input: tool-local button semantics (never remapped globally)
//   Up / Left / PageBack      -> previous
//   Down / Right / PageForward -> next
//   Confirm (short)           -> select / toggle
//   Confirm (hold)            -> secondary action (reported on release)
//   Back (short)              -> back one level
//   Back (hold)               -> exit Tools to Home
// ---------------------------------------------------------------------------

class ToolInput {
 public:
  static constexpr unsigned long kLongPressMs = 700;

  void poll(const MappedInputManager& input);
  // Ignore releases of buttons that were already held when the tool opened.
  void reset(const MappedInputManager& input);

  bool back = false;
  bool backLong = false;
  bool confirm = false;
  bool confirmLong = false;
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  bool pageBack = false;
  bool pageForward = false;
  bool any = false;

  bool prev() const { return up || left || pageBack; }
  bool next() const { return down || right || pageForward; }

 private:
  bool backArmed_ = false;
  bool confirmArmed_ = false;
  bool backLongFired_ = false;
  bool confirmLongFired_ = false;
};

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

// Draws the themed header and returns the content area between the header and
// the button hint strip.
Rect drawFrame(GfxRenderer& renderer, const char* title, const char* subtitle = nullptr);
void drawHints(GfxRenderer& renderer, const MappedInputManager& input, const char* back, const char* confirm,
               const char* previous, const char* next);

// Large clock digits in Inter (TOOLS_DIGITS_44/30). `top` is the top edge of
// the figures, not of the line box. Returns the figure height in pixels.
int digitHeight(const GfxRenderer& renderer, int fontId);
int drawDigits(const GfxRenderer& renderer, int fontId, int x, int top, const char* text);

// Circular progress ring rasterised one scanline at a time: for every row the
// outer and inner x-extents come from integer square roots, and only pixels in
// the ring band get an angle test. `fraction` (0..1) is filled clockwise from
// 12 o'clock; the rest of the band is drawn as a thin track.
void drawProgressRing(const GfxRenderer& renderer, int cx, int cy, int outerRadius, int thickness, float fraction);

// Word-wrapped text with an optional Markdown subset (#/## headings, - / *
// bullets, 1. numbered items, **bold** spans; `code` marks are hidden). Lines
// before `skipLines` are laid out but not drawn, which gives cheap
// pagination. Returns the total number of laid-out lines.
struct WrapOptions {
  int fontId = 0;
  bool centered = false;
  bool markdown = false;
  int skipLines = 0;
  int maxLines = 0x7fff;
  bool draw = true;
};
int drawWrappedText(const GfxRenderer& renderer, const Rect& box, const char* text, const WrapOptions& opt);

// Returns true when the tools session used WiFi or left the heap fragmented,
// in which case exiting Tools should silently reboot to Home (the same
// defragmentation pattern CrossInk's network activities use).
void markNetworkUsed();
bool shouldRestartOnExit();
void clearSessionFlags();

// Leaves Tools entirely (Back held). Reboots silently to Home when the
// session used WiFi or fragmented the heap, otherwise returns to Home.
void exitToHome();

// Refresh used when entering or leaving a tool: a clean full refresh on X3
// (as main.cpp does for silent restarts) and the single-pass HALF refresh on
// X4-class panels. Everything else inside a tool uses FAST (partial) refresh.
HalDisplay::RefreshMode transitionRefresh();

}  // namespace tools
