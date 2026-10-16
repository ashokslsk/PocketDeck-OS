#include "KannadaReading.h"

#include <GfxRenderer.h>
#include <Logging.h>
#include <MemoryBudget.h>

#include <cstring>

#include "activities/tools/KannadaText.h"

namespace kannadareading {

namespace {

GfxRenderer* gRenderer = nullptr;
bool gActive = false;      // a Kannada book is open and the font is loaded
int gFloorFont = 0;        // reader font id the metric floor was computed for
int gTier = 1;             // Kannada size for the reader font, see kTiers
int gUiSessions = 0;       // book-list screens open (Home, Recent Books, Files)
bool gUiTried = false;     // the font was opened (or failed) for a UI session
bool gUiAcquired = false;  // this module holds a font reference for the UI
bool gDict = false;        // a Kannada dictionary definition is open (font held)
int gDictFloorFont = 0;    // definition font id the dictionary floor is for
int gDictTier = 1;         // Kannada size for the definition font

// Kannada sizes by reader font size: {regular, bold}. Kannada letters sit
// lower in their em than Latin capitals, so each tier is a little larger than
// the reader font it accompanies. With an older kannada.knf (no reading
// sizes) tiers 2 and 3 fall back to 24 / 26 px inside the font.
constexpr kannada::Style kTiers[4][2] = {
    {kannada::Style::Label, kannada::Style::Value},        // 19 / 21 px
    {kannada::Style::Body, kannada::Style::Title},         // 24 / 26 px
    {kannada::Style::Read28, kannada::Style::ReadBold30},  // 28 / 30 px
    {kannada::Style::Read32, kannada::Style::ReadBold34},  // 32 / 34 px
};

// Tier for a font by its own line height (any floor cleared first).
int tierForLineHeight(const int ownLine) { return ownLine < 24 ? 0 : ownLine < 31 ? 1 : ownLine < 39 ? 2 : 3; }

// Raises fontId's line box to fit Kannada of the given tier.
void applyFloor(GfxRenderer& renderer, const int fontId, const int tier) {
  const kannada::Style regular = kTiers[tier][0];
  renderer.setMetricFloor(fontId, kannada::ascent(renderer, regular), kannada::lineHeight(renderer, regular));
}

// In a dictionary definition or a book the tier follows the definition or
// reader font; on book-list screens it follows the UI font the title is drawn with.
kannada::Style styleFor(const GfxRenderer& r, const int fontId, const EpdFontFamily::Style style) {
  const int tier = gDict ? gDictTier : gActive ? gTier : (r.getLineHeight(fontId) < 24 ? 0 : 1);
  return kTiers[tier][(style & EpdFontFamily::BOLD) != 0 ? 1 : 0];
}

bool handles(const char* text) {
  if (gActive || gDict) return kannada::hasKannada(text);
  if (gUiSessions == 0 || !kannada::hasKannada(text)) return false;
  // A Kannada title on Home / Recent Books / Files: open the font the first
  // time one is drawn, and keep it until those screens close.
  if (!kannada::ready() && !gUiTried) {
    gUiTried = true;
    if (kannada::installed()) {
      gUiAcquired = kannada::acquire();
      if (!gUiAcquired) kannada::release();
    }
  }
  return kannada::ready();
}

int measure(const GfxRenderer& r, const int fontId, const char* text, const EpdFontFamily::Style style) {
  return kannada::layoutMixed(r, 0, 0, text, styleFor(r, fontId, style), true, fontId, style, false);
}

void draw(const GfxRenderer& r, const int fontId, const int x, const int y, const char* text, const bool black,
          const EpdFontFamily::Style style) {
  // y is the top of the reader font's line box; put the Kannada baseline on
  // the reader font's baseline.
  const kannada::Style ks = styleFor(r, fontId, style);
  const int top = y + r.getFontAscenderSize(fontId) - kannada::ascent(r, ks);
  kannada::layoutMixed(r, x, top, text, ks, black, fontId, style, true);
}

const ScriptShaper kShaper{&handles, &measure, &draw};

// The OPF parser joins several dc:language entries without a separator
// ("en" + "kn" -> "enkn"), so Kannada may appear anywhere in the string.
bool startsWithKannadaLanguage(const char* lang) {
  return lang != nullptr && (strstr(lang, "kn") != nullptr || strstr(lang, "kan") != nullptr);
}

}  // namespace

void install(GfxRenderer& renderer) {
  gRenderer = &renderer;
  renderer.setScriptShaper(&kShaper);
}

bool beginBook(GfxRenderer& renderer, const char* language, const char* title, const char* sample,
               const size_t sampleLen) {
  endBook(renderer);
  const bool kannadaBook = startsWithKannadaLanguage(language) || (title != nullptr && kannada::hasKannada(title)) ||
                           (sample != nullptr && sampleLen > 0 && kannada::hasKannada(sample, sampleLen));
  if (!kannadaBook || !kannada::installed()) return false;
  if (!kannada::acquire()) {
    kannada::release();
    LOG_ERR("KNR", "Kannada font could not be opened; the book shows without shaping");
    return false;
  }
  gActive = true;
  gFloorFont = 0;
  kannada::setBookCache(true);
  return true;
}

void endBook(GfxRenderer& renderer) {
  if (!gActive) return;
  gActive = false;
  gFloorFont = 0;
  renderer.setMetricFloor(0, 0, 0);
  kannada::logCacheStats("Kannada book");
  kannada::setBookCache(false);
  kannada::release();
}

void useReaderFont(const int fontId) {
  if (!gActive || gRenderer == nullptr || fontId == 0 || fontId == gFloorFont) return;
  GfxRenderer& renderer = *gRenderer;
  // The reader font's own line height (clear any floor from another size first).
  renderer.setMetricFloor(0, 0, 0);
  gTier = tierForLineHeight(renderer.getLineHeight(fontId));
  applyFloor(renderer, fontId, gTier);
  gFloorFont = fontId;
}

bool beginDictionary() {
  if (gDict) return true;
  if (!kannada::installed()) return false;
  // Opening the font takes about 20 KB; keep the definition layout's headroom.
  // A Kannada book that already holds the font costs nothing extra.
  constexpr uint32_t kMinFree = 40U * 1024U;
  constexpr uint32_t kMinMaxAlloc = 24U * 1024U;
  const auto heap = MemoryBudget::snapshot();
  if (!kannada::ready() && !MemoryBudget::hasHeap(heap, kMinFree, kMinMaxAlloc)) {
    LOG_ERR("KNR", "Low heap for Kannada dictionary font (%u free, %u max alloc); showing without shaping",
            static_cast<unsigned>(heap.freeHeap), static_cast<unsigned>(heap.maxAllocHeap));
    return false;
  }
  if (!kannada::acquire()) {
    kannada::release();
    LOG_ERR("KNR", "Kannada font could not be opened; the dictionary shows without shaping");
    return false;
  }
  gDict = true;
  gDictFloorFont = 0;
  return true;
}

void dictionaryFont(const int fontId) {
  if (!gDict || gRenderer == nullptr || fontId == 0 || fontId == gDictFloorFont) return;
  GfxRenderer& renderer = *gRenderer;
  renderer.setMetricFloor(0, 0, 0);
  gDictTier = tierForLineHeight(renderer.getLineHeight(fontId));
  applyFloor(renderer, fontId, gDictTier);
  gDictFloorFont = fontId;
}

void endDictionary(GfxRenderer& renderer) {
  if (!gDict) return;
  gDict = false;
  gDictFloorFont = 0;
  renderer.setMetricFloor(0, 0, 0);
  kannada::release();
  // A Kannada book underneath gets its reader-font floor back.
  if (gActive && gFloorFont != 0) {
    const int readerFont = gFloorFont;
    gFloorFont = 0;
    useReaderFont(readerFont);
  }
}

uint8_t layoutFlag() { return gActive ? 1 : 0; }

void beginUi() { ++gUiSessions; }

void endUi() {
  if (gUiSessions > 0) --gUiSessions;
  if (gUiSessions == 0) {
    if (gUiAcquired) kannada::release();
    gUiAcquired = false;
    gUiTried = false;
  }
}

void releaseForLowMemory() {
  if (gActive) kannada::trimForLowMemory();
}

}  // namespace kannadareading
