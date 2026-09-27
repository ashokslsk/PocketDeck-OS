#include "DailyQuoteActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "components/UITheme.h"
#include "fontIds.h"

constexpr char DailyQuoteActivity::kQuotesPath[];
constexpr char DailyQuoteActivity::kIndexPath[];

namespace {
constexpr uint32_t kIndexMagic = 0x31584951;  // "QIX1"
constexpr int32_t kUndatedKey = -99999;
constexpr unsigned long kPollMs = 30UL * 1000UL;

struct IndexHeader {
  uint32_t magic;
  uint32_t sourceSize;
  uint32_t count;
};

struct IndexEntry {
  int32_t key;  // days since epoch, -(MMDD) for yearly lines, kUndatedKey otherwise
  uint32_t offset;
};

struct BuildCtx {
  FsFile* source;
  char* scratch;
  size_t scratchCap;
  uint32_t sourceSize;
  uint32_t count;
};

int32_t keyForPrefix(const char* line, const char** textOut) {
  const char* pipe = strchr(line, '|');
  if (pipe == nullptr) {
    *textOut = line;
    return kUndatedKey;
  }
  *textOut = pipe + 1;
  const size_t prefixLen = static_cast<size_t>(pipe - line);
  char prefix[12] = {};
  if (prefixLen >= sizeof(prefix)) return kUndatedKey;
  memcpy(prefix, line, prefixLen);
  int32_t days = 0;
  if (prefixLen == 10 && tools::parseIsoDate(prefix, days)) return days;
  unsigned m = 0, d = 0;
  if (prefixLen == 5 && sscanf(prefix, "%2u-%2u", &m, &d) == 2 && m >= 1 && m <= 12 && d >= 1 && d <= 31) {
    return -static_cast<int32_t>(m * 100 + d);
  }
  return kUndatedKey;
}

uint32_t mix(uint32_t x) {
  // Small integer hash so consecutive days pick unrelated quotes.
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}
}  // namespace

bool DailyQuoteActivity::buildIndex(FsFile& out, void* ctx) {
  auto* b = static_cast<BuildCtx*>(ctx);
  IndexHeader header{kIndexMagic, b->sourceSize, 0};
  if (out.write(&header, sizeof(header)) != sizeof(header)) return false;
  while (true) {
    const auto offset = static_cast<uint32_t>(b->source->position());
    const int len = tools::readLine(*b->source, b->scratch, b->scratchCap);
    if (len < 0) break;
    if (len == 0 || b->scratch[0] == '#') continue;
    const char* text = nullptr;
    const IndexEntry entry{keyForPrefix(b->scratch, &text), offset};
    if (text == nullptr || *text == '\0') continue;
    if (out.write(&entry, sizeof(entry)) != sizeof(entry)) return false;
    ++b->count;
  }
  header.count = b->count;
  return out.seek(0) && out.write(&header, sizeof(header)) == sizeof(header);
}

bool DailyQuoteActivity::ensureIndex(uint32_t& count) {
  count = 0;
  FsFile src;
  if (!Storage.exists(kQuotesPath) || !Storage.openFileForRead("QUOTE", kQuotesPath, src)) {
    status_ = Status::MissingFile;
    return false;
  }
  const auto sourceSize = static_cast<uint32_t>(src.fileSize());

  FsFile idx;
  if (Storage.exists(kIndexPath) && Storage.openFileForRead("QUOTE", kIndexPath, idx)) {
    IndexHeader header{};
    const bool valid = idx.read(&header, sizeof(header)) == sizeof(header) && header.magic == kIndexMagic &&
                       header.sourceSize == sourceSize;
    idx.close();
    if (valid) {
      src.close();
      count = header.count;
      status_ = count > 0 ? Status::Ok : Status::Empty;
      return count > 0;
    }
  }

  LOG_INF("QUOTE", "Building quote index (%u bytes)", sourceSize);
  BuildCtx ctx{&src, line_, sizeof(line_), sourceSize, 0};
  const bool built = tools::writeFileAtomic(kIndexPath, &DailyQuoteActivity::buildIndex, &ctx);
  src.close();
  if (!built) {
    LOG_ERR("QUOTE", "Failed to build %s", kIndexPath);
    status_ = Status::Empty;
    return false;
  }
  count = ctx.count;
  status_ = count > 0 ? Status::Ok : Status::Empty;
  return count > 0;
}

