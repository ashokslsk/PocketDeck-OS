#include "RecentBooksActivity.h"

#include <Arduino.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>
#include <memory>

#include "BookActions.h"
#include "FileBrowserActionActivity.h"
#include "MappedInputManager.h"
#include "RecentBookProgress.h"
#include "RecentBooksStore.h"
#include "activities/reader/BookInfoActivity.h"
#include "activities/reader/EpubReaderActivity.h"
#include "activities/reader/ReadingStatsUtils.h"
#include "activities/reader/KannadaReading.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/OptionSelectionActivity.h"
#include "components/CompactHeader.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UIThemeTokens.h"
#include "components/UiAppHelpers.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {
// Hold threshold for the long-press action menu (firmware convention).
constexpr unsigned long LONG_PRESS_MS = 1000;
constexpr unsigned long ACTION_FEEDBACK_MS = 1000;
constexpr fui::ActionId ACTION_ROW = 1;
}  // namespace

RecentBooksActivity::RecentBooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("RecentBooks", renderer, mappedInput),
      uiTarget(makeUiTarget(renderer)),
      app(uiTarget, uiTarget.deviceContext()) {}

RecentBooksActivity::SortMode RecentBooksActivity::sortMode = RecentBooksActivity::SortMode::RecentFirst;

void RecentBooksActivity::rebuildOrder() {
  orderDirty_ = false;
  order_.clear();
  if (sortMode == SortMode::RecentFirst) return;
  const unsigned long started = millis();

  struct Entry {
    uint8_t pos;
    int32_t key;  // larger first; ties keep recent-first order
  };
  std::vector<Entry> entries;
  std::vector<std::string> titles;  // Title sort only
  entries.reserve(RecentBooksStore::MAX_TOTAL_BOOKS);
  if (sortMode == SortMode::Title) titles.reserve(RecentBooksStore::MAX_TOTAL_BOOKS);

  std::vector<RecentBook> page;
  page.reserve(kPageSize);
  size_t offset = 0;
  bool more = true;
  while (more && offset < static_cast<size_t>(RecentBooksStore::MAX_TOTAL_BOOKS)) {
    page.clear();
    more = RECENT_BOOKS.loadPage(page, offset, kPageSize);
    if (page.empty()) break;
    for (const RecentBook& book : page) {
      const auto pos = static_cast<uint8_t>(offset);
      int32_t key = 0;
      switch (sortMode) {
        case SortMode::OldestFirst:
          key = static_cast<int32_t>(offset);
          break;
        case SortMode::Progress: {
          const float pct = FsHelpers::hasEpubExtension(book.path) ? RecentBookProgress::loadCachedEpubPercent(book)
                                                                    : RecentBookProgress::loadPercent(book);
          key = RecentBookProgress::hasPercent(pct) ? static_cast<int32_t>(pct * 100.0f) : -1;
          break;
        }
        case SortMode::Finished: {
          // Finished books first, newest finish date first; then the rest.
          const std::string cachePath = BookActions::statsCachePath(book.path);
          const BookReadingStats stats = cachePath.empty() ? BookReadingStats{} : BookReadingStats::load(cachePath);
          if (stats.isCompleted) {
            key = stats.finishedDate.isValid() ? static_cast<int32_t>(readingStatsDayIndex(stats.finishedDate)) : 0;
          } else {
            key = -1;
          }
          break;
        }
        case SortMode::Title:
          titles.push_back(book.title.empty() ? book.path : book.title);
          break;
        case SortMode::RecentFirst:
        case SortMode::Count:
          break;
      }
      entries.push_back({pos, key});
      ++offset;
    }
  }

  if (sortMode == SortMode::Title) {
    std::stable_sort(entries.begin(), entries.end(), [&titles](const Entry& a, const Entry& b) {
      return strcasecmp(titles[a.pos].c_str(), titles[b.pos].c_str()) < 0;
    });
  } else {
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.key > b.key; });
  }
  order_.reserve(entries.size());
  for (const Entry& e : entries) order_.push_back(e.pos);
  LOG_DBG("RBA", "Sorted %u recent books (mode %u) in %lums", static_cast<unsigned>(order_.size()),
          static_cast<unsigned>(sortMode), millis() - started);
}

