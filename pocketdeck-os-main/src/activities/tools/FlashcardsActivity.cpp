#include "FlashcardsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <strings.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "KannadaText.h"
#include "SrsState.h"
#include "ToolStatsPages.h"
#include "ToolsLog.h"
#include "components/UITheme.h"
#include "fontIds.h"

constexpr char FlashcardsActivity::kDeckDir[];
constexpr char FlashcardsActivity::kStatePath[];

namespace {
using Tok = tools::JsonReader::Token;

constexpr uint32_t kIndexMagic = 0x31584946;  // "FIX1"

struct IndexHeader {
  uint32_t magic;
  uint32_t sourceSize;
  uint32_t count;
};

struct IndexEntry {
  uint32_t offset;
  uint32_t hash;
};

int32_t dayBase() { return tools::daysFromCivil(2020, 1, 1); }

uint16_t encodeDay(const int32_t day) { return static_cast<uint16_t>(day - dayBase() + 1); }
int32_t decodeDay(const uint16_t stored) { return dayBase() + stored - 1; }

bool hasJsonExtension(const char* name) {
  const size_t n = strlen(name);
  return n > 5 && strcasecmp(name + n - 5, ".json") == 0;
}

// Reads the fields of one card object; the opening '{' was already consumed.
bool readCardObject(tools::JsonReader& r, char* question, const size_t qCap, char* answer, const size_t aCap) {
  char key[16];
  if (question) question[0] = '\0';
  if (answer) answer[0] = '\0';
  while (true) {
    Tok t = r.next(key, sizeof(key));
    if (t == Tok::Comma) continue;
    if (t == Tok::ObjectEnd) return true;
    if (t != Tok::String) return false;
    if (r.next(nullptr, 0) != Tok::Colon) return false;
    const bool isQ = strcmp(key, "question") == 0;
    const bool isA = strcmp(key, "answer") == 0;
    if (isQ) {
      t = r.next(question, question ? qCap : 0);
    } else if (isA) {
      t = r.next(answer, answer ? aCap : 0);
    } else {
      t = r.next(nullptr, 0);
    }
    if (t == Tok::Error || t == Tok::End) return false;
    if (!isQ && !isA && !r.skipValue(t)) return false;
  }
}

bool writeStateEntry(FsFile& out, const char* deck, const uint32_t hash, const int32_t lastDay, const uint16_t interval,
                     bool& first) {
  char date[12];
  tools::formatIsoDate(date, sizeof(date), lastDay);
  char tail[96];
  snprintf(tail, sizeof(tail), ", \"id\": \"%08lx\", \"lastReviewed\": \"%s\", \"interval\": %u}",
           static_cast<unsigned long>(hash), date, interval);
  const bool ok = tools::writeText(out, first ? "\n    {\"deck\": " : ",\n    {\"deck\": ") &&
                  tools::writeJsonString(out, deck) && tools::writeText(out, tail);
  first = false;
  return ok;
}
}  // namespace

void FlashcardsActivity::deckPath(char* buf, const size_t len) const {
  snprintf(buf, len, "%s/%s.json", kDeckDir, decks_[openDeck_]);
}

void FlashcardsActivity::indexPath(char* buf, const size_t len) const {
  snprintf(buf, len, "%s/fc_%s.idx", tools::kToolsCacheDir, decks_[openDeck_]);
}

void FlashcardsActivity::scanDecks() {
  deckCount_ = 0;
  Storage.ensureDirectoryExists(kDeckDir);
  FsFile dir = Storage.open(kDeckDir);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return;
  }
  char name[kDeckNameCap + 8];
  for (FsFile entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    const bool isFile = !entry.isDirectory();
    entry.getName(name, sizeof(name));
    entry.close();
    if (!isFile || name[0] == '.' || !hasJsonExtension(name) || deckCount_ >= kMaxDecks) continue;
    name[strlen(name) - 5] = '\0';
    if (strlen(name) >= kDeckNameCap) continue;
    snprintf(decks_[deckCount_++], kDeckNameCap, "%s", name);
  }
  dir.close();
  for (int i = 1; i < deckCount_; ++i) {
    for (int j = i; j > 0 && strcasecmp(decks_[j - 1], decks_[j]) > 0; --j) {
      char tmp[kDeckNameCap];
      memcpy(tmp, decks_[j], kDeckNameCap);
      memcpy(decks_[j], decks_[j - 1], kDeckNameCap);
      memcpy(decks_[j - 1], tmp, kDeckNameCap);
    }
  }
}

