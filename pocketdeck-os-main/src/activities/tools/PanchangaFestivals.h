#pragma once

#include <cstddef>
#include <cstdint>

// Festival and holiday calendars from the SD card, in layers.
//
// Every *.json file in /tools/panchanga/festivals is one layer (for example a
// 1976-2075 master calendar, a Mysuru/venue layer, this year's government
// holiday list). Each holds {"festivals": [ {...}, ... ]} (or a bare array) of
// objects with at least "date": "YYYY-MM-DD" and a name; optional fields are
// shown when present: kannada_name, english_name, type, is_public_holiday,
// location_scope (or region), date_note, significance.
//
// The files are never loaded whole. A small index of (date, file, byte offset)
// sorted by date lives in /tools/.cache/festivals.idx and is rebuilt only when
// a layer is added, removed or changes size. Entries are read from their file
// when a date is shown.
namespace festivals {

constexpr char kDir[] = "/tools/panchanga/festivals";
constexpr int kMaxLayers = 8;
constexpr int kLayerNameCap = 40;

struct Ref {
  int32_t day = 0;   // days since 1970-01-01
  uint32_t loc = 0;  // layer << 28 | byte offset of the object in its file
};

struct Entry {
  char kannada[96] = {};
  char english[88] = {};
  char type[24] = {};
  char scope[64] = {};
  char note[200] = {};
  char significance[220] = {};
  int8_t holiday = -1;  // -1 unknown, 0 no, 1 yes
  uint8_t layer = 0;
};

// Festivals the Panchanga engine also calculates. When a layer lists one of
// these in a year, that layer's date is used for that year and the calculated
// date is hidden, so the two never disagree on screen.
enum Mapped : uint8_t {
  kUgadi,
  kJanmashtami,
  kGaneshaChaturthi,
  kNavaratriStart,
  kVijayadashami,
  kDeepavali,
  kShivaratri,
  kMakaraSankranti,
  kMappedCount
};
// Which engine festival an English name refers to, or -1.
int mappedFestival(const char* englishName);

class Calendar {
 public:
  // Rebuilds the index when a layer changed. Returns false only on a card
  // error; no layers at all is a valid, empty calendar.
  bool refresh();
  bool hasData() const { return count_ > 0; }
  uint32_t count() const { return count_; }
  int layerCount() const { return layerCount_; }
  const char* layerName(int i) const { return i >= 0 && i < layerCount_ ? layers_[i] : ""; }
  int firstYear() const { return minYear_; }
  int lastYear() const { return maxYear_; }

  // Entries dated first..last (inclusive), in date order. Returns how many
  // were written (at most cap).
  size_t find(int32_t first, int32_t last, Ref* out, size_t cap) const;
  // Reads one entry. With full = false only names, type and holiday are read.
  bool read(const Ref& ref, Entry& out, bool full) const;
  // Bit i set when some layer lists Mapped festival i in that year.
  uint16_t yearMask(int year) const;

 private:
  static constexpr int kMaxYears = 256;
  bool build(uint32_t signature);
  bool loadHeader();

  uint32_t count_ = 0;
  int layerCount_ = 0;
  int minYear_ = 0;
  int maxYear_ = -1;
  char layers_[kMaxLayers][kLayerNameCap] = {};
  uint16_t masks_[kMaxYears] = {};
  uint32_t recordsOffset_ = 0;
};

}  // namespace festivals
