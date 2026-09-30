#pragma once

#include <HalStorage.h>

#include <cstddef>
#include <cstdint>
#include <memory>

// Kannada text from the SD card font /tools/fonts/kannada.knf (built by
// scripts/kannada/build_kannada_font.py from Noto Sans Kannada).
//
// The firmware has no OpenType shaper. Instead, every Kannada syllable of up
// to three consonants was shaped with HarfBuzz when the font file was built
// and stored in a fixed-size slot, so shaping here is: split the text into
// syllables, compute each syllable's slot number from its letters, and read
// that slot (one 20-byte read, cached). Reph, anusvara/visarga and pair
// kerning between syllables are applied from small tables kept in RAM.
// Nothing here is stored in firmware flash; without the file on the card,
// callers fall back to English.
namespace kannada {

enum class Style : uint8_t { Label = 0, Value = 1, Title = 2, Body = 3 };
constexpr int kStyleCount = 4;
constexpr char kFontPath[] = "/tools/fonts/kannada.knf";

// Decodes the UTF-8 codepoint at s[i]; returns its byte length (0 at the end).
size_t decodeUtf8(const char* s, size_t len, size_t i, uint32_t& cp);

struct Glyph {
  uint16_t id = 0;
  int16_t advance = 0;  // font units, adjusted for the style's weight and kerning
};

struct GlyphBitmap {
  uint8_t w = 0;
  uint8_t h = 0;
  int8_t left = 0;                // pixels from the pen position to the bitmap's left edge
  int8_t top = 0;                 // pixels from the baseline up to the bitmap's top row
  const uint8_t* bits = nullptr;  // rows padded to whole bytes, MSB first, 1 = ink
};

class Font {
 public:
  Font() = default;
  Font(const Font&) = delete;
  Font& operator=(const Font&) = delete;
  ~Font() { close(); }

  // Opens the font and loads its small tables and caches (about 20 KB of
  // heap). Returns false, logging why, if the file is missing or invalid.
  bool open(const char* path = kFontPath);
  void close();
  bool isOpen() const { return open_; }

  int pixelSize(Style s) const { return styles_[static_cast<int>(s)].px; }
  int ascent(Style s) const { return styles_[static_cast<int>(s)].ascent; }
  int lineHeight(Style s) const { return styles_[static_cast<int>(s)].height; }
  uint16_t unitsPerEm() const { return unitsPerEm_; }

  // True if this font draws the codepoint (the Kannada block, dandas, digits,
  // spaces and common punctuation). Latin letters are drawn by the UI font.
  bool covers(uint32_t cp) const;

  // Shapes the run of covered text at the start of text[0..len) into out.
  // Stops at the first codepoint the font does not cover, or when out is
  // nearly full (always at a syllable boundary). Returns the number of
  // glyphs; *consumed is the number of bytes used (0 if text starts with an
  // uncovered codepoint).
  size_t shape(const char* text, size_t len, Style style, Glyph* out, size_t cap, size_t* consumed);

  // Bitmap for a glyph; valid until the next call. Returns false if the
  // glyph could not be read (an empty bitmap is still true).
  bool bitmap(Style style, uint16_t glyph, GlyphBitmap& out);

 private:
  struct StyleInfo {
    uint8_t px = 0, bold = 0, ascent = 0, height = 0;
    uint32_t metricsOffset = 0, bitmapsOffset = 0;
  };
  struct CmapEntry {
    uint32_t cp;
    uint16_t glyph;
  };
  struct KernEntry {
    uint32_t pair;  // left << 16 | right
    int16_t delta;
  };
  struct SyllableSlot {
    uint32_t key = 0;  // slot index + 1; 0 = empty
    uint8_t data[20] = {};
  };
  struct GlyphEntry {
    uint16_t key = 0xFFFF;  // style << 10 | glyph
    uint8_t w = 0, h = 0;
    int8_t left = 0, top = 0;
    uint16_t arenaOffset = 0;
  };
  static constexpr int kSyllableCache = 128;
  static constexpr int kGlyphBuckets = 64;
  // Glyph bitmap cache: 10 KB when the heap allows, less on a fragmented heap
  // (glyphs are then re-read from the card more often, nothing else changes).
  static constexpr size_t kArenaSizes[] = {10 * 1024, 6 * 1024, 3 * 1024};

  int glyphFor(uint32_t cp) const;
  int16_t kerning(uint16_t left, uint16_t right) const;
  int16_t styleAdvance(Style style, uint16_t glyph, int16_t regularAdvance) const;
  const uint8_t* syllable(uint32_t slot);
  bool readAt(uint32_t offset, void* dst, size_t n);

  HalFile file_;
  bool open_ = false;
  uint16_t glyphCount_ = 0;
  uint16_t unitsPerEm_ = 1000;
  uint16_t rephGlyph_ = 0;
  uint8_t consonants_ = 0, slotsPer_ = 0, matras_ = 0, slotSize_ = 0;
  uint32_t dictOffset_ = 0, dictSlots_ = 0;
  StyleInfo styles_[kStyleCount];
  std::unique_ptr<CmapEntry[]> cmap_;
  uint16_t cmapCount_ = 0;
  std::unique_ptr<KernEntry[]> kern_;
  uint16_t kernCount_ = 0;
  std::unique_ptr<uint16_t[]> advances_;  // glyphCount x {regular, bold}
  std::unique_ptr<SyllableSlot[]> syllables_;
  std::unique_ptr<GlyphEntry[]> glyphs_;
  std::unique_ptr<uint8_t[]> arena_;
  size_t arenaUsed_ = 0;
  size_t arenaBytes_ = 0;
};

}  // namespace kannada