void RecentBooksActivity::loadRecentBooks() {
  // One page; books no longer on the card are skipped.
  if (sortMode == SortMode::RecentFirst) {
    hasOlder = RECENT_BOOKS.loadPage(recentBooks, pageOffset, kPageSize);
  } else {
    if (orderDirty_) rebuildOrder();
    recentBooks.clear();
    std::vector<RecentBook> one;
    for (size_t i = pageOffset; i < order_.size() && i < pageOffset + kPageSize; ++i) {
      one.clear();
      RECENT_BOOKS.loadPage(one, order_[i], 1);
      if (!one.empty()) recentBooks.push_back(std::move(one.front()));
    }
    hasOlder = pageOffset + kPageSize < order_.size();
  }
  recentBooks.erase(std::remove_if(recentBooks.begin(), recentBooks.end(), &RecentBooksStore::isMissing),
                    recentBooks.end());
  // EPUBs use the small cached percentage (no book open); XTC/TXT read their
  // own progress file. Done here, not in render(), so redraws stay SD-free.
  for (size_t i = 0; i < kPageSize; ++i) {
    percentLabels[i][0] = '\0';
    if (i >= recentBooks.size()) continue;
    const RecentBook& book = recentBooks[i];
    const float pct = FsHelpers::hasEpubExtension(book.path) ? RecentBookProgress::loadCachedEpubPercent(book)
                                                              : RecentBookProgress::loadPercent(book);
    if (RecentBookProgress::hasPercent(pct)) {
      snprintf(percentLabels[i], sizeof(percentLabels[i]), "%d%%", static_cast<int>(pct + 0.5f));
    }
  }
}

int RecentBooksActivity::bookForRow(const int row) const {
  const int first = hasNewer() ? 1 : 0;
  if (hasNewer() && row == 0) return -1;
  const int book = row - first;
  if (book >= 0 && book < static_cast<int>(recentBooks.size())) return book;
  return -2;
}

void RecentBooksActivity::showPage(const size_t offset, const bool selectLast) {
  {
    RenderLock lock(*this);  // render() reads recentBooks
    pageOffset = offset;
    loadRecentBooks();
    const int rows = rowCount();
    if (selectLast) {
      selectorIndex = static_cast<size_t>(std::max(0, rows - 1 - (hasOlder ? 1 : 0)));
    } else {
      selectorIndex = hasNewer() && rows > 1 ? 1 : 0;
    }
    topIndex = followListSelection(static_cast<int>(selectorIndex), 0, visibleRows, rows);
  }
  requestUpdate(true);
}

void RecentBooksActivity::activateRow(const int row) {
  const int book = bookForRow(row);
  if (book == -1) {
    showPage(pageOffset >= kPageSize ? pageOffset - kPageSize : 0, true);
  } else if (book == -2) {
    if (hasOlder) showPage(pageOffset + kPageSize);
  } else {
    onSelectBook(recentBooks[static_cast<size_t>(book)].path);
  }
}

void RecentBooksActivity::onRowEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<RecentBooksActivity*>(user);
  if (event.value < 0 || event.value >= self->rowCount()) return;
  self->selectorIndex = static_cast<size_t>(event.value);
  const int book = self->bookForRow(event.value);
  if (event.longPress) {
    self->app.clearTapFlash();
    if (book >= 0) self->showBookActionMenu(static_cast<size_t>(book));
    return;
  }
  // Opening the book leaves this screen; a lingering flash would gray an
  // unrelated row when the list next appears.
  self->app.clearTapFlash();
  self->activateRow(event.value);
}

void RecentBooksActivity::onEnter() {
  Activity::onEnter();
  kannadareading::beginUi();

  // Prune entries whose backing files are gone; this is one of two interaction
  // points where the persistent store gets cleaned.
  if (RECENT_BOOKS.pruneMissing()) {
    RECENT_BOOKS.saveToFile();
  }

  // Load data
  pageOffset = 0;
  orderDirty_ = true;
  loadRecentBooks();

  selectorIndex = 0;
  uiReady = false;
  visibleRows = 1;
  topIndex = 0;
  applySharedUiTheme(app, uiTarget);
  app.on(ACTION_ROW, &RecentBooksActivity::onRowEvent, this);
  app.setScreen(&RecentBooksActivity::listScreen, this);
  requestUpdate();
}

void RecentBooksActivity::onExit() {
  kannadareading::endUi();
  Activity::onExit();
  recentBooks.clear();
  recentBooks.shrink_to_fit();
  order_.clear();
  order_.shrink_to_fit();
}

