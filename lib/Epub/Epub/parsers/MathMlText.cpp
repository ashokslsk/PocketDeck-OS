#include "MathMlText.h"

#include <cstring>

namespace {

// Unicode superscript / subscript digits (U+00B9 U+00B2 U+00B3 U+2070...,
// U+2080...); both built-in reader fonts carry them. Other characters use
// ^(...) / _(...).
const char* const kSuperDigits[10] = {"\xE2\x81\xB0", "\xC2\xB9",     "\xC2\xB2",     "\xC2\xB3",     "\xE2\x81\xB4",
                                      "\xE2\x81\xB5", "\xE2\x81\xB6", "\xE2\x81\xB7", "\xE2\x81\xB8", "\xE2\x81\xB9"};
const char* const kSubDigits[10] = {"\xE2\x82\x80", "\xE2\x82\x81", "\xE2\x82\x82", "\xE2\x82\x83", "\xE2\x82\x84",
                                    "\xE2\x82\x85", "\xE2\x82\x86", "\xE2\x82\x87", "\xE2\x82\x88", "\xE2\x82\x89"};

bool allDigits(const std::string& s) {
  if (s.empty()) return false;
  for (const char c : s) {
    if (c < '0' || c > '9') return false;
  }
  return true;
}

// Number of UTF-8 code points.
size_t codepoints(const std::string& s) {
  size_t n = 0;
  for (const char c : s) {
    if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
  }
  return n;
}

bool isScriptDigit(const char* p) {
  for (int d = 0; d < 10; ++d) {
    if (strncmp(p, kSuperDigits[d], strlen(kSuperDigits[d])) == 0) return true;
    if (strncmp(p, kSubDigits[d], strlen(kSubDigits[d])) == 0) return true;
  }
  return false;
}

// Code points that are not superscript/subscript digits: x² counts as one.
size_t symbols(const std::string& s) {
  size_t n = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if ((c & 0xC0) == 0x80) continue;
    if (!isScriptDigit(s.c_str() + i)) ++n;
  }
  return n;
}

// A part of a fraction or script needs brackets when it is more than one
// symbol and not a plain number: (a+b)/(2m), but h/p, c³ and x^12 stay bare.
bool needsBrackets(const std::string& s) {
  // √x counts as one symbol too.
  static const std::string kRoot = "\xE2\x88\x9A";
  if (s.compare(0, kRoot.size(), kRoot) == 0) return symbols(s) > 2;
  return symbols(s) > 1 && !allDigits(s);
}

void appendScript(std::string& out, const std::string& script, const bool super) {
  if (script.empty()) return;
  // Primes attach directly: x′, x″.
  if (super && (script == "\xE2\x80\xB2" || script == "\xE2\x80\xB3")) {
    out += script;
    return;
  }
  if (allDigits(script)) {
    for (const char c : script) out += (super ? kSuperDigits : kSubDigits)[c - '0'];
    return;
  }
  out += super ? '^' : '_';
  if (needsBrackets(script)) {
    out += '(';
    out += script;
    out += ')';
  } else {
    out += script;
  }
}

// Operators that read better with spaces around them (and give long formulas
// places to wrap). Others (− + ⊗ ( ) | , ...) stay tight.
bool spacedOperator(const std::string& op) {
  static const char* const kSpaced[] = {"=",
                                        "\xE2\x89\x88" /*≈*/,
                                        "\xE2\x89\xA0" /*≠*/,
                                        "\xE2\x89\xA4" /*≤*/,
                                        "\xE2\x89\xA5" /*≥*/,
                                        "\xE2\x89\xA1" /*≡*/,
                                        "\xE2\x86\x92" /*→*/,
                                        "\xE2\x87\x92" /*⇒*/,
                                        "\xE2\x9F\xB6" /*⟶*/,
                                        "\xE2\x86\xA6" /*↦*/,
                                        "\xE2\x88\x9D" /*∝*/,
                                        "\xE2\x89\xAA" /*≪*/,
                                        "\xE2\x89\xAB" /*≫*/,
                                        "<",
                                        ">"};
  for (const char* s : kSpaced) {
    if (op == s) return true;
  }
  return false;
}

}  // namespace

MathMlText::Kind MathMlText::kindOf(const char* tag) {
  // Ignore an XML namespace prefix (m:mfrac).
  const char* colon = strchr(tag, ':');
  if (colon != nullptr) tag = colon + 1;
  struct Entry {
    const char* name;
    Kind kind;
  };
  static const Entry kTable[] = {
      {"mi", Kind::Token},
      {"mn", Kind::Token},
      {"mtext", Kind::Token},
      {"ms", Kind::Token},
      {"mo", Kind::Op},
      {"mfrac", Kind::Frac},
      {"msup", Kind::Sup},
      {"msub", Kind::Sub},
      {"msubsup", Kind::SubSup},
      {"munder", Kind::Under},
      {"mover", Kind::Over},
      {"munderover", Kind::UnderOver},
      {"msqrt", Kind::Sqrt},
      {"mroot", Kind::Root},
      {"mtable", Kind::Table},
      {"mtr", Kind::Row},
      {"mlabeledtr", Kind::Row},
      {"annotation", Kind::Skip},
      {"annotation-xml", Kind::Skip},
      {"mphantom", Kind::Skip},
      {"none", Kind::Skip},
      {"mprescripts", Kind::Skip},
      {"semantics", Kind::Semantics},
  };
  for (const Entry& e : kTable) {
    if (strcmp(tag, e.name) == 0) return e.kind;
  }
  return Kind::Group;
}

