#pragma once

#include <string>
#include <vector>

#include "SavedItemsHomeActivity.h"
#include "activities/Activity.h"
#include "activities/tools/ToolsCommon.h"

// Bookmarks and clippings as cards, one per screen: the book, the chapter, the
// clipped text or bookmark snippet. Left/Right (and the side buttons) page
// through; Confirm opens the book at that place; Back returns.
// Used for one book's saved items and for Resurface Trove, a random handful
// from every book to rediscover what you once marked.
class SavedCardsActivity final : public Activity {
 public:
  struct CardRef {
    uint16_t book;   // index into the books list
    bool clipping;   // false: bookmark
    uint16_t index;  // into that book's bookmarks or clippings
  };

  SavedCardsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::vector<SavedBookEntry> books,
                     std::vector<CardRef> cards, bool resurface)
      : Activity("SavedCards", renderer, mappedInput),
        books_(std::move(books)),
        cards_(std::move(cards)),
        resurface_(resurface) {}

  // Cards for one book: bookmarks then clippings, in reading order.
  static std::vector<CardRef> cardsForBook(const std::vector<SavedBookEntry>& books, uint16_t book);
  // Up to `count` random cards drawn from every book's saved items.
  static std::vector<CardRef> resurfaceCards(const std::vector<SavedBookEntry>& books, size_t count);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // The shown card, read from its book's store when the card changes.
  struct Shown {
    std::string kind;     // "Bookmark" / "Clipping"
    std::string chapter;  // chapter title, may be empty
    std::string text;     // clipping text or bookmark snippet
    // Where Confirm jumps.
    uint16_t spine = 0;
    float progress = 0.0f;
    uint16_t paragraph = UINT16_MAX;
    uint16_t clippingIndex = UINT16_MAX;
    bool valid = false;
  };
  void loadCard();
  void openCard();

  std::vector<SavedBookEntry> books_;
  std::vector<CardRef> cards_;
  bool resurface_ = false;
  int current_ = 0;
  Shown shown_;
  tools::ToolInput input_;
};
