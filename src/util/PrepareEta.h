#pragma once

#include <cstddef>
#include <string>

// Time readouts for the Prepare screens (book, folder, library).
namespace PrepareEta {
// "1 h 05 min", "12 min" or "under a minute".
std::string duration(unsigned long ms);
// The local clock time `ms` from now ("4:35 PM" / "16:35"), following the
// clock settings. False when the device has no valid clock.
bool clockAhead(unsigned long ms, char* buf, size_t size);
}  // namespace PrepareEta