void RecentBooksActivity::loop() {
  if (pendingCacheDeletedFeedback && millis() - cacheDeletedFeedbackShowTime >= ACTION_FEEDBACK_MS) {
    pendingCacheDeletedFeedback = false;
    requestUpdate();
    return;
  }

  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    onGoHome();
    return;
  }
  const int listSize = rowCount();
  // After a long-press has fired, swallow input until Confirm is physically released
  // (so the release doesn't also open the book; re-arm only once the button is up).
  if (longPressFired) {
    if (!mappedInput.isPressed(MappedInputManager::Button::Confirm)) {
      longPressFired = false;
    }
    return;
  }

  // Long-press Confirm on the selected book: open the same action menu shape used by File Browser.
  // Fires when the hold times out while still held (firmware hold-to-act pattern,
  // cf. FileBrowserActivity BACK long-press).
  if (bookForRow(static_cast<int>(selectorIndex)) >= 0 && mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
      mappedInput.getHeldTime() >= LONG_PRESS_MS) {
    longPressFired = true;
    showBookActionMenu(static_cast<size_t>(bookForRow(static_cast<int>(selectorIndex))), true);
    return;
  }

  // Touch goes through the FreeInkApp: render() registered the row hit rects;
  // route the snapshot and let onRowEvent dispatch.
  if (uiReady) {
    const fui::InputSnapshot snap = touchSnapshotFrom(mappedInput);
    if (snap.touchPressed || snap.touchReleased) {
      const auto event = app.route(snap);
      // No pressed-state repaint: the render it triggers would drop a slow
      // tap's release inside the uiReady window (tap-to-activate needed two
      // taps), and it costs a second e-ink refresh per tap.
      if (app.invalidated()) requestUpdate();
      if (event) return;  // dispatched to onRowEvent
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (static_cast<int>(selectorIndex) < listSize) {
      activateRow(static_cast<int>(selectorIndex));
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  // Swipes scroll the viewport; the selection stays put and button navigation
  // pulls the view back to it.
  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
    const int delta = swipe == MappedInputManager::SwipeDir::Up ? visibleRows : -visibleRows;
    const int next = scrollListBy(topIndex, delta, visibleRows, listSize);
    if (next != topIndex) {
      topIndex = next;
      requestUpdate();
    }
    return;
  }

  const auto moveSelection = [this, listSize](const int index) {
    selectorIndex = static_cast<size_t>(index);
    topIndex = followListSelection(static_cast<int>(selectorIndex), topIndex, visibleRows, listSize);
    requestUpdate();
  };
  buttonNavigator.onNextRelease([this, listSize, &moveSelection] {
    moveSelection(ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), listSize));
  });
  buttonNavigator.onPreviousRelease([this, listSize, &moveSelection] {
    moveSelection(ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), listSize));
  });
  buttonNavigator.onNextContinuous([this, listSize, &moveSelection] {
    moveSelection(ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), listSize, visibleRows));
  });
  buttonNavigator.onPreviousContinuous([this, listSize, &moveSelection] {
    moveSelection(ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), listSize, visibleRows));
  });
}

void RecentBooksActivity::reloadAfterBookAction() {
  orderDirty_ = true;  // a removed or finished book moves the sorted order
  loadRecentBooks();
  if (recentBooks.empty() && pageOffset > 0) {
    pageOffset = pageOffset >= kPageSize ? pageOffset - kPageSize : 0;
    loadRecentBooks();
  }
  const int rows = rowCount();
  if (rows == 0) {
    selectorIndex = 0;
  } else if (static_cast<int>(selectorIndex) >= rows) {
    selectorIndex = static_cast<size_t>(rows - 1);
  }
  topIndex = followListSelection(static_cast<int>(selectorIndex), topIndex, visibleRows, rows);
  requestUpdate(true);
}

void RecentBooksActivity::promptDeleteBook(const RecentBook& book) {
  const std::string path = book.path;
  auto handler = [this, path](const ActivityResult& res) {
    if (res.isCancelled) {
      return;
    }

    BookActions::clearFileMetadata(path);
    if (!Storage.remove(path.c_str())) {
      LOG_ERR("RBA", "Failed to delete file: %s", path.c_str());
      return;
    }

    RECENT_BOOKS.removeByPath(path);
    reloadAfterBookAction();
  };

  const std::string heading = tr(STR_DELETE) + std::string("? ");
  startActivityForResult(std::make_unique<ConfirmationActivity>(renderer, mappedInput, heading, book.title),
                         std::move(handler));
}

