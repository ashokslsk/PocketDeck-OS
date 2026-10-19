#include "RecentBooksStore.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>
#include <Xtc.h>

#include <algorithm>
#include <iterator>
#include <utility>

namespace {
constexpr uint8_t RECENT_BOOKS_FILE_VERSION = 3;
constexpr char RECENT_BOOKS_FILE_BIN[] = "/.pocketdeck-os/recent.bin";
constexpr char RECENT_BOOKS_FILE_BAK[] = "/.pocketdeck-os/recent.bin.bak";
constexpr char HISTORY_TMP[] = "/.pocketdeck-os/recent_more.tmp";
constexpr size_t kMaxHistoryLine = 768;

// One history line: path, title, author, cover, cover state, tab separated.
void writeField(HalFile& f, const std::string& v) {
  for (const char c : v) f.write(static_cast<uint8_t>(c == '\t' || c == '\n' || c == '\r' ? ' ' : c));
}

void writeHistoryLine(HalFile& f, const RecentBook& b) {
  writeField(f, b.path);
  f.write('\t');
  writeField(f, b.title);
  f.write('\t');
  writeField(f, b.author);
  f.write('\t');
  writeField(f, b.coverBmpPath);
  f.write('\t');
  f.write(static_cast<uint8_t>('0' + static_cast<uint8_t>(b.coverState)));
  f.write('\n');
}

bool parseHistoryLine(const std::string& line, RecentBook& out) {
  size_t fieldStart = 0;
  std::string* fields[] = {&out.path, &out.title, &out.author, &out.coverBmpPath};
  for (std::string* field : fields) {
    const size_t tab = line.find('\t', fieldStart);
    if (tab == std::string::npos) return false;
    field->assign(line, fieldStart, tab - fieldStart);
    fieldStart = tab + 1;
  }
  FsHelpers::upgradeSavedDataPath(out.coverBmpPath);
  out.coverState = fieldStart < line.size() && line[fieldStart] == '1' ? RecentBook::CoverState::Missing
                                                                       : RecentBook::CoverState::Unknown;
  return !out.path.empty();
}

// Reads the history file line by line through a small buffer.
class HistoryReader {
 public:
  explicit HistoryReader(HalFile& f) : f_(f) {}
  bool next(RecentBook& out) {
    std::string line;
    while (true) {
      if (pos_ >= len_) {
        const int n = f_.read(buf_, sizeof(buf_));
        if (n <= 0) {
          if (line.empty()) return false;
          if (parseHistoryLine(line, out)) return true;
          return false;
        }
        len_ = static_cast<size_t>(n);
        pos_ = 0;
      }
      const char c = buf_[pos_++];
      if (c == '\n') {
        if (parseHistoryLine(line, out)) return true;
        line.clear();
        continue;
      }
      if (line.size() < kMaxHistoryLine) line += c;
    }
  }

 private:
  HalFile& f_;
  char buf_[128];
  size_t pos_ = 0;
  size_t len_ = 0;
};
}  // namespace

bool RecentBooksStore::rewriteHistory(const RecentBook* prepend, const std::string& dropPath, RecentBook* popFirst,
                                      const std::string& renameFrom, const RecentBook* renameTo) {
  const bool hasFile = Storage.exists(getHistoryPath());
  if (!hasFile && prepend == nullptr) return false;
  HalFile out;
  if (!Storage.openFileForWrite("RBS", HISTORY_TMP, out)) {
    LOG_ERR("RBS", "Cannot write recent-books history");
    return false;
  }
  int written = 0;
  bool hit = false;
  if (prepend != nullptr) {
    writeHistoryLine(out, *prepend);
    ++written;
  }
  if (hasFile) {
    HalFile in;
    if (Storage.openFileForRead("RBS", getHistoryPath(), in)) {
      HistoryReader reader(in);
      RecentBook b;
      while (written < MAX_HISTORY_BOOKS && reader.next(b)) {
        if (!dropPath.empty() && b.path == dropPath) {
          hit = true;
          continue;
        }
        if (prepend != nullptr && b.path == prepend->path) continue;
        if (popFirst != nullptr && !hit && popFirst->path.empty()) {
          *popFirst = std::move(b);
          hit = true;
          continue;
        }
        if (renameTo != nullptr && b.path == renameFrom) {
          writeHistoryLine(out, *renameTo);
          hit = true;
        } else {
          writeHistoryLine(out, b);
        }
        ++written;
      }
      in.close();
    }
  }
  out.close();
  Storage.remove(getHistoryPath());
  if (written == 0) {
    Storage.remove(HISTORY_TMP);
  } else if (!Storage.rename(HISTORY_TMP, getHistoryPath())) {
    LOG_ERR("RBS", "Cannot replace recent-books history");
  }
  return hit;
}

