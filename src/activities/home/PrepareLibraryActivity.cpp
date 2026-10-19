#include "PrepareLibraryActivity.h"

#include <Arduino.h>
#include <Epub.h>
#include <FileIndex.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <HalClock.h>
#include <MemoryBudget.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "FileBrowserActivity.h"
#include "components/UITheme.h"
#include "util/PrepareEta.h"
#include "util/PreparedMarks.h"
#include "CrossPointSettings.h"
#include "fontIds.h"

namespace {
constexpr unsigned long kWalkSliceMs = 40;
constexpr unsigned long kRedrawMs = 1500;
// Explorer switches a folder to its saved sorted index above this many items.
constexpr uint32_t kIndexThreshold = 200;
constexpr char kDataDir[] = "/.pocketdeck-os";
constexpr char kReportPath[] = "/.pocketdeck-os/prepare-report.txt";

std::string childPath(const std::string& parent, const char* name) {
  std::string path = parent;
  if (path.empty() || path.back() != '/') path += '/';
  path += name;
  return path;
}

// "12345" -> "12,345"
std::string grouped(const uint32_t n) {
  char raw[12];
  snprintf(raw, sizeof(raw), "%u", static_cast<unsigned>(n));
  std::string out;
  const int len = static_cast<int>(strlen(raw));
  for (int i = 0; i < len; ++i) {
    if (i > 0 && (len - i) % 3 == 0) out += ',';
    out += raw[i];
  }
  return out;
}
}  // namespace

void PrepareLibraryActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  startMs_ = millis();
  reportBegin();
  phase_ = beginWalk() ? Phase::Counting : Phase::Done;
  requestUpdate();
}

void PrepareLibraryActivity::onExit() {
  endWalk();
  Activity::onExit();
}

bool PrepareLibraryActivity::beginWalk() {
  endWalk();
  stack_[0].dir = Storage.open(rootPath_.c_str());
  if (!stack_[0].dir || !stack_[0].dir.isDirectory()) {
    if (stack_[0].dir) stack_[0].dir.close();
    return false;
  }
  stack_[0].path = rootPath_;
  stack_[0].accepted = 0;
  stack_[0].consumed = 0;
  stack_[0].retries = 0;
  stack_[0].fingerprint = 0;
  stack_[0].entries = 0;
  stack_[0].readError = false;
  depth_ = 1;
  return true;
}

void PrepareLibraryActivity::endWalk() {
  for (int i = depth_ - 1; i >= 0; --i) {
    if (stack_[i].dir) stack_[i].dir.close();
  }
  depth_ = 0;
}

int PrepareLibraryActivity::nextBook(const unsigned long budgetMs) {
  char name[256];  // FAT long names; on the activity task's stack only for this call
  const unsigned long start = millis();
  while (millis() - start < budgetMs) {
    if (depth_ == 0) return 0;
    Level& level = stack_[depth_ - 1];
    HalFile entry = level.dir.openNextFile();
    if (!entry && FsHelpers::directoryIterationFailed(level.dir)) {
      char detail[64];
      snprintf(detail, sizeof(detail), "%s after %u entries", level.dir.allocationFailed() ? "out of memory" : "card read error",
               static_cast<unsigned>(level.consumed));
      reportLine("FOLDER READ ERROR", level.path, detail);
      if (resumeLevel(level)) continue;
      folderErrors_++;
      level.readError = true;
      LOG_ERR("PREP", "Giving up on folder after read errors: %s", level.path.c_str());
    }
    if (!entry) {
      level.dir.close();
      if (phase_ == Phase::Preparing && !level.readError) {
        // Every book here has been handled: Explorer shows this folder ticked
        // until its contents change.
        PreparedMarks::writeFolderMark(level.path, level.fingerprint, level.entries);
      }
      if (phase_ == Phase::Counting && level.accepted > kIndexThreshold) {
        // Build (or refresh) Explorer's sorted list now, so the folder opens
        // instantly later.
        FileIndex index;
        if (index.open(level.path.c_str(), &FileBrowserActivity::acceptBrowserEntry)) foldersIndexed_++;
      }
      --depth_;
      continue;
    }
    level.consumed++;
    entry.getName(name, sizeof(name));
    const bool isDir = entry.isDirectory();
    // Hidden folders (the app's own data among them) are never walked.
    if (name[0] == '.' || !FileBrowserActivity::acceptBrowserEntry(name, isDir)) {
      entry.close();
      continue;
    }
    level.accepted++;
    if (const uint32_t h = FsHelpers::entryHash(name, strlen(name), isDir)) {
      level.fingerprint += h;
      level.entries++;
    }
    if (isDir) {
      if (depth_ < kMaxDepth) {
        Level& child = stack_[depth_];
        child.path = childPath(level.path, name);
        child.accepted = 0;
        child.consumed = 0;
        child.retries = 0;
        child.fingerprint = 0;
        child.entries = 0;
        child.readError = false;
        child.dir = std::move(entry);
        depth_++;
      } else {
        entry.close();
      }
      continue;
    }
    entry.close();
    if (FsHelpers::hasEpubExtension(std::string_view{name})) {
      bookPath_ = childPath(level.path, name);
      return 1;
    }
  }
  return -1;
}

