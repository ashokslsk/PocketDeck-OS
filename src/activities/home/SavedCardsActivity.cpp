#include "SavedCardsActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "BookmarkStore.h"
#include "ClippingStore.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "activities/reader/KannadaReading.h"
#include "components/CompactHeader.h"
#include "components/UITheme.h"
#include "fontIds.h"

std::vector<SavedCardsActivity::CardRef> SavedCardsActivity::cardsForBook(const std::vector<SavedBookEntry>& books,
                                                                          const uint16_t book) {
  std::vector<CardRef> cards;
  if (book >= books.size()) return cards;
  const SavedBookEntry& entry = books[book];
  cards.reserve(entry.bookmarkCount + entry.clippingCount);
  for (uint16_t i = 0; i < entry.bookmarkCount; ++i) cards.push_back({book, false, i});
  for (uint16_t i = 0; i < entry.clippingCount; ++i) cards.push_back({book, true, i});
  return cards;
}

std::vector<SavedCardsActivity::CardRef> SavedCardsActivity::resurfaceCards(const std::vector<SavedBookEntry>& books,
                                                                            const size_t count) {
  // Every saved item is one ticket; draw without repeats.
  std::vector<CardRef> all;
  for (uint16_t b = 0; b < books.size(); ++b) {
    for (uint16_t i = 0; i < books[b].bookmarkCount; ++i) all.push_back({b, false, i});
    for (uint16_t i = 0; i < books[b].clippingCount; ++i) all.push_back({b, true, i});
  }
  for (size_t i = 0; i + 1 < all.size() && i < count; ++i) {
    const size_t j = i + esp_random() % (all.size() - i);
    std::swap(all[i], all[j]);
  }
  if (all.size() > count) all.resize(count);
  return all;
}

void SavedCardsActivity::onEnter() {
  Activity::onEnter();
  kannadareading::beginUi();
  input_.reset(mappedInput);
  current_ = 0;
  loadCard();
  requestUpdate();
}

void SavedCardsActivity::onExit() {
  kannadareading::endUi();
  Activity::onExit();
}

void SavedCardsActivity::loadCard() {
  shown_ = Shown{};
  if (current_ < 0 || current_ >= static_cast<int>(cards_.size())) return;
  const CardRef& card = cards_[static_cast<size_t>(current_)];
  if (card.book >= books_.size()) return;
  const SavedBookEntry& book = books_[card.book];
  if (card.clipping) {
    CLIPPINGS.loadForBook(book.bookPath, book.bookTitle, book.bookAuthor, book.bookType);
    const Clipping* clip = CLIPPINGS.clippingAt(card.index);
    if (!clip) {
      LOG_ERR("SVC", "Clipping %u missing in %s", card.index, book.bookPath.c_str());
      return;
    }
    shown_.kind = tr(STR_SAVED_CARD_CLIPPING);
    shown_.chapter = clip->chapterTitle;
    if (!CLIPPINGS.readClippingText(card.index, shown_.text)) CLIPPINGS.readClippingPreview(card.index, shown_.text);
    shown_.spine = clip->spineIndex;
    shown_.progress = clip->pageCount > 0 ? static_cast<float>(clip->startPage) / clip->pageCount : 0.0f;
    shown_.paragraph = clip->paragraphIndex;
    shown_.clippingIndex = card.index;
  } else {
    BOOKMARKS.loadForBook(book.bookPath, book.bookTitle, book.bookAuthor, book.bookType);
    const auto& marks = BOOKMARKS.getBookmarks();
    if (card.index >= marks.size()) {
      LOG_ERR("SVC", "Bookmark %u missing in %s", card.index, book.bookPath.c_str());
      return;
    }
    const Bookmark& bm = marks[card.index];
    shown_.kind = tr(STR_SAVED_CARD_BOOKMARK);
    shown_.chapter = bm.chapterTitle;
    shown_.text = bm.snippet;
    shown_.spine = bm.spineIndex;
    shown_.progress = bm.progress;
    shown_.paragraph = bm.paragraphIndex;
  }
  shown_.valid = true;
}

void SavedCardsActivity::openCard() {
  if (!shown_.valid || current_ >= static_cast<int>(cards_.size())) return;
  const SavedBookEntry& book = books_[cards_[static_cast<size_t>(current_)].book];
  // The same hand-off the bookmark and clipping lists use to open a place.
  APP_STATE.pendingBookmarkSpine = shown_.spine;
  APP_STATE.pendingBookmarkProgress = shown_.progress;
  APP_STATE.pendingBookmarkParagraphIndex = shown_.paragraph;
  APP_STATE.pendingClippingIndex = shown_.clippingIndex;
  APP_STATE.saveToFile();
  onSelectBook(book.bookPath);
}

