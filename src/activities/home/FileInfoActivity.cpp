#include "FileInfoActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <Arduino.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

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

void FileInfoActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  if (usageRow_ >= 0) {
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
  }
}

void FileInfoActivity::finishScan() {
  scanning_ = false;
  scanDone_ = true;
  shown_ = counting_;
  saveStats();
}

// /.pocketdeck-os/card_stats.bin: "CST1", files, folders, books, images (u32),
// largest size (u64), name length (u8), name.
namespace {
constexpr char kStatsPath[] = "/.pocketdeck-os/card_stats.bin";
constexpr char kStatsMagic[4] = {'C', 'S', 'T', '1'};
}  // namespace

bool FileInfoActivity::loadSavedStats() {
  HalFile file;
  if (!Storage.exists(kStatsPath) || !Storage.openFileForRead("INFO", kStatsPath, file)) return false;
  char magic[4];
  uint32_t counts[4];
  uint64_t largest = 0;
  uint8_t nameLen = 0;
  char name[256];
  bool ok = file.read(reinterpret_cast<uint8_t*>(magic), 4) == 4 && memcmp(magic, kStatsMagic, 4) == 0 &&
            file.read(reinterpret_cast<uint8_t*>(counts), sizeof(counts)) == static_cast<int>(sizeof(counts)) &&
            file.read(reinterpret_cast<uint8_t*>(&largest), sizeof(largest)) == static_cast<int>(sizeof(largest)) &&
            file.read(&nameLen, 1) == 1 && file.read(reinterpret_cast<uint8_t*>(name), nameLen) == nameLen;
  file.close();
  if (!ok) return false;
  shown_.files = counts[0];
  shown_.folders = counts[1];
  shown_.books = counts[2];
  shown_.images = counts[3];
  shown_.largestSize = largest;
  shown_.largestName.assign(name, nameLen);
  return true;
}

void FileInfoActivity::saveStats() const {
  HalFile file;
  if (!Storage.openFileForWrite("INFO", kStatsPath, file)) {
    LOG_ERR("INFO", "could not save card stats");
    return;
  }
  const uint32_t counts[4] = {shown_.files, shown_.folders, shown_.books, shown_.images};
  const auto nameLen = static_cast<uint8_t>(std::min<size_t>(shown_.largestName.size(), 255));
  file.write(reinterpret_cast<const uint8_t*>(kStatsMagic), 4);
  file.write(reinterpret_cast<const uint8_t*>(counts), sizeof(counts));
  file.write(reinterpret_cast<const uint8_t*>(&shown_.largestSize), sizeof(shown_.largestSize));
  file.write(&nameLen, 1);
  file.write(reinterpret_cast<const uint8_t*>(shown_.largestName.data()), nameLen);
  file.close();
}

void FileInfoActivity::drawCardStats(int y, const int bottom, const Rect& content) const {
  const int labelH = renderer.getLineHeight(SMALL_FONT_ID);
  const int valueH = renderer.getLineHeight(UI_12_FONT_ID);
  const int cellH = labelH + 2 + valueH + 12;
  const int colW = content.width / 2;
  // Until the new count finishes: the saved count if there is one, else the
  // running totals.
  const CardStats& stats = (scanDone_ || haveSaved_) ? shown_ : counting_;
  const std::string counts[4] = {groupedCount(stats.files), groupedCount(stats.folders), groupedCount(stats.books),
                                 groupedCount(stats.images)};
  const char* labels[4] = {tr(STR_INFO_FILES), tr(STR_INFO_FOLDERS), tr(STR_INFO_BOOKS), tr(STR_INFO_IMAGES)};
  // A 2x2 grid of counts, then the largest file.
  for (int row = 0; row < 2; ++row) {
    if (y + cellH > bottom) return;
    for (int col = 0; col < 2; ++col) {
      const int i = row * 2 + col;
      const int x = content.x + col * colW;
      renderer.drawText(SMALL_FONT_ID, x, y, labels[i]);
      renderer.drawText(UI_12_FONT_ID, x, y + labelH + 2, counts[i].c_str());
    }
    y += cellH;
  }
  if (y + labelH > bottom) return;
  if (!scanDone_ && !haveSaved_) {
    renderer.drawText(SMALL_FONT_ID, content.x, y, tr(STR_INFO_COUNTING));
    return;
  }
  if (stats.largestSize > 0 && y + cellH <= bottom) {
    renderer.drawText(SMALL_FONT_ID, content.x, y, tr(STR_INFO_LARGEST));
    const std::string sizeText = shortByteSize(stats.largestSize);
    const int sizeW = renderer.getTextWidth(UI_12_FONT_ID, sizeText.c_str());
    const std::string nameText =
        renderer.truncatedText(UI_12_FONT_ID, stats.largestName.c_str(), std::max(0, content.width - sizeW - 16));
    renderer.drawText(UI_12_FONT_ID, content.x, y + labelH + 2, nameText.c_str());
    renderer.drawText(UI_12_FONT_ID, content.x + content.width - sizeW, y + labelH + 2, sizeText.c_str());
    y += cellH;
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
    renderer.drawText(SMALL_FONT_ID, content.x, y, row.first.c_str());
    if (usage) {
      // "NN% used" right-aligned on the label line.
      char percent[24];
      snprintf(percent, sizeof(percent), tr(STR_INFO_USED_PERCENT), (usedPermille_ + 5) / 10);
      renderer.drawText(SMALL_FONT_ID, content.x + content.width - renderer.getTextWidth(SMALL_FONT_ID, percent), y,
                        percent);
    }
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
