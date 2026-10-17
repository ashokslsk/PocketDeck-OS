#include "BookSearchActivity.h"

#include <Epub.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Print.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "KannadaReading.h"
#include "MappedInputManager.h"
#include "activities/ActivityResult.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {
constexpr size_t kContextBefore = 18;  // short, so the match itself shows on one row
constexpr size_t kContextAfter = 60;
constexpr unsigned long kProgressRedrawMs = 1500;

char lowerAscii(const char c) {
  return static_cast<unsigned char>(c) < 0x80 ? static_cast<char>(std::tolower(static_cast<unsigned char>(c))) : c;
}

void appendUtf8(std::string& out, uint32_t cp) {
  if (cp < 0x80) {
    out.push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

// Drops a trailing partial UTF-8 character so a snippet never ends mid-letter.
void trimPartialUtf8(std::string& s) {
  size_t i = s.size();
  size_t continuation = 0;
  while (i > 0 && (static_cast<unsigned char>(s[i - 1]) & 0xC0) == 0x80 && continuation < 3) {
    --i;
    ++continuation;
  }
  if (i == 0) return;
  const auto lead = static_cast<unsigned char>(s[i - 1]);
  const size_t need = lead >= 0xF0 ? 3 : lead >= 0xE0 ? 2 : lead >= 0xC0 ? 1 : 0;
  if (need > continuation) s.resize(i - 1);
}

bool containsKannada(const std::string& s) {
  for (size_t i = 0; i + 1 < s.size(); ++i) {
    if (static_cast<unsigned char>(s[i]) == 0xE0 &&
        (static_cast<unsigned char>(s[i + 1]) == 0xB2 || static_cast<unsigned char>(s[i + 1]) == 0xB3)) {
      return true;
    }
  }
  return false;
}

// Drops leading UTF-8 continuation bytes so a snippet never starts mid-letter.
std::string fromCharBoundary(const std::string& s) {
  size_t i = 0;
  while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
  return s.substr(i);
}

// Streams one chapter's XHTML: skips tags, <head>/<style>/<script> contents
// and decodes entities, then matches the visible text against the needle.
class SearchSink final : public Print {
 public:
  SearchSink(const std::string& needle, const uint16_t spine, std::vector<BookSearchActivity::Hit>& hits)
      : needle_(needle), spine_(spine), hits_(hits) {}

  size_t write(uint8_t b) override {
    if (full()) return 0;
    take(static_cast<char>(b));
    return 1;
  }
  size_t write(const uint8_t* buf, size_t n) override {
    if (full()) return 0;  // early stop: enough hits
    for (size_t i = 0; i < n; ++i) take(static_cast<char>(buf[i]));
    return n;
  }
  uint32_t bytesSeen() const { return raw_; }
  bool full() const { return hits_.size() >= BookSearchActivity::kMaxHits && pendingAfter_ == 0; }

 private:
  void take(const char c) {
    ++raw_;
    if (inTag_) {
      if (c == '>') {
        inTag_ = false;
        tag_[tagLen_] = '\0';
        closeTag();
      } else if (!tagNameDone_) {
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
          tagNameDone_ = tagLen_ > 0;
        } else if (tagLen_ < sizeof(tag_) - 1) {
          tag_[tagLen_++] = lowerAscii(c);
        }
      }
      lastTagChar_ = c;
      return;
    }
    if (c == '<') {
      inTag_ = true;
      tagNameDone_ = false;
      tagLen_ = 0;
      return;
    }
    if (skipDepth_ > 0) return;
    if (inEntity_) {
      if (c == ';' || entity_.size() > 8) {
        emitEntity();
        inEntity_ = false;
      } else {
        entity_.push_back(c);
      }
      return;
    }
    if (c == '&') {
      inEntity_ = true;
      entity_.clear();
      return;
    }
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
      if (!lastSpace_) emit(' ');
      return;
    }
    emit(c);
  }

  void closeTag() {
    const char* name = tag_;
    const bool closing = name[0] == '/';
    if (closing) ++name;
    if (std::strncmp(name, "head", 4) == 0 || std::strncmp(name, "style", 5) == 0 ||
        std::strncmp(name, "script", 6) == 0) {
      if (closing) {
        if (skipDepth_ > 0) --skipDepth_;
      } else if (lastTagChar_ != '/') {  // <head/> opens nothing
        ++skipDepth_;
      }
      return;
    }
    // Block boundaries separate words.
    if (!lastSpace_ &&
        (std::strncmp(name, "p", 1) == 0 || std::strncmp(name, "br", 2) == 0 || std::strncmp(name, "div", 3) == 0 ||
         std::strncmp(name, "h", 1) == 0 || std::strncmp(name, "li", 2) == 0)) {
      emit(' ');
    }
  }

  void emitEntity() {
    std::string text;
    if (entity_ == "amp") {
      text = "&";
    } else if (entity_ == "lt") {
      text = "<";
    } else if (entity_ == "gt") {
      text = ">";
    } else if (entity_ == "quot") {
      text = "\"";
    } else if (entity_ == "apos") {
      text = "'";
    } else if (entity_ == "nbsp") {
      text = " ";
    } else if (!entity_.empty() && entity_[0] == '#') {
      const bool hex = entity_.size() > 1 && (entity_[1] == 'x' || entity_[1] == 'X');
      const uint32_t cp = static_cast<uint32_t>(std::strtoul(entity_.c_str() + (hex ? 2 : 1), nullptr, hex ? 16 : 10));
      if (cp == 0xA0) {
        text = " ";
      } else if (cp > 0) {
        appendUtf8(text, cp);
      }
    }
    for (const char ch : text) {
      if (ch == ' ') {
        if (!lastSpace_) emit(' ');
      } else {
        emit(ch);
      }
    }
  }

  void emit(const char c) {
    lastSpace_ = c == ' ';
    // Context for snippets (original case) and the match window (lower case).
    context_.push_back(c);
    if (context_.size() > kContextBefore + needle_.size())
      context_.erase(0, context_.size() - kContextBefore - needle_.size());
    if (pendingAfter_ > 0 && !hits_.empty()) {
      hits_.back().snippet.push_back(c);
      if (--pendingAfter_ == 0) closeSnippet(hits_.back().snippet);
    }
    window_.push_back(lowerAscii(c));
    if (window_.size() > needle_.size()) window_.erase(0, window_.size() - needle_.size());
    if (cooldown_ > 0) {
      --cooldown_;
      return;
    }
    if (window_ == needle_ && hits_.size() < BookSearchActivity::kMaxHits) {
      BookSearchActivity::Hit hit;
      hit.spine = spine_;
      hit.offset = raw_;
      hit.progress = 0.0f;
      hit.snippet = "\xe2\x80\xa6" + fromCharBoundary(context_);
      if (pendingAfter_ > 0 && !hits_.empty()) closeSnippet(hits_.back().snippet);
      hits_.push_back(std::move(hit));
      pendingAfter_ = kContextAfter;
      cooldown_ = needle_.size();  // one hit per occurrence
    }
  }

  static void closeSnippet(std::string& snippet) {
    trimPartialUtf8(snippet);
    snippet += "\xe2\x80\xa6";
  }

  const std::string& needle_;
  uint16_t spine_;
  std::vector<BookSearchActivity::Hit>& hits_;
  uint32_t raw_ = 0;
  bool inTag_ = false;
  char tag_[12] = {};
  size_t tagLen_ = 0;
  bool tagNameDone_ = false;
  char lastTagChar_ = 0;
  int skipDepth_ = 0;
  bool inEntity_ = false;
  std::string entity_;
  bool lastSpace_ = true;
  std::string context_;
  std::string window_;
  size_t pendingAfter_ = 0;
  size_t cooldown_ = 0;
};
}  // namespace

