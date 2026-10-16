#include "MoodHistoryActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ToolsCharts.h"
#include "ToolsLog.h"
#include "fontIds.h"

namespace {
struct Load {
  MoodHistoryActivity::Entry* entries;  // indexed by day offset, oldest first
  int32_t firstDay;
};

void onLine(const tlog::Stamp& at, char* fields, void* ctx) {
  auto* l = static_cast<Load*>(ctx);
  char* cursor = fields;
  const char* date = tlog::nextField(&cursor);
  const char* mood = tlog::nextField(&cursor);
  int32_t day = 0;
  if (date == nullptr || mood == nullptr || !tools::parseIsoDate(date, day)) return;
  const int idx = day - l->firstDay;
  const int m = atoi(mood);
  if (idx < 0 || idx >= MoodHistoryActivity::kMaxEntries || m < 1 || m > 5) return;
  MoodHistoryActivity::Entry& e = l->entries[idx];  // the latest entry for a day wins
  e.day = day;
  e.mood = static_cast<uint8_t>(m);
  e.minute = day == at.day ? at.minute : -1;
  snprintf(e.note, sizeof(e.note), "%s", cursor != nullptr ? cursor : "");
}

const char* moodName(const int mood) {
  static constexpr StrId kNames[] = {StrId::STR_TOOLS_MOOD_AWFUL, StrId::STR_TOOLS_MOOD_LOW, StrId::STR_TOOLS_MOOD_OKAY,
                                     StrId::STR_TOOLS_MOOD_GOOD, StrId::STR_TOOLS_MOOD_GREAT};
  return I18N.get(kNames[std::clamp(mood, 1, 5) - 1]);
}
}  // namespace

void MoodHistoryActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  tlog::Stamp now;
  count_ = 0;
  if (tlog::now(now)) {
    Load load{entries_, now.day - (kMaxEntries - 1)};
    tlog::scan("mood", load.firstDay, now.day, &onLine, &load);
    // Drop days without an entry (in place, oldest first), then newest first.
    for (int i = 0; i < kMaxEntries; ++i) {
      if (entries_[i].mood != 0) entries_[count_++] = entries_[i];
    }
    std::reverse(entries_, entries_ + count_);
  }
  transitionPending_ = true;
  requestUpdate();
}

void MoodHistoryActivity::loop() {
  input_.poll(mappedInput);
  if (input_.backLong) {
    tools::exitToHome();
    return;
  }
  if (input_.back) {
    finish();
    return;
  }
  const int pages = std::max(1, (count_ + perPage_ - 1) / perPage_);
  if ((input_.left || input_.up || input_.pageBack) && page_ > 0) {
    --page_;
    requestUpdate();
  } else if ((input_.right || input_.down || input_.pageForward) && page_ + 1 < pages) {
    ++page_;
    requestUpdate();
  }
}

void MoodHistoryActivity::render(RenderLock&&) {
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_MOOD_HISTORY));
  const int titleH = renderer.getLineHeight(UI_10_FONT_ID);
  const int noteH = renderer.getLineHeight(SMALL_FONT_ID);
  const int rowH = titleH + noteH + 12;
  const int footH = noteH + 4;
  perPage_ = std::max(1, (content.height - footH) / rowH);
  const int pages = std::max(1, (count_ + perPage_ - 1) / perPage_);
  page_ = std::min(page_, pages - 1);
  if (count_ == 0) {
    renderer.drawCenteredText(UI_12_FONT_ID, content.y + content.height / 2 - 20, tr(STR_TOOLS_MOOD_NO_HISTORY), true,
                              EpdFontFamily::BOLD);
  }
  int y = content.y;
  for (int i = page_ * perPage_; i < count_ && i < (page_ + 1) * perPage_; ++i) {
    const Entry& e = entries_[i];
    char date[20];
    tools::formatShortDate(date, sizeof(date), e.day);
    char when[12] = "";
    if (e.minute >= 0) charts::formatMinute(when, sizeof(when), e.minute);
    char head[64];
    snprintf(head, sizeof(head), "%s %s%s%s", tools::weekdayShortName(tools::weekdayMon0(e.day)), date,
             e.minute >= 0 ? "  " : "", when);
    renderer.drawText(UI_10_FONT_ID, content.x, y, head);
    const char* mood = moodName(e.mood);
    const int mw = renderer.getTextWidth(UI_10_FONT_ID, mood, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, content.x + content.width - mw, y, mood, true, EpdFontFamily::BOLD);
    // Mood as a small five-step bar next to the name.
    const int barX = content.x + content.width - mw - 12 - 5 * 8;
    for (int k = 0; k < 5; ++k) {
      const int bx = barX + k * 8;
      if (k < e.mood) {
        renderer.fillRect(bx, y + 4, 6, titleH - 8);
      } else {
        renderer.drawRect(bx, y + 4, 6, titleH - 8);
      }
    }
    if (e.note[0] != '\0') {
      const auto note = renderer.truncatedText(SMALL_FONT_ID, e.note, content.width);
      renderer.drawText(SMALL_FONT_ID, content.x, y + titleH + 1, note.c_str());
    }
    y += rowH;
    for (int x = content.x; x < content.x + content.width; x += 3) renderer.drawPixel(x, y - 6);
  }
  char foot[32];
  snprintf(foot, sizeof(foot), "%s %d / %d", tr(STR_TOOLS_PAGE), page_ + 1, pages);
  renderer.drawCenteredText(SMALL_FONT_ID, content.y + content.height - footH + 2, foot);
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", page_ > 0 ? tr(STR_TOOLS_PREV_PAGE) : "",
                   page_ + 1 < pages ? tr(STR_TOOLS_NEXT_PAGE) : "");
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
