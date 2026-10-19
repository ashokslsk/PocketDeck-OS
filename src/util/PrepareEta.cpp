#include "PrepareEta.h"

#include <HalClock.h>
#include <I18n.h>

#include <cstdio>

#include "CrossPointSettings.h"

namespace PrepareEta {

std::string duration(const unsigned long ms) {
  const unsigned long minutes = (ms + 30000UL) / 60000UL;
  char buf[32];
  if (minutes < 1) return tr(STR_DURATION_UNDER_MINUTE);
  if (minutes < 60) {
    snprintf(buf, sizeof(buf), tr(STR_DURATION_M_FORMAT), static_cast<int>(minutes));
  } else {
    snprintf(buf, sizeof(buf), tr(STR_DURATION_HM_FORMAT), static_cast<int>(minutes / 60),
             static_cast<int>(minutes % 60));
  }
  return buf;
}

bool clockAhead(const unsigned long ms, char* buf, const size_t size) {
  uint8_t h = 0;
  uint8_t m = 0;
  if (!halClock.isAvailable() || !halClock.getTime(h, m)) return false;
  uint8_t offset = SETTINGS.clockUtcOffsetQ;
  if (offset > 104) offset = 104;
  int total = h * 60 + m + (static_cast<int>(offset) - 48) * 15 + static_cast<int>((ms + 30000UL) / 60000UL);
  total = ((total % 1440) + 1440) % 1440;
  const int hour24 = total / 60;
  const int minute = total % 60;
  if (SETTINGS.clockFormat == 1) {
    int hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;
    snprintf(buf, size, "%d:%02d %s", hour12, minute, hour24 >= 12 ? "PM" : "AM");
  } else {
    snprintf(buf, size, "%02d:%02d", hour24, minute);
  }
  return true;
}

}  // namespace PrepareEta
