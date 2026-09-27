#pragma once

#include "ToolsCommon.h"
#include "activities/Activity.h"

// Knowledge: question-and-answer study topics from /tools/knowledge/<topic>.json.
//
//   {"topic": "Python", "cards": [{"question": "...", "answer": "..."}]}
//   (a bare JSON array of {"question", "answer"} objects also works)
//
// Pick a topic, read the question, press Confirm for the answer. Long answers
// are paged (Left/Right, "Page 2/4"); Up/Down moves between questions. Each
// topic opens on its question of the day. A per-topic offset index in
// /tools/.cache keeps only the current item in RAM.
class KnowledgeCardActivity final : public Activity {
 public:
  explicit KnowledgeCardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Knowledge", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr char kDir[] = "/tools/knowledge";
  static constexpr int kMaxTopics = 16;
  static constexpr size_t kNameCap = 40;
  static constexpr uint32_t kMaxCards = 500;
  static constexpr size_t kQuestionCap = 400;
  static constexpr size_t kAnswerCap = 4096;

  enum class Screen : uint8_t { Topics, Question, Answer, Error };

  void scanTopics();
  bool openTopic(int topic);
  bool ensureIndex(uint32_t& count) const;
  static bool buildIndex(FsFile& out, void* ctx);
  bool loadItem(int index);
  void topicPath(char* buf, size_t len, int topic) const;
  void indexPath(char* buf, size_t len, int topic) const;

  tools::ToolInput input_;
  Screen screen_ = Screen::Topics;
  char topics_[kMaxTopics][kNameCap] = {};
  uint16_t topicCounts_[kMaxTopics] = {};
  int topicCount_ = 0;
  int selectedTopic_ = 0;
  int openTopic_ = -1;
  int itemCount_ = 0;
  int item_ = 0;
  int todayItem_ = 0;
  int32_t today_ = 0;
  int page_ = 0;
  int pageCount_ = 1;
  char question_[kQuestionCap] = {};
  char answer_[kAnswerCap] = {};
  bool transitionPending_ = true;
};
