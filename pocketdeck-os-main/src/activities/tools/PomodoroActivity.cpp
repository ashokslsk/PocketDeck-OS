#include "PomodoroActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ToolStatsPages.h"
#include "ToolsLog.h"
#include "components/UITheme.h"
#include "fontIds.h"

constexpr char PomodoroActivity::kStatePath[];

namespace {
constexpr uint32_t kMsPerMinute = 60UL * 1000UL;
}

void PomodoroActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  loadState();
  transitionPending_ = true;
  requestUpdate();
}

void PomodoroActivity::onExit() {
  // Exiting stops the timer; only the settings and today's count persist.
  running_ = false;
  saveState();
  Activity::onExit();
}

uint16_t PomodoroActivity::completedOn(const int32_t day) {
  FsFile f;
  if (!Storage.exists(kStatePath) || !Storage.openFileForRead("TOOLS", kStatePath, f)) return 0;
  char line[48];
  int32_t fileDay = -1;
  int count = 0;
  while (tools::readLine(f, line, sizeof(line)) >= 0) {
    if (strncmp(line, "date=", 5) == 0) tools::parseIsoDate(line + 5, fileDay);
    if (strncmp(line, "completed=", 10) == 0) count = atoi(line + 10);
  }
  f.close();
  return fileDay == day && count > 0 ? static_cast<uint16_t>(count) : 0;
}

void PomodoroActivity::loadState() {
  tools::DateTime now;
  const int32_t today = tools::getLocalNow(now) ? tools::daysOf(now) : 0;
  statsDay_ = today;
  completedToday_ = 0;

  tools::recoverFromBackup(kStatePath);
  FsFile f;
  if (!Storage.exists(kStatePath) || !Storage.openFileForRead("TOOLS", kStatePath, f)) return;
  char line[48];
  int32_t fileDay = 0;
  uint16_t fileCount = 0;
  while (tools::readLine(f, line, sizeof(line)) >= 0) {
    char* eq = strchr(line, '=');
    if (eq == nullptr) continue;
    *eq = '\0';
    const char* value = eq + 1;
    const int n = atoi(value);
    if (strcmp(line, "focus") == 0 && n >= 1 && n <= 120) focusMinutes_ = static_cast<uint8_t>(n);
    if (strcmp(line, "break") == 0 && n >= 1 && n <= 60) breakMinutes_ = static_cast<uint8_t>(n);
    if (strcmp(line, "date") == 0) tools::parseIsoDate(value, fileDay);
    if (strcmp(line, "completed") == 0 && n >= 0) fileCount = static_cast<uint16_t>(n);
    if (strcmp(line, "seconds") == 0) showSeconds_ = n != 0;
  }
  f.close();
  // The counter is per day: a file from an earlier day starts again at zero.
  if (fileDay == today) completedToday_ = fileCount;
}

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool PomodoroActivity::writeState(FsFile& out, void* ctx) {
  const auto* self = static_cast<const PomodoroActivity*>(ctx);
  char date[12] = "1970-01-01";
  if (self->statsDay_ > 0) tools::formatIsoDate(date, sizeof(date), self->statsDay_);
  char buf[96];
  snprintf(buf, sizeof(buf),
           "focus=%u\nbreak=%u\ndate=%s\ncompleted=%u\n"
           "# seconds=0 shows whole minutes (one refresh a minute, saves battery)\nseconds=%d\n",
           self->focusMinutes_, self->breakMinutes_, date, self->completedToday_, self->showSeconds_ ? 1 : 0);
  return tools::writeText(out, buf);
}

void PomodoroActivity::saveState() {
  if (!tools::writeFileAtomic(kStatePath, &PomodoroActivity::writeState, this)) {
    LOG_ERR("POMO", "Failed to save %s", kStatePath);
  }
}

uint32_t PomodoroActivity::phaseDurationMs() const {
  return static_cast<uint32_t>(phase_ == Phase::Focus ? focusMinutes_ : breakMinutes_) * kMsPerMinute;
}

