#pragma once

#include "ToolsCommon.h"
#include "activities/Activity.h"

// Word Tetris: fit four lettered tetrominoes into a 4x4 board so that every
// row reads a word. Pieces never rotate. Puzzles come from the generated
// WordTetrisPuzzles.h table in flash; only the puzzle number and two counters
// are saved (/tools/wordtetris.txt).
//
// Memory: the whole game state is a few dozen bytes inside this object, no
// heap allocations beyond the activity itself.
class WordTetrisActivity final : public Activity {
 public:
  explicit WordTetrisActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("WordTetris", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int kSize = 4;    // board is kSize x kSize
  static constexpr int kPieces = 4;  // four tetrominoes per puzzle
  static constexpr int kCells = 4;   // cells per piece
  static constexpr char kStatePath[] = "/tools/wordtetris.txt";

  enum class Mode : uint8_t { Tray, Place, Solved };
  enum class Note : uint8_t { None, NoFit, NotWords };

  struct Piece {
    int8_t dr[kCells];  // cell offsets inside the piece's bounding box
    int8_t dc[kCells];
    char letter[kCells];
    int8_t height = 1;
    int8_t width = 1;
    int8_t solRow = 0;  // bounding-box origin in the solution
    int8_t solCol = 0;
    int8_t row = 0;  // current origin when placed
    int8_t col = 0;
    bool placed = false;
  };

  void loadState();
  void saveState();
  static bool writeState(FsFile& out, void* ctx);
  void loadPuzzle(int index);

  // Piece index covering a board cell, or -1. `skip` ignores one piece.
  int pieceAt(int row, int col, int skip = -1) const;
  bool fits(int piece, int row, int col) const;
  void pickUp();
  void drop();
  void cancelMove();
  void moveBy(int dRow, int dCol);
  void giveHint();
  void checkBoard();
  bool rowIsWord(int row) const;
  char letterAt(int row, int col) const;

  void drawBoard(int x, int y, int cell) const;
  void drawTray(const Rect& area) const;
  void drawPiece(int piece, int x, int y, int cell, int fontId, bool inverted) const;

  tools::ToolInput input_;
  Piece pieces_[kPieces]{};
  int puzzle_ = 0;
  uint16_t solvedCount_ = 0;
  uint16_t hintCount_ = 0;
  Mode mode_ = Mode::Tray;
  Note note_ = Note::None;
  int selected_ = 0;
  // Where the piece being moved came from, so Back can put it back.
  bool movingWasPlaced_ = false;
  int8_t movingFromRow_ = 0;
  int8_t movingFromCol_ = 0;
  bool rowOk_[kSize]{};
  bool showRowMarks_ = false;
  uint16_t fastRefreshCount_ = 0;  // partial refreshes since the last clean one
  bool transitionPending_ = true;
};