void RecentBooksActivity::promptRemoveBook(const std::string& path, const std::string& title) {
  auto handler = [this, path](const ActivityResult& res) {
    if (res.isCancelled) {
      return;
    }
    if (RECENT_BOOKS.removeByPath(path)) {
      reloadAfterBookAction();
    }
  };

  startActivityForResult(
      std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_REMOVE_FROM_RECENTS), title,
                                             /*ignoreInitialConfirmRelease=*/false),
      std::move(handler));
}

void RecentBooksActivity::showBookActionMenu(const size_t bookIndex, const bool ignoreInitialConfirmRelease) {
  if (bookIndex >= recentBooks.size()) return;

  const RecentBook book = recentBooks[bookIndex];
  std::vector<FileBrowserActionActivity::MenuItem> items =
      BookActions::buildBookActionItems(book.path, /*includeRemoveFromRecents=*/true);
  // List-wide Sort by and this book's Book Info lead the menu.
  items.insert(items.begin(), {FileBrowserAction::BookInfo, StrId::STR_BOOK_INFO});
  items.insert(items.begin(), {FileBrowserAction::SortRecents, StrId::STR_RECENT_SORT_BY});
  if (BookActions::canSendNearby(book.path)) {
    items.push_back({FileBrowserAction::SendNearby, StrId::STR_SEND_NEARBY_BOOK});
  }

  startActivityForResult(
      std::make_unique<FileBrowserActionActivity>(renderer, mappedInput, book.title, std::move(items),
                                                  ignoreInitialConfirmRelease),
      [this, book](const ActivityResult& result) {
        longPressFired = false;
        if (result.isCancelled) {
          return;
        }

        const auto* actionResult = std::get_if<FileBrowserActionResult>(&result.data);
        if (!actionResult) {
          LOG_ERR("RBA", "Book action result missing");
          return;
        }

        switch (static_cast<FileBrowserAction>(actionResult->action)) {
          case FileBrowserAction::Delete:
            promptDeleteBook(book);
            return;
          case FileBrowserAction::DeleteCache:
            startActivityForResult(
                std::make_unique<ConfirmationActivity>(
                    renderer, mappedInput, BookActions::confirmationHeading(StrId::STR_DELETE_CACHE), book.title),
                [this, book](const ActivityResult& confirmation) {
                  if (!confirmation.isCancelled) {
                    if (!BookActions::clearBookCache(book.path)) {
                      LOG_ERR("RBA", "Failed to clear book cache for: %s", book.path.c_str());
                    } else {
                      pendingCacheDeletedFeedback = true;
                      cacheDeletedFeedbackShowTime = millis();
                    }
                  }
                  reloadAfterBookAction();
                });
            return;
          case FileBrowserAction::DeleteStats:
            startActivityForResult(
                std::make_unique<ConfirmationActivity>(
                    renderer, mappedInput, BookActions::confirmationHeading(StrId::STR_DELETE_BOOK_STATS), book.title),
                [this, book](const ActivityResult& confirmation) {
                  if (!confirmation.isCancelled) {
                    if (!BookActions::deleteBookStats(book.path)) {
                      LOG_ERR("RBA", "Failed to delete book stats for: %s", book.path.c_str());
                    } else {
                      BookActions::drawToast(renderer, tr(STR_BOOK_STATS_DELETED));
                      delay(1000);
                    }
                  }
                  reloadAfterBookAction();
                });
            return;
          case FileBrowserAction::ResetReaderSettings:
            startActivityForResult(
                std::make_unique<ConfirmationActivity>(
                    renderer, mappedInput, BookActions::confirmationHeading(StrId::STR_RESET_BOOK_READER_SETTINGS),
                    book.title),
                [this, book](const ActivityResult& confirmation) {
                  if (!confirmation.isCancelled) {
                    if (!BookActions::resetBookReaderSettings(book.path)) {
                      LOG_ERR("RBA", "Failed to reset reader settings for: %s", book.path.c_str());
                    } else {
                      BookActions::drawToast(renderer, tr(STR_BOOK_READER_SETTINGS_RESET));
                      delay(1000);
                    }
                  }
                  reloadAfterBookAction();
                });
            return;
          case FileBrowserAction::ToggleCompleted: {
            bool completed = false;
            if (BookActions::toggleBookCompleted(book.path, book.title, completed)) {
              BookActions::drawToast(renderer, completed ? tr(STR_MARKED_FINISHED) : tr(STR_MARKED_UNFINISHED));
              delay(1000);
            }
            reloadAfterBookAction();
            return;
          }
          case FileBrowserAction::EpubRenderMode: {
            const uint8_t currentIndex =
                BookActions::epubRenderModeDisplayIndex(EpubReaderActivity::loadBookRenderMode(book.path));
            startActivityForResult(
                std::make_unique<OptionSelectionActivity>(renderer, mappedInput, "RecentEpubRenderModeSelect",
                                                          StrId::STR_EPUB_RENDER_MODE,
                                                          BookActions::epubRenderModeOptions(), currentIndex),
                [this, book](const ActivityResult& selectionResult) {
                  if (!selectionResult.isCancelled) {
                    const auto* selection = std::get_if<OptionSelectionResult>(&selectionResult.data);
                    if (selection != nullptr &&
                        !EpubReaderActivity::saveBookRenderMode(
                            book.path, BookActions::epubRenderModeForDisplayIndex(selection->index))) {
                      LOG_ERR("RBA", "Failed to save render mode for: %s", book.path.c_str());
                    }
                  }
                  reloadAfterBookAction();
                });
            return;
          }
          case FileBrowserAction::SortRecents:
            showSortMenu();
            return;
          case FileBrowserAction::BookInfo:
            showBookInfo(book);
            return;
          case FileBrowserAction::RemoveFromRecents:
            promptRemoveBook(book.path, book.title);
            return;
          case FileBrowserAction::SendNearby:
            activityManager.goToNearbyBookSend(book.path, false);
            return;
          case FileBrowserAction::PinFavorite:
          case FileBrowserAction::UnpinFavorite:
          case FileBrowserAction::PinBootFavorite:
          case FileBrowserAction::UnpinBootFavorite:
          case FileBrowserAction::SetSleepFolder:
          case FileBrowserAction::ClearSleepFolder:
          case FileBrowserAction::ViewBookmarks:
          case FileBrowserAction::ViewClippings:
          case FileBrowserAction::DeleteBookmarks:
          case FileBrowserAction::DeleteClippings:
          case FileBrowserAction::Rename:
          case FileBrowserAction::Move:
          case FileBrowserAction::Info:
          case FileBrowserAction::NewFolder:
          case FileBrowserAction::Copy:
          case FileBrowserAction::Search:
            return;
        }
      });
}

