#pragma once

#include <cstddef>
#include <cstdint>

// Time-bound courses of medicine or supplements ("Paracetamol, 3 a day for
// 4 days"). Storage under /tools/medicine:
//
//   courses.txt        one course per line:
//                      id|name|doses per day|days|start date|stopped date or -|dose times
//                      c1a2b3c4|Paracetamol|3|4|2026-09-27|-|08:00,13:00,19:00
//   log-YYYY-MM.txt    one line per tick / untick (ToolsLog.h):
//                      2026-09-27 08:12|c1a2b3c4|2026-09-27|0|1
//                      (course id | dose day | dose slot 0..3 | 1 taken, 0 undone)
//
// Courses can be created on the device or by editing courses.txt.
namespace meds {

constexpr int kMaxCourses = 8;
constexpr int kMaxDoses = 4;
constexpr int kMaxDays = 90;
constexpr size_t kNameCap = 24;
constexpr size_t kIdCap = 12;

struct Course {
  char id[kIdCap] = {};
  char name[kNameCap] = {};
  uint8_t doses = 1;                                   // doses per day, 1..4
  uint16_t days = 1;                                   // planned length, 1..90
  int32_t start = 0;                                   // first day (tools day number)
  int32_t stopped = 0;                                 // 0 = not stopped early, else the stop day
  int16_t slotMinute[kMaxDoses] = {};                  // planned time of each dose
  uint8_t taken[(kMaxDays * kMaxDoses + 7) / 8] = {};  // day-major bits

  int32_t lastDay() const { return start + days - 1; }
  // Last day the course is really running (stop day or planned end).
  int32_t endDay() const { return stopped != 0 && stopped < lastDay() ? stopped : lastDay(); }
  bool isTaken(int dayIndex, int slot) const;
  void setTaken(int dayIndex, int slot, bool on);
  int takenCount() const;
};

enum class Status : uint8_t { Upcoming, Active, Completed, Stopped };
Status statusOf(const Course& c, int32_t today);

// Default times for n doses a day: 08:00 / +20:00 / 13:00+19:00 / ... .
void defaultSlots(int doses, int16_t (&out)[kMaxDoses]);
// Translated slot name ("Morning", "Noon", "Evening", "Night") for dose i of n.
const char* slotName(int doses, int slot);

// Loads courses.txt (and their dose bits from the logs). Returns the count.
int load(Course (&courses)[kMaxCourses]);
bool save(const Course (&courses)[kMaxCourses], int count);
// Logs one tick/untick and updates the in-memory bit.
void logDose(Course& c, int dayIndex, int slot, bool taken);
// Fresh id for a new course.
void makeId(char (&out)[kIdCap], const char* name, int32_t day, int minute);

// Per-dose actual times (minutes after midnight, or INT16_MIN when not taken
// the same day) for the stats screen. `out` holds days * kMaxDoses entries.
void loadDoseTimes(const Course& c, int16_t* out, int capacity);

// Everything the Details screen and the /stats/medicine export report.
struct CourseSummary {
  Status status = Status::Upcoming;
  int planned = 0;  // doses * days
  int due = 0;      // doses whose time has come (up to today / stop day)
  int taken = 0;
  int missed = 0;    // due but not taken
  int timed = 0;     // taken the same day, so the real time is known
  int onTime = 0;    // taken within kOnTimeWindow of the planned time
  int avgDelay = 0;  // minutes, + late / - early (valid when timed > 0)
  int fullDays = 0;  // days with every dose taken
  int32_t lastTakenDay = 0;
};
constexpr int kOnTimeWindow = 60;
// doseMinute: scratch of kMaxDays * kMaxDoses entries, filled by loadDoseTimes.
void summarize(const Course& c, int32_t today, int16_t nowMinute, int16_t* doseMinute, CourseSummary& out);

}  // namespace meds
