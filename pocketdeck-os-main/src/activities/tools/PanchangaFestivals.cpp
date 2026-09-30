#include "PanchangaFestivals.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "ToolsDate.h"
#include "ToolsStore.h"

namespace festivals {

namespace {

constexpr char kIndexPath[] = "/tools/.cache/festivals.idx";
constexpr char kMagic[4] = {'F', 'I', 'X', '1'};
constexpr size_t kHeaderBytes = 32;
constexpr uint32_t kMaxRecords = 6000;  // 48 KB while building; a 100-year calendar has ~1,800
constexpr uint32_t kOffsetMask = 0x0FFFFFFF;

uint32_t fnv(uint32_t h, const void* data, const size_t n) {
  const auto* p = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < n; ++i) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}

bool endsWithJson(const char* name) {
  const size_t n = strlen(name);
  return n > 5 && strcasecmp(name + n - 5, ".json") == 0;
}

bool containsNoCase(const char* hay, const char* needle) {
  const size_t n = strlen(needle);
  for (const char* p = hay; *p; ++p) {
    if (strncasecmp(p, needle, n) == 0) return true;
  }
  return false;
}

// Layer files in name order, with a signature of their names and sizes.
int listLayers(char (*names)[kLayerNameCap], uint32_t& signature) {
  signature = 2166136261u;
  int count = 0;
  FsFile dir = Storage.open(kDir);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return 0;
  }
  char name[kLayerNameCap + 8];
  uint32_t sizes[kMaxLayers] = {};
  for (FsFile e = dir.openNextFile(); e; e = dir.openNextFile()) {
    const bool file = !e.isDirectory();
    e.getName(name, sizeof(name));
    const uint32_t size = static_cast<uint32_t>(e.fileSize());
    e.close();
    if (!file || name[0] == '.' || !endsWithJson(name) || strlen(name) >= kLayerNameCap) continue;
    if (count >= kMaxLayers) {
      LOG_ERR("FEST", "More than %d festival files; ignoring %s", kMaxLayers, name);
      continue;
    }
    snprintf(names[count], kLayerNameCap, "%s", name);
    sizes[count] = size;
    ++count;
  }
  dir.close();
  // Name order, so layers are listed predictably.
  for (int i = 1; i < count; ++i) {
    for (int j = i; j > 0 && strcasecmp(names[j - 1], names[j]) > 0; --j) {
      char tmp[kLayerNameCap];
      memcpy(tmp, names[j], kLayerNameCap);
      memcpy(names[j], names[j - 1], kLayerNameCap);
      memcpy(names[j - 1], tmp, kLayerNameCap);
      std::swap(sizes[j], sizes[j - 1]);
    }
  }
  for (int i = 0; i < count; ++i) {
    signature = fnv(signature, names[i], strlen(names[i]) + 1);
    signature = fnv(signature, &sizes[i], sizeof(sizes[i]));
  }
  return count;
}

struct BuildState {
  Ref* records;
  uint32_t count;
  uint16_t* masks;
  int minYear;
  int maxYear;
  bool truncated;
};

// Reads one festival object (the ObjectStart token was just consumed).
void indexObject(tools::JsonReader& rd, const uint32_t offset, const int layer, BuildState& st) {
  char key[32];
  char text[96];
  int32_t day = 0;
  bool haveDay = false;
  int mapped = -1;
  while (true) {
    auto t = rd.next(key, sizeof(key));
    if (t == tools::JsonReader::Token::ObjectEnd || t == tools::JsonReader::Token::End ||
        t == tools::JsonReader::Token::Error) {
      break;
    }
    if (t == tools::JsonReader::Token::Comma) continue;
    if (t != tools::JsonReader::Token::String) {
      rd.skipValue(t);
      continue;
    }
    if (rd.next(text, sizeof(text)) != tools::JsonReader::Token::Colon) break;
    const auto v = rd.next(text, sizeof(text));
    if (v == tools::JsonReader::Token::ObjectStart || v == tools::JsonReader::Token::ArrayStart) {
      rd.skipValue(v);
      continue;
    }
    if (v != tools::JsonReader::Token::String) continue;
    if (strcmp(key, "date") == 0) {
      haveDay = tools::parseIsoDate(text, day);
    } else if (strcmp(key, "english_name") == 0) {
      mapped = mappedFestival(text);
    }
  }
  if (!haveDay) return;
  if (st.count >= kMaxRecords) {
    st.truncated = true;
    return;
  }
  st.records[st.count++] = {day, static_cast<uint32_t>(layer) << 28 | (offset & kOffsetMask)};
  uint16_t y = 0;
  uint8_t m = 0, d = 0;
  tools::civilFromDays(day, y, m, d);
  st.minYear = std::min<int>(st.minYear, y);
  st.maxYear = std::max<int>(st.maxYear, y);
  if (mapped >= 0 && y >= 1900 && y < 1900 + 256) st.masks[y - 1900] |= static_cast<uint16_t>(1u << mapped);
}

