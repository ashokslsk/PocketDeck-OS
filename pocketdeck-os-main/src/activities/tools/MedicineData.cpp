#include "MedicineData.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ToolsCharts.h"
#include "ToolsCommon.h"
#include "ToolsLog.h"

namespace meds {

namespace {
constexpr char kDir[] = "/tools/medicine";
constexpr char kPath[] = "/tools/medicine/courses.txt";
constexpr char kFeature[] = "medicine";

void trim(char* s) {
  size_t n = strlen(s);
  while (n > 0 && s[n - 1] == ' ') s[--n] = '\0';
}

bool parseHm(const char* s, int16_t& out) {
  if (s == nullptr || strlen(s) < 4) return false;
  const int h = atoi(s);
  const char* colon = strchr(s, ':');
  const int m = colon != nullptr ? atoi(colon + 1) : 0;
  if (h < 0 || h > 23 || m < 0 || m > 59) return false;
  out = static_cast<int16_t>(h * 60 + m);
  return true;
}

struct BitsScan {
  Course* courses;
  int count;
};

void onLogLine(const tlog::Stamp&, char* fields, void* ctx) {
  auto* s = static_cast<BitsScan*>(ctx);
  char* cursor = fields;
  const char* id = tlog::nextField(&cursor);
  const char* date = tlog::nextField(&cursor);
  const char* slot = tlog::nextField(&cursor);
  const char* value = tlog::nextField(&cursor);
  if (id == nullptr || date == nullptr || slot == nullptr || value == nullptr) return;
  int32_t day = 0;
  if (!tools::parseIsoDate(date, day)) return;
  for (int i = 0; i < s->count; ++i) {
    Course& c = s->courses[i];
    if (strcmp(c.id, id) != 0) continue;
    const int idx = day - c.start;
    const int sl = atoi(slot);
    if (idx >= 0 && idx < c.days && sl >= 0 && sl < c.doses) c.setTaken(idx, sl, value[0] == '1');
  }
}

struct TimesScan {
  const Course* course;
  int16_t* out;
  int capacity;
};

void onTimeLine(const tlog::Stamp& at, char* fields, void* ctx) {
  auto* s = static_cast<TimesScan*>(ctx);
  char* cursor = fields;
  const char* id = tlog::nextField(&cursor);
  const char* date = tlog::nextField(&cursor);
  const char* slot = tlog::nextField(&cursor);
  const char* value = tlog::nextField(&cursor);
  if (id == nullptr || date == nullptr || slot == nullptr || value == nullptr || strcmp(id, s->course->id) != 0) {
    return;
  }
  int32_t day = 0;
  if (!tools::parseIsoDate(date, day)) return;
  const int idx = (day - s->course->start) * kMaxDoses + atoi(slot);
  if (idx < 0 || idx >= s->capacity) return;
  // Only same-day ticks say when the dose was really taken.
  s->out[idx] = value[0] == '1' && day == at.day ? at.minute : charts::kNoValue;
}

struct SaveCtx {
  const Course* courses;
  int count;
};

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool writeCourses(FsFile& out, void* ctx) {
  const auto* s = static_cast<const SaveCtx*>(ctx);
  if (!tools::writeText(out,
                        "# PocketDeck-OS medicine & supplement courses\n"
                        "# id|name|doses per day (1-4)|days (1-90)|start YYYY-MM-DD|stopped YYYY-MM-DD or -|"
                        "dose times HH:MM,...\n")) {
    return false;
  }
  char line[160];
  for (int i = 0; i < s->count; ++i) {
    const Course& c = s->courses[i];
    char start[12];
    char stopped[12] = "-";
    tools::formatIsoDate(start, sizeof(start), c.start);
    if (c.stopped != 0) tools::formatIsoDate(stopped, sizeof(stopped), c.stopped);
    char times[32] = "";
    for (int d = 0; d < c.doses; ++d) {
      char hm[8];
      charts::formatMinute(hm, sizeof(hm), c.slotMinute[d]);
      if (d > 0) strncat(times, ",", sizeof(times) - strlen(times) - 1);
      strncat(times, hm, sizeof(times) - strlen(times) - 1);
    }
    snprintf(line, sizeof(line), "%s|%s|%u|%u|%s|%s|%s\n", c.id, c.name, static_cast<unsigned>(c.doses),
             static_cast<unsigned>(c.days), start, stopped, times);
    if (!tools::writeText(out, line)) return false;
  }
  return true;
}
}  // namespace

bool Course::isTaken(const int dayIndex, const int slot) const {
  const int bit = dayIndex * kMaxDoses + slot;
  return bit >= 0 && bit < kMaxDays * kMaxDoses && (taken[bit >> 3] >> (bit & 7)) & 1;
}

void Course::setTaken(const int dayIndex, const int slot, const bool on) {
  const int bit = dayIndex * kMaxDoses + slot;
  if (bit < 0 || bit >= kMaxDays * kMaxDoses) return;
  if (on) {
    taken[bit >> 3] |= static_cast<uint8_t>(1U << (bit & 7));
  } else {
    taken[bit >> 3] &= static_cast<uint8_t>(~(1U << (bit & 7)));
  }
}

int Course::takenCount() const {
  int n = 0;
  for (int d = 0; d < days; ++d) {
    for (int s = 0; s < doses; ++s) n += isTaken(d, s) ? 1 : 0;
  }
  return n;
}

Status statusOf(const Course& c, const int32_t today) {
  if (c.stopped != 0 && c.stopped <= today) return Status::Stopped;
  if (today < c.start) return Status::Upcoming;
  if (today > c.lastDay()) return Status::Completed;
  // All planned doses taken early also counts as completed.
  if (c.takenCount() >= c.doses * c.days) return Status::Completed;
  return Status::Active;
}

void defaultSlots(const int doses, int16_t (&out)[kMaxDoses]) {
  static constexpr int16_t kTimes[4][kMaxDoses] = {
      {8 * 60, 0, 0, 0}, {8 * 60, 20 * 60, 0, 0}, {8 * 60, 13 * 60, 19 * 60, 0}, {8 * 60, 13 * 60, 18 * 60, 22 * 60}};
  const int row = std::clamp(doses, 1, kMaxDoses) - 1;
  std::copy(kTimes[row], kTimes[row] + kMaxDoses, out);
}

const char* slotName(const int doses, const int slot) {
  // 1: morning; 2: morning, evening; 3: morning, noon, evening; 4: + night.
  static constexpr uint8_t kMap[4][kMaxDoses] = {{0, 0, 0, 0}, {0, 2, 0, 0}, {0, 1, 2, 0}, {0, 1, 2, 3}};
  const int row = std::clamp(doses, 1, kMaxDoses) - 1;
  switch (kMap[row][std::clamp(slot, 0, kMaxDoses - 1)]) {
    case 0:
      return tr(STR_TOOLS_SLOT_MORNING);
    case 1:
      return tr(STR_TOOLS_SLOT_NOON);
    case 2:
      return tr(STR_TOOLS_SLOT_EVENING);
    default:
      return tr(STR_TOOLS_SLOT_NIGHT);
  }
}

int load(Course (&courses)[kMaxCourses]) {
  int count = 0;
  if (!tools::ensureToolsDirs() || !Storage.ensureDirectoryExists(kDir)) return 0;
  tools::recoverFromBackup(kPath);
  FsFile f;
  if (!Storage.exists(kPath) || !Storage.openFileForRead("MEDS", kPath, f)) return 0;
  char line[160];
  while (count < kMaxCourses && tools::readLine(f, line, sizeof(line)) >= 0) {
    if (line[0] == '#' || line[0] == '\0') continue;
    char* cursor = line;
    const char* id = tlog::nextField(&cursor);
    char* name = tlog::nextField(&cursor);
    const char* doses = tlog::nextField(&cursor);
    const char* days = tlog::nextField(&cursor);
    const char* start = tlog::nextField(&cursor);
    const char* stopped = tlog::nextField(&cursor);
    char* times = tlog::nextField(&cursor);
    Course& c = courses[count];
    c = Course{};
    if (id == nullptr || name == nullptr || doses == nullptr || days == nullptr || start == nullptr ||
        !tools::parseIsoDate(start, c.start)) {
      LOG_ERR("MEDS", "Skipping malformed course line");
      continue;
    }
    snprintf(c.id, sizeof(c.id), "%s", id);
    trim(name);
    snprintf(c.name, sizeof(c.name), "%s", name);
    c.doses = static_cast<uint8_t>(std::clamp(atoi(doses), 1, kMaxDoses));
    c.days = static_cast<uint16_t>(std::clamp(atoi(days), 1, kMaxDays));
    if (stopped != nullptr && stopped[0] != '-' && !tools::parseIsoDate(stopped, c.stopped)) c.stopped = 0;
    defaultSlots(c.doses, c.slotMinute);
    for (int d = 0; d < c.doses && times != nullptr; ++d) {
      char* comma = strchr(times, ',');
      if (comma != nullptr) *comma = '\0';
      parseHm(times, c.slotMinute[d]);
      times = comma != nullptr ? comma + 1 : nullptr;
    }
    ++count;
  }
  f.close();
  // Dose ticks live in the monthly logs; replay the span all courses cover.
  if (count > 0) {
    int32_t from = courses[0].start;
    int32_t to = courses[0].lastDay();
    for (int i = 1; i < count; ++i) {
      from = std::min(from, courses[i].start);
      to = std::max(to, courses[i].lastDay());
    }
    BitsScan scan{courses, count};
    // Doses can be ticked up to a day after they were due.
    tlog::scan(kFeature, from, to + 1, &onLogLine, &scan);
  }
  return count;
}

bool save(const Course (&courses)[kMaxCourses], const int count) {
  if (!Storage.ensureDirectoryExists(kDir)) return false;
  SaveCtx ctx{courses, count};
  const bool ok = tools::writeFileAtomic(kPath, &writeCourses, &ctx);
  if (!ok) LOG_ERR("MEDS", "Failed to save %s", kPath);
  return ok;
}

void logDose(Course& c, const int dayIndex, const int slot, const bool taken) {
  c.setTaken(dayIndex, slot, taken);
  tlog::Stamp at;
  if (!tlog::now(at)) return;
  char date[12];
  tools::formatIsoDate(date, sizeof(date), c.start + dayIndex);
  char fields[48];
  snprintf(fields, sizeof(fields), "%s|%s|%d|%d", c.id, date, slot, taken ? 1 : 0);
  tlog::append(kFeature, at, fields);
}

void makeId(char (&out)[kIdCap], const char* name, const int32_t day, const int minute) {
  uint32_t h = 2166136261u;
  for (const char* p = name; *p != '\0'; ++p) h = (h ^ static_cast<uint8_t>(*p)) * 16777619u;
  h = (h ^ static_cast<uint32_t>(day)) * 16777619u;
  h = (h ^ static_cast<uint32_t>(minute)) * 16777619u;
  snprintf(out, sizeof(out), "c%08lx", static_cast<unsigned long>(h));
}

void loadDoseTimes(const Course& c, int16_t* out, const int capacity) {
  std::fill(out, out + capacity, charts::kNoValue);
  TimesScan scan{&c, out, capacity};
  tlog::scan(kFeature, c.start, c.lastDay() + 1, &onTimeLine, &scan);
}

void summarize(const Course& c, const int32_t today, const int16_t nowMinute, int16_t* doseMinute, CourseSummary& out) {
  out = CourseSummary{};
  loadDoseTimes(c, doseMinute, kMaxDays * kMaxDoses);
  out.status = statusOf(c, today);
  out.planned = c.doses * c.days;
  // Doses due so far: every slot of past course days, plus today's slots
  // whose planned time has passed; nothing after a stop.
  const int32_t lastCounted = std::min<int32_t>(today, c.endDay());
  long delaySum = 0;
  for (int32_t day = c.start; day <= c.lastDay(); ++day) {
    const int idx = day - c.start;
    int takenThatDay = 0;
    for (int s = 0; s < c.doses; ++s) {
      const bool isTaken = c.isTaken(idx, s);
      if (isTaken) {
        ++out.taken;
        ++takenThatDay;
        out.lastTakenDay = day;
      }
      if (day < lastCounted || (day == lastCounted && (day < today || c.slotMinute[s] <= nowMinute))) ++out.due;
      const int16_t actual = doseMinute[idx * kMaxDoses + s];
      if (isTaken && actual != charts::kNoValue) {
        const int delta = actual - c.slotMinute[s];
        delaySum += delta;
        ++out.timed;
        if (std::abs(delta) <= kOnTimeWindow) ++out.onTime;
      }
    }
    if (takenThatDay == c.doses) ++out.fullDays;
  }
  out.missed = std::max(0, out.due - std::min(out.due, out.taken));
  if (out.timed > 0) out.avgDelay = static_cast<int>(delaySum / out.timed);
}

}  // namespace meds
