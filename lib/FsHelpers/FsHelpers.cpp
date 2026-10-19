#include "FsHelpers.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <vector>

namespace FsHelpers {

bool directoryIterationFailed(const HalFile& directory) {
#ifdef SIMULATOR
  // The host filesystem adapter cannot surface an SdFat read error.
  return directory.allocationFailed();
#else
  return directory.allocationFailed() || directory.iterationFailed();
#endif
}

bool directoryCanBeEnumerated(const char* path) {
  HalFile directory = Storage.open(path);
  if (!directory || !directory.isDirectory()) {
    directory.close();
    return false;
  }

  for (HalFile entry = directory.openNextFile(); entry; entry = directory.openNextFile()) {
    entry.close();
    yield();
  }

  const bool complete = !directoryIterationFailed(directory);
  if (!complete) {
    LOG_ERR("FS", "Directory listing failed before EOF: %s", path);
  }
  directory.close();
  return complete;
}

DirectoryEntryVisibility directoryEntryVisibility(const char* directoryPath, const char* entryPath) {
  if (!directoryPath || !entryPath || entryPath[0] == '\0') return DirectoryEntryVisibility::Missing;

  HalFile expectedEntry = Storage.open(entryPath);
  if (!expectedEntry) {
    return expectedEntry.allocationFailed() ? DirectoryEntryVisibility::IterationFailed
                                            : DirectoryEntryVisibility::Missing;
  }

  char expectedName[256] = {};  // FAT long filenames are at most 255 bytes.
  expectedEntry.getName(expectedName, sizeof(expectedName));
  expectedEntry.close();
  if (expectedName[0] == '\0') return DirectoryEntryVisibility::IterationFailed;

  HalFile directory = Storage.open(directoryPath);
  if (!directory || !directory.isDirectory()) {
    directory.close();
    return DirectoryEntryVisibility::IterationFailed;
  }

  char name[256];  // FAT long filenames are at most 255 bytes.
  for (HalFile entry = directory.openNextFile(); entry; entry = directory.openNextFile()) {
    entry.getName(name, sizeof(name));
    entry.close();
    if (strcmp(name, expectedName) == 0) {
      directory.close();
      return DirectoryEntryVisibility::Visible;
    }
    yield();
  }

  if (directoryIterationFailed(directory)) {
    LOG_ERR("FS", "Directory listing failed before finding %s in %s", expectedName, directoryPath);
    directory.close();
    return DirectoryEntryVisibility::IterationFailed;
  }
  directory.close();
  return DirectoryEntryVisibility::Missing;
}

bool resolveRootDirectoryIgnoreCase(const char* expectedPath, char* resolvedPath, const size_t resolvedPathSize) {
  if (!expectedPath || expectedPath[0] != '/' || expectedPath[1] == '\0' || strchr(expectedPath + 1, '/') ||
      !resolvedPath || resolvedPathSize == 0) {
    return false;
  }

  HalFile exact = Storage.open(expectedPath);
  if (exact && exact.isDirectory()) {
    const int written = snprintf(resolvedPath, resolvedPathSize, "%s", expectedPath);
    exact.close();
    return written > 0 && static_cast<size_t>(written) < resolvedPathSize;
  }
  exact.close();

  HalFile root = Storage.open("/");
  if (!root || !root.isDirectory()) {
    root.close();
    return false;
  }

  char name[256];  // FAT long filenames are at most 255 bytes.
  const char* expectedName = expectedPath + 1;
  for (HalFile entry = root.openNextFile(); entry; entry = root.openNextFile()) {
    const bool isDirectory = entry.isDirectory();
    entry.getName(name, sizeof(name));
    entry.close();
    if (!isDirectory || strcasecmp(name, expectedName) != 0) continue;

    const int written = snprintf(resolvedPath, resolvedPathSize, "/%s", name);
    root.close();
    return written > 0 && static_cast<size_t>(written) < resolvedPathSize;
  }

  root.close();
  return false;
}

namespace {
bool isHexDigit(const char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

uint8_t hexValue(const char c) {
  if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
  if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + (c - 'a'));
  return static_cast<uint8_t>(10 + (c - 'A'));
}
}  // namespace

std::string decodeUriEscapes(const std::string& path) {
  std::string decoded;
  decoded.reserve(path.size());

  for (size_t i = 0; i < path.size(); i++) {
    if (path[i] == '%' && i + 2 < path.size() && isHexDigit(path[i + 1]) && isHexDigit(path[i + 2])) {
      const uint8_t value = static_cast<uint8_t>((hexValue(path[i + 1]) << 4) | hexValue(path[i + 2]));
      decoded += static_cast<char>(value);
      i += 2;
      continue;
    }

    decoded += path[i];
  }

  return decoded;
}

std::string normalisePath(const std::string& path) {
  std::vector<std::string_view> components;
  components.reserve(8);  // Eight nested folders is more than we might expect

  size_t start = 0;
  for (size_t i = 0; i <= path.length(); ++i) {
    if (i == path.length() || path[i] == '/') {
      if (i > start) {
        std::string_view component(path.data() + start, i - start);
        if (component == "..") {
          if (!components.empty()) {
            components.pop_back();
          }
        } else {
          components.push_back(component);
        }
      }
      start = i + 1;
    }
  }

  if (components.empty()) {
    return "";
  }

  size_t total_len = 0;
  for (const auto& c : components) {
    total_len += c.length() + 1;
  }

  std::string result;
  result.reserve(total_len - 1);

  for (size_t i = 0; i < components.size(); ++i) {
    if (i > 0) {
      result += '/';
    }
    result.append(components[i].data(), components[i].length());
  }

  return result;
}

bool naturalLess(const std::string& str1, const std::string& str2) {
  return naturalCompare(str1.c_str(), str2.c_str()) < 0;
}

void sortFileList(std::vector<std::string>& strs) {
  std::sort(begin(strs), end(strs), [](const std::string& str1, const std::string& str2) {
    const bool isDir1 = str1.back() == '/';
    const bool isDir2 = str2.back() == '/';
    if (isDir1 != isDir2) return isDir1;

    return naturalLess(str1, str2);
  });
}

bool checkFileExtension(std::string_view fileName, const char* extension) {
  const size_t extLen = strlen(extension);
  if (fileName.length() < extLen) {
    return false;
  }

  const size_t offset = fileName.length() - extLen;
  for (size_t i = 0; i < extLen; i++) {
    if (tolower(static_cast<unsigned char>(fileName[offset + i])) !=
        tolower(static_cast<unsigned char>(extension[i]))) {
      return false;
    }
  }
  return true;
}

bool hasJpgExtension(std::string_view fileName) {
  return checkFileExtension(fileName, ".jpg") || checkFileExtension(fileName, ".jpeg");
}

bool hasPngExtension(std::string_view fileName) { return checkFileExtension(fileName, ".png"); }

bool hasBmpExtension(std::string_view fileName) { return checkFileExtension(fileName, ".bmp"); }

bool hasGifExtension(std::string_view fileName) { return checkFileExtension(fileName, ".gif"); }

bool hasEpubExtension(std::string_view fileName) { return checkFileExtension(fileName, ".epub"); }

bool hasXtcExtension(std::string_view fileName) {
  return checkFileExtension(fileName, ".xtc") || checkFileExtension(fileName, ".xtch");
}

bool hasTxtExtension(std::string_view fileName) { return checkFileExtension(fileName, ".txt"); }

bool hasMarkdownExtension(std::string_view fileName) { return checkFileExtension(fileName, ".md"); }

bool hasCssExtension(std::string_view fileName) { return checkFileExtension(fileName, ".css"); }

std::string extractFolderPath(const std::string& filePath) {
  const auto lastSlash = filePath.find_last_of('/');
  if (lastSlash == std::string::npos || lastSlash == 0) {
    return "/";
  }
  return filePath.substr(0, lastSlash);
}

bool isSafePathComponent(const std::string_view name) {
  return !name.empty() && name.find_first_of("/\\") == std::string_view::npos && name != "." && name != "..";
}

void sanitizePathComponentForFat32(const char* input, char* output, size_t maxLen) {
  if (maxLen == 0) {
    return;
  }

  size_t i = 0;
  for (; i < maxLen - 1 && input[i] != '\0'; i++) {
    const char c = input[i];
    if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' ||
        c == ' ' || (c > 0x00 && c <= 0x1f)) {
      output[i] = '-';
    } else {
      output[i] = c;
    }
  }
  output[i] = '\0';
}

}  // namespace FsHelpers