// Walks one layer file: a top-level array of objects, or an object whose
// "festivals" member is that array.
void indexLayer(const char* path, const int layer, BuildState& st) {
  FsFile f;
  if (!Storage.openFileForRead("FEST", path, f)) {
    LOG_ERR("FEST", "Could not open %s", path);
    return;
  }
  tools::JsonReader rd(f);
  char text[40];
  using T = tools::JsonReader::Token;
  T t = rd.next(text, sizeof(text));
  bool inArray = t == T::ArrayStart;
  if (t == T::ObjectStart) {
    while (!inArray) {
      t = rd.next(text, sizeof(text));
      if (t == T::Comma) continue;
      if (t != T::String) break;
      const bool isFestivals = strcmp(text, "festivals") == 0;
      if (rd.next(text, sizeof(text)) != T::Colon) break;
      t = rd.next(text, sizeof(text));
      if (isFestivals && t == T::ArrayStart) {
        inArray = true;
      } else if (t == T::ObjectStart || t == T::ArrayStart) {
        rd.skipValue(t);
      }
    }
  }
  if (inArray) {
    while (true) {
      t = rd.next(text, sizeof(text));
      if (t == T::ArrayEnd || t == T::End || t == T::Error) break;
      if (t == T::ObjectStart) {
        indexObject(rd, rd.tokenOffset(), layer, st);
      } else if (t == T::ArrayStart) {
        rd.skipValue(t);
      }
    }
  } else {
    LOG_ERR("FEST", "%s has no festivals array", path);
  }
  f.close();
}

struct WriteCtx {
  const BuildState* st;
  const char (*names)[kLayerNameCap];
  int layers;
  uint32_t signature;
};

bool writeIndex(FsFile& out, void* ctx) {
  const auto* w = static_cast<const WriteCtx*>(ctx);
  const BuildState& st = *w->st;
  const int minYear = st.count ? st.minYear : 0;
  const int maxYear = st.count ? st.maxYear : -1;
  uint8_t h[kHeaderBytes] = {};
  memcpy(h, kMagic, 4);
  tools::putLe16(h + 4, 1);
  h[6] = static_cast<uint8_t>(w->layers);
  tools::putLe32(h + 8, st.count);
  tools::putLe32(h + 12, w->signature);
  tools::putLe16(h + 16, static_cast<uint16_t>(minYear));
  tools::putLe16(h + 18, static_cast<uint16_t>(maxYear));
  if (out.write(h, sizeof(h)) != sizeof(h)) return false;
  for (int i = 0; i < w->layers; ++i) {
    if (out.write(w->names[i], kLayerNameCap) != kLayerNameCap) return false;
  }
  for (int y = minYear; y <= maxYear; ++y) {
    uint8_t b[2];
    tools::putLe16(b, y >= 1900 && y < 1900 + 256 ? st.masks[y - 1900] : 0);
    if (out.write(b, 2) != 2) return false;
  }
  for (uint32_t i = 0; i < st.count; ++i) {
    uint8_t b[8];
    tools::putLe32(b, static_cast<uint32_t>(st.records[i].day));
    tools::putLe32(b + 4, st.records[i].loc);
    if (out.write(b, 8) != 8) return false;
  }
  return true;
}

}  // namespace

int mappedFestival(const char* name) {
  if (name == nullptr || *name == '\0') return -1;
  // A venue-specific entry (e.g. "Mysuru Dasara") is its own event.
  if (containsNoCase(name, "mysuru") || containsNoCase(name, "mysore")) return -1;
  if (containsNoCase(name, "ugadi")) return kUgadi;
  if (containsNoCase(name, "janmashtami") || containsNoCase(name, "gokulashtami")) return kJanmashtami;
  if (containsNoCase(name, "ganesh") || containsNoCase(name, "vinayaka chaturthi")) return kGaneshaChaturthi;
  if (containsNoCase(name, "navaratri")) return kNavaratriStart;
  if (containsNoCase(name, "vijayadashami")) return kVijayadashami;
  if (containsNoCase(name, "deepavali") || containsNoCase(name, "diwali")) return kDeepavali;
  if (containsNoCase(name, "shivaratri")) return kShivaratri;
  if (containsNoCase(name, "makara sankranti") || containsNoCase(name, "makar sankranti")) return kMakaraSankranti;
  return -1;
}