void PrepareLibraryActivity::reportBegin() {
  reportLines_ = 0;
  HalFile file;
  if (!Storage.openFileForWrite("PREP", kReportPath, file)) return;
  char when[48] = "unknown time";
  char date[24];
  char clock[16];
  if (halClock.formatDate(date, sizeof(date), SETTINGS.clockUtcOffsetQ) &&
      halClock.formatTime(clock, sizeof(clock), SETTINGS.clockUtcOffsetQ, SETTINGS.clockFormat == 1)) {
    snprintf(when, sizeof(when), "%s %s", date, clock);
  }
  const auto heap = MemoryBudget::snapshot();
  char header[256];
  const int n = snprintf(header, sizeof(header), "PocketDeck-OS prepare report\nStarted: %s\nFolder: %s\nMemory at start: %u KB free, largest block %u KB\n\n",
                         when, rootPath_.c_str(), static_cast<unsigned>(heap.freeHeap / 1024),
                         static_cast<unsigned>(heap.maxAllocHeap / 1024));
  if (n > 0) file.write(reinterpret_cast<const uint8_t*>(header), std::min<size_t>(n, sizeof(header) - 1));
  file.close();
}

void PrepareLibraryActivity::reportLine(const char* kind, const std::string& path, const char* detail) {
  if (reportLines_ >= kMaxReportLines) return;
  HalFile file = Storage.open(kReportPath, O_WRONLY | O_CREAT | O_APPEND);
  if (!file) return;
  // "<kind> | memory <free> KB free, largest <block> KB | <detail>" then the path.
  const auto heap = MemoryBudget::snapshot();
  char head[160];
  const int n = snprintf(head, sizeof(head), "%s | memory %u KB free, largest %u KB | %s\n    ", kind,
                         static_cast<unsigned>(heap.freeHeap / 1024), static_cast<unsigned>(heap.maxAllocHeap / 1024),
                         detail ? detail : "");
  if (n > 0) file.write(reinterpret_cast<const uint8_t*>(head), std::min<size_t>(n, sizeof(head) - 1));
  file.write(reinterpret_cast<const uint8_t*>(path.data()), path.size());
  file.write(reinterpret_cast<const uint8_t*>("\n"), 1);
  file.close();
  if (++reportLines_ == kMaxReportLines) {
    HalFile more = Storage.open(kReportPath, O_WRONLY | O_CREAT | O_APPEND);
    if (more) {
      static constexpr char kMore[] = "(more problems not listed)\n";
      more.write(reinterpret_cast<const uint8_t*>(kMore), sizeof(kMore) - 1);
      more.close();
    }
  }
}

