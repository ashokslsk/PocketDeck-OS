#include "CopyActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "activities/ActivityResult.h"
#include "activities/tools/ToolsCommon.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr unsigned long kSliceMs = 40;     // copy work per loop() pass, keeps Back responsive
constexpr unsigned long kRedrawMs = 1500;  // e-ink progress refresh cadence

std::string formatBytes(const uint64_t bytes) {
  char buf[24];
  if (bytes < 1024ULL * 1024ULL) {
    snprintf(buf, sizeof(buf), "%.0f KB", static_cast<double>(bytes) / 1024.0);
  } else {
    snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
  }
  return buf;
}

std::string joinPath(const std::string& base, const std::string& rel) { return rel.empty() ? base : base + rel; }
}  // namespace

void CopyActivity::onEnter() {
  Activity::onEnter();
  // 8 KB transfer buffer, held only while this screen copies.
  buffer_ = makeUniqueNoThrow<uint8_t[]>(kBufferSize);
  if (!buffer_) {
    LOG_ERR("COPY", "OOM: copy buffer (%u bytes)", static_cast<unsigned>(kBufferSize));
    finishWith(false, false);
    return;
  }
  if (isDirectory_) {
    items_.push_back({"", 0, true});
    if (!scan(src_, "")) {
      LOG_ERR("COPY", "Folder scan failed or too large: %s", src_.c_str());
      tooMany_ = items_.size() > kMaxItems;
      items_.clear();
      finishWith(false, false);
      return;
    }
  } else {
    HalFile file = Storage.open(src_.c_str());
    const uint32_t size = file ? static_cast<uint32_t>(file.fileSize()) : 0;
    if (file) file.close();
    items_.push_back({"", size, false});
    totalBytes_ = size;
  }
  // Refuse up front when the copy cannot fit (each file may round up to a
  // whole cluster, so leave 32 KB of slack per file).
  uint64_t cardTotal = 0;
#ifndef SIMULATOR  // the desktop storage backend has no volume statistics
  cardTotal = Storage.totalBytes();
#endif
  if (cardTotal > 0) {
    uint64_t cardUsed = 0;
#ifndef SIMULATOR
    cardUsed = Storage.usedBytes();
#endif
    const uint64_t cardFree = cardUsed < cardTotal ? cardTotal - cardUsed : 0;
    const uint64_t needed = totalBytes_ + static_cast<uint64_t>(items_.size()) * 32ULL * 1024ULL;
    if (needed > cardFree) {
      LOG_ERR("COPY", "Not enough space: need %llu, free %llu", static_cast<unsigned long long>(needed),
              static_cast<unsigned long long>(cardFree));
      noSpace_ = true;
      finishWith(false, false);
      return;
    }
  }
  lastDrawMs_ = millis();
  requestUpdate();
}

void CopyActivity::onExit() {
  closeFiles();
  buffer_.reset();
  Activity::onExit();
}

bool CopyActivity::scan(const std::string& dir, const std::string& rel) {
  HalFile root = Storage.open(dir.c_str());
  if (!root || !root.isDirectory()) return false;
  char name[256];
  bool ok = true;
  for (HalFile file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(name, sizeof(name));
    const std::string childRel = rel + "/" + name;
    const bool isDir = file.isDirectory();
    const uint32_t size = isDir ? 0 : static_cast<uint32_t>(file.fileSize());
    file.close();
    if (items_.size() >= kMaxItems) {
      items_.push_back({childRel, 0, isDir});  // marks the overflow for the caller
      ok = false;
      break;
    }
    items_.push_back({childRel, size, isDir});
    totalBytes_ += size;
    if (isDir && !scan(dir + "/" + name, childRel)) {
      ok = false;
      break;
    }
  }
  root.close();
  return ok;
}

void CopyActivity::closeFiles() {
  if (fileOpen_) {
    in_.close();
    out_.close();
    fileOpen_ = false;
  }
}

void CopyActivity::removeDestination() {
  closeFiles();
  if (isDirectory_) {
    if (Storage.exists(dst_.c_str()) && !Storage.removeDir(dst_.c_str())) {
      LOG_ERR("COPY", "Could not remove partial folder copy: %s", dst_.c_str());
    }
  } else if (Storage.exists(dst_.c_str()) && !Storage.remove(dst_.c_str())) {
    LOG_ERR("COPY", "Could not remove partial copy: %s", dst_.c_str());
  }
}

void CopyActivity::finishWith(const bool success, const bool cancelled) {
  done_ = true;
  closeFiles();
  // Nothing was written when the space check refused, so the destination
  // (which may be an unrelated existing path) is left alone.
  if (!success && !noSpace_ && !tooMany_) removeDestination();
  if (cancelled) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
  } else {
    // A failed copy reports "" (or the overflow marker) so the caller can explain.
    setResult(
        FilePathResult{success ? dst_ : (tooMany_ ? std::string("*") : (noSpace_ ? std::string("!") : std::string()))});
  }
  finish();
}

