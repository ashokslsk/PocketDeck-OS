#pragma once

#include <HalStorage.h>

#include <cstddef>
#include <cstdint>

// SD-card persistence helpers for the tools. Depends only on HalStorage so it
// can be unit tested on the host. Nothing here allocates on the heap.
namespace tools {

constexpr char kToolsDir[] = "/tools";
constexpr char kToolsCacheDir[] = "/tools/.cache";

bool ensureToolsDirs();

// Crash-safe replace: write to <path>.tmp, move the old file to <path>.bak,
// rename tmp into place, then drop the backup. FAT rename cannot overwrite,
// so this mirrors ClippingStore's sequence. A leftover .bak is restored on the
// next read via recoverFromBackup().
using WriteFn = bool (*)(FsFile& out, void* ctx);
bool writeFileAtomic(const char* path, WriteFn writer, void* ctx);
void recoverFromBackup(const char* path);

// Reads one line (without CR/LF) into buf, truncating overlong lines but
// still consuming them. Returns the stored length, or -1 at EOF.
int readLine(FsFile& file, char* buf, size_t cap);

bool writeText(FsFile& out, const char* text);

// Little-endian fields in the tools' binary cache files.
inline uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
inline uint32_t le32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}
inline void putLe16(uint8_t* p, const uint16_t v) {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
}
inline void putLe32(uint8_t* p, const uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
bool writeJsonString(FsFile& out, const char* text);

// Minimal pull tokenizer for JSON read straight from the SD card, so large
// files (flashcard decks, SRS state) are never loaded into RAM. Strings are
// unescaped (including \uXXXX) into a caller buffer and truncated to fit.
class JsonReader {
 public:
  enum class Token : uint8_t {
    End,
    Error,
    ObjectStart,
    ObjectEnd,
    ArrayStart,
    ArrayEnd,
    Colon,
    Comma,
    String,
    Literal
  };

  explicit JsonReader(FsFile& file) : file_(file), bufStart_(static_cast<uint32_t>(file.position())) {}
  Token next(char* text, size_t cap);
  // File offset of the first byte of the most recent token.
  uint32_t tokenOffset() const { return tokenOffset_; }
  // Skips the value that starts with `first` (already consumed).
  bool skipValue(Token first);
  // Repositions reading at an absolute file offset.
  bool seek(uint32_t offset);

 private:
  int peekByte();
  int readByte();
  bool readString(char* text, size_t cap);

  FsFile& file_;
  uint8_t buf_[64] = {};
  uint16_t len_ = 0;
  uint16_t pos_ = 0;
  uint32_t bufStart_ = 0;
  uint32_t tokenOffset_ = 0;
};

// Streams objects of a top-level JSON array (or {"cards":[...]}) and reports
// byte offsets. Used to build flashcard indexes without loading the deck.
bool findFirstArray(JsonReader& reader);

}  // namespace tools
