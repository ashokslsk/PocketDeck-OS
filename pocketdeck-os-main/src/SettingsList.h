#pragma once

#include <CrossInkHalFrontlight.h>
#include <HalClock.h>
#include <HalGPIO.h>
#include <HalTiltSensor.h>
#include <I18n.h>
#include <SdCardFontRegistry.h>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "CrossPointSettings.h"
#include "DeviceCapabilities.h"
#include "KOReaderCredentialStore.h"
#include "QuickActions.h"
#include "activities/settings/SettingsActivity.h"
#include "util/Dictionary.h"
#include "util/DictionaryRegistry.h"
#include "util/FontFamilyLabel.h"
#include "util/FrontlightSchedule.h"

inline std::string fontSizePointLabel(const uint8_t pointSize) { return std::to_string(pointSize) + " pt"; }

inline void appendBuiltinFontSizeOption(SettingInfo& setting, const CrossPointSettings::FONT_SIZE size) {
  const uint8_t pointSize = CrossPointSettings::getReaderFontPointSize(size);
  setting.enumStringValues.push_back(fontSizePointLabel(pointSize));
  setting.enumRawValues.push_back(pointSize);
}

inline SettingInfo buildBuiltinFontSizeSetting() {
  SettingInfo s;
  s.nameId = StrId::STR_FONT_SIZE;
  s.type = SettingType::ENUM;
  s.valuePtr = &CrossPointSettings::readerFontPointSize;
  s.key = "fontSize";
  s.category = StrId::STR_CAT_READER;
  s.enumStringValues.reserve(CrossPointSettings::FONT_SIZE_COUNT);
  s.enumRawValues.reserve(CrossPointSettings::FONT_SIZE_COUNT);

  appendBuiltinFontSizeOption(s, CrossPointSettings::TINY);
  appendBuiltinFontSizeOption(s, CrossPointSettings::SMALL);
  appendBuiltinFontSizeOption(s, CrossPointSettings::MEDIUM);
  appendBuiltinFontSizeOption(s, CrossPointSettings::LARGE);

  return s;
}

inline SettingInfo buildSdFontSizeSetting(const SdCardFontFamilyInfo& family) {
  SettingInfo s;
  s.nameId = StrId::STR_FONT_SIZE;
  s.type = SettingType::ENUM;
  s.valuePtr = &CrossPointSettings::readerFontPointSize;
  s.key = "fontSize";
  s.category = StrId::STR_CAT_READER;

  const std::vector<uint8_t> sizes = family.availableSizes();
  s.enumStringValues.reserve(sizes.size());
  s.enumRawValues.reserve(sizes.size());
  for (size_t i = 0; i < sizes.size(); i++) {
    s.enumStringValues.push_back(fontSizePointLabel(sizes[i]));
    s.enumRawValues.push_back(sizes[i]);
  }
  return s;
}

inline void removeEnumRawValue(SettingInfo& setting, const uint8_t rawValue) {
  const auto it = std::find(setting.enumRawValues.begin(), setting.enumRawValues.end(), rawValue);
  if (it == setting.enumRawValues.end()) {
    return;
  }

  const size_t index = static_cast<size_t>(std::distance(setting.enumRawValues.begin(), it));
  setting.enumRawValues.erase(it);
  if (index < setting.enumValues.size()) {
    setting.enumValues.erase(setting.enumValues.begin() + index);
  }
}

inline bool settingKeyIs(const SettingInfo& setting, const char* key) {
  return setting.key && std::strcmp(setting.key, key) == 0;
}

inline SettingInfo buildFontSizeSetting(const SdCardFontRegistry* registry) {
  if (registry && SETTINGS.sdFontFamilyName[0] != '\0') {
    const SdCardFontFamilyInfo* family = registry->findFamily(SETTINGS.sdFontFamilyName);
    if (family && !family->files.empty()) {
      return buildSdFontSizeSetting(*family);
    }
  }
  return buildBuiltinFontSizeSetting();
}

inline uint8_t closestPointSizeIndex(const std::vector<uint8_t>& sizes, const uint8_t targetPointSize) {
  if (sizes.empty()) return 0;

  uint8_t bestIndex = 0;
  uint8_t bestDiff = UINT8_MAX;
  for (size_t i = 0; i < sizes.size(); i++) {
    const uint8_t size = sizes[i];
    const uint8_t diff = size > targetPointSize ? size - targetPointSize : targetPointSize - size;
    if (diff < bestDiff || (diff == bestDiff && size < sizes[bestIndex])) {
      bestIndex = static_cast<uint8_t>(i);
      bestDiff = diff;
    }
  }
  return bestIndex;
}

inline uint8_t closestBuiltinFontSizeIndex(const uint8_t targetPointSize) {
  uint8_t bestStored = 0;
  uint8_t bestPointSize = 0;
  uint8_t bestDiff = UINT8_MAX;

  for (uint8_t i = 0; i < CrossPointSettings::FONT_SIZE_COUNT; i++) {
    const auto size = static_cast<CrossPointSettings::FONT_SIZE>(i);
    const uint8_t stored = CrossPointSettings::getStoredReaderFontSize(size);
    if (stored == UINT8_MAX) continue;

    const uint8_t pointSize = CrossPointSettings::getReaderFontPointSize(size);
    const uint8_t diff = pointSize > targetPointSize ? pointSize - targetPointSize : targetPointSize - pointSize;
    if (diff < bestDiff || (diff == bestDiff && pointSize < bestPointSize)) {
      bestStored = stored;
      bestPointSize = pointSize;
      bestDiff = diff;
    }
  }
  return bestStored;
}