uint32_t PomodoroActivity::elapsedMs() const {
  // Unsigned subtraction stays correct across the 49-day millis() wrap.
  return accumulatedMs_ + (running_ ? millis() - phaseStartMs_ : 0);
}

uint32_t PomodoroActivity::remainingSeconds() const {
  const uint32_t total = phaseDurationMs();
  const uint32_t elapsed = std::min(elapsedMs(), total);
  // Round up so "25:00" shows until a full second has passed.
  return (total - elapsed + 999) / 1000;
}

uint32_t PomodoroActivity::shownSeconds() const {
  // MM:SS every second by default. With seconds=0 in pomodoro.txt a running
  // timer shows whole minutes until the last one, so the screen refreshes once
  // a minute instead of every second (e-ink refreshes are the battery cost).
  const uint32_t seconds = remainingSeconds();
  if (showSeconds_ || !running_ || seconds <= 60) return seconds;
  return (seconds + 59) / 60 * 60;
}

void PomodoroActivity::startOrPause() {
  if (running_) {
    accumulatedMs_ += millis() - phaseStartMs_;
    running_ = false;
  } else {
    phaseStartMs_ = millis();
    running_ = true;
  }
  requestUpdate();
}

void PomodoroActivity::resetPhase() {
  running_ = false;
  accumulatedMs_ = 0;
  requestUpdate();
}

void PomodoroActivity::switchPhase(const bool completedFocus) {
  if (completedFocus) {
    tools::DateTime now;
    const int32_t today = tools::getLocalNow(now) ? tools::daysOf(now) : statsDay_;
    if (today != statsDay_) {
      statsDay_ = today;
      completedToday_ = 0;
    }
    ++completedToday_;
    saveState();
    // History for Stats & export: one line per finished focus session.
    tlog::Stamp at;
    if (tlog::now(at)) {
      char fields[24];
      snprintf(fields, sizeof(fields), "focus|%u", static_cast<unsigned>(focusMinutes_));
      tlog::append("pomodoro", at, fields);
    }
  }
  const bool wasFocus = phase_ == Phase::Focus;
  phase_ = wasFocus ? Phase::Break : Phase::Focus;
  accumulatedMs_ = 0;
  // A finished focus rolls straight into its break; a finished break stops
  // and waits, which also lets the device auto-sleep again.
  running_ = completedFocus && wasFocus;
  phaseStartMs_ = millis();
  phaseJustEnded_ = true;
  requestUpdate();
}

void PomodoroActivity::loop() {
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
    startOrPause();
  } else if (input_.confirmLong) {
    resetPhase();
  } else if (input_.leftUp) {
    // Stats; a running timer keeps counting underneath (it is millis-based).
    auto stats =
        makeUniqueNoThrow<ToolStatsActivity>(renderer, mappedInput, tr(STR_TOOLS_POMO_STATS), &toolstats::buildPomodoro,
                                             nullptr, statsx::Feature::Pomodoro, running_);
    if (stats) {
      startActivityForResult(std::move(stats), [this](const ActivityResult&) {
        input_.reset(mappedInput);
        transitionPending_ = true;
        requestUpdate();
      });
      return;
    }
  } else if (input_.right) {
    // Skip (Right only, as labelled): the other phase starts without counting a
    // completed focus. Side buttons do nothing here, so a stray press cannot end a session.
    switchPhase(false);
    running_ = false;
    phaseJustEnded_ = false;
  }

  if (running_) {
    if (elapsedMs() >= phaseDurationMs()) {
      switchPhase(phase_ == Phase::Focus);
      return;
    }
    if (shownSeconds() != lastShownSeconds_) requestUpdate();
  }
}