void SavedCardsActivity::loop() {
  input_.poll(mappedInput);
  if (input_.back) {
    finish();
    return;
  }
  if (input_.confirm) {
    openCard();
    return;
  }
  const int total = static_cast<int>(cards_.size());
  if (total <= 1) return;
  if (input_.next() || input_.prev()) {
    current_ = (current_ + (input_.next() ? 1 : total - 1)) % total;
    loadCard();
    requestUpdate();
  }
}

void SavedCardsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  CompactHeader::drawTitle(renderer, resurface_ ? tr(STR_RESURFACE_TROVE) : tr(STR_BOOKMARKS_AND_CLIPPINGS));
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const int top = CompactHeader::contentTop(metrics) + 8;
  const int bottom = screenH - metrics.buttonHintsHeight - 12;

  if (cards_.empty() || !shown_.valid) {
    renderer.drawCenteredText(UI_10_FONT_ID, (top + bottom) / 2, tr(STR_NO_BOOKMARKS));
  } else {
    const SavedBookEntry& book = books_[cards_[static_cast<size_t>(current_)].book];
    const int cardX = 16;
    const int cardW = screenW - 32;
    const int cardH = bottom - top - 22;
    renderer.drawRoundedRect(cardX, top, cardW, cardH, 2, 14, true);
    const int pad = 18;
    const int x = cardX + pad;
    const int w = cardW - 2 * pad;
    int y = top + pad - 4;

    // Kind pill, then the book.
    const int pillH = renderer.getLineHeight(SMALL_FONT_ID) + 4;
    const int pillW = renderer.getTextWidth(SMALL_FONT_ID, shown_.kind.c_str(), EpdFontFamily::BOLD) + 16;
    renderer.fillRoundedRect(x, y, pillW, pillH, pillH / 2, Color::Black);
    renderer.drawText(SMALL_FONT_ID, x + 8, y + 2, shown_.kind.c_str(), false, EpdFontFamily::BOLD);
    y += pillH + 10;

    tools::WrapOptions titleOpt;
    titleOpt.fontId = UI_12_FONT_ID;
    titleOpt.maxLines = 2;
    const std::string bookTitle = "**" + book.bookTitle + "**";
    titleOpt.markdown = true;
    const int titleLines = tools::drawWrappedText(
        renderer, Rect{x, y, w, renderer.getLineHeight(UI_12_FONT_ID) * 2}, bookTitle.c_str(), titleOpt);
    y += renderer.getLineHeight(UI_12_FONT_ID) * std::max(1, std::min(titleLines, 2));
    if (!book.bookAuthor.empty()) {
      renderer.drawText(UI_10_FONT_ID, x, y, renderer.truncatedText(UI_10_FONT_ID, book.bookAuthor.c_str(), w).c_str());
      y += renderer.getLineHeight(UI_10_FONT_ID);
    }
    if (!shown_.chapter.empty()) {
      y += 4;
      renderer.drawText(SMALL_FONT_ID, x, y, renderer.truncatedText(SMALL_FONT_ID, shown_.chapter.c_str(), w).c_str());
      y += renderer.getLineHeight(SMALL_FONT_ID);
    }
    y += 10;
    renderer.drawLine(x, y, x + w, y);
    y += 14;

    // The saved words, as many lines as the card holds.
    const int textBottom = top + cardH - pad - renderer.getLineHeight(SMALL_FONT_ID) - 8;
    tools::WrapOptions textOpt;
    textOpt.fontId = UI_12_FONT_ID;
    const int lineH = tools::wrapLineHeight(renderer, UI_12_FONT_ID, shown_.text.c_str());
    textOpt.maxLines = std::max(1, (textBottom - y) / std::max(1, lineH));
    const std::string text = shown_.text.empty() ? std::string(tr(STR_SAVED_CARD_NO_TEXT)) : shown_.text;
    tools::drawWrappedText(renderer, Rect{x, y, w, textBottom - y}, text.c_str(), textOpt);

    // Footer: where in the book, and which card.
    char footer[48];
    snprintf(footer, sizeof(footer), "%d / %d", current_ + 1, static_cast<int>(cards_.size()));
    const int footerY = top + cardH - pad - renderer.getLineHeight(SMALL_FONT_ID) + 4;
    renderer.drawText(SMALL_FONT_ID, x + w - renderer.getTextWidth(SMALL_FONT_ID, footer), footerY, footer);
    char where[32];
    snprintf(where, sizeof(where), tr(STR_SAVED_CARD_CHAPTER_FORMAT), static_cast<unsigned>(shown_.spine + 1));
    renderer.drawText(SMALL_FONT_ID, x, footerY, where);
  }

  const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), tr(STR_OPEN),
                                            tr(STR_TOOLS_PREVIOUS), tr(STR_TOOLS_NEXT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
