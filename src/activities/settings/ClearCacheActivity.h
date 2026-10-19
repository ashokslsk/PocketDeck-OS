#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"

// Settings > Clear Reading Cache / Clear Prepared Data / Remove Unused Book Data.
// All three walk /.pocketdeck-os/book_reading_cache one shard (00..ff) at a
// time, so memory stays small however many books there are, show progress,
// and stop cleanly on Back.
class ClearCacheActivity final : public Activity {
 public:
  enum class Mode : uint8_t {
    FullReset,     // every book cache (progress included); stats are kept
    PreparedOnly,  // only what the reader rebuilds: layouts, book info, covers...
    Unused,        // caches of books no longer on the card
  };

  explicit ClearCacheActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, Mode mode = Mode::FullReset)
      : Activity("ClearCache", renderer, mappedInput), mode(mode) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool skipLoopDelay() override { return true; }  // Prevent power-saving mode
  bool preventAutoSleep() override { return state == CLEARING; }
  void render(RenderLock&&) override;

 private:
  enum State { WARNING, CLEARING, SUCCESS, FAILED };
  static constexpr int kShards = 256;

  State state = WARNING;
  const Mode mode;
  int shard = 0;
  bool stopped = false;
  int clearedCount = 0;
  int failedCount = 0;
  int checkedCount = 0;
  unsigned long lastDrawMs = 0;

  void goBack() { finish(); }
  void startClearing();
  void processShard(int index);
  void finishClearing();
  void handleFolder(const std::string& folder, const char* name);
  const char* title() const;
};