// Build the font family setting dynamically. When registry is non-null, SD card fonts
// are appended after the built-in fonts. Otherwise only built-in fonts are listed.
inline SettingInfo buildFontFamilySetting(const SdCardFontRegistry* registry) {
  // Built-in font labels (StrId)
  std::vector<StrId> enumValues = {StrId::STR_LEXEND_DECA, StrId::STR_BITTER};
  // Runtime string labels for SD card fonts
  std::vector<std::string> enumStringValues;

  // Reserve: first CrossPointSettings::BUILTIN_FONT_COUNT entries use StrId, rest use strings
  if (registry) {
    const auto& families = registry->getFamilies();
    enumStringValues.reserve(families.size());
    std::transform(families.begin(), families.end(), std::back_inserter(enumStringValues),
                   [](const SdCardFontFamilyInfo& f) { return fontFamilyLabel(f.name, fontFamilyPointSizeRange(f)); });
  }

  // Capture the SD font count for the lambdas
  const int sdFontCount = static_cast<int>(enumStringValues.size());

  // Total option count = built-in + SD card families
  // For the combined enumStringValues: we need all entries as strings (built-in names + SD names)
  // The render code checks enumStringValues first, then enumValues. So we build enumStringValues
  // with all options when SD fonts are present.
  std::vector<std::string> allStringValues;
  if (sdFontCount > 0) {
    constexpr FontFamilyPointSizeRange builtinRange{10, 16};
    allStringValues.push_back(fontFamilyLabel(I18N.get(StrId::STR_LEXEND_DECA), builtinRange));
    allStringValues.push_back(fontFamilyLabel(I18N.get(StrId::STR_BITTER), builtinRange));
    allStringValues.insert(allStringValues.end(), enumStringValues.begin(), enumStringValues.end());
  }

  SettingInfo s;
  s.nameId = StrId::STR_FONT_FAMILY;
  s.type = SettingType::ENUM;
  s.enumValues = std::move(enumValues);
  s.enumStringValues = std::move(allStringValues);
  s.key = "fontFamily";
  s.category = StrId::STR_CAT_READER;

  // Capture registry families by copy for the lambdas
  std::vector<std::string> sdFamilyNames;

  if (registry) {
    const auto& families = registry->getFamilies();
    sdFamilyNames.reserve(families.size());

    std::transform(families.begin(), families.end(), std::back_inserter(sdFamilyNames),
                   [](const SdCardFontFamilyInfo& f) { return f.name; });
  }

  s.valueGetter = [sdFamilyNames]() -> uint8_t {
    // If an SD card font is selected, find its index
    if (SETTINGS.sdFontFamilyName[0] != '\0') {
      for (int i = 0; i < static_cast<int>(sdFamilyNames.size()); i++) {
        if (sdFamilyNames[i] == SETTINGS.sdFontFamilyName) {
          return static_cast<uint8_t>(CrossPointSettings::BUILTIN_FONT_COUNT + i);
        }
      }
      // SD font name not found in registry — fall through to built-in
    }
    return SETTINGS.fontFamily < CrossPointSettings::BUILTIN_FONT_COUNT ? SETTINGS.fontFamily : 0;
  };

  s.valueSetter = [sdFamilyNames, registry](uint8_t v) {
    const uint8_t targetPointSize = SETTINGS.readerFontPointSize;

    if (v < CrossPointSettings::BUILTIN_FONT_COUNT) {
      SETTINGS.fontFamily = v;
      SETTINGS.sdFontFamilyName[0] = '\0';
      SETTINGS.readerFontPointSize = CrossPointSettings::getReaderFontPointSize(
          static_cast<CrossPointSettings::FONT_SIZE>(closestBuiltinFontSizeIndex(targetPointSize)));
    } else {
      int sdIdx = v - CrossPointSettings::BUILTIN_FONT_COUNT;
      if (sdIdx < static_cast<int>(sdFamilyNames.size())) {
        const auto* family = registry ? registry->findFamily(sdFamilyNames[sdIdx]) : nullptr;
        const auto sizes = family ? family->availableSizes() : std::vector<uint8_t>{};
        if (sizes.empty()) return;
        SETTINGS.readerFontPointSize = sizes[closestPointSizeIndex(sizes, targetPointSize)];
        strncpy(SETTINGS.sdFontFamilyName, sdFamilyNames[sdIdx].c_str(), sizeof(SETTINGS.sdFontFamilyName) - 1);
        SETTINGS.sdFontFamilyName[sizeof(SETTINGS.sdFontFamilyName) - 1] = '\0';
      }
    }
  };

  return s;
}

inline SettingInfo buildDictionaryFontFamilySetting(const SdCardFontRegistry* registry) {
  SettingInfo s;
  s.nameId = StrId::STR_DICTIONARY_FONT;
  s.type = SettingType::ENUM;
  s.key = "dictionaryFont";
  s.category = StrId::STR_CAT_READER;
  s.enumStringValues.push_back(I18N.get(StrId::STR_USE_READER_FONT));

  std::vector<std::string> familyNames;
  if (registry) {
    const auto& families = registry->getFamilies();
    familyNames.reserve(families.size());
    s.enumStringValues.reserve(families.size() + 1);
    for (const auto& family : families) {
      familyNames.push_back(family.name);
      s.enumStringValues.push_back(family.name);
    }
  }

  s.valueGetter = [familyNames]() -> uint8_t {
    for (size_t i = 0; i < familyNames.size(); ++i) {
      if (familyNames[i] == SETTINGS.dictionarySdFontFamilyName) return static_cast<uint8_t>(i + 1);
    }
    return 0;
  };
  s.valueSetter = [familyNames](const uint8_t value) {
    if (value == 0 || value > familyNames.size()) {
      SETTINGS.dictionarySdFontFamilyName[0] = '\0';
      SETTINGS.dictionaryFontPointSize = 0;
      return;
    }
    strncpy(SETTINGS.dictionarySdFontFamilyName, familyNames[value - 1].c_str(),
            sizeof(SETTINGS.dictionarySdFontFamilyName) - 1);
    SETTINGS.dictionarySdFontFamilyName[sizeof(SETTINGS.dictionarySdFontFamilyName) - 1] = '\0';
  };
  return s;
}

inline SettingInfo buildDictionaryFontSizeSetting(const SdCardFontRegistry* registry) {
  SettingInfo s;
  s.nameId = StrId::STR_DICTIONARY_FONT_SIZE;
  s.type = SettingType::ENUM;
  s.valuePtr = &CrossPointSettings::dictionaryFontPointSize;
  s.key = "dictionaryFontSize";
  s.category = StrId::STR_CAT_READER;
  s.enumStringValues.push_back(I18N.get(StrId::STR_USE_READER_FONT_SIZE));
  s.enumRawValues.push_back(0);

  if (!registry) return s;
  // With no dedicated dictionary family, a non-zero dictionary size applies
  // to the reader's SD-card family. Built-in reader fonts have no selectable
  // files, so they deliberately retain just the "use reader size" entry.
  const char* familyName =
      SETTINGS.dictionarySdFontFamilyName[0] != '\0' ? SETTINGS.dictionarySdFontFamilyName : SETTINGS.sdFontFamilyName;
  if (familyName[0] == '\0') return s;
  const auto* family = registry->findFamily(familyName);
  if (!family) return s;

  const auto sizes = family->availableSizes();
  s.enumStringValues.reserve(sizes.size() + 1);
  s.enumRawValues.reserve(sizes.size() + 1);
  for (const uint8_t pointSize : sizes) {
    s.enumStringValues.push_back(fontSizePointLabel(pointSize));
    s.enumRawValues.push_back(pointSize);
  }
  return s;
}

inline SettingInfo buildDictionarySetting(const DictionaryRegistry* dictRegistry) {
  SettingInfo s;
  s.nameId = StrId::STR_DICTIONARY;
  s.type = SettingType::ENUM;
  s.key = "dictionary";
  s.category = StrId::STR_CAT_READER;
  s.enumStringValues.push_back(I18N.get(StrId::STR_DICT_NONE));

  std::vector<DictionaryEntry> entries;
  if (dictRegistry) {
    entries = dictRegistry->getEntries();
    s.enumStringValues.reserve(entries.size() + 1);
    for (const auto& entry : entries) {
      s.enumStringValues.push_back(entry.name);
    }
  }

  s.valueGetter = [entries]() -> uint8_t {
    const std::string activePath = Dictionary::readDictPath();
    if (activePath.empty()) {
      return 0;
    }
    for (size_t i = 0; i < entries.size(); i++) {
      if (entries[i].basePath == activePath) {
        return static_cast<uint8_t>(i + 1);
      }
    }
    return 0;
  };

  s.valueSetter = [entries](uint8_t v) {
    if (v == 0) {
      Dictionary::saveGlobalDictPath("");
      return;
    }
    const size_t entryIndex = static_cast<size_t>(v - 1);
    if (entryIndex < entries.size()) {
      Dictionary::saveGlobalDictPath(entries[entryIndex].basePath.c_str());
    }
  };

  return s;
}