BookSearchActivity::BookSearchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                       std::shared_ptr<Epub> epub, std::string query)
    : Activity("BookSearch", renderer, mappedInput),
      epub_(std::move(epub)),
      query_(std::move(query)),
      uiTarget_(makeUiTarget(renderer)),
      app_(uiTarget_, uiTarget_.deviceContext()) {}

void BookSearchActivity::onEnter() {
  Activity::onEnter();
  kannadareading::beginUi();
  // Normalize the query like the book text: lower-case Latin, single spaces.
  std::string needle;
  bool space = true;
  for (const char c : query_) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      if (!space) needle.push_back(' ');
      space = true;
    } else {
      needle.push_back(lowerAscii(c));
      space = false;
    }
  }
  while (!needle.empty() && needle.back() == ' ') needle.pop_back();
  hits_.clear();
  hits_.reserve(16);
  needle_ = std::move(needle);
  spineCount_ = epub_ ? epub_->getSpineItemsCount() : 0;
  nextSpine_ = 0;
  searching_ = spineCount_ > 0 && !needle_.empty();
  lastDrawMs_ = millis();
  applySharedUiTheme(app_, uiTarget_);
  app_.on(ACTION_ROW, &BookSearchActivity::onRowEvent, this);
  app_.setScreen(&BookSearchActivity::listScreen, this);
  requestUpdate();
}

