#include "FileInfoActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <Arduino.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "BookActions.h"
#include "RecentBookProgress.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr unsigned long kScanSliceMs = 40;
constexpr unsigned long kScanRedrawMs = 1500;

bool hasExtension(const char* name, const char* const* exts) {
  const char* dot = strrchr(name, '.');
  if (!dot) return false;
  for (; *exts; ++exts) {
    if (strcasecmp(dot + 1, *exts) == 0) return true;
  }
  return false;
}

constexpr const char* const kBookExts[] = {"epub", "txt", "md", "xtc", "xtch", "pdf", "fb2", "mobi", "azw3", nullptr};
constexpr const char* const kImageExts[] = {"jpg", "jpeg", "png", "bmp", "gif", "webp", nullptr};

// 12345 -> "12,345"
std::string groupedCount(const uint32_t n) {
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

std::string shortByteSize(const uint64_t bytes) {
  char buf[24];
  if (bytes < 1024ULL * 1024ULL) {
    snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
  } else if (bytes < 1024ULL * 1024ULL * 1024ULL) {
    snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
  } else {
    snprintf(buf, sizeof(buf), "%.1f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
  }
  return buf;
}
}  // namespace

void FileInfoActivity::findReadingNow() {
  readingNow_.clear();
  // The newest few recents are in RAM; EPUBs use their cached percentage.
  for (const RecentBook& book : RECENT_BOOKS.getBooks()) {
    const float pct = FsHelpers::hasEpubExtension(book.path) ? RecentBookProgress::loadCachedEpubPercent(book)
                                                              : RecentBookProgress::loadPercent(book);
    if (!RecentBookProgress::hasPercent(pct) || pct <= 5.0f || pct >= 100.0f) continue;
    if (BookActions::isBookCompleted(book.path)) continue;
    char pctText[8];
    snprintf(pctText, sizeof(pctText), "%d%%", static_cast<int>(pct + 0.5f));
    readingNow_ = (book.title.empty() ? book.path : book.title) + '\t' + pctText;
    return;
  }
}

void FileInfoActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  if (usageRow_ >= 0) {
    findReadingNow();
    haveSaved_ = loadSavedStats();
    dirStack_[0] = Storage.open("/");
    if (dirStack_[0] && dirStack_[0].isDirectory()) {
      dirDepth_ = 1;
      scanning_ = true;
    } else if (dirStack_[0]) {
      dirStack_[0].close();
    }
  }
  requestUpdate();
}

void FileInfoActivity::onExit() {
  for (int i = dirDepth_ - 1; i >= 0; --i) dirStack_[i].close();
  dirDepth_ = 0;
  Activity::onExit();
}

void FileInfoActivity::loop() {
  input_.poll(mappedInput);
  // Back only: the Confirm release that chose Info in the Options sheet
  // arrives here and must not close the page straight away.
  if (input_.back || input_.backLong) {
    finish();
    return;
  }
  if (scanning_) {
    scanSlice();
    // While a saved count is on screen, wait for the new one instead of
    // redrawing partial numbers.
    if (!scanning_ || (!haveSaved_ && millis() - lastDrawMs_ >= kScanRedrawMs)) {
      lastDrawMs_ = millis();
      requestUpdate();
    }
  }
}

void FileInfoActivity::scanSlice() {
  char name[256];  // FAT long names; on the activity task's stack only for this slice
  const unsigned long start = millis();
  while (millis() - start < kScanSliceMs) {
    if (dirDepth_ == 0) {
      finishScan();
      return;
    }
    HalFile& dir = dirStack_[dirDepth_ - 1];
    HalFile entry = dir.openNextFile();
    if (!entry) {
      // This folder is done: back up to its parent.
      dir.close();
      --dirDepth_;
      continue;
    }
    entry.getName(name, sizeof(name));
    const bool isDir = entry.isDirectory();
    const uint64_t size = isDir ? 0 : static_cast<uint64_t>(entry.fileSize());
    if (name[0] == '.' || strcmp(name, "System Volume Information") == 0) {
      entry.close();
      continue;
    }
    if (isDir) {
      ++counting_.folders;
      if (dirDepth_ < kMaxScanDepth) {
        // Descend: the entry handle becomes the next level.
        dirStack_[dirDepth_++] = std::move(entry);
      } else {
        entry.close();
      }
      continue;
    }
    entry.close();
    ++counting_.files;
    if (hasExtension(name, kBookExts)) {
      ++counting_.books;
    } else if (hasExtension(name, kImageExts)) {
      ++counting_.images;
    }
    if (size > counting_.largestSize) {
      counting_.largestSize = size;
      counting_.largestName = name;
    }
    if (size > 0 && (counting_.smallestSize == 0 || size < counting_.smallestSize)) {
      counting_.smallestSize = size;
      counting_.smallestName = name;
    }
  }
}

