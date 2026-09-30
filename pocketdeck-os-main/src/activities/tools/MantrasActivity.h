#pragma once

#include <memory>

#include "KannadaText.h"
#include "MantraLibrary.h"
#include "ToolsCommon.h"
#include "activities/Activity.h"

// Daily mantras and a japa counter, from /tools/mantras/mantras.json.
//
// Home: a time-aware greeting, today's mantra (the weekday's deity, a new
// mantra each week), and rows for the daily ritual sequence, deities, daily
// and ritual mantras, kavacha, the japa counter and stats. Each mantra shows
// its Kannada text (SD card Kannada font), transliteration and meaning; the
// japa counter keeps a mala of 108 (or 54 / 27) and logs every session to
// /tools/mantras/log-YYYY-MM.txt for the stats page and /stats export.
// Settings (meaning language, mala size, daily ritual, weekday deities) are
// kept in /tools/mantras/settings.txt.
class MantrasActivity final : public Activity {
 public:
  explicit MantrasActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Mantras", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Screen : uint8_t { Loading, Missing, Home, Ritual, Groups, List, View, Japa, About };
  enum class Group : uint8_t { Deities, Rituals, Kavacha };
  static constexpr int kHomeRows = 7;
  static constexpr int kMaxSteps = 24;
  static constexpr int kPreviewRows = 8;
  static constexpr int kMaxStack = 6;

  struct Preview {
    char kannada[120];
    char english[80];
  };
  // A row on the Groups screen: a category, or a kavacha's About / Read-all row.
  struct GroupRow {
    int16_t category;  // -1 for kavacha rows
    int8_t kavacha;
    bool about;
  };

  // Settings
  void loadSettings();
  bool saveSettings() const;
  static bool writeSettings(FsFile& out, void* ctx);
  void openSettings();
  void chooseRitual();

  // Navigation
  void push(Screen s);
  void pop();
  void openGroups(Group g);
  void openList(int category);
  // kavachaAll >= 0: read that whole kavacha from its first mantra (mantra is ignored).
  void openView(uint16_t mantra, int category, int kavachaAll);
  void openJapa(uint16_t mantra);
  void openAbout(int kavacha);
  void openStats();
  void stepView(int dir);
  void loadPreviews();
  void loadRitual();
  void computeToday();
  void loadTodayJapa();
  void saveJapa();
  void confirmReset();
  bool loadMantra(uint16_t index);

  // Drawing
  bool useKannada(const char* text) const;
  int drawText(const char* text, kannada::Style s, int x, int y, int maxW, bool black = true) const;
  void renderHome();
  void renderRitual();
  void renderGroups();
  void renderList();
  void renderView();
  void renderJapa();
  void renderAbout();
  void renderMessage(const char* title, const char* body);

  tools::ToolInput input_;
  mantras::Library lib_;
  std::unique_ptr<mantras::Mantra> mantra_;
  std::unique_ptr<char[]> about_;
  bool kannadaFont_ = false;
  bool transitionPending_ = true;

  Screen screen_ = Screen::Loading;
  Screen stack_[kMaxStack] = {};
  int depth_ = 0;

  // Settings
  enum class Meaning : uint8_t { Both, Kannada, English };
  Meaning meaning_ = Meaning::Both;  // Kannada then English by default; Settings cycles
  int mala_ = 108;
  char ritualKey_[56] = "Morning routine";
  char dayDeity_[7][24] = {"surya", "shiva", "hanuman", "vishnu", "dattatreya", "lakshmi", "venkateshwara"};

  // Clock and today's mantra
  bool clockValid_ = false;
  int32_t today_ = 0;
  int minuteNow_ = 0;
  int weekday_ = 0;  // 0 = Sunday
  int todayCategory_ = -1;
  int todayMantra_ = -1;
  mantras::Category todayCat_;
  int homeSel_ = 0;

  // Groups
  Group group_ = Group::Deities;
  GroupRow groupRows_[64];  // valid up to groupCount_
  int groupCount_ = 0;
  int groupSel_ = 0;

  // List of one category
  int listCat_ = -1;
  mantras::Category listInfo_;
  int listSel_ = 0;
  int previewTop_ = -1;
  Preview previews_[kPreviewRows];  // filled by loadPreviews() before the list is shown

  // Daily ritual
  int ritualCat_ = -1;
  mantras::Category ritualInfo_;
  char steps_[kMaxSteps][64];  // valid up to stepCount_
  int stepCount_ = 0;
  int ritualSel_ = 0;

  // Mantra view
  uint16_t viewIdx_ = 0;
  int viewCat_ = -1;
  int viewKavacha_ = -1;  // >= 0: Left/Right step through that whole kavacha
  mantras::Category viewInfo_;
  int loadedMantra_ = -1;
  int viewPage_ = 0;
  int viewPages_ = 1;  // set by render

  // Japa
  uint16_t japaIdx_ = 0;
  int japaCount_ = 0;
  int todayJapa_ = 0;
  bool malaDone_ = false;

  // About (kavacha)
  int aboutPage_ = 0;
  int aboutPages_ = 1;
};