inline SettingInfo buildSleepScreenSetting() {
  SettingInfo s = SettingInfo::Enum(
      StrId::STR_SLEEP_SCREEN, &CrossPointSettings::sleepScreen,
      {StrId::STR_NONE_OPT, StrId::STR_DARK, StrId::STR_LIGHT, StrId::STR_CUSTOM, StrId::STR_COVER,
       StrId::STR_COVER_CUSTOM, StrId::STR_PAGE_OVERLAY, StrId::STR_READING_STATS, StrId::STR_THEME_MINIMAL,
       StrId::STR_THEME_MINIMAL_STATS, StrId::STR_THEME_DASHBOARD, StrId::STR_QUICK_RESUME},
      "sleepScreen", StrId::STR_CAT_DISPLAY);
  s.withEnumRawValues({
      static_cast<uint8_t>(CrossPointSettings::BLANK),
      static_cast<uint8_t>(CrossPointSettings::DARK),
      static_cast<uint8_t>(CrossPointSettings::LIGHT),
      static_cast<uint8_t>(CrossPointSettings::CUSTOM),
      static_cast<uint8_t>(CrossPointSettings::COVER),
      static_cast<uint8_t>(CrossPointSettings::COVER_CUSTOM),
      static_cast<uint8_t>(CrossPointSettings::OVERLAY),
      static_cast<uint8_t>(CrossPointSettings::READING_STATS_SLEEP),
      static_cast<uint8_t>(CrossPointSettings::MINIMAL_SLEEP),
      static_cast<uint8_t>(CrossPointSettings::MINIMAL_STATS_SLEEP),
      static_cast<uint8_t>(CrossPointSettings::DASHBOARD_SLEEP),
      static_cast<uint8_t>(CrossPointSettings::QUICK_RESUME),
  });
  return s;
}

enum class ShortcutOptionCatalog { PowerButton, ButtonChord, LongPress, HomeButton };

constexpr uint8_t SHORTCUT_OPTION_UNAVAILABLE = UINT8_MAX;

inline uint8_t shortcutRawValue(const ShortcutOptionCatalog catalog, const CrossPointSettings::SHORT_PWRBTN action) {
  using Action = CrossPointSettings::SHORT_PWRBTN;
  using LongPress = CrossPointSettings::LONG_PRESS_MENU_ACTION;
  using Chord = CrossPointSettings::POWER_CHORD_ACTION;

  switch (catalog) {
    case ShortcutOptionCatalog::PowerButton:
      return static_cast<uint8_t>(action);
    case ShortcutOptionCatalog::ButtonChord:
      switch (action) {
        case Action::IGNORE:
          return Chord::CHORD_DISABLED;
        // Deep sleep wakes from the Power GPIO alone. A chord cannot be used
        // as the matching wake gesture, so do not offer a misleading action.
        case Action::SLEEP:
          return SHORTCUT_OPTION_UNAVAILABLE;
        case Action::PAGE_TURN:
          return Chord::CHORD_PAGE_TURN;
        case Action::PREVIOUS_PAGE:
          return Chord::CHORD_PREVIOUS_PAGE;
        case Action::TOGGLE_BOOKMARK:
          return Chord::CHORD_TOGGLE_BOOKMARK;
        case Action::READING_STATS:
          return Chord::CHORD_READING_STATS;
        case Action::MARK_FINISHED:
          return Chord::CHORD_MARK_FINISHED;
        case Action::FORCE_REFRESH:
          return Chord::CHORD_FORCE_REFRESH;
        case Action::TOGGLE_FONT:
          return Chord::CHORD_TOGGLE_FONT;
        case Action::TOGGLE_GUIDE_DOTS:
          return Chord::CHORD_TOGGLE_GUIDE_DOTS;
        case Action::TOGGLE_FOCUS_READING:
          return Chord::CHORD_TOGGLE_FOCUS_READING;
        case Action::CYCLE_PAGE_TURN:
          return Chord::CHORD_CYCLE_PAGE_TURN;
        case Action::SYNC_PROGRESS:
          return Chord::CHORD_SYNC_PROGRESS;
        case Action::NEARBY_POSITION_SYNC:
          return Chord::CHORD_NEARBY_POSITION_SYNC;
        case Action::FILE_TRANSFER:
          return Chord::CHORD_FILE_TRANSFER;
        case Action::CALIBRE_WIRELESS:
          return Chord::CHORD_CALIBRE_WIRELESS;
        case Action::JOIN_NETWORK:
          return Chord::CHORD_JOIN_NETWORK;
        case Action::CREATE_HOTSPOT:
          return Chord::CHORD_CREATE_HOTSPOT;
        case Action::SCREENSHOT:
          return Chord::CHORD_SCREENSHOT;
        case Action::TOGGLE_DARK_MODE:
          return Chord::CHORD_TOGGLE_DARK_MODE;
        case Action::FOOTNOTES:
          return Chord::CHORD_FOOTNOTES;
        case Action::FILE_BROWSER:
          return Chord::CHORD_FILE_BROWSER;
        case Action::CREATE_CLIPPING:
          return Chord::CHORD_CREATE_CLIPPING;
        case Action::LOOKUP_WORD:
          return Chord::CHORD_LOOKUP_WORD;
        case Action::TOGGLE_HOME_BUTTON_IN_READER:
          return Chord::CHORD_TOGGLE_HOME_BUTTON;
        case Action::QUICK_ACTIONS:
          return Chord::CHORD_QUICK_ACTIONS;
        case Action::TOGGLE_FRONTLIGHT:
          return Chord::CHORD_TOGGLE_FRONTLIGHT;
        case Action::TOGGLE_TOUCHSCREEN:
          return Chord::CHORD_TOGGLE_TOUCHSCREEN;
        case Action::QUICK_LOCK:
          return Chord::CHORD_QUICK_LOCK;
        case Action::TOGGLE_TILT_PAGE_TURN:
          return SHORTCUT_OPTION_UNAVAILABLE;
        default:
          return SHORTCUT_OPTION_UNAVAILABLE;
      }
      break;
    case ShortcutOptionCatalog::LongPress:
      switch (action) {
        case Action::IGNORE:
          return LongPress::LONG_MENU_OFF;
        case Action::SLEEP:
          return LongPress::LONG_MENU_SLEEP;
        case Action::TOGGLE_BOOKMARK:
          return LongPress::LONG_MENU_TOGGLE_BOOKMARK;
        case Action::READING_STATS:
          return LongPress::LONG_MENU_READING_STATS;
        case Action::MARK_FINISHED:
          return LongPress::LONG_MENU_MARK_FINISHED;
        case Action::FORCE_REFRESH:
          return LongPress::LONG_MENU_REFRESH_SCREEN;
        case Action::TOGGLE_FONT:
          return LongPress::LONG_MENU_CHANGE_FONT;
        case Action::TOGGLE_GUIDE_DOTS:
          return LongPress::LONG_MENU_TOGGLE_GUIDE_DOTS;
        case Action::TOGGLE_FOCUS_READING:
          return LongPress::LONG_MENU_TOGGLE_FOCUS;
        case Action::CYCLE_PAGE_TURN:
          return LongPress::LONG_MENU_CYCLE_PAGE_TURN;
        case Action::TOGGLE_TILT_PAGE_TURN:
          return LongPress::LONG_MENU_TOGGLE_TILT_PAGE_TURN;
        case Action::SYNC_PROGRESS:
          return LongPress::LONG_MENU_SYNC_PROGRESS;
        case Action::FILE_TRANSFER:
          return LongPress::LONG_MENU_FILE_TRANSFER;
        case Action::CALIBRE_WIRELESS:
          return LongPress::LONG_MENU_CALIBRE_WIRELESS;
        case Action::JOIN_NETWORK:
          return LongPress::LONG_MENU_JOIN_NETWORK;
        case Action::CREATE_HOTSPOT:
          return LongPress::LONG_MENU_CREATE_HOTSPOT;
        case Action::SCREENSHOT:
          return LongPress::LONG_MENU_SCREENSHOT;
        case Action::TOGGLE_DARK_MODE:
          return LongPress::LONG_MENU_TOGGLE_DARK_MODE;
        case Action::FOOTNOTES:
          return LongPress::LONG_MENU_FOOTNOTES;
        case Action::FILE_BROWSER:
          return LongPress::LONG_MENU_FILE_BROWSER;
        case Action::CREATE_CLIPPING:
          return LongPress::LONG_MENU_CREATE_CLIPPING;
        case Action::LOOKUP_WORD:
          return LongPress::LONG_MENU_LOOKUP_WORD;
        case Action::QUICK_ACTIONS:
          return LongPress::LONG_MENU_QUICK_ACTIONS;
        case Action::QUICK_LOCK:
          return LongPress::LONG_MENU_QUICK_LOCK;
        case Action::PAGE_TURN:
        case Action::PREVIOUS_PAGE:
        case Action::NEARBY_POSITION_SYNC:
        case Action::TOGGLE_HOME_BUTTON_IN_READER:
        case Action::TOGGLE_FRONTLIGHT:
        case Action::TOGGLE_TOUCHSCREEN:
          return SHORTCUT_OPTION_UNAVAILABLE;
        default:
          return SHORTCUT_OPTION_UNAVAILABLE;
      }
      break;
    case ShortcutOptionCatalog::HomeButton:
      switch (action) {
        case Action::SLEEP:
        case Action::TOGGLE_TILT_PAGE_TURN:
        case Action::TOGGLE_HOME_BUTTON_IN_READER:
        case Action::TOGGLE_FRONTLIGHT:
          return SHORTCUT_OPTION_UNAVAILABLE;
        default:
          return static_cast<uint8_t>(action);
      }
  }

  return SHORTCUT_OPTION_UNAVAILABLE;
}

