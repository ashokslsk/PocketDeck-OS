#pragma once
#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>
#include <I18n.h>

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "RecentBooksStore.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class RecentBooksActivity final : public Activity {
 private:
  // FreeInkApp hosts the book list (themed rows, icons, touch routing); the
  // header stays on GUI.drawHeader for the battery indicator.
  using UiApp = freeink::ui::FreeInkApp<20, 4>;

  ButtonNavigator buttonNavigator;

  size_t selectorIndex = 0;

  // Set when a long-press has fired; input is swallowed until Confirm is released
  // again so the release doesn't also open the book.
  bool longPressFired = false;
  bool pendingCacheDeletedFeedback = false;
  unsigned long cacheDeletedFeedbackShowTime = 0UL;

  // Recent tab state: one page of the recent list. The newest page comes from
  // RAM; "Older books" fetches the next page from the SD history, so only
  // kPageSize entries are ever held here.
  static constexpr size_t kPageSize = 10;
  std::vector<RecentBook> recentBooks;
  // List order from the long-press menu's Sort by. Kept for the session, so
  // the list opens the way it was last sorted (until restart).
  enum class SortMode : uint8_t { RecentFirst, OldestFirst, Progress, Title, Finished, Count };
  static SortMode sortMode;
  // Positions in the full recent list (newest = 0), in display order; unused
  // for RecentFirst, the store's own order.
  std::vector<uint8_t> order_;
  bool orderDirty_ = true;
  // "42%" for each book on the page, read once per page load (empty if unknown).
  char percentLabels[kPageSize][6] = {};
  size_t pageOffset = 0;
  bool hasOlder = false;
  bool hasNewer() const { return pageOffset > 0; }
  // Rows: [Newer books] + books + [Older books].
  int rowCount() const { return static_cast<int>(recentBooks.size()) + (hasNewer() ? 1 : 0) + (hasOlder ? 1 : 0); }
  // Book index for a row, or -1 for "Newer books", -2 for "Older books".
  int bookForRow(int row) const;
  void activateRow(int row);
  void showPage(size_t offset, bool selectLast = false);

  freeink::ui::GfxRendererTarget uiTarget;  // must precede `app`: the app holds a reference to it
  UiApp app;
  // render() rebuilds the app's interaction table; loop() only routes touch
  // snapshots against it while this is true (the two run on different tasks).
  std::atomic<bool> uiReady{false};
  int visibleRows = 1;  // rows per page at the current scale; set by the screen builder
  int topIndex = 0;     // viewport scroll position, decoupled from the selection

  static void listScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildListScreen(UiApp::ScreenType& screen);

  // Data loading
  void loadRecentBooks();
  void reloadAfterBookAction();

  void promptDeleteBook(const RecentBook& book);
  // Show an OK/Cancel prompt to remove the given book from the Recent Books list.
  void promptRemoveBook(const std::string& path, const std::string& title);
  void showBookActionMenu(size_t bookIndex, bool ignoreInitialConfirmRelease = false);
  void showSortMenu();
  void showBookInfo(const RecentBook& book);
  // Fills order_ for the current sort: one pass over the whole recent list
  // (up to RecentBooksStore::MAX_TOTAL_BOOKS), reading each book's progress
  // or stats when the sort needs them. Pages then load by position.
  void rebuildOrder();

 public:
  explicit RecentBooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
