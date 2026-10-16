#pragma once

#include <cstddef>
#include <cstdint>

// Workout content from the SD card: /tools/workout/manifests/*.json (one
// category per file, in file-name order) plus 1-bit PBM figures referenced by
// the manifests. Files are streamed; only the open category's item list and
// the open practice are held in RAM. Format: docs/productivity-tools.md.
namespace workout {

constexpr char kDir[] = "/tools/workout";
constexpr char kManifestDir[] = "/tools/workout/manifests";
constexpr int kMaxCategories = 8;
constexpr int kMaxItems = 40;
constexpr int kMaxPhases = 8;
constexpr int kMaxSteps = 12;
constexpr int kMaxChecklist = 8;

enum class Type : uint8_t { Breath, Hold, Sequence, Interval };
// How the breathing circle moves during a phase.
enum class Shape : uint8_t { Grow, Shrink, Hold, Empty, Pulse };

struct CategoryInfo {
  char file[32];
  char key[24];  // file name without order prefix and extension ("pranayama")
  char titleEn[40];
  char titleKn[72];
};

struct ItemInfo {
  uint32_t offset;  // the item object in its manifest
  char nameEn[56];
  char nameKn[56];
  char group[24];
  Type type;
};

struct Category {
  CategoryInfo info;
  char note[400];
  char checklist[kMaxChecklist][36];
  uint8_t checklistCount;
  ItemInfo items[kMaxItems];
  uint8_t itemCount;
};

struct Phase {
  char label[32];
  uint16_t seconds;
  Shape shape;
};

struct Step {
  char name[64];
  char nameKn[64];
  char text[160];
  char image[48];
  char mantraKn[72];  // optional: a mantra chanted in this pose (Surya Namaskara)
  char mantraEn[48];
  char meaning[128];
  char breath[32];
  uint16_t seconds;  // 0 = use the practice's step_seconds for the level
};

// One page of a verse file (mantra_pages): a verse with its section title.
struct Verse {
  char sectionKn[96];
  char sectionEn[64];
  uint8_t number;  // 0 = unnumbered
  char kn[640];
  char en[288];
  char meaning[320];
};
constexpr int kMaxVerses = 48;

struct Item {
  char id[32];
  char nameEn[64];
  char nameKn[64];
  char group[24];
  char image[48];
  Type type;
  // Two levels: [0] beginner, [1] advanced.
  uint16_t minutes[2];  // breath
  Phase phases[kMaxPhases];
  uint8_t phaseCount;
  uint8_t repeat;       // breath: phase pattern repeats before the rest phase
  Phase rest;           // seconds 0 = no rest phase
  uint16_t seconds[2];  // hold
  uint8_t sides;
  uint8_t rounds[2];  // sequence
  uint16_t stepSeconds[2];
  Step steps[kMaxSteps];
  uint8_t stepCount;
  uint16_t work[2];  // interval
  uint16_t restSeconds[2];
  uint8_t sets[2];
  uint16_t reps;   // target reps per set (0 = none)
  char how[1400];  // one instruction per line
  char accessories[240];
  char caution[320];
  char benefit[96];
  char pages[48];  // optional verse file (relative to kDir): an opening verse and closing pages
};

// Lists the category manifests (titles only). Returns how many were found.
int listCategories(CategoryInfo* out, int cap);
// Reads one category's note, checklist and item list.
bool loadCategory(const CategoryInfo& info, Category& out);
// Reads one practice in full.
bool loadItem(const CategoryInfo& info, uint32_t offset, Item& out);
// Indexes a verse file {"opening": {...}, "pages": [{...}, ...]}: page offsets
// into `offsets` (returns the count) and the opening verse's offset (0 if none).
int indexVerses(const char* relPath, uint32_t* offsets, int cap, uint32_t& opening);
bool loadVerse(const char* relPath, uint32_t offset, Verse& out);

}  // namespace workout

class GfxRenderer;
namespace workout {
// Draws a PBM figure (path relative to /tools/workout) with its top-left at
// x,y, each pixel scale x scale. Returns false if the file is missing or bad.
bool drawFigure(const GfxRenderer& r, const char* relPath, int x, int y, int scale, bool black = true);
}  // namespace workout
