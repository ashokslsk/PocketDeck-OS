#pragma once

#include <HalStorage.h>

#include <cstdint>
#include <cstdio>

#include "ToolsStore.h"

// Streaming pretty JSON writer for the /stats exports: values go straight to
// the SD card, so exports never build documents in RAM. Commas and two-space
// indentation are handled here; the first write error sticks in ok().
namespace tools {

class JsonOut {
 public:
  explicit JsonOut(FsFile& out) : out_(out) {}

  JsonOut& beginObject(const char* key = nullptr) { return open(key, '{'); }
  JsonOut& endObject() { return close('}'); }
  JsonOut& beginArray(const char* key = nullptr) { return open(key, '['); }
  JsonOut& endArray() { return close(']'); }

  JsonOut& str(const char* key, const char* value) {
    item(key);
    ok_ = ok_ && writeJsonString(out_, value != nullptr ? value : "");
    return *this;
  }
  JsonOut& num(const char* key, long value) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%ld", value);
    return raw(key, buf);
  }
  JsonOut& dec(const char* key, double value, int places = 1) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", places, value);
    return raw(key, buf);
  }
  JsonOut& boolean(const char* key, bool value) { return raw(key, value ? "true" : "false"); }
  JsonOut& null(const char* key) { return raw(key, "null"); }
  // A value that is already valid JSON.
  JsonOut& raw(const char* key, const char* json) {
    item(key);
    ok_ = ok_ && writeText(out_, json);
    return *this;
  }

  bool ok() const { return ok_ && depth_ == 0; }

 private:
  static constexpr int kMaxDepth = 12;

  void newline() {
    ok_ = ok_ && writeText(out_, "\n");
    for (int i = 0; i < depth_ && ok_; ++i) ok_ = writeText(out_, "  ");
  }
  void item(const char* key) {
    if (depth_ > 0) {
      if (count_[depth_] > 0) ok_ = ok_ && writeText(out_, ",");
      ++count_[depth_];
      newline();
    }
    if (key != nullptr) {
      ok_ = ok_ && writeJsonString(out_, key) && writeText(out_, ": ");
    }
  }
  JsonOut& open(const char* key, const char brace) {
    item(key);
    const char s[2] = {brace, '\0'};
    ok_ = ok_ && writeText(out_, s);
    if (depth_ + 1 < kMaxDepth) {
      ++depth_;
      count_[depth_] = 0;
    } else {
      ok_ = false;
    }
    return *this;
  }
  JsonOut& close(const char brace) {
    const bool hadItems = count_[depth_] > 0;
    if (depth_ > 0) --depth_;
    if (hadItems) newline();
    const char s[2] = {brace, '\0'};
    ok_ = ok_ && writeText(out_, s);
    if (depth_ == 0) ok_ = ok_ && writeText(out_, "\n");
    return *this;
  }

  FsFile& out_;
  int depth_ = 0;
  uint16_t count_[kMaxDepth] = {};
  bool ok_ = true;
};

}  // namespace tools
