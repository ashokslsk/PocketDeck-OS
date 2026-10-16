#include "ToolsStore.h"

#include <Logging.h>

#include <cstdio>
#include <cstring>

namespace tools {

bool ensureToolsDirs() {
  const bool ok = Storage.ensureDirectoryExists(kToolsDir) && Storage.ensureDirectoryExists(kToolsCacheDir);
  if (!ok) LOG_ERR("TOOLS", "Failed to create %s", kToolsCacheDir);
  return ok;
}

namespace {
// Paths under /tools are short; 96 bytes covers "<path>.tmp" for every file
// the tools write, and snprintf truncation is checked.
constexpr size_t kPathCap = 96;

bool makeSibling(char* out, const char* path, const char* suffix) {
  const int n = snprintf(out, kPathCap, "%s%s", path, suffix);
  return n > 0 && static_cast<size_t>(n) < kPathCap;
}
}  // namespace

void recoverFromBackup(const char* path) {
  char bak[kPathCap];
  if (!makeSibling(bak, path, ".bak")) return;
  if (!Storage.exists(path) && Storage.exists(bak)) {
    if (Storage.rename(bak, path)) {
      LOG_INF("TOOLS", "Recovered %s from backup", path);
    } else {
      LOG_ERR("TOOLS", "Failed to recover %s from backup", path);
    }
  }
}

bool writeFileAtomic(const char* path, const WriteFn writer, void* ctx) {
  char tmp[kPathCap];
  char bak[kPathCap];
  if (!makeSibling(tmp, path, ".tmp") || !makeSibling(bak, path, ".bak")) {
    LOG_ERR("TOOLS", "Path too long for atomic write: %s", path);
    return false;
  }
  if (!ensureToolsDirs()) return false;
  recoverFromBackup(path);
  if (Storage.exists(tmp)) Storage.remove(tmp);

  FsFile out = Storage.open(tmp, O_WRONLY | O_CREAT | O_TRUNC);
  if (!out) {
    LOG_ERR("TOOLS", "Failed to open %s for write", tmp);
    return false;
  }
  const bool written = writer(out, ctx);
  const bool synced = written && out.sync();
  out.close();
  if (!written || !synced) {
    LOG_ERR("TOOLS", "Failed to write %s", tmp);
    Storage.remove(tmp);
    return false;
  }

  if (Storage.exists(bak)) Storage.remove(bak);
  const bool hadOriginal = Storage.exists(path);
  if (hadOriginal && !Storage.rename(path, bak)) {
    LOG_ERR("TOOLS", "Failed to back up %s", path);
    Storage.remove(tmp);
    return false;
  }
  if (!Storage.rename(tmp, path)) {
    LOG_ERR("TOOLS", "Failed to move %s into place", tmp);
    if (hadOriginal) Storage.rename(bak, path);
    return false;
  }
  if (hadOriginal) Storage.remove(bak);
  return true;
}

int readLine(FsFile& file, char* buf, const size_t cap) {
  size_t n = 0;
  bool sawAny = false;
  while (true) {
    const int c = file.read();
    if (c < 0) break;
    sawAny = true;
    if (c == '\n') break;
    if (c == '\r') continue;
    if (n + 1 < cap) buf[n++] = static_cast<char>(c);
  }
  if (cap > 0) buf[n] = '\0';
  if (!sawAny) return -1;
  // Never leave a split UTF-8 sequence at a truncation point.
  while (n > 0 && (static_cast<uint8_t>(buf[n - 1]) & 0xC0) == 0x80) --n;
  if (n > 0 && (static_cast<uint8_t>(buf[n - 1]) & 0xC0) == 0xC0) --n;
  buf[n] = '\0';
  return static_cast<int>(n);
}

bool writeText(FsFile& out, const char* text) {
  const size_t len = strlen(text);
  return out.write(text, len) == len;
}

bool writeJsonString(FsFile& out, const char* text) {
  if (!writeText(out, "\"")) return false;
  char esc[8];
  for (const char* p = text; *p != '\0'; ++p) {
    const auto c = static_cast<uint8_t>(*p);
    const char* chunk = nullptr;
    size_t chunkLen = 0;
    if (c == '"' || c == '\\') {
      esc[0] = '\\';
      esc[1] = static_cast<char>(c);
      chunk = esc;
      chunkLen = 2;
    } else if (c == '\n') {
      chunk = "\\n";
      chunkLen = 2;
    } else if (c == '\t') {
      chunk = "\\t";
      chunkLen = 2;
    } else if (c < 0x20) {
      snprintf(esc, sizeof(esc), "\\u%04x", c);
      chunk = esc;
      chunkLen = 6;
    } else {
      chunk = p;
      chunkLen = 1;
    }
    if (out.write(chunk, chunkLen) != chunkLen) return false;
  }
  return writeText(out, "\"");
}

// ---------------------------------------------------------------------------
// JsonReader
// ---------------------------------------------------------------------------

bool JsonReader::seek(const uint32_t offset) {
  len_ = 0;
  pos_ = 0;
  bufStart_ = offset;
  return file_.seek(offset);
}

int JsonReader::peekByte() {
  if (pos_ >= len_) {
    bufStart_ += len_;
    const int n = file_.read(buf_, sizeof(buf_));
    pos_ = 0;
    len_ = n > 0 ? static_cast<uint16_t>(n) : 0;
    if (len_ == 0) return -1;
  }
  return buf_[pos_];
}

int JsonReader::readByte() {
  const int c = peekByte();
  if (c >= 0) ++pos_;
  return c;
}

namespace {
size_t appendUtf8(char* text, const size_t cap, size_t n, const uint32_t cp) {
  char enc[4];
  size_t len = 0;
  if (cp < 0x80) {
    enc[len++] = static_cast<char>(cp);
  } else if (cp < 0x800) {
    enc[len++] = static_cast<char>(0xC0 | (cp >> 6));
    enc[len++] = static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    enc[len++] = static_cast<char>(0xE0 | (cp >> 12));
    enc[len++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    enc[len++] = static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    enc[len++] = static_cast<char>(0xF0 | (cp >> 18));
    enc[len++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    enc[len++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    enc[len++] = static_cast<char>(0x80 | (cp & 0x3F));
  }
  if (n + len >= cap) return n;  // drop whole code point rather than split it
  memcpy(text + n, enc, len);
  return n + len;
}

int hexValue(const int c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
}  // namespace

bool JsonReader::readString(char* text, const size_t cap) {
  size_t n = 0;
  uint32_t pendingHigh = 0;
  while (true) {
    int c = readByte();
    if (c < 0) return false;
    if (c == '"') break;
    if (c == '\\') {
      c = readByte();
      if (c < 0) return false;
      uint32_t cp = 0;
      switch (c) {
        case 'n':
          cp = '\n';
          break;
        case 't':
          cp = '\t';
          break;
        case 'r':
        case 'b':
        case 'f':
          continue;
        case 'u': {
          for (int i = 0; i < 4; ++i) {
            const int h = hexValue(readByte());
            if (h < 0) return false;
            cp = (cp << 4) | static_cast<uint32_t>(h);
          }
          if (cp >= 0xD800 && cp <= 0xDBFF) {
            pendingHigh = cp;
            continue;
          }
          if (cp >= 0xDC00 && cp <= 0xDFFF && pendingHigh != 0) {
            cp = 0x10000 + ((pendingHigh - 0xD800) << 10) + (cp - 0xDC00);
          }
          pendingHigh = 0;
          break;
        }
        default:
          cp = static_cast<uint32_t>(c);
          break;
      }
      if (text != nullptr && cap > 0) n = appendUtf8(text, cap, n, cp);
      continue;
    }
    if (text != nullptr && n + 1 < cap) text[n++] = static_cast<char>(c);
  }
  if (text != nullptr && cap > 0) {
    // Trim a multi-byte sequence cut off by truncation.
    size_t end = n;
    while (end > 0 && (static_cast<uint8_t>(text[end - 1]) & 0xC0) == 0x80) --end;
    if (end > 0 && (static_cast<uint8_t>(text[end - 1]) & 0xC0) == 0xC0) {
      const uint8_t lead = static_cast<uint8_t>(text[end - 1]);
      const size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2;
      if (n - (end - 1) < need) n = end - 1;
    }
    text[n] = '\0';
  }
  return true;
}

JsonReader::Token JsonReader::next(char* text, const size_t cap) {
  if (text != nullptr && cap > 0) text[0] = '\0';
  int c = peekByte();
  while (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
    ++pos_;
    c = peekByte();
  }
  tokenOffset_ = bufStart_ + pos_;
  if (c < 0) return Token::End;
  ++pos_;
  switch (c) {
    case '{':
      return Token::ObjectStart;
    case '}':
      return Token::ObjectEnd;
    case '[':
      return Token::ArrayStart;
    case ']':
      return Token::ArrayEnd;
    case ':':
      return Token::Colon;
    case ',':
      return Token::Comma;
    case '"':
      return readString(text, cap) ? Token::String : Token::Error;
    default:
      break;
  }
  // Literal: number, true, false or null.
  size_t n = 0;
  if (text != nullptr && cap > 1) text[n++] = static_cast<char>(c);
  while (true) {
    c = peekByte();
    if (c < 0 || c == ',' || c == ']' || c == '}' || c == ':' || c == ' ' || c == '\n' || c == '\r' || c == '\t') {
      break;
    }
    ++pos_;
    if (text != nullptr && n + 1 < cap) text[n++] = static_cast<char>(c);
  }
  if (text != nullptr && cap > 0) text[n] = '\0';
  return Token::Literal;
}

bool JsonReader::skipValue(const Token first) {
  if (first == Token::String || first == Token::Literal) return true;
  if (first != Token::ObjectStart && first != Token::ArrayStart) return false;
  int depth = 1;
  while (depth > 0) {
    const Token t = next(nullptr, 0);
    if (t == Token::End || t == Token::Error) return false;
    if (t == Token::ObjectStart || t == Token::ArrayStart) ++depth;
    if (t == Token::ObjectEnd || t == Token::ArrayEnd) --depth;
  }
  return true;
}

bool findFirstArray(JsonReader& reader) {
  while (true) {
    const JsonReader::Token t = reader.next(nullptr, 0);
    if (t == JsonReader::Token::ArrayStart) return true;
    if (t == JsonReader::Token::End || t == JsonReader::Token::Error) return false;
  }
}

}  // namespace tools