bool RecentBooksStore::loadPage(std::vector<RecentBook>& out, const size_t offset, const size_t count,
                                const bool keepCovers) const {
  ensureLoaded();
  out.clear();
  out.reserve(count);
  size_t index = 0;
  for (const RecentBook& b : recentBooks) {
    if (index >= offset + count) return true;
    if (index++ >= offset) out.push_back(b);
  }
  HalFile in;
  if (!Storage.exists(getHistoryPath()) || !Storage.openFileForRead("RBS", getHistoryPath(), in)) return false;
  HistoryReader reader(in);
  RecentBook b;
  bool more = false;
  while (reader.next(b)) {
    if (index >= offset + count) {
      more = true;
      break;
    }
    if (index++ >= offset) {
      if (!keepCovers) b.coverBmpPath.clear();
      out.push_back(std::move(b));
    }
    b = RecentBook{};
  }
  in.close();
  return more;
}

void RecentBooksStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["books"].to<JsonArray>();
  for (const auto& book : recentBooks) {
    JsonObject obj = arr.add<JsonObject>();
    obj["path"] = book.path;
    obj["title"] = book.title;
    obj["author"] = book.author;
    obj["coverBmpPath"] = book.coverBmpPath;
    obj["coverState"] = static_cast<uint8_t>(book.coverState);
  }
}

bool RecentBooksStore::fromJson(JsonVariantConst doc) {
  // Tolerate a missing/invalid 'books' key (treat as empty list); only a
  // JSON parse error is fatal. A null JsonArray iterates zero times.
  recentBooks.clear();
  JsonArrayConst arr = doc["books"].as<JsonArrayConst>();
  overflow_.clear();
  recentBooks.reserve(std::min(arr.size(), static_cast<size_t>(MAX_RECENT_BOOKS)));
  for (JsonObjectConst obj : arr) {
    if (recentBooks.size() + overflow_.size() >= static_cast<size_t>(MAX_TOTAL_BOOKS)) break;
    RecentBook book;
    book.path = obj["path"] | "";
    book.title = obj["title"] | "";
    book.author = obj["author"] | "";
    book.coverBmpPath = obj["coverBmpPath"] | "";
    // Cover paths saved under an older data layout (/.crosspoint, or book
    // folders directly under /.pocketdeck-os) point at the current one.
    FsHelpers::upgradeSavedDataPath(book.coverBmpPath);
    const int storedCoverState = obj["coverState"] | 0;
    if (storedCoverState == static_cast<int>(RecentBook::CoverState::Missing)) {
      book.coverState = RecentBook::CoverState::Missing;
    }
    if (recentBooks.size() < static_cast<size_t>(MAX_RECENT_BOOKS)) {
      recentBooks.push_back(book);
    } else {
      overflow_.push_back(book);
    }
  }

  return true;
}

bool RecentBooksStore::saveToFile() const {
  std::lock_guard<std::mutex> lock(storeMutex);
  JsonDocument doc;
  toJson(doc);
  return PersistableStoreBase::writeDocToFileAtomically(getFilePath(), doc);
}

void RecentBooksStore::addOrUpdateBook(const std::string& path, const std::string& title, const std::string& author,
                                       const std::string& coverBmpPath, const RecentBook::CoverState coverState) {
  ensureLoaded();

  // No-op write suppression adapted from Sichroteph/YACP commit
  // 20af8aee8d3e1d560456753b08d1f52e5488621f (MIT), preserving CrossInk's
  // cover-state metadata.
  bool changed = pruneMissing();

  // Remove existing entry if present
  auto it =
      std::find_if(recentBooks.begin(), recentBooks.end(), [&](const RecentBook& book) { return book.path == path; });
  if (it != recentBooks.end()) {
    changed = changed || it->title != title || it->author != author || it->coverBmpPath != coverBmpPath ||
              it->coverState != coverState;
    it->title = title;
    it->author = author;
    it->coverBmpPath = coverBmpPath;
    it->coverState = coverState;
    if (it != recentBooks.begin()) {
      RecentBook book = std::move(*it);
      recentBooks.erase(it);
      recentBooks.insert(recentBooks.begin(), std::move(book));
      changed = true;
    }
  } else {
    recentBooks.insert(recentBooks.begin(), {path, title, author, coverBmpPath, coverState});
    changed = true;
    // The book may come back from the SD history; the oldest RAM entry moves
    // there instead.
    if (recentBooks.size() > MAX_RECENT_BOOKS) {
      const RecentBook evicted = std::move(recentBooks.back());
      recentBooks.pop_back();
      rewriteHistory(&evicted, path, nullptr);
    } else if (Storage.exists(getHistoryPath())) {
      rewriteHistory(nullptr, path, nullptr);
    }
  }
  if (changed) saveToFile();
}

