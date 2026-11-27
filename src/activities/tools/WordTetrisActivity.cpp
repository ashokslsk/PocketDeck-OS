#include "WordTetrisActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "WordTetrisPuzzles.h"
#include "components/UITheme.h"
#include "fontIds.h"

constexpr char WordTetrisActivity::kStatePath[];

namespace {
constexpr int kBorder = 3;       // piece outline width in pixels
constexpr int kMarkMargin = 30;  // room right of the board for row marks
constexpr int kFastRefreshesBeforeClean = 30;

// Draws `text` centred in a square cell. drawText() takes the top of the line
// box; capitals sit in roughly the top 3/4 of the ascender, so centre on that.
void drawCellLetter(const GfxRenderer& renderer, const int fontId, const int x, const int y, const int cell,
                    const char letter, const bool black) {
  const char text[2] = {letter, '\0'};
  const int tw = renderer.getTextWidth(fontId, text, EpdFontFamily::BOLD);
  const int asc = renderer.getFontAscenderSize(fontId);
  renderer.drawText(fontId, x + (cell - tw) / 2, y + cell / 2 - asc * 64 / 100, text, black, EpdFontFamily::BOLD);
}
}  // namespace

void WordTetrisActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  loadState();
  loadPuzzle(puzzle_);
  transitionPending_ = true;
  requestUpdate();
}

void WordTetrisActivity::onExit() {
  saveState();
  Activity::onExit();
}

// ---------------------------------------------------------------------------
// Persistence: puzzle number plus solved and hint counters.
// ---------------------------------------------------------------------------

void WordTetrisActivity::loadState() {
  tools::recoverFromBackup(kStatePath);
  FsFile f;
  if (!Storage.exists(kStatePath) || !Storage.openFileForRead("TOOLS", kStatePath, f)) return;
  char line[32];
  while (tools::readLine(f, line, sizeof(line)) >= 0) {
    char* eq = strchr(line, '=');
    if (eq == nullptr) continue;
    *eq = '\0';
    const int n = atoi(eq + 1);
    if (n < 0) continue;
    if (strcmp(line, "puzzle") == 0) puzzle_ = n % wordtetris::kPuzzleCount;
    if (strcmp(line, "solved") == 0) solvedCount_ = static_cast<uint16_t>(std::min(n, 65535));
    if (strcmp(line, "hints") == 0) hintCount_ = static_cast<uint16_t>(std::min(n, 65535));
  }
  f.close();
}

// cppcheck-suppress constParameterCallback ; WriteFn requires a mutable void* context
bool WordTetrisActivity::writeState(FsFile& out, void* ctx) {
  const auto* self = static_cast<const WordTetrisActivity*>(ctx);
  char buf[64];
  snprintf(buf, sizeof(buf), "puzzle=%d\nsolved=%u\nhints=%u\n", self->puzzle_, self->solvedCount_, self->hintCount_);
  return tools::writeText(out, buf);
}

void WordTetrisActivity::saveState() {
  if (!tools::writeFileAtomic(kStatePath, &WordTetrisActivity::writeState, this)) {
    LOG_ERR("WTET", "Failed to save %s", kStatePath);
  }
}

// ---------------------------------------------------------------------------
// Board logic
// ---------------------------------------------------------------------------

void WordTetrisActivity::loadPuzzle(const int index) {
  puzzle_ = ((index % wordtetris::kPuzzleCount) + wordtetris::kPuzzleCount) % wordtetris::kPuzzleCount;
  const wordtetris::Puzzle& src = wordtetris::kPuzzles[puzzle_];
  for (int p = 0; p < kPieces; ++p) {
    Piece& piece = pieces_[p];
    piece = Piece{};
    int count = 0;
    int minRow = kSize;
    int minCol = kSize;
    int maxRow = 0;
    int maxCol = 0;
    for (int i = 0; i < kSize * kSize; ++i) {
      if (static_cast<int>((src.pieces >> (2 * i)) & 3u) != p) continue;
      const int r = i / kSize;
      const int c = i % kSize;
      minRow = std::min(minRow, r);
      minCol = std::min(minCol, c);
      maxRow = std::max(maxRow, r);
      maxCol = std::max(maxCol, c);
      if (count < kCells) {
        piece.dr[count] = static_cast<int8_t>(r);
        piece.dc[count] = static_cast<int8_t>(c);
        piece.letter[count] = src.letters[i];
        ++count;
      }
    }
    if (count != kCells) {
      LOG_ERR("WTET", "Puzzle %d piece %d has %d cells", puzzle_, p, count);
    }
    for (int k = 0; k < kCells; ++k) {
      piece.dr[k] = static_cast<int8_t>(piece.dr[k] - minRow);
      piece.dc[k] = static_cast<int8_t>(piece.dc[k] - minCol);
    }
    piece.height = static_cast<int8_t>(maxRow - minRow + 1);
    piece.width = static_cast<int8_t>(maxCol - minCol + 1);
    piece.solRow = static_cast<int8_t>(minRow);
    piece.solCol = static_cast<int8_t>(minCol);
  }
  selected_ = 0;
  mode_ = Mode::Tray;
  note_ = Note::None;
  showRowMarks_ = false;
}

