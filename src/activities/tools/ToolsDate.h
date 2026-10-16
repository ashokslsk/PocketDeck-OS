#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

// Pure calendar arithmetic for the tools (no HAL, no allocation), kept
// header-only so the native test suite can exercise it directly.
namespace tools {

// Days since 1970-01-01 in the proleptic Gregorian calendar
// (Howard Hinnant's days_from_civil; exact for every valid date).
inline int32_t daysFromCivil(int year, const unsigned month, const unsigned day) {
  year -= month <= 2 ? 1 : 0;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int32_t>(doe) - 719468;
}

inline void civilFromDays(int32_t days, uint16_t& year, uint8_t& month, uint8_t& day) {
  days += 719468;
  const int32_t era = (days >= 0 ? days : days - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(days - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int32_t y = static_cast<int32_t>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  day = static_cast<uint8_t>(doy - (153 * mp + 2) / 5 + 1);
  month = static_cast<uint8_t>(mp < 10 ? mp + 3 : mp - 9);
  year = static_cast<uint16_t>(y + (month <= 2 ? 1 : 0));
}

// 0 = Monday ... 6 = Sunday. 1970-01-01 was a Thursday.
inline uint8_t weekdayMon0(const int32_t days) {
  const int32_t w = (days + 3) % 7;
  return static_cast<uint8_t>(w < 0 ? w + 7 : w);
}

// ISO-8601 week: the week belongs to the year containing its Thursday.
inline void isoWeek(const int32_t days, uint16_t& isoYear, uint8_t& week) {
  const int32_t thursday = days - weekdayMon0(days) + 3;
  uint8_t m = 0, d = 0;
  civilFromDays(thursday, isoYear, m, d);
  week = static_cast<uint8_t>((thursday - daysFromCivil(isoYear, 1, 1)) / 7 + 1);
}

// "YYYY-MM-DD"; buffer needs 11 bytes.
inline void formatIsoDate(char* buf, const size_t len, const int32_t days) {
  uint16_t y = 0;
  uint8_t m = 0, d = 0;
  civilFromDays(days, y, m, d);
  snprintf(buf, len, "%04u-%02u-%02u", static_cast<unsigned>(y), static_cast<unsigned>(m), static_cast<unsigned>(d));
}

inline bool parseIsoDate(const char* s, int32_t& days) {
  if (s == nullptr) return false;
  unsigned v[3] = {0, 0, 0};
  const int widths[3] = {4, 2, 2};
  for (int part = 0; part < 3; ++part) {
    for (int i = 0; i < widths[part]; ++i, ++s) {
      if (*s < '0' || *s > '9') return false;
      v[part] = v[part] * 10 + static_cast<unsigned>(*s - '0');
    }
    if (part < 2 && *s++ != '-') return false;
  }
  if (v[0] < 1970 || v[0] > 2099 || v[1] < 1 || v[1] > 12 || v[2] < 1 || v[2] > 31) return false;
  days = daysFromCivil(static_cast<int>(v[0]), v[1], v[2]);
  return true;
}

inline int32_t firstSundayOf(const int year, const unsigned month) {
  const int32_t first = daysFromCivil(year, month, 1);
  return first + (6 - weekdayMon0(first) + 7) % 7;
}

inline int32_t lastSundayOf(const int year, const unsigned month) {
  const int32_t last = (month == 12 ? daysFromCivil(year + 1, 1, 1) : daysFromCivil(year, month + 1, 1)) - 1;
  return last - (weekdayMon0(last) + 1) % 7;
}

// Daylight-saving rules supported by the World Clock.
enum class DstRule : uint8_t { None, US, EU, AU, NZ };

// UTC offset (minutes) in effect at `utcMinutes` (minutes since the epoch)
// for a zone whose standard offset is `standard`.
inline int16_t effectiveUtcOffset(const int16_t standard, const DstRule rule, const int64_t utcMinutes) {
  if (rule == DstRule::None) return standard;
  uint16_t year = 0;
  uint8_t m = 0, d = 0;
  civilFromDays(static_cast<int32_t>(utcMinutes / 1440), year, m, d);
  const int64_t daylight = standard + 60;
  auto at = [](const int32_t day, const int minuteOfDay, const int64_t offset) {
    return static_cast<int64_t>(day) * 1440 + minuteOfDay - offset;
  };
  bool dst = false;
  switch (rule) {
    case DstRule::US:  // 2nd Sunday Mar 02:00 standard -> 1st Sunday Nov 02:00 daylight
      dst = utcMinutes >= at(firstSundayOf(year, 3) + 7, 120, standard) &&
            utcMinutes < at(firstSundayOf(year, 11), 120, daylight);
      break;
    case DstRule::EU:  // last Sunday Mar 01:00 UTC -> last Sunday Oct 01:00 UTC
      dst = utcMinutes >= at(lastSundayOf(year, 3), 60, 0) && utcMinutes < at(lastSundayOf(year, 10), 60, 0);
      break;
    case DstRule::AU:  // 1st Sunday Oct 02:00 standard -> 1st Sunday Apr 03:00 daylight
      dst = utcMinutes >= at(firstSundayOf(year, 10), 120, standard) ||
            utcMinutes < at(firstSundayOf(year, 4), 180, daylight);
      break;
    case DstRule::NZ:  // last Sunday Sep 02:00 standard -> 1st Sunday Apr 03:00 daylight
      dst = utcMinutes >= at(lastSundayOf(year, 9), 120, standard) ||
            utcMinutes < at(firstSundayOf(year, 4), 180, daylight);
      break;
    case DstRule::None:
      break;
  }
  return static_cast<int16_t>(dst ? daylight : standard);
}

}  // namespace tools
