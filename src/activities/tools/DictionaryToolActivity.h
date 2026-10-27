#pragma once
#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"
#include "util/DictionaryLookupController.h"
#include "util/LookupHistory.h"

// Tools > Dictionary: look up any word without opening a book. One list:
//   Search a word…     (on-screen keyboard; Hold Down there pastes a copy)
//   Copied · <text>    (recent "Copy text" selections from books)
//   √ word …           (this tool's own search history; hold Confirm to delete)
// Definitions use the reader's definition screen, so Kannada renders shaped,
// Kannada words go to an installed Kannada dictionary automatically, and
// Left / "Switch Dictionary" shows the same word in another dictionary.
// History lives in /tools/dictionary/dictionary_history.txt.
class DictionaryToolActivity final : public Activity {
 public:
  DictionaryToolActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  using UiApp = freeink::ui::FreeInkApp<20, 4>;
  static constexpr freeink::ui::ActionId ACTION_ROW = 1;
  enum class RowKind : uint8_t { Search, WordShelf, AllBooks, Copied, History };
  struct Row {
    RowKind kind;
    uint16_t index;  // into copies / history
  };

  static void listScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildListScreen(UiApp::ScreenType& screen);
  void reload();
  void activate(int rowIndex);
  void openSearch();
  // Lookups across all books: books with word counts, then one book's words.
  // Back goes up one level at a time (words -> books -> this list).
  // Word Shelf: the word of the day (same all day) or another random word of 4+
  // letters from the dictionary, opened as a flashcard in the definition view.
  void openWordShelf();
  void openAllBooks(int selected = 0);
  void openBookWords(const std::string& cachePath, const std::string& title, int bookRow);
  void lookUp(const std::string& text);
  void confirmDeleteHistory(bool ignoreInitialConfirmRelease);
  std::string dictionaryName() const;
  bool skipLoopDelay() override { return controller.skipLoopDelay(); }

  std::string cachePath;
  std::vector<std::string> copies;
  std::vector<LookupHistory::Entry> history;
  std::vector<Row> rows;
  std::vector<std::string> labels;
  std::vector<freeink::ui::ListItem> uiItems;
  int selectedIndex = 0;
  bool allBooksLongPressFired_ = false;
  int topIndex = 0;
  int visibleRows = 1;
  bool uiReady = false;

  DictionaryLookupController controller;
  ButtonNavigator buttonNavigator;
  freeink::ui::GfxRendererTarget uiTarget;
  UiApp app;
};