void RecentBooksActivity::showSortMenu() {
  std::vector<std::string> options;
  options.reserve(static_cast<size_t>(SortMode::Count));
  options.emplace_back(tr(STR_RECENT_SORT_RECENT));
  options.emplace_back(tr(STR_RECENT_SORT_OLDEST));
  options.emplace_back(tr(STR_RECENT_SORT_PROGRESS));
  options.emplace_back(tr(STR_RECENT_SORT_TITLE));
  options.emplace_back(tr(STR_RECENT_SORT_FINISHED));
  startActivityForResult(
      std::make_unique<OptionSelectionActivity>(renderer, mappedInput, "RecentSort", StrId::STR_RECENT_SORT_BY,
                                                std::move(options), static_cast<uint8_t>(sortMode)),
      [this](const ActivityResult& result) {
        const auto* sel = std::get_if<OptionSelectionResult>(&result.data);
        if (!result.isCancelled && sel != nullptr && sel->index < static_cast<int>(SortMode::Count)) {
          sortMode = static_cast<SortMode>(sel->index);
          orderDirty_ = true;
          showPage(0);
          return;
        }
        requestUpdate();
      });
}

void RecentBooksActivity::showBookInfo(const RecentBook& book) {
  auto data = makeUniqueNoThrow<BookInfoData>();
  if (!data) {
    LOG_ERR("RBA", "OOM: BookInfoData (%u bytes)", static_cast<unsigned>(sizeof(BookInfoData)));
    return;
  }
  data->title = book.title;
  data->author = book.author;
  if (FsHelpers::hasEpubExtension(book.path)) {
    Epub(book.path, "/.pocketdeck-os").readOpfMetadataField("dc:publisher", data->publisher, sizeof(data->publisher));
    data->progressPercent = RecentBookProgress::loadCachedEpubPercent(book);
  } else {
    data->progressPercent = RecentBookProgress::loadPercent(book);
  }
  const std::string cachePath = BookActions::statsCachePath(book.path);
  if (!cachePath.empty()) data->stats = BookReadingStats::load(cachePath);
  // The reader saves its last time-left estimate with the stats.
  data->timeLeftSeconds = data->stats.estimatedTimeLeftSeconds;
  data->hasTimeLeft = data->timeLeftSeconds > 0;
  data->globalStats = GlobalReadingStats::load();
  auto info = makeUniqueNoThrow<BookInfoActivity>(renderer, mappedInput, std::move(*data));
  if (!info) {
    LOG_ERR("RBA", "OOM: BookInfoActivity");
    return;
  }
  startActivityForResult(std::move(info), [this](const ActivityResult&) { requestUpdate(); });
}