bool RecentBooksStore::updateBook(const std::string& path, const std::string& title, const std::string& author,
                                  const std::string& coverBmpPath, const RecentBook::CoverState coverState) {
  ensureLoaded();

  auto it =
      std::find_if(recentBooks.begin(), recentBooks.end(), [&](const RecentBook& book) { return book.path == path; });
  if (it == recentBooks.end()) {
    return false;
  }
  RecentBook& book = *it;
  if (book.title == title && book.author == author && book.coverBmpPath == coverBmpPath &&
      book.coverState == coverState) {
    return true;
  }
  book.title = title;
  book.author = author;
  book.coverBmpPath = coverBmpPath;
  book.coverState = coverState;
  saveToFile();
  return true;
}

bool RecentBooksStore::removeByPath(const std::string& path) {
  ensureLoaded();

  auto it =
      std::find_if(recentBooks.begin(), recentBooks.end(), [&](const RecentBook& book) { return book.path == path; });
  if (it == recentBooks.end()) {
    return rewriteHistory(nullptr, path, nullptr);
  }
  recentBooks.erase(it);
  // Refill RAM from the newest history entry so Home keeps a full list.
  RecentBook refill;
  if (rewriteHistory(nullptr, std::string(), &refill) && !refill.path.empty()) {
    recentBooks.push_back(std::move(refill));
  }
  if (!saveToFile()) {
    LOG_ERR("RBS", "Failed to persist removal of recent book: %s", path.c_str());
  }
  return true;
}

bool RecentBooksStore::updatePath(const std::string& oldPath, const std::string& newPath,
                                  const std::string& oldCachePath, const std::string& newCachePath) {
  ensureLoaded();

  auto it = std::find_if(recentBooks.begin(), recentBooks.end(),
                         [&](const RecentBook& book) { return book.path == oldPath; });
  if (it == recentBooks.end()) {
    if (Storage.exists(getHistoryPath())) {
      // Rewrite the matching history line (cover path follows the cache dir).
      HalFile in;
      RecentBook moved;
      bool found = false;
      if (Storage.openFileForRead("RBS", getHistoryPath(), in)) {
        HistoryReader reader(in);
        RecentBook b;
        while (reader.next(b)) {
          if (b.path == oldPath) {
            moved = std::move(b);
            found = true;
            break;
          }
        }
        in.close();
      }
      if (found) {
        moved.path = newPath;
        if (!oldCachePath.empty() && moved.coverBmpPath.rfind(oldCachePath, 0) == 0) {
          moved.coverBmpPath = newCachePath + moved.coverBmpPath.substr(oldCachePath.size());
        }
        rewriteHistory(nullptr, std::string(), nullptr, oldPath, &moved);
      }
    }
    return true;
  }
  const RecentBook original = *it;
  it->path = newPath;
  if (!oldCachePath.empty() && !it->coverBmpPath.empty() && it->coverBmpPath.rfind(oldCachePath, 0) == 0) {
    it->coverBmpPath = newCachePath + it->coverBmpPath.substr(oldCachePath.size());
  }
  if (!saveToFile()) {
    *it = original;
    LOG_ERR("RBS", "Failed to persist recent book path update: %s -> %s", oldPath.c_str(), newPath.c_str());
    return false;
  }
  return true;
}

bool RecentBooksStore::isMissing(const RecentBook& book) { return !Storage.exists(book.path.c_str()); }

bool RecentBooksStore::pruneMissing() {
  ensureLoaded();

  const size_t before = recentBooks.size();
  recentBooks.erase(std::remove_if(recentBooks.begin(), recentBooks.end(), &isMissing), recentBooks.end());
  return recentBooks.size() != before;
}