inline void appendShortcutOptions(SettingInfo& setting, const ShortcutOptionCatalog catalog) {
  setting.enumValues.reserve(QuickActions::shortcutActionOrder.size() + 3);
  setting.enumRawValues.reserve(QuickActions::shortcutActionOrder.size() + 3);

  if (catalog == ShortcutOptionCatalog::HomeButton) {
    setting.enumValues.push_back(StrId::STR_BACK_HOME);
    setting.enumRawValues.push_back(CrossPointSettings::HOME_BUTTON_BACK_HOME);
    if (Frontlight.present()) {
      setting.enumValues.push_back(StrId::STR_TOGGLE_FRONTLIGHT);
      setting.enumRawValues.push_back(CrossPointSettings::HOME_BUTTON_TOGGLE_FRONTLIGHT);
    }
    setting.enumValues.push_back(StrId::STR_READER_MENU);
    setting.enumRawValues.push_back(CrossPointSettings::HOME_BUTTON_READER_MENU);
  }

  for (const auto action : QuickActions::shortcutActionOrder) {
    if (!QuickActions::isActionAvailable(static_cast<uint8_t>(action))) continue;
    const uint8_t rawValue = shortcutRawValue(catalog, action);
    if (rawValue == SHORTCUT_OPTION_UNAVAILABLE) continue;
    setting.enumValues.push_back(QuickActions::actionLabel(static_cast<uint8_t>(action)));
    setting.enumRawValues.push_back(rawValue);
  }
}

inline SettingInfo buildShortcutSetting(const StrId nameId, uint8_t CrossPointSettings::* const valuePtr,
                                        const char* const key, const ShortcutOptionCatalog catalog) {
  SettingInfo setting = SettingInfo::Enum(nameId, valuePtr, std::vector<StrId>{}, key, StrId::STR_CAT_CONTROLS);
  appendShortcutOptions(setting, catalog);
  return setting;
}

inline SettingInfo buildHomeButtonActionSetting(const StrId nameId, uint8_t CrossPointSettings::* const valuePtr,
                                                const char* const key) {
  return buildShortcutSetting(nameId, valuePtr, key, ShortcutOptionCatalog::HomeButton);
}

// Shared settings list used by both the device settings UI and the web settings API.
// Each entry has a key (for JSON API) and category (for grouping).
// ACTION-type entries and entries without a key are device-only.
//
// The static list is constructed exactly once (master's optimization, #1086 +
// #1636) so the per-entry SettingInfo cost is paid once. Read-only consumers
// can use it directly; mutable device UI lists use getSettingsList(), which
// returns an owned copy and can add SD-card font and dictionary options.
inline constexpr size_t BASE_SETTINGS_CAPACITY = 104;  // 102 regular entries plus two optional tilt entries.

// Defined in SettingsList.cpp: one copy of this large table builder instead
// of one per file that includes this header.
const std::vector<SettingInfo>& getBaseSettingsList();

