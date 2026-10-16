#pragma once

#include "MedicineData.h"
#include "StatsExport.h"
#include "ToolsCharts.h"
#include "ToolsCommon.h"
#include "activities/Activity.h"

// Medicine & supplement courses: each course has 1-4 doses a day for a set
// number of days. The list shows today's doses as boxes to tick; Details
// shows when the course started, when it ends / ended / was stopped, doses
// taken versus planned, adherence, missed doses, on-time rate, average delay
// and a day-by-dose grid. Left opens the course menu, Right moves between
// today's doses and Confirm ticks the selected one.
class MedicineActivity final : public Activity {
 public:
  explicit MedicineActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Medicine", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Screen : uint8_t { List, Details };

  void reload();
  void openMenu();
  void addCourseName();
  void addCourseDoses();
  void addCourseFood();
  void addCourseDays();
  void addCourseStart();
  void confirmCourse();
  void stopCourse();
  void deleteCourse();
  void openDetails();
  void computeDetails();
  void renderList(const Rect& content);
  void renderDetails(const Rect& content);
  bool todayIndex(const meds::Course& c, int& index) const;

  tools::ToolInput input_;
  meds::Course courses_[meds::kMaxCourses] = {};
  int count_ = 0;
  int selected_ = 0;
  int slot_ = 0;
  int32_t today_ = 0;
  int16_t nowMinute_ = 0;
  bool clockValid_ = false;
  Screen screen_ = Screen::List;
  // Add-course wizard state.
  meds::Course draft_ = {};
  // Details screen.
  charts::Stat stats_[10] = {};
  int16_t doseMinute_[meds::kMaxDays * meds::kMaxDoses] = {};
  uint8_t exportState_ = 0;  // details screen: 0 idle, 1 working, 2 saved, 3 failed
  bool transitionPending_ = true;
};
