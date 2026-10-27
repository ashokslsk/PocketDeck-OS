#pragma once

// /.pocketdeck-os/restart_log.txt: one line per restart that was not a normal
// power-on or wake ("2026-10-23 14:03  reset TASK_WDT  free=84K max=49K"), the
// reader's deliberate low-memory restarts, and each reading session's memory
// low point ("session 24 min "Title": lowest free=38K, smallest block=21K").
// Kept small (oldest half dropped at 8 KB) to read on the card or send along.
namespace RestartLog {
// `event` is a short reason ("PANIC", "low memory: page build"). Adds the
// clock time when known and the current free / largest heap block.
void append(const char* event);
}  // namespace RestartLog
