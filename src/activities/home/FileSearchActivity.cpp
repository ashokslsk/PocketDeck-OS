#include "FileSearchActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "activities/reader/KannadaReading.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {
constexpr unsigned long kSliceMs = 40;
constexpr unsigned long kProgressRedrawMs = 1500;
constexpr size_t kMaxPathBytes = 240;

std::string lowerAscii(std::string s) {
  for (auto& c : s) {
    if (static_cast<unsigned char>(c) < 0x80) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

bool isBook(const std::string& path) {
  return FsHelpers::hasEpubExtension(path) || FsHelpers::hasXtcExtension(path) || FsHelpers::hasTxtExtension(path) ||
         FsHelpers::hasMarkdownExtension(path);
}

bool skipFolderName(const char* name) {
  if (name[0] != '.') {
    return std::strcmp(name, "System Volume Information") == 0 || std::strcmp(name, "XTCache") == 0;
  }
  // Reader data is never a search target; other dot-folders follow the setting.
  return std::strcmp(name, ".pocketdeck-os") == 0 || !SETTINGS.showHiddenFiles;
}

std::string join(const std::string& dir, const char* name) {
  return dir == "/" ? "/" + std::string(name) : dir + "/" + name;
}
}  // namespace

FileSearchActivity::FileSearchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string query)
    : Activity("FileSearch", renderer, mappedInput),
      query_(std::move(query)),
      uiTarget_(makeUiTarget(renderer)),
      app_(uiTarget_, uiTarget_.deviceContext()) {}

void FileSearchActivity::onEnter() {
  Activity::onEnter();
  kannadareading::beginUi();  // Kannada file names render shaped
  needle_ = lowerAscii(query_);
  pendingDirs_.clear();
  pendingDirs_.push_back("/");
  results_.clear();
  searching_ = !needle_.empty();
  truncated_ = false;
  foldersScanned_ = 0;
  lastDrawMs_ = millis();
  applySharedUiTheme(app_, uiTarget_);
  app_.on(ACTION_ROW, &FileSearchActivity::onRowEvent, this);
  app_.setScreen(&FileSearchActivity::listScreen, this);
  requestUpdate();
}

void FileSearchActivity::onExit() {
  kannadareading::endUi();
  Activity::onExit();
}

bool FileSearchActivity::searchSlice(const unsigned long budgetMs) {
  const unsigned long start = millis();
  char name[256];
  while (!pendingDirs_.empty() && millis() - start < budgetMs) {
    const std::string dirPath = std::move(pendingDirs_.back());
    pendingDirs_.pop_back();
    HalFile dir = Storage.open(dirPath.c_str());
    if (!dir || !dir.isDirectory()) continue;
    ++foldersScanned_;
    for (HalFile file = dir.openNextFile(); file; file = dir.openNextFile()) {
      file.getName(name, sizeof(name));
      const bool isDir = file.isDirectory();
      file.close();
      if (isDir ? skipFolderName(name) : (name[0] == '.' && !SETTINGS.showHiddenFiles)) continue;
      const std::string path = join(dirPath, name);
      if (lowerAscii(name).find(needle_) != std::string::npos) {
        if (results_.size() >= kMaxResults) {
          truncated_ = true;
          pendingDirs_.clear();
          break;
        }
        results_.push_back({path, isDir});
      }
      if (isDir && path.size() < kMaxPathBytes) pendingDirs_.push_back(path);
    }
    dir.close();
  }
  return !pendingDirs_.empty();
}

void FileSearchActivity::listScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<FileSearchActivity*>(user)->buildListScreen(screen);
}

void FileSearchActivity::buildListScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) +
                                       metrics.verticalSpacing),
                  0, static_cast<int16_t>(metrics.buttonHintsHeight + metrics.verticalSpacing), 0});
  if (itemsBuiltFor_ != results_.size()) {
    // Results only grow while searching; rows point into these strings.
    names_.clear();
    folders_.clear();
    uiItems_.clear();
    names_.reserve(results_.size());
    folders_.reserve(results_.size());
    uiItems_.reserve(results_.size());
    for (size_t i = 0; i < results_.size(); ++i) {
      const auto& r = results_[i];
      names_.push_back(r.path.substr(r.path.find_last_of('/') + 1));
      folders_.push_back(FsHelpers::extractFolderPath(r.path));
    }
    for (size_t i = 0; i < results_.size(); ++i) {
      fui::ListItem item;
      item.label = names_[i].c_str();
      item.subtitle = folders_[i].c_str();
      item.icon = listIconFor(UITheme::getFileIcon(results_[i].isDirectory ? names_[i] + "/" : names_[i]), 24);
      item.actionValue = static_cast<int16_t>(i);
      uiItems_.push_back(item);
    }
    itemsBuiltFor_ = results_.size();
  }
  fui::ListProps props;
  props.items = uiItems_.data();
  props.count = static_cast<uint16_t>(uiItems_.size());
  props.selectedIndex = static_cast<int16_t>(selectedIndex_);
  props.topIndex = static_cast<uint16_t>(topIndex_);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.labelText = screen.theme().bodyText;
  props.labelText.maxLines = 1;
  const auto rows = configureUiList(props, screen.theme(), screen.body());
  visibleRows_ = rows > 0 ? rows : 1;
  topIndex_ = scrollListBy(topIndex_, 0, visibleRows_, static_cast<int>(uiItems_.size()));
  props.topIndex = static_cast<uint16_t>(topIndex_);
  screen.list(props);
}

