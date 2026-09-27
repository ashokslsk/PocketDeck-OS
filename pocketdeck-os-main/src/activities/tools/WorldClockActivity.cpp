#include "WorldClockActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "activities/settings/ClockSyncActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

constexpr char WorldClockActivity::kConfigPath[];

namespace {
constexpr unsigned long kPollMs = 1000;

// Accepts minutes ("330", "-300") or hours ("+9", "-5", "+5:30").
bool parseOffset(const char* s, int16_t& minutes) {
  while (*s == ' ') ++s;
  int sign = 1;
  if (*s == '+' || *s == '-') {
    sign = *s == '-' ? -1 : 1;
    ++s;
  }
  if (*s < '0' || *s > '9') return false;
  const int whole = atoi(s);
  const char* colon = strchr(s, ':');
  int total = 0;
  if (colon != nullptr) {
    total = whole * 60 + atoi(colon + 1);
  } else if (whole <= 14) {
    total = whole * 60;
  } else {
    total = whole;
  }
  if (total > 14 * 60) return false;
  minutes = static_cast<int16_t>(sign * total);
  return true;
}

tools::DstRule parseRule(const char* s) {
  while (*s == ' ') ++s;
  if (strncmp(s, "US", 2) == 0) return tools::DstRule::US;
  if (strncmp(s, "EU", 2) == 0) return tools::DstRule::EU;
  if (strncmp(s, "AU", 2) == 0) return tools::DstRule::AU;
  if (strncmp(s, "NZ", 2) == 0) return tools::DstRule::NZ;
  return tools::DstRule::None;
}

void trimRight(char* s) {
  size_t n = strlen(s);
  while (n > 0 && s[n - 1] == ' ') s[--n] = '\0';
}
}  // namespace

bool WorldClockActivity::writeDefaults(FsFile& out, void*) {
  return tools::writeText(out,
                          "# World Clock cities (max 4): Name|UTC offset in standard time|DST rule\n"
                          "# Offset: hours (+9, -5, +5:30) or minutes (330). DST rules: US, EU, AU, NZ, NONE\n"
                          "New York|-5|US\n"
                          "London|0|EU\n"
                          "Tokyo|+9|NONE\n"
                          "Sydney|+10|AU\n");
}

void WorldClockActivity::loadCities() {
  cityCount_ = 0;
  tools::recoverFromBackup(kConfigPath);
  if (!Storage.exists(kConfigPath) &&
      !tools::writeFileAtomic(kConfigPath, &WorldClockActivity::writeDefaults, nullptr)) {
    LOG_ERR("WCLK", "Could not create %s", kConfigPath);
  }
  FsFile f;
  if (!Storage.openFileForRead("WCLK", kConfigPath, f)) return;
  char line[80];
  while (cityCount_ < kMaxCities && tools::readLine(f, line, sizeof(line)) >= 0) {
    if (line[0] == '#' || line[0] == '\0') continue;
    char* sep1 = strchr(line, '|');
    if (sep1 == nullptr) continue;
    *sep1 = '\0';
    char* sep2 = strchr(sep1 + 1, '|');
    if (sep2 != nullptr) *sep2 = '\0';
    City& city = cities_[cityCount_];
    if (!parseOffset(sep1 + 1, city.standardOffset)) {
      LOG_ERR("WCLK", "Bad offset for %s", line);
      continue;
    }
    snprintf(city.name, sizeof(city.name), "%s", line);
    trimRight(city.name);
    city.rule = sep2 != nullptr ? parseRule(sep2 + 1) : tools::DstRule::None;
    ++cityCount_;
  }
  f.close();
}

void WorldClockActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  loadCities();
  transitionPending_ = true;
  requestUpdate();
}

void WorldClockActivity::syncClock() {
  auto sync = makeUniqueNoThrow<ClockSyncActivity>(renderer, mappedInput);
  if (!sync) {
    LOG_ERR("WCLK", "OOM creating clock sync activity");
    return;
  }
  // WiFi will be brought up: leaving Tools later reboots to defragment.
  tools::markNetworkUsed();
  startActivityForResult(std::move(sync), [this](const ActivityResult&) {
    transitionPending_ = true;
    lastMinute_ = -1;
    input_.reset(mappedInput);
  });
}

void WorldClockActivity::loop() {
  input_.poll(mappedInput);
  if (input_.backLong) {
    tools::exitToHome();
    return;
  }
  if (input_.back) {
    finish();
    return;
  }
  if (input_.confirm) {
    syncClock();
    return;
  }
  const unsigned long now = millis();
  if (now - lastPollMs_ >= kPollMs) {
    lastPollMs_ = now;
    tools::DateTime local;
    const int minute = tools::getLocalNow(local) ? local.hour * 60 + local.minute : -2;
    if (minute != lastMinute_) requestUpdate();
  }
}

