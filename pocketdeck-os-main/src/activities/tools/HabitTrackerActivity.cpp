#include "HabitTrackerActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "HabitStatsActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "activities/util/OptionSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

void HabitTrackerActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  count_ = habits::loadNames(names_);
  tools::DateTime local;
  clockValid_ = tools::getLocalNow(local);
  if (clockValid_) {
    today_ = tools::daysOf(local);
    selDay_ = tools::weekdayMon0(today_);
    showWeek(habits::mondayOf(today_));
  }
  transitionPending_ = true;
  requestUpdate();
}

void HabitTrackerActivity::onExit() {
  saveIfDirty();
  Activity::onExit();
}

void HabitTrackerActivity::showWeek(const int32_t monday) {
  saveIfDirty();
  viewMonday_ = monday;
  habits::loadWeek(viewMonday_, names_, count_, bits_);
  computeStreaks();
}

void HabitTrackerActivity::saveIfDirty() {
  if (!dirty_) return;
  if (habits::saveWeek(viewMonday_, names_, count_, bits_)) dirty_ = false;
}

void HabitTrackerActivity::computeStreaks() {
  // A streak counts consecutive done days ending today, or yesterday when
  // today is not ticked yet (the day is still in progress).
  const int32_t currentMonday = habits::mondayOf(today_);
  const int todayIdx = tools::weekdayMon0(today_);
  uint8_t week[habits::kMaxHabits];
  if (currentMonday == viewMonday_) {
    std::copy(bits_, bits_ + habits::kMaxHabits, week);
  } else {
    habits::loadWeek(currentMonday, names_, count_, week);
  }
  int32_t endDay[habits::kMaxHabits] = {};
  bool alive[habits::kMaxHabits] = {};
  int aliveCount = count_;
  for (int h = 0; h < count_; ++h) {
    streak_[h] = 0;
    alive[h] = true;
    endDay[h] = (week[h] >> todayIdx) & 1 ? today_ : today_ - 1;
  }
  for (int w = 0; w < kMaxStreakWeeks && aliveCount > 0; ++w) {
    const int32_t monday = currentMonday - 7 * w;
    if (w > 0) {
      if (monday == viewMonday_) {
        std::copy(bits_, bits_ + habits::kMaxHabits, week);
      } else {
        habits::loadWeek(monday, names_, count_, week);
      }
    }
    for (int h = 0; h < count_; ++h) {
      for (int d = 6; d >= 0 && alive[h]; --d) {
        if (monday + d > endDay[h]) continue;
        if ((week[h] >> d) & 1) {
          ++streak_[h];
        } else {
          alive[h] = false;
          --aliveCount;
        }
      }
    }
  }
}

void HabitTrackerActivity::reloadNames() {
  RenderLock lock(*this);
  count_ = habits::loadNames(names_);
  selHabit_ = std::min(selHabit_, count_ - 1);
  habits::loadWeek(viewMonday_, names_, count_, bits_);
  computeStreaks();
}

void HabitTrackerActivity::openMenu() {
  saveIfDirty();
  std::vector<std::string> options;
  options.reserve(4);
  options.emplace_back(std::string(tr(STR_TOOLS_HABIT_STATS)) + ": " + names_[selHabit_]);
  if (count_ < habits::kMaxHabits) options.emplace_back(tr(STR_TOOLS_HABIT_ADD));
  options.emplace_back(std::string(tr(STR_TOOLS_HABIT_RENAME)) + ": " + names_[selHabit_]);
  if (count_ > 1) options.emplace_back(std::string(tr(STR_TOOLS_HABIT_DELETE)) + ": " + names_[selHabit_]);
  const bool canAdd = count_ < habits::kMaxHabits;
  auto picker = makeUniqueNoThrow<OptionSelectionActivity>(renderer, mappedInput, "HabitMenu",
                                                           StrId::STR_TOOLS_HABIT_MENU, std::move(options), 0);
  if (!picker) return;
  startActivityForResult(std::move(picker), [this, canAdd](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    const auto* sel = std::get_if<OptionSelectionResult>(&result.data);
    if (result.isCancelled || sel == nullptr) {
      requestUpdate();
      return;
    }
    int choice = sel->index;
    if (choice == 0) {
      auto stats = makeUniqueNoThrow<HabitStatsActivity>(renderer, mappedInput, names_[selHabit_]);
      if (stats) {
        startActivityForResult(std::move(stats), [this](const ActivityResult&) {
          input_.reset(mappedInput);
          transitionPending_ = true;
          requestUpdate();
        });
      }
      return;
    }
    if (!canAdd) ++choice;  // "Add" row was hidden
    if (choice == 1) {
      addHabit();
    } else if (choice == 2) {
      renameHabit();
    } else {
      deleteHabit();
    }
  });
}

