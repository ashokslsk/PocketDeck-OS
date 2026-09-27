#pragma once

// In-memory SD card for tools_core tests. Mirrors the FAT semantics the tools
// rely on: rename() refuses to overwrite an existing file.

#include <Print.h>
#include <fcntl.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct HostFileData {
  std::vector<uint8_t> bytes;
};

class HalFile : public Print {
 public:
  HalFile() = default;
  explicit HalFile(std::shared_ptr<HostFileData> data) : data_(std::move(data)) {}

  int read(void* output, const size_t length) {
    if (!data_) return -1;
    const size_t start = std::min(cursor_, data_->bytes.size());
    const size_t readable = std::min(length, data_->bytes.size() - start);
    if (readable > 0) std::copy_n(data_->bytes.data() + start, readable, static_cast<uint8_t*>(output));
    cursor_ = start + readable;
    return static_cast<int>(readable);
  }
  int read() {
    uint8_t b = 0;
    return read(&b, 1) == 1 ? b : -1;
  }
  size_t write(const uint8_t value) override { return write(&value, 1); }
  size_t write(const uint8_t* input, const size_t length) override {
    if (!data_ || failWrites) return 0;
    if (cursor_ + length > data_->bytes.size()) data_->bytes.resize(cursor_ + length);
    std::copy_n(input, length, data_->bytes.begin() + static_cast<std::ptrdiff_t>(cursor_));
    cursor_ += length;
    return length;
  }
  size_t write(const void* input, const size_t length) { return write(static_cast<const uint8_t*>(input), length); }
  bool seek(const size_t pos) {
    if (!data_) return false;
    cursor_ = pos;
    return true;
  }
  size_t position() const { return cursor_; }
  bool sync() const { return static_cast<bool>(data_); }
  bool close() {
    data_.reset();
    cursor_ = 0;
    return true;
  }
  size_t fileSize() const { return data_ ? data_->bytes.size() : 0; }
  explicit operator bool() const { return static_cast<bool>(data_); }

  static inline bool failWrites = false;

 private:
  std::shared_ptr<HostFileData> data_;
  size_t cursor_ = 0;
};

using FsFile = HalFile;

class HalStorage {
 public:
  void reset() {
    files_.clear();
    dirs_.clear();
    failRenameFrom_.clear();
    HalFile::failWrites = false;
  }

  bool exists(const char* path) const { return files_.contains(path) || dirs_.contains(path); }
  bool ensureDirectoryExists(const char* path) {
    dirs_.insert(path);
    return true;
  }
  bool remove(const char* path) { return files_.erase(path) > 0; }
  bool rename(const char* from, const char* to) {
    if (failRenameFrom_.erase(from) > 0) return false;
    const auto source = files_.find(from);
    if (source == files_.end() || files_.contains(to)) return false;  // FAT: no overwrite
    files_[to] = source->second;
    files_.erase(source);
    return true;
  }
  HalFile open(const char* path, const int flags = O_RDONLY) {
    auto found = files_.find(path);
    if ((flags & O_CREAT) != 0) {
      if (found == files_.end() || (flags & O_TRUNC) != 0) {
        files_[path] = std::make_shared<HostFileData>();
      }
      return HalFile(files_[path]);
    }
    return found == files_.end() ? HalFile() : HalFile(found->second);
  }
  bool openFileForRead(const char*, const char* path, HalFile& file) {
    file = open(path);
    return static_cast<bool>(file);
  }

  // Test helpers.
  void put(const std::string& path, const std::string& content) {
    auto data = std::make_shared<HostFileData>();
    data->bytes.assign(content.begin(), content.end());
    files_[path] = std::move(data);
  }
  std::string get(const std::string& path) const {
    const auto found = files_.find(path);
    if (found == files_.end()) return {};
    return std::string(found->second->bytes.begin(), found->second->bytes.end());
  }
  void failNextRenameFrom(std::string path) { failRenameFrom_.insert(std::move(path)); }

 private:
  std::unordered_map<std::string, std::shared_ptr<HostFileData>> files_;
  std::set<std::string> dirs_;
  std::set<std::string> failRenameFrom_;
};

inline HalStorage Storage;
