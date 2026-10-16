#pragma once

#include <string>
#include <vector>

// Text clipboard with a short history, kept on the SD card
// (/.pocketdeck-os/clipboard.txt, one entry per line, newest first) so copies
// survive leaving the reader and sleep, and cost no RAM until pasted. Filled by
// the reader's "Copy text"; pasted by the on-screen keyboard (Hold Down) and
// offered by the Dictionary tool.
namespace clipboard {

constexpr size_t kMaxEntries = 10;
constexpr size_t kMaxBytes = 512;  // per entry

// Adds text as the newest entry (whitespace collapsed, trimmed, cut at
// kMaxBytes on a UTF-8 boundary). The same text copied again moves to the top.
bool add(const std::string& text);
// All entries, newest first (empty when nothing was copied).
std::vector<std::string> entries();
// The newest entry, or "".
std::string latest();

}  // namespace clipboard