void FileSearchActivity::onRowEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<FileSearchActivity*>(user);
  if (event.value < 0 || event.value >= static_cast<int16_t>(self->results_.size())) return;
  self->selectedIndex_ = event.value;
  self->app_.clearTapFlash();
  self->open(self->selectedIndex_);
}

void FileSearchActivity::open(const int index) {
  if (index < 0 || index >= static_cast<int>(results_.size())) return;
  const Result r = results_[index];
  if (!r.isDirectory && isBook(r.path)) {
    // Back from the book returns to its folder, as for a book opened in Explorer.
    APP_STATE.readerReturnTo = CrossPointState::READER_RETURN_FILE_BROWSER;
    activityManager.goToReader(r.path);
    return;
  }
  // A folder opens in Explorer; any other file opens its folder, highlighted.
  activityManager.goToFileBrowser(r.path);
}

void FileSearchActivity::loop() {
  if (searching_) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      searching_ = false;  // stop and show what was found so far
      requestUpdate();
      return;
    }
    {
      RenderLock lock(*this);
      searching_ = searchSlice(kSliceMs);
    }
    if (!searching_ || millis() - lastDrawMs_ >= kProgressRedrawMs) {
      lastDrawMs_ = millis();
      requestUpdate();
    }
    return;
  }

  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (uiReady_) {
    const fui::InputSnapshot snap = touchSnapshotFrom(mappedInput);
    if (snap.touchPressed || snap.touchReleased) {
      const auto event = app_.route(snap);
      if (app_.invalidated()) requestUpdate();
      if (event) return;
    }
  }
  const int total = static_cast<int>(results_.size());
  if (total == 0) return;
  buttonNavigator_.onNextRelease([this, total] {
    selectedIndex_ = ButtonNavigator::nextIndex(selectedIndex_, total);
    topIndex_ = followListSelection(selectedIndex_, topIndex_, visibleRows_, total);
    requestUpdate();
  });
  buttonNavigator_.onPreviousRelease([this, total] {
    selectedIndex_ = ButtonNavigator::previousIndex(selectedIndex_, total);
    topIndex_ = followListSelection(selectedIndex_, topIndex_, visibleRows_, total);
    requestUpdate();
  });
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    open(selectedIndex_);
  }
}

void FileSearchActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  char title[96];
  snprintf(title, sizeof(title), "\xe2\x80\x9c%s\xe2\x80\x9d", query_.c_str());
  char status[64];
  if (searching_) {
    snprintf(status, sizeof(status), tr(STR_SEARCHING_FORMAT), foldersScanned_);
  } else {
    snprintf(status, sizeof(status), truncated_ ? tr(STR_SEARCH_FIRST_FORMAT) : tr(STR_SEARCH_FOUND_FORMAT),
             static_cast<unsigned>(results_.size()));
  }
  const Rect header{0, metrics.topPadding, pageWidth, TouchHeaderBackButton::height(metrics, mappedInput)};
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, uiTarget_, header, title, true);
  } else {
    GUI.drawHeader(renderer, header, title, status);
  }

  if (results_.empty()) {
    const int contentTop = header.y + header.height;
    const int midY = contentTop + (pageHeight - contentTop - metrics.buttonHintsHeight) / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, midY, searching_ ? status : tr(STR_SEARCH_NO_MATCH));
    uiReady_ = false;
  } else {
    uiReady_ = false;
    app_.render();
    uiReady_ = true;
  }

  const auto labels =
      searching_
          ? mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_SEARCH_STOP)), "", "", "")
          : mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), results_.empty() ? "" : tr(STR_OPEN),
                                  results_.empty() ? "" : tr(STR_DIR_UP), results_.empty() ? "" : tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