void WorldClockActivity::render(RenderLock&&) {
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_WORLD_CLOCK));
  const int width = renderer.getScreenWidth();

  tools::DateTime utc;
  tools::DateTime local;
  if (!tools::getUtcNow(utc) || !tools::getLocalNow(local)) {
    lastMinute_ = -2;
    const int midY = content.y + content.height / 2;
    renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_TOOLS_CLOCK_NOT_SET), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, tr(STR_TOOLS_CLOCK_SYNC_HINT));
    tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_SYNC), "", "");
    const bool transition = transitionPending_;
    transitionPending_ = false;
    renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
    return;
  }
  lastMinute_ = local.hour * 60 + local.minute;

  // Local time in large Inter digits.
  int y = content.y;
  const int localDay = tools::daysOf(local);
  char date[32];
  tools::formatLongDate(date, sizeof(date), local);
  char line[64];
  snprintf(line, sizeof(line), "%s, %s", tools::weekdayName(tools::weekdayMon0(localDay)), date);
  renderer.drawCenteredText(UI_10_FONT_ID, y, line);
  y += renderer.getLineHeight(UI_10_FONT_ID) + 8;

  const bool twelveHour = SETTINGS.clockFormat == 1;
  char digits[8];
  const int shownHour = twelveHour ? (local.hour % 12 == 0 ? 12 : local.hour % 12) : local.hour;
  snprintf(digits, sizeof(digits), twelveHour ? "%d:%02d" : "%02d:%02d", shownHour, local.minute);
  const int digitsW = renderer.getTextWidth(TOOLS_DIGITS_44_FONT_ID, digits);
  const int suffixW = twelveHour ? renderer.getTextWidth(UI_12_FONT_ID, tr(STR_PM)) + 10 : 0;
  const int startX = (width - digitsW - suffixW) / 2;
  const int digitH = tools::drawDigits(renderer, TOOLS_DIGITS_44_FONT_ID, startX, y, digits);
  if (twelveHour) {
    renderer.drawText(UI_12_FONT_ID, startX + digitsW + 10, y + digitH - renderer.getLineHeight(UI_12_FONT_ID),
                      local.hour >= 12 ? tr(STR_PM) : tr(STR_AM), true, EpdFontFamily::BOLD);
  }
  y += digitH + 16;

  // City rows.
  const int64_t utcMinutes = static_cast<int64_t>(tools::daysOf(utc)) * 1440 + utc.hour * 60 + utc.minute;
  const int rowsAreaH = content.y + content.height - y;
  const int rowH = cityCount_ > 0 ? std::min(rowsAreaH / cityCount_, 80) : 0;
  const int nameH = renderer.getLineHeight(UI_12_FONT_ID);
  for (int i = 0; i < cityCount_; ++i) {
    const City& city = cities_[i];
    const int16_t offset = tools::effectiveUtcOffset(city.standardOffset, city.rule, utcMinutes);
    tools::DateTime t;
    tools::getTimeAtOffset(offset, t);
    const int rowY = y + i * rowH;
    renderer.drawLine(content.x, rowY, content.x + content.width, rowY);

    renderer.drawText(UI_12_FONT_ID, content.x, rowY + 8, city.name, true, EpdFontFamily::BOLD);
    const int dayDelta = tools::daysOf(t) - localDay;
    const char* rel = dayDelta > 0   ? tr(STR_TOOLS_TOMORROW)
                      : dayDelta < 0 ? tr(STR_TOOLS_YESTERDAY)
                                     : tr(STR_TOOLS_TODAY);
    const int sign = offset < 0 ? -1 : 1;
    const int absOff = offset * sign;
    char sub[64];
    if (absOff % 60 == 0) {
      snprintf(sub, sizeof(sub), "%s, UTC%c%d", rel, sign < 0 ? '-' : '+', absOff / 60);
    } else {
      snprintf(sub, sizeof(sub), "%s, UTC%c%d:%02d", rel, sign < 0 ? '-' : '+', absOff / 60, absOff % 60);
    }
    renderer.drawText(UI_10_FONT_ID, content.x, rowY + 8 + nameH, sub);

    char clock[16];
    tools::formatClock(clock, sizeof(clock), t.hour, t.minute);
    const int clockW = renderer.getTextWidth(UI_12_FONT_ID, clock, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, content.x + content.width - clockW, rowY + 8 + nameH / 2, clock, true,
                      EpdFontFamily::BOLD);
  }

  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_SYNC), "", "");
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