bool Calendar::loadHeader() {
  count_ = 0;
  layerCount_ = 0;
  minYear_ = 0;
  maxYear_ = -1;
  memset(masks_, 0, sizeof(masks_));
  FsFile f;
  if (!Storage.exists(kIndexPath) || !Storage.openFileForRead("FEST", kIndexPath, f)) return false;
  uint8_t h[kHeaderBytes];
  bool ok = f.read(h, sizeof(h)) == static_cast<int>(sizeof(h)) && memcmp(h, kMagic, 4) == 0 &&
            tools::le16(h + 4) == 1 && h[6] <= kMaxLayers;
  if (ok) {
    layerCount_ = h[6];
    count_ = tools::le32(h + 8);
    minYear_ = tools::le16(h + 16);
    maxYear_ = static_cast<int16_t>(tools::le16(h + 18));
    for (int i = 0; ok && i < layerCount_; ++i) {
      ok = f.read(layers_[i], kLayerNameCap) == kLayerNameCap;
      layers_[i][kLayerNameCap - 1] = '\0';
    }
    const int years = maxYear_ - minYear_ + 1;
    for (int y = 0; ok && y < years; ++y) {
      uint8_t b[2];
      ok = f.read(b, 2) == 2;
      if (y < kMaxYears) masks_[y] = tools::le16(b);
    }
    recordsOffset_ = static_cast<uint32_t>(kHeaderBytes + kLayerNameCap * layerCount_ + 2 * std::max(0, years));
  }
  f.close();
  if (!ok) count_ = 0;
  return ok;
}

bool Calendar::build(const uint32_t signature) {
  char names[kMaxLayers][kLayerNameCap] = {};
  uint32_t sig = 0;
  const int layers = listLayers(names, sig);
  auto records = makeUniqueNoThrow<Ref[]>(kMaxRecords);
  auto masks = makeUniqueNoThrow<uint16_t[]>(kMaxYears);
  if (!records || !masks) {
    LOG_ERR("FEST", "OOM building the festival index");
    return false;
  }
  memset(masks.get(), 0, sizeof(uint16_t) * kMaxYears);
  BuildState st{records.get(), 0, masks.get(), 9999, 0, false};
  char path[96];
  for (int i = 0; i < layers; ++i) {
    snprintf(path, sizeof(path), "%s/%s", kDir, names[i]);
    indexLayer(path, i, st);
  }
  if (st.truncated) LOG_ERR("FEST", "Festival index full at %lu entries", static_cast<unsigned long>(kMaxRecords));
  // Insertion sort: layers are usually in date order already, and this is
  // much smaller in flash than std::sort.
  for (uint32_t i = 1; i < st.count; ++i) {
    const Ref r = st.records[i];
    uint32_t j = i;
    while (j > 0 &&
           (st.records[j - 1].day > r.day || (st.records[j - 1].day == r.day && st.records[j - 1].loc > r.loc))) {
      st.records[j] = st.records[j - 1];
      --j;
    }
    st.records[j] = r;
  }
  Storage.ensureDirectoryExists("/tools/.cache");
  WriteCtx ctx{&st, names, layers, signature};
  if (!tools::writeFileAtomic(kIndexPath, &writeIndex, &ctx)) {
    LOG_ERR("FEST", "Could not write %s", kIndexPath);
    return false;
  }
  LOG_INF("FEST", "Indexed %lu festival entries from %d file(s)", static_cast<unsigned long>(st.count), layers);
  return loadHeader();
}