namespace {
struct IndexBuildCtx {
  FsFile* deck;
  char* scratch;
  size_t scratchCap;
  uint32_t sourceSize;
  uint32_t count;
  uint32_t maxCards;
};
}  // namespace

bool FlashcardsActivity::buildIndex(FsFile& out, void* ctx) {
  auto* b = static_cast<IndexBuildCtx*>(ctx);
  IndexHeader header{kIndexMagic, b->sourceSize, 0};
  if (out.write(&header, sizeof(header)) != sizeof(header)) return false;
  tools::JsonReader r(*b->deck);
  if (!tools::findFirstArray(r)) return false;
  while (b->count < b->maxCards) {
    const Tok t = r.next(nullptr, 0);
    if (t == Tok::Comma) continue;
    if (t != Tok::ObjectStart) break;
    const uint32_t offset = r.tokenOffset();
    if (!readCardObject(r, b->scratch, b->scratchCap, nullptr, 0)) return false;
    if (b->scratch[0] == '\0') continue;
    const IndexEntry entry{offset, srs::hashQuestion(b->scratch)};
    if (out.write(&entry, sizeof(entry)) != sizeof(entry)) return false;
    ++b->count;
  }
  header.count = b->count;
  return out.seek(0) && out.write(&header, sizeof(header)) == sizeof(header);
}

bool FlashcardsActivity::ensureIndex() {
  char deck[96];
  char idx[96];
  deckPath(deck, sizeof(deck));
  indexPath(idx, sizeof(idx));

  FsFile src;
  if (!Storage.openFileForRead("FLASH", deck, src)) return false;
  const auto sourceSize = static_cast<uint32_t>(src.fileSize());

  FsFile existing;
  if (Storage.exists(idx) && Storage.openFileForRead("FLASH", idx, existing)) {
    IndexHeader header{};
    const bool valid = existing.read(&header, sizeof(header)) == sizeof(header) && header.magic == kIndexMagic &&
                       header.sourceSize == sourceSize && header.count <= kMaxCards;
    if (valid) {
      cardCount_ = static_cast<int>(header.count);
      for (int i = 0; i < cardCount_; ++i) {
        IndexEntry e{};
        if (existing.read(&e, sizeof(e)) != sizeof(e)) {
          cardCount_ = i;
          break;
        }
        hashes_[i] = e.hash;
      }
      existing.close();
      src.close();
      return cardCount_ > 0;
    }
    existing.close();
  }

  LOG_INF("FLASH", "Indexing deck %s (%u bytes)", deck, sourceSize);
  IndexBuildCtx ctx{&src, question_, sizeof(question_), sourceSize, 0, kMaxCards};
  const bool ok = tools::writeFileAtomic(idx, &FlashcardsActivity::buildIndex, &ctx);
  src.close();
  if (!ok) {
    LOG_ERR("FLASH", "Could not index %s (is it a JSON array of {question, answer}?)", deck);
    return false;
  }
  if (ctx.count >= static_cast<uint32_t>(kMaxCards)) LOG_INF("FLASH", "Deck capped at %d cards", kMaxCards);
  // Re-read hashes from the fresh index.
  FsFile fresh;
  if (!Storage.openFileForRead("FLASH", idx, fresh)) return false;
  fresh.seek(sizeof(IndexHeader));
  cardCount_ = static_cast<int>(ctx.count);
  for (int i = 0; i < cardCount_; ++i) {
    IndexEntry e{};
    if (fresh.read(&e, sizeof(e)) != sizeof(e)) {
      cardCount_ = i;
      break;
    }
    hashes_[i] = e.hash;
  }
  fresh.close();
  return cardCount_ > 0;
}

