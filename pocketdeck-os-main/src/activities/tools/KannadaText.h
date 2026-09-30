#pragma once

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