int WordTetrisActivity::pieceAt(const int row, const int col, const int skip) const {
  for (int p = 0; p < kPieces; ++p) {
    if (p == skip || !pieces_[p].placed) continue;
    const Piece& piece = pieces_[p];
    for (int k = 0; k < kCells; ++k) {
      if (piece.row + piece.dr[k] == row && piece.col + piece.dc[k] == col) return p;
    }
  }
  return -1;
}

char WordTetrisActivity::letterAt(const int row, const int col) const {
  const int p = pieceAt(row, col);
  if (p < 0) return '\0';
  const Piece& piece = pieces_[p];
  for (int k = 0; k < kCells; ++k) {
    if (piece.row + piece.dr[k] == row && piece.col + piece.dc[k] == col) return piece.letter[k];
  }
  return '\0';
}

bool WordTetrisActivity::fits(const int p, const int row, const int col) const {
  const Piece& piece = pieces_[p];
  if (row < 0 || col < 0 || row + piece.height > kSize || col + piece.width > kSize) return false;
  for (int k = 0; k < kCells; ++k) {
    if (pieceAt(row + piece.dr[k], col + piece.dc[k], p) >= 0) return false;
  }
  return true;
}

bool WordTetrisActivity::rowIsWord(const int row) const {
  char word[kSize];
  for (int c = 0; c < kSize; ++c) {
    word[c] = letterAt(row, c);
    if (word[c] == '\0') return false;
  }
  // kWords is sorted, four letters per word with no separators.
  int lo = 0;
  int hi = wordtetris::kWordCount - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const int cmp = memcmp(word, &wordtetris::kWords[mid * kSize], kSize);
    if (cmp == 0) return true;
    if (cmp < 0) {
      hi = mid - 1;
    } else {
      lo = mid + 1;
    }
  }
  return false;
}

void WordTetrisActivity::checkBoard() {
  for (const Piece& piece : pieces_) {
    if (!piece.placed) return;
  }
  bool all = true;
  for (int r = 0; r < kSize; ++r) {
    rowOk_[r] = rowIsWord(r);
    all = all && rowOk_[r];
  }
  showRowMarks_ = true;
  if (all) {
    // The generator only keeps puzzles with a single all-words layout, so
    // every row being a word means this is the solution.
    mode_ = Mode::Solved;
    note_ = Note::None;
    if (solvedCount_ < 65535) ++solvedCount_;
    saveState();
    LOG_INF("WTET", "Solved puzzle %d", puzzle_ + 1);
  } else {
    note_ = Note::NotWords;
  }
}

void WordTetrisActivity::pickUp() {
  Piece& piece = pieces_[selected_];
  movingWasPlaced_ = piece.placed;
  movingFromRow_ = piece.row;
  movingFromCol_ = piece.col;
  piece.placed = false;
  if (!movingWasPlaced_) {
    // Start at the first free spot (reading order), or the top-left corner.
    piece.row = 0;
    piece.col = 0;
    for (int r = 0, found = 0; r + piece.height <= kSize && !found; ++r) {
      for (int c = 0; c + piece.width <= kSize; ++c) {
        if (fits(selected_, r, c)) {
          piece.row = static_cast<int8_t>(r);
          piece.col = static_cast<int8_t>(c);
          found = 1;
          break;
        }
      }
    }
  }
  mode_ = Mode::Place;
  note_ = Note::None;
  showRowMarks_ = false;
}

