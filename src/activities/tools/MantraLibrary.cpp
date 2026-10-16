#include "MantraLibrary.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ToolsStore.h"

namespace mantras {

namespace {

constexpr char kIndexPath[] = "/tools/.cache/mantras.idx";
constexpr char kMagic[4] = {'M', 'I', 'X', '1'};
constexpr size_t kHeaderBytes = 32;
constexpr size_t kCategoryBytes = 160;
constexpr size_t kKavachaBytes = 132;
constexpr int kMaxCategories = 80;  // matches Library::kMaxCategories
constexpr int kMaxKavachas = 8;
constexpr uint32_t kMaxMantras = 1600;
constexpr char kRitualKey[] = "additional_ritualistic_mantras";

using T = tools::JsonReader::Token;

struct Record {
  uint32_t offset;
  uint8_t category;
  uint16_t sequence;
};

struct Builder {
  tools::JsonReader& rd;
  Category* cats;
  int catCount = 0;
  Kavacha* kavachas;
  int kavachaCount = 0;
  Record* records;
  uint32_t count = 0;
  bool full = false;

  int addCategory(const Kind kind, const char* key, const int kavacha) {
    if (catCount >= kMaxCategories) {
      full = true;
      return -1;
    }
    Category& c = cats[catCount];
    c = Category{};
    c.kind = kind;
    c.kavacha = static_cast<uint8_t>(kavacha);
    snprintf(c.key, sizeof(c.key), "%s", key);
    return catCount++;
  }

  int ritualCategory(const char* name) {
    for (int i = 0; i < catCount; ++i) {
      if (cats[i].kind == Kind::Ritual && strncmp(cats[i].english, name, sizeof(cats[i].english) - 1) == 0) return i;
    }
    const int i = addCategory(Kind::Ritual, name, 0);
    if (i >= 0) snprintf(cats[i].english, sizeof(cats[i].english), "%s", name);
    return i;
  }

  void record(const uint32_t offset, const int cat, const uint16_t seq) {
    if (cat < 0) return;
    if (count >= kMaxMantras) {
      full = true;
      return;
    }
    records[count++] = {offset, static_cast<uint8_t>(cat), seq};
  }

  // Reads "key": and returns the value's first token, or End after the object ends.
  T nextMember(char* key, const size_t keyCap, char* value, const size_t valueCap) {
    while (true) {
      const T t = rd.next(key, keyCap);
      if (t == T::Comma) continue;
      if (t != T::String) return T::End;
      if (rd.next(nullptr, 0) != T::Colon) return T::End;
      return rd.next(value, valueCap);
    }
  }

  // One mantra object (ObjectStart consumed): its ritual category and sequence.
  void mantra(const uint32_t offset, int cat, const bool ritual) {
    char key[24], value[64];
    uint16_t seq = 0;
    T v;
    while ((v = nextMember(key, sizeof(key), value, sizeof(value))) != T::End) {
      if (v == T::ObjectStart || v == T::ArrayStart) {
        rd.skipValue(v);
      } else if (strcmp(key, "sequence") == 0 && v == T::Literal) {
        seq = static_cast<uint16_t>(atoi(value));
      } else if (ritual && strcmp(key, "category") == 0 && v == T::String) {
        cat = ritualCategory(value);
      }
    }
    record(offset, cat, seq);
  }

  void mantraArray(const int cat, const bool ritual) {
    T t;
    while ((t = rd.next(nullptr, 0)) != T::ArrayEnd && t != T::End && t != T::Error) {
      if (t == T::ObjectStart) {
        mantra(rd.tokenOffset(), cat, ritual);
      } else if (t == T::ArrayStart) {
        rd.skipValue(t);
      }
    }
  }

  // A deity object, or the ritual collection (ObjectStart consumed).
  void deity(const char* deityKey) {
    const bool ritual = strcmp(deityKey, kRitualKey) == 0;
    const int cat = ritual ? -1 : addCategory(Kind::Deity, deityKey, 0);
    char key[24], value[96];
    T v;
    while ((v = nextMember(key, sizeof(key), value, sizeof(value))) != T::End) {
      if (strcmp(key, "mantras") == 0 && v == T::ArrayStart) {
        mantraArray(cat, ritual);
      } else if (v == T::ObjectStart || v == T::ArrayStart) {
        rd.skipValue(v);
      } else if (cat >= 0 && strcmp(key, "deity_kannada") == 0) {
        snprintf(cats[cat].kannada, sizeof(cats[cat].kannada), "%s", value);
      } else if (cat >= 0 && strcmp(key, "deity_english") == 0) {
        snprintf(cats[cat].english, sizeof(cats[cat].english), "%s", value);
      }
    }
  }

