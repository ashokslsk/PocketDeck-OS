#include "DictionaryToolActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "activities/reader/DictionaryDefinitionActivity.h"
#include "activities/reader/KannadaReading.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "fontIds.h"
#include "util/Clipboard.h"
#include "util/Dictionary.h"
#include "util/DictionaryActivityUtils.h"

namespace fui = freeink::ui;

namespace {
constexpr char kToolPath[] = "/tools/dictionary";
constexpr size_t kSearchMax = 64;

const char* glyphFor(const LookupHistory::Status s) {
  switch (s) {
    case LookupHistory::Status::Direct:
      return "\xe2\x88\x9a";  // √
    case LookupHistory::Status::Stem:
    case LookupHistory::Status::AltForm:
      return "~";
    case LookupHistory::Status::NotFound:
      return "\xc3\x97";  // ×
    default:
      return "?";
  }
}
}  // namespace

DictionaryToolActivity::DictionaryToolActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("DictionaryTool", renderer, mappedInput),
      cachePath(kToolPath),
      controller(renderer, mappedInput, *this, cachePath),
      uiTarget(makeUiTarget(renderer)),
      app(uiTarget, uiTarget.deviceContext()) {}

void DictionaryToolActivity::onEnter() {
  Activity::onEnter();
  Storage.mkdir(kToolPath);
  kannadareading::beginUi();  // Kannada words in the list render shaped
  selectedIndex = 0;
  topIndex = 0;
  visibleRows = 1;
  uiReady = false;
  applySharedUiTheme(app, uiTarget);
  app.on(ACTION_ROW, &DictionaryToolActivity::onRowEvent, this);
  app.setScreen(&DictionaryToolActivity::listScreen, this);
  reload();
  requestUpdate();
}

void DictionaryToolActivity::onExit() {
  controller.onExit();
  // A dictionary picked with "Switch Dictionary" applies to this tool only.
  Dictionary::clearLookupDictPathOverride();
  kannadareading::endUi();
  Activity::onExit();
}

void DictionaryToolActivity::reload() {
  copies = clipboard::entries();
  history = LookupHistory::load(cachePath);
  rows.clear();
  labels.clear();
  rows.reserve(1 + copies.size() + history.size());
  labels.reserve(rows.capacity());
  rows.push_back({RowKind::Search, 0});
  labels.push_back(std::string(tr(STR_DICT_TOOL_SEARCH)));
  for (size_t i = 0; i < copies.size(); ++i) {
    rows.push_back({RowKind::Copied, static_cast<uint16_t>(i)});
    labels.push_back(std::string(tr(STR_DICT_TOOL_COPIED)) + " \xc2\xb7\xc2\xa0" + copies[i]);
  }
  for (size_t i = 0; i < history.size(); ++i) {
    rows.push_back({RowKind::History, static_cast<uint16_t>(i)});
    labels.push_back(std::string(glyphFor(history[i].status)) + " " + history[i].word);
  }
  uiItems.clear();
  uiItems.reserve(rows.size());
  for (size_t i = 0; i < rows.size(); ++i) {
    fui::ListItem item;
    item.label = labels[i].c_str();
    item.actionValue = static_cast<int16_t>(i);
    uiItems.push_back(item);
  }
  if (selectedIndex >= static_cast<int>(rows.size())) selectedIndex = static_cast<int>(rows.size()) - 1;
  topIndex = scrollListBy(topIndex, 0, visibleRows, static_cast<int>(rows.size()));
}

std::string DictionaryToolActivity::dictionaryName() const {
  const std::string path = Dictionary::readDictPath(cachePath.c_str());
  if (path.empty()) return std::string();
  const size_t last = path.find_last_of('/');
  if (last == std::string::npos || last == 0) return path;
  const size_t prev = path.find_last_of('/', last - 1);
  return path.substr(prev == std::string::npos ? 0 : prev + 1, last - (prev == std::string::npos ? 0 : prev + 1));
}

void DictionaryToolActivity::listScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<DictionaryToolActivity*>(user)->buildListScreen(screen);
}

void DictionaryToolActivity::buildListScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) +
                                       metrics.verticalSpacing),
                  0, static_cast<int16_t>(metrics.buttonHintsHeight + metrics.verticalSpacing), 0});
  fui::ListProps props;
  props.items = uiItems.data();
  props.count = static_cast<uint16_t>(uiItems.size());
  props.selectedIndex = static_cast<int16_t>(selectedIndex);
  props.topIndex = static_cast<uint16_t>(topIndex);
  props.action = ACTION_ROW;
  props.inputMask = static_cast<uint16_t>(fui::InputTouch | fui::InputLongPress);
  props.labelText = screen.theme().bodyText;
  props.labelText.maxLines = 2;
  const auto visible = configureUiList(props, screen.theme(), screen.body());
  visibleRows = visible > 0 ? visible : 1;
  topIndex = scrollListBy(topIndex, 0, visibleRows, static_cast<int>(uiItems.size()));
  props.topIndex = static_cast<uint16_t>(topIndex);
  screen.list(props);
}