void FlashcardsActivity::loadState() {
  memset(lastReviewed_, 0, sizeof(lastReviewed_));
  memset(interval_, 0, sizeof(interval_));
  tools::recoverFromBackup(kStatePath);
  FsFile f;
  if (!Storage.exists(kStatePath) || !Storage.openFileForRead("FLASH", kStatePath, f)) return;
  tools::JsonReader r(f);
  if (!tools::findFirstArray(r)) {
    f.close();
    return;
  }
  srs::StateEntry e;
  while (true) {
    const Tok t = r.next(nullptr, 0);
    if (t == Tok::Comma) continue;
    if (t != Tok::ObjectStart || !srs::readStateObject(r, e)) break;
    if (strcmp(e.deck, decks_[openDeck_]) != 0 || e.lastDay <= dayBase()) continue;
    for (int i = 0; i < cardCount_; ++i) {
      if (hashes_[i] == e.hash) {
        lastReviewed_[i] = encodeDay(e.lastDay);
        interval_[i] = e.interval;
        break;
      }
    }
  }
  f.close();
}

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool FlashcardsActivity::writeState(FsFile& out, void* ctx) {
  const auto* self = static_cast<const FlashcardsActivity*>(ctx);
  const char* deckName = self->decks_[self->openDeck_];
  if (!tools::writeText(out, "{\n  \"version\": 1,\n  \"cards\": [")) return false;
  bool first = true;

  // Copy every other deck's entries through unchanged, one object at a time.
  FsFile old;
  if (Storage.exists(kStatePath) && Storage.openFileForRead("FLASH", kStatePath, old)) {
    tools::JsonReader r(old);
    if (tools::findFirstArray(r)) {
      srs::StateEntry e;
      while (true) {
        const Tok t = r.next(nullptr, 0);
        if (t == Tok::Comma) continue;
        if (t != Tok::ObjectStart || !srs::readStateObject(r, e)) break;
        if (strcmp(e.deck, deckName) == 0 || e.deck[0] == '\0') continue;
        if (!writeStateEntry(out, e.deck, e.hash, e.lastDay, e.interval, first)) {
          old.close();
          return false;
        }
      }
    }
    old.close();
  }

  for (int i = 0; i < self->cardCount_; ++i) {
    if (self->lastReviewed_[i] == 0) continue;
    if (!writeStateEntry(out, deckName, self->hashes_[i], decodeDay(self->lastReviewed_[i]), self->interval_[i],
                         first)) {
      return false;
    }
  }
  return tools::writeText(out, "\n  ]\n}\n");
}

void FlashcardsActivity::saveState() {
  if (!dirty_ || openDeck_ < 0) return;
  if (tools::writeFileAtomic(kStatePath, &FlashcardsActivity::writeState, this)) {
    dirty_ = false;
    gradesSinceSave_ = 0;
  } else {
    LOG_ERR("FLASH", "Failed to save %s", kStatePath);
  }
}

bool FlashcardsActivity::loadCard(const int card) {
  question_[0] = answer_[0] = '\0';
  char idx[96];
  char deck[96];
  indexPath(idx, sizeof(idx));
  deckPath(deck, sizeof(deck));
  IndexEntry e{};
  FsFile f;
  if (!Storage.openFileForRead("FLASH", idx, f)) return false;
  const bool ok = f.seek(sizeof(IndexHeader) + static_cast<size_t>(card) * sizeof(IndexEntry)) &&
                  f.read(&e, sizeof(e)) == sizeof(e);
  f.close();
  if (!ok || !Storage.openFileForRead("FLASH", deck, f)) return false;
  tools::JsonReader r(f);
  const bool parsed = r.seek(e.offset) && r.next(nullptr, 0) == Tok::ObjectStart &&
                      readCardObject(r, question_, sizeof(question_), answer_, sizeof(answer_));
  f.close();
  if (!parsed) LOG_ERR("FLASH", "Failed to read card %d from %s", card, deck);
  return parsed;
}

bool FlashcardsActivity::openDeck(const int deck) {
  openDeck_ = deck;
  cardCount_ = 0;
  reviewed_ = remembered_ = forgot_ = 0;
  queueHead_ = queueSize_ = 0;
  dirty_ = false;
  if (!ensureIndex()) {
    screen_ = Screen::Error;
    return false;
  }
  loadState();
  for (int i = 0; i < cardCount_; ++i) {
    const bool reviewed = lastReviewed_[i] != 0;
    if (srs::isDue(today_, reviewed, reviewed ? decodeDay(lastReviewed_[i]) : 0, interval_[i])) {
      queue_[queueSize_++] = static_cast<uint16_t>(i);
    }
  }
  showNext();
  return true;
}

