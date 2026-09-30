#pragma once

#include <cstddef>
#include <cstdint>

// The mantra collection on the SD card (/tools/mantras/mantras.json), read in
// place. The file is organised as:
//   vedic_mantras_collection: { <deity>: {deity_kannada, deity_english, mantras: [...]}, ...,
//                               additional_ritualistic_mantras: {mantras: [{..., category, sequence}]} }
//   kavacha_collection: [ {name_kannada, name_english, rishi, ..., phalashruti,
//                          sections: { <key>: {title_kannada, title_english, mantras: [...]} }} ]
// A one-time scan writes /tools/.cache/mantras.idx: the categories (deities,
// ritual categories, kavacha sections) and the byte offset of every mantra, so
// a mantra is read with one seek. The index is rebuilt when the file changes.
namespace mantras {

constexpr char kDir[] = "/tools/mantras";
constexpr char kLibraryPath[] = "/tools/mantras/mantras.json";

enum class Kind : uint8_t { Deity, Ritual, KavachaSection };

struct Category {
  Kind kind = Kind::Deity;
  uint8_t kavacha = 0;  // which kavacha a section belongs to
  uint16_t first = 0;   // index of its first mantra
  uint16_t count = 0;
  char key[24] = {};      // JSON key (deities) or category name (rituals)
  char kannada[72] = {};  // may be empty
  char english[56] = {};
};

struct Kavacha {
  uint32_t offset = 0;  // the kavacha object, for its "about" fields
  char kannada[72] = {};
  char english[56] = {};
};

struct Mantra {
  char kannada[2560] = {};
  char english[1152] = {};
  char meaningKannada[640] = {};
  char meaningEnglish[512] = {};
  char use[96] = {};
};

class Library {
 public:
  // Loads (or rebuilds) the index. Returns false when there is no library
  // file or it cannot be read.
  bool open();
  bool isOpen() const { return mantraCount_ > 0; }

  uint16_t categoryCount() const { return categoryCount_; }
  uint16_t mantraCount() const { return mantraCount_; }
  uint8_t kavachaCount() const { return kavachaCount_; }
  bool category(uint16_t i, Category& out) const;
  bool kavacha(uint8_t i, Kavacha& out) const;
  // Category by kind and position within that kind, or -1.
  int findCategory(Kind kind, int nth) const;
  int countOf(Kind kind) const;
  // Deity category by JSON key ("vishnu"), or -1.
  int findDeity(const char* key) const;

  // Reads mantra i (global index); fields that are absent stay empty.
  bool read(uint16_t i, Mantra& out) const;
  // Category of mantra i.
  int categoryOf(uint16_t i) const;

  // Kavacha "about" text: rishi, chandas, ... and phalashruti in one language,
  // one "label: value" paragraph per line, into out (always terminated).
  bool kavachaAbout(uint8_t i, bool kannada, char* out, size_t cap) const;

 private:
  bool build(uint32_t sourceSize);
  bool loadHeader();
  uint32_t mantraOffset(uint16_t i) const;

  // Kind and mantra range of every category, kept in RAM (names stay on the card).
  struct Brief {
    Kind kind;
    uint8_t kavacha;
    uint16_t first;
    uint16_t count;
  };
  static constexpr int kMaxCategories = 80;
  Brief briefs_[kMaxCategories];  // valid up to categoryCount_

  uint16_t categoryCount_ = 0;
  uint16_t mantraCount_ = 0;
  uint8_t kavachaCount_ = 0;
  uint32_t categoriesOffset_ = 0;
  uint32_t kavachasOffset_ = 0;
  uint32_t mantrasOffset_ = 0;
};

}  // namespace mantras
