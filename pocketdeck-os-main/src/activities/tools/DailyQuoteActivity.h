#pragma once

#include "ToolsCommon.h"
#include "activities/Activity.h"

// Shows the quote for today's date from /tools/quotes.txt.
//
// Line formats (UTF-8, one quote per line):
//   2026-09-27|Quote text — Author   exact date
//   09-27|Quote text — Author        every year on that day
//   *|Quote text — Author            only used for random picks
// A compact offset index (/tools/.cache/quotes.idx, 8 bytes per line) means
// only the single matching line is ever read into RAM. With no match for the
// day, a quote is picked at random (stable for the whole day).
class DailyQuoteActivity final : public Activity {
 public:
  explicit DailyQuoteActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DailyQuote", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr char kQuotesPath[] = "/tools/quotes.txt";
  static constexpr char kIndexPath[] = "/tools/.cache/quotes.idx";
  static constexpr size_t kLineCap = 2048;  // longest quote line, in bytes

  enum class Status : uint8_t { Ok, MissingFile, Empty };

  bool ensureIndex(uint32_t& count);
  static bool buildIndex(FsFile& out, void* ctx);
  void showDay(int32_t day);
  void showRandom(uint32_t seed);
  bool readQuoteAt(uint32_t offset);

  tools::ToolInput input_;
  char line_[kLineCap] = {};  // current quote line; also the index-build scratch buffer
  const char* text_ = "";
  const char* author_ = "";
  Status status_ = Status::Ok;
  int32_t today_ = 0;
  int32_t shownDay_ = 0;
  bool clockValid_ = false;
  bool randomPick_ = false;
  int page_ = 0;  // Up/Down page within a long quote
  int pageCount_ = 1;
  unsigned long lastPollMs_ = 0;
  bool transitionPending_ = true;
};
