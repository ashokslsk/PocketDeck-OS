#pragma once

#include <cstdint>

// Simplified spaced-repetition schedule: each card keeps only its
// lastReviewed day and an interval in days (no SM-2 ease factors).
namespace srs {

constexpr uint16_t kMaxInterval = 180;

// remembered: 0 -> 1 -> 2 -> 4 -> ... capped at kMaxInterval.
// forgot: back to 0, i.e. due again immediately.
inline uint16_t nextInterval(const uint16_t interval, const bool remembered) {
  if (!remembered) return 0;
  if (interval == 0) return 1;
  const uint32_t doubled = static_cast<uint32_t>(interval) * 2;
  return static_cast<uint16_t>(doubled > kMaxInterval ? kMaxInterval : doubled);
}

// A never-reviewed card is always due.
inline bool isDue(const int32_t today, const bool reviewed, const int32_t lastReviewed, const uint16_t interval) {
  return !reviewed || today >= lastReviewed + interval;
}

// FNV-1a over the question text: stable card identity across deck edits.
inline uint32_t hashQuestion(const char* text) {
  uint32_t h = 2166136261u;
  for (const char* p = text; *p != '\0'; ++p) {
    h ^= static_cast<uint8_t>(*p);
    h *= 16777619u;
  }
  return h;
}

}  // namespace srs