void FlashcardsActivity::showNext() {
  if (queueSize_ == 0) {
    currentCard_ = -1;
    screen_ = Screen::Done;
    saveState();
    return;
  }
  currentCard_ = queue_[queueHead_];
  queueHead_ = (queueHead_ + 1) % kMaxCards;
  --queueSize_;
  screen_ = loadCard(currentCard_) ? Screen::Question : Screen::Error;
}

void FlashcardsActivity::grade(const bool wasRemembered) {
  if (currentCard_ < 0) return;
  ++reviewed_;
  if (wasRemembered) {
    ++remembered_;
  } else {
    ++forgot_;
    // Forgotten cards come back at the end of this session.
    queue_[(queueHead_ + queueSize_) % kMaxCards] = static_cast<uint16_t>(currentCard_);
    ++queueSize_;
  }
  interval_[currentCard_] = srs::nextInterval(interval_[currentCard_], wasRemembered);
  lastReviewed_[currentCard_] = encodeDay(today_);
  dirty_ = true;
  if (++gradesSinceSave_ >= kSaveEveryGrades) saveState();
  showNext();
}

void FlashcardsActivity::logSession() {
  // History for Stats & export: one line per study session with grades.
  if (openDeck_ < 0 || reviewed_ == 0) return;
  tlog::Stamp at;
  if (tlog::now(at)) {
    char fields[kDeckNameCap + 32];
    snprintf(fields, sizeof(fields), "%s|%d|%d|%d", decks_[openDeck_], reviewed_, remembered_, forgot_);
    tlog::append("flashcards", at, fields);
  }
  reviewed_ = remembered_ = forgot_ = 0;
}

void FlashcardsActivity::closeDeck() {
  logSession();
  saveState();
  openDeck_ = -1;
  screen_ = Screen::Decks;
}

void FlashcardsActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  kannada::acquire();  // Kannada text in the user's decks, when the font is on the card
  tools::ensureToolsDirs();
  tools::DateTime local;
  today_ = tools::getLocalNow(local) ? tools::daysOf(local) : dayBase() + 1;
  scanDecks();
  screen_ = Screen::Decks;
  transitionPending_ = true;
  requestUpdate();
}

void FlashcardsActivity::onExit() {
  logSession();
  saveState();
  kannada::release();
  Activity::onExit();
}

void FlashcardsActivity::openStats() {
  auto stats = makeUniqueNoThrow<ToolStatsActivity>(renderer, mappedInput, tr(STR_TOOLS_FC_STATS),
                                                    &toolstats::buildFlashcards, nullptr, statsx::Feature::Flashcards);
  if (!stats) return;
  startActivityForResult(std::move(stats), [this](const ActivityResult&) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    requestUpdate();
  });
}

void FlashcardsActivity::loop() {
  input_.poll(mappedInput);
  if (input_.backLong) {
    logSession();
    saveState();
    tools::exitToHome();
    return;
  }

  switch (screen_) {
    case Screen::Decks:
      if (input_.back) {
        finish();
        return;
      }
      if (input_.leftUp) {
        openStats();
        return;
      }
      if (deckCount_ == 0) return;
      if (input_.up || input_.pageBack) {
        selectedDeck_ = (selectedDeck_ + deckCount_ - 1) % deckCount_;
        requestUpdate();
      } else if (input_.next()) {
        selectedDeck_ = (selectedDeck_ + 1) % deckCount_;
        requestUpdate();
      } else if (input_.confirm) {
        {
          RenderLock lock(*this);  // card text and screen are read by render()
          openDeck(selectedDeck_);
        }
        requestUpdate();
      }
      break;
    case Screen::Question:
      if (input_.back) {
        {
          RenderLock lock(*this);
          closeDeck();
        }
        requestUpdate();
      } else if (input_.confirm) {  // only the labelled button reveals the answer
        screen_ = Screen::Answer;
        requestUpdate();
      }
      break;
    case Screen::Answer:
      if (input_.back) {
        {
          RenderLock lock(*this);
          closeDeck();
        }
        requestUpdate();
      } else if (input_.confirm) {  // Got it
        {
          RenderLock lock(*this);
          grade(true);
        }
        requestUpdate();
      } else if (input_.left) {  // Forgot (side buttons never grade a card)
        {
          RenderLock lock(*this);
          grade(false);
        }
        requestUpdate();
      }
      break;
    case Screen::Done:
      if (input_.confirm) {
        {
          RenderLock lock(*this);
          closeDeck();
        }
        openStats();
        return;
      }
      [[fallthrough]];
    case Screen::Error:
      if (input_.back || input_.confirm) {
        {
          RenderLock lock(*this);
          closeDeck();
        }
        requestUpdate();
      }
      break;
  }
}

