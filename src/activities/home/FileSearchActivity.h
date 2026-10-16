#pragma once
#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Explorer > Options > Search files…: finds files and folders anywhere on the
// SD card whose name contains the query (case-insensitive). The card is walked
// in short slices from loop(), so the screen stays responsive and Back cancels.
// Hidden folders are skipped unless Show Hidden Files is on; the internal
// /.pocketdeck-os folder is always skipped.
// Choosing a result: a book opens in the reader (Back from it returns to the
// book's folder), a folder opens in Explorer, any other file opens its folder
// with the file highlighted.
class FileSearchActivity final : public Activity {
 public:
  FileSearchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string query);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  static constexpr size_t kMaxResults = 200;

 private:
  using UiApp = freeink::ui::FreeInkApp<20, 4>;
  static constexpr freeink::ui::ActionId ACTION_ROW = 1;
  struct Result {
    std::string path;
    bool isDirectory;
  };

  static void listScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildListScreen(UiApp::ScreenType& screen);
  bool searchSlice(unsigned long budgetMs);
  void open(int index);

  std::string query_;
  std::string needle_;  // lower-cased query
  std::vector<std::string> pendingDirs_;
  std::vector<Result> results_;
  bool searching_ = true;
  bool truncated_ = false;
  unsigned foldersScanned_ = 0;
  unsigned long lastDrawMs_ = 0;

  // List rows (rebuilt when the results change).
  std::vector<std::string> names_;
  std::vector<std::string> folders_;
  std::vector<freeink::ui::ListItem> uiItems_;
  size_t itemsBuiltFor_ = SIZE_MAX;
  int selectedIndex_ = 0;
  int topIndex_ = 0;
  int visibleRows_ = 1;
  bool uiReady_ = false;

  ButtonNavigator buttonNavigator_;
  freeink::ui::GfxRendererTarget uiTarget_;
  UiApp app_;
};
