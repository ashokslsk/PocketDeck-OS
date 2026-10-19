#pragma once

#include <FileIndex.h>
#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "RecentBooksStore.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class FileBrowserActivity final : public Activity {
 public:
  // Picker modes return their selected path via ActivityResult.
  enum class Mode { Books, PickFirmware, PickDirectory };
  // Explorer's own filter for the book list (hidden/system entries, supported
  // types). Shared so a prepared folder index matches what Explorer expects.
  static bool acceptBrowserEntry(const char* name, bool isDir);

 private:
  // FreeInkApp hosts the file list (themed rows, icons, touch routing); the
  // header stays on GUI.drawHeader for the battery indicator.
  using UiApp = freeink::ui::FreeInkApp<20, 4>;

  // Deletion
  void promptDeleteFile(const std::string& fullPath, const std::string& entry);
  void promptDeleteDirectory(const std::string& fullPath, const std::string& entry,
                             bool ignoreInitialConfirmRelease = false);
  void showDirectoryActionMenu(const std::string& entry, bool ignoreInitialConfirmRelease = false);
  void pinSleepFavorite(const std::string& fullPath);
  void unpinSleepFavorite();
  bool isPinnedSleepFavorite(const std::string& fullPath) const;
  void setPreferredSleepFolder(const std::string& fullPath);
  void clearPreferredSleepFolder();
  bool isPreferredSleepFolder(const std::string& fullPath) const;
  void pinBootFavorite(const std::string& fullPath);
  void unpinBootFavorite();
  bool isPinnedBootFavorite(const std::string& fullPath) const;
  void showFileActionMenu(const std::string& entry, bool ignoreInitialConfirmRelease = false);
  void startRenameFile(const std::string& fullPath, const std::string& entry);
  void renameFile(const std::string& oldPath, const std::string& oldEntry, const std::string& newStem,
                  const std::string& extension);

  // File management (Options sheet). The relocate helpers keep a book's reader
  // state (progress, stats, bookmarks, clippings, recents) with it.
  bool relocateFile(const std::string& oldPath, const std::string& newPath);
  bool relocateDirectory(const std::string& oldDir, const std::string& newDir);
  void startRenameDirectory(const std::string& fullPath, const std::string& entry);
  void startMove(const std::string& fullPath, const std::string& entry);
  void startCopy(const std::string& fullPath, const std::string& entry);
  void startSearch();
  void startNewFolder();
  void showInfo(const std::string& fullPath, const std::string& entry);
  void reloadAndSelect(const std::string& entry);
  void showToast(const char* message);

  ButtonNavigator buttonNavigator;

  size_t selectorIndex = 0;
  bool showFileSelection = true;

  bool lockLongPressBack = false;
  bool longPressBackHandled = false;
  bool longPressConfirmHandled = false;
  bool pendingCompletedFeedback = false;
  bool completedFeedbackIsFinished = false;
  unsigned long completedFeedbackShowTime = 0UL;
  // True when this activity was entered while Confirm was already held; we must swallow the next
  // release so we don't immediately auto-open the first entry.
  bool lockNextConfirmRelease = false;
  unsigned long enteredAtMs = 0;
  // File-management feedback ("Moved to books", "Copy cancelled"), drawn over the list.
  std::string toastMessage;
  unsigned long toastShownMs = 0;
  static constexpr unsigned long TOAST_MS = 1800;
  static constexpr unsigned long CONFIRM_CARRYOVER_MS = 400;

  Mode mode = Mode::Books;

  // Files state
  static constexpr size_t INDEX_ROW_CACHE_SIZE = 32;
  std::string basepath = "/";
  std::vector<std::string> files;
  std::unique_ptr<char[]> fileNameBuffer;
  std::unique_ptr<FileIndex> fileIndex;
  std::unique_ptr<FileIndex::Entry> indexEntry;
  std::array<std::string, INDEX_ROW_CACHE_SIZE> indexCachedNames;
  std::array<size_t, INDEX_ROW_CACHE_SIZE> indexCachedRows{};
  bool usingIndex = false;
  // A big folder shown from its saved index while the folder is re-read in
  // the background (loop()); a change rebuilds the list.
  bool indexVerifyPending = false;
  // Prepared marks for rows already shown in this folder (path -> mark), so
  // scrolling back does not touch the card again. Cleared when the folder
  // reloads; bounded so a long scroll cannot grow it without limit.
  struct RowMark {
    std::string path;
    uint8_t mark;  // books: PreparedMarks::BookState; folders: 1 = prepared
  };
  std::vector<RowMark> rowMarks;
  uint8_t rowMarkFor(const std::string& fullPath, bool isDirectory);
  // Re-check this folder's own mark against what is on the card now.
  void validateFolderMarkAfterLoad();
  bool fileListMemoryLimited = false;
  bool fileListReadFailed = false;

  freeink::ui::GfxRendererTarget uiTarget;  // must precede `app`: the app holds a reference to it
  UiApp app;
  // render() rebuilds the app's interaction table; loop() only routes touch
  // snapshots against it while this is true (the two run on different tasks).
  std::atomic<bool> uiReady{false};
  int visibleRows = 1;  // rows per page at the current scale; set by the screen builder
  int topIndex = 0;     // viewport scroll position, decoupled from the selection
  // The current FreeInkUI action payload is int16_t. For larger folders the
  // renderer supplies a local row number and this records its absolute base.
  size_t actionWindowFirst = 0;
  freeink::ui::ListNav listNav;

  static void listScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onSettingsEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildListScreen(UiApp::ScreenType& screen);
  void activateSelected();
  void navigateBack();
  void openSettings();

  // Data loading
  void clearIndexNameCache();
  // allowCachedIndex: navigation may show a big folder from its saved index at
  // once; after the app itself changed the folder, pass false.
  void loadFiles(bool allowCachedIndex = false);
  // Caller holds RenderLock while replacing the list backing storage together
  // with its associated navigation state.
  void loadFilesLocked(bool allowCachedIndex = false);
  bool loadFilesIntoVector(size_t cap, bool& overflow);
  size_t entryCount() const;
  const char* entryNameAt(size_t row);
  void toggleHiddenFiles();
  size_t findEntry(const std::string& name);

 public:
  // PickDirectory: label for the "choose this folder" button (default "Select").
  const char* pickDirectoryLabel = nullptr;
  // PickDirectory: header title (default "Select receive folder").
  const char* pickDirectoryTitle = nullptr;

  explicit FileBrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string initialPath = "/",
                               Mode mode = Mode::Books);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