namespace FsHelpers {

namespace {
constexpr char kDataDirPrefix[] = "/.pocketdeck-os/";
constexpr const char* kBookCachePrefixes[] = {"epub_", "xtc_", "txt_"};

// Length of the book-cache name at `name` (prefix + digits), 0 if none.
size_t bookCacheNameLength(const char* name) {
  for (const char* prefix : kBookCachePrefixes) {
    const size_t plen = strlen(prefix);
    if (strncmp(name, prefix, plen) != 0) continue;
    size_t n = plen;
    while (name[n] >= '0' && name[n] <= '9') ++n;
    return n > plen ? n : 0;
  }
  return 0;
}

void shardName(const uint64_t id, char shard[3]) {
  snprintf(shard, 3, "%02x", static_cast<unsigned>(id % kBookCacheShards));
}

// Moves one book folder (by name) from `fromDir` into its shard. Never
// overwrites: if the destination exists the source stays where it is.
bool moveBookFolderToShard(const std::string& fromDir, const char* name) {
  char shard[3];
  if (!bookCacheShardOf(name, shard)) return false;
  const std::string shardDir = std::string(kBookCacheDir) + "/" + shard;
  const std::string to = shardDir + "/" + name;
  if (Storage.exists(to.c_str())) return false;
  Storage.mkdir(shardDir.c_str());
  const std::string from = fromDir + "/" + name;
  if (!Storage.rename(from.c_str(), to.c_str())) {
    LOG_ERR("FSH", "Could not move %s into %s", from.c_str(), shardDir.c_str());
    return false;
  }
  return true;
}

// Moves every book folder directly inside `dir` into its shard, in batches
// (renaming while iterating a directory is not safe), with bounded memory.
void shardBookFoldersIn(const char* dir) {
  char name[128];
  for (int pass = 0; pass < 4096; ++pass) {
    std::vector<std::string> batch;
    batch.reserve(32);
    HalFile root = Storage.open(dir);
    if (!root || !root.isDirectory()) {
      if (root) root.close();
      return;
    }
    for (HalFile entry = root.openNextFile(); entry && batch.size() < 32; entry = root.openNextFile()) {
      entry.getName(name, sizeof(name));
      const bool isDir = entry.isDirectory();
      entry.close();
      if (isDir && bookCacheNameLength(name) == strlen(name)) batch.emplace_back(name);
    }
    root.close();
    if (batch.empty()) return;
    size_t moved = 0;
    for (const auto& folder : batch) {
      if (moveBookFolderToShard(dir, folder.c_str())) ++moved;
    }
    LOG_INF("FSH", "Moved %u book cache folders from %s into shards", static_cast<unsigned>(moved), dir);
    if (moved == 0) return;  // only unmovable ones left; do not loop on them
  }
}
}  // namespace

uint32_t entryHash(const char* name, const size_t len, const bool isDir) {
  if (!name || len == 0 || name[0] == '.') return 0;
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < len; ++i) {
    h ^= static_cast<uint8_t>(name[i]);
    h *= 16777619u;
  }
  h ^= isDir ? 0x9E3779B9u : 0u;
  // Spread before summing so swapped/duplicated names do not cancel.
  h ^= h >> 16;
  h *= 0x7FEB352Du;
  h ^= h >> 15;
  return h | 1u;  // never zero: every counted entry changes the sum
}

