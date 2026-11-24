#pragma once
#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <cstring>
#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"
#include "util/DictionaryLookupController.h"
#include "util/LookupHistory.h"

class LookedUpWordsActivity final : public Activity {
 public:
  explicit LookedUpWordsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookCachePath,
                                 const char* dictionaryFontFamilyName = nullptr, uint8_t dictionaryFontPointSize = 0);

  // Pick mode (Speed Read look-up): the given words instead of the book's
  // history, starting on `initialIndex`; no delete. Lookups are still
  // recorded in the book's history like any other.
  void usePickList(std::vector<std::string> words, int initialIndex);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  using UiApp = freeink::ui::FreeInkApp<20, 4>;
  static constexpr freeink::ui::ActionId ACTION_ROW = 1;

  static void historyScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildHistoryScreen(UiApp::ScreenType& screen);
  void reloadEntries();

  std::string cachePath;
  char dictionaryFontFamilyName[64] = "";
  uint8_t dictionaryFontPointSize = 0;
  std::vector<LookupHistory::Entry> entries;
  std::vector<std::string> labels;
  std::vector<freeink::ui::ListItem> uiItems;
  int selectedIndex = 0;
  bool pickMode_ = false;
  std::vector<std::string> pickWords_;
  int pickInitial_ = 0;
  // The Confirm hold that opened a pick list ends inside it; that release must
  // not look up the first word.
  bool ignoreInitialConfirmRelease_ = false;

  DictionaryLookupController controller;
  ButtonNavigator buttonNavigator;
  freeink::ui::GfxRendererTarget uiTarget;
  UiApp app;
  bool uiReady = false;
  int visibleRows = 1;
  int topIndex = 0;

  bool skipLoopDelay() override { return controller.skipLoopDelay(); }

  void showDeleteConfirmation(bool ignoreInitialConfirmRelease);
  // Not-found popup's "switch dictionary": pick another and retry the word.
  void openDictionarySwitch();
  static const char* glyphFor(LookupHistory::Status s);
};
