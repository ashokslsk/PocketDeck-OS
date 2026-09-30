#pragma once

#include "PanchangaFestivals.h"
#include "ToolsCommon.h"
#include "activities/Activity.h"

// Date picker for the Panchanga, 1976-2075.
//
// Month grid: Left/Right move a day, Up/Down a month; days with a festival
// have a dot and public holidays a square; the month's special days are
// listed under the grid. Confirm returns the day (IntervalResult.value =
// days since 1970) so the Panchanga opens it.
//
// "Go to month and year" mode: three rows (decade, year, month) chosen with
// Up/Down and changed with Left/Right, so any month in the hundred years is a
// few presses away; Confirm shows that month's grid.
class PanchangaCalendarActivity final : public Activity {
 public:
  PanchangaCalendarActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int32_t selected, int32_t today,
                            bool english, bool startWithJump)
      : Activity("PanchangaCalendar", renderer, mappedInput),
        selected_(selected),
        today_(today),
        english_(english),
        jump_(startWithJump),
        jumpOnly_(startWithJump) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int kMaxItems = 16;
  struct Item {
    uint8_t day;
    bool holiday;
    char name[96];
  };

  void setSelected(int32_t day);
  void loadMonth();
  bool kannadaMode() const;
  // packed: a kn:: string from PanchangaStrings.h.
  int drawName(const char* packed, const char* en, int x, int y, bool bold, bool draw = true) const;
  void renderGrid();
  void renderJump();

  tools::ToolInput input_;
  festivals::Calendar calendar_;
  int32_t selected_;
  int32_t today_;
  bool english_;
  bool jump_;
  bool jumpOnly_;  // opened as "Go to month and year": Back leaves
  bool kannadaFont_ = false;
  int year_ = 2026;
  int month_ = 1;
  int loadedYear_ = 0;
  int loadedMonth_ = 0;
  uint32_t festivalDays_ = 0;  // bit d-1 set: a festival on day d
  uint32_t holidayDays_ = 0;
  Item items_[kMaxItems];  // valid up to itemCount_
  int itemCount_ = 0;
  int jumpRow_ = 1;  // 0 decade, 1 year, 2 month
  bool transitionPending_ = true;
};
