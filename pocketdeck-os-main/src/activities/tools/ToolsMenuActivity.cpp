#include "ToolsMenuActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <string>

#include "DailyCommandCenterActivity.h"
#include "DailyQuoteActivity.h"
#include "FlashcardsActivity.h"
#include "HabitTrackerActivity.h"
#include "KnowledgeCardActivity.h"
#include "MantrasActivity.h"
#include "MedicineActivity.h"
#include "MoodActivity.h"
#include "PanchangaActivity.h"
#include "PomodoroActivity.h"
#include "StatsActivity.h"
#include "WorldClockActivity.h"
#include "components/UITheme.h"
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
    case FLASHCARDS:
      return tr(STR_TOOLS_FLASHCARDS);
    case QUOTE:
      return tr(STR_TOOLS_DAILY_QUOTE);
    case KNOWLEDGE:
      return tr(STR_TOOLS_KNOWLEDGE);
    case PANCHANGA:
      return tr(STR_TOOLS_PANCHANGA);
    case MANTRAS:
      return tr(STR_TOOLS_MANTRAS);
    case STATS:
      return tr(STR_TOOLS_STATS);
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
    case FLASHCARDS:
      return tr(STR_TOOLS_FLASHCARDS_DESC);
    case QUOTE:
      return tr(STR_TOOLS_DAILY_QUOTE_DESC);
    case KNOWLEDGE:
      return tr(STR_TOOLS_KNOWLEDGE_DESC);
    case PANCHANGA:
      return tr(STR_TOOLS_PANCHANGA_DESC);
    case MANTRAS:
      return tr(STR_TOOLS_MANTRAS_DESC);
    case STATS:
      return tr(STR_TOOLS_STATS_DESC);
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
    case FLASHCARDS:
      tool = makeUniqueNoThrow<FlashcardsActivity>(renderer, mappedInput);
      break;
    case QUOTE:
      tool = makeUniqueNoThrow<DailyQuoteActivity>(renderer, mappedInput);
      break;
    case KNOWLEDGE:
      tool = makeUniqueNoThrow<KnowledgeCardActivity>(renderer, mappedInput);
      break;
    case PANCHANGA:
      tool = makeUniqueNoThrow<PanchangaActivity>(renderer, mappedInput);
      break;
    case MANTRAS:
      tool = makeUniqueNoThrow<MantrasActivity>(renderer, mappedInput);
      break;
    case STATS:
      tool = makeUniqueNoThrow<StatsActivity>(renderer, mappedInput);
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
  GUI.drawList(
      renderer, Rect{0, content.y, renderer.getScreenWidth(), content.height}, TOOL_COUNT, selected_,
      [](const int i) { return std::string(toolLabel(i)); },
      [](const int i) { return std::string(toolDescription(i)); });
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