void BookSearchActivity::onExit() {
  kannadareading::endUi();
  Activity::onExit();
}

void BookSearchActivity::searchChapter(const int spine) {
  // A table of contents only repeats chapter titles; leave it out.
  if (epub_->isNavigationDocumentSpine(spine)) return;
  const auto entry = epub_->getSpineItem(spine);
  const size_t before = hits_.size();
  SearchSink sink(needle_, static_cast<uint16_t>(spine), hits_);
  if (!epub_->readItemContentsToStream(entry.href, sink, 1024, /*allowEarlyStop=*/true)) {
    if (!sink.full()) LOG_ERR("SRCH", "Could not read chapter %d", spine);
  }
  const uint32_t total = std::max<uint32_t>(1, sink.bytesSeen());
  for (size_t i = before; i < hits_.size(); ++i) {
    hits_[i].progress = std::min(1.0f, static_cast<float>(hits_[i].offset) / static_cast<float>(total));
  }
  if (sink.full()) {
    truncated_ = true;
    searching_ = false;
  }
}

void BookSearchActivity::listScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<BookSearchActivity*>(user)->buildListScreen(screen);
}

void BookSearchActivity::buildListScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) +
                                       metrics.verticalSpacing),
                  0, static_cast<int16_t>(metrics.buttonHintsHeight + metrics.verticalSpacing), 0});
  if (itemsBuiltFor_ != hits_.size()) {
    labels_.clear();
    subtitles_.clear();
    uiItems_.clear();
    labels_.reserve(hits_.size());
    subtitles_.reserve(hits_.size());
    uiItems_.reserve(hits_.size());
    const size_t bookSize = std::max<size_t>(1, epub_->getBookSize());
    for (const auto& hit : hits_) {
      labels_.push_back(hit.snippet);
      std::string chapter;
      const int toc = epub_->getTocIndexForSpineIndex(hit.spine);
      if (toc >= 0) chapter = epub_->getTocItem(toc).title;
      const size_t start = hit.spine > 0 ? epub_->getCumulativeSpineItemSize(hit.spine - 1) : 0;
      const size_t end = epub_->getCumulativeSpineItemSize(hit.spine);
      const int percent =
          static_cast<int>((static_cast<double>(start) + hit.progress * static_cast<double>(end - start)) * 100.0 /
                           static_cast<double>(bookSize));
      char where[24];
      snprintf(where, sizeof(where), "%d%%", std::max(0, std::min(100, percent)));
      // Kannada is taller than a two-line row allows: show the snippet on one
      // line with its position at the right instead.
      if (containsKannada(hit.snippet) || containsKannada(chapter)) {
        subtitles_.push_back(std::string("\x01") + where);
      } else {
        subtitles_.push_back(chapter.empty() ? std::string(where) : chapter + " \xc2\xb7\xc2\xa0" + where);
      }
    }
    for (size_t i = 0; i < hits_.size(); ++i) {
      fui::ListItem item;
      item.label = labels_[i].c_str();
      if (!subtitles_[i].empty() && subtitles_[i][0] == '\x01') {
        item.value = subtitles_[i].c_str() + 1;  // one-line Kannada row
      } else {
        item.subtitle = subtitles_[i].c_str();
      }
      item.actionValue = static_cast<int16_t>(i);
      uiItems_.push_back(item);
    }
    itemsBuiltFor_ = hits_.size();
  }
  fui::ListProps props;
  props.items = uiItems_.data();
  props.count = static_cast<uint16_t>(uiItems_.size());
  props.selectedIndex = static_cast<int16_t>(selectedIndex_);
  props.topIndex = static_cast<uint16_t>(topIndex_);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.labelText = screen.theme().bodyText;
  props.labelText.maxLines = 2;
  const auto rows = configureUiList(props, screen.theme(), screen.body());
  visibleRows_ = rows > 0 ? rows : 1;
  topIndex_ = scrollListBy(topIndex_, 0, visibleRows_, static_cast<int>(uiItems_.size()));
  props.topIndex = static_cast<uint16_t>(topIndex_);
  screen.list(props);
}