bool DailyQuoteActivity::readQuoteAt(const uint32_t offset) {
  page_ = 0;
  FsFile src;
  if (!Storage.openFileForRead("QUOTE", kQuotesPath, src)) return false;
  const bool ok = src.seek(offset) && tools::readLine(src, line_, sizeof(line_)) > 0;
  src.close();
  if (!ok) return false;

  const char* text = nullptr;
  keyForPrefix(line_, &text);
  char* body = const_cast<char*>(text);
  // Split "quote — author" at the last em dash (or " -- ").
  author_ = "";
  char* split = nullptr;
  for (char* p = strstr(body, " \xE2\x80\x94 "); p != nullptr; p = strstr(p + 1, " \xE2\x80\x94 ")) split = p;
  if (split != nullptr) {
    *split = '\0';
    author_ = split + 5;
  } else if ((split = strstr(body, " -- ")) != nullptr) {
    *split = '\0';
    author_ = split + 4;
  }
  text_ = body;
  return true;
}

void DailyQuoteActivity::showDay(const int32_t day) {
  shownDay_ = day;
  randomPick_ = false;
  uint32_t count = 0;
  if (!ensureIndex(count)) return;

  FsFile idx;
  if (!Storage.openFileForRead("QUOTE", kIndexPath, idx)) return;
  uint16_t y = 0;
  uint8_t m = 0, d = 0;
  tools::civilFromDays(day, y, m, d);
  const int32_t yearlyKey = -static_cast<int32_t>(m * 100 + d);
  int64_t exact = -1;
  int64_t yearly = -1;

  idx.seek(sizeof(IndexHeader));
  IndexEntry chunk[16];  // 128 bytes on the stack; the index is streamed
  uint32_t remaining = count;
  while (remaining > 0 && exact < 0) {
    const uint32_t n = std::min<uint32_t>(remaining, 16);
    if (idx.read(chunk, n * sizeof(IndexEntry)) != static_cast<int>(n * sizeof(IndexEntry))) break;
    for (uint32_t i = 0; i < n; ++i) {
      if (chunk[i].key == day) {
        exact = chunk[i].offset;
        break;
      }
      if (yearly < 0 && chunk[i].key == yearlyKey) yearly = chunk[i].offset;
    }
    remaining -= n;
  }
  idx.close();

  const int64_t offset = exact >= 0 ? exact : yearly;
  if (offset >= 0 && readQuoteAt(static_cast<uint32_t>(offset))) return;
  // Nothing scheduled: a random pick that stays the same all day.
  showRandom(mix(static_cast<uint32_t>(day)));
  randomPick_ = true;
}

void DailyQuoteActivity::showRandom(const uint32_t seed) {
  uint32_t count = 0;
  if (!ensureIndex(count)) return;
  FsFile idx;
  if (!Storage.openFileForRead("QUOTE", kIndexPath, idx)) return;
  IndexEntry entry{};
  const uint32_t pick = seed % count;
  const bool ok =
      idx.seek(sizeof(IndexHeader) + pick * sizeof(IndexEntry)) && idx.read(&entry, sizeof(entry)) == sizeof(entry);
  idx.close();
  if (ok) readQuoteAt(entry.offset);
  randomPick_ = true;
}

void DailyQuoteActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  tools::ensureToolsDirs();
  tools::DateTime local;
  clockValid_ = tools::getLocalNow(local);
  if (clockValid_) {
    today_ = tools::daysOf(local);
    showDay(today_);
  } else {
    showRandom(mix(static_cast<uint32_t>(millis())));
  }
  transitionPending_ = true;
  requestUpdate();
}