inline std::vector<SettingInfo> getSettingsList(const SdCardFontRegistry* registry = nullptr,
                                                const DictionaryRegistry* dictRegistry = nullptr) {
  std::vector<SettingInfo> v = getBaseSettingsList();
  const bool hasTouch = gpio.hasTouch();
  if (!hasTouch) {
    v.erase(std::remove_if(v.begin(), v.end(),
                           [](const SettingInfo& s) {
                             return s.nameId == StrId::STR_TOUCH_READER_CONTROLS ||
                                    s.nameId == StrId::STR_DISABLE_TOUCHSCREEN || s.nameId == StrId::STR_NEXT_PAGE ||
                                    s.nameId == StrId::STR_PREV_PAGE || s.nameId == StrId::STR_TAP_HIDE_STATUS_BAR ||
                                    s.nameId == StrId::STR_PINCH_FONT_RESIZE ||
                                    s.nameId == StrId::STR_TWO_FINGER_ROTATION ||
                                    s.nameId == StrId::STR_TWO_FINGER_SWIPE_UP ||
                                    s.nameId == StrId::STR_TWO_FINGER_SWIPE_DOWN ||
                                    s.nameId == StrId::STR_TWO_FINGER_SWIPE_LEFT ||
                                    s.nameId == StrId::STR_TWO_FINGER_SWIPE_RIGHT;
                           }),
            v.end());
  }
  if (!gpio.supportsMultiTouch()) {
    v.erase(std::remove_if(v.begin(), v.end(),
                           [](const SettingInfo& s) {
                             return s.nameId == StrId::STR_PINCH_FONT_RESIZE ||
                                    s.nameId == StrId::STR_TWO_FINGER_ROTATION ||
                                    s.nameId == StrId::STR_TWO_FINGER_SWIPE_UP ||
                                    s.nameId == StrId::STR_TWO_FINGER_SWIPE_DOWN ||
                                    s.nameId == StrId::STR_TWO_FINGER_SWIPE_LEFT ||
                                    s.nameId == StrId::STR_TWO_FINGER_SWIPE_RIGHT;
                           }),
            v.end());
  }
  for (auto& setting : v) {
    const bool isTwoFingerSwipe =
        setting.nameId == StrId::STR_TWO_FINGER_SWIPE_UP || setting.nameId == StrId::STR_TWO_FINGER_SWIPE_DOWN ||
        setting.nameId == StrId::STR_TWO_FINGER_SWIPE_LEFT || setting.nameId == StrId::STR_TWO_FINGER_SWIPE_RIGHT;
    if (!isTwoFingerSwipe) continue;
    if (!Frontlight.present()) {
      removeEnumRawValue(setting, CrossPointSettings::TWO_FINGER_SWIPE_INCREASE_BRIGHTNESS);
      removeEnumRawValue(setting, CrossPointSettings::TWO_FINGER_SWIPE_DECREASE_BRIGHTNESS);
      removeEnumRawValue(setting, CrossPointSettings::TWO_FINGER_SWIPE_INCREASE_WARMTH);
      removeEnumRawValue(setting, CrossPointSettings::TWO_FINGER_SWIPE_DECREASE_WARMTH);
    } else if (!Frontlight.hasColorTemperature()) {
      removeEnumRawValue(setting, CrossPointSettings::TWO_FINGER_SWIPE_INCREASE_WARMTH);
      removeEnumRawValue(setting, CrossPointSettings::TWO_FINGER_SWIPE_DECREASE_WARMTH);
    }
  }
  if (hasTouch) {
    v.erase(std::remove_if(v.begin(), v.end(),
                           [](const SettingInfo& s) {
                             return s.nameId == StrId::STR_FRONT_BTN_FOLLOW_ORIENTATION ||
                                    s.nameId == StrId::STR_SUNLIGHT_FADING_FIX;
                           }),
            v.end());

    const auto themeIt =
        std::find_if(v.begin(), v.end(), [](const SettingInfo& s) { return s.nameId == StrId::STR_UI_THEME; });
    if (themeIt != v.end()) {
      removeEnumRawValue(*themeIt, static_cast<uint8_t>(CrossPointSettings::UI_THEME::CLASSIC));
      removeEnumRawValue(*themeIt, static_cast<uint8_t>(CrossPointSettings::UI_THEME::ROUNDEDRAFF));
    }
  }
  if (!gpio.hasHomeKey()) {
    v.erase(std::remove_if(v.begin(), v.end(),
                           [](const SettingInfo& s) {
                             return s.nameId == StrId::STR_IN_READER || s.nameId == StrId::STR_HOME_BUTTON_TAP ||
                                    s.nameId == StrId::STR_HOME_BUTTON_DOUBLE_TAP ||
                                    settingKeyIs(s, "homeButtonLongPressAction");
                           }),
            v.end());
    for (auto& setting : v) {
      if (setting.nameId == StrId::STR_SHORT_PWR_BTN || settingKeyIs(setting, "longPwrBtn")) {
        removeEnumRawValue(setting, CrossPointSettings::TOGGLE_HOME_BUTTON_IN_READER);
      } else if (settingKeyIs(setting, "powerChordAction") || settingKeyIs(setting, "sideButtonChordAction")) {
        removeEnumRawValue(setting, shortcutRawValue(ShortcutOptionCatalog::ButtonChord,
                                                     CrossPointSettings::TOGGLE_HOME_BUTTON_IN_READER));
      }
    }
  }
  if (!Frontlight.present() || !gpio.hasTouch()) {
    for (auto& setting : v) {
      if (setting.nameId != StrId::STR_SHORT_PWR_BTN && !settingKeyIs(setting, "longPwrBtn") &&
          !settingKeyIs(setting, "powerChordAction") && !settingKeyIs(setting, "sideButtonChordAction")) {
        continue;
      }
      const auto catalog = settingKeyIs(setting, "powerChordAction") || settingKeyIs(setting, "sideButtonChordAction")
                               ? ShortcutOptionCatalog::ButtonChord
                               : ShortcutOptionCatalog::PowerButton;
      if (!Frontlight.present()) {
        removeEnumRawValue(setting, shortcutRawValue(catalog, CrossPointSettings::TOGGLE_FRONTLIGHT));
      }
      if (!gpio.hasTouch()) {
        removeEnumRawValue(setting, shortcutRawValue(catalog, CrossPointSettings::TOGGLE_TOUCHSCREEN));
      }
    }
  }
  if (!Frontlight.present()) {
    for (auto& setting : v) {
      if (setting.nameId == StrId::STR_REFRESH_FREQ) {
        removeEnumRawValue(setting, CrossPointSettings::REFRESH_NEVER);
      }
    }
  }
  if (registry && registry->getFamilyCount() > 0) {
    auto it = std::find_if(v.begin(), v.end(), [](const SettingInfo& s) { return s.nameId == StrId::STR_FONT_FAMILY; });
    if (it != v.end()) {
      *it = buildFontFamilySetting(registry);
    }
    auto fontSizeIt =
        std::find_if(v.begin(), v.end(), [](const SettingInfo& s) { return s.nameId == StrId::STR_FONT_SIZE; });
    if (fontSizeIt != v.end()) {
      *fontSizeIt = buildFontSizeSetting(registry);
    }
  }
  if (dictRegistry) {
    if (dictRegistry->count() > 0) {
      auto fontSizeIt =
          std::find_if(v.begin(), v.end(), [](const SettingInfo& s) { return s.nameId == StrId::STR_FONT_SIZE; });
      const size_t insertIndex =
          fontSizeIt == v.end() ? v.size() : static_cast<size_t>(std::distance(v.begin(), fontSizeIt) + 1);
      v.insert(v.begin() + insertIndex, buildDictionaryFontFamilySetting(registry));
      v.insert(v.begin() + insertIndex + 1, buildDictionaryFontSizeSetting(registry));
    }
    auto guideIt =
        std::find_if(v.begin(), v.end(), [](const SettingInfo& s) { return s.nameId == StrId::STR_GUIDE_READING; });
    const auto insertPos = guideIt == v.end() ? v.end() : guideIt + 1;
    v.insert(insertPos, buildDictionarySetting(dictRegistry));
  }
  return v;
}

