#include "KnowledgeCardActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <strings.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "KannadaText.h"
#include "ToolStatsPages.h"
#include "ToolsLog.h"
#include "components/UITheme.h"
#include "fontIds.h"

constexpr char KnowledgeCardActivity::kDir[];

namespace {
using Tok = tools::JsonReader::Token;
constexpr uint32_t kIndexMagic = 0x31584e4b;  // "KNX1"

struct IndexHeader {
  uint32_t magic;
  uint32_t sourceSize;
  uint32_t count;
};

struct BuildCtx {
  FsFile* src;
  uint32_t sourceSize;
  uint32_t count;
  uint32_t maxCards;
};

bool hasJsonExtension(const char* name) {
  const size_t n = strlen(name);
  return n > 5 && strcasecmp(name + n - 5, ".json") == 0;
}

// Reads one {"question","answer"} object whose '{' was already consumed.
bool readItem(tools::JsonReader& r, char* question, const size_t qCap, char* answer, const size_t aCap) {
  char key[16];
  if (question != nullptr && qCap > 0) question[0] = '\0';
  if (answer != nullptr && aCap > 0) answer[0] = '\0';
  bool sawQuestion = false;
  while (true) {
    Tok t = r.next(key, sizeof(key));
    if (t == Tok::Comma) continue;
    if (t == Tok::ObjectEnd) return sawQuestion;
    if (t != Tok::String) return false;
    if (r.next(nullptr, 0) != Tok::Colon) return false;
    const bool isQ = strcmp(key, "question") == 0;
    const bool isA = strcmp(key, "answer") == 0;
    t = isQ ? r.next(question, qCap) : isA ? r.next(answer, aCap) : r.next(nullptr, 0);
    if (t == Tok::End || t == Tok::Error) return false;
    if (isQ) sawQuestion = true;
    if (!isQ && !isA && !r.skipValue(t)) return false;
  }
}
}  // namespace

void KnowledgeCardActivity::topicPath(char* buf, const size_t len, const int topic) const {
  snprintf(buf, len, "%s/%s.json", kDir, topics_[topic]);
}

void KnowledgeCardActivity::indexPath(char* buf, const size_t len, const int topic) const {
  snprintf(buf, len, "%s/kn_%s.idx", tools::kToolsCacheDir, topics_[topic]);
}

void KnowledgeCardActivity::scanTopics() {
  topicCount_ = 0;
  Storage.ensureDirectoryExists(kDir);
  FsFile dir = Storage.open(kDir);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return;
  }
  char name[kNameCap + 8];
  for (FsFile entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    const bool isFile = !entry.isDirectory();
    entry.getName(name, sizeof(name));
    entry.close();
    if (!isFile || name[0] == '.' || !hasJsonExtension(name) || topicCount_ >= kMaxTopics) continue;
    name[strlen(name) - 5] = '\0';
    if (strlen(name) >= kNameCap) continue;
    snprintf(topics_[topicCount_++], kNameCap, "%s", name);
  }
  dir.close();
  for (int i = 1; i < topicCount_; ++i) {
    for (int j = i; j > 0 && strcasecmp(topics_[j - 1], topics_[j]) > 0; --j) {
      char tmp[kNameCap];
      memcpy(tmp, topics_[j], kNameCap);
      memcpy(topics_[j], topics_[j - 1], kNameCap);
      memcpy(topics_[j - 1], tmp, kNameCap);
    }
  }
  // Card counts for the list (indexes are cached, so this is cheap after the first run).
  for (int i = 0; i < topicCount_; ++i) {
    openTopic_ = i;
    uint32_t count = 0;
    topicCounts_[i] = ensureIndex(count) ? static_cast<uint16_t>(count) : 0;
  }
  openTopic_ = -1;
}

