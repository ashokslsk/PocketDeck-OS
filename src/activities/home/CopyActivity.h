#pragma once

#include <HalStorage.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"

// Browse Files > Options > Copy to…: copies a file or a whole folder with a
// progress screen. The work runs in short slices from loop(), so the bar keeps
// moving and Back cancels at any moment. A cancelled or failed copy removes
// everything it wrote, so no half-copied book is left behind. Copies start
// fresh: reading progress stays with the original.
//
// Result: FilePathResult{destination} on success; on failure "" (write error,
// e.g. card full), "*" (too many items) or "!" (not enough free space);
// isCancelled when the reader pressed Back.
class CopyActivity final : public Activity {
 public:
  CopyActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string sourcePath, std::string destPath,
               bool isDirectory)
      : Activity("Copy", renderer, mappedInput),
        src_(std::move(sourcePath)),
        dst_(std::move(destPath)),
        isDirectory_(isDirectory) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  static constexpr size_t kMaxItems = 1000;

 private:
  struct Item {
    std::string rel;  // path below the copied root ("" for the root itself)
    uint32_t size;
    bool isDirectory;
  };

  bool scan(const std::string& dir, const std::string& rel);
  bool copySlice(unsigned long budgetMs);
  void closeFiles();
  void removeDestination();
  void finishWith(bool success, bool cancelled);

  std::string src_;
  std::string dst_;
  bool isDirectory_;
  std::vector<Item> items_;
  size_t index_ = 0;
  uint64_t totalBytes_ = 0;
  uint64_t doneBytes_ = 0;
  HalFile in_;
  HalFile out_;
  bool fileOpen_ = false;
  std::unique_ptr<uint8_t[]> buffer_;
  static constexpr size_t kBufferSize = 8192;
  bool tooMany_ = false;
  bool noSpace_ = false;
  bool done_ = false;
  unsigned long lastDrawMs_ = 0;
};
