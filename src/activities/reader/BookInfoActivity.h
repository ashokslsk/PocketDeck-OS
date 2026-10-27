#pragma once

#include <string>

#include "../Activity.h"
#include "BookReadingStats.h"
#include "GlobalReadingStats.h"

// Reader menu > Book Info: one popup card with the book's metadata, a progress
// bar and this book's reading stats (start date, pages, time, estimated finish,
// streaks and days off). Read-only; Back or Confirm closes it. Everything is
// prepared by the reader before opening, so this screen does no SD work.
struct BookInfoData {
  std::string title;
  std::string author;
  std::string path;  // the book's file on the SD card, shown at the card's foot
  char publisher[64] = {};  // empty when the book does not name one
  float progressPercent = -1.0f;
  BookReadingStats stats;  // includes the current session's time
  uint32_t pagesDone = 0;   // page you are on of pagesTotal (0: not known)
  uint32_t pagesTotal = 0;
  bool hasTimeLeft = false;
  uint32_t timeLeftSeconds = 0;
  GlobalReadingStats globalStats;  // day-by-day history, used for streaks and days off
};

class BookInfoActivity final : public Activity {
 public:
  BookInfoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, BookInfoData data)
      : Activity("BookInfo", renderer, mappedInput), data_(std::move(data)) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool allowPowerAsConfirmInReaderMode() const override { return true; }

 private:
  // Up to this many "Label  value" rows under the progress bar.
  static constexpr int kMaxRows = 10;
  struct Row {
    const char* label;
    char value[40];
  };

  void buildRows();

  BookInfoData data_;
  Row rows_[kMaxRows] = {};
  int rowCount_ = 0;
  bool ignoreInitialBackRelease_ = false;
  bool ignoreInitialConfirmRelease_ = false;
  bool ignoreInitialPowerRelease_ = false;
};
