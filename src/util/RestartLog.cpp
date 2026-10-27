#include "RestartLog.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include "activities/reader/ReadingStatsUtils.h"

namespace RestartLog {
namespace {
constexpr char kPath[] = "/.pocketdeck-os/restart_log.txt";
constexpr size_t kMaxBytes = 8u * 1024u;

// Keeps the newest half of an over-long log (whole lines only).
void trimIfLarge() {
  HalFile in;
  if (!Storage.openFileForRead("RLOG", kPath, in)) return;
  const size_t size = in.fileSize();
  if (size < kMaxBytes) {
    in.close();
    return;
  }
  const size_t keep = kMaxBytes / 2;
  auto buf = makeUniqueNoThrow<char[]>(keep);
  if (!buf) {
    in.close();
    LOG_ERR("RLOG", "no memory to trim restart log; clearing it");
    Storage.remove(kPath);
    return;
  }
  in.seek(size - keep);
  const int got = in.read(reinterpret_cast<uint8_t*>(buf.get()), keep);
  in.close();
  if (got <= 0) return;
  // Start at the first full line.
  const char* start = static_cast<const char*>(memchr(buf.get(), '\n', static_cast<size_t>(got)));
  start = start ? start + 1 : buf.get();
  const size_t len = static_cast<size_t>(got) - static_cast<size_t>(start - buf.get());
  HalFile out;
  if (!Storage.openFileForWrite("RLOG", kPath, out)) return;
  out.write(reinterpret_cast<const uint8_t*>(start), len);
  out.close();
}
}  // namespace

void append(const char* event) {
  char when[20] = "time unknown";
  ReadingStatsDateTime now;
  if (getCurrentLocalReadingStatsDateTime(now)) {
    snprintf(when, sizeof(when), "%04u-%02u-%02u %02u:%02u", static_cast<unsigned>(now.date.year),
             static_cast<unsigned>(now.date.month), static_cast<unsigned>(now.date.day),
             static_cast<unsigned>(now.hour), static_cast<unsigned>(now.minute));
  }
  char line[192];
  const int n = snprintf(line, sizeof(line), "%s  %s  free=%uK max=%uK\n", when, event ? event : "?",
                         static_cast<unsigned>(ESP.getFreeHeap() / 1024), static_cast<unsigned>(ESP.getMaxAllocHeap() / 1024));
  if (n <= 0) return;
  trimIfLarge();
  HalFile file = Storage.open(kPath, O_WRITE | O_CREAT | O_APPEND);
  if (!file) {
    LOG_ERR("RLOG", "could not open %s", kPath);
    return;
  }
  file.write(reinterpret_cast<const uint8_t*>(line), std::min(static_cast<size_t>(n), sizeof(line) - 1));
  file.close();
}
}  // namespace RestartLog
