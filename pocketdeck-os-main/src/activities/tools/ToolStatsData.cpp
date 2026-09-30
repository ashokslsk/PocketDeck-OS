#include "ToolStatsData.h"

#include <HalStorage.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "SrsState.h"
#include "ToolsCharts.h"
#include "ToolsDate.h"
#include "ToolsStore.h"

namespace tstats {

void clearSeries(DaySeries& s, const int32_t today, const int16_t fill) {
  s.firstDay = today - (kHistoryDays - 1);
  std::fill(s.a, s.a + kHistoryDays, fill);
  std::fill(s.b, s.b + kHistoryDays, fill);
  std::fill(s.minute, s.minute + kHistoryDays, charts::kNoValue);
}

// mood: a = mood 1..5 (latest wins), minute = time logged (same day)
void onMood(const tlog::Stamp& at, char* fields, void* p) {
  auto* s = static_cast<DaySeries*>(p);
  char* cursor = fields;
  const char* date = tlog::nextField(&cursor);
  const char* mood = tlog::nextField(&cursor);
  int32_t day = 0;
  if (date == nullptr || mood == nullptr || !tools::parseIsoDate(date, day)) return;
  const int idx = day - s->firstDay;
  const int m = atoi(mood);
  if (idx < 0 || idx >= kHistoryDays || m < 1 || m > 5) return;
  s->a[idx] = static_cast<int16_t>(m);
  s->minute[idx] = day == at.day ? at.minute : charts::kNoValue;
}

// pomodoro: a = sessions, b = focus minutes, minute = first session time
void onPomodoro(const tlog::Stamp& at, char* fields, void* p) {
  auto* s = static_cast<DaySeries*>(p);
  char* cursor = fields;
  const char* kind = tlog::nextField(&cursor);
  const char* minutes = tlog::nextField(&cursor);
  const int idx = at.day - s->firstDay;
  if (kind == nullptr || strcmp(kind, "focus") != 0 || idx < 0 || idx >= kHistoryDays) return;
  ++s->a[idx];
  s->b[idx] = static_cast<int16_t>(s->b[idx] + (minutes != nullptr ? atoi(minutes) : 0));
  if (s->minute[idx] == charts::kNoValue) s->minute[idx] = at.minute;
}

// today: a = done, b = total (for the day the list belonged to)
void onToday(const tlog::Stamp&, char* fields, void* p) {
  auto* s = static_cast<DaySeries*>(p);
  char* cursor = fields;
  const char* date = tlog::nextField(&cursor);
  const char* done = tlog::nextField(&cursor);
  const char* total = tlog::nextField(&cursor);
  int32_t day = 0;
  if (date == nullptr || done == nullptr || total == nullptr || !tools::parseIsoDate(date, day)) return;
  const int idx = day - s->firstDay;
  if (idx < 0 || idx >= kHistoryDays) return;
  s->a[idx] = static_cast<int16_t>(atoi(done));
  s->b[idx] = static_cast<int16_t>(atoi(total));
}

// flashcards: a = cards reviewed, b = remembered
void onFlashcards(const tlog::Stamp& at, char* fields, void* p) {
  auto* s = static_cast<DaySeries*>(p);
  char* cursor = fields;
  tlog::nextField(&cursor);  // deck
  const char* reviewed = tlog::nextField(&cursor);
  const char* remembered = tlog::nextField(&cursor);
  const int idx = at.day - s->firstDay;
  if (reviewed == nullptr || remembered == nullptr || idx < 0 || idx >= kHistoryDays) return;
  s->a[idx] = static_cast<int16_t>(s->a[idx] + atoi(reviewed));
  s->b[idx] = static_cast<int16_t>(s->b[idx] + atoi(remembered));
  if (s->minute[idx] == charts::kNoValue) s->minute[idx] = at.minute;
}

int averageMinute(const DaySeries& s) {
  long sum = 0;
  int n = 0;
  for (const int16_t m : s.minute) {
    if (m == charts::kNoValue) continue;
    sum += m;
    ++n;
  }
  return n > 0 ? static_cast<int>(sum / n) : -1;
}

void onDeck(const tlog::Stamp&, char* fields, void* p) {
  auto* t = static_cast<DeckTotals*>(p);
  char* cursor = fields;
  const char* deck = tlog::nextField(&cursor);
  const char* reviewed = tlog::nextField(&cursor);
  const char* remembered = tlog::nextField(&cursor);
  const char* forgot = tlog::nextField(&cursor);
  if (deck == nullptr || reviewed == nullptr || remembered == nullptr || forgot == nullptr) return;
  int i = 0;
  while (i < t->count && strcmp(t->name[i], deck) != 0) ++i;
  if (i == t->count) {
    if (t->count >= DeckTotals::kMaxDecks) return;
    snprintf(t->name[i], sizeof(t->name[i]), "%s", deck);
    ++t->count;
  }
  ++t->sessions[i];
  t->reviewed[i] += static_cast<uint32_t>(atoi(reviewed));
  t->remembered[i] += static_cast<uint32_t>(atoi(remembered));
  t->forgot[i] += static_cast<uint32_t>(atoi(forgot));
}

void scanSeries(const char* feature, const int32_t today, const tlog::LineFn fn, DaySeries& s, const int16_t fill) {
  clearSeries(s, today, fill);
  tlog::scan(feature, s.firstDay, today, fn, &s);
}

// knowledge: "topic|index" per answer opened
void onKnowledge(const tlog::Stamp& at, char* fields, void* p) {
  auto* s = static_cast<DaySeries*>(p);
  (void)fields;
  const int idx = at.day - s->firstDay;
  if (idx < 0 || idx >= kHistoryDays) return;
  ++s->a[idx];
  if (s->minute[idx] == charts::kNoValue) s->minute[idx] = at.minute;
}

// quotes: "open|YYYY-MM-DD" when the tool is opened
void onQuotes(const tlog::Stamp& at, char* fields, void* p) {
  auto* s = static_cast<DaySeries*>(p);
  char* cursor = fields;
  const char* kind = tlog::nextField(&cursor);
  const int idx = at.day - s->firstDay;
  if (kind == nullptr || strcmp(kind, "open") != 0 || idx < 0 || idx >= kHistoryDays) return;
  s->a[idx] = 1;
  if (s->minute[idx] == charts::kNoValue) s->minute[idx] = at.minute;
}

// mantras: "count|category|mantra|text" for each japa session
void onMantras(const tlog::Stamp& at, char* fields, void* p) {
  auto* s = static_cast<DaySeries*>(p);
  char* cursor = fields;
  const char* count = tlog::nextField(&cursor);
  const int idx = at.day - s->firstDay;
  if (count == nullptr || idx < 0 || idx >= kHistoryDays) return;
  const int n = atoi(count);
  if (n <= 0) return;
  s->a[idx] = static_cast<int16_t>(std::min(32000, s->a[idx] + n));
  ++s->b[idx];
  if (s->minute[idx] == charts::kNoValue) s->minute[idx] = at.minute;
}

int sumLast(const int16_t* values, const int days) {
  int sum = 0;
  for (int i = kHistoryDays - days; i < kHistoryDays; ++i) {
    if (i >= 0 && values[i] > 0) sum += values[i];
  }
  return sum;
}

int streak(const int16_t* values) {
  int n = 0;
  for (int i = kHistoryDays - 1; i >= 0; --i) {
    if (values[i] <= 0) {
      if (i == kHistoryDays - 1) continue;  // nothing yet today does not break it
      break;
    }
    ++n;
  }
  return n;
}

uint32_t indexCount(const char* path) {
  FsFile f;
  if (!Storage.exists(path) || !Storage.openFileForRead("TST", path, f)) return 0;
  uint32_t header[3] = {};
  const bool ok = f.read(header, sizeof(header)) == sizeof(header);
  f.close();
  return ok ? header[2] : 0;
}

namespace {
// Counts "question" keys in a deck or topic file by streaming it (used when
// the tool has not built its index yet).
uint32_t countQuestions(const char* path) {
  FsFile f;
  if (!Storage.exists(path) || !Storage.openFileForRead("TST", path, f)) return 0;
  tools::JsonReader r(f);
  using Tok = tools::JsonReader::Token;
  char text[12];
  uint32_t n = 0;
  bool prevWasQuestionKey = false;
  while (true) {
    const Tok t = r.next(text, sizeof(text));
    if (t == Tok::End || t == Tok::Error) break;
    if (t == Tok::Colon && prevWasQuestionKey) ++n;
    prevWasQuestionKey = t == Tok::String && strcmp(text, "question") == 0;
  }
  f.close();
  return n;
}

int findOrAdd(char (*names)[32], int& count, const int cap, const char* name) {
  for (int i = 0; i < count; ++i) {
    if (strcmp(names[i], name) == 0) return i;
  }
  if (count >= cap) return -1;
  snprintf(names[count], sizeof(names[count]), "%s", name);
  return count++;
}

bool stripJson(char* name) {
  const size_t n = strlen(name);
  if (n <= 5 || strcasecmp(name + n - 5, ".json") != 0) return false;
  name[n - 5] = '\0';
  return true;
}

void listJsonFiles(const char* dir, char (*names)[32], int& count, const int cap) {
  FsFile d = Storage.open(dir);
  if (!d || !d.isDirectory()) {
    if (d) d.close();
    return;
  }
  char name[64];
  for (FsFile e = d.openNextFile(); e; e = d.openNextFile()) {
    const bool isDir = e.isDirectory();
    e.getName(name, sizeof(name));
    e.close();
    if (isDir || name[0] == '.' || !stripJson(name)) continue;
    findOrAdd(names, count, cap, name);
  }
  d.close();
}
}  // namespace

int SrsSummary::totalCards() const {
  int n = 0;
  for (int i = 0; i < count; ++i) n += cards[i];
  return n;
}
int SrsSummary::totalReviewed() const {
  int n = 0;
  for (int i = 0; i < count; ++i) n += reviewed[i];
  return n;
}
int SrsSummary::totalMastered() const {
  int n = 0;
  for (int i = 0; i < count; ++i) n += mastered[i];
  return n;
}
int SrsSummary::totalDue() const {
  int n = 0;
  for (int i = 0; i < count; ++i) n += due[i] + std::max(0, cards[i] - reviewed[i]);
  return n;
}

void summarizeSrs(const int32_t today, SrsSummary& out) {
  out = SrsSummary{};
  listJsonFiles("/tools/flashcards", out.name, out.count, SrsSummary::kMaxDecks);
  char path[96];
  for (int i = 0; i < out.count; ++i) {
    snprintf(path, sizeof(path), "%s/fc_%s.idx", tools::kToolsCacheDir, out.name[i]);
    uint32_t n = indexCount(path);
    if (n == 0) {
      snprintf(path, sizeof(path), "/tools/flashcards/%s.json", out.name[i]);
      n = countQuestions(path);
    }
    out.cards[i] = static_cast<uint16_t>(n);
  }
  tools::recoverFromBackup("/tools/srs_state.json");
  FsFile f;
  if (!Storage.exists("/tools/srs_state.json") || !Storage.openFileForRead("TST", "/tools/srs_state.json", f)) return;
  tools::JsonReader r(f);
  using Tok = tools::JsonReader::Token;
  if (tools::findFirstArray(r)) {
    srs::StateEntry e;
    while (true) {
      const Tok t = r.next(nullptr, 0);
      if (t == Tok::Comma) continue;
      if (t != Tok::ObjectStart || !srs::readStateObject(r, e)) break;
      const int d = findOrAdd(out.name, out.count, SrsSummary::kMaxDecks, e.deck);
      if (d < 0) continue;
      ++out.reviewed[d];
      if (e.interval >= 21) ++out.mastered[d];
      if (today >= e.lastDay + e.interval) ++out.due[d];
    }
  }
  f.close();
  // A reviewed count above the indexed card count means the deck changed;
  // never report negative "new" cards.
  for (int i = 0; i < out.count; ++i) out.cards[i] = std::max(out.cards[i], out.reviewed[i]);
}

namespace {
void onCoverage(const tlog::Stamp&, char* fields, void* p) {
  auto* c = static_cast<KnowledgeCoverage*>(p);
  char* cursor = fields;
  const char* topic = tlog::nextField(&cursor);
  const char* index = tlog::nextField(&cursor);
  if (topic == nullptr || index == nullptr) return;
  const int t = findOrAdd(c->topic, c->count, KnowledgeCoverage::kMaxTopics, topic);
  const int q = atoi(index);
  if (t < 0 || q < 0 || q >= KnowledgeCoverage::kMaxQuestions) return;
  uint8_t& byte = c->bits[t][q >> 3];
  const uint8_t mask = static_cast<uint8_t>(1U << (q & 7));
  if ((byte & mask) == 0) {
    byte |= mask;
    ++c->seen[t];
  }
}
}  // namespace

void summarizeKnowledge(const int32_t today, KnowledgeCoverage& out) {
  out = KnowledgeCoverage{};
  listJsonFiles("/tools/knowledge", out.topic, out.count, KnowledgeCoverage::kMaxTopics);
  // Answers opened in the last 400 days count towards coverage.
  tlog::scan("knowledge", today - 400, today, &onCoverage, &out);
  char path[96];
  for (int i = 0; i < out.count; ++i) {
    snprintf(path, sizeof(path), "%s/kn_%s.idx", tools::kToolsCacheDir, out.topic[i]);
    uint32_t n = indexCount(path);
    if (n == 0) {
      snprintf(path, sizeof(path), "/tools/knowledge/%s.json", out.topic[i]);
      n = countQuestions(path);
    }
    out.total[i] = static_cast<uint16_t>(std::max<uint32_t>(n, out.seen[i]));
  }
}

int countFavouriteQuotes() {
  FsFile f;
  if (!Storage.exists(kFavouritesPath) || !Storage.openFileForRead("TST", kFavouritesPath, f)) return 0;
  int n = 0;
  char line[8];
  while (tools::readLine(f, line, sizeof(line)) >= 0) n += line[0] != '\0' && line[0] != '#' ? 1 : 0;
  f.close();
  return n;
}

}  // namespace tstats