bool KnowledgeCardActivity::buildIndex(FsFile& out, void* ctx) {
  auto* b = static_cast<BuildCtx*>(ctx);
  IndexHeader header{kIndexMagic, b->sourceSize, 0};
  if (out.write(&header, sizeof(header)) != sizeof(header)) return false;
  tools::JsonReader r(*b->src);
  if (!tools::findFirstArray(r)) return false;
  while (b->count < b->maxCards) {
    const Tok t = r.next(nullptr, 0);
    if (t == Tok::Comma) continue;
    if (t != Tok::ObjectStart) break;
    const uint32_t offset = r.tokenOffset();
    if (!readItem(r, nullptr, 0, nullptr, 0)) return false;
    if (out.write(&offset, sizeof(offset)) != sizeof(offset)) return false;
    ++b->count;
  }
  header.count = b->count;
  return out.seek(0) && out.write(&header, sizeof(header)) == sizeof(header);
}

bool KnowledgeCardActivity::ensureIndex(uint32_t& count) const {
  count = 0;
  char src[96];
  char idx[96];
  topicPath(src, sizeof(src), openTopic_);
  indexPath(idx, sizeof(idx), openTopic_);
  FsFile deck;
  if (!Storage.openFileForRead("KNOW", src, deck)) return false;
  const auto sourceSize = static_cast<uint32_t>(deck.fileSize());
  FsFile cached;
  if (Storage.exists(idx) && Storage.openFileForRead("KNOW", idx, cached)) {
    IndexHeader header{};
    const bool valid = cached.read(&header, sizeof(header)) == sizeof(header) && header.magic == kIndexMagic &&
                       header.sourceSize == sourceSize;
    cached.close();
    if (valid) {
      deck.close();
      count = header.count;
      return count > 0;
    }
  }
  LOG_INF("KNOW", "Indexing %s (%u bytes)", src, sourceSize);
  BuildCtx ctx{&deck, sourceSize, 0, kMaxCards};
  const bool ok = tools::writeFileAtomic(idx, &KnowledgeCardActivity::buildIndex, &ctx);
  deck.close();
  if (!ok) {
    LOG_ERR("KNOW", "Could not index %s (expected {\"cards\": [{question, answer}]})", src);
    return false;
  }
  count = ctx.count;
  return count > 0;
}

bool KnowledgeCardActivity::loadItem(const int index) {
  question_[0] = answer_[0] = '\0';
  page_ = 0;
  char src[96];
  char idx[96];
  topicPath(src, sizeof(src), openTopic_);
  indexPath(idx, sizeof(idx), openTopic_);
  uint32_t offset = 0;
  FsFile f;
  if (!Storage.openFileForRead("KNOW", idx, f)) return false;
  const bool ok = f.seek(sizeof(IndexHeader) + static_cast<size_t>(index) * sizeof(uint32_t)) &&
                  f.read(&offset, sizeof(offset)) == sizeof(offset);
  f.close();
  if (!ok || !Storage.openFileForRead("KNOW", src, f)) return false;
  tools::JsonReader r(f);
  const bool parsed = r.seek(offset) && r.next(nullptr, 0) == Tok::ObjectStart &&
                      readItem(r, question_, sizeof(question_), answer_, sizeof(answer_));
  f.close();
  if (!parsed) LOG_ERR("KNOW", "Failed to read item %d of %s", index, src);
  item_ = index;
  return parsed;
}

bool KnowledgeCardActivity::openTopic(const int topic) {
  openTopic_ = topic;
  uint32_t count = 0;
  if (!ensureIndex(count)) {
    screen_ = Screen::Error;
    return false;
  }
  itemCount_ = static_cast<int>(count);
  todayItem_ = static_cast<int>(((today_ % itemCount_) + itemCount_) % itemCount_);
  if (!loadItem(todayItem_)) {
    screen_ = Screen::Error;
    return false;
  }
  screen_ = Screen::Question;
  return true;
}

void KnowledgeCardActivity::onExit() {
  kannada::release();
  Activity::onExit();
}

void KnowledgeCardActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  kannada::acquire();  // Kannada text in the user's files, when the font is on the card
  tools::ensureToolsDirs();
  tools::DateTime local;
  today_ = tools::getLocalNow(local) ? tools::daysOf(local) : 0;
  scanTopics();
  screen_ = Screen::Topics;
  transitionPending_ = true;
  requestUpdate();
}

void KnowledgeCardActivity::showAnswer() {
  screen_ = Screen::Answer;
  page_ = 0;
  // Study history for Knowledge stats: which question of which topic.
  tlog::Stamp at;
  if (openTopic_ >= 0 && tlog::now(at)) {
    char fields[48];
    snprintf(fields, sizeof(fields), "%s|%d", topics_[openTopic_], item_);
    tlog::append("knowledge", at, fields);
  }
}