void DictionaryToolActivity::onRowEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<DictionaryToolActivity*>(user);
  if (event.value < 0 || event.value >= static_cast<int16_t>(self->rows.size())) return;
  self->selectedIndex = event.value;
  self->app.clearTapFlash();
  if (event.longPress) {
    if (self->rows[self->selectedIndex].kind == RowKind::History) self->confirmDeleteHistory(false);
    return;
  }
  self->activate(self->selectedIndex);
}

void DictionaryToolActivity::activate(const int rowIndex) {
  if (rowIndex < 0 || rowIndex >= static_cast<int>(rows.size())) return;
  const Row row = rows[rowIndex];
  switch (row.kind) {
    case RowKind::Search:
      openSearch();
      return;
    case RowKind::Copied:
      if (row.index < copies.size()) lookUp(copies[row.index]);
      return;
    case RowKind::History:
      if (row.index < history.size()) controller.startLookup(history[row.index].word);
      return;
  }
}

void DictionaryToolActivity::openSearch() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_DICT_TOOL_SEARCH_TITLE), "",
                                                           kSearchMax, InputType::Text, 1);
  if (!keyboard) {
    LOG_ERR("DICTOOL", "OOM: search keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    const auto* typed = std::get_if<KeyboardResult>(&result.data);
    if (result.isCancelled || !typed || typed->text.empty()) {
      requestUpdate();
      return;
    }
    lookUp(typed->text);
  });
}

void DictionaryToolActivity::lookUp(const std::string& text) {
  // Cleans punctuation and handles a copied phrase the same way the reader does.
  const size_t words = 1 + static_cast<size_t>(std::count(text.begin(), text.end(), ' '));
  controller.lookupOrPopup(text, words);
}

void DictionaryToolActivity::confirmDeleteHistory(const bool ignoreInitialConfirmRelease) {
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(rows.size())) return;
  const Row row = rows[selectedIndex];
  if (row.kind != RowKind::History || row.index >= history.size()) return;
  auto confirmation =
      makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, std::string(tr(STR_DELETE)) + "?",
                                              history[row.index].word, ignoreInitialConfirmRelease, true);
  if (!confirmation) return;
  const int historyIndex = row.index;
  startActivityForResult(std::move(confirmation), [this, historyIndex](const ActivityResult& result) {
    if (!result.isCancelled) {
      LookupHistory::removeRecentAt(cachePath, historyIndex);
      reload();
    }
    requestUpdate();
  });
}

void DictionaryToolActivity::loop() {
  if (controller.isActive()) {
    switch (controller.handleInput()) {
      case DictionaryLookupController::LookupEvent::FoundDefinition: {
        auto definition = makeUniqueNoThrow<DictionaryDefinitionActivity>(
            renderer, mappedInput, controller.getFoundWord(), controller.getFoundLocation(), true, cachePath,
            controller.getRecordHistory(), controller.getLookupWord(),
            DictionaryLookupController::toHistStatus(controller.getFoundStatus()));
        if (!definition) {
          LOG_ERR("DICTOOL", "OOM: DictionaryDefinitionActivity");
          requestUpdate();
          break;
        }
        startActivityForResult(std::move(definition), [this](const ActivityResult&) {
          reload();
          requestUpdate();
        });
        break;
      }
      case DictionaryLookupController::LookupEvent::SwitchDictionary:
      case DictionaryLookupController::LookupEvent::NotFoundDismissedBack:
      case DictionaryLookupController::LookupEvent::NotFoundDismissedDone:
      case DictionaryLookupController::LookupEvent::Cancelled:
        reload();
        requestUpdate();
        break;
      default:
        break;
    }
    return;
  }

  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    DictUtils::cancelAndFinish(*this);
    return;
  }

  if (uiReady) {
    const fui::InputSnapshot snap = touchSnapshotFrom(mappedInput);
    if (snap.touchPressed || snap.touchReleased) {
      const auto event = app.route(snap);
      if (app.invalidated()) requestUpdate();
      if (event) return;
    }
  }

  // Hold Confirm on a history word to delete it.
  if (mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
      mappedInput.getHeldTime() >= Dictionary::LONG_PRESS_MS && selectedIndex < static_cast<int>(rows.size()) &&
      rows[selectedIndex].kind == RowKind::History) {
    confirmDeleteHistory(true);
    return;
  }

  const int total = static_cast<int>(rows.size());
  buttonNavigator.onNextRelease([this, total] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, total);
    topIndex = followListSelection(selectedIndex, topIndex, visibleRows, total);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this, total] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, total);
    topIndex = followListSelection(selectedIndex, topIndex, visibleRows, total);
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activate(selectedIndex);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    DictUtils::cancelAndFinish(*this);
  }
}

void DictionaryToolActivity::render(RenderLock&&) {
  renderer.clearScreen();
  if (controller.render()) return;

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const Rect header{0, metrics.topPadding, pageWidth, TouchHeaderBackButton::height(metrics, mappedInput)};
  const std::string dictName = dictionaryName();
  const char* subtitle = dictName.empty() ? tr(STR_DICT_NO_DICT_SET) : dictName.c_str();
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, uiTarget, header, tr(STR_TOOLS_DICTIONARY), true);
  } else {
    GUI.drawHeader(renderer, header, tr(STR_TOOLS_DICTIONARY), subtitle);
  }

  uiReady = false;
  app.render();
  uiReady = true;

  const auto labels =
      mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
