#include "ClearCacheActivity.h"

#include <FsHelpers.h>

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <vector>
#include <utility>
#include <string>
#include <cstring>

#include "MappedInputManager.h"
#include "components/TouchActionButtons.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/BookCacheUtils.h"
#include "util/PreparedMarks.h"
#include "activities/home/BookActions.h"
#include "RecentBooksStore.h"
#include <Epub.h>
#include <Txt.h>
#include <Xtc.h>
#include <Arduino.h>
#include <cstdio>

namespace {

TouchActionButtons::Layout touchActionLayout(const GfxRenderer& renderer) {
  auto& theme = UITheme::getInstance();
  const auto& metrics = theme.getMetrics();
  const Rect screen = theme.getScreenSafeArea(renderer, true, false);
  constexpr int buttonCount = 2;
  const int totalHeight = TouchActionButtons::kDefaultHeight * buttonCount + TouchActionButtons::kDefaultGap;
  const Rect container{screen.x + metrics.contentSidePadding,
                       screen.y + screen.height - metrics.verticalSpacing - totalHeight,
                       std::max(1, screen.width - metrics.contentSidePadding * 2), totalHeight};
  return TouchActionButtons::vertical(container, buttonCount);
}

}  // namespace

void ClearCacheActivity::onEnter() {
  Activity::onEnter();
  state = WARNING;
  requestUpdate();
}

void ClearCacheActivity::onExit() { Activity::onExit(); }

const char* ClearCacheActivity::title() const {
  switch (mode) {
    case Mode::PreparedOnly:
      return tr(STR_CLEAR_PREPARED_DATA);
    case Mode::Unused:
      return tr(STR_CLEAR_UNUSED_DATA);
    case Mode::FullReset:
    default:
      return tr(STR_CLEAR_READING_CACHE);
  }
}

void ClearCacheActivity::render(RenderLock&&) {
  const auto pageHeight = renderer.getScreenHeight();
  const auto pageWidth = renderer.getScreenWidth();
  renderer.clearScreen();

  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, title(), false);
  } else {
    GUI.drawHeader(renderer, header, title());
  }

  if (state == WARNING) {
    const char* lines[4];
    if (mode == Mode::PreparedOnly) {
      lines[0] = tr(STR_CLEAR_PREPARED_WARNING_1);
      lines[1] = tr(STR_CLEAR_PREPARED_WARNING_2);
      lines[2] = tr(STR_CLEAR_PREPARED_WARNING_3);
      lines[3] = tr(STR_CLEAR_PREPARED_WARNING_4);
    } else if (mode == Mode::Unused) {
      lines[0] = tr(STR_CLEAR_UNUSED_WARNING_1);
      lines[1] = tr(STR_CLEAR_UNUSED_WARNING_2);
      lines[2] = tr(STR_CLEAR_UNUSED_WARNING_3);
      lines[3] = tr(STR_CLEAR_UNUSED_WARNING_4);
    } else {
      lines[0] = tr(STR_CLEAR_CACHE_WARNING_1);
      lines[1] = tr(STR_CLEAR_CACHE_WARNING_2);
      lines[2] = tr(STR_CLEAR_CACHE_WARNING_3);
      lines[3] = tr(STR_CLEAR_CACHE_WARNING_4);
    }
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 60, lines[0], true);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 30, lines[1], true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, lines[2], true);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 30, lines[3], true);

    if (mappedInput.hasTouch()) {
      const auto actions = touchActionLayout(renderer);
      const char* labels[] = {tr(STR_CLEAR_BUTTON), tr(STR_CANCEL)};
      TouchActionButtons::draw(renderer, actions, labels, 0, -1, UI_10_FONT_ID);
    } else {
      const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_CLEAR_BUTTON), "", "");
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    }
    renderer.displayBuffer();
    return;
  }

  char line[96];
  if (state == CLEARING) {
    // Progress: shards of book_reading_cache walked.
    const int x = 24;
    const int w = pageWidth - 48;
    const int barY = pageHeight / 2 - 50;
    constexpr int kBarH = 24;
    renderer.drawRoundedRect(x, barY, w, kBarH, 2, kBarH / 2, true);
    const int fillW = std::max(shard > 0 ? kBarH - 8 : 0, (w - 8) * shard / kShards);
    if (fillW > 0) renderer.fillRoundedRect(x + 4, barY + 4, fillW, kBarH - 8, (kBarH - 8) / 2, Color::Black);
    renderer.drawCenteredText(UI_10_FONT_ID, barY + kBarH + 16, tr(STR_CLEARING_CACHE), true, EpdFontFamily::BOLD);
    snprintf(line, sizeof(line), tr(STR_CLEAR_PROGRESS_FORMAT), checkedCount, clearedCount);
    renderer.drawCenteredText(UI_10_FONT_ID, barY + kBarH + 46, line, true);
    const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_PREPARE_STOP)), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }

  if (state == SUCCESS) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, stopped ? tr(STR_PREPARE_STOPPED) : tr(STR_CACHE_CLEARED),
                              true, EpdFontFamily::BOLD);
    std::string resultText = std::to_string(clearedCount) + " " + std::string(tr(STR_ITEMS_REMOVED));
    if (failedCount > 0) {
      resultText += ", " + std::to_string(failedCount) + " " + std::string(tr(STR_FAILED_LOWER));
    }
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, resultText.c_str());
    const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == FAILED) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_CLEAR_CACHE_FAILED), true,
                              EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, tr(STR_CHECK_SERIAL_OUTPUT));
    const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
  }
}

