#include "KannadaFont.h"

#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>

namespace kannada {

namespace {

constexpr uint32_t kVirama = 0x0CCD;
constexpr uint32_t kNukta = 0x0CBC;
constexpr uint32_t kRa = 0x0CB0;
constexpr uint32_t kZwnj = 0x200C;
constexpr uint32_t kZwj = 0x200D;
constexpr int kKsha = 39;  // ಕ್ಷ, one letter in the font
constexpr int kJnya = 40;  // ಜ್ಞ
// Matra order used by the file: 0 = none, 1..13 vowel signs, 14 = virama.
constexpr uint32_t kMatras[] = {0x0CBE, 0x0CBF, 0x0CC0, 0x0CC1, 0x0CC2, 0x0CC3, 0x0CC4,
                                0x0CC6, 0x0CC7, 0x0CC8, 0x0CCA, 0x0CCB, 0x0CCC, kVirama};

uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t le32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

// Decodes one UTF-8 codepoint at s[i]; returns its byte length (0 at the end).
size_t decode(const char* s, const size_t len, const size_t i, uint32_t& cp) {
  if (i >= len) return 0;
  const auto b0 = static_cast<uint8_t>(s[i]);
  if (b0 < 0x80) {
    cp = b0;
    return 1;
  }
  size_t n = 0;
  if ((b0 & 0xE0) == 0xC0) {
    cp = b0 & 0x1F;
    n = 2;
  } else if ((b0 & 0xF0) == 0xE0) {
    cp = b0 & 0x0F;
    n = 3;
  } else if ((b0 & 0xF8) == 0xF0) {
    cp = b0 & 0x07;
    n = 4;
  } else {
    cp = 0xFFFD;
    return 1;
  }
  if (i + n > len) {
    cp = 0xFFFD;
    return len - i;
  }
  for (size_t k = 1; k < n; ++k) {
    const auto b = static_cast<uint8_t>(s[i + k]);
    if ((b & 0xC0) != 0x80) {
      cp = 0xFFFD;
      return k;
    }
    cp = (cp << 6) | (b & 0x3F);
  }
  return n;
}

// Consonant index of a single codepoint (file order), or -1.
int consonantIndex(const uint32_t cp) {
  if (cp >= 0x0C95 && cp <= 0x0CB9 && cp != 0x0CA9 && cp != 0x0CB4) {
    int idx = static_cast<int>(cp - 0x0C95);
    if (cp > 0x0CA9) --idx;
    if (cp > 0x0CB4) --idx;
    return idx;
  }
  if (cp == 0x0CDD) return 35;
  if (cp == 0x0CDE) return 36;
  return -1;
}

// Consonant (or one-letter conjunct) at s[i]: returns its index and byte length.
int consonantAt(const char* s, const size_t len, const size_t i, size_t& bytes) {
  uint32_t cp = 0;
  const size_t n = decode(s, len, i, cp);
  if (n == 0) return -1;
  const int c = consonantIndex(cp);
  if (c < 0) return -1;
  uint32_t next = 0, third = 0;
  const size_t n2 = decode(s, len, i + n, next);
  if (n2 && next == kVirama) {
    const size_t n3 = decode(s, len, i + n + n2, third);
    if (n3 && ((cp == 0x0C95 && third == 0x0CB7) || (cp == 0x0C9C && third == 0x0C9E))) {
      bytes = n + n2 + n3;
      return cp == 0x0C95 ? kKsha : kJnya;
    }
  }
  if (n2 && next == kNukta && (cp == 0x0C9C || cp == 0x0CAB)) {
    bytes = n + n2;
    return cp == 0x0C9C ? 37 : 38;
  }
  bytes = n;
  return c;
}

int matraIndex(const uint32_t cp) {
  for (size_t k = 0; k < sizeof(kMatras) / sizeof(kMatras[0]); ++k) {
    if (kMatras[k] == cp) return static_cast<int>(k) + 1;
  }
  return 0;
}

}  // namespace

size_t decodeUtf8(const char* s, const size_t len, const size_t i, uint32_t& cp) { return decode(s, len, i, cp); }

bool Font::readAt(const uint32_t offset, void* dst, const size_t n) {
  if (!file_.seek(offset)) return false;
  return file_.read(dst, n) == static_cast<int>(n);
}

bool Font::open(const char* path) {
  close();
  if (!Storage.exists(path)) {
    LOG_INF("KNF", "No Kannada font at %s", path);
    return false;
  }
  if (!Storage.openFileForRead("KNF", path, file_)) {
    LOG_ERR("KNF", "Could not open %s", path);
    return false;
  }
  uint8_t h[64];
  if (!readAt(0, h, sizeof(h)) || memcmp(h, "KNF1", 4) != 0 || le16(h + 4) != 1) {
    LOG_ERR("KNF", "%s is not a version 1 Kannada font", path);
    file_.close();
    return false;
  }
  glyphCount_ = le16(h + 6);
  consonants_ = h[8];
  slotsPer_ = h[9];
  matras_ = h[10];
  slotSize_ = h[11];
  dictOffset_ = le32(h + 12);
  dictSlots_ = le32(h + 16);
  const uint32_t cmapOffset = le32(h + 20);
  cmapCount_ = le16(h + 24);
  rephGlyph_ = le16(h + 26);
  const uint32_t advOffset = le32(h + 28);
  const uint8_t styleCount = h[32];
  const uint32_t styleOffset = le32(h + 36);
  unitsPerEm_ = le16(h + 40);
  const uint32_t kernOffset = le32(h + 42);
  kernCount_ = le16(h + 46);
  if (consonants_ != 41 || slotsPer_ != 42 || matras_ != 15 || slotSize_ != sizeof(SyllableSlot::data) ||
      styleCount < kStyleCount || glyphCount_ == 0 || glyphCount_ > 1024 || unitsPerEm_ == 0) {
    LOG_ERR("KNF", "Unsupported Kannada font layout");
    file_.close();
    return false;
  }

  cmap_ = makeUniqueNoThrow<CmapEntry[]>(cmapCount_);
  kern_ = makeUniqueNoThrow<KernEntry[]>(kernCount_ ? kernCount_ : 1);
  advances_ = makeUniqueNoThrow<uint16_t[]>(static_cast<size_t>(glyphCount_) * 2);
  syllables_ = makeUniqueNoThrow<SyllableSlot[]>(kSyllableCache);
  glyphs_ = makeUniqueNoThrow<GlyphEntry[]>(kGlyphBuckets);
  for (const size_t size : kArenaSizes) {
    arena_ = makeUniqueNoThrow<uint8_t[]>(size);
    if (arena_) {
      arenaBytes_ = size;
      break;
    }
  }
  if (!cmap_ || !kern_ || !advances_ || !syllables_ || !glyphs_ || !arena_) {
    LOG_ERR("KNF", "OOM loading Kannada font tables");
    close();
    return false;
  }

  bool ok = true;
  uint8_t rec[16];
  for (uint16_t i = 0; ok && i < cmapCount_; ++i) {
    ok = readAt(cmapOffset + 8u * i, rec, 8);
    cmap_[i] = {le32(rec), le16(rec + 4)};
  }
  for (uint16_t i = 0; ok && i < kernCount_; ++i) {
    ok = readAt(kernOffset + 6u * i, rec, 6);
    kern_[i] = {static_cast<uint32_t>(le16(rec)) << 16 | le16(rec + 2), static_cast<int16_t>(le16(rec + 4))};
  }
  if (ok) ok = readAt(advOffset, advances_.get(), static_cast<size_t>(glyphCount_) * 4);
  for (int s = 0; ok && s < kStyleCount; ++s) {
    ok = readAt(styleOffset + 16u * s, rec, 16);
    styles_[s] = {rec[0], rec[1], rec[2], rec[3], le32(rec + 4), le32(rec + 8)};
  }
  if (!ok) {
    LOG_ERR("KNF", "Truncated Kannada font %s", path);
    close();
    return false;
  }
  open_ = true;
  arenaUsed_ = 0;
  LOG_INF("KNF", "Kannada font ready: %u glyphs, %u kerning pairs", glyphCount_, kernCount_);
  return true;
}

void Font::close() {
  if (file_) file_.close();
  open_ = false;
  cmap_.reset();
  kern_.reset();
  advances_.reset();
  syllables_.reset();
  glyphs_.reset();
  arena_.reset();
  cmapCount_ = kernCount_ = 0;
  arenaUsed_ = 0;
}

int Font::glyphFor(const uint32_t cp) const {
  size_t lo = 0, hi = cmapCount_;
  while (lo < hi) {
    const size_t mid = (lo + hi) / 2;
    if (cmap_[mid].cp < cp) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return lo < cmapCount_ && cmap_[lo].cp == cp ? cmap_[lo].glyph : -1;
}

bool Font::covers(const uint32_t cp) const {
  if (!open_) return false;
  if (cp == kZwj || cp == kZwnj) return true;
  return glyphFor(cp) >= 0;
}

int16_t Font::kerning(const uint16_t left, const uint16_t right) const {
  const uint32_t key = static_cast<uint32_t>(left) << 16 | right;
  size_t lo = 0, hi = kernCount_;
  while (lo < hi) {
    const size_t mid = (lo + hi) / 2;
    if (kern_[mid].pair < key) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return lo < kernCount_ && kern_[lo].pair == key ? kern_[lo].delta : 0;
}

// Syllable advances were shaped with the Regular face; the Bold styles keep
// the same GPOS adjustment on top of the Bold glyph's own width.
int16_t Font::styleAdvance(const Style style, const uint16_t glyph, const int16_t regularAdvance) const {
  if (glyph >= glyphCount_ || !styles_[static_cast<int>(style)].bold) return regularAdvance;
  return static_cast<int16_t>(regularAdvance + advances_[glyph * 2 + 1] - advances_[glyph * 2]);
}

const uint8_t* Font::syllable(const uint32_t slot) {
  SyllableSlot& e = syllables_[slot % kSyllableCache];
  if (e.key != slot + 1) {
    if (slot >= dictSlots_ || !readAt(dictOffset_ + slot * slotSize_, e.data, sizeof(e.data))) {
      LOG_ERR("KNF", "Could not read syllable %lu", static_cast<unsigned long>(slot));
      e.key = 0;
      return nullptr;
    }
    e.key = slot + 1;
  }
  return e.data;
}

size_t Font::shape(const char* text, const size_t len, const Style style, Glyph* out, const size_t cap,
                   size_t* consumed) {
  size_t n = 0;
  size_t i = 0;
  bool boundaryKern = false;  // kerning applies between syllables, not inside one
  auto emit = [&](const uint16_t g, const int16_t regularAdvance, const bool boundary) {
    if (n > 0 && boundary && boundaryKern) {
      out[n - 1].advance = static_cast<int16_t>(out[n - 1].advance + kerning(out[n - 1].id, g));
    }
    out[n].id = g;
    out[n].advance = styleAdvance(style, g, regularAdvance);
    ++n;
    boundaryKern = true;
  };
  // Leave room for the largest syllable (6 glyphs + reph).
  while (open_ && i < len && n + 8 <= cap) {
    uint32_t cp = 0;
    const size_t cpLen = decode(text, len, i, cp);
    size_t clen = 0;
    int c = consonantAt(text, len, i, clen);
    if (c >= 0) {
      bool reph = false;
      if (cp == kRa) {
        uint32_t v = 0;
        const size_t vlen = decode(text, len, i + cpLen, v);
        size_t next = 0;
        if (vlen && v == kVirama && consonantAt(text, len, i + cpLen + vlen, next) >= 0) {
          reph = true;
          i += cpLen + vlen;
          c = consonantAt(text, len, i, clen);
        }
      }
      int cons[3] = {c, -1, -1};
      int count = 1;
      i += clen;
      while (count < 3) {
        uint32_t v = 0;
        const size_t vlen = decode(text, len, i, v);
        if (!vlen || v != kVirama) break;
        size_t nlen = 0;
        const int nc = consonantAt(text, len, i + vlen, nlen);
        if (nc < 0) break;
        cons[count++] = nc;
        i += vlen + nlen;
      }
      int m = 0;
      uint32_t v = 0;
      const size_t vlen = decode(text, len, i, v);
      if (vlen && (m = matraIndex(v)) > 0) i += vlen;
      const uint32_t slot =
          ((static_cast<uint32_t>(cons[0]) * slotsPer_ + (cons[1] + 1)) * slotsPer_ + (cons[2] + 1)) * matras_ + m;
      const uint8_t* data = syllable(slot);
      if (data != nullptr) {
        const int count2 = std::min<int>(data[0], 6);
        for (int k = 0; k < count2; ++k) {
          const uint32_t packed = data[1 + 3 * k] | (data[2 + 3 * k] << 8) | (data[3 + 3 * k] << 16);
          emit(static_cast<uint16_t>(packed & 0x3FF), static_cast<int16_t>(packed >> 10), k == 0);
        }
      }
      if (reph) emit(rephGlyph_, static_cast<int16_t>(advances_[rephGlyph_ * 2]), true);
      continue;
    }
    if (cp == kZwj || cp == kZwnj) {
      i += cpLen;
      continue;
    }
    const int g = glyphFor(cp);
    if (g < 0) break;  // not ours: the caller draws it with the UI font
    emit(static_cast<uint16_t>(g), static_cast<int16_t>(advances_[g * 2]), true);
    i += cpLen;
  }
  if (consumed != nullptr) *consumed = i;
  return n;
}

bool Font::bitmap(const Style style, const uint16_t glyph, GlyphBitmap& out) {
  out = {};
  if (!open_ || glyph >= glyphCount_) return false;
  const int s = static_cast<int>(style);
  const uint16_t key = static_cast<uint16_t>(s << 10 | glyph);
  GlyphEntry& e = glyphs_[(glyph * 7 + s) % kGlyphBuckets];
  if (e.key != key) {
    uint8_t m[8];
    if (!readAt(styles_[s].metricsOffset + 8u * glyph, m, sizeof(m))) return false;
    const size_t bytes = static_cast<size_t>((m[4] + 7) / 8) * m[5];
    if (bytes > arenaBytes_) return false;
    if (arenaUsed_ + bytes > arenaBytes_) {
      // Arena full: forget every cached glyph and start again.
      for (int b = 0; b < kGlyphBuckets; ++b) glyphs_[b].key = 0xFFFF;
      arenaUsed_ = 0;
    }
    if (bytes > 0 && !readAt(styles_[s].bitmapsOffset + le32(m), arena_.get() + arenaUsed_, bytes)) {
      e.key = 0xFFFF;
      return false;
    }
    e.key = key;
    e.w = m[4];
    e.h = m[5];
    e.left = static_cast<int8_t>(m[6]);
    e.top = static_cast<int8_t>(m[7]);
    e.arenaOffset = static_cast<uint16_t>(arenaUsed_);
    arenaUsed_ += bytes;
  }
  out.w = e.w;
  out.h = e.h;
  out.left = e.left;
  out.top = e.top;
  out.bits = arena_.get() + e.arenaOffset;
  return true;
}

}  // namespace kannada