inline void addSettingByName(std::vector<SettingInfo>& target, const std::vector<SettingInfo>& allSettings,
                             StrId nameId) {
  const auto it = std::find_if(allSettings.begin(), allSettings.end(),
                               [nameId](const auto& setting) { return setting.nameId == nameId; });
  if (it != allSettings.end()) {
    target.push_back(*it);
  }
}

inline std::vector<SettingInfo> buildReaderSettingsParentList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> readerSettings;
  readerSettings.reserve(12);
  readerSettings.push_back(SettingInfo::Submenu(StrId::STR_READER_FONT_OPTIONS, SettingAction::ReaderFontOptions));
  readerSettings.push_back(SettingInfo::Submenu(StrId::STR_READER_PAGE_LAYOUT, SettingAction::ReaderPageLayout));
  readerSettings.push_back(SettingInfo::Action(StrId::STR_CUSTOMISE_STATUS_BAR, SettingAction::CustomiseStatusBar));
  addSettingByName(readerSettings, allSettings, StrId::STR_PUBLISHER_PAGE_NUMBERS);
  addSettingByName(readerSettings, allSettings, StrId::STR_DISABLE_TOUCHSCREEN);
  addSettingByName(readerSettings, allSettings, StrId::STR_EMBEDDED_STYLE);
  addSettingByName(readerSettings, allSettings, StrId::STR_IMAGES);
  addSettingByName(readerSettings, allSettings, StrId::STR_FOCUS_READING);
  addSettingByName(readerSettings, allSettings, StrId::STR_GUIDE_READING);
  addSettingByName(readerSettings, allSettings, StrId::STR_DICTIONARY);
  addSettingByName(readerSettings, allSettings, StrId::STR_INDEXING_METHOD);
  return readerSettings;
}

inline std::vector<SettingInfo> buildBookReaderSettingsParentList(const std::vector<SettingInfo>& allSettings) {
  auto settings = buildReaderSettingsParentList(allSettings);
  settings.erase(
      std::remove_if(settings.begin(), settings.end(),
                     [](const SettingInfo& setting) { return setting.nameId == StrId::STR_DISABLE_TOUCHSCREEN; }),
      settings.end());
  // PocketDeck-OS: tilt page turn is also reachable from inside a book. These
  // are the same SETTINGS fields as Settings > Controls, so both stay in sync;
  // they only exist when the device has a motion sensor (X3, X4 Pro, ...).
  addSettingByName(settings, allSettings, StrId::STR_TILT_PAGE_TURN);
  addSettingByName(settings, allSettings, StrId::STR_TILT_PAGE_TURN_DIRECTION);
  return settings;
}

inline std::vector<SettingInfo> buildReaderFontSettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> settings;
  settings.reserve(10);
  addSettingByName(settings, allSettings, StrId::STR_FONT_FAMILY);
  addSettingByName(settings, allSettings, StrId::STR_FONT_SIZE);
  addSettingByName(settings, allSettings, StrId::STR_DICTIONARY_FONT);
  addSettingByName(settings, allSettings, StrId::STR_DICTIONARY_FONT_SIZE);
  addSettingByName(settings, allSettings, StrId::STR_LINE_SPACING);
  addSettingByName(settings, allSettings, StrId::STR_WORD_SPACING);
  addSettingByName(settings, allSettings, StrId::STR_TEXT_AA);
  settings.push_back(SettingInfo::Action(StrId::STR_DOWNLOAD_FONTS, SettingAction::DownloadFonts));
  addSettingByName(settings, allSettings, StrId::STR_SD_FONT_SIZE_RANGE);
  return settings;
}

inline std::vector<SettingInfo> buildReaderPageLayoutSettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> settings;
  settings.reserve(6);
  addSettingByName(settings, allSettings, StrId::STR_ORIENTATION);
  addSettingByName(settings, allSettings, StrId::STR_SCREEN_MARGIN);
  addSettingByName(settings, allSettings, StrId::STR_PARA_ALIGNMENT);
  addSettingByName(settings, allSettings, StrId::STR_HYPHENATION);
  addSettingByName(settings, allSettings, StrId::STR_EXTRA_SPACING);
  addSettingByName(settings, allSettings, StrId::STR_FORCE_PARAGRAPH_INDENTS);
  return settings;
}

inline std::vector<SettingInfo> buildReaderScreenMarginSettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> settings;
  settings.reserve(2);
  addSettingByName(settings, allSettings, StrId::STR_TOP_BOTTOM);
  addSettingByName(settings, allSettings, StrId::STR_LEFT_RIGHT);
  return settings;
}

inline void addSettingByKey(std::vector<SettingInfo>& target, const std::vector<SettingInfo>& allSettings,
                            const char* key) {
  const auto it = std::find_if(allSettings.begin(), allSettings.end(),
                               [key](const auto& setting) { return settingKeyIs(setting, key); });
  if (it != allSettings.end()) {
    target.push_back(*it);
  }
}

inline bool hasSettingByName(const std::vector<SettingInfo>& allSettings, StrId nameId) {
  return std::any_of(allSettings.begin(), allSettings.end(),
                     [nameId](const auto& setting) { return setting.nameId == nameId; });
}

inline bool hasSideButtonChordSetting(const std::vector<SettingInfo>& allSettings) {
  return deviceSupportsSideButtonChord(gpio) && hasSettingByName(allSettings, StrId::STR_SIDE_BUTTON_CHORD);
}

inline std::vector<SettingInfo> buildControlsSettingsParentList(const std::vector<SettingInfo>& allSettings) {
  const bool hasTiltPageTurnSetting = hasSettingByName(allSettings, StrId::STR_TILT_PAGE_TURN);
  const bool hasTiltPageTurnDirectionSetting = hasSettingByName(allSettings, StrId::STR_TILT_PAGE_TURN_DIRECTION);
  const bool hasTapsGestures = hasSettingByName(allSettings, StrId::STR_NEXT_PAGE);
  const bool hasFrontButtons = !gpio.hasTouch();
  const bool hasHomeKey = gpio.hasHomeKey();

  std::vector<SettingInfo> settings;
  settings.reserve(3 + (hasHomeKey ? 1u : 0u) + (hasFrontButtons ? 1u : 0u) + (hasTiltPageTurnSetting ? 1u : 0u) +
                   (hasTiltPageTurnDirectionSetting ? 1u : 0u) + (hasTapsGestures ? 1u : 0u));
  if (hasHomeKey) {
    settings.push_back(SettingInfo::Submenu(StrId::STR_HOME_BUTTON, SettingAction::ControlsHomeButton));
  }
  settings.push_back(SettingInfo::Submenu(StrId::STR_POWER_BUTTON, SettingAction::ControlsPowerButton));
  if (hasFrontButtons) {
    settings.push_back(SettingInfo::Submenu(StrId::STR_FRONT_BUTTONS, SettingAction::ControlsFrontButtons));
  }
  settings.push_back(SettingInfo::Submenu(StrId::STR_SIDE_BUTTONS, SettingAction::ControlsSideButtons));
  settings.push_back(SettingInfo::Action(StrId::STR_QUICK_ACTIONS, SettingAction::QuickActions));
  if (hasTapsGestures) {
    settings.push_back(SettingInfo::Submenu(StrId::STR_TAPS_AND_GESTURES, SettingAction::ControlsTapsGestures));
  }
  if (hasTiltPageTurnSetting) addSettingByName(settings, allSettings, StrId::STR_TILT_PAGE_TURN);
  if (hasTiltPageTurnDirectionSetting) addSettingByName(settings, allSettings, StrId::STR_TILT_PAGE_TURN_DIRECTION);
  return settings;
}

