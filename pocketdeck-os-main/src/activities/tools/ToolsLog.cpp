#include "ToolsLog.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ToolsDate.h"
#include "ToolsStore.h"

namespace tlog {

namespace {
constexpr size_t kPathCap = 64;

void monthPath(char* buf, const size_t len, const char* feature, const int32_t day) {
  uint16_t y = 0;
  uint8_t m = 0, d = 0;
  tools::civilFromDays(day, y, m, d);
  snprintf(buf, len, "%s/%s/log-%04u-%02u.txt", tools::kToolsDir, feature, static_cast<unsigned>(y),
           static_cast<unsigned>(m));
}

struct AppendCtx {
  const char* path;
  const char* line;
};

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool writeAppended(FsFile& out, void* ctx) {
  const auto* a = static_cast<const AppendCtx*>(ctx);
  FsFile in;
  if (Storage.exists(a->path) && Storage.openFileForRead("TLOG", a->path, in)) {
    uint8_t buf[128];
    int n = 0;
    bool endsWithNewline = true;
    while ((n = in.read(buf, sizeof(buf))) > 0) {
      if (out.write(buf, static_cast<size_t>(n)) != static_cast<size_t>(n)) {
        in.close();
        return false;
      }
      endsWithNewline = buf[n - 1] == '\n';
    }
    in.close();
    if (!endsWithNewline && !tools::writeText(out, "\n")) return false;
  }
  return tools::writeText(out, a->line) && tools::writeText(out, "\n");
}

// "YYYY-MM-DD HH:MM|rest" -> stamp + pointer to rest.
bool parseStamp(char* line, Stamp& at, char** rest) {
  if (strlen(line) < 17 || line[10] != ' ' || line[13] != ':' || line[16] != '|') return false;
  line[10] = '\0';
  if (!tools::parseIsoDate(line, at.day)) return false;
  const int hh = atoi(line + 11);
  const int mm = atoi(line + 14);
  if (hh < 0 || hh > 23 || mm < 0 || mm > 59) return false;
  at.minute = static_cast<int16_t>(hh * 60 + mm);
  *rest = line + 17;
  return true;
}
}  // namespace

bool append(const char* feature, const Stamp& at, const char* fields) {
  char dir[kPathCap];
  snprintf(dir, sizeof(dir), "%s/%s", tools::kToolsDir, feature);
  if (!tools::ensureToolsDirs() || !Storage.ensureDirectoryExists(dir)) {
    LOG_ERR("TLOG", "Cannot create %s", dir);
    return false;
  }
  char path[kPathCap];
  monthPath(path, sizeof(path), feature, at.day);
  char date[12];
  tools::formatIsoDate(date, sizeof(date), at.day);
  char line[kLineCap];
  snprintf(line, sizeof(line), "%s %02d:%02d|%s", date, at.minute / 60, at.minute % 60, fields);
  AppendCtx ctx{path, line};
  const bool ok = tools::writeFileAtomic(path, &writeAppended, &ctx);
  if (!ok) LOG_ERR("TLOG", "Failed to append to %s", path);
  return ok;
}

void scan(const char* feature, const int32_t fromDay, const int32_t toDay, const LineFn fn, void* ctx) {
  if (toDay < fromDay) return;
  uint16_t y = 0;
  uint8_t m = 0, d = 0;
  tools::civilFromDays(fromDay, y, m, d);
  int32_t monthStart = tools::daysFromCivil(y, m, 1);
  char path[kPathCap];
  char line[kLineCap];
  while (monthStart <= toDay) {
    monthPath(path, sizeof(path), feature, monthStart);
    tools::recoverFromBackup(path);
    FsFile f;
    if (Storage.exists(path) && Storage.openFileForRead("TLOG", path, f)) {
      while (tools::readLine(f, line, sizeof(line)) >= 0) {
        Stamp at;
        char* rest = nullptr;
        if (!parseStamp(line, at, &rest) || at.day < fromDay || at.day > toDay) continue;
        fn(at, rest, ctx);
      }
      f.close();
    }
    tools::civilFromDays(monthStart, y, m, d);
    monthStart = m == 12 ? tools::daysFromCivil(y + 1, 1, 1) : tools::daysFromCivil(y, m + 1, 1);
  }
}

char* nextField(char** cursor) {
  if (cursor == nullptr || *cursor == nullptr) return nullptr;
  char* start = *cursor;
  char* bar = strchr(start, '|');
  if (bar != nullptr) {
    *bar = '\0';
    *cursor = bar + 1;
  } else {
    *cursor = nullptr;
  }
  return start;
}

}  // namespace tlog
