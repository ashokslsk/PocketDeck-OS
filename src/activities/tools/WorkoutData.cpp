#include "WorkoutData.h"

#include <HalStorage.h>
#include <Logging.h>
#include <strings.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ToolsStore.h"

namespace workout {

namespace {

using T = tools::JsonReader::Token;
using MemberFn = void (*)(tools::JsonReader& rd, const char* key, void* ctx);

// Walks the members of an object whose '{' was just read. fn must consume
// each value (with one of the readers below).
void members(tools::JsonReader& rd, const MemberFn fn, void* ctx) {
  char key[24];
  while (true) {
    const T t = rd.next(key, sizeof(key));
    if (t == T::Comma) continue;
    if (t != T::String || rd.next(nullptr, 0) != T::Colon) return;
    fn(rd, key, ctx);
  }
}

void skip(tools::JsonReader& rd) {
  const T t = rd.next(nullptr, 0);
  if (t == T::ObjectStart || t == T::ArrayStart) rd.skipValue(t);
}

void str(tools::JsonReader& rd, char* dst, const size_t cap) {
  const T t = rd.next(dst, cap);
  if (t == T::ObjectStart || t == T::ArrayStart) {
    rd.skipValue(t);
    dst[0] = '\0';
  }
}

long num(tools::JsonReader& rd) {
  char b[16];
  const T t = rd.next(b, sizeof(b));
  if (t == T::ObjectStart || t == T::ArrayStart) {
    rd.skipValue(t);
    return 0;
  }
  return atol(b);
}

// A number, or [beginner, advanced].
void pair(tools::JsonReader& rd, uint16_t out[2]) {
  char b[16];
  T t = rd.next(b, sizeof(b));
  if (t == T::Literal) {
    out[0] = out[1] = static_cast<uint16_t>(atol(b));
    return;
  }
  if (t != T::ArrayStart) {
    if (t == T::ObjectStart) rd.skipValue(t);
    return;
  }
  int n = 0;
  while ((t = rd.next(b, sizeof(b))) != T::ArrayEnd && t != T::End && t != T::Error) {
    if (t == T::Literal && n < 2) out[n++] = static_cast<uint16_t>(atol(b));
    if (t == T::ObjectStart || t == T::ArrayStart) rd.skipValue(t);
  }
  if (n == 1) out[1] = out[0];
}

void pair8(tools::JsonReader& rd, uint8_t out[2]) {
  uint16_t v[2] = {out[0], out[1]};
  pair(rd, v);
  out[0] = static_cast<uint8_t>(v[0] > 255 ? 255 : v[0]);
  out[1] = static_cast<uint8_t>(v[1] > 255 ? 255 : v[1]);
}

// An array of strings joined with sep (or a single string).
void listInto(tools::JsonReader& rd, char* dst, const size_t cap, const char* sep) {
  dst[0] = '\0';
  T t = rd.next(dst, cap);
  if (t == T::String) return;
  if (t != T::ArrayStart) {
    if (t == T::ObjectStart) rd.skipValue(t);
    dst[0] = '\0';
    return;
  }
  size_t n = 0;
  const size_t sepLen = strlen(sep);
  while (true) {
    const size_t at = n == 0 ? 0 : n + sepLen;
    t = rd.next(at + 2 < cap ? dst + at : nullptr, at + 2 < cap ? cap - at : 0);
    if (t == T::ArrayEnd || t == T::End || t == T::Error) break;
    if (t == T::Comma) continue;
    if (t == T::ObjectStart || t == T::ArrayStart) {
      rd.skipValue(t);
      continue;
    }
    if (at + 2 >= cap) continue;
    if (n > 0) memcpy(dst + n, sep, sepLen);
    n = at + strlen(dst + at);
  }
  dst[n < cap ? n : cap - 1] = '\0';
}

Type typeFrom(const char* s) {
  if (strcmp(s, "breath") == 0) return Type::Breath;
  if (strcmp(s, "sequence") == 0) return Type::Sequence;
  if (strcmp(s, "interval") == 0) return Type::Interval;
  return Type::Hold;
}

Shape shapeFrom(const char* s) {
  if (strcmp(s, "grow") == 0) return Shape::Grow;
  if (strcmp(s, "shrink") == 0) return Shape::Shrink;
  if (strcmp(s, "empty") == 0) return Shape::Empty;
  if (strcmp(s, "pulse") == 0) return Shape::Pulse;
  return Shape::Hold;
}

bool openManifest(const CategoryInfo& info, FsFile& f) {
  char path[96];
  snprintf(path, sizeof(path), "%s/%s", kManifestDir, info.file);
  if (!Storage.openFileForRead("WKT", path, f)) {
    LOG_ERR("WKT", "Could not open %s", path);
    return false;
  }
  return true;
}

// --- Category title (listCategories) ---
struct TitleCtx {
  CategoryInfo* info;
  bool done;
};
void onTitleMember(tools::JsonReader& rd, const char* key, void* p) {
  auto* c = static_cast<TitleCtx*>(p);
  if (c->done) {
    skip(rd);
  } else if (strcmp(key, "title_en") == 0) {
    str(rd, c->info->titleEn, sizeof(c->info->titleEn));
  } else if (strcmp(key, "title_kn") == 0) {
    str(rd, c->info->titleKn, sizeof(c->info->titleKn));
  } else {
    skip(rd);
  }
  if (c->info->titleEn[0] && c->info->titleKn[0]) c->done = true;
}

// --- Category (loadCategory) ---
void onItemInfoMember(tools::JsonReader& rd, const char* key, void* p) {
  auto* it = static_cast<ItemInfo*>(p);
  char type[16];
  if (strcmp(key, "name_en") == 0) {
    str(rd, it->nameEn, sizeof(it->nameEn));
  } else if (strcmp(key, "name_kn") == 0) {
    str(rd, it->nameKn, sizeof(it->nameKn));
  } else if (strcmp(key, "group") == 0) {
    str(rd, it->group, sizeof(it->group));
  } else if (strcmp(key, "type") == 0) {
    str(rd, type, sizeof(type));
    it->type = typeFrom(type);
  } else {
    skip(rd);
  }
}

void onCategoryMember(tools::JsonReader& rd, const char* key, void* p) {
  auto* c = static_cast<Category*>(p);
  if (strcmp(key, "note") == 0) {
    str(rd, c->note, sizeof(c->note));
  } else if (strcmp(key, "checklist") == 0) {
    T t = rd.next(nullptr, 0);
    if (t != T::ArrayStart) {
      if (t == T::ObjectStart) rd.skipValue(t);
      return;
    }
    while (true) {
      char* dst = c->checklistCount < kMaxChecklist ? c->checklist[c->checklistCount] : nullptr;
      t = rd.next(dst, dst ? sizeof(c->checklist[0]) : 0);
      if (t == T::ArrayEnd || t == T::End || t == T::Error) break;
      if (t == T::String && dst) ++c->checklistCount;
      if (t == T::ObjectStart || t == T::ArrayStart) rd.skipValue(t);
    }
  } else if (strcmp(key, "items") == 0) {
    T t = rd.next(nullptr, 0);
    if (t != T::ArrayStart) {
      if (t == T::ObjectStart) rd.skipValue(t);
      return;
    }
    while ((t = rd.next(nullptr, 0)) != T::ArrayEnd && t != T::End && t != T::Error) {
      if (t != T::ObjectStart) continue;
      if (c->itemCount >= kMaxItems) {
        rd.skipValue(t);
        continue;
      }
      ItemInfo& it = c->items[c->itemCount];
      it = ItemInfo{};
      it.offset = rd.tokenOffset();
      members(rd, &onItemInfoMember, &it);
      if (it.nameEn[0] || it.nameKn[0]) ++c->itemCount;
    }
  } else {
    skip(rd);
  }
}

// --- Item (loadItem) ---
struct PhaseCtx {
  Phase* phase;
};
void onPhaseMember(tools::JsonReader& rd, const char* key, void* p) {
  Phase* ph = static_cast<PhaseCtx*>(p)->phase;
  char shape[12];
  if (strcmp(key, "label") == 0) {
    str(rd, ph->label, sizeof(ph->label));
  } else if (strcmp(key, "seconds") == 0) {
    ph->seconds = static_cast<uint16_t>(num(rd));
  } else if (strcmp(key, "shape") == 0) {
    str(rd, shape, sizeof(shape));
    ph->shape = shapeFrom(shape);
  } else {
    skip(rd);
  }
}

void onStepMember(tools::JsonReader& rd, const char* key, void* p) {
  auto* st = static_cast<Step*>(p);
  if (strcmp(key, "name") == 0) {
    str(rd, st->name, sizeof(st->name));
  } else if (strcmp(key, "name_kn") == 0) {
    str(rd, st->nameKn, sizeof(st->nameKn));
  } else if (strcmp(key, "text") == 0) {
    str(rd, st->text, sizeof(st->text));
  } else if (strcmp(key, "image") == 0) {
    str(rd, st->image, sizeof(st->image));
  } else if (strcmp(key, "seconds") == 0) {
    st->seconds = static_cast<uint16_t>(num(rd));
  } else if (strcmp(key, "mantra_kn") == 0) {
    str(rd, st->mantraKn, sizeof(st->mantraKn));
  } else if (strcmp(key, "mantra_en") == 0) {
    str(rd, st->mantraEn, sizeof(st->mantraEn));
  } else if (strcmp(key, "meaning") == 0) {
    str(rd, st->meaning, sizeof(st->meaning));
  } else if (strcmp(key, "breath") == 0) {
    str(rd, st->breath, sizeof(st->breath));
  } else {
    skip(rd);
  }
}

void onVerseMember(tools::JsonReader& rd, const char* key, void* p) {
  auto* v = static_cast<Verse*>(p);
  if (strcmp(key, "section_kn") == 0) {
    str(rd, v->sectionKn, sizeof(v->sectionKn));
  } else if (strcmp(key, "section_en") == 0) {
    str(rd, v->sectionEn, sizeof(v->sectionEn));
  } else if (strcmp(key, "number") == 0) {
    v->number = static_cast<uint8_t>(num(rd));
  } else if (strcmp(key, "kn") == 0) {
    str(rd, v->kn, sizeof(v->kn));
  } else if (strcmp(key, "en") == 0) {
    str(rd, v->en, sizeof(v->en));
  } else if (strcmp(key, "meaning") == 0) {
    str(rd, v->meaning, sizeof(v->meaning));
  } else {
    skip(rd);
  }
}

struct VerseIndexCtx {
  uint32_t* offsets;
  int cap;
  int count;
  uint32_t opening;
};
void onVerseFileMember(tools::JsonReader& rd, const char* key, void* p) {
  auto* c = static_cast<VerseIndexCtx*>(p);
  T t = rd.next(nullptr, 0);
  if (strcmp(key, "opening") == 0 && t == T::ObjectStart) {
    c->opening = rd.tokenOffset();
    rd.skipValue(t);
  } else if (strcmp(key, "pages") == 0 && t == T::ArrayStart) {
    while ((t = rd.next(nullptr, 0)) != T::ArrayEnd && t != T::End && t != T::Error) {
      if (t != T::ObjectStart) continue;
      if (c->count < c->cap) c->offsets[c->count++] = rd.tokenOffset();
      rd.skipValue(t);
    }
  } else if (t == T::ObjectStart || t == T::ArrayStart) {
    rd.skipValue(t);
  }
}

bool openRel(const char* relPath, FsFile& f) {
  char path[112];
  snprintf(path, sizeof(path), "%s/%s", kDir, relPath);
  if (!Storage.exists(path) || !Storage.openFileForRead("WKT", path, f)) {
    LOG_ERR("WKT", "Could not open %s", path);
    return false;
  }
  return true;
}

void onItemMember(tools::JsonReader& rd, const char* key, void* p) {
  auto* it = static_cast<Item*>(p);
  char type[16];
  if (strcmp(key, "id") == 0) {
    str(rd, it->id, sizeof(it->id));
  } else if (strcmp(key, "name_en") == 0) {
    str(rd, it->nameEn, sizeof(it->nameEn));
  } else if (strcmp(key, "name_kn") == 0) {
    str(rd, it->nameKn, sizeof(it->nameKn));
  } else if (strcmp(key, "group") == 0) {
    str(rd, it->group, sizeof(it->group));
  } else if (strcmp(key, "image") == 0) {
    str(rd, it->image, sizeof(it->image));
  } else if (strcmp(key, "type") == 0) {
    str(rd, type, sizeof(type));
    it->type = typeFrom(type);
  } else if (strcmp(key, "minutes") == 0) {
    pair(rd, it->minutes);
  } else if (strcmp(key, "seconds") == 0) {
    pair(rd, it->seconds);
  } else if (strcmp(key, "rounds") == 0) {
    pair8(rd, it->rounds);
  } else if (strcmp(key, "step_seconds") == 0) {
    pair(rd, it->stepSeconds);
  } else if (strcmp(key, "work") == 0) {
    pair(rd, it->work);
  } else if (strcmp(key, "rest_seconds") == 0) {
    pair(rd, it->restSeconds);
  } else if (strcmp(key, "sets") == 0) {
    pair8(rd, it->sets);
  } else if (strcmp(key, "repeat") == 0) {
    it->repeat = static_cast<uint8_t>(num(rd));
  } else if (strcmp(key, "sides") == 0) {
    it->sides = static_cast<uint8_t>(num(rd));
  } else if (strcmp(key, "reps") == 0) {
    it->reps = static_cast<uint16_t>(num(rd));
  } else if (strcmp(key, "rest") == 0) {
    const T t = rd.next(nullptr, 0);
    if (t == T::ObjectStart) {
      PhaseCtx c{&it->rest};
      members(rd, &onPhaseMember, &c);
    } else if (t == T::ArrayStart) {
      rd.skipValue(t);
    }
  } else if (strcmp(key, "phases") == 0 || strcmp(key, "steps") == 0) {
    const bool phases = key[0] == 'p';
    T t = rd.next(nullptr, 0);
    if (t != T::ArrayStart) {
      if (t == T::ObjectStart) rd.skipValue(t);
      return;
    }
    while ((t = rd.next(nullptr, 0)) != T::ArrayEnd && t != T::End && t != T::Error) {
      if (t != T::ObjectStart) continue;
      if (phases && it->phaseCount < kMaxPhases) {
        Phase& ph = it->phases[it->phaseCount++];
        ph = Phase{};
        PhaseCtx c{&ph};
        members(rd, &onPhaseMember, &c);
        if (ph.seconds == 0) ph.seconds = 1;
      } else if (!phases && it->stepCount < kMaxSteps) {
        Step& st = it->steps[it->stepCount++];
        st = Step{};
        members(rd, &onStepMember, &st);
      } else {
        rd.skipValue(t);
      }
    }
  } else if (strcmp(key, "how") == 0) {
    listInto(rd, it->how, sizeof(it->how), "\n");
  } else if (strcmp(key, "accessories") == 0) {
    listInto(rd, it->accessories, sizeof(it->accessories), ", ");
  } else if (strcmp(key, "caution") == 0) {
    str(rd, it->caution, sizeof(it->caution));
  } else if (strcmp(key, "benefit") == 0) {
    str(rd, it->benefit, sizeof(it->benefit));
  } else if (strcmp(key, "mantra_pages") == 0) {
    str(rd, it->pages, sizeof(it->pages));
  } else {
    skip(rd);
  }
}

bool endsWithJson(const char* name) {
  const size_t n = strlen(name);
  return n > 5 && strcasecmp(name + n - 5, ".json") == 0;
}

}  // namespace

int listCategories(CategoryInfo* out, const int cap) {
  int count = 0;
  FsFile dir = Storage.open(kManifestDir);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return 0;
  }
  char name[40];
  for (FsFile e = dir.openNextFile(); e; e = dir.openNextFile()) {
    const bool file = !e.isDirectory();
    e.getName(name, sizeof(name));
    e.close();
    if (!file || name[0] == '.' || !endsWithJson(name) || strlen(name) >= sizeof(out[0].file) || count >= cap) continue;
    CategoryInfo& c = out[count];
    c = CategoryInfo{};
    snprintf(c.file, sizeof(c.file), "%s", name);
    // Key: the name without its "1_" order prefix and ".json".
    const char* k = name;
    while (*k >= '0' && *k <= '9') ++k;
    if (*k == '_' || *k == '-') ++k;
    snprintf(c.key, sizeof(c.key), "%.*s", static_cast<int>(strlen(k) - 5), k);
    ++count;
  }
  dir.close();
  // File-name order.
  for (int i = 1; i < count; ++i) {
    for (int j = i; j > 0 && strcmp(out[j - 1].file, out[j].file) > 0; --j) {
      const CategoryInfo tmp = out[j];
      out[j] = out[j - 1];
      out[j - 1] = tmp;
    }
  }
  for (int i = 0; i < count; ++i) {
    FsFile f;
    if (!openManifest(out[i], f)) continue;
    tools::JsonReader rd(f);
    if (rd.next(nullptr, 0) == T::ObjectStart) {
      TitleCtx c{&out[i], false};
      members(rd, &onTitleMember, &c);
    }
    f.close();
    if (out[i].titleEn[0] == '\0') snprintf(out[i].titleEn, sizeof(out[i].titleEn), "%s", out[i].key);
  }
  return count;
}