void BookSearchActivity::onRowEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<BookSearchActivity*>(user);
  if (event.value < 0 || event.value >= static_cast<int16_t>(self->hits_.size())) return;
  self->app_.clearTapFlash();
  self->choose(event.value);
}

void BookSearchActivity::choose(const int index) {
  if (index < 0 || index >= static_cast<int>(hits_.size())) return;
  BookmarkResult result;
  result.spineIndex = hits_[index].spine;
  result.progress = hits_[index].progress;
  setResult(result);
  finish();
}

void BookSearchActivity::loop() {
  if (searching_) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      searching_ = false;  // keep what was found so far
      requestUpdate();
      return;
    }
    {
      RenderLock lock(*this);  // EPUB metadata reads share a file with rendering
      searchChapter(nextSpine_);
      ++nextSpine_;
      if (nextSpine_ >= spineCount_) searching_ = false;
    }
    if (!searching_ || millis() - lastDrawMs_ >= kProgressRedrawMs) {
      lastDrawMs_ = millis();
      requestUpdate();
    }
    return;
  }

  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult cancelled;
    cancelled.isCancelled = true;
    setResult(std::move(cancelled));
    finish();
    return;
  }
  if (uiReady_) {
    const fui::InputSnapshot snap = touchSnapshotFrom(mappedInput);
    if (snap.touchPressed || snap.touchReleased) {
      const auto event = app_.route(snap);
      if (app_.invalidated()) requestUpdate();
      if (event) return;
    }
  }
  const int total = static_cast<int>(hits_.size());
  if (total == 0) return;
  buttonNavigator_.onNextRelease([this, total] {
    selectedIndex_ = ButtonNavigator::nextIndex(selectedIndex_, total);
    topIndex_ = followListSelection(selectedIndex_, topIndex_, visibleRows_, total);
    requestUpdate();
  });
  buttonNavigator_.onPreviousRelease([this, total] {
    selectedIndex_ = ButtonNavigator::previousIndex(selectedIndex_, total);
    topIndex_ = followListSelection(selectedIndex_, topIndex_, visibleRows_, total);
    requestUpdate();
  });
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) choose(selectedIndex_);
}

void BookSearchActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  char title[96];
  snprintf(title, sizeof(title), "\xe2\x80\x9c%s\xe2\x80\x9d", query_.c_str());
  char status[64];
  if (searching_) {
    snprintf(status, sizeof(status), tr(STR_BOOK_SEARCHING_FORMAT), nextSpine_, spineCount_);
  } else {
    snprintf(status, sizeof(status), truncated_ ? tr(STR_SEARCH_FIRST_FORMAT) : tr(STR_SEARCH_FOUND_FORMAT),
             static_cast<unsigned>(hits_.size()));
  }
  const Rect header{0, metrics.topPadding, pageWidth, TouchHeaderBackButton::height(metrics, mappedInput)};
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, uiTarget_, header, title, true);
  } else {
    GUI.drawHeader(renderer, header, title, status);
  }
  uiReady_ = false;
  if (hits_.empty()) {
    const int contentTop = header.y + header.height;
    renderer.drawCenteredText(UI_10_FONT_ID, contentTop + (pageHeight - contentTop - metrics.buttonHintsHeight) / 2,
                              searching_ ? status : tr(STR_BOOK_SEARCH_NO_MATCH));
  } else {
    app_.render();
    uiReady_ = true;
  }
  const auto labels =
      searching_ ? mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_SEARCH_STOP)), "", "", "")
                 : mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), hits_.empty() ? "" : tr(STR_OPEN),
                                         hits_.empty() ? "" : tr(STR_DIR_UP), hits_.empty() ? "" : tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