void FileInfoActivity::finishScan() {
  scanning_ = false;
  scanDone_ = true;
  shown_ = counting_;
  saveStats();
}

// /.pocketdeck-os/system_cache/card_stats.bin: "CST2", files, folders, books, images (u32),
// then largest and smallest file: size (u64), name length (u8), name. A "CST1"
// file (no smallest) is ignored and simply counted again.
namespace {
constexpr char kStatsPath[] = "/.pocketdeck-os/system_cache/card_stats.bin";
constexpr char kStatsMagic[4] = {'C', 'S', 'T', '2'};

bool readSizedName(HalFile& file, uint64_t& size, std::string& name) {
  uint8_t nameLen = 0;
  char buf[256];
  if (file.read(reinterpret_cast<uint8_t*>(&size), sizeof(size)) != static_cast<int>(sizeof(size)) ||
      file.read(&nameLen, 1) != 1 || file.read(reinterpret_cast<uint8_t*>(buf), nameLen) != nameLen) {
    return false;
  }
  name.assign(buf, nameLen);
  return true;
}

void writeSizedName(HalFile& file, const uint64_t size, const std::string& name) {
  const auto nameLen = static_cast<uint8_t>(std::min<size_t>(name.size(), 255));
  file.write(reinterpret_cast<const uint8_t*>(&size), sizeof(size));
  file.write(&nameLen, 1);
  file.write(reinterpret_cast<const uint8_t*>(name.data()), nameLen);
}
}  // namespace

bool FileInfoActivity::loadSavedStats() {
  HalFile file;
  if (!Storage.exists(kStatsPath) || !Storage.openFileForRead("INFO", kStatsPath, file)) return false;
  char magic[4];
  uint32_t counts[4];
  CardStats loaded;
  const bool ok = file.read(reinterpret_cast<uint8_t*>(magic), 4) == 4 && memcmp(magic, kStatsMagic, 4) == 0 &&
                  file.read(reinterpret_cast<uint8_t*>(counts), sizeof(counts)) == static_cast<int>(sizeof(counts)) &&
                  readSizedName(file, loaded.largestSize, loaded.largestName) &&
                  readSizedName(file, loaded.smallestSize, loaded.smallestName);
  file.close();
  if (!ok) return false;
  loaded.files = counts[0];
  loaded.folders = counts[1];
  loaded.books = counts[2];
  loaded.images = counts[3];
  shown_ = std::move(loaded);
  return true;
}

void FileInfoActivity::saveStats() const {
  HalFile file;
  if (!Storage.openFileForWrite("INFO", kStatsPath, file)) {
    LOG_ERR("INFO", "could not save card stats");
    return;
  }
  const uint32_t counts[4] = {shown_.files, shown_.folders, shown_.books, shown_.images};
  file.write(reinterpret_cast<const uint8_t*>(kStatsMagic), 4);
  file.write(reinterpret_cast<const uint8_t*>(counts), sizeof(counts));
  writeSizedName(file, shown_.largestSize, shown_.largestName);
  writeSizedName(file, shown_.smallestSize, shown_.smallestName);
  file.close();
}