void KnowledgeCardActivity::openStats() {
  auto stats = makeUniqueNoThrow<ToolStatsActivity>(renderer, mappedInput, tr(STR_TOOLS_KN_STATS),
                                                    &toolstats::buildKnowledge, nullptr, statsx::Feature::Knowledge);
  if (!stats) return;
  startActivityForResult(std::move(stats), [this](const ActivityResult&) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    requestUpdate();
  });
}

void KnowledgeCardActivity::loop() {
  input_.poll(mappedInput);
  if (input_.backLong) {
    tools::exitToHome();
    return;
  }
  switch (screen_) {
    case Screen::Topics:
      if (input_.back) {
        finish();
        return;
      }
      if (input_.leftUp) {
        openStats();
        return;
      }
      if (topicCount_ == 0) return;
      if (input_.up || input_.pageBack) {
        selectedTopic_ = (selectedTopic_ + topicCount_ - 1) % topicCount_;
        requestUpdate();
      } else if (input_.next()) {
        selectedTopic_ = (selectedTopic_ + 1) % topicCount_;
        requestUpdate();
      } else if (input_.confirm) {
        RenderLock lock(*this);  // question/answer buffers are read by render()
        openTopic(selectedTopic_);
        requestUpdate();
      }
      break;
    case Screen::Question:
    case Screen::Answer: {
      if (input_.back) {
        screen_ = Screen::Topics;
        requestUpdate();
        return;
      }
      const bool answer = screen_ == Screen::Answer;
      if (input_.confirm) {
        if (answer) {
          screen_ = Screen::Question;
          page_ = 0;
        } else {
          showAnswer();
        }
        requestUpdate();
      } else if (input_.up || input_.pageBack) {
        RenderLock lock(*this);
        loadItem((item_ + itemCount_ - 1) % itemCount_);
        screen_ = Screen::Question;
        requestUpdate();
      } else if (input_.down || input_.pageForward) {
        RenderLock lock(*this);
        loadItem((item_ + 1) % itemCount_);
        screen_ = Screen::Question;
        requestUpdate();
      } else if (input_.right) {
        // Right on a question goes to the next question; on an answer it turns the page.
        if (!answer) {
          RenderLock lock(*this);
          loadItem((item_ + 1) % itemCount_);
          requestUpdate();
        } else if (page_ + 1 < pageCount_) {
          ++page_;
          requestUpdate();
        }
      } else if (input_.left) {
        if (answer && page_ > 0) {
          --page_;
          requestUpdate();
        } else if (answer) {
          screen_ = Screen::Question;
          requestUpdate();
        } else {
          RenderLock lock(*this);
          loadItem((item_ + itemCount_ - 1) % itemCount_);
          requestUpdate();
        }
      } else if (input_.confirmLong && item_ != todayItem_) {
        RenderLock lock(*this);
        loadItem(todayItem_);
        screen_ = Screen::Question;
        requestUpdate();
      }
      break;
    }
    case Screen::Error:
      if (input_.back || input_.confirm) {
        screen_ = Screen::Topics;
        requestUpdate();
      }
      break;
  }
}