namespace {
// What the reader rebuilds by itself inside a book's cache folder. Everything
// else there (progress.bin, stats, reader settings, lookup history) is kept.
bool isRebuildableBookCacheEntry(const char* name, const bool isDir) {
  if (isDir) return strcmp(name, "sections") == 0 || strcmp(name, "html") == 0;
  static const char* const kFiles[] = {"book.bin", "css_rules.cache", "kannada.flag", ".tmp.css", "prepared.sig",
                                       "layout.sig"};
  for (const char* file : kFiles) {
    if (strcmp(name, file) == 0) return true;
  }
  static const char* const kPrefixes[] = {"img_", "thumb_", "cover_src"};
  for (const char* prefix : kPrefixes) {
    if (strncmp(name, prefix, strlen(prefix)) == 0) return true;
  }
  return false;
}

// Names of the entries in `dir` (one shard, or one book folder: small).
std::vector<std::pair<std::string, bool>> listEntries(const std::string& dir) {
  std::vector<std::pair<std::string, bool>> out;
  char name[128];
  HalFile handle = Storage.open(dir.c_str());
  if (!handle || !handle.isDirectory()) {
    if (handle) handle.close();
    return out;
  }
  for (HalFile entry = handle.openNextFile(); entry; entry = handle.openNextFile()) {
    entry.getName(name, sizeof(name));
    const bool isDir = entry.isDirectory();
    entry.close();
    out.emplace_back(name, isDir);
  }
  handle.close();
  return out;
}

// The cache folder `bookPath` would use for a folder named like `name`.
std::string expectedCacheFolder(const char* name, const std::string& bookPath) {
  constexpr char kDataDir[] = "/.pocketdeck-os";
  if (strncmp(name, "epub_", 5) == 0) return Epub::cachePathForFilePath(bookPath, kDataDir);
  if (strncmp(name, "xtc_", 4) == 0) return Xtc(bookPath, kDataDir).getCachePath();
  if (strncmp(name, "txt_", 4) == 0) return Txt(bookPath, kDataDir).getCachePath();
  return {};
}
}  // namespace

void ClearCacheActivity::handleFolder(const std::string& folder, const char* name) {
  checkedCount++;
  switch (mode) {
    case Mode::FullReset:
      if (clearBookCacheDirectoryPreservingStats(folder.c_str())) {
        clearedCount++;
      } else {
        LOG_ERR("CLEAR_CACHE", "Failed to remove: %s", folder.c_str());
        failedCount++;
      }
      return;
    case Mode::PreparedOnly: {
      bool ok = true;
      bool any = false;
      for (const auto& item : listEntries(folder)) {
        if (!isRebuildableBookCacheEntry(item.first.c_str(), item.second)) continue;
        any = true;
        const std::string path = folder + "/" + item.first;
        if (!(item.second ? Storage.removeDir(path.c_str()) : Storage.remove(path.c_str()))) ok = false;
      }
      if (!ok) {
        failedCount++;
        LOG_ERR("CLEAR_CACHE", "Could not clear all prepared data in %s", folder.c_str());
      } else if (any) {
        clearedCount++;
      }
      return;
    }
    case Mode::Unused: {
      // Delete only when every check agrees the book is gone: the cache names
      // its book, that name maps back to exactly this folder, the book is not
      // on the card, and the cache folder itself still reads (so a card that
      // stops answering can never look like "every book is gone").
      std::string bookPath;
      if (!FsHelpers::readBookSource(folder, bookPath)) return;
      if (expectedCacheFolder(name, bookPath) != folder) return;
      if (Storage.exists(bookPath.c_str())) return;
      if (!Storage.exists(folder.c_str()) || !Storage.exists(FsHelpers::kBookCacheDir)) return;
      LOG_INF("CLEAR_CACHE", "Removing data of missing book %s", bookPath.c_str());
      BookActions::clearFileMetadata(bookPath);  // cache, bookmarks, clippings
      if (Storage.exists(folder.c_str())) Storage.removeDir(folder.c_str());
      RECENT_BOOKS.removeByPath(bookPath);
      if (Storage.exists(folder.c_str())) {
        failedCount++;
      } else {
        clearedCount++;
      }
      return;
    }
  }
}