void PrepareLibraryActivity::reportEnd() {
  if (!stopped_ && doneBooks_ < totalBooks_) {
    char detail[64];
    snprintf(detail, sizeof(detail), "walk ended early: checked %u of %u books", static_cast<unsigned>(doneBooks_),
             static_cast<unsigned>(totalBooks_));
    reportLine("EARLY END", rootPath_, detail);
  }
  HalFile file = Storage.open(kReportPath, O_WRONLY | O_CREAT | O_APPEND);
  if (!file) return;
  const auto heap = MemoryBudget::snapshot();
  char buf[320];
  const int n = snprintf(buf, sizeof(buf),
                         "\n%s after %lu s\nBooks counted: %u\nChecked: %u\nPrepared now: %u\nAlready ready: %u\n"
                         "Could not read: %u\nFolder errors: %u\nFolder lists saved: %u\nMemory at end: %u KB free, "
                         "largest block %u KB\n",
                         stopped_ ? "Stopped" : "Finished", static_cast<unsigned long>((millis() - startMs_) / 1000),
                         static_cast<unsigned>(totalBooks_), static_cast<unsigned>(doneBooks_),
                         static_cast<unsigned>(preparedNow_), static_cast<unsigned>(alreadyReady_),
                         static_cast<unsigned>(failed_), static_cast<unsigned>(folderErrors_),
                         static_cast<unsigned>(foldersIndexed_), static_cast<unsigned>(heap.freeHeap / 1024),
                         static_cast<unsigned>(heap.maxAllocHeap / 1024));
  if (n > 0) file.write(reinterpret_cast<const uint8_t*>(buf), std::min<size_t>(n, sizeof(buf) - 1));
  file.close();
}

bool PrepareLibraryActivity::resumeLevel(Level& level) {
  if (level.retries >= 3) return false;
  level.retries++;
  LOG_ERR("PREP", "Directory read failed in %s after %u entries; reopening (try %u)", level.path.c_str(),
          static_cast<unsigned>(level.consumed), static_cast<unsigned>(level.retries));
  level.dir.close();
  delay(100);  // let memory and the card settle
  level.dir = Storage.open(level.path.c_str());
  if (!level.dir || !level.dir.isDirectory()) return false;
  // Skip what was already handled; a failed entry was not counted, so it is
  // read again.
  for (uint32_t i = 0; i < level.consumed; ++i) {
    HalFile skipped = level.dir.openNextFile();
    if (!skipped) return false;
    skipped.close();
  }
  return true;
}

void PrepareLibraryActivity::prepareBook() {
  // Until its info is read, a book is known by its file name.
  const char* slash = strrchr(bookPath_.c_str(), '/');
  currentTitle_ = slash ? slash + 1 : bookPath_;
  const size_t dot = currentTitle_.rfind('.');
  if (dot != std::string::npos && dot > 0) currentTitle_.resize(dot);
  if (Epub::hasCache(bookPath_, kDataDir)) {
    alreadyReady_++;
    return;
  }
  const unsigned long startedAt = millis();
  Epub epub(bookPath_, kDataDir);
  // A cache folder that already exists may hold the book's progress and stats.
  const bool hadCacheFolder = Storage.exists(epub.getCachePath().c_str());
  if (epub.load(/*buildIfMissing=*/true, /*skipLoadingCss=*/true, Epub::XLocationLoadMode::Skip)) {
    preparedNow_++;
    if (!epub.getTitle().empty()) currentTitle_ = epub.getTitle();
  } else {
    failed_++;
    LOG_ERR("PREP", "Could not read book info: %s", bookPath_.c_str());
    reportLine("COULD NOT READ", bookPath_, "not a readable EPUB, damaged, or too little memory");
    // Leave no empty folder behind for a file that is not a readable EPUB.
    if (!hadCacheFolder && Storage.exists(epub.getCachePath().c_str())) {
      Storage.removeDir(epub.getCachePath().c_str());
    }
  }
  prepareMs_ += millis() - startedAt;
}

