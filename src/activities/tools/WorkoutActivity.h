#pragma once

#include <memory>

#include "KannadaText.h"
#include "ToolsCommon.h"
#include "WorkoutData.h"
#include "activities/Activity.h"

// Workout: pranayama, yoga, gym and longevity practices from the SD card
// (/tools/workout, see WorkoutData.h). The firmware only holds the menus, the
// timer engine and the drawing; every practice, figure and timing comes from
// the manifests.
//
// Flow: Categories > Practices > Detail (figure, names, how-to, level) >
// optional checklist > optional opening verse > Session > optional verse
// pages (after a completed session) > Summary. Steps may carry a mantra,
// its meaning and the breath, shown with the figure (Surya Namaskara). A session is a timeline of
// segments built from the practice type:
//   breath    phases (inhale / hold / exhale ...) for N minutes, a breathing
//             circle grows and shrinks with the phase
//   hold      a timed pose, once per side with a "switch sides" pause
//   sequence  rounds of steps (Surya Namaskara, Baduanjin ...), one figure
//             per step
//   interval  work / rest sets with a manual rep counter
// The screen redraws once a second with a fast partial refresh (a clean
// refresh every 60 updates); hold and interval segment changes flash the
// screen as a visual cue. Every session of 10 seconds or more is appended to
// /tools/workout/log-YYYY-MM.txt as
//   category|id|seconds|count|reps|level|done
// for the stats page and the /stats/workout export.
class WorkoutActivity final : public Activity {
 public:
  explicit WorkoutActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Workout", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return screen_ == Screen::Session && running_; }

 private:
  enum class Screen : uint8_t { Missing, Categories, List, Detail, Checklist, Opening, Session, Verses, Summary };
  enum class SegKind : uint8_t { Phase, BreathRest, Hold, Switch, Step, Work, Recover };
  struct Seg {
    SegKind kind;
    uint8_t index;  // phase, side, step, or set
    uint16_t seconds;
  };
  static constexpr int kTextCap = 3072;
  static constexpr uint16_t kSwitchSeconds = 5;
  static constexpr uint32_t kMinLogSeconds = 10;

  // Navigation
  void show(Screen s);
  void openCategory(int index);
  void openItem(int index);
  void openStats();
  void buildDetailText();
  void requestStart();  // the opening verse first, when the practice has one
  void openVerse(int index);
  void loopSession();

  // Session engine
  void startSession();
  int segCount() const;
  Seg seg(int i) const;
  uint32_t elapsedMs() const;
  uint32_t totalMs() const;  // breath only: the practice length
  void advance(bool completed);
  void togglePause();
  void finishSession(bool done);

  // Drawing
  bool useKannada(const char* text) const;
  int drawKannadaLine(const char* text, kannada::Style s, int x, int latinTop, int font, int maxW, bool black) const;
  void timingLine(char* buf, size_t len) const;
  const char* levelName() const;
  void renderMessage(const char* title, const char* body);
  void renderCategories();
  void renderList();
  void renderDetail();
  void renderChecklist();
  void renderSession();
  void renderBreath(const Rect& c, const Seg& s, uint32_t segElapsedMs);
  void renderFigureSession(const Rect& c, const Seg& s, uint32_t segElapsedMs);
  void renderSummary();
  void renderVerse();
  void renderMantraStep(const Rect& c, const Seg& s, uint32_t segElapsedMs);

  tools::ToolInput input_;
  bool transitionPending_ = true;
  Screen screen_ = Screen::Categories;

  workout::CategoryInfo cats_[workout::kMaxCategories];
  int catCount_ = 0;
  int catSel_ = 0;
  int openCat_ = -1;
  std::unique_ptr<workout::Category> cat_;
  int itemSel_ = 0;
  std::unique_ptr<workout::Item> item_;
  std::unique_ptr<char[]> text_;  // the detail page (Markdown subset)
  // 0 Rookie, 1 Beginner, 2 Expert, 3 Veteran, 4 Go Nuts. The practice files
  // hold Beginner and Advanced values; Beginner = file Beginner, Veteran = file
  // Advanced, and the other three are derived (see levelValue()).
  static constexpr int kLevelCount = 5;
  int level_ = 1;
  enum class LevelField : uint8_t { Minutes, HoldSeconds, Rounds, StepSeconds, Sets, Work, Rest };
  uint16_t levelValue(LevelField field, uint16_t beginner, uint16_t advanced) const;
  // Slow pace (remembered in /tools/workout/pace.txt): slow-breathing phases
  // become 10 s in / 15 s out (holds and fast breathing as written) and
  // every sequence pose (Sun Salutation...) lasts at least 30 s.
  bool slowPace_ = false;  // with level_, remembered in /tools/workout/pace.txt
  uint16_t phaseSeconds(const workout::Phase& phase) const;
  uint16_t stepSeconds(uint16_t written) const;
  void savePace() const;
  int detailPage_ = 0;
  int detailPages_ = 1;  // set by render
  bool checklistShown_ = false;

  // Verse pages (mantra_pages): the opening verse and the pages after the last round
  std::unique_ptr<workout::Verse> verse_;
  uint32_t verseOffsets_[workout::kMaxVerses] = {};
  uint32_t openingOffset_ = 0;
  int verseCount_ = 0;
  int verseIdx_ = 0;

  // Session
  bool running_ = false;
  uint32_t runStartMs_ = 0;  // millis() when the current run began
  uint32_t accMs_ = 0;       // elapsed ms from earlier runs (pauses)
  uint32_t segStartMs_ = 0;  // session time when the current segment began
  int segIdx_ = 0;
  uint16_t count_ = 0;  // completed cycles, holds, rounds or sets
  uint16_t reps_ = 0;
  uint32_t lastShown_ = UINT32_MAX;
  uint16_t fastRefreshes_ = 0;
  bool flashPending_ = false;
  bool redrawAfterFlash_ = false;

  // Summary
  uint32_t durationSec_ = 0;
  bool done_ = false;
  bool saved_ = false;
};