bool Calendar::refresh() {
  Storage.ensureDirectoryExists("/tools/panchanga");
  Storage.ensureDirectoryExists(kDir);
  tools::recoverFromBackup(kIndexPath);
  char names[kMaxLayers][kLayerNameCap] = {};
  uint32_t signature = 0;
  const int layers = listLayers(names, signature);
  // Reuse the index when its signature matches the files on the card.
  FsFile f;
  if (Storage.exists(kIndexPath) && Storage.openFileForRead("FEST", kIndexPath, f)) {
    uint8_t h[kHeaderBytes];
    const bool same = f.read(h, sizeof(h)) == static_cast<int>(sizeof(h)) && memcmp(h, kMagic, 4) == 0 &&
                      tools::le16(h + 4) == 1 && tools::le32(h + 12) == signature && h[6] == layers;
    f.close();
    if (same) return loadHeader() || layers == 0;
  }
  if (layers == 0) {
    Storage.remove(kIndexPath);
    loadHeader();
    return true;
  }
  return build(signature);
}

size_t Calendar::find(const int32_t first, const int32_t last, Ref* out, const size_t cap) const {
  if (count_ == 0 || cap == 0 || last < first) return 0;
  FsFile f;
  if (!Storage.openFileForRead("FEST", kIndexPath, f)) return 0;
  auto dayAt = [&](const uint32_t i, Ref* r) {
    uint8_t b[8];
    if (!f.seek(recordsOffset_ + 8u * i) || f.read(b, 8) != 8) return false;
    r->day = static_cast<int32_t>(tools::le32(b));
    r->loc = tools::le32(b + 4);
    return true;
  };
  // First record on or after `first`.
  uint32_t lo = 0, hi = count_;
  Ref r;
  while (lo < hi) {
    const uint32_t mid = (lo + hi) / 2;
    if (!dayAt(mid, &r)) {
      f.close();
      return 0;
    }
    if (r.day < first) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  size_t n = 0;
  for (uint32_t i = lo; i < count_ && n < cap; ++i) {
    if (!dayAt(i, &r) || r.day > last) break;
    out[n++] = r;
  }
  f.close();
  return n;
}

bool Calendar::read(const Ref& ref, Entry& out, const bool full) const {
  out = Entry{};
  const int layer = static_cast<int>(ref.loc >> 28);
  if (layer >= layerCount_) return false;
  out.layer = static_cast<uint8_t>(layer);
  char path[96];
  snprintf(path, sizeof(path), "%s/%s", kDir, layers_[layer]);
  FsFile f;
  if (!Storage.openFileForRead("FEST", path, f)) return false;
  tools::JsonReader rd(f);
  using T = tools::JsonReader::Token;
  char key[32];
  char text[224];
  bool ok = rd.seek(ref.loc & kOffsetMask) && rd.next(text, sizeof(text)) == T::ObjectStart;
  char region[64] = {};
  while (ok) {
    T t = rd.next(key, sizeof(key));
    if (t == T::ObjectEnd || t == T::End || t == T::Error) break;
    if (t == T::Comma) continue;
    if (t != T::String || rd.next(text, sizeof(text)) != T::Colon) break;
    t = rd.next(text, sizeof(text));
    if (t == T::ObjectStart || t == T::ArrayStart) {
      rd.skipValue(t);
      continue;
    }
    auto copy = [&](char* dst, const size_t cap) { snprintf(dst, cap, "%s", text); };
    if (strcmp(key, "kannada_name") == 0) {
      copy(out.kannada, sizeof(out.kannada));
    } else if (strcmp(key, "english_name") == 0) {
      copy(out.english, sizeof(out.english));
    } else if (strcmp(key, "type") == 0) {
      copy(out.type, sizeof(out.type));
    } else if (strcmp(key, "is_public_holiday") == 0 && t == T::Literal) {
      out.holiday = strcmp(text, "true") == 0 ? 1 : 0;
    } else if (!full) {
      continue;
    } else if (strcmp(key, "location_scope") == 0) {
      copy(out.scope, sizeof(out.scope));
    } else if (strcmp(key, "region") == 0 && t == T::String) {
      copy(region, sizeof(region));
    } else if (strcmp(key, "date_note") == 0) {
      copy(out.note, sizeof(out.note));
    } else if (strcmp(key, "significance") == 0) {
      copy(out.significance, sizeof(out.significance));
    }
  }
  f.close();
  if (out.scope[0] == '\0' && region[0] != '\0') snprintf(out.scope, sizeof(out.scope), "%s", region);
  if (out.english[0] == '\0' && out.kannada[0] == '\0') return false;
  return true;
}

uint16_t Calendar::yearMask(const int year) const {
  if (count_ == 0 || year < minYear_ || year > maxYear_) return 0;
  const int i = year - minYear_;
  return i < kMaxYears ? masks_[i] : 0;
}

}  // namespace festivals