void RecentBooksActivity::listScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<RecentBooksActivity*>(user)->buildListScreen(screen);
}

void RecentBooksActivity::buildListScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  // Content below the GUI.drawHeader band, above the button hints.
  screen.setContentMargin(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput)), 0,
                  static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (rowCount() == 0) {
    screen.centeredText(tr(STR_NO_RECENT_BOOKS), screen.theme().bodyText);
    return;
  }

  // Transient per-render: points into the recentBooks strings.
  std::vector<fui::ListItem> items;
  items.reserve(static_cast<size_t>(rowCount()));
  // Control rows name the books they lead to: "1-10" back, "21+" forward.
  char newerInfo[24];
  char olderInfo[24];
  const size_t newerStart = pageOffset >= kPageSize ? pageOffset - kPageSize : 0;
  snprintf(newerInfo, sizeof(newerInfo), "%u-%u", static_cast<unsigned>(newerStart + 1),
           static_cast<unsigned>(newerStart + kPageSize));
  snprintf(olderInfo, sizeof(olderInfo), "%u+", static_cast<unsigned>(pageOffset + kPageSize + 1));
  const auto addControl = [&items](const char* label, const char* info) {
    fui::ListItem item;
    item.label = label;
    item.subtitle = info;
    item.icon = listIconFor(UIIcon::Recent, 32);
    item.actionValue = static_cast<int16_t>(items.size());
    items.push_back(item);
  };
  if (hasNewer()) addControl(tr(STR_RECENT_NEWER), newerInfo);
  for (size_t b = 0; b < recentBooks.size(); ++b) {
    const auto& book = recentBooks[b];
    fui::ListItem item;
    item.label = book.title.c_str();
    if (!book.author.empty()) item.subtitle = book.author.c_str();
    if (b < kPageSize && percentLabels[b][0] != '\0') item.value = percentLabels[b];
    item.icon = listIconFor(UITheme::getFileIcon(book.path), 32);  // subtitle rows carry the larger icon
    item.actionValue = static_cast<int16_t>(items.size());
    items.push_back(item);
  }
  if (hasOlder) addControl(tr(STR_RECENT_OLDER), olderInfo);

  fui::ListProps props;
  props.items = items.data();
  props.count = static_cast<uint16_t>(items.size());
  props.selectedIndex = static_cast<int16_t>(selectorIndex);
  props.action = ACTION_ROW;
  props.inputMask = static_cast<uint16_t>(fui::InputTouch | fui::InputLongPress);  // physical buttons stay in loop()
  props.iconSize = 28;
  props.labelText = screen.theme().bodyText;
  props.labelText.bold = true;
  const fui::Rect listBounds = screen.body();
  const auto rows = configureUiList(props, screen.theme(), listBounds, UiListRowType::WithSubtitle);
  visibleRows = rows > 0 ? rows : 1;
  topIndex = scrollListBy(topIndex, 0, visibleRows, rowCount());  // clamp to range
  props.topIndex = static_cast<uint16_t>(topIndex);
  screen.list(props);
}

void RecentBooksActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto& metrics = UITheme::getInstance().getMetrics();

  // Header via GUI.drawHeader (already FreeInkUI-themed) for the battery
  // indicator; the rest of the screen renders through the app.
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, uiTarget, header, tr(STR_MENU_RECENT_BOOKS), false);
  } else {
    GUI.drawHeader(renderer, header, tr(STR_MENU_RECENT_BOOKS));
  }

  uiReady = false;
  app.render();
  uiReady = true;

  const auto labels =
      mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_HOME)), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (pendingCacheDeletedFeedback) {
    GUI.drawPopup(renderer, tr(STR_BOOK_CACHE_DELETED));
  }

  renderer.displayBuffer();
}
