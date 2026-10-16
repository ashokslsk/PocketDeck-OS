#include "Clipboard.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>

namespace clipboard {
namespace {
constexpr char kPath[] = "/.pocketdeck-os/clipboard.txt";

// One line per entry: line breaks and tabs inside a copy become spaces.
std::string normalize(const std::string& text) {
  std::string out;
  out.reserve(std::min(text.size(), kMaxBytes));
  bool pendingSpace = false;
  for (const char c : text) {
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      pendingSpace = !out.empty();
      continue;
    }
    if (pendingSpace) {
      out.push_back(' ');
      pendingSpace = false;
    }
    out.push_back(c);
  }
  if (out.size() > kMaxBytes) {
    size_t cut = kMaxBytes;
    while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) --cut;  // whole characters only
    out.resize(cut);
  }
  return out;
}
}  // namespace

std::vector<std::string> entries() {
  std::vector<std::string> list;
  HalFile file;
  if (!Storage.exists(kPath) || !Storage.openFileForRead("CLIP", kPath, file)) return list;
  std::string line;
  char buf[128];
  int got;
  while ((got = file.read(buf, sizeof(buf))) > 0 && list.size() < kMaxEntries) {
    for (int i = 0; i < got && list.size() < kMaxEntries; ++i) {
      if (buf[i] == '\n') {
        if (!line.empty()) list.push_back(std::move(line));
        line.clear();
      } else if (line.size() < kMaxBytes) {
        line.push_back(buf[i]);
      }
    }
  }
  if (!line.empty() && list.size() < kMaxEntries) list.push_back(std::move(line));
  file.close();
  return list;
}

std::string latest() {
  auto list = entries();
  return list.empty() ? std::string() : list.front();
}

bool add(const std::string& text) {
  const std::string value = normalize(text);
  if (value.empty()) return false;
  auto list = entries();
  list.erase(std::remove(list.begin(), list.end(), value), list.end());
  list.insert(list.begin(), value);
  if (list.size() > kMaxEntries) list.resize(kMaxEntries);

  HalFile file;
  if (!Storage.openFileForWrite("CLIP", kPath, file)) {
    LOG_ERR("CLIP", "Could not write clipboard");
    return false;
  }
  bool ok = true;
  for (const auto& entry : list) {
    ok = ok && file.write(entry.data(), entry.size()) == entry.size() && file.write("\n", 1) == 1;
  }
  file.close();
  if (!ok) LOG_ERR("CLIP", "Short write to clipboard");
  return ok;
}

}  // namespace clipboard