void WordTetrisActivity::moveBy(const int dRow, const int dCol) {
  Piece& piece = pieces_[selected_];
  const int row = std::max(0, std::min(kSize - piece.height, piece.row + dRow));
  const int col = std::max(0, std::min(kSize - piece.width, piece.col + dCol));
  note_ = Note::None;
  if (row == piece.row && col == piece.col) return;
  piece.row = static_cast<int8_t>(row);
  piece.col = static_cast<int8_t>(col);
}

void WordTetrisActivity::drop() {
  Piece& piece = pieces_[selected_];
  if (!fits(selected_, piece.row, piece.col)) {
    note_ = Note::NoFit;
    return;
  }
  piece.placed = true;
  mode_ = Mode::Tray;
  note_ = Note::None;
  // Move the tray cursor to the next piece still waiting to be placed.
  for (int step = 1; step < kPieces; ++step) {
    const int p = (selected_ + step) % kPieces;
    if (!pieces_[p].placed) {
      selected_ = p;
      break;
    }
  }
  checkBoard();
}

void WordTetrisActivity::cancelMove() {
  if (movingWasPlaced_) {
    Piece& piece = pieces_[selected_];
    piece.row = movingFromRow_;
    piece.col = movingFromCol_;
    piece.placed = true;
  }
  mode_ = Mode::Tray;
  note_ = Note::None;
}