void KnowledgeCardActivity::render(RenderLock&&) {
  if (screen_ == Screen::Topics) {
    const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_KNOWLEDGE));
    if (topicCount_ == 0) {
      const int midY = content.y + content.height / 2;
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_TOOLS_NO_CARDS), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, tr(STR_TOOLS_NO_CARDS_HINT));
      tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
    } else {
      GUI.drawList(
          renderer, Rect{0, content.y, renderer.getScreenWidth(), content.height}, topicCount_, selectedTopic_,
          [this](const int i) { return std::string(topics_[i]); },
          [this](const int i) {
            char sub[40];
            snprintf(sub, sizeof(sub), "%u %s", topicCounts_[i], tr(STR_TOOLS_QUESTIONS));
            return std::string(sub);
          });
      tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_OPEN), tr(STR_TOOLS_STATS_SHORT),
                       tr(STR_TOOLS_NEXT));
    }
  } else if (screen_ == Screen::Error) {
    const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_KNOWLEDGE));
    const int midY = content.y + content.height / 2;
    renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_TOOLS_DECK_ERROR), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, tr(STR_TOOLS_DECK_ERROR_HINT));
    tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
  } else {
    char subtitle[40];
    snprintf(subtitle, sizeof(subtitle), "%s%d / %d", item_ == todayItem_ ? "* " : "", item_ + 1, itemCount_);
    const Rect content = tools::drawFrame(renderer, topics_[openTopic_], subtitle);
    const int footerH = renderer.getLineHeight(SMALL_FONT_ID) + 8;

    if (screen_ == Screen::Question) {
      renderer.drawText(SMALL_FONT_ID, content.x, content.y, tr(STR_TOOLS_QUESTION_LABEL));
      Rect box{content.x + 8, content.y, content.width - 16, content.height - footerH};
      tools::WrapOptions measure;
      measure.fontId = BITTER_14_FONT_ID;
      measure.centered = true;
      measure.draw = false;
      const int lines = tools::drawWrappedText(renderer, box, question_, measure);
      const int textH = std::min(box.height, lines * tools::wrapLineHeight(renderer, BITTER_14_FONT_ID, question_));
      box.y += (box.height - textH) / 2;
      box.height = textH;
      tools::WrapOptions opt = measure;
      opt.draw = true;
      tools::drawWrappedText(renderer, box, question_, opt);
      renderer.drawCenteredText(SMALL_FONT_ID, content.y + content.height - footerH + 4,
                                item_ == todayItem_ ? tr(STR_TOOLS_TODAYS_CARD) : tr(STR_TOOLS_KN_HOLD_TODAY));
      tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_ANSWER), tr(STR_TOOLS_PREV_QUESTION),
                       tr(STR_TOOLS_NEXT_QUESTION));
    } else {
      // Question as a short heading, then the paged answer.
      const int qLineH = tools::wrapLineHeight(renderer, UI_10_FONT_ID, question_);
      Rect qBox{content.x, content.y, content.width, qLineH * 2};
      tools::WrapOptions qOpt;
      qOpt.fontId = UI_10_FONT_ID;
      qOpt.maxLines = 2;
      const int qLines = std::min(2, tools::drawWrappedText(renderer, qBox, question_, qOpt));
      int y = content.y + qLines * qLineH + 6;
      renderer.fillRect(content.x, y, content.width, 2);
      y += 10;
      const Rect body{content.x, y, content.width, content.y + content.height - footerH - y};
      const int lineH = tools::wrapLineHeight(renderer, UI_12_FONT_ID, answer_);
      const int linesPerPage = std::max(1, body.height / lineH);
      tools::WrapOptions measure;
      measure.fontId = UI_12_FONT_ID;
      measure.markdown = true;
      measure.draw = false;
      const int totalLines = tools::drawWrappedText(renderer, body, answer_, measure);
      pageCount_ = std::max(1, (totalLines + linesPerPage - 1) / linesPerPage);
      page_ = std::min(page_, pageCount_ - 1);
      tools::WrapOptions opt = measure;
      opt.draw = true;
      opt.skipLines = page_ * linesPerPage;
      opt.maxLines = linesPerPage;
      tools::drawWrappedText(renderer, body, answer_, opt);

      // Page indicator: text plus dots, always visible.
      char pageLabel[32];
      snprintf(pageLabel, sizeof(pageLabel), "%s %d / %d", tr(STR_TOOLS_PAGE), page_ + 1, pageCount_);
      const int fy = content.y + content.height - footerH + 4;
      renderer.drawText(SMALL_FONT_ID, content.x, fy, pageLabel);
      const int dots = std::min(pageCount_, 12);
      const int dotX = content.x + content.width - dots * 12;
      for (int i = 0; i < dots; ++i) {
        const int dx = dotX + i * 12;
        if (i == page_) {
          renderer.fillRect(dx, fy + 4, 8, 8);
        } else {
          renderer.drawRect(dx, fy + 4, 8, 8);
        }
      }
      tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_QUESTION_LABEL), tr(STR_TOOLS_PREV_PAGE),
                       tr(STR_TOOLS_NEXT_PAGE));
    }
  }
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