inline std::vector<SettingInfo> buildControlsTapsGesturesSettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> settings;
  const bool hasPinch = hasSettingByName(allSettings, StrId::STR_PINCH_FONT_RESIZE);
  const bool hasRotation = hasSettingByName(allSettings, StrId::STR_TWO_FINGER_ROTATION);
  const bool hasTwoFingerSwipe = hasSettingByName(allSettings, StrId::STR_TWO_FINGER_SWIPE_UP);
  settings.reserve(3 + (hasPinch ? 1u : 0u) + (hasRotation ? 1u : 0u) + (hasTwoFingerSwipe ? 1u : 0u));
  addSettingByName(settings, allSettings, StrId::STR_NEXT_PAGE);
  addSettingByName(settings, allSettings, StrId::STR_PREV_PAGE);
  if (hasPinch) addSettingByName(settings, allSettings, StrId::STR_PINCH_FONT_RESIZE);
  if (hasRotation) addSettingByName(settings, allSettings, StrId::STR_TWO_FINGER_ROTATION);
  addSettingByName(settings, allSettings, StrId::STR_TAP_HIDE_STATUS_BAR);
  if (hasTwoFingerSwipe) {
    settings.push_back(SettingInfo::Submenu(StrId::STR_TWO_FINGER_SWIPE, SettingAction::ControlsTwoFingerSwipe));
  }
  return settings;
}

inline std::vector<SettingInfo> buildControlsTwoFingerSwipeSettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> settings;
  settings.reserve(4);
  addSettingByName(settings, allSettings, StrId::STR_TWO_FINGER_SWIPE_UP);
  addSettingByName(settings, allSettings, StrId::STR_TWO_FINGER_SWIPE_DOWN);
  addSettingByName(settings, allSettings, StrId::STR_TWO_FINGER_SWIPE_LEFT);
  addSettingByName(settings, allSettings, StrId::STR_TWO_FINGER_SWIPE_RIGHT);
  return settings;
}

inline std::vector<SettingInfo> buildControlsHomeButtonSettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> settings;
  settings.reserve(4);
  addSettingByName(settings, allSettings, StrId::STR_IN_READER);
  addSettingByName(settings, allSettings, StrId::STR_HOME_BUTTON_TAP);
  addSettingByName(settings, allSettings, StrId::STR_HOME_BUTTON_DOUBLE_TAP);
  addSettingByKey(settings, allSettings, "homeButtonLongPressAction");
  return settings;
}

inline std::vector<SettingInfo> buildControlsPowerSettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> settings;
  settings.reserve(4);
  addSettingByName(settings, allSettings, StrId::STR_SHORT_PWR_BTN);
  addSettingByKey(settings, allSettings, "longPwrBtn");
  if (SETTINGS.shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::FOOTNOTES ||
      SETTINGS.longPwrBtn == CrossPointSettings::SHORT_PWRBTN::FOOTNOTES ||
      SETTINGS.longPressMenuAction == CrossPointSettings::LONG_PRESS_MENU_ACTION::LONG_MENU_FOOTNOTES ||
      SETTINGS.longPressBackAction == CrossPointSettings::LONG_PRESS_MENU_ACTION::LONG_MENU_FOOTNOTES) {
    addSettingByName(settings, allSettings, StrId::STR_PWR_BTN_FOOTNOTE_BACK);
  }
  addSettingByName(settings, allSettings, StrId::STR_POWER_BUTTON_CHORD);
  return settings;
}

inline std::vector<SettingInfo> buildControlsFrontButtonSettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> settings;
  settings.reserve(6);
  settings.push_back(SettingInfo::Action(StrId::STR_REMAP_FRONT_BUTTONS, SettingAction::RemapFrontButtons));
  settings.push_back(
      SettingInfo::Action(StrId::STR_REMAP_FRONT_BUTTONS_READER, SettingAction::RemapFrontButtonsReader));
  addSettingByKey(settings, allSettings, "frontButtonOrientationAware");
  addSettingByKey(settings, allSettings, "longPressButtonBehavior");
  addSettingByName(settings, allSettings, StrId::STR_LONG_PRESS_BACK_ACTION);
  addSettingByName(settings, allSettings, StrId::STR_LONG_PRESS_MENU_ACTION);
  return settings;
}

inline std::vector<SettingInfo> buildControlsSideButtonSettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> settings;
  const bool hasChord = hasSideButtonChordSetting(allSettings);
  settings.reserve(3 + (hasChord ? 1u : 0u));
  addSettingByName(settings, allSettings, StrId::STR_SIDE_BTN_LAYOUT);
  addSettingByKey(settings, allSettings, "sideButtonOrientationAware");
  addSettingByKey(settings, allSettings, "sideButtonLongPress");
  if (hasChord) {
    addSettingByName(settings, allSettings, StrId::STR_SIDE_BUTTON_CHORD);
  }
  return settings;
}

inline std::vector<SettingInfo> buildGroupedDisplaySettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> displaySettings;
  displaySettings.reserve(10);

  auto addDisplaySetting = [&](StrId nameId) {
    const auto it = std::find_if(allSettings.begin(), allSettings.end(),
                                 [nameId](const auto& setting) { return setting.nameId == nameId; });
    if (it != allSettings.end()) {
      displaySettings.push_back(*it);
    }
  };

  displaySettings.push_back(SettingInfo::Submenu(StrId::STR_DISPLAY_SLEEP_SCREEN, SettingAction::DisplaySleepScreen));
  if (Frontlight.present()) {
    displaySettings.push_back(SettingInfo::Submenu(StrId::STR_FRONTLIGHT, SettingAction::DisplayFrontlight));
  }
  addDisplaySetting(StrId::STR_HIDE_BATTERY);
  if (halClock.isAvailable()) {
    addDisplaySetting(StrId::STR_HIDE_CLOCK);
  }
  addDisplaySetting(StrId::STR_REFRESH_FREQ);
  addDisplaySetting(StrId::STR_NIGHT_MODE);
  addDisplaySetting(StrId::STR_UI_THEME);
  addDisplaySetting(StrId::STR_CAROUSEL_BOOK_STATS);  // used by the Lyra Carousel theme
  addDisplaySetting(StrId::STR_UI_SCALE);
  addDisplaySetting(StrId::STR_RECENT_BOOKS_VIEW);
  addDisplaySetting(StrId::STR_SUNLIGHT_FADING_FIX);

  return displaySettings;
}

