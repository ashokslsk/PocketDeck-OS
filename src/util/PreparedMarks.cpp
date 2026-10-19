#include "PreparedMarks.h"

#include <Epub.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

namespace PreparedMarks {
namespace {
constexpr char kDataDir[] = "/.pocketdeck-os";
constexpr char kLayoutFile[] = "/layout.sig";
constexpr char kWholeFile[] = "/prepared.sig";

bool readU32(const std::string& path, uint32_t& out) {
  HalFile file;
  if (!Storage.exists(path.c_str()) || !Storage.openFileForRead("PMARK", path, file)) return false;
  const bool ok = file.read(&out, sizeof(out)) == static_cast<int>(sizeof(out));
  file.close();
  return ok;
}

void writeBytes(const std::string& path, const void* data, const size_t len) {
  HalFile file;
  if (!Storage.openFileForWrite("PMARK", path, file)) return;
  file.write(static_cast<const uint8_t*>(data), len);
  file.close();
}

std::string folderMarkPath(const std::string& folderPath) {
  // FNV-1a 64 of the path: one small file per prepared folder.
  uint64_t h = 1469598103934665603ULL;
  for (const char c : folderPath) {
    h ^= static_cast<uint8_t>(c);
    h *= 1099511628211ULL;
  }
  char name[40];
  snprintf(name, sizeof(name), "/%016llx.mark", static_cast<unsigned long long>(h));
  return std::string(kFolderMarksDir) + name;
}

struct FolderMark {
  uint32_t fingerprint;
  uint32_t entries;
};
}  // namespace

BookState bookState(const std::string& epubPath) {
  if (!Epub::hasCache(epubPath, kDataDir)) return BookState::None;
  const std::string cache = Epub::cachePathForFilePath(epubPath, kDataDir);
  uint32_t whole = 0;
  uint32_t layout = 0;
  if (readU32(cache + kWholeFile, whole) && readU32(cache + kLayoutFile, layout) && whole == layout) {
    return BookState::Whole;
  }
  return BookState::Info;
}

void writeLayoutSignature(const std::string& cachePath, const uint32_t signature) {
  uint32_t current = 0;
  if (readU32(cachePath + kLayoutFile, current) && current == signature) return;  // unchanged: no write
  writeBytes(cachePath + kLayoutFile, &signature, sizeof(signature));
}

void writeWholeBookSignature(const std::string& cachePath, const uint32_t signature) {
  writeBytes(cachePath + kWholeFile, &signature, sizeof(signature));
  writeLayoutSignature(cachePath, signature);
}

void clearWholeBookSignature(const std::string& cachePath) {
  const std::string path = cachePath + kWholeFile;
  if (Storage.exists(path.c_str())) Storage.remove(path.c_str());
}

void writeFolderMark(const std::string& folderPath, const uint32_t fingerprint, const uint32_t entries) {
  Storage.mkdir(kFolderMarksDir);
  const FolderMark mark{fingerprint, entries};
  writeBytes(folderMarkPath(folderPath), &mark, sizeof(mark));
}

bool hasFolderMark(const std::string& folderPath) { return Storage.exists(folderMarkPath(folderPath).c_str()); }

void validateFolderMark(const std::string& folderPath, const uint32_t fingerprint, const uint32_t entries) {
  const std::string path = folderMarkPath(folderPath);
  HalFile file;
  if (!Storage.exists(path.c_str()) || !Storage.openFileForRead("PMARK", path, file)) return;
  FolderMark mark{};
  const bool ok = file.read(&mark, sizeof(mark)) == static_cast<int>(sizeof(mark));
  file.close();
  if (!ok || mark.fingerprint != fingerprint || mark.entries != entries) {
    LOG_DBG("PMARK", "Folder changed since it was prepared: %s", folderPath.c_str());
    Storage.remove(path.c_str());
  }
}

void dropFolderMark(const std::string& folderPath) {
  const std::string path = folderMarkPath(folderPath);
  if (Storage.exists(path.c_str())) Storage.remove(path.c_str());
}

}  // namespace PreparedMarks