void HabitTrackerActivity::addHabit() {
  auto kb = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_TOOLS_HABIT_NEW_NAME), "",
                                                     habits::kNameCap - 1);
  if (!kb) return;
  startActivityForResult(std::move(kb), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    const auto* text = std::get_if<KeyboardResult>(&result.data);
    if (!result.isCancelled && text != nullptr && !text->text.empty() && text->text.find('|') == std::string::npos &&
        count_ < habits::kMaxHabits) {
      snprintf(names_[count_], habits::kNameCap, "%s", text->text.c_str());
      if (habits::saveNames(names_, count_ + 1)) {
        reloadNames();
        selHabit_ = count_ - 1;
      }
    }
    requestUpdate();
  });
}

void HabitTrackerActivity::renameHabit() {
  auto kb = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_TOOLS_HABIT_RENAME),
                                                     names_[selHabit_], habits::kNameCap - 1);
  if (!kb) return;
  startActivityForResult(std::move(kb), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    const auto* text = std::get_if<KeyboardResult>(&result.data);
    if (!result.isCancelled && text != nullptr && !text->text.empty() && text->text.find('|') == std::string::npos &&
        text->text != names_[selHabit_]) {
      char oldName[habits::kNameCap];
      snprintf(oldName, sizeof(oldName), "%s", names_[selHabit_]);
      snprintf(names_[selHabit_], habits::kNameCap, "%s", text->text.c_str());
      if (habits::saveNames(names_, count_)) {
        // Carry the ticks over so the renamed habit keeps its streak and graph.
        habits::renameInHistory(today_, oldName, names_[selHabit_]);
      }
      reloadNames();
    }
    requestUpdate();
  });
}

void HabitTrackerActivity::deleteHabit() {
  if (count_ <= 1) return;
  char heading[64];
  snprintf(heading, sizeof(heading), "%s: %s", tr(STR_TOOLS_HABIT_DELETE), names_[selHabit_]);
  auto confirm =
      makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, heading, tr(STR_TOOLS_HABIT_DELETE_BODY));
  if (!confirm) return;
  startActivityForResult(std::move(confirm), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    if (!result.isCancelled) {
      for (int i = selHabit_; i + 1 < count_; ++i) memcpy(names_[i], names_[i + 1], habits::kNameCap);
      if (habits::saveNames(names_, count_ - 1)) reloadNames();
    }
    requestUpdate();
  });
}

void HabitTrackerActivity::loop() {
  input_.poll(mappedInput);
  if (input_.backLong) {
    saveIfDirty();
    tools::exitToHome();
    return;
  }
  if (input_.back) {
    finish();
    return;
  }
  if (!clockValid_) return;

  if (input_.up || input_.pageBack) {
    selHabit_ = (selHabit_ + count_ - 1) % count_;
    requestUpdate();
  } else if (input_.down || input_.pageForward) {
    selHabit_ = (selHabit_ + 1) % count_;
    requestUpdate();
  } else if (input_.left) {
    RenderLock lock(*this);  // week bits and streaks are read by render()
    if (selDay_ > 0) {
      --selDay_;
    } else {
      showWeek(viewMonday_ - 7);
      selDay_ = 6;
    }
    requestUpdate();
  } else if (input_.right) {
    RenderLock lock(*this);
    if (selDay_ < 6) {
      ++selDay_;
    } else if (viewMonday_ + 7 <= today_) {
      showWeek(viewMonday_ + 7);
      selDay_ = 0;
    }
    requestUpdate();
  } else if (input_.confirmLong) {
    openMenu();
    return;
  } else if (input_.confirm && !isFuture(selDay_)) {
    RenderLock lock(*this);
    bits_[selHabit_] ^= static_cast<uint8_t>(1U << selDay_);
    habits::logToggle(viewMonday_ + selDay_, names_[selHabit_], (bits_[selHabit_] >> selDay_) & 1);
    dirty_ = true;
    dirtySinceMs_ = millis();
    computeStreaks();
    requestUpdate();
  }

  if (dirty_ && millis() - dirtySinceMs_ >= kSaveDelayMs) saveIfDirty();
}

