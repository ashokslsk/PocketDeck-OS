#include "ToolsMenuActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "DailyCommandCenterActivity.h"
#include "DailyQuoteActivity.h"
#include "DictionaryToolActivity.h"
#include "HabitTrackerActivity.h"
#include "MantrasActivity.h"
#include "MedicineActivity.h"
#include "MoodActivity.h"
#include "PanchangaActivity.h"
#include "PomodoroActivity.h"
#include "StatsActivity.h"
#include "WorkoutActivity.h"
#include "WorldClockActivity.h"
#include "activities/home/SavedItemsHomeActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/ButtonNavigator.h"

const char* ToolsMenuActivity::toolLabel(const int index) {
  switch (index) {
    case POMODORO:
      return tr(STR_TOOLS_POMODORO);
    case WORLD_CLOCK:
      return tr(STR_TOOLS_WORLD_CLOCK);
    case HABITS:
      return tr(STR_TOOLS_HABITS);
    case MEDICINE:
      return tr(STR_TOOLS_MEDICINE);
    case MOOD:
      return tr(STR_TOOLS_MOOD);
    case QUOTE:
      return tr(STR_TOOLS_DAILY_QUOTE);
    case PANCHANGA:
      return tr(STR_TOOLS_PANCHANGA);
    case MANTRAS:
      return tr(STR_TOOLS_MANTRAS);
    case WORKOUT:
      return tr(STR_TOOLS_WORKOUT);
    case STATS:
      return tr(STR_TOOLS_STATS);
    case DICTIONARY:
      return tr(STR_TOOLS_DICTIONARY);
    case SAVED:
      return tr(STR_BOOKMARKS_AND_CLIPPINGS);
    default:
      return tr(STR_TOOLS_TODAY);
  }
}

const char* ToolsMenuActivity::toolDescription(const int index) {
  switch (index) {
    case POMODORO:
      return tr(STR_TOOLS_POMODORO_DESC);
    case WORLD_CLOCK:
      return tr(STR_TOOLS_WORLD_CLOCK_DESC);
    case HABITS:
      return tr(STR_TOOLS_HABITS_DESC);
    case MEDICINE:
      return tr(STR_TOOLS_MEDICINE_DESC);
    case MOOD:
      return tr(STR_TOOLS_MOOD_DESC);
    case QUOTE:
      return tr(STR_TOOLS_DAILY_QUOTE_DESC);
    case PANCHANGA:
      return tr(STR_TOOLS_PANCHANGA_DESC);
    case MANTRAS:
      return tr(STR_TOOLS_MANTRAS_DESC);
    case WORKOUT:
      return tr(STR_TOOLS_WORKOUT_DESC);
    case STATS:
      return tr(STR_TOOLS_STATS_DESC);
    case DICTIONARY:
      return tr(STR_TOOLS_DICTIONARY_DESC);
    case SAVED:
      return tr(STR_TOOLS_SAVED_DESC);
    default:
      return tr(STR_TOOLS_TODAY_DESC);
  }
}

void ToolsMenuActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  transitionPending_ = true;
  tools::ensureToolsDirs();
  requestUpdate();
}