void MathMlText::begin(const bool block) {
  nodes_.clear();
  stack_.clear();
  plain_.clear();
  overflow_ = false;
  block_ = block;
  depth_ = 1;
  nodes_.reserve(32);
  nodes_.push_back({Kind::Group, {}, {}});
  stack_.push_back(0);
}

void MathMlText::open(const char* tag) {
  ++depth_;
  if (overflow_ || nodes_.size() >= kMaxNodes) {
    overflow_ = true;
    return;
  }
  const auto index = static_cast<uint16_t>(nodes_.size());
  nodes_.push_back({kindOf(tag), {}, {}});
  nodes_[stack_.back()].kids.push_back(index);
  stack_.push_back(index);
}

void MathMlText::text(const char* s, const int len) {
  if (len <= 0) return;
  if (plain_.size() + static_cast<size_t>(len) > kMaxText) {
    overflow_ = true;
    return;
  }
  plain_.append(s, static_cast<size_t>(len));
  if (overflow_) return;
  Node& n = nodes_[stack_.back()];
  if (n.kind == Kind::Token || n.kind == Kind::Op) {
    // Collapse whitespace inside tokens.
    for (int i = 0; i < len; ++i) {
      const char c = s[i];
      if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
      n.text += c;
    }
  }
}

bool MathMlText::close() {
  if (depth_ <= 0) return false;
  --depth_;
  if (!overflow_ && stack_.size() > 1) stack_.pop_back();
  return depth_ == 0;
}

std::string MathMlText::linear(const uint16_t node) const {
  std::string s;
  emit(node, s);
  return s;
}

void MathMlText::emit(const uint16_t node, std::string& out) const {
  const Node& n = nodes_[node];
  const auto kid = [&](const size_t i) { return i < n.kids.size() ? linear(n.kids[i]) : std::string(); };
  switch (n.kind) {
    case Kind::Token:
      out += n.text;
      return;
    case Kind::Op:
      if (n.text == ",") {
        out += ", ";
      } else if (spacedOperator(n.text)) {
        out += ' ';
        out += n.text;
        out += ' ';
      } else {
        out += n.text;
      }
      return;
    case Kind::Skip:
      return;
    case Kind::Semantics:
      if (!n.kids.empty()) emit(n.kids[0], out);
      return;
    case Kind::Frac: {
      const std::string num = kid(0);
      const std::string den = kid(1);
      out += needsBrackets(num) ? "(" + num + ")" : num;
      out += '/';
      out += needsBrackets(den) ? "(" + den + ")" : den;
      return;
    }
    case Kind::Sup:
      out += kid(0);
      appendScript(out, kid(1), true);
      return;
    case Kind::Sub:
      out += kid(0);
      appendScript(out, kid(1), false);
      return;
    case Kind::SubSup:
    case Kind::UnderOver:
      out += kid(0);
      appendScript(out, kid(1), false);
      appendScript(out, kid(2), true);
      return;
    case Kind::Under:
      out += kid(0);
      appendScript(out, kid(1), false);
      return;
    case Kind::Over: {
      // Accents (x̂ written as <mover>x ^</mover>, vector arrows) follow the
      // base, unspaced.
      out += kid(0);
      std::string over = kid(1);
      if (n.kids.size() > 1 && (nodes_[n.kids[1]].kind == Kind::Op || nodes_[n.kids[1]].kind == Kind::Token)) {
        over = nodes_[n.kids[1]].text;
      }
      if (codepoints(over) == 1) {
        out += over;
      } else {
        appendScript(out, over, true);
      }
      return;
    }
    case Kind::Sqrt: {
      std::string inner;
      for (const uint16_t k : n.kids) emit(k, inner);
      out += "\xE2\x88\x9A";  // √
      out += needsBrackets(inner) ? "(" + inner + ")" : inner;
      return;
    }
    case Kind::Root: {
      const std::string base = kid(0);
      const std::string index = kid(1);
      appendScript(out, index, true);
      out += "\xE2\x88\x9A";
      out += needsBrackets(base) ? "(" + base + ")" : base;
      return;
    }
    case Kind::Table:
      for (size_t i = 0; i < n.kids.size(); ++i) {
        if (i > 0) out += "; ";
        emit(n.kids[i], out);
      }
      return;
    case Kind::Row:
      for (size_t i = 0; i < n.kids.size(); ++i) {
        if (i > 0) out += ", ";
        emit(n.kids[i], out);
      }
      return;
    case Kind::Group:
      for (const uint16_t k : n.kids) emit(k, out);
      return;
  }
}

std::string MathMlText::result() const {
  std::string body = overflow_ ? plain_ : linear(0);
  // Collapse runs of spaces left by spaced operators.
  std::string out;
  out.reserve(body.size() + 2);
  if (block_) out += ' ';
  for (const char c : body) {
    if (c == ' ' && !out.empty() && out.back() == ' ') continue;
    out += c;
  }
  if (block_ && (out.empty() || out.back() != ' ')) out += ' ';
  return out;
}
