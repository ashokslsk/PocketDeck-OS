#pragma once

#include "ToolsCommon.h"
#include "activities/Activity.h"

// Today at a glance: date, time, habit/focus summary and a checkable to-do
// list persisted to /tools/daily.json. Unfinished items carry over to the
// next day; items completed on an earlier day are cleared automatically.
class DailyCommandCenterActivity final : public Activity {
 public:
  explicit DailyCommandCenterActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DailyCommandCenter", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr char kDataPath[] = "/tools/daily.json";
  static constexpr int kMaxTodos = 20;
  static constexpr size_t kTextCap = 80;
  static constexpr unsigned long kSaveDelayMs = 1500;

  struct Todo {
    char text[kTextCap];
    bool done;
  };

  void load();
  void save();
  static bool writeJson(FsFile& out, void* ctx);
  void markDirty();
  void rollOverDay(int32_t today);
  void addTodo(const char* text);
  void removeTodo(int index);
  void clearCompleted();
  void openAddTask();
  int rowCount() const { return count_ + 2; }  // items + "Add task" + "Clear completed"

  tools::ToolInput input_;
  Todo todos_[kMaxTodos] = {};
  int count_ = 0;
  int32_t dataDay_ = 0;
  int selected_ = 0;
  int top_ = 0;
  int visibleRows_ = 1;
  int habitsDone_ = 0;
  int habitCount_ = 0;
  uint16_t focusToday_ = 0;
  int lastMinute_ = -1;
  unsigned long lastPollMs_ = 0;
  bool dirty_ = false;
  unsigned long dirtySinceMs_ = 0;
  bool transitionPending_ = true;
};
