#include "HabitData.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

#include "ToolsCommon.h"

namespace habits {

namespace {
constexpr char kDir[] = "/tools/habits";
constexpr char kNamesPath[] = "/tools/habits/habits.txt";
// Defaults are user data written once to the SD card; the user renames, adds
// or removes habits by editing habits.txt (up to kMaxHabits lines).
constexpr const char* kDefaultNames[] = {"Water", "Reading", "Exercise", "Meditation", "Study", "Sleep"};

void weekPath(char* buf, const size_t len, const int32_t mondayDay) {
  uint16_t isoYear = 0;
  uint8_t week = 0;
  tools::isoWeek(mondayDay, isoYear, week);
  snprintf(buf, len, "%s/%04u-W%02u.txt", kDir, static_cast<unsigned>(isoYear), static_cast<unsigned>(week));
}

bool writeDefaultNames(FsFile& out, void*) {
  return std::all_of(std::begin(kDefaultNames), std::end(kDefaultNames),
                     [&out](const char* name) { return tools::writeText(out, name) && tools::writeText(out, "\n"); });
}

struct WeekWriteCtx {
  const Names* names;
  int count;
  const uint8_t* bits;
};

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool writeWeek(FsFile& out, void* ctx) {
  const auto* w = static_cast<const WeekWriteCtx*>(ctx);
  char line[kNameCap + 12];
  for (int h = 0; h < w->count; ++h) {
    char days[8];
    for (int d = 0; d < 7; ++d) days[d] = (w->bits[h] >> d) & 1 ? '1' : '0';
    days[7] = '\0';
    snprintf(line, sizeof(line), "%s|%s\n", (*w->names)[h], days);
    if (!tools::writeText(out, line)) return false;
  }
  return true;
}

uint8_t parseDays(const char* days) {
  uint8_t bits = 0;
  for (int d = 0; d < 7 && days[d] != '\0'; ++d) {
    if (days[d] == '1') bits |= static_cast<uint8_t>(1U << d);
  }
  return bits;
}
}  // namespace

int loadNames(Names& names) {
  int count = 0;
  for (const char* name : kDefaultNames) snprintf(names[count++], kNameCap, "%s", name);
  if (!Storage.ensureDirectoryExists(kDir)) {
    LOG_ERR("HABIT", "Cannot create %s", kDir);
    return count;
  }
  tools::recoverFromBackup(kNamesPath);
  if (!Storage.exists(kNamesPath)) {
    tools::writeFileAtomic(kNamesPath, &writeDefaultNames, nullptr);
    return count;
  }
  FsFile f;
  if (!Storage.openFileForRead("HABIT", kNamesPath, f)) return count;
  char line[kNameCap];
  int loaded = 0;
  while (loaded < kMaxHabits && tools::readLine(f, line, sizeof(line)) >= 0) {
    // Trim spaces; skip blanks and comments.
    char* s = line;
    while (*s == ' ') ++s;
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == ' ') s[--n] = '\0';
    if (*s == '\0' || *s == '#') continue;
    snprintf(names[loaded++], kNameCap, "%s", s);
  }
  f.close();
  return loaded > 0 ? loaded : count;
}

void loadWeek(const int32_t mondayDay, const Names& names, const int count, uint8_t (&bits)[kMaxHabits]) {
  memset(bits, 0, sizeof(bits));
  char path[48];
  weekPath(path, sizeof(path), mondayDay);
  tools::recoverFromBackup(path);
  if (!Storage.exists(path)) return;
  FsFile f;
  if (!Storage.openFileForRead("HABIT", path, f)) return;
  char line[kNameCap + 12];
  int position = 0;
  while (tools::readLine(f, line, sizeof(line)) >= 0) {
    if (line[0] == '\0') continue;
    char* sep = strrchr(line, '|');
    if (sep == nullptr) {
      // Legacy line without a name: match by position.
      if (position < count) bits[position] = parseDays(line);
    } else {
      *sep = '\0';
      for (int h = 0; h < count; ++h) {
        if (strcmp(names[h], line) == 0) {
          bits[h] = parseDays(sep + 1);
          break;
        }
      }
    }
    ++position;
  }
  f.close();
}

bool saveWeek(const int32_t mondayDay, const Names& names, const int count, const uint8_t (&bits)[kMaxHabits]) {
  if (!Storage.ensureDirectoryExists(kDir)) return false;
  char path[48];
  weekPath(path, sizeof(path), mondayDay);
  WeekWriteCtx ctx{&names, count, bits};
  const bool ok = tools::writeFileAtomic(path, &writeWeek, &ctx);
  if (!ok) LOG_ERR("HABIT", "Failed to save %s", path);
  return ok;
}

int countDoneOn(const int32_t day, int* habitCount) {
  Names names;
  const int count = loadNames(names);
  if (habitCount != nullptr) *habitCount = count;
  uint8_t bits[kMaxHabits];
  loadWeek(mondayOf(day), names, count, bits);
  const int d = tools::weekdayMon0(day);
  int done = 0;
  for (int h = 0; h < count; ++h) done += (bits[h] >> d) & 1;
  return done;
}

}  // namespace habits
