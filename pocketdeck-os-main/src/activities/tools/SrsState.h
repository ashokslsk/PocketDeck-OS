#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ToolsDate.h"
#include "ToolsStore.h"

// One entry of /tools/srs_state.json: {"deck","id","lastReviewed","interval"}.
// Shared by Flashcards (scheduling) and its stats page (mastery summary).
namespace srs {

struct StateEntry {
  char deck[40];
  uint32_t hash;
  int32_t lastDay;
  uint16_t interval;
};

// Reads the fields of one state object; the opening '{' was already consumed.
inline bool readStateObject(tools::JsonReader& r, StateEntry& e) {
  using Tok = tools::JsonReader::Token;
  char key[16];
  char value[48];
  e = StateEntry{};
  while (true) {
    Tok t = r.next(key, sizeof(key));
    if (t == Tok::Comma) continue;
    if (t == Tok::ObjectEnd) return true;
    if (t != Tok::String) return false;
    if (r.next(nullptr, 0) != Tok::Colon) return false;
    t = r.next(value, sizeof(value));
    if (t == Tok::ObjectStart || t == Tok::ArrayStart) {
      if (!r.skipValue(t)) return false;
      continue;
    }
    if (strcmp(key, "deck") == 0) snprintf(e.deck, sizeof(e.deck), "%s", value);
    if (strcmp(key, "id") == 0) e.hash = static_cast<uint32_t>(strtoul(value, nullptr, 16));
    if (strcmp(key, "lastReviewed") == 0) tools::parseIsoDate(value, e.lastDay);
    if (strcmp(key, "interval") == 0) e.interval = static_cast<uint16_t>(std::min(atoi(value), 3650));
  }
}

}  // namespace srs