void PomodoroActivity::render(RenderLock&&) {
  const bool focus = phase_ == Phase::Focus;
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_POMODORO));
  const int lineH = renderer.getLineHeight(UI_12_FONT_ID);

  // Phase label above the ring.
  renderer.drawCenteredText(UI_12_FONT_ID, content.y, focus ? tr(STR_TOOLS_FOCUS) : tr(STR_TOOLS_BREAK), true,
                            EpdFontFamily::BOLD);

  // Ring fills the remaining square area, leaving room for two status lines.
  const int ringTop = content.y + lineH + 8;
  const int ringAreaH = content.height - 2 * lineH - 24 - (lineH + 8);
  const int radius = std::max(40, std::min(content.width, ringAreaH) / 2);
  const int cx = renderer.getScreenWidth() / 2;
  const int cy = ringTop + radius;
  const int thickness = std::max(8, radius / 7);

  const uint32_t total = phaseDurationMs();
  const uint32_t elapsed = std::min(elapsedMs(), total);
  const float remainingFraction = total == 0 ? 0.0f : static_cast<float>(total - elapsed) / static_cast<float>(total);
  tools::drawProgressRing(renderer, cx, cy, radius, thickness, remainingFraction);

  // Remaining time as MM:SS in large Inter digits, centred in the ring.
  const uint32_t seconds = shownSeconds();
  lastShownSeconds_ = seconds;
  const bool wholeMinutes = !showSeconds_ && running_ && seconds > 60;
  char digits[8];
  if (wholeMinutes) {
    snprintf(digits, sizeof(digits), "%u", static_cast<unsigned>(seconds / 60));
  } else {
    snprintf(digits, sizeof(digits), "%02u:%02u", static_cast<unsigned>(seconds / 60),
             static_cast<unsigned>(seconds % 60));
  }
  const int innerWidth = 2 * (radius - thickness);
  int font = TOOLS_DIGITS_44_FONT_ID;
  if (renderer.getTextWidth(font, digits) > innerWidth * 9 / 10) font = TOOLS_DIGITS_30_FONT_ID;
  const int ascender = renderer.getFontAscenderSize(font);
  const int digitH = ascender * 3 / 4;  // Inter figures are about 3/4 of the ascender
  const int labelH = renderer.getLineHeight(UI_10_FONT_ID);
  const int baseline = cy + (digitH - labelH - 8) / 2;
  renderer.drawCenteredText(font, baseline - ascender, digits);
  renderer.drawCenteredText(UI_10_FONT_ID, baseline + 10,
                            wholeMinutes ? tr(STR_TOOLS_MIN_REMAINING) : tr(STR_TOOLS_REMAINING));

  // Status lines under the ring.
  int y = cy + radius + 12;
  const char* status =
      running_ ? tr(STR_TOOLS_RUNNING) : (elapsed > 0 ? tr(STR_TOOLS_PAUSED_HOLD_RESET) : tr(STR_TOOLS_PRESS_START));
  renderer.drawCenteredText(UI_10_FONT_ID, y, status);
  y += lineH;
  char stats[64];
  snprintf(stats, sizeof(stats), "%s %u", tr(STR_TOOLS_COMPLETED_TODAY), completedToday_);
  renderer.drawCenteredText(UI_10_FONT_ID, y, stats);

  tools::drawHints(renderer, mappedInput, tr(STR_BACK), running_ ? tr(STR_TOOLS_PAUSE) : tr(STR_START),
                   tr(STR_TOOLS_POMO_STATS_SHORT), tr(STR_TOOLS_SKIP));

  // Updates are fast partial refreshes; about every five minutes one clean
  // refresh clears e-ink ghosting.
  const uint16_t kFastRefreshesBeforeClean = showSeconds_ ? 300 : 5;
  const bool clean = transitionPending_ || phaseJustEnded_ || fastRefreshCount_ >= kFastRefreshesBeforeClean;
  fastRefreshCount_ = clean ? 0 : fastRefreshCount_ + 1;
  transitionPending_ = false;
  phaseJustEnded_ = false;
  renderer.displayBuffer(clean ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
