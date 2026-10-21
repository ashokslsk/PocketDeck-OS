#pragma once

#include <EpdFontFamily.h>

#include <cstddef>
#include <cstdint>

#include "KannadaFont.h"

class GfxRenderer;

// Draws text that may mix Kannada and Latin: Kannada (and the digits, spaces
// and punctuation around it) comes from the SD card font, Latin letters from
// the UI font, both on one line box. Used by Panchanga, Mantras, and any
// Kannada text in quotes, flashcards or knowledge cards.
//
// acquire() opens the shared font (about 20 KB of heap) and release() frees
// it when the last user leaves; screens call them in onEnter()/onExit().
// Everything else is safe to call without the font: text is then drawn with
// the UI font only, so callers should check ready() and prefer English.
// Only the render task should draw or measure (the caches are not locked).
namespace kannada {

bool acquire();
void release();
bool ready();
// A book being read: a larger shaped-syllable cache (fewer SD reads while
// laying out long chapters); false returns to the Tools size.
int setBookCache(bool book);
// Gives back the Kannada caches' memory (syllables to 128, glyph cache to its
// smallest) when a reader runs short; text still shapes, only more slowly.
void trimForLowMemory();
void logCacheStats(const char* where);
// Opens the font again if a holder's earlier acquire() could not (no-op when open).
bool retry();
// True if the font file is on the card (checked without opening it).
bool installed();

int lineHeight(const GfxRenderer& r, Style s);
// Line box top to baseline; top = latinY + latinAscender - ascent() puts a
// Kannada run on the same baseline as UI-font text drawn at latinY.
int ascent(const GfxRenderer& r, Style s);
int width(const GfxRenderer& r, const char* text, Style s, size_t len = SIZE_MAX);
// Draws with the line box top at `top`; returns the width drawn.
int draw(const GfxRenderer& r, int x, int top, const char* text, Style s, bool black = true, size_t len = SIZE_MAX);
bool hasKannada(const char* text, size_t len = SIZE_MAX);

// Tools screens (Mantras, Workout, quotes, flashcards) draw Kannada one size
// up from the base styles: the 19 px label was hard to read next to Latin.
// The 28 px size needs a newer kannada.knf; older files fall back to Body.
constexpr Style kToolSmall = Style::Body;   // was Label (19 px) -> 24 px
constexpr Style kToolBold = Style::Title;   // was Value (21 px bold) -> 26 px bold
constexpr Style kToolText = Style::Read28;  // was Body (24 px) -> 28 px
// One line of mixed text where Latin runs use a given font and style (the
// EPUB reader's font) instead of the UI font; draw=false only measures.
// Latin shares the Kannada baseline at top + ascent(s). Returns the width.
int layoutMixed(const GfxRenderer& r, int x, int top, const char* text, Style s, bool black, int latinFontId,
                EpdFontFamily::Style latinSt, bool draw);

// Splits text into lines no wider than maxWidth, breaking at spaces (or
// anywhere, for a word longer than the line) and at '\n'. Writes up to
// maxLines {start, length} pairs and returns the number of lines the whole
// text needs (which may be more than maxLines).
struct Line {
  uint16_t start;
  uint16_t length;
};
size_t wrap(const GfxRenderer& r, const char* text, Style s, int maxWidth, Line* lines, size_t maxLines);

}  // namespace kannada
