#pragma once

#include <cstdint>

#include "ToolsDate.h"

// Built-in cities for World Clock > hold Confirm > choose cities. Standard
// (winter) UTC offsets in minutes plus the DST rule the tools know about.
// Sorted by name; the table lives in flash. Any other place can still be
// added by hand to /tools/worldclock.txt ("Name|+5:30|NONE").
namespace tools {

struct CatalogCity {
  const char* name;
  int16_t offsetMinutes;
  DstRule rule;
};

inline constexpr CatalogCity kCityCatalog[] = {
    {"Abu Dhabi", 240, DstRule::None},
    {"Adelaide", 570, DstRule::AU},
    {"Amsterdam", 60, DstRule::EU},
    {"Anchorage", -540, DstRule::US},
    {"Athens", 120, DstRule::EU},
    {"Auckland", 720, DstRule::NZ},
    {"Bangkok", 420, DstRule::None},
    {"Beijing", 480, DstRule::None},
    {"Bengaluru", 330, DstRule::None},
    {"Berlin", 60, DstRule::EU},
    {"Bogota", -300, DstRule::None},
    {"Brisbane", 600, DstRule::None},
    {"Brussels", 60, DstRule::EU},
    {"Buenos Aires", -180, DstRule::None},
    {"Cairo", 120, DstRule::None},
    {"Cape Town", 120, DstRule::None},
    {"Chennai", 330, DstRule::None},
    {"Chicago", -360, DstRule::US},
    {"Colombo", 330, DstRule::None},
    {"Copenhagen", 60, DstRule::EU},
    {"Delhi", 330, DstRule::None},
    {"Denver", -420, DstRule::US},
    {"Dhaka", 360, DstRule::None},
    {"Doha", 180, DstRule::None},
    {"Dubai", 240, DstRule::None},
    {"Dublin", 0, DstRule::EU},
    {"Frankfurt", 60, DstRule::EU},
    {"Helsinki", 120, DstRule::EU},
    {"Ho Chi Minh City", 420, DstRule::None},
    {"Hong Kong", 480, DstRule::None},
    {"Honolulu", -600, DstRule::None},
    {"Hyderabad", 330, DstRule::None},
    {"Istanbul", 180, DstRule::None},
    {"Jakarta", 420, DstRule::None},
    {"Johannesburg", 120, DstRule::None},
    {"Karachi", 300, DstRule::None},
    {"Kathmandu", 345, DstRule::None},
    {"Kolkata", 330, DstRule::None},
    {"Kuala Lumpur", 480, DstRule::None},
    {"Lagos", 60, DstRule::None},
    {"Lima", -300, DstRule::None},
    {"Lisbon", 0, DstRule::EU},
    {"London", 0, DstRule::EU},
    {"Los Angeles", -480, DstRule::US},
    {"Madrid", 60, DstRule::EU},
    {"Manila", 480, DstRule::None},
    {"Melbourne", 600, DstRule::AU},
    {"Mexico City", -360, DstRule::None},
    {"Moscow", 180, DstRule::None},
    {"Mumbai", 330, DstRule::None},
    {"Muscat", 240, DstRule::None},
    {"Nairobi", 180, DstRule::None},
    {"New York", -300, DstRule::US},
    {"Oslo", 60, DstRule::EU},
    {"Paris", 60, DstRule::EU},
    {"Perth", 480, DstRule::None},
    {"Phoenix", -420, DstRule::None},
    {"Prague", 60, DstRule::EU},
    {"Riyadh", 180, DstRule::None},
    {"Rome", 60, DstRule::EU},
    {"San Francisco", -480, DstRule::US},
    {"Santiago", -240, DstRule::None},
    {"Sao Paulo", -180, DstRule::None},
    {"Seattle", -480, DstRule::US},
    {"Seoul", 540, DstRule::None},
    {"Shanghai", 480, DstRule::None},
    {"Singapore", 480, DstRule::None},
    {"Stockholm", 60, DstRule::EU},
    {"Sydney", 600, DstRule::AU},
    {"Taipei", 480, DstRule::None},
    {"Tehran", 210, DstRule::None},
    {"Tel Aviv", 120, DstRule::EU},  // Israel changes 2 days before the EU in spring
    {"Tokyo", 540, DstRule::None},
    {"Toronto", -300, DstRule::US},
    {"UTC", 0, DstRule::None},
    {"Vancouver", -480, DstRule::US},
    {"Vienna", 60, DstRule::EU},
    {"Warsaw", 60, DstRule::EU},
    {"Zurich", 60, DstRule::EU},
};

inline constexpr int kCityCatalogCount = static_cast<int>(sizeof(kCityCatalog) / sizeof(kCityCatalog[0]));

}  // namespace tools