void FileInfoActivity::drawCardStats(int y, const int bottom, const Rect& content) const {
  const int labelH = renderer.getLineHeight(SMALL_FONT_ID);
  const int valueH = renderer.getLineHeight(UI_12_FONT_ID);
  const int cellH = labelH + 2 + valueH + 12;
  const int colW = content.width / 4;
  // Until the new count finishes: the saved count if there is one, else the
  // running totals.
  const CardStats& stats = (scanDone_ || haveSaved_) ? shown_ : counting_;
  const std::string counts[4] = {groupedCount(stats.files), groupedCount(stats.folders), groupedCount(stats.books),
                                 groupedCount(stats.images)};
  const char* labels[4] = {tr(STR_INFO_FILES), tr(STR_INFO_FOLDERS), tr(STR_INFO_BOOKS), tr(STR_INFO_IMAGES)};
  // One row of four counts (files, folders, books, images), then the files.
  if (y + cellH > bottom) return;
  for (int i = 0; i < 4; ++i) {
    const int x = content.x + i * colW;
    const std::string label = renderer.truncatedText(SMALL_FONT_ID, labels[i], colW - 6);
    const std::string count = renderer.truncatedText(UI_12_FONT_ID, counts[i].c_str(), colW - 6);
    renderer.drawText(SMALL_FONT_ID, x, y, label.c_str());
    renderer.drawText(UI_12_FONT_ID, x, y + labelH + 2, count.c_str());
  }
  y += cellH;
  if (y + labelH > bottom) return;
  if (!scanDone_ && !haveSaved_) {
    renderer.drawText(SMALL_FONT_ID, content.x, y, tr(STR_INFO_COUNTING));
    return;
  }
  // "Label" over "name ........ value" (value right-aligned).
  const auto namedRow = [&](const char* label, const std::string& name, const std::string& value) {
    if (name.empty() || y + cellH > bottom) return;
    renderer.drawText(SMALL_FONT_ID, content.x, y, label);
    const int valueW = renderer.getTextWidth(UI_12_FONT_ID, value.c_str());
    const std::string nameText =
        renderer.truncatedText(UI_12_FONT_ID, name.c_str(), std::max(0, content.width - valueW - 16));
    renderer.drawText(UI_12_FONT_ID, content.x, y + labelH + 2, nameText.c_str());
    renderer.drawText(UI_12_FONT_ID, content.x + content.width - valueW, y + labelH + 2, value.c_str());
    y += cellH;
  };
  if (stats.largestSize > 0) namedRow(tr(STR_INFO_LARGEST), stats.largestName, shortByteSize(stats.largestSize));
  if (stats.smallestSize > 0) namedRow(tr(STR_INFO_SMALLEST), stats.smallestName, shortByteSize(stats.smallestSize));
  if (!readingNow_.empty()) {
    // readingNow_ is "title\tNN%".
    const size_t tab = readingNow_.find('\t');
    namedRow(tr(STR_INFO_READING_NOW), readingNow_.substr(0, tab),
             tab == std::string::npos ? std::string() : readingNow_.substr(tab + 1));
  }
  if (!scanDone_ && y + labelH <= bottom) {
    renderer.drawText(SMALL_FONT_ID, content.x, y, tr(STR_INFO_UPDATING));
  }
}

void FileInfoActivity::render(RenderLock&&) {
  const Rect content = tools::drawFrame(renderer, tr(STR_FILE_INFO));
  const int labelH = renderer.getLineHeight(SMALL_FONT_ID);
  const int valueH = renderer.getLineHeight(UI_12_FONT_ID);
  int y = content.y + 4;

  // The item's own name, wrapped over up to two lines.
  tools::WrapOptions opt;
  opt.fontId = UI_12_FONT_ID;
  opt.maxLines = 2;
  const int titleLines =
      tools::drawWrappedText(renderer, Rect{content.x, y, content.width, valueH * 2}, title_.c_str(), opt);
  y += valueH * std::max(1, titleLines) + 14;
  renderer.drawLine(content.x, y - 7, content.x + content.width, y - 7, true);

  constexpr int kBarH = 24;
  for (size_t i = 0; i < rows_.size(); ++i) {
    const auto& row = rows_[i];
    const bool usage = static_cast<int>(i) == usageRow_;
    if (y + labelH + valueH + (usage ? kBarH + 8 : 0) > content.y + content.height) break;
    int labelRoom = content.width;
    if (usage) {
      // "NN% used" right-aligned on the label line.
      char percent[24];
      snprintf(percent, sizeof(percent), tr(STR_INFO_USED_PERCENT), (usedPermille_ + 5) / 10);
      const int percentW = renderer.getTextWidth(SMALL_FONT_ID, percent);
      renderer.drawText(SMALL_FONT_ID, content.x + content.width - percentW, y, percent);
      labelRoom -= percentW + 12;
    }
    const std::string label = renderer.truncatedText(SMALL_FONT_ID, row.first.c_str(), std::max(0, labelRoom));
    renderer.drawText(SMALL_FONT_ID, content.x, y, label.c_str());
    y += labelH + 2;
    const std::string value = renderer.truncatedText(UI_12_FONT_ID, row.second.c_str(), content.width);
    renderer.drawText(UI_12_FONT_ID, content.x, y, value.c_str());
    y += valueH + 12;
    if (usage) {
      // Capacity bar: used space filled solid, free space left white.
      y -= 2;
      renderer.drawRoundedRect(content.x, y, content.width, kBarH, 2, kBarH / 2, true);
      const int inner = content.width - 8;
      const int used = std::clamp(usedPermille_, 0, 1000);
      int fillW = static_cast<int>(static_cast<int64_t>(inner) * used / 1000);
      if (used > 0 && fillW < kBarH - 8) fillW = kBarH - 8;  // a sliver stays visible
      if (fillW > 0) renderer.fillRoundedRect(content.x + 4, y + 4, fillW, kBarH - 8, (kBarH - 8) / 2, Color::Black);
      y += kBarH + 16;
      // Card statistics close the page; nothing follows the SD card row.
      drawCardStats(y, content.y + content.height, content);
      break;
    }
  }

  tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
  // Count updates while scanning use a fast refresh to avoid flashing.
  if (drawnOnce_) {
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  } else {
    renderer.displayBuffer();
  }
  drawnOnce_ = true;
}