void WordTetrisActivity::giveHint() {
  // Lift every piece that is in the wrong place, then put one piece in its
  // solution spot. Correct pieces never overlap another piece's solution spot.
  for (Piece& piece : pieces_) {
    if (piece.placed && (piece.row != piece.solRow || piece.col != piece.solCol)) piece.placed = false;
  }
  for (int p = 0; p < kPieces; ++p) {
    Piece& piece = pieces_[p];
    if (piece.placed) continue;
    piece.row = piece.solRow;
    piece.col = piece.solCol;
    piece.placed = true;
    selected_ = p;
    if (hintCount_ < 65535) ++hintCount_;
    break;
  }
  note_ = Note::None;
  showRowMarks_ = false;
  checkBoard();
  for (int p = 0; p < kPieces && mode_ == Mode::Tray; ++p) {
    if (!pieces_[p].placed) {
      selected_ = p;
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void WordTetrisActivity::loop() {
  input_.poll(mappedInput);
  if (input_.backLong) {
    tools::exitToHome();
    return;
  }
  if (!input_.any) return;

  switch (mode_) {
    case Mode::Tray:
      if (input_.back) {
        finish();
        return;
      }
      if (input_.confirm) {
        pickUp();
      } else if (input_.confirmLong) {
        giveHint();
      } else if (input_.prev()) {
        selected_ = (selected_ + kPieces - 1) % kPieces;
        note_ = Note::None;
      } else if (input_.next()) {
        selected_ = (selected_ + 1) % kPieces;
        note_ = Note::None;
      } else {
        return;
      }
      break;
    case Mode::Place:
      // Side buttons report both Up/Down and PageBack/PageForward; treat the
      // pair as one move so a press never jumps two rows.
      if (input_.back) {
        cancelMove();
      } else if (input_.confirm) {
        drop();
      } else if (input_.left) {
        moveBy(0, -1);
      } else if (input_.right) {
        moveBy(0, 1);
      } else if (input_.up || input_.pageBack) {
        moveBy(-1, 0);
      } else if (input_.down || input_.pageForward) {
        moveBy(1, 0);
      } else {
        return;
      }
      break;
    case Mode::Solved:
      if (input_.back) {
        finish();
        return;
      }
      if (!input_.confirm) return;
      loadPuzzle(puzzle_ + 1);
      saveState();
      transitionPending_ = true;
      break;
  }
  requestUpdate();
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void WordTetrisActivity::drawPiece(const int p, const int x, const int y, const int cell, const int fontId,
                                   const bool inverted) const {
  const Piece& piece = pieces_[p];
  auto inPiece = [&piece](const int r, const int c) {
    for (int k = 0; k < kCells; ++k) {
      if (piece.dr[k] == r && piece.dc[k] == c) return true;
    }
    return false;
  };
  for (int k = 0; k < kCells; ++k) {
    const int r = piece.dr[k];
    const int c = piece.dc[k];
    const int px = x + c * cell;
    const int py = y + r * cell;
    renderer.fillRect(px, py, cell, cell, inverted);
    if (fontId != 0) drawCellLetter(renderer, fontId, px, py, cell, piece.letter[k], !inverted);
    // Thick edges where the piece ends, thin lines between its own cells.
    if (inPiece(r - 1, c)) {
      renderer.drawLine(px, py, px + cell - 1, py, !inverted);
    } else {
      renderer.fillRect(px, py, cell, kBorder, true);
    }
    if (!inPiece(r + 1, c)) renderer.fillRect(px, py + cell - kBorder, cell, kBorder, true);
    if (inPiece(r, c - 1)) {
      renderer.drawLine(px, py, px, py + cell - 1, !inverted);
    } else {
      renderer.fillRect(px, py, kBorder, cell, true);
    }
    if (!inPiece(r, c + 1)) renderer.fillRect(px + cell - kBorder, py, kBorder, cell, true);
  }
}

void WordTetrisActivity::drawBoard(const int x, const int y, const int cell) const {
  const int side = cell * kSize;
  // Empty cells: a light grid.
  for (int r = 0; r < kSize; ++r) {
    for (int c = 0; c < kSize; ++c) renderer.drawRect(x + c * cell, y + r * cell, cell + 1, cell + 1, true);
  }
  const int moving = mode_ == Mode::Place ? selected_ : -1;
  for (int p = 0; p < kPieces; ++p) {
    if (p == moving || !pieces_[p].placed) continue;
    drawPiece(p, x + pieces_[p].col * cell, y + pieces_[p].row * cell, cell, LEXENDDECA_16_FONT_ID, false);
  }
  renderer.drawRect(x - kBorder, y - kBorder, side + 2 * kBorder, side + 2 * kBorder, kBorder, true);
  if (moving >= 0) {
    // The piece in hand is drawn inverted on top, so it reads as "lifted".
    drawPiece(moving, x + pieces_[moving].col * cell, y + pieces_[moving].row * cell, cell, LEXENDDECA_16_FONT_ID,
              true);
  }
  if (!showRowMarks_) return;
  // Row marks: a tick for rows that read a word, a cross for the rest.
  const int mx = x + side + 10;
  const int m = std::min(16, kMarkMargin - 12);
  for (int r = 0; r < kSize; ++r) {
    const int cy = y + r * cell + cell / 2;
    if (rowOk_[r]) {
      renderer.drawLine(mx, cy, mx + m / 3, cy + m / 3, 3, true);
      renderer.drawLine(mx + m / 3, cy + m / 3, mx + m, cy - m / 2, 3, true);
    } else {
      renderer.drawLine(mx, cy - m / 2, mx + m, cy + m / 2, 3, true);
      renderer.drawLine(mx, cy + m / 2, mx + m, cy - m / 2, 3, true);
    }
  }
}

void WordTetrisActivity::drawTray(const Rect& area) const {
  // Each slot fits the largest piece bounding box of this puzzle. Pick the
  // slot arrangement (4x1, 2x2 or 1x4) that gives the largest cells.
  constexpr int kGap = 10;
  constexpr int kPad = 8;
  int slotCols = 1;
  int slotRows = 1;
  for (const Piece& piece : pieces_) {
    slotCols = std::max<int>(slotCols, piece.width);
    slotRows = std::max<int>(slotRows, piece.height);
  }
  int bestCell = 0;
  int bestCols = kPieces;
  for (const int cols : {4, 2, 1}) {
    const int rows = kPieces / cols;
    const int byW = (area.width - (cols - 1) * kGap - cols * 2 * kPad) / (cols * slotCols);
    const int byH = (area.height - (rows - 1) * kGap - rows * 2 * kPad) / (rows * slotRows);
    const int cell = std::min(byW, byH);
    if (cell > bestCell) {
      bestCell = cell;
      bestCols = cols;
    }
  }
  const int cell = std::min(bestCell, 40);
  if (cell < 8) return;
  const int cols = bestCols;
  const int rows = kPieces / cols;
  const int slotW = slotCols * cell + 2 * kPad;
  const int slotH = slotRows * cell + 2 * kPad;
  const int totalW = cols * slotW + (cols - 1) * kGap;
  const int totalH = rows * slotH + (rows - 1) * kGap;
  const int x0 = area.x + (area.width - totalW) / 2;
  const int y0 = area.y + (area.height - totalH) / 2;
  const int fontId = cell >= 30 ? UI_12_FONT_ID : UI_10_FONT_ID;

  for (int p = 0; p < kPieces; ++p) {
    const Piece& piece = pieces_[p];
    const int sx = x0 + (p % cols) * (slotW + kGap);
    const int sy = y0 + (p / cols) * (slotH + kGap);
    const int px = sx + (slotW - piece.width * cell) / 2;
    const int py = sy + (slotH - piece.height * cell) / 2;
    const bool inHand = mode_ == Mode::Place && p == selected_;
    if (piece.placed || inHand) {
      // Already on the board (or in hand): an empty outline keeps its slot.
      for (int k = 0; k < kCells; ++k) {
        renderer.drawRect(px + piece.dc[k] * cell, py + piece.dr[k] * cell, cell + 1, cell + 1, true);
      }
    } else {
      drawPiece(p, px, py, cell, fontId, false);
    }
    if (p == selected_ && mode_ != Mode::Solved) {
      renderer.drawRoundedRect(sx, sy, slotW, slotH, 3, 8, true);
    }
  }
}

void WordTetrisActivity::render(RenderLock&&) {
  char subtitle[32];
  snprintf(subtitle, sizeof(subtitle), "%s %d / %d", tr(STR_TOOLS_WT_PUZZLE), puzzle_ + 1, wordtetris::kPuzzleCount);
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_WORD_TETRIS), subtitle);

  // One status line on top tells the player what the buttons do right now.
  const char* status = tr(STR_TOOLS_WT_TRAY_HINT);
  if (mode_ == Mode::Solved) {
    status = tr(STR_TOOLS_WT_SOLVED);
  } else if (note_ == Note::NoFit) {
    status = tr(STR_TOOLS_WT_NO_FIT);
  } else if (note_ == Note::NotWords) {
    status = tr(STR_TOOLS_WT_NOT_WORDS);
  } else if (mode_ == Mode::Place) {
    status = tr(STR_TOOLS_WT_PLACE_HINT);
  }
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const std::string statusText = renderer.truncatedText(UI_10_FONT_ID, status, content.width, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, content.y, statusText.c_str(), true, EpdFontFamily::BOLD);

  const int top = content.y + lineH + 10 + kBorder;
  const int availH = content.y + content.height - top;
  const bool landscape = content.width > availH * 3 / 2;
  int cell = 0;
  int boardX = 0;
  Rect rest;
  if (landscape) {
    cell = std::min(availH - kBorder, content.width / 2 - kMarkMargin) / kSize;
    boardX = content.x + kMarkMargin;
    const int restX = boardX + cell * kSize + kMarkMargin + 10;
    rest = Rect(restX, top, content.x + content.width - restX, availH);
  } else {
    cell = std::min(content.width - 2 * kMarkMargin, availH * 55 / 100) / kSize;
    boardX = content.x + (content.width - cell * kSize) / 2;
    const int restY = top + cell * kSize + kBorder + 12;
    rest = Rect(content.x, restY, content.width, content.y + content.height - restY);
  }
  drawBoard(boardX, top, cell);

  if (mode_ == Mode::Solved) {
    // The tray is empty once solved: show the tally there instead.
    char line[64];
    snprintf(line, sizeof(line), "%s %u   %s %u", tr(STR_TOOLS_WT_SOLVED_COUNT), solvedCount_,
             tr(STR_TOOLS_WT_HINTS_COUNT), hintCount_);
    const int h = renderer.getLineHeight(UI_12_FONT_ID);
    const int w = renderer.getTextWidth(UI_12_FONT_ID, line);
    renderer.drawText(UI_12_FONT_ID, rest.x + (rest.width - w) / 2, rest.y + (rest.height - h) / 2, line, true);
  } else {
    drawTray(rest);
  }

  switch (mode_) {
    case Mode::Tray:
      tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_WT_PICK), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
      break;
    case Mode::Place:
      tools::drawHints(renderer, mappedInput, tr(STR_CANCEL), tr(STR_TOOLS_WT_DROP), tr(STR_DIR_LEFT),
                       tr(STR_DIR_RIGHT));
      break;
    case Mode::Solved:
      tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_WT_NEXT), "", "");
      break;
  }

  // Moving a piece is a fast partial refresh; a clean one on entry, on a new
  // puzzle, and now and then to clear ghosting from the inverted piece.
  const bool clean = transitionPending_ || fastRefreshCount_ >= kFastRefreshesBeforeClean;
  fastRefreshCount_ = clean ? 0 : fastRefreshCount_ + 1;
  transitionPending_ = false;
  renderer.displayBuffer(clean ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