bool CopyActivity::copySlice(const unsigned long budgetMs) {
  const unsigned long start = millis();
  while (index_ < items_.size() && millis() - start < budgetMs) {
    const Item& item = items_[index_];
    const std::string target = joinPath(dst_, item.rel);
    if (item.isDirectory) {
      if (!Storage.exists(target.c_str()) && !Storage.mkdir(target.c_str())) {
        LOG_ERR("COPY", "mkdir failed: %s", target.c_str());
        return false;
      }
      ++index_;
      continue;
    }
    if (!fileOpen_) {
      const std::string source = joinPath(src_, item.rel);
      if (!Storage.openFileForRead("COPY", source, in_)) return false;
      if (!Storage.openFileForWrite("COPY", target, out_)) {
        in_.close();
        return false;
      }
      fileOpen_ = true;
    }
    const int got = in_.read(buffer_.get(), kBufferSize);
    if (got < 0) {
      LOG_ERR("COPY", "Read failed: %s", item.rel.c_str());
      return false;
    }
    if (got == 0) {
      if (!out_.sync()) {
        LOG_ERR("COPY", "Sync failed (card full?): %s", target.c_str());
        return false;
      }
      closeFiles();
      ++index_;
      continue;
    }
    if (out_.write(buffer_.get(), static_cast<size_t>(got)) != static_cast<size_t>(got)) {
      LOG_ERR("COPY", "Write failed (card full?): %s", target.c_str());
      return false;
    }
    doneBytes_ += static_cast<uint64_t>(got);
  }
  return true;
}

void CopyActivity::loop() {
  if (done_) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    RenderLock lock(*this);
    finishWith(false, true);
    return;
  }
  bool ok;
  {
    // The render task reads the counters and the current item name.
    RenderLock lock(*this);
    ok = copySlice(kSliceMs);
    if (!ok) {
      finishWith(false, false);
      return;
    }
    if (index_ >= items_.size()) {
      finishWith(true, false);
      return;
    }
  }
  if (millis() - lastDrawMs_ >= kRedrawMs) {
    lastDrawMs_ = millis();
    requestUpdate();
  }
}

void CopyActivity::render(RenderLock&&) {
  if (done_) return;
  const Rect content = tools::drawFrame(renderer, tr(STR_COPYING));
  const std::string root = src_.substr(src_.find_last_of('/') + 1);
  int y = content.y + 12;
  const std::string title = renderer.truncatedText(UI_12_FONT_ID, root.c_str(), content.width, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, content.x, y, title.c_str(), true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 18;

  const unsigned files = static_cast<unsigned>(
      std::count_if(items_.begin(), items_.end(), [](const Item& item) { return !item.isDirectory; }));
  unsigned fileNo = 0;
  for (size_t i = 0; i <= std::min(index_, items_.empty() ? 0 : items_.size() - 1); ++i) {
    if (!items_[i].isDirectory) ++fileNo;
  }
  char line[96];
  if (files > 1) {
    snprintf(line, sizeof(line), tr(STR_COPY_ITEM_FORMAT), std::max(1U, std::min(fileNo, files)), files);
    renderer.drawText(UI_10_FONT_ID, content.x, y, line);
    y += renderer.getLineHeight(UI_10_FONT_ID) + 12;
  }

  // Progress bar.
  const int percent = totalBytes_ > 0 ? static_cast<int>(std::min<uint64_t>(100, doneBytes_ * 100 / totalBytes_)) : 0;
  constexpr int barH = 22;
  renderer.drawRoundedRect(content.x, y, content.width, barH, 2, 8, true);
  const int fillW = (content.width - 8) * percent / 100;
  if (fillW > 0) renderer.fillRoundedRect(content.x + 4, y + 4, fillW, barH - 8, 4, Color::Black);
  y += barH + 14;

  snprintf(line, sizeof(line), "%d%%", percent);
  renderer.drawText(UI_12_FONT_ID, content.x, y, line, true, EpdFontFamily::BOLD);
  const std::string bytes = formatBytes(doneBytes_);
  const std::string total = formatBytes(totalBytes_);
  char sizes[64];
  snprintf(sizes, sizeof(sizes), tr(STR_COPY_BYTES_FORMAT), bytes.c_str(), total.c_str());
  const int sizesW = renderer.getTextWidth(UI_10_FONT_ID, sizes);
  renderer.drawText(UI_10_FONT_ID, content.x + content.width - sizesW, y + 4, sizes);

  tools::drawHints(renderer, mappedInput, tr(STR_CANCEL), "", "", "");
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
