#pragma once

#include "PanchangaFestivals.h"
#include "PanchangaMath.h"
#include "PanchangaStrings.h"
#include "ToolsCommon.h"
#include "activities/Activity.h"

// Offline panchanga in Kannada or English: tithi, vara, nakshatra, yoga,
// karana, masa, samvatsara, sunrise/sunset, Rahu/Yamaganda/Gulika kaala,
// Abhijit muhurta, festivals and a Moon-phase widget, for any date from 1976
// to 2075.
//
// Kannada text is shaped at run time with the SD card font
// (/tools/fonts/kannada.knf, KannadaText.h); without it the screen falls back
// to English. Festivals and holidays come from the JSON layers in
// /tools/panchanga/festivals (PanchangaFestivals.h), with the engine's own
// calculation filling years a layer does not cover and adding the monthly
// observances. Confirm opens a menu: today, calendar, month and year, day
// details, language, and more. Settings live in /tools/panchanga.txt.
class PanchangaActivity final : public Activity {
 public:
  explicit PanchangaActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Panchanga", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  // First and last dates the Panchanga shows (matches the festival calendars).
  static int32_t firstDay();
  static int32_t lastDay();

 private:
  static constexpr char kConfigPath[] = "/tools/panchanga.txt";
  static constexpr int kAnimFrames = 5;
  static constexpr unsigned long kAnimFrameMs = 220;
  static constexpr int kMaxDayNames = 5;

  enum class Sty : uint8_t { Label, Value, Title };
  enum class Screen : uint8_t { Main, Details };

  // One name for the selected day: from a festival layer or the engine.
  struct DayName {
    char kannada[96];
    char english[88];
    bool holiday;
    bool layer;          // from a festival file (false = calculated by the engine)
    int8_t special;      // panchanga::Special for calculated names
    festivals::Ref ref;  // where a festival-file name came from
  };

  void loadConfig();
  bool saveConfig() const;
  static bool writeConfig(FsFile& out, void* ctx);
  void openMenu();
  void openCalendar(bool jump);
  void openDetails();
  bool kannadaMode() const;
  // Draws one Panchanga string in the active language with its line box top at
  // y (row top); returns its width. draw=false only measures.
  // k is a packed kn:: string (PanchangaStrings.h); textUtf8 takes plain UTF-8.
  int text(const char* k, const char* e, int x, int y, Sty sty, bool draw = true) const;
  int textUtf8(const char* k, const char* e, int x, int y, Sty sty, bool draw = true) const;
  void selectDay(int32_t day);
  void loadDayNames();
  void startAnimation();
  void renderMain();
  void renderDetails();

  tools::ToolInput input_;
  festivals::Calendar calendar_;
  double lat_ = 12.9716, lon_ = 77.5946, tz_ = 5.5;
  bool defaultPlace_ = true;
  bool animate_ = true;
  bool english_ = false;
  bool clockValid_ = false;
  bool kannadaFont_ = false;
  bool indexPending_ = true;  // festival index is refreshed after the first frame
  Screen screen_ = Screen::Main;
  int32_t today_ = 0;
  int32_t selected_ = 0;
  panchanga::Day day_;
  DayName names_[kMaxDayNames];  // valid up to nameCount_
  int nameCount_ = 0;
  int detailsTop_ = 0;           // first name shown on the details screen
  double moonElongation_ = 0.0;  // at noon (or now, for today)
  int moonNakshatra_ = 0;
  int animFrame_ = kAnimFrames;  // == kAnimFrames when idle
  unsigned long animLastMs_ = 0;
  bool transitionPending_ = true;
};
