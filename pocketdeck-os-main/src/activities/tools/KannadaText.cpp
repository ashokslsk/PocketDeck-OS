#include "KannadaText.h"

#include <GfxRenderer.h>
#include <HalStorage.h>

#include <cstring>

#include "fontIds.h"

namespace kannada {

namespace {

// One shared font: several screens (Panchanga, its calendar, Mantras) can
// hold it at once, and it is freed when the last one leaves.
Font gFont;
int gRefs = 0;

int latinFont(const Style s) { return s == Style::Label ? UI_10_FONT_ID : UI_12_FONT_ID; }
EpdFontFamily::Style latinStyle(const Style s) {
  return s == Style::Value || s == Style::Title ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
}

bool isMark(const uint32_t cp) { return (cp >= 0x0CBC && cp <= 0x0CD6) || cp == 0x0C82 || cp == 0x0C83; }

// Draws (or only measures) one line; returns its width in pixels.
int layout(const GfxRenderer& r, const int x, const int top, const char* text, size_t len, const Style s,
           const bool black, const bool draw) {
  if (text == nullptr) return 0;
  if (len == SIZE_MAX) len = strlen(text);
  const int lineH = lineHeight(r, s);
  int px = x;
  size_t i = 0;
  Glyph glyphs[40];
  while (i < len) {
    // A Kannada run starts at a Kannada-script character; digits, spaces and
    // punctuation then continue it, but never start one (so a comma inside
    // English stays in the Latin font).
    uint32_t first = 0;
    decodeUtf8(text, len, i, first);
    if (gFont.isOpen() && first >= 0x80 && gFont.covers(first)) {
      size_t used = 0;
      const size_t n = gFont.shape(text + i, len - i, s, glyphs, sizeof(glyphs) / sizeof(glyphs[0]), &used);
      if (used > 0) {
        const int32_t size = gFont.pixelSize(s);
        const int32_t upem = gFont.unitsPerEm();
        const int asc = gFont.ascent(s);
        int32_t pen = 0;
        for (size_t k = 0; k < n; ++k) {
          GlyphBitmap b;
          if (draw && gFont.bitmap(s, glyphs[k].id, b) && b.w > 0) {
            const int gx = px + static_cast<int>((pen * size + upem / 2) / upem) + b.left;
            const int gy = top + asc - b.top;
            const int stride = (b.w + 7) / 8;
            for (int row = 0; row < b.h; ++row) {
              const uint8_t* bits = b.bits + row * stride;
              for (int col = 0; col < b.w; ++col) {
                if (bits[col >> 3] & (0x80 >> (col & 7))) r.drawPixel(gx + col, gy + row, black);
              }
            }
          }
          pen += glyphs[k].advance;
        }
        px += static_cast<int>((pen * size + upem / 2) / upem);
        i += used;
        continue;
      }
    }
    // A run the Kannada font does not draw: Latin letters (or everything,
    // without the font). Newlines are laid out as spaces; wrap() splits lines.
    char buf[72];
    size_t bl = 0;
    while (i < len && bl + 5 < sizeof(buf)) {
      uint32_t cp = 0;
      const size_t n = decodeUtf8(text, len, i, cp);
      if (n == 0) break;
      if (gFont.isOpen() && cp >= 0x80 && gFont.covers(cp)) break;
      if (cp == '\n' || cp == '\r' || cp == '\t') {
        buf[bl++] = ' ';
      } else {
        memcpy(buf + bl, text + i, n);
        bl += n;
      }
      i += n;
    }
    buf[bl] = '\0';
    if (bl > 0) {
      const int f = latinFont(s);
      const auto st = latinStyle(s);
      // With the Kannada font, Latin shares its baseline; without it, centre in the line box.
      const int ly = gFont.isOpen() ? top + gFont.ascent(s) - r.getFontAscenderSize(f)
                                    : top + (lineH - r.getLineHeight(f)) / 2 + 1;
      if (draw) r.drawText(f, px, ly, buf, black, st);
      px += r.getTextWidth(f, buf, st);
    }
  }
  return px - x;
}

}  // namespace

bool acquire() {
  if (gRefs > 0) {
    ++gRefs;
    // An earlier open may have failed (card busy, fragmented heap): try again.
    return gFont.isOpen() || gFont.open(kFontPath);
  }
  gRefs = 1;
  return gFont.open(kFontPath);
}

void release() {
  if (gRefs == 0) return;
  if (--gRefs == 0) gFont.close();
}

bool ready() { return gFont.isOpen(); }

bool retry() { return gRefs > 0 && (gFont.isOpen() || gFont.open(kFontPath)); }

bool installed() { return Storage.exists(kFontPath); }

int ascent(const GfxRenderer& r, const Style s) {
  return gFont.isOpen() ? gFont.ascent(s) : r.getFontAscenderSize(latinFont(s)) + 2;
}

int lineHeight(const GfxRenderer& r, const Style s) {
  return gFont.isOpen() ? gFont.lineHeight(s) : r.getLineHeight(latinFont(s)) + 4;
}

int width(const GfxRenderer& r, const char* text, const Style s, const size_t len) {
  return layout(r, 0, 0, text, len, s, true, false);
}

int draw(const GfxRenderer& r, const int x, const int top, const char* text, const Style s, const bool black,
         const size_t len) {
  return layout(r, x, top, text, len, s, black, true);
}

bool hasKannada(const char* text, size_t len) {
  if (text == nullptr) return false;
  if (len == SIZE_MAX) len = strlen(text);
  for (size_t i = 0; i + 2 < len; ++i) {
    // Every Kannada-block codepoint (U+0C80..U+0CFF) starts with 0xE0 0xB2/0xB3.
    if (static_cast<uint8_t>(text[i]) == 0xE0 &&
        (static_cast<uint8_t>(text[i + 1]) == 0xB2 || static_cast<uint8_t>(text[i + 1]) == 0xB3)) {
      return true;
    }
  }
  return false;
}

size_t wrap(const GfxRenderer& r, const char* text, const Style s, const int maxWidth, Line* lines,
            const size_t maxLines) {
  if (text == nullptr) return 0;
  const size_t len = strlen(text);
  size_t count = 0;
  size_t pos = 0;
  while (pos < len) {
    while (pos < len && text[pos] == ' ') ++pos;
    if (pos >= len) break;
    // Longest run of whole words that fits.
    size_t best = 0;
    size_t j = pos;
    while (true) {
      size_t next = j;
      while (next < len && text[next] != ' ' && text[next] != '\n') ++next;
      if (layout(r, 0, 0, text + pos, next - pos, s, true, false) > maxWidth) break;
      best = next - pos;
      if (next >= len || text[next] == '\n') break;
      j = next + 1;
    }
    if (best == 0) {
      // One word wider than the line: cut it between syllables.
      size_t k = pos;
      size_t lastGood = 0;
      while (k < len && text[k] != ' ' && text[k] != '\n') {
        uint32_t cp = 0;
        const size_t n = decodeUtf8(text, len, k, cp);
        if (n == 0) break;
        k += n;
        uint32_t next = 0;
        const bool splittable = decodeUtf8(text, len, k, next) == 0 || (!isMark(next) && cp != 0x0CCD);
        if (!splittable) continue;
        if (layout(r, 0, 0, text + pos, k - pos, s, true, false) > maxWidth && lastGood > 0) break;
        lastGood = k - pos;
      }
      best = lastGood > 0 ? lastGood : 1;
    }
    size_t trimmed = best;
    while (trimmed > 0 && text[pos + trimmed - 1] == ' ') --trimmed;
    if (count < maxLines && lines != nullptr) {
      lines[count] = {static_cast<uint16_t>(pos), static_cast<uint16_t>(trimmed)};
    }
    ++count;
    pos += best;
    if (pos < len && text[pos] == '\n') ++pos;
  }
  return count;
}

}  // namespace kannada