void recordBookSource(const std::string& cachePath, const std::string& bookPath) {
  if (cachePath.empty() || bookPath.empty()) return;
  std::string current;
  if (readBookSource(cachePath, current) && current == bookPath) return;
  const std::string path = cachePath + "/source.path";
  HalFile file;
  if (!Storage.openFileForWrite("FSH", path, file)) return;
  file.write(reinterpret_cast<const uint8_t*>(bookPath.data()), bookPath.size());
  file.close();
}

bool readBookSource(const std::string& cachePath, std::string& bookPath) {
  const std::string path = cachePath + "/source.path";
  HalFile file;
  if (!Storage.exists(path.c_str()) || !Storage.openFileForRead("FSH", path, file)) return false;
  char buf[512];
  const int n = file.read(buf, sizeof(buf) - 1);
  file.close();
  if (n <= 0) return false;
  bookPath.assign(buf, static_cast<size_t>(n));
  return bookPath[0] == '/';
}

std::string bookCacheFolder(const std::string& dataDir, const char* prefix, const uint64_t id) {
  char shard[3];
  shardName(id, shard);
  return dataDir + "/" + kBookCacheDirName + "/" + shard + "/" + prefix + std::to_string(id);
}

bool bookCacheShardOf(const char* folderName, char shard[3]) {
  const size_t len = bookCacheNameLength(folderName);
  if (len == 0) return false;
  const char* digits = strchr(folderName, '_') + 1;
  uint64_t id = 0;
  for (const char* p = digits; p < folderName + len; ++p) id = id * 10 + static_cast<uint64_t>(*p - '0');
  shardName(id, shard);
  return true;
}

