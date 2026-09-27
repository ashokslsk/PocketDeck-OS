#pragma once

#include <cstdint>

// Whole-library summary for the Reading Stats "Library" page: what is on the
// SD card and how much of it has been read. Scanning happens once, when the
// page is first shown.
struct LibrarySummary {
  static constexpr int kMaxCurrent = 4;
  struct Current {
    char title[48];
    float progress;  // 0..100
  };

  bool scanned = false;
  uint16_t folders = 0;  // folders that contain at least one book
  uint16_t books = 0;    // EPUB, XTC and TXT files
  uint16_t opened = 0;   // books with reading data on this device
  uint16_t notOpened = 0;
  uint16_t inProgress = 0;    // opened, started, not finished
  uint16_t finished = 0;      // marked finished or at 100%
  float averageProgress = 0;  // across opened books
  Current current[kMaxCurrent] = {};
  int currentCount = 0;

  // Walks the SD card (skipping hidden folders and /tools) up to 6 levels
  // deep and at most 1000 books.
  static LibrarySummary scan();
};
