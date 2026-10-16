#pragma once

#include <cstddef>
#include <cstdint>

// Append-only activity logs for the trackers (habits, medicine, mood,
// Pomodoro, Today, flashcards). One small text file per feature and month:
//
//   /tools/<feature>/log-YYYY-MM.txt
//   YYYY-MM-DD HH:MM|field|field|...
//
// The timestamp is local time when the event was recorded. Appends go
// through writeFileAtomic (old contents are streamed into a temp file with
// the new line, then swapped in), so a power cut never truncates history.
// Monthly files keep every rewrite and every stats scan small.
namespace tlog {

constexpr size_t kLineCap = 160;

// Local wall-clock of an event, in the tools' day numbering.
struct Stamp {
  int32_t day = 0;     // days since 1970-01-01 (tools::daysFromCivil)
  int16_t minute = 0;  // minutes after local midnight
};

// Appends one event stamped `at`. `fields` is the part after the timestamp,
// without a leading '|'. Creates /tools/<feature> as needed.
bool append(const char* feature, const Stamp& at, const char* fields);

// Current local time as a Stamp; false when the clock is not set.
// (Implemented in ToolsCommon.cpp, next to the other clock helpers.)
bool now(Stamp& out);

// Calls fn for every event whose stamp day lies in [fromDay, toDay], oldest
// month first. `fields` points into a scratch buffer the callback may modify.
using LineFn = void (*)(const Stamp& at, char* fields, void* ctx);
void scan(const char* feature, int32_t fromDay, int32_t toDay, LineFn fn, void* ctx);

// Splits "a|b|c" in place: returns the next field and advances *cursor
// (nullptr once exhausted).
char* nextField(char** cursor);

}  // namespace tlog