  void section(const int kav, const char* sectionKey) {
    const int cat = addCategory(Kind::KavachaSection, sectionKey, kav);
    char key[24], value[96];
    T v;
    while ((v = nextMember(key, sizeof(key), value, sizeof(value))) != T::End) {
      if (strcmp(key, "mantras") == 0 && v == T::ArrayStart) {
        mantraArray(cat, false);
      } else if (v == T::ObjectStart || v == T::ArrayStart) {
        rd.skipValue(v);
      } else if (cat >= 0 && strcmp(key, "title_kannada") == 0) {
        snprintf(cats[cat].kannada, sizeof(cats[cat].kannada), "%s", value);
      } else if (cat >= 0 && strcmp(key, "title_english") == 0) {
        snprintf(cats[cat].english, sizeof(cats[cat].english), "%s", value);
      }
    }
  }

  void kavacha(const uint32_t offset) {
    if (kavachaCount >= kMaxKavachas) {
      full = true;
      rd.skipValue(T::ObjectStart);
      return;
    }
    const int k = kavachaCount++;
    kavachas[k] = Kavacha{};
    kavachas[k].offset = offset;
    char key[24], value[96];
    T v;
    while ((v = nextMember(key, sizeof(key), value, sizeof(value))) != T::End) {
      if (strcmp(key, "sections") == 0 && v == T::ObjectStart) {
        char sectionKey[24];
        T s;
        while ((s = nextMember(sectionKey, sizeof(sectionKey), nullptr, 0)) != T::End) {
          if (s == T::ObjectStart) {
            section(k, sectionKey);
          } else if (s == T::ArrayStart) {
            rd.skipValue(s);
          }
        }
      } else if (v == T::ObjectStart || v == T::ArrayStart) {
        rd.skipValue(v);
      } else if (strcmp(key, "name_kannada") == 0) {
        snprintf(kavachas[k].kannada, sizeof(kavachas[k].kannada), "%s", value);
      } else if (strcmp(key, "name_english") == 0) {
        snprintf(kavachas[k].english, sizeof(kavachas[k].english), "%s", value);
      }
    }
  }

