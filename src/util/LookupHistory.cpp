#include "LookupHistory.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <common/FsApiConstants.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "activities/reader/ReadingStatsUtils.h"

std::string LookupHistory::filePath(const std::string& cachePath) { return cachePath + "/dictionary_history.txt"; }

static LookupHistory::Status parseStatusCode(char code) {
  switch (code) {
    case 'D':
      return LookupHistory::Status::Direct;
    case 'T':
      return LookupHistory::Status::Stem;
    case 'Y':
      return LookupHistory::Status::AltForm;
    case 'S':
      return LookupHistory::Status::Suggestion;
    default:
      return LookupHistory::Status::NotFound;
  }
}

static LookupHistory::Entry parseLine(const char* lineBuf, int lineLen) {
  LookupHistory::Entry entry;
  int separator = -1;
  for (int i = lineLen - 1; i >= 0; i--) {
    if (lineBuf[i] == '|') {
      separator = i;
      break;
    }
  }
  if (separator >= 0 && separator + 1 < lineLen) {
    entry.word = std::string(lineBuf, separator);
    entry.status = parseStatusCode(lineBuf[separator + 1]);
  } else {
    entry.word = std::string(lineBuf, lineLen);
  }
  return entry;
}

std::vector<LookupHistory::Entry> LookupHistory::readRecent(const std::string& path) {
  std::vector<Entry> entries;
  entries.reserve(MAX_VISIBLE_ENTRIES);

  HalFile file;
  if (!Storage.openFileForRead("LH", path.c_str(), file)) return entries;

  char lineBuf[256];
  int lineLen = 0;
  auto retainLine = [&]() {
    if (lineLen == 0) return;
    Entry entry = parseLine(lineBuf, lineLen);
    if (!entry.word.empty()) {
      if (entries.size() == MAX_VISIBLE_ENTRIES) entries.erase(entries.begin());
      entries.push_back(std::move(entry));
    }
    lineLen = 0;
  };

  while (file.available()) {
    const int byte = file.read();
    if (byte < 0) break;
    if (byte == '\n' || byte == '\r') {
      retainLine();
    } else if (lineLen < static_cast<int>(sizeof(lineBuf)) - 1) {
      lineBuf[lineLen++] = static_cast<char>(byte);
    }
  }
  retainLine();
  file.close();
  return entries;
}

bool LookupHistory::addWord(const std::string& cachePath, const std::string& word, Status status) {
  if (word.empty()) return false;

  const std::string path = filePath(cachePath);
  HalFile file = Storage.open(path.c_str(), O_WRONLY | O_CREAT | O_AT_END);
  if (!file) {
    LOG_ERR("LH", "Failed to open for append: %s", path.c_str());
    return false;
  }

  const char suffix[] = {'|', static_cast<char>(status), '\n'};
  const bool ok =
      file.write(word.c_str(), word.size()) == word.size() && file.write(suffix, sizeof(suffix)) == sizeof(suffix);
  file.flush();
  file.close();
  if (!ok) LOG_ERR("LH", "Failed to append history: %s", path.c_str());
  appendGlobal(cachePath, word, status);
  return ok;
}

namespace {
constexpr char kGlobalPath[] = "/.pocketdeck-os/lookup_history_all.txt";
constexpr size_t kGlobalMaxBytes = 64u * 1024u;

// Keeps the newest half of an over-long global log (whole lines only).
void trimGlobalIfLarge() {
  HalFile in;
  if (!Storage.openFileForRead("LH", kGlobalPath, in)) return;
  const size_t size = in.fileSize();
  if (size < kGlobalMaxBytes) {
    in.close();
    return;
  }
  const std::string tmp = std::string(kGlobalPath) + ".tmp";
  HalFile out;
  if (!Storage.openFileForWrite("LH", tmp.c_str(), out)) {
    in.close();
    return;
  }
  in.seek(size - kGlobalMaxBytes / 2);
  bool started = false;  // skip the partial first line
  char buf[256];
  while (in.available()) {
    const int n = in.read(reinterpret_cast<uint8_t*>(buf), sizeof(buf));
    if (n <= 0) break;
    int from = 0;
    if (!started) {
      const char* nl = static_cast<const char*>(memchr(buf, '\n', static_cast<size_t>(n)));
      if (!nl) continue;
      from = static_cast<int>(nl - buf) + 1;
      started = true;
    }
    out.write(reinterpret_cast<const uint8_t*>(buf + from), static_cast<size_t>(n - from));
  }
  in.close();
  out.close();
  Storage.remove(kGlobalPath);
  if (!Storage.rename(tmp.c_str(), kGlobalPath)) LOG_ERR("LH", "Failed to replace trimmed global history");
}

// Streams the global log line by line: fn(cachePath, when, word) per entry.
template <typename Fn>
void forEachGlobalLine(Fn&& fn) {
  HalFile file;
  if (!Storage.openFileForRead("LH", kGlobalPath, file)) return;
  char line[320];
  int len = 0;
  const auto flush = [&]() {
    if (len == 0) return;
    line[len] = '\0';
    // cache|when|word|S (cache and when have no '|'; the word may).
    char* first = static_cast<char*>(memchr(line, '|', static_cast<size_t>(len)));
    char* second = first ? strchr(first + 1, '|') : nullptr;
    char* last = strrchr(line, '|');
    if (first && second && last && last > second) {
      *first = '\0';
      *second = '\0';
      *last = '\0';
      if (second[1] != '\0') fn(line, first + 1, second + 1);
    }
    len = 0;
  };
  char buf[256];
  while (file.available()) {
    const int n = file.read(reinterpret_cast<uint8_t*>(buf), sizeof(buf));
    if (n <= 0) break;
    for (int i = 0; i < n; ++i) {
      if (buf[i] == '\n' || buf[i] == '\r') {
        flush();
      } else if (len < static_cast<int>(sizeof(line)) - 1) {
        line[len++] = buf[i];
      }
    }
  }
  flush();
  file.close();
}

std::string bookTitleFor(const std::string& cachePath) {
  std::string bookPath;
  if (!FsHelpers::readBookSource(cachePath, bookPath) || bookPath.empty()) return std::string();
  const size_t slash = bookPath.find_last_of('/');
  std::string name = slash == std::string::npos ? bookPath : bookPath.substr(slash + 1);
  const size_t dot = name.find_last_of('.');
  if (dot != std::string::npos && dot > 0) name.resize(dot);
  return name;
}
}  // namespace

