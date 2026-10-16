#include "DailyQuoteActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "KannadaText.h"
#include "ToolStatsData.h"
#include "ToolStatsPages.h"
#include "ToolsLog.h"
#include "activities/util/OptionSelectionActivity.h"
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
  parseLine();
  refreshFavourite();
  return true;
}

void DailyQuoteActivity::parseLine() {
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
}

namespace {
constexpr char kFavDir[] = "/tools/quotes";

// "text — author" as saved in favourites (and quotes.txt).
std::string quoteBody(const char* text, const char* author) {
  std::string body = text;
  if (author != nullptr && author[0] != '\0') {
    body += " \xE2\x80\x94 ";
    body += author;
  }
  return body;
}

struct FavCtx {
  const std::string* body;  // "text — author" of the quote being added / removed
  const char* date;         // added line's date prefix
};
}  // namespace

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool DailyQuoteActivity::writeFavouriteAdded(FsFile& out, void* ctx) {
  const auto* f = static_cast<const FavCtx*>(ctx);
  FsFile in;
  if (Storage.exists(tstats::kFavouritesPath) && Storage.openFileForRead("QUOTE", tstats::kFavouritesPath, in)) {
    uint8_t buf[128];
    int n = 0;
    while ((n = in.read(buf, sizeof(buf))) > 0) {
      if (out.write(buf, static_cast<size_t>(n)) != static_cast<size_t>(n)) {
        in.close();
        return false;
      }
    }
    in.close();
  }
  return tools::writeText(out, f->date) && tools::writeText(out, "|") && tools::writeText(out, f->body->c_str()) &&
         tools::writeText(out, "\n");
}

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool DailyQuoteActivity::writeFavouriteRemoved(FsFile& out, void* ctx) {
  const auto* f = static_cast<const FavCtx*>(ctx);
  FsFile in;
  if (!Storage.openFileForRead("QUOTE", tstats::kFavouritesPath, in)) return false;
  auto line = makeUniqueNoThrow<char[]>(kLineCap);
  if (!line) {
    in.close();
    return false;
  }
  bool ok = true;
  while (ok && tools::readLine(in, line.get(), kLineCap) >= 0) {
    const char* bar = strchr(line.get(), '|');
    if (strcmp(bar != nullptr ? bar + 1 : line.get(), f->body->c_str()) == 0) continue;
    ok = tools::writeText(out, line.get()) && tools::writeText(out, "\n");
  }
  in.close();
  return ok;
}

void DailyQuoteActivity::refreshFavourite() {
  favourite_ = false;
  FsFile in;
  if (!Storage.exists(tstats::kFavouritesPath) || !Storage.openFileForRead("QUOTE", tstats::kFavouritesPath, in)) {
    return;
  }
  auto line = makeUniqueNoThrow<char[]>(kLineCap);
  if (line) {
    const std::string body = quoteBody(text_, author_);
    while (!favourite_ && tools::readLine(in, line.get(), kLineCap) >= 0) {
      const char* bar = strchr(line.get(), '|');
      favourite_ = strcmp(bar != nullptr ? bar + 1 : line.get(), body.c_str()) == 0;
    }
  }
  in.close();
}

bool DailyQuoteActivity::toggleFavourite() {
  if (!Storage.ensureDirectoryExists(kFavDir)) return false;
  const std::string body = quoteBody(text_, author_);
  char date[12];
  tools::formatIsoDate(date, sizeof(date), clockValid_ ? shownDay_ : 0);
  FavCtx ctx{&body, date};
  const bool ok =
      tools::writeFileAtomic(tstats::kFavouritesPath, favourite_ ? &writeFavouriteRemoved : &writeFavouriteAdded, &ctx);
  if (ok) favourite_ = !favourite_;
  return ok;
}

bool DailyQuoteActivity::showFavourite(const int index) {
  favCount_ = tstats::countFavouriteQuotes();
  if (favCount_ == 0) return false;
  favIndex_ = (index % favCount_ + favCount_) % favCount_;
  FsFile in;
  if (!Storage.openFileForRead("QUOTE", tstats::kFavouritesPath, in)) return false;
  int k = -1;
  while (tools::readLine(in, line_, sizeof(line_)) >= 0) {
    if (line_[0] == '\0' || line_[0] == '#') continue;
    if (++k == favIndex_) break;
  }
  in.close();
  if (k != favIndex_) return false;
  page_ = 0;
  parseLine();
  favourite_ = true;
  return true;
}