void HabitTrackerActivity::render(RenderLock&&) {
  char subtitle[40] = "";
  if (clockValid_) {
    uint16_t isoYear = 0;
    uint8_t week = 0;
    tools::isoWeek(viewMonday_, isoYear, week);
    snprintf(subtitle, sizeof(subtitle), "%s %u, %u", tr(STR_TOOLS_WEEK), week, isoYear);
  }
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_HABITS), clockValid_ ? subtitle : nullptr);

  if (!clockValid_) {
    const int midY = content.y + content.height / 2;
    renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_TOOLS_CLOCK_NOT_SET), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, tr(STR_TOOLS_SYNC_FROM_WORLD_CLOCK));
    tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
    renderer.displayBuffer(tools::transitionRefresh());
    transitionPending_ = false;
    return;
  }

  // Column geometry: names | 7 day blocks | streak.
  const int headerH = renderer.getLineHeight(UI_10_FONT_ID) + 6;
  const int nameW = content.width * 30 / 100;
  const int streakW = renderer.getTextWidth(UI_10_FONT_ID, "999d") + 8;
  const int gridX = content.x + nameW;
  const int gridW = content.width - nameW - streakW;
  const int hintH = renderer.getLineHeight(SMALL_FONT_ID) + 4;
  const int rowH = std::min(72, (content.height - headerH - hintH) / count_);
  const int cellW = gridW / 7;
  const int block = std::max(10, std::min(cellW - 8, rowH - 14));

  // Day headers; today's column is bold and underlined. If any name is too
  // wide for its column, every column uses its first letter ("M T W ...").
  bool initialsOnly = false;
  for (int d = 0; d < 7 && !initialsOnly; ++d) {
    const char* name = tools::weekdayShortName(static_cast<uint8_t>(d));
    initialsOnly = renderer.getTextWidth(UI_10_FONT_ID, name, EpdFontFamily::BOLD) > cellW - 4;
  }
  for (int d = 0; d < 7; ++d) {
    const bool isToday = viewMonday_ + d == today_;
    const auto style = isToday ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    char label[8];
    snprintf(label, sizeof(label), "%s", tools::weekdayShortName(static_cast<uint8_t>(d)));
    if (initialsOnly) {
      size_t n = 1;
      while (label[n] != '\0' && (static_cast<uint8_t>(label[n]) & 0xC0) == 0x80) ++n;
      label[n] = '\0';
    }
    const int w = renderer.getTextWidth(UI_10_FONT_ID, label, style);
    const int x = gridX + d * cellW + (cellW - w) / 2;
    renderer.drawText(UI_10_FONT_ID, x, content.y, label, true, style);
    if (isToday) renderer.fillRect(x, content.y + headerH - 4, w, 2);
  }

  for (int h = 0; h < count_; ++h) {
    const int rowY = content.y + headerH + h * rowH;
    const bool selRow = h == selHabit_;
    const int textY = rowY + (rowH - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
    const std::string name = renderer.truncatedText(UI_12_FONT_ID, names_[h], nameW - 8,
                                                    selRow ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    renderer.drawText(UI_12_FONT_ID, content.x, textY, name.c_str(), true,
                      selRow ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);

    for (int d = 0; d < 7; ++d) {
      const int bx = gridX + d * cellW + (cellW - block) / 2;
      const int by = rowY + (rowH - block) / 2;
      const bool done = (bits_[h] >> d) & 1;
      if (done) {
        renderer.fillRect(bx, by, block, block, true);
      } else if (isFuture(d)) {
        renderer.fillRectDither(bx, by, block, block, Color::LightGray);
      } else {
        renderer.drawRect(bx, by, block, block, 2, true);
      }
      if (selRow && d == selDay_) renderer.drawRect(bx - 5, by - 5, block + 10, block + 10, 3, true);
    }

    if (streak_[h] > 0 && viewMonday_ == habits::mondayOf(today_)) {
      char s[8];
      snprintf(s, sizeof(s), "%ud", streak_[h]);
      const int w = renderer.getTextWidth(UI_10_FONT_ID, s, EpdFontFamily::BOLD);
      renderer.drawText(UI_10_FONT_ID, content.x + content.width - w, textY, s, true, EpdFontFamily::BOLD);
    }
  }

  renderer.drawCenteredText(SMALL_FONT_ID, content.y + content.height - hintH + 2, tr(STR_TOOLS_HABIT_HOLD_HINT));
  const char* confirmLabel = isFuture(selDay_) ? "" : tr(STR_TOOLS_TOGGLE);
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), confirmLabel, tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