bool loadCategory(const CategoryInfo& info, Category& out) {
  out.info = info;
  out.note[0] = '\0';
  out.checklistCount = 0;
  out.itemCount = 0;
  FsFile f;
  if (!openManifest(info, f)) return false;
  tools::JsonReader rd(f);
  if (rd.next(nullptr, 0) == T::ObjectStart) members(rd, &onCategoryMember, &out);
  f.close();
  return out.itemCount > 0;
}

bool loadItem(const CategoryInfo& info, const uint32_t offset, Item& out) {
  memset(&out, 0, sizeof(out));
  out.minutes[0] = 5;
  out.minutes[1] = 10;
  out.repeat = 1;
  out.sides = 1;
  out.rounds[0] = out.rounds[1] = 1;
  out.sets[0] = out.sets[1] = 1;
  out.seconds[0] = 30;
  out.seconds[1] = 60;
  out.stepSeconds[0] = 30;
  out.stepSeconds[1] = 30;
  out.work[0] = out.work[1] = 30;
  out.restSeconds[0] = out.restSeconds[1] = 15;
  FsFile f;
  if (!openManifest(info, f)) return false;
  tools::JsonReader rd(f);
  const bool ok = rd.seek(offset) && rd.next(nullptr, 0) == T::ObjectStart;
  if (ok) members(rd, &onItemMember, &out);
  f.close();
  if (out.repeat == 0) out.repeat = 1;
  if (out.sides == 0) out.sides = 1;
  return ok && (out.nameEn[0] || out.nameKn[0]);
}

int indexVerses(const char* relPath, uint32_t* offsets, const int cap, uint32_t& opening) {
  opening = 0;
  FsFile f;
  if (relPath == nullptr || relPath[0] == '\0' || !openRel(relPath, f)) return 0;
  VerseIndexCtx c{offsets, cap, 0, 0};
  tools::JsonReader rd(f);
  if (rd.next(nullptr, 0) == T::ObjectStart) members(rd, &onVerseFileMember, &c);
  f.close();
  opening = c.opening;
  return c.count;
}

bool loadVerse(const char* relPath, const uint32_t offset, Verse& out) {
  memset(&out, 0, sizeof(out));
  FsFile f;
  if (offset == 0 || !openRel(relPath, f)) return false;
  tools::JsonReader rd(f);
  const bool ok = rd.seek(offset) && rd.next(nullptr, 0) == T::ObjectStart;
  if (ok) members(rd, &onVerseMember, &out);
  f.close();
  return ok && (out.kn[0] || out.en[0]);
}

}  // namespace workout