void DailyQuoteActivity::loop() {
  input_.poll(mappedInput);
  if (input_.backLong) {
    tools::exitToHome();
    return;
  }
  if (input_.back) {
    finish();
    return;
  }
  // line_ (and text_/author_ pointing into it) is read by render(); hold the
  // render lock while replacing it.
  if (input_.confirm) {
    RenderLock lock(*this);
    showRandom(mix(static_cast<uint32_t>(micros()) ^ static_cast<uint32_t>(millis())));
    requestUpdate();
  } else if (input_.confirmLong && clockValid_) {
    RenderLock lock(*this);
    showDay(today_);
    requestUpdate();
  } else if ((input_.up || input_.pageBack) && page_ > 0) {
    --page_;
    requestUpdate();
  } else if ((input_.down || input_.pageForward) && page_ + 1 < pageCount_) {
    ++page_;
    requestUpdate();
  } else if (clockValid_ && input_.left) {
    RenderLock lock(*this);
    showDay(shownDay_ - 1);
    requestUpdate();
  } else if (clockValid_ && input_.right) {
    RenderLock lock(*this);
    showDay(shownDay_ + 1);
    requestUpdate();
  }

  // After midnight, move on to the new day's quote automatically.
  const unsigned long now = millis();
  if (clockValid_ && now - lastPollMs_ >= kPollMs) {
    lastPollMs_ = now;
    tools::DateTime local;
    if (tools::getLocalNow(local) && tools::daysOf(local) != today_) {
      const bool wasOnToday = shownDay_ == today_;
      today_ = tools::daysOf(local);
      if (wasOnToday) {
        RenderLock lock(*this);
        showDay(today_);
        transitionPending_ = true;
        requestUpdate();
      }
    }
  }
}

void DailyQuoteActivity::render(RenderLock&&) {
  char subtitle[32] = "";
  if (clockValid_) tools::formatIsoDate(subtitle, sizeof(subtitle), shownDay_);
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_DAILY_QUOTE), clockValid_ ? subtitle : nullptr);

  if (status_ != Status::Ok) {
    const int midY = content.y + content.height / 2;
    renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_TOOLS_NO_QUOTES), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, tr(STR_TOOLS_NO_QUOTES_HINT));
  } else {
    // Largest serif size whose wrapped quote still leaves room for the author.
    const int authorH = renderer.getLineHeight(BITTER_12_FONT_ID) + 24;
    Rect box{content.x + 10, content.y, content.width - 20, content.height - authorH};
    static constexpr int kFonts[] = {BITTER_16_FONT_ID, BITTER_14_FONT_ID, BITTER_12_FONT_ID, UI_10_FONT_ID};
    int font = kFonts[0];
    int lines = 0;
    for (const int candidate : kFonts) {
      font = candidate;
      tools::WrapOptions measure;
      measure.fontId = font;
      measure.centered = true;
      measure.draw = false;
      lines = tools::drawWrappedText(renderer, box, text_, measure);
      if (lines * renderer.getLineHeight(font) <= box.height) break;
    }
    // A quote too long even at the smallest size is paged with Up/Down; the
    // author appears on the last page.
    const int lineH = renderer.getLineHeight(font);
    const int linesPerPage = std::max(1, box.height / lineH);
    pageCount_ = std::max(1, (lines + linesPerPage - 1) / linesPerPage);
    page_ = std::min(page_, pageCount_ - 1);
    const bool lastPage = page_ == pageCount_ - 1;
    const int shownLines = pageCount_ == 1 ? lines : (lastPage ? lines - page_ * linesPerPage : linesPerPage);
    const int textH = std::min(box.height, shownLines * lineH);
    const int blockH = textH + (lastPage ? authorH : 0);
    box.y = content.y + std::max(0, (content.height - blockH) / 2);
    box.height = textH;
    tools::WrapOptions opt;
    opt.fontId = font;
    opt.centered = true;
    opt.skipLines = page_ * linesPerPage;
    opt.maxLines = linesPerPage;
    tools::drawWrappedText(renderer, box, text_, opt);
    if (pageCount_ > 1) {
      char pageLabel[24];
      snprintf(pageLabel, sizeof(pageLabel), "%d / %d", page_ + 1, pageCount_);
      renderer.drawCenteredText(SMALL_FONT_ID, content.y + content.height - renderer.getLineHeight(SMALL_FONT_ID),
                                pageLabel);
    }

    int y = box.y + textH + 12;
    const int cx = renderer.getScreenWidth() / 2;
    if (lastPage) renderer.fillRect(cx - 30, y, 60, 2);
    y += 12;
    if (lastPage && author_[0] != '\0') {
      const std::string author =
          renderer.truncatedText(BITTER_12_FONT_ID, author_, content.width, EpdFontFamily::ITALIC);
      renderer.drawCenteredText(BITTER_12_FONT_ID, y, author.c_str(), true, EpdFontFamily::ITALIC);
    }
  }

  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_SHUFFLE),
                   clockValid_ ? tr(STR_TOOLS_PREV_DAY) : "", clockValid_ ? tr(STR_TOOLS_NEXT_DAY) : "");
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