void PrepareLibraryActivity::loop() {
  input_.poll(mappedInput);
  if (phase_ == Phase::Done) {
    if (input_.back || input_.confirm) finish();
    return;
  }
  if (input_.back) {
    stopped_ = true;
    endWalk();
    phase_ = Phase::Done;
    requestUpdate();
    return;
  }

  if (phase_ == Phase::Counting) {
    for (;;) {
      const int found = nextBook(kWalkSliceMs);
      if (found == 1) {
        totalBooks_++;
        continue;  // counting is cheap; keep going within the slice
      }
      if (found == 0) {
        // Second pass over the same tree prepares the books in the same order.
        phase_ = beginWalk() ? Phase::Preparing : Phase::Done;
      }
      break;
    }
  } else if (millis() >= pauseUntilMs_) {
    if (!pendingBook_) {
      const int found = nextBook(kWalkSliceMs);
      if (found == 1) {
        pendingBook_ = true;
      } else if (found == 0) {
        endWalk();
        phase_ = Phase::Done;
      }
    }
    if (pendingBook_) {
      // Reading book info needs a few tens of KB; let the heap recover first.
      const auto heap = MemoryBudget::snapshot();
      constexpr uint32_t kMinFree = 56U * 1024U;
      constexpr uint32_t kMinMaxAlloc = 28U * 1024U;
      if (!MemoryBudget::hasHeap(heap, kMinFree, kMinMaxAlloc) && memoryWaits_ < 10) {
        memoryWaits_++;
        pauseUntilMs_ = millis() + 500;
      } else {
        memoryWaits_ = 0;
        pendingBook_ = false;
        prepareBook();  // one book per loop: Back is checked between books
        doneBooks_++;
        if (++inBatch_ >= kBatchSize) {
          inBatch_ = 0;
          pauseUntilMs_ = millis() + 300;  // breathe between batches
          LOG_INF("PREP", "Batch done at book %u: free=%u maxAlloc=%u", static_cast<unsigned>(doneBooks_),
                  static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMaxAllocHeap()));
        }
      }
    }
  }

  if (phase_ == Phase::Done || millis() - lastDrawMs_ >= kRedrawMs) {
    lastDrawMs_ = millis();
    requestUpdate();
  }
}

void PrepareLibraryActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int screenW = renderer.getScreenWidth();
  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, screenW, metrics.headerHeight},
                 wholeLibrary_ ? tr(STR_PREPARE_LIBRARY) : tr(STR_PREPARE_FOLDER), "");
  const int x = metrics.contentSidePadding;
  const int w = screenW - 2 * metrics.contentSidePadding;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing + 8;
  const int titleH = renderer.getLineHeight(UI_12_FONT_ID);
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);

  const std::string where = renderer.truncatedText(UI_12_FONT_ID, rootPath_.c_str(), w, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, x, y, where.c_str(), true, EpdFontFamily::BOLD);
  y += titleH + 18;

  // Progress bar: books handled out of the books counted.
  constexpr int kBarH = 24;
  const uint32_t total = std::max<uint32_t>(1, totalBooks_);
  const uint32_t done = phase_ == Phase::Counting ? 0 : std::min(doneBooks_, total);
  renderer.drawRoundedRect(x, y, w, kBarH, 2, kBarH / 2, true);
  const int inner = w - 8;
  int fillW = static_cast<int>(static_cast<int64_t>(inner) * done / total);
  if (phase_ == Phase::Done && !stopped_) fillW = inner;
  if (done > 0 && fillW < kBarH - 8) fillW = kBarH - 8;
  if (fillW > 0) renderer.fillRoundedRect(x + 4, y + 4, fillW, kBarH - 8, (kBarH - 8) / 2, Color::Black);
  y += kBarH + 16;

  char line[112];
  const auto drawLine = [&](const char* text, const bool bold) {
    const std::string fitted = renderer.truncatedText(UI_10_FONT_ID, text, w);
    renderer.drawText(UI_10_FONT_ID, x, y, fitted.c_str(), true, bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    y += lineH + 8;
  };

  if (phase_ == Phase::Counting) {
    snprintf(line, sizeof(line), tr(STR_PREPARE_COUNTING_FORMAT), grouped(totalBooks_).c_str());
    drawLine(line, true);
  } else if (phase_ == Phase::Preparing) {
    snprintf(line, sizeof(line), tr(STR_PREPARE_BOOK_OF_FORMAT), grouped(std::min(doneBooks_ + 1, total)).c_str(),
             grouped(totalBooks_).c_str());
    drawLine(line, true);
    drawLine(currentTitle_.c_str(), false);
    snprintf(line, sizeof(line), tr(STR_PREPARE_ELAPSED_FORMAT), PrepareEta::duration(millis() - startMs_).c_str());
    drawLine(line, false);
    if (preparedNow_ > 0) {
      // Rough: average time per newly prepared book, for the books still to go
      // (books that are already ready go by in a moment, so this errs long).
      const unsigned long avgMs = prepareMs_ / preparedNow_;
      const unsigned long leftMs = avgMs * (total - done);
      if (leftMs < 30000UL) {
        drawLine(tr(STR_PREPARE_UNDER_MINUTE), false);
      } else {
        snprintf(line, sizeof(line), tr(STR_PREPARE_LEFT_FORMAT), PrepareEta::duration(leftMs).c_str());
        drawLine(line, false);
      }
      char clock[16];
      if (PrepareEta::clockAhead(leftMs, clock, sizeof(clock))) {
        snprintf(line, sizeof(line), tr(STR_PREPARE_DONE_AROUND_FORMAT), clock);
        drawLine(line, false);
      }
    }
  } else {
    if (finishedMs_ == 0) {
      finishedMs_ = millis();
      reportEnd();
    }
    const bool incomplete = !stopped_ && (doneBooks_ < totalBooks_ || folderErrors_ > 0);
    drawLine(stopped_ || incomplete ? tr(STR_PREPARE_STOPPED)
             : failed_ > 0          ? tr(STR_PREPARE_LIBRARY_DONE_SOME)
                                    : tr(STR_PREPARE_LIBRARY_DONE),
             true);
    snprintf(line, sizeof(line), tr(STR_PREPARE_TOOK_FORMAT), PrepareEta::duration(finishedMs_ - startMs_).c_str());
    drawLine(line, false);
    snprintf(line, sizeof(line), tr(STR_PREPARE_CHECKED_FORMAT), grouped(doneBooks_).c_str(), grouped(totalBooks_).c_str());
    drawLine(line, false);
  }
  y += 6;
  snprintf(line, sizeof(line), tr(STR_PREPARE_READY_NOW_FORMAT), grouped(preparedNow_).c_str());
  drawLine(line, false);
  snprintf(line, sizeof(line), tr(STR_PREPARE_ALREADY_FORMAT), grouped(alreadyReady_).c_str());
  drawLine(line, false);
  if (failed_ > 0) {
    snprintf(line, sizeof(line), tr(STR_PREPARE_SKIPPED_FORMAT), grouped(failed_).c_str());
    drawLine(line, false);
  }
  if (folderErrors_ > 0) {
    snprintf(line, sizeof(line), tr(STR_PREPARE_FOLDER_ERRORS_FORMAT), grouped(folderErrors_).c_str());
    drawLine(line, false);
  }
  if (foldersIndexed_ > 0) {
    snprintf(line, sizeof(line), tr(STR_PREPARE_FOLDERS_FORMAT), grouped(foldersIndexed_).c_str());
    drawLine(line, false);
  }
  {
    const auto heap = MemoryBudget::snapshot();
    const uint32_t total = std::max<uint32_t>(1, ESP.getHeapSize());
    const uint32_t used = total > heap.freeHeap ? total - heap.freeHeap : 0;
    snprintf(line, sizeof(line), tr(STR_PREPARE_MEMORY_FORMAT), static_cast<unsigned>(used * 100ULL / total),
             static_cast<unsigned>(heap.maxAllocHeap / 1024));
    drawLine(line, false);
  }
  if (phase_ != Phase::Done) drawLine(tr(STR_PREPARE_WORK_KEPT), false);

  const auto labels = phase_ == Phase::Done
                          ? mappedInput.mapLabels(tr(STR_PREPARE_BACK_TO_BOOK), "", "", "")
                          : mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_PREPARE_STOP)), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(firstDraw_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  firstDraw_ = false;
}
