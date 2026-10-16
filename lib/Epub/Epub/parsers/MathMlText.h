#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Turns a MathML <math> element into one line of readable text, because the
// layout engine only lays out words:
//   <mfrac>h p</mfrac>          -> h/p      ((a+b)/(2m) when a part is longer)
//   <msup>x 2</msup>            -> x²       (Unicode digits; x^(n+1) otherwise)
//   <msub>ψ 1</msub>            -> ψ₁       (x_(ij) otherwise)
//   <msqrt>x</msqrt>            -> √x / √(x+1)
//   <munderover>∑ i=1 n</...>   -> ∑_(i=1)^(n)
// Relations and arrows get spaces around them so long formulas can wrap.
// Elements are collected as the parser sees them (open/text/close) and the
// text is produced when the <math> element closes. Bounded: past kMaxNodes or
// kMaxText the plain text of the formula is used instead.
class MathMlText {
 public:
  static constexpr size_t kMaxNodes = 384;
  static constexpr size_t kMaxText = 4096;

  // Call at <math>; `block` is display="block".
  void begin(bool block);
  bool active() const { return depth_ > 0; }
  void open(const char* tag);
  void text(const char* s, int len);
  // Returns true when this closed the <math> element (result() is ready).
  bool close();
  // The linear text (with surrounding spaces for block formulas).
  std::string result() const;

 private:
  enum class Kind : uint8_t {
    Group,  // math, mrow, mstyle, mpadded, menclose, mtd ... (children concatenated)
    Token,  // mi, mn, mtext, ms
    Op,     // mo
    Frac,
    Sup,
    Sub,
    SubSup,
    Under,
    Over,
    UnderOver,
    Sqrt,
    Root,
    Table,
    Row,
    Skip,  // annotation, mphantom, none, mprescripts
    Semantics,
  };
  struct Node {
    Kind kind;
    std::string text;
    std::vector<uint16_t> kids;
  };

  static Kind kindOf(const char* tag);
  void emit(uint16_t node, std::string& out) const;
  std::string linear(uint16_t node) const;

  std::vector<Node> nodes_;
  std::vector<uint16_t> stack_;
  std::string plain_;  // every text run, for the overflow fallback
  int depth_ = 0;
  bool block_ = false;
  bool overflow_ = false;
};