bool upgradeSavedDataPath(std::string& path) {
  bool changed = false;
  static constexpr char kLegacyDataDir[] = "/.crosspoint/";
  if (path.rfind(kLegacyDataDir, 0) == 0) {
    path.replace(0, sizeof(kLegacyDataDir) - 1, kDataDirPrefix);
    changed = true;
  }
  // A book folder directly under /.pocketdeck-os/ or directly under
  // book_reading_cache/ (before sharding) moves into its shard.
  const std::string flatCache = std::string(kBookCacheDir) + "/";
  size_t nameAt = std::string::npos;
  if (path.rfind(flatCache, 0) == 0 && bookCacheNameLength(path.c_str() + flatCache.size()) > 0) {
    nameAt = flatCache.size();
  } else if (path.rfind(kDataDirPrefix, 0) == 0 && bookCacheNameLength(path.c_str() + sizeof(kDataDirPrefix) - 1) > 0) {
    nameAt = sizeof(kDataDirPrefix) - 1;
  }
  if (nameAt != std::string::npos) {
    char shard[3];
    bookCacheShardOf(path.c_str() + nameAt, shard);
    std::string prefix = flatCache + shard + "/";
    path = prefix + path.substr(nameAt);
    changed = true;
  }
  return changed;
}

void migrateDataLayout() {
  if (!Storage.exists("/.pocketdeck-os")) return;
  // Written once the layout is current, so later boots skip the folder scans.
  static constexpr char kLayoutMarker[] = "/.pocketdeck-os/system_cache/layout-v2";
  if (Storage.exists(kLayoutMarker)) return;
  Storage.mkdir(kBookCacheDir);
  Storage.mkdir(kSystemCacheDir);

  // App caches that used to sit at the top level.
  static constexpr const char* kSystemCaches[] = {"fileindex",        "card_stats.bin", "home_carousel_cache.bin",
                                                   "font-catalog.bin", "image_view.pxc", "sleep_frame.bin",
                                                   "sleep-image-index"};
  for (const char* name : kSystemCaches) {
    const std::string from = std::string(kDataDirPrefix) + name;
    const std::string to = std::string(kSystemCacheDir) + "/" + name;
    if (Storage.exists(from.c_str()) && !Storage.exists(to.c_str()) && !Storage.rename(from.c_str(), to.c_str())) {
      LOG_ERR("FSH", "Could not move %s into system_cache", from.c_str());
    }
  }

  // Book folders from older layouts: top level, then unsharded cache root.
  shardBookFoldersIn("/.pocketdeck-os");
  shardBookFoldersIn(kBookCacheDir);

  HalFile marker;
  if (Storage.openFileForWrite("FSH", kLayoutMarker, marker)) {
    marker.write(reinterpret_cast<const uint8_t*>("2"), 1);
    marker.close();
  }
}

}  // namespace FsHelpers
