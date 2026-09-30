#pragma once

#include "SrsSchedule.h"
#include "ToolsCommon.h"
#include "activities/Activity.h"

// Spaced-repetition flashcards.
//
// Decks: /tools/flashcards/<deck>.json, a JSON array (or {"cards": [...]}) of
// {"question": "...", "answer": "..."} objects. A per-deck offset index in
// /tools/.cache means only the card on screen is ever read into RAM.
//
// Scheduling keeps just two numbers per card (lastReviewed, interval):
//   remembered -> interval = 1 on first success, then doubles (max 180 days)
//   forgot     -> interval = 0, card comes back later in the same session
// A card is due when today >= lastReviewed + interval. State is stored in
// /tools/srs_state.json keyed by deck + a hash of the question, so editing or
// reordering a deck keeps the history of unchanged cards.
class FlashcardsActivity final : public Activity {
 public:
  explicit FlashcardsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Flashcards", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr char kDeckDir[] = "/tools/flashcards";
  static constexpr char kStatePath[] = "/tools/srs_state.json";
  static constexpr int kMaxDecks = 16;
  static constexpr size_t kDeckNameCap = 40;
  static constexpr int kMaxCards = 400;
  static constexpr size_t kQuestionCap = 400;
  static constexpr size_t kAnswerCap = 800;
  static constexpr int kSaveEveryGrades = 10;

  enum class Screen : uint8_t { Decks, Question, Answer, Done, Error };

  void scanDecks();
  bool openDeck(int deck);
  bool ensureIndex();
  static bool buildIndex(FsFile& out, void* ctx);
  void loadState();
  void saveState();
  static bool writeState(FsFile& out, void* ctx);
  bool loadCard(int card);
  void showNext();
  void grade(bool wasRemembered);
  void closeDeck();
  void logSession();
  void openStats();
  void deckPath(char* buf, size_t len) const;
  void indexPath(char* buf, size_t len) const;

  tools::ToolInput input_;
  Screen screen_ = Screen::Decks;
  char decks_[kMaxDecks][kDeckNameCap] = {};
  int deckCount_ = 0;
  int selectedDeck_ = 0;
  int openDeck_ = -1;

  // Per-card scheduling for the open deck (~4 KB, lives only while open).
  int cardCount_ = 0;
  uint32_t hashes_[kMaxCards] = {};
  uint16_t lastReviewed_[kMaxCards] = {};  // days since 2020-01-01, plus one; 0 = never
  uint16_t interval_[kMaxCards] = {};
  uint16_t queue_[kMaxCards] = {};
  int queueHead_ = 0;
  int queueSize_ = 0;
  int currentCard_ = -1;
  int32_t today_ = 0;

  int reviewed_ = 0;
  int remembered_ = 0;
  int forgot_ = 0;
  int gradesSinceSave_ = 0;
  bool dirty_ = false;

  char question_[kQuestionCap] = {};
  char answer_[kAnswerCap] = {};
  bool transitionPending_ = true;
};
