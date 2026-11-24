#pragma once

#include <Logging.h>
#include <Memory.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string_view>
#include <vector>

// Chapter anchors (HTML id -> page) collected while a section is laid out.
//
// Stored packed in small fixed chunks instead of a std::vector of
// (std::string, page) pairs. The vector costs 28 bytes per anchor and grows by
// doubling, so a link-heavy chapter (a 1,000-entry table of contents) needs a
// single ~28 KB block plus the old copy while it grows -- more contiguous heap
// than the C3 has once layout is running. Here each anchor costs its id length
// plus 3 bytes, and memory grows 1 KB at a time with no copying.
class AnchorList {
 public:
  // Returns false (anchor dropped, logged) when the id is empty or memory is short.
  bool push(std::string_view id, uint16_t page) {
    if (id.empty()) return false;
    if (id.size() > UINT8_MAX) id = id.substr(0, UINT8_MAX);  // ids are never this long in practice
    const size_t need = kHeaderBytes + id.size();
    if (chunks_.empty() || chunks_.back().cap - chunks_.back().used < need) {
      const size_t cap = need > kChunkBytes ? need : kChunkBytes;
      Chunk chunk;
      chunk.data = makeUniqueNoThrow<uint8_t[]>(cap);
      if (!chunk.data) {
        LOG_ERR("ANC", "OOM: anchor chunk (%u bytes), dropping anchor", static_cast<unsigned>(cap));
        return false;
      }
      chunk.cap = static_cast<uint16_t>(cap);
      if (chunks_.size() == chunks_.capacity()) chunks_.reserve(chunks_.size() + 8);
      chunks_.push_back(std::move(chunk));
    }
    Chunk& chunk = chunks_.back();
    uint8_t* out = chunk.data.get() + chunk.used;
    out[0] = static_cast<uint8_t>(id.size());
    out[1] = static_cast<uint8_t>(page & 0xFF);
    out[2] = static_cast<uint8_t>(page >> 8);
    memcpy(out + kHeaderBytes, id.data(), id.size());
    chunk.used = static_cast<uint16_t>(chunk.used + need);
    count_++;
    return true;
  }

  // Calls fn(std::string_view id, uint16_t page) for each anchor in insertion order.
  // Stops early and returns false if fn returns false.
  template <typename Fn>
  bool forEach(Fn&& fn) const {
    for (const Chunk& chunk : chunks_) {
      size_t pos = 0;
      while (pos < chunk.used) {
        const uint8_t* in = chunk.data.get() + pos;
        const size_t len = in[0];
        const uint16_t page = static_cast<uint16_t>(in[1] | (in[2] << 8));
        if (!fn(std::string_view(reinterpret_cast<const char*>(in + kHeaderBytes), len), page)) return false;
        pos += kHeaderBytes + len;
      }
    }
    return true;
  }

  size_t size() const { return count_; }
  bool empty() const { return count_ == 0; }
  void clear() {
    chunks_.clear();
    chunks_.shrink_to_fit();
    count_ = 0;
  }

 private:
  static constexpr size_t kChunkBytes = 1024;
  static constexpr size_t kHeaderBytes = 3;  // id length (1) + page (2, little-endian)

  struct Chunk {
    std::unique_ptr<uint8_t[]> data;
    uint16_t used = 0;
    uint16_t cap = 0;
  };

  std::vector<Chunk> chunks_;
  size_t count_ = 0;
};