  void top() {
    if (rd.next(nullptr, 0) != T::ObjectStart) return;
    char key[40];
    T v;
    while ((v = nextMember(key, sizeof(key), nullptr, 0)) != T::End) {
      if (strcmp(key, "vedic_mantras_collection") == 0 && v == T::ObjectStart) {
        char deityKey[48];
        T d;
        while ((d = nextMember(deityKey, sizeof(deityKey), nullptr, 0)) != T::End) {
          if (d == T::ObjectStart) {
            deity(deityKey);
          } else if (d == T::ArrayStart) {
            rd.skipValue(d);
          }
        }
      } else if (strcmp(key, "kavacha_collection") == 0 && v == T::ArrayStart) {
        T k;
        while ((k = rd.next(nullptr, 0)) != T::ArrayEnd && k != T::End && k != T::Error) {
          if (k == T::ObjectStart) kavacha(rd.tokenOffset());
        }
      } else if (v == T::ObjectStart || v == T::ArrayStart) {
        rd.skipValue(v);
      }
    }
  }
};

struct WriteCtx {
  const Builder* b;
  uint32_t sourceSize;
};

bool writeIndex(FsFile& out, void* ctx) {
  const auto* w = static_cast<const WriteCtx*>(ctx);
  const Builder& b = *w->b;
  uint8_t h[kHeaderBytes] = {};
  memcpy(h, kMagic, 4);
  tools::putLe16(h + 4, 1);
  tools::putLe16(h + 6, static_cast<uint16_t>(b.catCount));
  tools::putLe16(h + 8, static_cast<uint16_t>(b.count));
  h[10] = static_cast<uint8_t>(b.kavachaCount);
  tools::putLe32(h + 12, w->sourceSize);
  if (out.write(h, sizeof(h)) != sizeof(h)) return false;
  for (int i = 0; i < b.catCount; ++i) {
    uint8_t c[kCategoryBytes] = {};
    const Category& cat = b.cats[i];
    c[0] = static_cast<uint8_t>(cat.kind);
    c[1] = cat.kavacha;
    tools::putLe16(c + 2, cat.first);
    tools::putLe16(c + 4, cat.count);
    memcpy(c + 8, cat.key, sizeof(cat.key));
    memcpy(c + 32, cat.kannada, sizeof(cat.kannada));
    memcpy(c + 104, cat.english, sizeof(cat.english));
    if (out.write(c, sizeof(c)) != sizeof(c)) return false;
  }
  for (int i = 0; i < b.kavachaCount; ++i) {
    uint8_t k[kKavachaBytes] = {};
    tools::putLe32(k, b.kavachas[i].offset);
    memcpy(k + 4, b.kavachas[i].kannada, sizeof(b.kavachas[i].kannada));
    memcpy(k + 76, b.kavachas[i].english, sizeof(b.kavachas[i].english));
    if (out.write(k, sizeof(k)) != sizeof(k)) return false;
  }
  for (uint32_t i = 0; i < b.count; ++i) {
    uint8_t m[4];
    tools::putLe32(m, b.records[i].offset);
    if (out.write(m, 4) != 4) return false;
  }
  return true;
}

}  // namespace

bool Library::loadHeader() {
  categoryCount_ = mantraCount_ = 0;
  kavachaCount_ = 0;
  FsFile f;
  if (!Storage.exists(kIndexPath) || !Storage.openFileForRead("MANT", kIndexPath, f)) return false;
  uint8_t h[kHeaderBytes];
  const bool ok =
      f.read(h, sizeof(h)) == static_cast<int>(sizeof(h)) && memcmp(h, kMagic, 4) == 0 && tools::le16(h + 4) == 1;
  f.close();
  if (!ok) return false;
  categoryCount_ = std::min<uint16_t>(tools::le16(h + 6), kMaxCategories);
  mantraCount_ = tools::le16(h + 8);
  kavachaCount_ = h[10];
  categoriesOffset_ = kHeaderBytes;
  kavachasOffset_ = categoriesOffset_ + static_cast<uint32_t>(kCategoryBytes * tools::le16(h + 6));
  mantrasOffset_ = kavachasOffset_ + static_cast<uint32_t>(kKavachaBytes * kavachaCount_);
  if (!Storage.openFileForRead("MANT", kIndexPath, f)) return false;
  bool read = f.seek(categoriesOffset_);
  for (uint16_t i = 0; read && i < categoryCount_; ++i) {
    uint8_t c[8];
    read = f.read(c, sizeof(c)) == sizeof(c) && f.seek(categoriesOffset_ + kCategoryBytes * (i + 1u));
    briefs_[i] = {static_cast<Kind>(c[0]), c[1], tools::le16(c + 2), tools::le16(c + 4)};
  }
  f.close();
  if (!read) categoryCount_ = mantraCount_ = 0;
  return mantraCount_ > 0;
}

bool Library::build(const uint32_t sourceSize) {
  auto cats = makeUniqueNoThrow<Category[]>(kMaxCategories);
  auto kavachas = makeUniqueNoThrow<Kavacha[]>(kMaxKavachas);
  auto records = makeUniqueNoThrow<Record[]>(kMaxMantras);
  if (!cats || !kavachas || !records) {
    LOG_ERR("MANT", "OOM building the mantra index");
    return false;
  }
  FsFile f;
  if (!Storage.openFileForRead("MANT", kLibraryPath, f)) {
    LOG_ERR("MANT", "Could not open %s", kLibraryPath);
    return false;
  }
  tools::JsonReader rd(f);
  Builder b{rd, cats.get(), 0, kavachas.get(), 0, records.get(), 0, false};
  b.top();
  f.close();
  if (b.full) LOG_ERR("MANT", "Mantra library larger than the index; some entries were left out");
  // Group by category; rituals follow their "sequence", everything else file order.
  // Insertion sort (stable, and far smaller in flash than std::stable_sort);
  // records are nearly in order already, so it is quick.
  auto before = [&](const Record& x, const Record& y) {
    if (x.category != y.category) return x.category < y.category;
    if (b.cats[x.category].kind == Kind::Ritual && x.sequence != y.sequence) return x.sequence < y.sequence;
    return x.offset < y.offset;
  };
  for (uint32_t i = 1; i < b.count; ++i) {
    const Record r = b.records[i];
    uint32_t j = i;
    while (j > 0 && before(r, b.records[j - 1])) {
      b.records[j] = b.records[j - 1];
      --j;
    }
    b.records[j] = r;
  }
  for (uint32_t i = 0; i < b.count; ++i) {
    Category& c = b.cats[b.records[i].category];
    if (c.count == 0) c.first = static_cast<uint16_t>(i);
    ++c.count;
  }
  // Categories without mantras are dropped from view by keeping count 0.
  Storage.ensureDirectoryExists("/tools/.cache");
  WriteCtx ctx{&b, sourceSize};
  if (!tools::writeFileAtomic(kIndexPath, &writeIndex, &ctx)) {
    LOG_ERR("MANT", "Could not write %s", kIndexPath);
    return false;
  }
  LOG_INF("MANT", "Indexed %lu mantras in %d categories", static_cast<unsigned long>(b.count), b.catCount);
  return loadHeader();
}

bool Library::open() {
  Storage.ensureDirectoryExists(kDir);
  if (!Storage.exists(kLibraryPath)) {
    categoryCount_ = mantraCount_ = 0;
    return false;
  }
  FsFile f;
  if (!Storage.openFileForRead("MANT", kLibraryPath, f)) return false;
  const auto size = static_cast<uint32_t>(f.fileSize());
  f.close();
  tools::recoverFromBackup(kIndexPath);
  FsFile idx;
  if (Storage.exists(kIndexPath) && Storage.openFileForRead("MANT", kIndexPath, idx)) {
    uint8_t h[kHeaderBytes];
    const bool same = idx.read(h, sizeof(h)) == static_cast<int>(sizeof(h)) && memcmp(h, kMagic, 4) == 0 &&
                      tools::le16(h + 4) == 1 && tools::le32(h + 12) == size;
    idx.close();
    if (same && loadHeader()) return true;
  }
  return build(size);
}

namespace {
bool readRecord(const uint32_t offset, uint8_t* dst, const size_t n) {
  FsFile f;
  if (!Storage.openFileForRead("MANT", kIndexPath, f)) return false;
  const bool ok = f.seek(offset) && f.read(dst, n) == static_cast<int>(n);
  f.close();
  return ok;
}
}  // namespace

bool Library::category(const uint16_t i, Category& out) const {
  uint8_t c[kCategoryBytes];
  if (i >= categoryCount_ || !readRecord(categoriesOffset_ + kCategoryBytes * i, c, sizeof(c))) return false;
  out.kind = static_cast<Kind>(c[0]);
  out.kavacha = c[1];
  out.first = tools::le16(c + 2);
  out.count = tools::le16(c + 4);
  memcpy(out.key, c + 8, sizeof(out.key));
  memcpy(out.kannada, c + 32, sizeof(out.kannada));
  memcpy(out.english, c + 104, sizeof(out.english));
  out.key[sizeof(out.key) - 1] = out.kannada[sizeof(out.kannada) - 1] = out.english[sizeof(out.english) - 1] = '\0';
  return true;
}

bool Library::kavacha(const uint8_t i, Kavacha& out) const {
  uint8_t k[kKavachaBytes];
  if (i >= kavachaCount_ || !readRecord(kavachasOffset_ + kKavachaBytes * i, k, sizeof(k))) return false;
  out.offset = tools::le32(k);
  memcpy(out.kannada, k + 4, sizeof(out.kannada));
  memcpy(out.english, k + 76, sizeof(out.english));
  out.kannada[sizeof(out.kannada) - 1] = out.english[sizeof(out.english) - 1] = '\0';
  return true;
}

int Library::findCategory(const Kind kind, const int nth) const {
  int seen = 0;
  for (uint16_t i = 0; i < categoryCount_; ++i) {
    if (briefs_[i].kind != kind || briefs_[i].count == 0) continue;
    if (seen++ == nth) return i;
  }
  return -1;
}

int Library::countOf(const Kind kind) const {
  int n = 0;
  for (uint16_t i = 0; i < categoryCount_; ++i) {
    if (briefs_[i].kind == kind && briefs_[i].count > 0) ++n;
  }
  return n;
}

int Library::findDeity(const char* key) const {
  Category c;
  for (uint16_t i = 0; i < categoryCount_; ++i) {
    if (briefs_[i].kind != Kind::Deity || briefs_[i].count == 0) continue;
    if (category(i, c) && strcasecmp(c.key, key) == 0) return i;
  }
  return -1;
}

int Library::categoryOf(const uint16_t i) const {
  for (uint16_t k = 0; k < categoryCount_; ++k) {
    if (briefs_[k].count > 0 && i >= briefs_[k].first && i < briefs_[k].first + briefs_[k].count) return k;
  }
  return -1;
}

uint32_t Library::mantraOffset(const uint16_t i) const {
  uint8_t m[4];
  if (i >= mantraCount_ || !readRecord(mantrasOffset_ + 4u * i, m, sizeof(m))) return UINT32_MAX;
  return tools::le32(m);
}

bool Library::read(const uint16_t i, Mantra& out) const {
  out.kannada[0] = out.english[0] = out.meaningKannada[0] = out.meaningEnglish[0] = out.use[0] = '\0';
  const uint32_t offset = mantraOffset(i);
  if (offset == UINT32_MAX) return false;
  FsFile f;
  if (!Storage.openFileForRead("MANT", kLibraryPath, f)) return false;
  tools::JsonReader rd(f);
  bool ok = rd.seek(offset) && rd.next(nullptr, 0) == T::ObjectStart;
  char key[24];
  while (ok) {
    T t = rd.next(key, sizeof(key));
    if (t == T::Comma) continue;
    if (t != T::String || rd.next(nullptr, 0) != T::Colon) break;
    char* dst = nullptr;
    size_t cap = 0;
    if (strcmp(key, "kannada") == 0) {
      dst = out.kannada, cap = sizeof(out.kannada);
    } else if (strcmp(key, "english") == 0) {
      dst = out.english, cap = sizeof(out.english);
    } else if (strcmp(key, "meaning_kannada") == 0) {
      dst = out.meaningKannada, cap = sizeof(out.meaningKannada);
    } else if (strcmp(key, "meaning_english") == 0 || (strcmp(key, "meaning") == 0 && out.meaningEnglish[0] == '\0')) {
      dst = out.meaningEnglish, cap = sizeof(out.meaningEnglish);
    } else if (strcmp(key, "use") == 0) {
      dst = out.use, cap = sizeof(out.use);
    }
    t = rd.next(dst, cap);
    if (t == T::ObjectStart || t == T::ArrayStart) rd.skipValue(t);
  }
  f.close();
  return out.kannada[0] != '\0' || out.english[0] != '\0';
}

bool Library::kavachaAbout(const uint8_t i, const bool kannada, char* out, const size_t cap) const {
  if (cap == 0) return false;
  out[0] = '\0';
  Kavacha k;
  if (!kavacha(i, k)) return false;
  FsFile f;
  if (!Storage.openFileForRead("MANT", kLibraryPath, f)) return false;
  tools::JsonReader rd(f);
  size_t n = 0;
  const char* lang = kannada ? "kannada" : "english";
  bool ok = rd.seek(k.offset) && rd.next(nullptr, 0) == T::ObjectStart;
  char key[32], inner[16];
  static constexpr const char* kFields[] = {"source_tradition", "rishi",   "chandas",    "devata", "beeja",
                                            "shakti",           "keelaka", "phalashruti"};
  while (ok) {
    T t = rd.next(key, sizeof(key));
    if (t == T::Comma) continue;
    if (t != T::String || rd.next(nullptr, 0) != T::Colon) break;
    t = rd.next(nullptr, 0);
    bool wanted = false;
    for (const char* field : kFields) wanted = wanted || strcmp(field, key) == 0;
    if (t == T::ObjectStart && wanted) {
      // {"kannada": "...", "english": "..."}: keep the chosen language.
      while (true) {
        T m = rd.next(inner, sizeof(inner));
        if (m == T::Comma) continue;
        if (m != T::String || rd.next(nullptr, 0) != T::Colon) break;
        if (strcmp(inner, lang) == 0 && n + 8 < cap) {
          // Read the text straight into out, after a blank line.
          const size_t start = n ? n + 2 : n;
          m = rd.next(out + start, cap - start);
          if (m == T::String && out[start] != '\0') {
            if (n) out[n] = out[n + 1] = '\n';
            n = start + strlen(out + start);
          } else {
            out[n] = '\0';
          }
        } else {
          m = rd.next(nullptr, 0);
        }
        if (m == T::ObjectStart || m == T::ArrayStart) rd.skipValue(m);
      }
    } else if (t == T::ObjectStart || t == T::ArrayStart) {
      rd.skipValue(t);
    }
  }
  f.close();
  return n > 0;
}

}  // namespace mantras