void ClearCacheActivity::processShard(const int index) {
  char shardName[4];
  snprintf(shardName, sizeof(shardName), "%02x", index);
  const std::string shardDir = std::string(FsHelpers::kBookCacheDir) + "/" + shardName;
  if (!Storage.exists(shardDir.c_str())) return;
  // List first (a shard holds a few hundred folders at most), then act:
  // deleting while walking a directory is not safe.
  for (const auto& item : listEntries(shardDir)) {
    if (!item.second || !isBookCacheDirectoryName(item.first.c_str())) continue;
    handleFolder(shardDir + "/" + item.first, item.first.c_str());
  }
}

void ClearCacheActivity::finishClearing() {
  if (!stopped) {
    if (mode == Mode::FullReset) {
      // Book caches are gone, so no folder can still count as prepared.
      if (Storage.exists(PreparedMarks::kFolderMarksDir)) Storage.removeDir(PreparedMarks::kFolderMarksDir);
    } else if (mode == Mode::PreparedOnly) {
      // App caches that rebuild themselves (the sleep frame is kept for wake-up).
      static const char* const kSystemDirs[] = {"fileindex", "sleep-image-index", "prepared-folders"};
      for (const char* dir : kSystemDirs) {
        const std::string path = std::string(FsHelpers::kSystemCacheDir) + "/" + dir;
        if (Storage.exists(path.c_str())) Storage.removeDir(path.c_str());
      }
      static const char* const kSystemFiles[] = {"card_stats.bin", "home_carousel_cache.bin", "image_view.pxc",
                                                 "font-catalog.bin"};
      for (const char* file : kSystemFiles) {
        const std::string path = std::string(FsHelpers::kSystemCacheDir) + "/" + file;
        if (Storage.exists(path.c_str())) Storage.remove(path.c_str());
      }
    }
  }
  LOG_DBG("CLEAR_CACHE", "Done: checked %d, removed %d, failed %d%s", checkedCount, clearedCount, failedCount,
          stopped ? " (stopped)" : "");
  state = SUCCESS;
  requestUpdate();
}

void ClearCacheActivity::startClearing() {
  if (!Storage.exists(FsHelpers::kBookCacheDir)) {
    LOG_DBG("CLEAR_CACHE", "No book cache directory");
    // Nothing cached: still tidy the app caches for Clear Prepared Data.
    shard = kShards;
    stopped = false;
    state = CLEARING;
    finishClearing();
    return;
  }
  shard = 0;
  stopped = false;
  clearedCount = failedCount = checkedCount = 0;
  state = CLEARING;
  lastDrawMs = millis();
  requestUpdate();
}

void ClearCacheActivity::loop() {
  if (state == CLEARING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      stopped = true;
      finishClearing();
      return;
    }
    if (shard < kShards) {
      processShard(shard++);
    }
    if (shard >= kShards) {
      finishClearing();
    } else if (millis() - lastDrawMs >= 1500) {
      lastDrawMs = millis();
      requestUpdate();
    }
    return;
  }
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    goBack();
    return;
  }
  if (state == WARNING) {
    int x = 0;
    int y = 0;
    if (mappedInput.hasTouch() && mappedInput.wasScreenTouchDown(x, y)) {
      const int action = TouchActionButtons::indexAt(touchActionLayout(renderer), x, y);
      if (action == 0) {
        mappedInput.suppressNextTouchTap();
        startClearing();
        return;
      }
      if (action == 1) {
        mappedInput.suppressNextTouchTap();
        goBack();
        return;
      }
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      startClearing();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      goBack();
    }
    return;
  }
  if (state == SUCCESS || state == FAILED) {
    int x = 0;
    int y = 0;
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasScreenTapped(x, y)) {
      goBack();
    }
  }
}
