#pragma once

#include <cstddef>
#include <cstdint>

class GfxRenderer;
class Page;

namespace EpubGrayscale {
constexpr int GRAYSCALE_STRIP_ROWS = 80;

// Preserves the live BW buffer and existing controller synchronization. False
// leaves the caller responsible for its existing BW-snapshot fallback.
bool runTiledGrayscalePass(GfxRenderer& renderer, const Page& page, int fontId, int marginLeft, int marginTop,
                           bool foregroundBlack, bool needsTextGrayscale, bool needsImageGrayscale, uint8_t* scratch,
                           size_t scratchSize, bool asyncRefreshPending);

// A button press while the anti-aliasing pass runs cuts it short, so the next
// page turn or menu does not wait seconds for it. The reader clears the request
// when it starts a page, sets it from its input loop (main task), and the pass
// (render task) checks it between strips. A cut-short pass leaves the crisp
// black-and-white page on screen.
void clearAbort();
void requestAbort();
bool lastPassAborted();
}  // namespace EpubGrayscale