void ToolsMenuActivity::openTool(const int index) {
  std::unique_ptr<Activity> tool;
  switch (index) {
    case POMODORO:
      tool = makeUniqueNoThrow<PomodoroActivity>(renderer, mappedInput);
      break;
    case WORLD_CLOCK:
      tool = makeUniqueNoThrow<WorldClockActivity>(renderer, mappedInput);
      break;
    case HABITS:
      tool = makeUniqueNoThrow<HabitTrackerActivity>(renderer, mappedInput);
      break;
    case MEDICINE:
      tool = makeUniqueNoThrow<MedicineActivity>(renderer, mappedInput);
      break;
    case MOOD:
      tool = makeUniqueNoThrow<MoodActivity>(renderer, mappedInput);
      break;
    case QUOTE:
      tool = makeUniqueNoThrow<DailyQuoteActivity>(renderer, mappedInput);
      break;
    case PANCHANGA:
      tool = makeUniqueNoThrow<PanchangaActivity>(renderer, mappedInput);
      break;
    case MANTRAS:
      tool = makeUniqueNoThrow<MantrasActivity>(renderer, mappedInput);
      break;
    case WORKOUT:
      tool = makeUniqueNoThrow<WorkoutActivity>(renderer, mappedInput);
      break;
    case STATS:
      tool = makeUniqueNoThrow<StatsActivity>(renderer, mappedInput);
      break;
    case DICTIONARY:
      tool = makeUniqueNoThrow<DictionaryToolActivity>(renderer, mappedInput);
      break;
    case SAVED:
      tool = makeUniqueNoThrow<SavedItemsHomeActivity>(renderer, mappedInput);
      break;
    default:
      tool = makeUniqueNoThrow<DailyCommandCenterActivity>(renderer, mappedInput);
      break;
  }
  if (!tool) {
    LOG_ERR("TOOLS", "OOM opening tool %d (free=%u maxAlloc=%u)", index, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    return;
  }
  startActivityForResult(std::move(tool), [this](const ActivityResult&) {
    // Leaving a tool is a module transition: repaint cleanly and ignore the
    // release of the Back press that closed it.
    transitionPending_ = true;
    input_.reset(mappedInput);
  });
}

void ToolsMenuActivity::loop() {
  input_.poll(mappedInput);
  if (input_.back || input_.backLong) {
    tools::exitToHome();
    return;
  }
  if (input_.prev()) {
    selected_ = ButtonNavigator::previousIndex(selected_, TOOL_COUNT);
    requestUpdate();
  } else if (input_.next()) {
    selected_ = ButtonNavigator::nextIndex(selected_, TOOL_COUNT);
    requestUpdate();
  } else if (input_.confirm) {
    openTool(selected_);
  }
}

void ToolsMenuActivity::render(RenderLock&&) {
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS));
  // Tool names in 14 pt bold (the UI font tops out at 12 pt), the one-line
  // description beneath in the regular UI font; a page at a time.
  constexpr int kTitleFont = LEXENDDECA_14_FONT_ID;
  const int titleH = renderer.getLineHeight(kTitleFont);
  const int descH = renderer.getLineHeight(UI_10_FONT_ID);
  const int pageLabelH = renderer.getLineHeight(SMALL_FONT_ID) + 4;
  const int rowH = titleH + descH + 16;
  const int perPage = std::max(1, (content.height - pageLabelH) / rowH);
  const int pages = (TOOL_COUNT + perPage - 1) / perPage;
  const int first = (selected_ / perPage) * perPage;
  const int last = std::min<int>(TOOL_COUNT, first + perPage);
  const int x = content.x;
  const int w = content.width;
  const int textX = x + 14;
  const int textW = w - 28;
  for (int i = first; i < last; ++i) {
    const int y = content.y + (i - first) * rowH;
    const bool selected = i == selected_;
    if (selected) {
      renderer.fillRoundedRect(x, y + 3, w, rowH - 6, 10, Color::Black);
    } else if (i > first && i - 1 != selected_) {
      renderer.drawLine(textX, y, x + w - 14, y, true);
    }
    const std::string title = renderer.truncatedText(kTitleFont, toolLabel(i), textW, EpdFontFamily::BOLD);
    renderer.drawText(kTitleFont, textX, y + 8, title.c_str(), !selected, EpdFontFamily::BOLD);
    const std::string desc = renderer.truncatedText(UI_10_FONT_ID, toolDescription(i), textW);
    renderer.drawText(UI_10_FONT_ID, textX, y + 8 + titleH, desc.c_str(), !selected);
  }
  if (pages > 1) {
    char page[16];
    snprintf(page, sizeof(page), "%d / %d", selected_ / perPage + 1, pages);
    const int pw = renderer.getTextWidth(SMALL_FONT_ID, page);
    renderer.drawText(SMALL_FONT_ID, x + w - pw, content.y + content.height - pageLabelH + 2, page, true);
  }
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
