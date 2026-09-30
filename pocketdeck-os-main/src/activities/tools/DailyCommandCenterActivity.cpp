#include "DailyCommandCenterActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <variant>

#include "HabitData.h"
#include "PomodoroActivity.h"
#include "ToolStatsPages.h"
#include "ToolsLog.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

constexpr char DailyCommandCenterActivity::kDataPath[];

namespace {
using Tok = tools::JsonReader::Token;
// The clock shows minutes; a 5 s check is enough and saves wake-ups.
constexpr unsigned long kPollMs = 5000;
}  // namespace

void DailyCommandCenterActivity::load() {
  count_ = 0;
  dataDay_ = 0;
  tools::recoverFromBackup(kDataPath);
  FsFile f;
  if (!Storage.exists(kDataPath) || !Storage.openFileForRead("TODAY", kDataPath, f)) return;

  tools::JsonReader r(f);
  char text[kTextCap];
  if (r.next(text, sizeof(text)) != Tok::ObjectStart) {
    LOG_ERR("TODAY", "%s is not a JSON object", kDataPath);
    f.close();
    return;
  }
  while (true) {
    Tok t = r.next(text, sizeof(text));
    if (t == Tok::Comma) continue;
    if (t != Tok::String) break;  // ObjectEnd, End or Error
    char key[16];
    snprintf(key, sizeof(key), "%s", text);
    if (r.next(nullptr, 0) != Tok::Colon) break;
    t = r.next(text, sizeof(text));
    if (strcmp(key, "date") == 0 && t == Tok::String) {
      tools::parseIsoDate(text, dataDay_);
    } else if (strcmp(key, "todos") == 0 && t == Tok::ArrayStart) {
      while (true) {
        t = r.next(nullptr, 0);
        if (t == Tok::Comma) continue;
        if (t != Tok::ObjectStart) break;
        Todo item{};
        while (true) {
          t = r.next(text, sizeof(text));
          if (t == Tok::Comma) continue;
          if (t != Tok::String) break;
          const bool isText = strcmp(text, "text") == 0;
          const bool isDone = strcmp(text, "done") == 0;
          if (r.next(nullptr, 0) != Tok::Colon) break;
          t = r.next(text, sizeof(text));
          if (isText && t == Tok::String) snprintf(item.text, sizeof(item.text), "%s", text);
          if (isDone && t == Tok::Literal) item.done = strcmp(text, "true") == 0;
          if (!isText && !isDone) r.skipValue(t);
        }
        if (item.text[0] != '\0' && count_ < kMaxTodos) todos_[count_++] = item;
      }
    } else {
      r.skipValue(t);
    }
  }
  f.close();
}

bool DailyCommandCenterActivity::writeJson(FsFile& out, void* ctx) {
  const auto* self = static_cast<const DailyCommandCenterActivity*>(ctx);
  char date[12];
  tools::formatIsoDate(date, sizeof(date), self->dataDay_);
  bool ok = tools::writeText(out, "{\n  \"date\": \"") && tools::writeText(out, date) &&
            tools::writeText(out, "\",\n  \"todos\": [");
  for (int i = 0; ok && i < self->count_; ++i) {
    ok = tools::writeText(out, i == 0 ? "\n    {\"text\": " : ",\n    {\"text\": ") &&
         tools::writeJsonString(out, self->todos_[i].text) &&
         tools::writeText(out, self->todos_[i].done ? ", \"done\": true}" : ", \"done\": false}");
  }
  return ok && tools::writeText(out, "\n  ]\n}\n");
}

void DailyCommandCenterActivity::save() {
  if (!dirty_) return;
  if (tools::writeFileAtomic(kDataPath, &DailyCommandCenterActivity::writeJson, this)) {
    dirty_ = false;
  } else {
    LOG_ERR("TODAY", "Failed to save %s", kDataPath);
  }
}

void DailyCommandCenterActivity::markDirty() {
  dirty_ = true;
  dirtySinceMs_ = millis();
}

void DailyCommandCenterActivity::rollOverDay(const int32_t today) {
  if (today == 0 || dataDay_ == today) return;
  // History for Stats & export: how the finished day went.
  if (dataDay_ != 0 && count_ > 0) {
    int done = 0;
    for (int i = 0; i < count_; ++i) done += todos_[i].done ? 1 : 0;
    tlog::Stamp at;
    if (tlog::now(at)) {
      char fields[40];
      char date[12];
      tools::formatIsoDate(date, sizeof(date), dataDay_);
      snprintf(fields, sizeof(fields), "%s|%d|%d", date, done, count_);
      tlog::append("today", at, fields);
    }
  }
  // New day: drop what was finished before today, keep what is still open.
  int kept = 0;
  for (int i = 0; i < count_; ++i) {
    if (!todos_[i].done) todos_[kept++] = todos_[i];
  }
  count_ = kept;
  dataDay_ = today;
  markDirty();
}

void DailyCommandCenterActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  load();
  tools::DateTime now;
  if (tools::getLocalNow(now)) {
    const int32_t today = tools::daysOf(now);
    rollOverDay(today);
    habitsDone_ = habits::countDoneOn(today, &habitCount_);
    focusToday_ = PomodoroActivity::completedOn(today);
  }
  selected_ = 0;
  top_ = 0;
  transitionPending_ = true;
  requestUpdate();
}

void DailyCommandCenterActivity::onExit() {
  save();
  Activity::onExit();
}

void DailyCommandCenterActivity::addTodo(const char* text) {
  if (count_ >= kMaxTodos || text == nullptr || text[0] == '\0') return;
  Todo& item = todos_[count_++];
  snprintf(item.text, sizeof(item.text), "%s", text);
  item.done = false;
  markDirty();
}

void DailyCommandCenterActivity::removeTodo(const int index) {
  if (index < 0 || index >= count_) return;
  for (int i = index; i + 1 < count_; ++i) todos_[i] = todos_[i + 1];
  --count_;
  markDirty();
}

void DailyCommandCenterActivity::clearCompleted() {
  int kept = 0;
  for (int i = 0; i < count_; ++i) {
    if (!todos_[i].done) todos_[kept++] = todos_[i];
  }
  if (kept != count_) {
    count_ = kept;
    markDirty();
  }
}

void DailyCommandCenterActivity::openAddTask() {
  if (count_ >= kMaxTodos) return;
  auto keyboard =
      makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_TOOLS_NEW_TASK), "", kTextCap - 1);
  if (!keyboard) {
    LOG_ERR("TODAY", "OOM creating keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    if (!result.isCancelled) {
      if (const auto* kb = std::get_if<KeyboardResult>(&result.data)) addTodo(kb->text.c_str());
    }
    selected_ = std::max(0, count_ - 1);
    input_.reset(mappedInput);
  });
}

void DailyCommandCenterActivity::loop() {
  input_.poll(mappedInput);
  if (input_.backLong) {
    save();
    tools::exitToHome();
    return;
  }
  if (input_.back) {
    finish();
    return;
  }

  const int rows = rowCount();
  if (input_.leftUp) {
    auto stats = makeUniqueNoThrow<ToolStatsActivity>(renderer, mappedInput, tr(STR_TOOLS_TD_STATS),
                                                      &toolstats::buildToday, nullptr, statsx::Feature::Today);
    if (stats) {
      save();
      startActivityForResult(std::move(stats), [this](const ActivityResult&) {
        input_.reset(mappedInput);
        transitionPending_ = true;
        requestUpdate();
      });
    }
    return;
  }
  if (input_.up || input_.pageBack) {
    selected_ = (selected_ + rows - 1) % rows;
    requestUpdate();
  } else if (input_.next()) {
    selected_ = (selected_ + 1) % rows;
    requestUpdate();
  } else if (input_.confirm) {
    RenderLock lock(*this);  // todos_ is read by render()
    if (selected_ < count_) {
      todos_[selected_].done = !todos_[selected_].done;
      markDirty();
    } else if (selected_ == count_) {
      lock.unlock();
      openAddTask();
      return;
    } else {
      clearCompleted();
      selected_ = std::min(selected_, rowCount() - 1);
    }
    requestUpdate();
  } else if (input_.confirmLong && selected_ < count_) {
    RenderLock lock(*this);
    removeTodo(selected_);
    selected_ = std::min(selected_, rowCount() - 1);
    requestUpdate();
  }

  if (dirty_ && millis() - dirtySinceMs_ >= kSaveDelayMs) save();

  const unsigned long now = millis();
  if (now - lastPollMs_ >= kPollMs) {
    lastPollMs_ = now;
    tools::DateTime local;
    if (tools::getLocalNow(local)) {
      const int minute = local.hour * 60 + local.minute;
      if (minute != lastMinute_) {
        const int32_t today = tools::daysOf(local);
        if (today != dataDay_) {
          RenderLock lock(*this);
          rollOverDay(today);
          habitsDone_ = habits::countDoneOn(today, &habitCount_);
          focusToday_ = PomodoroActivity::completedOn(today);
        }
        requestUpdate();
      }
    }
  }
}

