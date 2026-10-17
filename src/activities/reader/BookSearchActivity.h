#pragma once
#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class Epub;

// Reader menu > Search in Book: finds a word or phrase in every chapter.
// Chapters are read straight from the EPUB with their markup skipped (no page
// layout needed), one chapter per loop pass so the progress line moves and
// Back stops early. Matching ignores letter case for Latin text; Kannada and
// other scripts match exactly. Each result shows the text around the match and
// its chapter; choosing one returns BookmarkResult{spine, progress} and the
// reader opens the page with the match highlighted.
class BookSearchActivity final : public Activity {
 public:
  struct Hit {
    uint16_t spine;
    uint32_t offset;  // byte offset of the match in the chapter's XHTML
    float progress;   // offset / chapter size, filled when the chapter is done
    std::string snippet;
  };
  static constexpr size_t kMaxHits = 60;

  BookSearchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::shared_ptr<Epub> epub,
                     std::string query);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }

 private:
  using UiApp = freeink::ui::FreeInkApp<20, 4>;
  static constexpr freeink::ui::ActionId ACTION_ROW = 1;

  static void listScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildListScreen(UiApp::ScreenType& screen);
  void searchChapter(int spine);
  void choose(int index);

  std::shared_ptr<Epub> epub_;
  std::string query_;
  std::string needle_;  // normalized query
  std::vector<Hit> hits_;
  int nextSpine_ = 0;
  int spineCount_ = 0;
  bool searching_ = true;
  bool truncated_ = false;
  unsigned long lastDrawMs_ = 0;

  std::vector<std::string> labels_;
  std::vector<std::string> subtitles_;
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