void DailyQuoteActivity::openMenu() {
  enum Action : uint8_t { Favourite, Random, Today, Favourites, Stats, Daily };
  std::vector<std::string> options;
  std::vector<uint8_t> actions;
  options.reserve(5);
  actions.reserve(5);
  auto add = [&](const Action a, const char* label) {
    options.emplace_back(label);
    actions.push_back(a);
  };
  if (status_ == Status::Ok) {
    add(Favourite, favourite_ ? tr(STR_TOOLS_QT_UNFAVOURITE) : tr(STR_TOOLS_QT_FAVOURITE));
  }
  if (browsingFavourites_) {
    add(Daily, tr(STR_TOOLS_QT_BACK_TO_DAILY));
  } else {
    if (status_ == Status::Ok) add(Random, tr(STR_TOOLS_QT_RANDOM));
    if (clockValid_ && (shownDay_ != today_ || randomPick_)) add(Today, tr(STR_TOOLS_QT_TODAY));
    add(Favourites, tr(STR_TOOLS_QT_MY_FAVOURITES));
  }
  add(Stats, tr(STR_TOOLS_HABIT_STATS));
  auto picker = makeUniqueNoThrow<OptionSelectionActivity>(renderer, mappedInput, "QuoteMenu",
                                                           StrId::STR_TOOLS_DAILY_QUOTE, std::move(options), 0);
  if (!picker) return;
  startActivityForResult(std::move(picker), [this, actions](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    const auto* sel = std::get_if<OptionSelectionResult>(&result.data);
    if (result.isCancelled || sel == nullptr || sel->index >= actions.size()) {
      requestUpdate();
      return;
    }
    if (actions[sel->index] == Stats) {
      auto stats = makeUniqueNoThrow<ToolStatsActivity>(renderer, mappedInput, tr(STR_TOOLS_QT_STATS),
                                                        &toolstats::buildQuotes, nullptr, statsx::Feature::Quotes);
      if (stats) {
        startActivityForResult(std::move(stats), [this](const ActivityResult&) {
          input_.reset(mappedInput);
          transitionPending_ = true;
          requestUpdate();
        });
      }
      return;
    }
    RenderLock lock(*this);  // line_ and the quote pointers are read by render()
    switch (actions[sel->index]) {
      case Favourite: {
        const bool wasBrowsing = browsingFavourites_;
        toggleFavourite();
        if (wasBrowsing && !showFavourite(favIndex_)) {
          browsingFavourites_ = false;
          if (clockValid_) showDay(today_);
        }
        break;
      }
      case Random:
        showRandom(mix(static_cast<uint32_t>(micros()) ^ static_cast<uint32_t>(millis())));
        break;
      case Today:
        showDay(today_);
        break;
      case Favourites:
        browsingFavourites_ = showFavourite(0);
        break;
      default:  // Daily
        browsingFavourites_ = false;
        if (clockValid_) {
          showDay(today_);
        } else {
          showRandom(mix(static_cast<uint32_t>(millis())));
        }
        break;
    }
    requestUpdate();
  });
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

void DailyQuoteActivity::onExit() {
  kannada::release();
  Activity::onExit();
}

void DailyQuoteActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  kannada::acquire();  // Kannada text in the user's files, when the font is on the card
  tools::ensureToolsDirs();
  tools::DateTime local;
  clockValid_ = tools::getLocalNow(local);
  if (clockValid_) {
    today_ = tools::daysOf(local);
    showDay(today_);
    // History for Quote stats (days opened, streak).
    char fields[24];
    char date[12];
    tools::formatIsoDate(date, sizeof(date), today_);
    snprintf(fields, sizeof(fields), "open|%s", date);
    tlog::append("quotes", {today_, static_cast<int16_t>(local.hour * 60 + local.minute)}, fields);
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
  if (input_.back && browsingFavourites_) {
    RenderLock lock(*this);
    browsingFavourites_ = false;
    if (clockValid_) showDay(today_);
    transitionPending_ = true;
    requestUpdate();
    return;
  }
  if (input_.back) {
    finish();
    return;
  }
  // line_ (and text_/author_ pointing into it) is read by render(); hold the
  // render lock while replacing it.
  if (input_.confirm) {
    openMenu();
    return;
  } else if (browsingFavourites_ && (input_.left || input_.right)) {
    RenderLock lock(*this);
    showFavourite(favIndex_ + (input_.left ? -1 : 1));
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
  const Rect frame = tools::drawFrame(renderer, tr(STR_TOOLS_DAILY_QUOTE));
  Rect content = frame;

  // Date first, large: "Monday" / "28 September 2026" (or "Favourite 2 of 5").
  if (status_ == Status::Ok) {
    int y = frame.y;
    char line1[48];
    char line2[48];
    if (browsingFavourites_) {
      snprintf(line1, sizeof(line1), "%s", tr(STR_TOOLS_QT_MY_FAVOURITES));
      snprintf(line2, sizeof(line2), "%d / %d", favIndex_ + 1, favCount_);
    } else if (clockValid_) {
      snprintf(line1, sizeof(line1), "%s", tools::weekdayName(tools::weekdayMon0(shownDay_)));
      // Always spelled out ("28 September 2026") so the date reads at a glance.
      static constexpr StrId kMonths[] = {
          StrId::STR_MONTH_JANUARY,   StrId::STR_MONTH_FEBRUARY, StrId::STR_MONTH_MARCH,    StrId::STR_MONTH_APRIL,
          StrId::STR_MONTH_MAY,       StrId::STR_MONTH_JUNE,     StrId::STR_MONTH_JULY,     StrId::STR_MONTH_AUGUST,
          StrId::STR_MONTH_SEPTEMBER, StrId::STR_MONTH_OCTOBER,  StrId::STR_MONTH_NOVEMBER, StrId::STR_MONTH_DECEMBER};
      uint16_t yy = 0;
      uint8_t mm = 0, dd = 0;
      tools::civilFromDays(shownDay_, yy, mm, dd);
      snprintf(line2, sizeof(line2), "%u %s %u", static_cast<unsigned>(dd), I18N.get(kMonths[mm - 1]),
               static_cast<unsigned>(yy));
    } else {
      line1[0] = line2[0] = '\0';
    }
    if (line1[0] != '\0') {
      renderer.drawText(UI_12_FONT_ID, frame.x, y, line1, true, EpdFontFamily::REGULAR);
      if (!browsingFavourites_ && clockValid_ && shownDay_ == today_) {
        // "Today" pill after the weekday.
        const int wx = frame.x + renderer.getTextWidth(UI_12_FONT_ID, line1) + 10;
        const char* t = tr(STR_TOOLS_TODAY);
        const int tw = renderer.getTextWidth(SMALL_FONT_ID, t, EpdFontFamily::BOLD);
        renderer.fillRoundedRect(wx, y + 3, tw + 12, renderer.getLineHeight(UI_12_FONT_ID) - 4, 6, Color::Black);
        renderer.drawText(SMALL_FONT_ID, wx + 6, y + 5, t, false, EpdFontFamily::BOLD);
      }
      if (favourite_) {
        const char* f = tr(STR_TOOLS_QT_FAVOURITE_TAG);
        const int fw = renderer.getTextWidth(SMALL_FONT_ID, f, EpdFontFamily::BOLD);
        renderer.drawRect(frame.x + frame.width - fw - 12, y + 3, fw + 12, renderer.getLineHeight(UI_12_FONT_ID) - 4);
        renderer.drawText(SMALL_FONT_ID, frame.x + frame.width - fw - 6, y + 5, f, true, EpdFontFamily::BOLD);
      }
      y += renderer.getLineHeight(UI_12_FONT_ID);
      renderer.drawText(BITTER_16_FONT_ID, frame.x, y, line2, true, EpdFontFamily::BOLD);
      y += renderer.getLineHeight(BITTER_16_FONT_ID) + 6;
      renderer.fillRect(frame.x, y, frame.width, 2);
      y += 10;
      content = Rect{frame.x, y, frame.width, frame.y + frame.height - y};
    }
  }

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
      if (lines * tools::wrapLineHeight(renderer, font, text_) <= box.height) break;
    }
    // A quote too long even at the smallest size is paged with Up/Down; the
    // author appears on the last page.
    const int lineH = tools::wrapLineHeight(renderer, font, text_);
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

  if (browsingFavourites_) {
    tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_MENU), tr(STR_TOOLS_PREVIOUS),
                     tr(STR_TOOLS_NEXT));
  } else {
    tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_MENU), clockValid_ ? tr(STR_TOOLS_PREV_DAY) : "",
                     clockValid_ ? tr(STR_TOOLS_NEXT_DAY) : "");
  }
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
