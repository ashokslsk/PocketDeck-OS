#pragma once

#include <HalStorage.h>

#include <cstdint>
#include <string>

#include "activities/Activity.h"
#include "activities/tools/ToolsCommon.h"

// Explorer > folder Options > Prepare Folder, and Settings > Prepare Library.
// Gets books ready ahead of time so they open quickly:
//   1. Counting: walks the folder tree, counts the books and builds the sorted
//      list (folder index) Explorer uses for any folder over 200 items.
//   2. Preparing: reads each EPUB's book info (spine, contents, metadata) into
//      its cache folder, the main cost of a book's first open.
// Chapter layouts depend on the reading font and size, so they are left to the
// reader (Prepare Whole Book, or as you read). Back stops at any point; work
// already done is kept, and running it again skips books that are ready.
class PrepareLibraryActivity final : public Activity {
 public:
  PrepareLibraryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string rootPath,
                         bool wholeLibrary)
      : Activity("PrepareLibrary", renderer, mappedInput),
        rootPath_(std::move(rootPath)),
        wholeLibrary_(wholeLibrary) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return phase_ != Phase::Done; }
  bool skipLoopDelay() override { return phase_ != Phase::Done; }

 private:
  enum class Phase : uint8_t { Counting, Preparing, Done };
  static constexpr int kMaxDepth = 12;
  struct Level {
    HalFile dir;
    std::string path;
    uint32_t accepted = 0;  // entries Explorer would list here
    uint32_t consumed = 0;  // directory entries read so far (to resume after an error)
    uint8_t retries = 0;
    uint32_t fingerprint = 0;  // FsHelpers::entryHash sum, for Explorer's folder mark
    uint32_t entries = 0;
    bool readError = false;    // gave up on part of this folder: no mark
  };
  // A failed directory read (low memory or a card error) looks like the end of
  // the folder; reopen it and skip back to where the walk was instead.
  bool resumeLevel(Level& level);
  uint32_t folderErrors_ = 0;
  // Batches: a short pause after every kBatchSize books, and before each book
  // a free-memory check that waits (up to a limit) for the heap to recover.
  static constexpr uint32_t kBatchSize = 20;
  bool pendingBook_ = false;     // bookPath_ found, waiting to be prepared
  uint32_t inBatch_ = 0;
  uint8_t memoryWaits_ = 0;
  unsigned long pauseUntilMs_ = 0;
  std::string currentTitle_;     // shown on screen: title of the book in hand

  // /.pocketdeck-os/prepare-report.txt: one run's summary plus a line per
  // problem (unreadable book, folder read error, early end) with the memory
  // at that moment. Written as problems happen, so it survives a crash.
  static constexpr uint32_t kMaxReportLines = 400;
  uint32_t reportLines_ = 0;
  void reportBegin();
  void reportLine(const char* kind, const std::string& path, const char* detail = nullptr);
  void reportEnd();

  bool beginWalk();
  void endWalk();
  // Walks on to the next EPUB within the time budget. 1: found (bookPath_),
  // 0: walk finished, -1: budget spent.
  int nextBook(unsigned long budgetMs);
  void prepareBook();

  std::string rootPath_;
  bool wholeLibrary_;
  Phase phase_ = Phase::Counting;
  bool stopped_ = false;
  Level stack_[kMaxDepth];
  int depth_ = 0;
  std::string bookPath_;
  uint32_t totalBooks_ = 0;
  uint32_t foldersIndexed_ = 0;
  uint32_t doneBooks_ = 0;
  uint32_t preparedNow_ = 0;
  uint32_t alreadyReady_ = 0;
  uint32_t failed_ = 0;
  unsigned long prepareMs_ = 0;
  unsigned long startMs_ = 0;
  unsigned long finishedMs_ = 0;
  unsigned long lastDrawMs_ = 0;
  bool firstDraw_ = true;
  tools::ToolInput input_;
};