inline std::vector<SettingInfo> buildDisplayFrontlightSettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> settings;
  settings.reserve(4);

  auto addDisplaySetting = [&](const StrId nameId) {
    const auto it = std::find_if(allSettings.begin(), allSettings.end(),
                                 [nameId](const auto& setting) { return setting.nameId == nameId; });
    if (it != allSettings.end()) settings.push_back(*it);
  };

  addDisplaySetting(StrId::STR_RESTORE_LIGHT_ON_WAKE);
  if (halClock.isAvailable()) {
    addDisplaySetting(StrId::STR_FRONTLIGHT_SCHEDULE);
    addDisplaySetting(StrId::STR_START);
    addDisplaySetting(StrId::STR_END);
  }
  return settings;
}

inline std::vector<SettingInfo> buildDisplaySleepSettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> sleepSettings;
  sleepSettings.reserve(6);

  auto addSleepSetting = [&](StrId nameId, StrId displayNameId) {
    const auto it = std::find_if(allSettings.begin(), allSettings.end(),
                                 [nameId](const auto& setting) { return setting.nameId == nameId; });
    if (it != allSettings.end()) {
      sleepSettings.push_back(*it);
      sleepSettings.back().nameId = displayNameId;
    }
  };

  addSleepSetting(StrId::STR_SLEEP_SCREEN, StrId::STR_SLEEP_SCREEN_WALLPAPER);
  addSleepSetting(StrId::STR_SLEEP_COVER_MODE, StrId::STR_SLEEP_COVER_MODE_SHORT);
  addSleepSetting(StrId::STR_SLEEP_COVER_FILTER, StrId::STR_SLEEP_COVER_FILTER_SHORT);
  addSleepSetting(StrId::STR_QUICK_RESUME_TIMEOUT, StrId::STR_QUICK_RESUME_TIMEOUT);
  addSleepSetting(StrId::STR_WALLPAPER_ROTATION, StrId::STR_WALLPAPER_ROTATION);
  sleepSettings.push_back(SettingInfo::Action(StrId::STR_WALLPAPER_HELP, SettingAction::WallpaperHelp));

  return sleepSettings;
}

inline std::vector<SettingInfo> buildSystemSettingsParentList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> systemSettings;
  systemSettings.reserve(9);
  systemSettings.push_back(SettingInfo::Submenu(StrId::STR_SYSTEM_DEVICE, SettingAction::SystemDevice));
  systemSettings.push_back(SettingInfo::Submenu(StrId::STR_SYSTEM_FILES_CACHE, SettingAction::SystemFilesCache));
  systemSettings.push_back(SettingInfo::Submenu(StrId::STR_READING_STATS, SettingAction::SystemReadingStats));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_WIFI_NETWORKS, SettingAction::Network));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_KOREADER_SYNC, SettingAction::KOReaderSync));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_OPDS_SERVERS, SettingAction::OPDSBrowser));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_CHECK_UPDATES, SettingAction::CheckForUpdates));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_SD_FIRMWARE_UPDATE, SettingAction::SdFirmwareUpdate));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_ABOUT, SettingAction::About));
  return systemSettings;
}

inline std::vector<SettingInfo> buildSystemDeviceSettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> settings;
  settings.reserve(10);
  addSettingByName(settings, allSettings, StrId::STR_DEVICE_NAME);
  addSettingByName(settings, allSettings, StrId::STR_TIME_TO_SLEEP);
  addSettingByName(settings, allSettings, StrId::STR_CUSTOM_BOOTSCREEN);
  settings.push_back(SettingInfo::Action(StrId::STR_LANGUAGE, SettingAction::Language));
  settings.push_back(SettingInfo::Action(StrId::STR_KEYBOARD_LAYOUTS, SettingAction::KeyboardLayouts));
  if (halClock.isAvailable()) {
    addSettingByName(settings, allSettings, StrId::STR_CLOCK_FORMAT);
    addSettingByName(settings, allSettings, StrId::STR_CLOCK_UTC_OFFSET);
    addSettingByName(settings, allSettings, StrId::STR_DATE_FORMAT);
    addSettingByName(settings, allSettings, StrId::STR_DATE_SEPARATOR);
    settings.push_back(SettingInfo::Action(StrId::STR_CLOCK_SYNC_NOW, SettingAction::ClockSync));
  }
  return settings;
}

inline std::vector<SettingInfo> buildSystemFilesCacheSettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> settings;
  settings.reserve(6);
  addSettingByName(settings, allSettings, StrId::STR_SHOW_HIDDEN_FILES);
  addSettingByName(settings, allSettings, StrId::STR_HIDE_FILE_EXTENSION);
  addSettingByName(settings, allSettings, StrId::STR_FILE_BROWSER_DISPLAY);
  addSettingByName(settings, allSettings, StrId::STR_REMOVE_READ_FROM_RECENTS);
  addSettingByName(settings, allSettings, StrId::STR_MOVE_FINISHED_TO_READ);
  settings.push_back(SettingInfo::Action(StrId::STR_CLEAR_READING_CACHE, SettingAction::ClearCache));
  return settings;
}

inline std::vector<SettingInfo> buildFileBrowserSettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> settings;
  settings.reserve(3);
  addSettingByName(settings, allSettings, StrId::STR_SHOW_HIDDEN_FILES);
  addSettingByName(settings, allSettings, StrId::STR_HIDE_FILE_EXTENSION);
  addSettingByName(settings, allSettings, StrId::STR_FILE_BROWSER_DISPLAY);
  return settings;
}

inline std::vector<SettingInfo> buildSystemReadingStatsSettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> settings;
  settings.reserve(3);
  addSettingByName(settings, allSettings, StrId::STR_TRACK_READING_STATS);
  settings.push_back(SettingInfo::Submenu(StrId::STR_ALL_TIME_STATS, SettingAction::SystemGlobalStats));
  addSettingByName(settings, allSettings, StrId::STR_IDLE_TIME_THRESHOLD);
  return settings;
}

inline std::vector<SettingInfo> buildSystemGlobalStatsSettingsList(const std::vector<SettingInfo>& allSettings) {
  std::vector<SettingInfo> settings;
  settings.reserve(3);
  if (halClock.isAvailable()) {
    addSettingByName(settings, allSettings, StrId::STR_AUTO_BACKUP_STATS);
  }
  settings.push_back(SettingInfo::Action(StrId::STR_BACKUP_NOW, SettingAction::BackupStats));
  settings.push_back(SettingInfo::Action(StrId::STR_RESET_ALL_TIME_STATS, SettingAction::ResetGlobalStats));
  return settings;
}