void FlashcardsActivity::render(RenderLock&&) {
  if (screen_ == Screen::Decks) {
    const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_FLASHCARDS));
    if (deckCount_ == 0) {
      const int midY = content.y + content.height / 2;
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_TOOLS_NO_DECKS), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, tr(STR_TOOLS_NO_DECKS_HINT));
      tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
    } else {
      GUI.drawList(renderer, Rect{0, content.y, renderer.getScreenWidth(), content.height}, deckCount_, selectedDeck_,
                   [this](const int i) { return std::string(decks_[i]); });
      tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_STUDY), tr(STR_TOOLS_STATS_SHORT),
                       tr(STR_TOOLS_NEXT));
    }
  } else {
    const char* deckName = openDeck_ >= 0 ? decks_[openDeck_] : "";
    char subtitle[48];
    snprintf(subtitle, sizeof(subtitle), "%s %d", tr(STR_TOOLS_LEFT_IN_SESSION),
             queueSize_ + (screen_ == Screen::Question || screen_ == Screen::Answer ? 1 : 0));
    const Rect content = tools::drawFrame(renderer, deckName, subtitle);

    if (screen_ == Screen::Question) {
      Rect box{content.x + 10, content.y, content.width - 20, content.height};
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
      tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_SHOW_ANSWER), "", "");
    } else if (screen_ == Screen::Answer) {
      // Question kept small at the top for context, answer below the rule.
      const int qLineH = tools::wrapLineHeight(renderer, UI_10_FONT_ID, question_);
      Rect qBox{content.x, content.y, content.width, qLineH * 3};
      tools::WrapOptions qOpt;
      qOpt.fontId = UI_10_FONT_ID;
      qOpt.maxLines = 3;
      const int qLines = std::min(3, tools::drawWrappedText(renderer, qBox, question_, qOpt));
      int y = content.y + qLines * qLineH + 8;
      renderer.fillRect(content.x, y, content.width, 2);
      y += 12;
      Rect aBox{content.x, y, content.width, content.y + content.height - y};
      tools::WrapOptions aOpt;
      aOpt.fontId = UI_12_FONT_ID;
      aOpt.markdown = true;
      tools::drawWrappedText(renderer, aBox, answer_, aOpt);
      tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_GOT_IT), tr(STR_TOOLS_FORGOT), "");
    } else if (screen_ == Screen::Done) {
      const int midY = content.y + content.height / 2;
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 40, tr(STR_TOOLS_SESSION_DONE), true, EpdFontFamily::BOLD);
      char line[96];
      snprintf(line, sizeof(line), "%s %d   %s %d   %s %d", tr(STR_TOOLS_REVIEWED), reviewed_, tr(STR_TOOLS_REMEMBERED),
               remembered_, tr(STR_TOOLS_FORGOT), forgot_);
      renderer.drawCenteredText(UI_10_FONT_ID, midY, line);
      if (reviewed_ == 0) renderer.drawCenteredText(UI_10_FONT_ID, midY + 30, tr(STR_TOOLS_NOTHING_DUE));
      tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_STATS_SHORT), "", "");
    } else {
      const int midY = content.y + content.height / 2;
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_TOOLS_DECK_ERROR), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, tr(STR_TOOLS_DECK_ERROR_HINT));
      tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
    }
  }

  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