void DailyCommandCenterActivity::render(RenderLock&&) {
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_TODAY));
  int y = content.y;

  tools::DateTime local;
  if (tools::getLocalNow(local)) {
    lastMinute_ = local.hour * 60 + local.minute;
    // The digit font only has numerals; 12-hour mode appends AM/PM text.
    char digits[8];
    const bool twelveHour = SETTINGS.clockFormat == 1;
    const int shownHour = twelveHour ? (local.hour % 12 == 0 ? 12 : local.hour % 12) : local.hour;
    snprintf(digits, sizeof(digits), twelveHour ? "%d:%02d" : "%02d:%02d", shownHour, local.minute);
    const int digitH = tools::drawDigits(renderer, TOOLS_DIGITS_30_FONT_ID, content.x, y, digits);
    const int digitsW = renderer.getTextWidth(TOOLS_DIGITS_30_FONT_ID, digits);
    if (twelveHour) {
      renderer.drawText(UI_10_FONT_ID, content.x + digitsW + 6, y + digitH - renderer.getLineHeight(UI_10_FONT_ID),
                        local.hour >= 12 ? tr(STR_PM) : tr(STR_AM), true, EpdFontFamily::BOLD);
    }

    // Weekday and date stacked to the right of the clock.
    const int rightX = content.x + content.width;
    const char* weekday = tools::weekdayName(tools::weekdayMon0(tools::daysOf(local)));
    char date[32];
    tools::formatLongDate(date, sizeof(date), local);
    const int wdW = renderer.getTextWidth(UI_12_FONT_ID, weekday, EpdFontFamily::BOLD);
    const int dateW = renderer.getTextWidth(UI_10_FONT_ID, date);
    renderer.drawText(UI_12_FONT_ID, rightX - wdW, y, weekday, true, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, rightX - dateW, y + renderer.getLineHeight(UI_12_FONT_ID) + 2, date);
    y += digitH + 12;
  } else {
    renderer.drawText(UI_12_FONT_ID, content.x, y, tr(STR_TOOLS_CLOCK_NOT_SET), true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_12_FONT_ID) + 8;
  }

  char summary[96];
  snprintf(summary, sizeof(summary), "%s %d/%d    %s %u", tr(STR_TOOLS_HABITS_LABEL), habitsDone_, habitCount_,
           tr(STR_TOOLS_FOCUS_LABEL), focusToday_);
  renderer.drawText(UI_10_FONT_ID, content.x, y, summary);
  y += renderer.getLineHeight(UI_10_FONT_ID) + 6;
  renderer.drawLine(content.x, y, content.x + content.width, y);
  y += 6;

  // To-do rows.
  const int rowH = renderer.getLineHeight(UI_12_FONT_ID) + 12;
  // A task row reserves one small line at the bottom for its hold hint.
  const int hintH = count_ > 0 ? renderer.getLineHeight(SMALL_FONT_ID) + 4 : 0;
  visibleRows_ = std::max(1, (content.y + content.height - hintH - y) / rowH);
  const int rows = rowCount();
  if (selected_ < top_) top_ = selected_;
  if (selected_ >= top_ + visibleRows_) top_ = selected_ - visibleRows_ + 1;
  const int box = renderer.getLineHeight(UI_12_FONT_ID) - 6;
  for (int i = top_; i < rows && i < top_ + visibleRows_; ++i) {
    const int rowY = y + (i - top_) * rowH;
    const bool selected = i == selected_;
    if (selected) renderer.fillRect(content.x - 6, rowY, content.width + 12, rowH - 2, true);
    const bool ink = !selected;
    const int textY = rowY + 6;
    if (i < count_) {
      const Todo& item = todos_[i];
      const int boxY = textY + 3;
      renderer.drawRect(content.x, boxY, box, box, 2, ink);
      if (item.done) {
        // Check mark drawn with two thick strokes.
        renderer.drawLine(content.x + 3, boxY + box / 2, content.x + box / 2 - 1, boxY + box - 4, 3, ink);
        renderer.drawLine(content.x + box / 2 - 1, boxY + box - 4, content.x + box - 3, boxY + 3, 3, ink);
      }
      const int textX = content.x + box + 12;
      const std::string shown =
          renderer.truncatedText(UI_12_FONT_ID, item.text, content.x + content.width - textX, EpdFontFamily::REGULAR);
      renderer.drawText(UI_12_FONT_ID, textX, textY, shown.c_str(), ink);
      if (item.done) {
        const int w = renderer.getTextWidth(UI_12_FONT_ID, shown.c_str());
        const int midY = textY + renderer.getFontAscenderSize(UI_12_FONT_ID) * 2 / 3;
        renderer.drawLine(textX, midY, textX + w, midY, 2, ink);
      }
    } else if (i == count_) {
      const char* label = count_ >= kMaxTodos ? tr(STR_TOOLS_LIST_FULL) : tr(STR_TOOLS_ADD_TASK);
      renderer.drawText(UI_12_FONT_ID, content.x, textY, label, ink, EpdFontFamily::BOLD);
    } else {
      renderer.drawText(UI_12_FONT_ID, content.x, textY, tr(STR_TOOLS_CLEAR_COMPLETED), ink);
    }
  }
  if (count_ == 0) {
    renderer.drawText(UI_10_FONT_ID, content.x, y + 2 * rowH + 6, tr(STR_TOOLS_NO_TASKS));
  }

  if (selected_ < count_) {
    renderer.drawCenteredText(SMALL_FONT_ID, content.y + content.height - hintH + 2, tr(STR_TOOLS_TD_HOLD_DELETE));
  }
  const char* confirmLabel = selected_ < count_ ? tr(STR_TOOLS_TOGGLE) : tr(STR_SELECT);
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), confirmLabel, tr(STR_TOOLS_STATS_SHORT), tr(STR_TOOLS_NEXT));
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