void LookupHistory::appendGlobal(const std::string& cachePath, const std::string& word, const Status status) {
  // Only lookups made in a book (not the Dictionary tool's own history).
  if (cachePath.rfind(FsHelpers::kBookCacheDir, 0) != 0) return;
  trimGlobalIfLarge();
  HalFile file = Storage.open(kGlobalPath, O_WRONLY | O_CREAT | O_AT_END);
  if (!file) {
    LOG_ERR("LH", "Failed to open global history");
    return;
  }
  char when[20] = "";
  ReadingStatsDateTime now;
  if (getCurrentLocalReadingStatsDateTime(now)) {
    snprintf(when, sizeof(when), "%04u-%02u-%02u %02u:%02u", static_cast<unsigned>(now.date.year),
             static_cast<unsigned>(now.date.month), static_cast<unsigned>(now.date.day),
             static_cast<unsigned>(now.hour), static_cast<unsigned>(now.minute));
  }
  const char suffix[] = {'|', static_cast<char>(status), '\n'};
  file.write(cachePath.c_str(), cachePath.size());
  file.write("|", 1);
  file.write(when, strlen(when));
  file.write("|", 1);
  file.write(word.c_str(), word.size());
  file.write(suffix, sizeof(suffix));
  file.close();
}

std::vector<LookupHistory::BookLookups> LookupHistory::loadAllBooks(uint32_t& totalWords) {
  std::vector<BookLookups> books;
  totalWords = 0;
  forEachGlobalLine([&](const char* cachePath, const char*, const char*) {
    ++totalWords;
    // Move the book to the end on every hit, so the list ends newest-last.
    auto it = std::find_if(books.begin(), books.end(),
                           [cachePath](const BookLookups& b) { return b.cachePath == cachePath; });
    BookLookups entry;
    if (it != books.end()) {
      entry = std::move(*it);
      books.erase(it);
    } else {
      entry.cachePath = cachePath;
    }
    if (entry.words < UINT16_MAX) ++entry.words;
    books.push_back(std::move(entry));
  });
  std::reverse(books.begin(), books.end());  // most recently used first
  for (BookLookups& book : books) {
    book.title = bookTitleFor(book.cachePath);
    if (book.title.empty()) book.title = "?";
  }
  return books;
}

std::vector<LookupHistory::TimedWord> LookupHistory::loadBookWords(const std::string& cachePath, const size_t max) {
  std::vector<TimedWord> words;
  forEachGlobalLine([&](const char* bookCache, const char* when, const char* word) {
    if (cachePath != bookCache) return;
    // Newest wins: drop an earlier copy of the same word.
    words.erase(std::remove_if(words.begin(), words.end(), [word](const TimedWord& w) { return w.word == word; }),
                words.end());
    words.push_back({word, when});
    if (words.size() > max) words.erase(words.begin());
  });
  std::reverse(words.begin(), words.end());
  return words;
}

void LookupHistory::addWordIf(const std::string& cachePath, const std::string& word, Status status, bool enabled) {
  if (!enabled || word.empty() || cachePath.empty()) return;
  addWord(cachePath, word, status);
}

std::vector<LookupHistory::Entry> LookupHistory::load(const std::string& cachePath) {
  auto entries = readRecent(filePath(cachePath));
  std::reverse(entries.begin(), entries.end());
  return entries;
}

bool LookupHistory::removeRecentAt(const std::string& cachePath, int indexFromNewest) {
  if (indexFromNewest < 0) return false;
  const std::string path = filePath(cachePath);
  HalFile source;
  if (!Storage.openFileForRead("LH", path.c_str(), source)) return false;

  int lineCount = 0;
  bool inLine = false;
  while (source.available()) {
    const int byte = source.read();
    if (byte < 0) break;
    if (byte == '\n') {
      lineCount++;
      inLine = false;
    } else if (byte != '\r') {
      inLine = true;
    }
  }
  if (inLine) lineCount++;
  const int skipLine = lineCount - 1 - indexFromNewest;
  if (skipLine < 0) {
    source.close();
    return false;
  }

  source.seek(0);
  const std::string tempPath = path + ".tmp";
  HalFile destination;
  if (!Storage.openFileForWrite("LH", tempPath.c_str(), destination)) {
    source.close();
    return false;
  }

  int currentLine = 0;
  bool ok = true;
  while (source.available()) {
    const int byte = source.read();
    if (byte < 0) break;
    if (currentLine != skipLine && destination.write(static_cast<uint8_t>(byte)) != 1) {
      ok = false;
      break;
    }
    if (byte == '\n') currentLine++;
  }
  destination.flush();
  source.close();
  destination.close();

  if (!ok || !Storage.remove(path.c_str()) || !Storage.rename(tempPath.c_str(), path.c_str())) {
    LOG_ERR("LH", "Failed to remove history entry: %s", path.c_str());
    Storage.remove(tempPath.c_str());
    return false;
  }
  return true;
}
