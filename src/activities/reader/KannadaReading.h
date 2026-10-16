#pragma once

#include <cstddef>
#include <cstdint>

class GfxRenderer;

// Kannada in EPUB and TXT books. The built-in font engine has no shaper, so
// while a Kannada book is open, words containing Kannada are measured and
// drawn by the Tools Kannada engine (KannadaText, SD card font
// /tools/fonts/kannada.knf) through GfxRenderer's ScriptShaper hook; Latin
// letters inside those words still use the reader font. The reader font also
// gets a taller line and lower baseline so vowel signs above and conjuncts
// below the letters do not collide between lines.
//
// The font (about 20 KB of heap) is opened only for a Kannada book: one whose
// language is "kn"/"kan", whose title is in Kannada, or (TXT) whose opening
// text contains Kannada. Other books are untouched. Without the font file the
// book shows as before.
namespace kannadareading {

// Once at boot: registers the shaper (inactive until a Kannada book opens).
void install(GfxRenderer& renderer);

// When a book opens / closes. `sample` is optional opening text (TXT).
bool beginBook(GfxRenderer& renderer, const char* language, const char* title, const char* sample = nullptr,
               size_t sampleLen = 0);
void endBook(GfxRenderer& renderer);

// Applies the line-height / baseline floor for the reader font about to be
// laid out or drawn (call whenever the reader font id may have changed).
void useReaderFont(int fontId);

// Nonzero while Kannada shaping is on; part of the section cache key so pages
// laid out with and without it are never mixed.
uint8_t layoutFlag();

// Book-list screens (Home, Recent Books, Files) call these in onEnter/onExit
// so Kannada titles are shaped there too; the font opens only when a Kannada
// title is actually drawn and closes when the last such screen closes.
void beginUi();
void endUi();

// Definitions of an English-Kannada dictionary (.ifo lang containing "kn", or
// "Kannada" in its name). beginDictionary() opens the font and returns false
// if it could not; dictionaryFont() applies the line-height floor for the
// definition font (call again whenever it changes); endDictionary() releases
// the font and puts back the reader font's floor of an open Kannada book.
bool beginDictionary();
void dictionaryFont(int fontId);
void endDictionary(GfxRenderer& renderer);

// The reader is short of heap for layout: shrink the Kannada caches.
void releaseForLowMemory();

}  // namespace kannadareading