RecentBook RecentBooksStore::getDataFromBook(std::string path) const {
  std::string lastBookFileName = "";
  const size_t lastSlash = path.find_last_of('/');
  if (lastSlash != std::string::npos) {
    lastBookFileName = path.substr(lastSlash + 1);
  }

  // If epub, try to load the metadata for title/author and cover.
  // Use buildIfMissing=false to avoid heavy epub loading on boot; getTitle()/getAuthor() may be
  // blank until the book is opened, and entries with missing title are omitted from recent list.
  if (FsHelpers::hasEpubExtension(lastBookFileName)) {
    Epub epub(path, "/.pocketdeck-os");
    epub.load(false, true, Epub::XLocationLoadMode::Skip);
    return RecentBook{path, epub.getTitle(), epub.getAuthor(), epub.getThumbBmpPath()};
  } else if (FsHelpers::hasXtcExtension(lastBookFileName)) {
    // Handle XTC file
    Xtc xtc(path, "/.pocketdeck-os");
    if (xtc.load()) {
      return RecentBook{path, xtc.getTitle(), xtc.getAuthor(), xtc.getThumbBmpPath()};
    }
  } else if (FsHelpers::hasTxtExtension(lastBookFileName) || FsHelpers::hasMarkdownExtension(lastBookFileName)) {
    return RecentBook{path, lastBookFileName, "", ""};
  }
  return RecentBook{path, "", "", ""};
}

bool RecentBooksStore::loadFromFile() {
  const bool hasStoreFile = Storage.exists(getFilePath());
  if (PersistableStore<RecentBooksStore>::loadFromFile()) {
    if (!overflow_.empty()) {
      // Oldest first, each prepended, so the history keeps newest-first order.
      for (auto it = overflow_.rbegin(); it != overflow_.rend(); ++it) rewriteHistory(&*it, std::string(), nullptr);
      overflow_.clear();
      overflow_.shrink_to_fit();
      saveToFile();
      LOG_DBG("RBS", "Moved older recent books to %s", getHistoryPath());
    }
    return true;
  }
  if (hasStoreFile) {
    return false;
  }

  if (Storage.exists(RECENT_BOOKS_FILE_BIN)) {
    if (loadFromBinaryFile()) {
      saveToFile();
      Storage.rename(RECENT_BOOKS_FILE_BIN, RECENT_BOOKS_FILE_BAK);
      LOG_DBG("RBS", "Migrated recent.bin to recent.json");
      return true;
    }
  }

  return false;
}

bool RecentBooksStore::loadFromBinaryFile() {
  HalFile inputFile;
  if (!Storage.openFileForRead("RBS", RECENT_BOOKS_FILE_BIN, inputFile)) {
    return false;
  }

  uint8_t version;
  serialization::readPod(inputFile, version);
  if (version == 1 || version == 2) {
    // Old version, just read paths
    uint8_t count;
    serialization::readPod(inputFile, count);
    recentBooks.clear();
    recentBooks.reserve(count);
    for (uint8_t i = 0; i < count; i++) {
      std::string path;
      serialization::readString(inputFile, path);

      // load book to get missing data
      RecentBook book = getDataFromBook(path);
      if (book.title.empty() && book.author.empty() && version == 2) {
        // Fall back to loading what we can from the store
        std::string title, author;
        serialization::readString(inputFile, title);
        serialization::readString(inputFile, author);
        recentBooks.push_back({path, title, author, ""});
      } else {
        recentBooks.push_back(book);
      }
    }
  } else if (version == RECENT_BOOKS_FILE_VERSION) {
    uint8_t count;
    serialization::readPod(inputFile, count);

    recentBooks.clear();
    recentBooks.reserve(count);
    uint8_t omitted = 0;

    for (uint8_t i = 0; i < count; i++) {
      std::string path, title, author, coverBmpPath;
      serialization::readString(inputFile, path);
      serialization::readString(inputFile, title);
      serialization::readString(inputFile, author);
      serialization::readString(inputFile, coverBmpPath);
      FsHelpers::upgradeSavedDataPath(coverBmpPath);

      // Omit books with missing title (e.g. saved before metadata was available)
      if (title.empty()) {
        omitted++;
        continue;
      }

      recentBooks.push_back({path, title, author, coverBmpPath});
    }

    if (omitted > 0) {
      LOG_DBG("RBS", "Omitted %u recent book(s) with missing title", omitted);
      return true;
    }
  } else {
    LOG_ERR("RBS", "Deserialization failed: Unknown version %u", version);
    return false;
  }

  return true;
}
