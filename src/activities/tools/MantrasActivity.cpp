#include "MantrasActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "PanchangaEnglish.h"
#include "StatsExport.h"
#include "ToolStatsActivity.h"
#include "ToolStatsPages.h"
#include "ToolsDate.h"
#include "ToolsLog.h"
#include "ToolsStore.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/OptionSelectionActivity.h"
#include "fontIds.h"

namespace {
constexpr char kSettingsPath[] = "/tools/mantras/settings.txt";
constexpr int kMalaSizes[] = {108, 54, 27};

// Latin Extended Additional letters (dot below and friends) are not in the UI
// font; show them as their base letter. U+1E00..U+1EFF are 3-byte sequences
// E1 B8..BB xx. Pairs: low byte of the codepoint, replacement letter.
constexpr char kIastBase[][2] = {{0x0C, 'D'}, {0x0D, 'd'}, {0x24, 'H'}, {0x25, 'h'}, {0x36, 'L'}, {0x37, 'l'},
                                 {0x38, 'L'}, {0x39, 'l'}, {0x40, 'M'}, {0x41, 'm'}, {0x42, 'M'}, {0x43, 'm'},
                                 {0x44, 'N'}, {0x45, 'n'}, {0x46, 'N'}, {0x47, 'n'}, {0x5A, 'R'}, {0x5B, 'r'},
                                 {0x5C, 'R'}, {0x5D, 'r'}, {0x62, 'S'}, {0x63, 's'}, {0x6C, 'T'}, {0x6D, 't'}};

char latinBase(const uint32_t cp) {
  if ((cp & 0xFF00) != 0x1E00) return 0;
  for (const auto& p : kIastBase) {
    if (static_cast<uint8_t>(p[0]) == (cp & 0xFF)) return p[1];
  }
  return 0;
}

void plainIast(char* s) {
  char* w = s;
  for (const char* r = s; *r != '\0';) {
    const auto b0 = static_cast<uint8_t>(r[0]);
    if (b0 == 0xE1 && r[1] != '\0' && r[2] != '\0') {
      const uint32_t cp =
          ((b0 & 0x0Fu) << 12) | ((static_cast<uint8_t>(r[1]) & 0x3Fu) << 6) | (static_cast<uint8_t>(r[2]) & 0x3Fu);
      const char c = latinBase(cp);
      if (c != 0) {
        *w++ = c;
        r += 3;
        continue;
      }
    }
    if (b0 == 0xCC && static_cast<uint8_t>(r[1]) == 0xA5) {  // U+0325 combining ring below
      r += 2;
      continue;
    }
    *w++ = *r++;
  }
  *w = '\0';
}

struct JapaSum {
  int total;
};
void addJapa(const tlog::Stamp&, char* fields, void* p) {
  char* cursor = fields;
  const char* count = tlog::nextField(&cursor);
  if (count != nullptr) static_cast<JapaSum*>(p)->total += std::max(0, atoi(count));
}
}  // namespace

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

bool MantrasActivity::writeSettings(FsFile& out, void* ctx) {
  const auto* self = static_cast<const MantrasActivity*>(ctx);
  char days[7 * 25 + 1] = {};
  for (int i = 0; i < 7; ++i) {
    strncat(days, self->dayDeity_[i], sizeof(days) - strlen(days) - 1);
    if (i < 6) strncat(days, ",", sizeof(days) - strlen(days) - 1);
  }
  char body[420];
  snprintf(body, sizeof(body),
           "# PocketDeck-OS Mantras settings\n"
           "# meaning=both (Kannada then English), kn or en\n"
           "meaning=%s\n"
           "# mala=108, 54 or 27\n"
           "mala=%d\n"
           "# ritual = the ritual category played by \"Start daily ritual\"\n"
           "ritual=%s\n"
           "# Deity for each weekday, Sunday first (keys of vedic_mantras_collection)\n"
           "days=%s\n",
           self->meaning_ == Meaning::Kannada   ? "kn"
           : self->meaning_ == Meaning::English ? "en"
                                                : "both",
           self->mala_, self->ritualKey_, days);
  return tools::writeText(out, body);
}

bool MantrasActivity::saveSettings() const {
  return tools::writeFileAtomic(kSettingsPath, &MantrasActivity::writeSettings, const_cast<MantrasActivity*>(this));
}

void MantrasActivity::loadSettings() {
  tools::recoverFromBackup(kSettingsPath);
  if (!Storage.exists(kSettingsPath)) return;
  FsFile f;
  if (!Storage.openFileForRead("MANT", kSettingsPath, f)) return;
  char line[200];
  while (tools::readLine(f, line, sizeof(line)) >= 0) {
    if (line[0] == '#') continue;
    char* eq = strchr(line, '=');
    if (eq == nullptr) continue;
    *eq = '\0';
    const char* v = eq + 1;
    if (strcmp(line, "meaning") == 0) {
      meaning_ = strncmp(v, "kn", 2) == 0   ? Meaning::Kannada
                 : strncmp(v, "en", 2) == 0 ? Meaning::English
                                            : Meaning::Both;
    } else if (strcmp(line, "mala") == 0) {
      const int m = atoi(v);
      if (m == 108 || m == 54 || m == 27) mala_ = m;
    } else if (strcmp(line, "ritual") == 0 && v[0] != '\0') {
      snprintf(ritualKey_, sizeof(ritualKey_), "%s", v);
    } else if (strcmp(line, "days") == 0) {
      char buf[200];
      snprintf(buf, sizeof(buf), "%s", v);
      char* cursor = buf;
      for (int i = 0; i < 7 && cursor != nullptr; ++i) {
        char* comma = strchr(cursor, ',');
        if (comma != nullptr) *comma = '\0';
        if (cursor[0] != '\0') snprintf(dayDeity_[i], sizeof(dayDeity_[i]), "%s", cursor);
        cursor = comma != nullptr ? comma + 1 : nullptr;
      }
    }
  }
  f.close();
}

void MantrasActivity::openSettings() {
  // Rows: meaning language, mala size, daily ritual.
  std::vector<std::string> options;
  options.reserve(3);
  char buf[96];
  options.emplace_back(meaning_ == Meaning::Both      ? tr(STR_TOOLS_MANTRA_MEANING_BOTH)
                       : meaning_ == Meaning::Kannada ? tr(STR_TOOLS_MANTRA_MEANING_KN)
                                                      : tr(STR_TOOLS_MANTRA_MEANING_EN));
  snprintf(buf, sizeof(buf), "%s: %d", tr(STR_TOOLS_MANTRA_MALA_SIZE), mala_);
  options.emplace_back(buf);
  snprintf(buf, sizeof(buf), "%s: %s", tr(STR_TOOLS_MANTRA_DAILY_RITUAL), ritualKey_);
  options.emplace_back(buf);
  auto picker = makeUniqueNoThrow<OptionSelectionActivity>(renderer, mappedInput, "MantraSettings",
                                                           StrId::STR_TOOLS_MANTRA_SETTINGS, std::move(options), 0);
  if (!picker) return;
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    const auto* sel = std::get_if<OptionSelectionResult>(&result.data);
    if (!result.isCancelled && sel != nullptr) {
      switch (sel->index) {
        case 0:
          // Both -> Kannada only -> English only -> Both
          meaning_ = meaning_ == Meaning::Both      ? Meaning::Kannada
                     : meaning_ == Meaning::Kannada ? Meaning::English
                                                    : Meaning::Both;
          saveSettings();
          break;
        case 1: {
          int next = 0;
          for (int i = 0; i < 3; ++i) {
            if (kMalaSizes[i] == mala_) next = (i + 1) % 3;
          }
          mala_ = kMalaSizes[next];
          saveSettings();
          break;
        }
        default:
          chooseRitual();
          return;
      }
    }
    requestUpdate();
  });
}

void MantrasActivity::chooseRitual() {
  std::vector<std::string> options;
  std::array<int8_t, 32> cats{};
  const int n = std::min(lib_.countOf(mantras::Kind::Ritual), static_cast<int>(cats.size()));
  options.reserve(n);
  mantras::Category c;
  int current = 0;
  for (int i = 0; i < n; ++i) {
    const int cat = lib_.findCategory(mantras::Kind::Ritual, i);
    if (cat < 0 || !lib_.category(static_cast<uint16_t>(cat), c)) continue;
    if (strcmp(c.english, ritualKey_) == 0) current = static_cast<int>(options.size());
    cats[options.size()] = static_cast<int8_t>(cat);
    options.emplace_back(c.english);
  }
  if (options.empty()) return;
  auto picker = makeUniqueNoThrow<OptionSelectionActivity>(renderer, mappedInput, "MantraRitual",
                                                           StrId::STR_TOOLS_MANTRA_DAILY_RITUAL, std::move(options),
                                                           static_cast<uint8_t>(current));
  if (!picker) return;
  startActivityForResult(std::move(picker), [this, cats](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    const auto* sel = std::get_if<OptionSelectionResult>(&result.data);
    if (!result.isCancelled && sel != nullptr && sel->index < cats.size() && cats[sel->index] >= 0) {
      mantras::Category c;
      if (lib_.category(static_cast<uint16_t>(cats[sel->index]), c)) {
        RenderLock lock(*this);
        snprintf(ritualKey_, sizeof(ritualKey_), "%s", c.english);
        saveSettings();
        loadRitual();
      }
    }
    requestUpdate();
  });
}

// ---------------------------------------------------------------------------
// Lifecycle and navigation
// ---------------------------------------------------------------------------

void MantrasActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  tools::ensureToolsDirs();
  Storage.ensureDirectoryExists(mantras::kDir);
  kannadaFont_ = kannada::acquire();
  mantra_ = makeUniqueNoThrow<mantras::Mantra>();
  loadSettings();
  screen_ = Screen::Loading;
  depth_ = 0;
  transitionPending_ = true;
  requestUpdate();
}

void MantrasActivity::onExit() {
  saveJapa();
  kannada::release();
  mantra_.reset();
  about_.reset();
  Activity::onExit();
}

void MantrasActivity::push(const Screen s) {
  // If the Kannada font could not be opened earlier (busy card, low memory), try again.
  if (!kannada::ready()) kannadaFont_ = kannada::retry();
  if (depth_ < kMaxStack) stack_[depth_++] = screen_;
  screen_ = s;
  transitionPending_ = true;
}

void MantrasActivity::pop() {
  if (depth_ == 0) {
    finish();
    return;
  }
  screen_ = stack_[--depth_];
  transitionPending_ = true;
  if (screen_ == Screen::Home && todayMantra_ >= 0) loadMantra(static_cast<uint16_t>(todayMantra_));
  if (screen_ == Screen::View) loadMantra(viewIdx_);
}

bool MantrasActivity::loadMantra(const uint16_t index) {
  if (!mantra_) return false;
  if (loadedMantra_ == index) return true;
  if (!lib_.read(index, *mantra_)) {
    loadedMantra_ = -1;
    return false;
  }
  plainIast(mantra_->english);
  plainIast(mantra_->meaningEnglish);
  loadedMantra_ = index;
  return true;
}

void MantrasActivity::computeToday() {
  tools::DateTime local;
  clockValid_ = tools::getLocalNow(local);
  if (clockValid_) {
    today_ = tools::daysOf(local);
    minuteNow_ = local.hour * 60 + local.minute;
  }
  weekday_ = (tools::weekdayMon0(today_) + 1) % 7;
  todayCategory_ = lib_.findDeity(dayDeity_[weekday_]);
  if (todayCategory_ < 0) {
    const int deities = std::max(1, lib_.countOf(mantras::Kind::Deity));
    todayCategory_ = lib_.findCategory(mantras::Kind::Deity, static_cast<int>(today_ % deities));
  }
  todayMantra_ = -1;
  if (todayCategory_ >= 0 && lib_.category(static_cast<uint16_t>(todayCategory_), todayCat_) && todayCat_.count > 0) {
    // A new mantra of the day's deity each week.
    todayMantra_ = todayCat_.first + static_cast<int>((today_ / 7) % todayCat_.count);
    loadMantra(static_cast<uint16_t>(todayMantra_));
  }
}

void MantrasActivity::loadTodayJapa() {
  JapaSum sum{0};
  if (clockValid_) tlog::scan("mantras", today_, today_, &addJapa, &sum);
  todayJapa_ = sum.total;
}

void MantrasActivity::saveJapa() {
  if (japaCount_ <= 0) return;
  tlog::Stamp at;
  if (!tlog::now(at)) {
    japaCount_ = 0;
    return;
  }
  mantras::Category c;
  const int cat = lib_.categoryOf(japaIdx_);
  const bool haveCat = cat >= 0 && lib_.category(static_cast<uint16_t>(cat), c);
  char text[48] = {};
  if (mantra_ && loadedMantra_ == japaIdx_) {
    snprintf(text, sizeof(text), "%s", mantra_->english);
    for (char* p = text; *p; ++p) {
      if (*p == '|' || *p == '\n') *p = ' ';
    }
  }
  char fields[tlog::kLineCap];
  snprintf(fields, sizeof(fields), "%d|%s|%u|%s", japaCount_, haveCat ? c.key : "-", japaIdx_, text);
  if (!tlog::append("mantras", at, fields)) LOG_ERR("MANT", "Could not log japa");
  todayJapa_ += japaCount_;
  japaCount_ = 0;
}

void MantrasActivity::loadRitual() {
  stepCount_ = 0;
  ritualSel_ = 0;
  ritualCat_ = -1;
  const int n = lib_.countOf(mantras::Kind::Ritual);
  for (int i = 0; i < n && ritualCat_ < 0; ++i) {
    const int cat = lib_.findCategory(mantras::Kind::Ritual, i);
    if (cat >= 0 && lib_.category(static_cast<uint16_t>(cat), ritualInfo_) &&
        strcmp(ritualInfo_.english, ritualKey_) == 0) {
      ritualCat_ = cat;
    }
  }
  if (ritualCat_ < 0) {  // unknown name in settings: the first ritual category
    ritualCat_ = lib_.findCategory(mantras::Kind::Ritual, 0);
    if (ritualCat_ < 0 || !lib_.category(static_cast<uint16_t>(ritualCat_), ritualInfo_)) return;
  }
  auto m = makeUniqueNoThrow<mantras::Mantra>();
  if (!m) return;
  for (int i = 0; i < ritualInfo_.count && stepCount_ < kMaxSteps; ++i) {
    if (!lib_.read(static_cast<uint16_t>(ritualInfo_.first + i), *m)) continue;
    plainIast(m->english);
    snprintf(steps_[stepCount_++], sizeof(steps_[0]), "%s", m->use[0] ? m->use : m->english);
  }
}

void MantrasActivity::openGroups(const Group g) {
  group_ = g;
  groupCount_ = 0;
  groupSel_ = 0;
  const int maxRows = static_cast<int>(sizeof(groupRows_) / sizeof(groupRows_[0]));
  if (g == Group::Kavacha) {
    mantras::Category c;
    for (int k = 0; k < lib_.kavachaCount() && groupCount_ + 2 < maxRows; ++k) {
      groupRows_[groupCount_++] = {-1, static_cast<int8_t>(k), true};
      groupRows_[groupCount_++] = {-1, static_cast<int8_t>(k), false};
      const int sections = lib_.countOf(mantras::Kind::KavachaSection);
      for (int s = 0; s < sections && groupCount_ < maxRows; ++s) {
        const int cat = lib_.findCategory(mantras::Kind::KavachaSection, s);
        if (cat >= 0 && lib_.category(static_cast<uint16_t>(cat), c) && c.kavacha == k) {
          groupRows_[groupCount_++] = {static_cast<int16_t>(cat), static_cast<int8_t>(k), false};
        }
      }
    }
  } else {
    const auto kind = g == Group::Deities ? mantras::Kind::Deity : mantras::Kind::Ritual;
    const int n = lib_.countOf(kind);
    for (int i = 0; i < n && groupCount_ < maxRows; ++i) {
      const int cat = lib_.findCategory(kind, i);
      if (cat >= 0) groupRows_[groupCount_++] = {static_cast<int16_t>(cat), -1, false};
    }
  }
  push(Screen::Groups);
}

void MantrasActivity::openList(const int category) {
  if (!lib_.category(static_cast<uint16_t>(category), listInfo_) || listInfo_.count == 0) return;
  listCat_ = category;
  listSel_ = 0;
  previewTop_ = -1;
  loadPreviews();
  push(Screen::List);
}

void MantrasActivity::loadPreviews() {
  const int top = listSel_ / kPreviewRows * kPreviewRows;
  if (top == previewTop_) return;
  previewTop_ = top;
  auto m = makeUniqueNoThrow<mantras::Mantra>();
  for (int i = 0; i < kPreviewRows; ++i) {
    previews_[i].kannada[0] = previews_[i].english[0] = '\0';
    const int idx = top + i;
    if (!m || idx >= listInfo_.count || !lib_.read(static_cast<uint16_t>(listInfo_.first + idx), *m)) continue;
    plainIast(m->english);
    snprintf(previews_[i].kannada, sizeof(previews_[i].kannada), "%s", m->kannada);
    snprintf(previews_[i].english, sizeof(previews_[i].english), "%s", m->english);
  }
}

void MantrasActivity::openView(uint16_t mantra, const int category, const int kavachaAll) {
  viewKavacha_ = kavachaAll;
  viewCat_ = category;
  if (kavachaAll >= 0) {
    // The whole kavacha: its sections are consecutive, so their mantras are too.
    mantras::Category c;
    int first = -1, last = -1;
    const int sections = lib_.countOf(mantras::Kind::KavachaSection);
    for (int s = 0; s < sections; ++s) {
      const int cat = lib_.findCategory(mantras::Kind::KavachaSection, s);
      if (cat < 0 || !lib_.category(static_cast<uint16_t>(cat), c) || c.kavacha != kavachaAll) continue;
      if (first < 0) first = c.first;
      last = c.first + c.count - 1;
    }
    if (first < 0) return;
    viewInfo_ = mantras::Category{};
    viewInfo_.first = static_cast<uint16_t>(first);
    viewInfo_.count = static_cast<uint16_t>(last - first + 1);
    mantras::Kavacha k;
    if (lib_.kavacha(static_cast<uint8_t>(kavachaAll), k)) {
      snprintf(viewInfo_.english, sizeof(viewInfo_.english), "%s", k.english);
      snprintf(viewInfo_.kannada, sizeof(viewInfo_.kannada), "%s", k.kannada);
    }
    mantra = viewInfo_.first;  // read the whole kavacha from its start
  } else if (!lib_.category(static_cast<uint16_t>(category), viewInfo_)) {
    return;
  }
  viewIdx_ = mantra;
  viewPage_ = 0;
  loadMantra(mantra);
  push(Screen::View);
}

void MantrasActivity::stepView(const int dir) {
  const int next = static_cast<int>(viewIdx_) + dir;
  if (next < viewInfo_.first || next >= viewInfo_.first + viewInfo_.count) return;
  RenderLock lock(*this);
  viewIdx_ = static_cast<uint16_t>(next);
  viewPage_ = 0;
  loadMantra(viewIdx_);
  requestUpdate();
}

void MantrasActivity::openJapa(const uint16_t mantra) {
  saveJapa();
  japaIdx_ = mantra;
  japaCount_ = 0;
  malaDone_ = false;
  loadMantra(mantra);
  push(Screen::Japa);
}

void MantrasActivity::openAbout(const int kavacha) {
  about_ = makeUniqueNoThrow<char[]>(3072);
  if (!about_) return;
  if (!lib_.kavachaAbout(static_cast<uint8_t>(kavacha), meaning_ != Meaning::English && kannada::ready(), about_.get(),
                         3072)) {
    lib_.kavachaAbout(static_cast<uint8_t>(kavacha), false, about_.get(), 3072);
  }
  plainIast(about_.get());
  aboutPage_ = 0;
  push(Screen::About);
}

void MantrasActivity::openStats() {
  auto stats = makeUniqueNoThrow<ToolStatsActivity>(renderer, mappedInput, tr(STR_TOOLS_MANTRA_STATS_TITLE),
                                                    &toolstats::buildMantras, nullptr, statsx::Feature::Mantras);
  if (!stats) return;
  startActivityForResult(std::move(stats), [this](const ActivityResult&) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    requestUpdate();
  });
}

void MantrasActivity::confirmReset() {
  auto confirm = makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_TOOLS_MANTRA_RESET_TITLE),
                                                         tr(STR_TOOLS_MANTRA_RESET_BODY));
  if (!confirm) return;
  startActivityForResult(std::move(confirm), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    if (!result.isCancelled) {
      RenderLock lock(*this);
      saveJapa();
      malaDone_ = false;
    }
    requestUpdate();
  });
}

void MantrasActivity::loop() {
  input_.poll(mappedInput);
  if (input_.backLong) {
    saveJapa();
    tools::exitToHome();
    return;
  }
  const auto moveSel = [&](int& sel, const int count) {
    if (count <= 0) return false;
    if (input_.up || input_.pageBack || input_.left) {
      sel = (sel + count - 1) % count;
      return true;
    }
    if (input_.down || input_.pageForward || input_.right) {
      sel = (sel + 1) % count;
      return true;
    }
    return false;
  };

  switch (screen_) {
    case Screen::Loading: {
      // After the first frame ("Preparing..."): open (or index) the library.
      const bool ok = lib_.open();
      RenderLock lock(*this);
      if (!ok) {
        screen_ = Screen::Missing;
      } else {
        computeToday();
        loadTodayJapa();
        loadRitual();
        screen_ = Screen::Home;
      }
      transitionPending_ = true;
      requestUpdate();
      return;
    }
    case Screen::Missing:
      if (input_.back) finish();
      return;
    case Screen::Home:
      if (input_.back) {
        finish();
        return;
      }
      if (input_.leftUp) {
        openSettings();
        return;
      }
      if (input_.right && todayMantra_ >= 0) {
        RenderLock lock(*this);
        openJapa(static_cast<uint16_t>(todayMantra_));
        requestUpdate();
        return;
      }
      if (input_.up || input_.pageBack) homeSel_ = (homeSel_ + kHomeRows - 1) % kHomeRows;
      if (input_.down || input_.pageForward) homeSel_ = (homeSel_ + 1) % kHomeRows;
      if (input_.confirm) {
        RenderLock lock(*this);
        switch (homeSel_) {
          case 0:
            if (todayMantra_ >= 0) openView(static_cast<uint16_t>(todayMantra_), todayCategory_, -1);
            break;
          case 1:
            ritualSel_ = 0;
            push(Screen::Ritual);
            break;
          case 2:
            openGroups(Group::Deities);
            break;
          case 3:
            openGroups(Group::Rituals);
            break;
          case 4:
            openGroups(Group::Kavacha);
            break;
          case 5:
            if (todayMantra_ >= 0) openJapa(static_cast<uint16_t>(todayMantra_));
            break;
          default:
            lock.unlock();
            openStats();
            return;
        }
      }
      if (input_.any) requestUpdate();
      return;
    case Screen::Ritual:
      if (input_.back) {
        pop();
      } else if (input_.leftUp) {
        chooseRitual();
        return;
      } else if (input_.confirm && stepCount_ > 0) {
        RenderLock lock(*this);
        openView(static_cast<uint16_t>(ritualInfo_.first + ritualSel_), ritualCat_, -1);
      } else if (input_.up || input_.pageBack) {
        ritualSel_ = (ritualSel_ + stepCount_ - 1) % std::max(1, stepCount_);
      } else if (input_.down || input_.pageForward || input_.right) {
        ritualSel_ = (ritualSel_ + 1) % std::max(1, stepCount_);
      }
      if (input_.any) requestUpdate();
      return;
    case Screen::Groups:
      if (input_.back) {
        pop();
      } else if (input_.confirm && groupCount_ > 0) {
        RenderLock lock(*this);
        const GroupRow& row = groupRows_[groupSel_];
        if (row.category >= 0) {
          openList(row.category);
        } else if (row.about) {
          openAbout(row.kavacha);
        } else {
          openView(0, -1, row.kavacha);
        }
      } else {
        moveSel(groupSel_, groupCount_);
      }
      if (input_.any) requestUpdate();
      return;
    case Screen::List:
      if (input_.back) {
        pop();
      } else if (input_.confirm) {
        RenderLock lock(*this);
        openView(static_cast<uint16_t>(listInfo_.first + listSel_), listCat_, -1);
      } else if (moveSel(listSel_, listInfo_.count)) {
        RenderLock lock(*this);
        loadPreviews();
      }
      if (input_.any) requestUpdate();
      return;
    case Screen::View:
      if (input_.back) {
        RenderLock lock(*this);
        pop();
      } else if (input_.confirm) {
        RenderLock lock(*this);
        openJapa(viewIdx_);
      } else if (input_.left) {
        stepView(-1);
        return;
      } else if (input_.right) {
        stepView(1);
        return;
      } else if ((input_.up || input_.pageBack) && viewPage_ > 0) {
        --viewPage_;
      } else if ((input_.down || input_.pageForward) && viewPage_ + 1 < viewPages_) {
        ++viewPage_;
      }
      if (input_.any) requestUpdate();
      return;
    case Screen::Japa:
      if (input_.back) {
        RenderLock lock(*this);
        saveJapa();
        pop();
      } else if (input_.right) {
        confirmReset();
        return;
      } else if (input_.left) {
        if (japaCount_ > 0) --japaCount_;
        malaDone_ = false;
      } else if (input_.confirm || input_.up || input_.down || input_.pageBack || input_.pageForward) {
        ++japaCount_;
        malaDone_ = japaCount_ % mala_ == 0;
        if (malaDone_) {
          // Each full mala is logged straight away, so a power cut loses less than one mala.
          RenderLock lock(*this);
          saveJapa();
          transitionPending_ = true;
        }
      }
      if (input_.any) requestUpdate();
      return;
    case Screen::About:
      if (input_.back) {
        RenderLock lock(*this);
        pop();
        about_.reset();
      } else if ((input_.up || input_.pageBack || input_.left) && aboutPage_ > 0) {
        --aboutPage_;
      } else if ((input_.down || input_.pageForward || input_.right) && aboutPage_ + 1 < aboutPages_) {
        ++aboutPage_;
      }
      if (input_.any) requestUpdate();
      return;
  }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

bool MantrasActivity::useKannada(const char* text) const {
  return kannada::ready() && text != nullptr && kannada::hasKannada(text);
}

// One line of mixed text, cut at a word boundary to fit maxW.
int MantrasActivity::drawText(const char* text, const kannada::Style s, const int x, const int y, const int maxW,
                              const bool black) const {
  if (text == nullptr || text[0] == '\0') return 0;
  kannada::Line line[1];
  if (kannada::wrap(renderer, text, s, maxW, line, 1) == 0) return 0;
  return kannada::draw(renderer, x, y, text + line[0].start, s, black, line[0].length);
}

void MantrasActivity::render(RenderLock&&) {
  switch (screen_) {
    case Screen::Loading:
      renderMessage(tr(STR_TOOLS_MANTRAS), tr(STR_TOOLS_MANTRA_LOADING));
      break;
    case Screen::Missing:
      renderMessage(tr(STR_TOOLS_MANTRAS), tr(STR_TOOLS_MANTRA_MISSING));
      break;
    case Screen::Home:
      renderHome();
      break;
    case Screen::Ritual:
      renderRitual();
      break;
    case Screen::Groups:
      renderGroups();
      break;
    case Screen::List:
      renderList();
      break;
    case Screen::View:
      renderView();
      break;
    case Screen::Japa:
      renderJapa();
      break;
    case Screen::About:
      renderAbout();
      break;
  }
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}

void MantrasActivity::renderMessage(const char* title, const char* body) {
  const Rect content = tools::drawFrame(renderer, title);
  tools::WrapOptions opt;
  opt.fontId = UI_12_FONT_ID;
  opt.centered = true;
  const Rect box{content.x, content.y + content.height / 3, content.width, content.height / 2};
  tools::drawWrappedText(renderer, box, body, opt);
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
}

void MantrasActivity::renderHome() {
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_MANTRAS));
  const int x0 = content.x;
  const int w = content.width;
  int y = content.y;
  char buf[160];

  // Greeting and time.
  const int h = minuteNow_ / 60;
  const char* greet = h >= 4 && h < 12    ? tr(STR_TOOLS_MANTRA_GOOD_MORNING)
                      : h >= 12 && h < 17 ? tr(STR_TOOLS_MANTRA_GOOD_AFTERNOON)
                      : h >= 17 && h < 21 ? tr(STR_TOOLS_MANTRA_GOOD_EVENING)
                                          : tr(STR_TOOLS_MANTRA_GOOD_NIGHT);
  if (clockValid_) {
    snprintf(buf, sizeof(buf), "%s  %02d:%02d", greet, h, minuteNow_ % 60);
  } else {
    snprintf(buf, sizeof(buf), "%s", greet);
  }
  renderer.drawText(UI_12_FONT_ID, x0, y, buf, true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 2;
  // "Wednesday - Vishnu" (the deity in Kannada when the font is there).
  {
    // One mixed line, so the Kannada name and the Latin text share a baseline.
    snprintf(buf, sizeof(buf), "%s  -  %s %s", en::kWeekday[weekday_],
             useKannada(todayCat_.kannada) ? todayCat_.kannada : "", todayCat_.english);
    kannada::draw(renderer, x0, y, buf, kannada::Style::Label);
    y += 30;
  }

  // Today's mantra card (row 0).
  const bool cardOn = homeSel_ == 0;
  const int cardH = 132;
  if (cardOn) {
    renderer.fillRoundedRect(x0, y, w, cardH, 12, Color::Black);
  } else {
    renderer.drawRoundedRect(x0, y, w, cardH, 2, 12, true);
  }
  renderer.drawText(SMALL_FONT_ID, x0 + 14, y + 8, tr(STR_TOOLS_MANTRA_TODAY), !cardOn, EpdFontFamily::BOLD);
  if (mantra_ && loadedMantra_ == todayMantra_) {
    int ty = y + 28;
    if (useKannada(mantra_->kannada)) {
      kannada::Line lines[2];
      const size_t n = kannada::wrap(renderer, mantra_->kannada, kannada::Style::Value, w - 28, lines, 2);
      for (size_t i = 0; i < n && i < 2; ++i) {
        kannada::draw(renderer, x0 + 14, ty, mantra_->kannada + lines[i].start, kannada::Style::Value, !cardOn,
                      lines[i].length);
        ty += 30;
      }
    }
    const std::string en = renderer.truncatedText(UI_10_FONT_ID, mantra_->english, w - 28, EpdFontFamily::REGULAR);
    renderer.drawText(UI_10_FONT_ID, x0 + 14, std::max(ty + 2, y + cardH - 30), en.c_str(), !cardOn);
  }
  y += cardH + 10;

  // Rows.
  const int rowH = 50;
  struct Row {
    const char* label;
    char info[24];
  } rows[kHomeRows - 1] = {};
  rows[0].label = tr(STR_TOOLS_MANTRA_RITUAL);
  snprintf(rows[0].info, sizeof(rows[0].info), "%d %s", stepCount_, tr(STR_TOOLS_MANTRA_STEPS));
  rows[1].label = tr(STR_TOOLS_MANTRA_DEITIES);
  snprintf(rows[1].info, sizeof(rows[1].info), "%d", lib_.countOf(mantras::Kind::Deity));
  rows[2].label = tr(STR_TOOLS_MANTRA_RITUALS);
  snprintf(rows[2].info, sizeof(rows[2].info), "%d", lib_.countOf(mantras::Kind::Ritual));
  rows[3].label = tr(STR_TOOLS_MANTRA_KAVACHA);
  snprintf(rows[3].info, sizeof(rows[3].info), "%d", lib_.kavachaCount());
  rows[4].label = tr(STR_TOOLS_MANTRA_JAPA);
  snprintf(rows[4].info, sizeof(rows[4].info), "%s %d", tr(STR_TOOLS_MANTRA_TOTAL_TODAY), todayJapa_);
  rows[5].label = tr(STR_TOOLS_MANTRA_STATS);
  for (int i = 0; i < kHomeRows - 1; ++i) {
    const int ry = y + i * rowH;
    const bool on = homeSel_ == i + 1;
    if (on) renderer.fillRoundedRect(x0 - 6, ry, w + 12, rowH - 6, 10, Color::Black);
    renderer.drawText(UI_12_FONT_ID, x0 + 6, ry + 9, rows[i].label, !on, EpdFontFamily::BOLD);
    if (rows[i].info[0] != '\0') {
      const int iw = renderer.getTextWidth(UI_10_FONT_ID, rows[i].info);
      renderer.drawText(UI_10_FONT_ID, x0 + w - iw - 6, ry + 12, rows[i].info, !on);
    }
    if (!on && i + 1 < kHomeRows - 1) renderer.drawLine(x0, ry + rowH - 3, x0 + w, ry + rowH - 3);
  }
  if (!kannada::ready()) {
    renderer.drawCenteredText(SMALL_FONT_ID, content.y + content.height - renderer.getLineHeight(SMALL_FONT_ID),
                              tr(STR_TOOLS_MANTRA_NO_FONT));
  }
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_OPEN), tr(STR_TOOLS_MANTRA_SETTINGS),
                   tr(STR_TOOLS_MANTRA_JAPA_SHORT));
}

void MantrasActivity::renderRitual() {
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_MANTRA_DAILY_RITUAL), ritualInfo_.english);
  const int x0 = content.x;
  const int w = content.width;
  int y = content.y;
  renderer.drawText(UI_10_FONT_ID, x0, y, tr(STR_TOOLS_MANTRA_SEQUENCE));
  y += renderer.getLineHeight(UI_10_FONT_ID) + 8;
  const int rowH = 46;
  const int fit = std::max(1, (content.y + content.height - y) / rowH);
  const int top = std::max(0, std::min(ritualSel_ - fit / 2, stepCount_ - fit));
  char buf[96];
  for (int i = top; i < stepCount_ && i < top + fit; ++i) {
    const int ry = y + (i - top) * rowH;
    const bool on = i == ritualSel_;
    if (on) renderer.fillRoundedRect(x0 - 6, ry, w + 12, rowH - 6, 10, Color::Black);
    snprintf(buf, sizeof(buf), "%d.", i + 1);
    renderer.drawText(UI_12_FONT_ID, x0 + 4, ry + 8, buf, !on, EpdFontFamily::BOLD);
    const std::string s = renderer.truncatedText(UI_12_FONT_ID, steps_[i], w - 52, EpdFontFamily::REGULAR);
    renderer.drawText(UI_12_FONT_ID, x0 + 44, ry + 8, s.c_str(), !on);
  }
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_MANTRA_START), tr(STR_TOOLS_MANTRA_CHANGE),
                   tr(STR_TOOLS_NEXT));
}

void MantrasActivity::renderGroups() {
  const char* title = group_ == Group::Deities   ? tr(STR_TOOLS_MANTRA_DEITIES)
                      : group_ == Group::Rituals ? tr(STR_TOOLS_MANTRA_RITUALS)
                                                 : tr(STR_TOOLS_MANTRA_KAVACHA);
  char sub[16];
  snprintf(sub, sizeof(sub), "%d / %d", groupCount_ ? groupSel_ + 1 : 0, groupCount_);
  const Rect content = tools::drawFrame(renderer, title, sub);
  const int x0 = content.x;
  const int w = content.width;
  const int rowH = 52;
  const int fit = std::max(1, content.height / rowH);
  const int top = groupSel_ / fit * fit;
  mantras::Category c;
  mantras::Kavacha k;
  char buf[96];
  for (int i = top; i < groupCount_ && i < top + fit; ++i) {
    const int ry = content.y + (i - top) * rowH;
    const bool on = i == groupSel_;
    if (on) renderer.fillRoundedRect(x0 - 6, ry, w + 12, rowH - 6, 10, Color::Black);
    const GroupRow& row = groupRows_[i];
    int x = x0 + 6;
    const char* kn = "";
    const char* en = "";
    int count = -1;
    if (row.category >= 0 && lib_.category(static_cast<uint16_t>(row.category), c)) {
      kn = c.kannada;
      en = c.english[0] ? c.english : c.key;
      count = c.count;
    } else if (row.category < 0 && lib_.kavacha(static_cast<uint8_t>(row.kavacha), k)) {
      snprintf(buf, sizeof(buf), "%s: %s", row.about ? tr(STR_TOOLS_MANTRA_ABOUT) : tr(STR_TOOLS_MANTRA_READ_ALL),
               k.english);
      en = buf;
    }
    char countText[12] = {};
    if (count >= 0) snprintf(countText, sizeof(countText), "%d", count);
    const int cw = countText[0] ? renderer.getTextWidth(UI_10_FONT_ID, countText) + 12 : 0;
    if (useKannada(kn)) {
      // Same baseline as the English name drawn at ry + 10.
      const int top =
          ry + 10 + renderer.getFontAscenderSize(UI_12_FONT_ID) - kannada::ascent(renderer, kannada::Style::Label);
      x += drawText(kn, kannada::Style::Label, x, top, (w - cw) / 2, !on) + 10;
    }
    const std::string shown = renderer.truncatedText(UI_12_FONT_ID, en, x0 + w - cw - x, EpdFontFamily::REGULAR);
    renderer.drawText(UI_12_FONT_ID, x, ry + 10, shown.c_str(), !on,
                      row.category < 0 ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    if (countText[0]) renderer.drawText(UI_10_FONT_ID, x0 + w - cw + 6, ry + 13, countText, !on);
  }
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_OPEN), tr(STR_TOOLS_PREVIOUS), tr(STR_TOOLS_NEXT));
}

void MantrasActivity::renderList() {
  char sub[16];
  snprintf(sub, sizeof(sub), "%d / %d", listSel_ + 1, listInfo_.count);
  const Rect content = tools::drawFrame(renderer, listInfo_.english[0] ? listInfo_.english : listInfo_.key, sub);
  const int x0 = content.x;
  const int w = content.width;
  const int rowH = std::max(60, content.height / kPreviewRows);
  const int top = listSel_ / kPreviewRows * kPreviewRows;
  for (int i = 0; i < kPreviewRows && top + i < listInfo_.count; ++i) {
    const int ry = content.y + i * rowH;
    if (ry + rowH > content.y + content.height + 4) break;
    const bool on = top + i == listSel_;
    if (on) renderer.fillRoundedRect(x0 - 6, ry, w + 12, rowH - 6, 10, Color::Black);
    const Preview& p = previews_[i];
    int ty = ry + 2;
    if (useKannada(p.kannada)) {
      drawText(p.kannada, kannada::Style::Label, x0 + 6, ty, w - 12, !on);
      ty += 28;
    }
    const std::string en = renderer.truncatedText(UI_10_FONT_ID, p.english, w - 12, EpdFontFamily::REGULAR);
    renderer.drawText(UI_10_FONT_ID, x0 + 6, ty + 2, en.c_str(), !on);
  }
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_OPEN), tr(STR_TOOLS_PREVIOUS), tr(STR_TOOLS_NEXT));
}

void MantrasActivity::renderView() {
  char sub[24];
  snprintf(sub, sizeof(sub), "%d / %d", viewIdx_ - viewInfo_.first + 1, viewInfo_.count);
  const Rect content = tools::drawFrame(renderer, viewInfo_.english[0] ? viewInfo_.english : viewInfo_.key, sub);
  const int x0 = content.x;
  const int w = content.width;
  if (!mantra_ || loadedMantra_ != viewIdx_) {
    tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
    return;
  }
  const mantras::Mantra& m = *mantra_;
  // Meanings: Kannada, English or both (Settings); an empty one is skipped.
  const bool showKn = meaning_ != Meaning::English && useKannada(m.meaningKannada);
  const bool showEn = meaning_ != Meaning::Kannada || !showKn;
  const bool both = showKn && showEn && m.meaningEnglish[0] != '\0';
  // Blocks: Kannada text, transliteration, meanings, use.
  struct Block {
    const char* text;
    kannada::Style style;
    int gapBefore;
    const char* heading;
  } blocks[5] = {
      {useKannada(m.kannada) ? m.kannada : "", kannada::Style::Body, 0, nullptr},
      {m.english, kannada::Style::Body, 14, nullptr},
      {showKn ? m.meaningKannada : "", kannada::Style::Label, 18,
       both ? tr(STR_TOOLS_MANTRA_MEANING_KN_HEAD) : tr(STR_TOOLS_MANTRA_MEANING)},
      {showEn ? m.meaningEnglish : "", kannada::Style::Label, 14,
       both ? tr(STR_TOOLS_MANTRA_MEANING_EN_HEAD) : tr(STR_TOOLS_MANTRA_MEANING)},
      {m.use, kannada::Style::Label, 14, tr(STR_TOOLS_MANTRA_USE)},
  };
  const int bottom = content.y + content.height - renderer.getLineHeight(SMALL_FONT_ID) - 4;
  int page = 0;
  int y = content.y;
  static constexpr size_t kMaxLines = 48;
  kannada::Line lines[kMaxLines];
  for (const Block& b : blocks) {
    if (b.text == nullptr || b.text[0] == '\0') continue;
    const int lh = kannada::lineHeight(renderer, b.style) + (b.style == kannada::Style::Body ? 2 : 0);
    const int headH = b.heading ? renderer.getLineHeight(SMALL_FONT_ID) + 2 : 0;
    if (y > content.y) y += b.gapBefore;
    if (y + headH + lh > bottom) {
      ++page;
      y = content.y;
    }
    if (b.heading) {
      if (page == viewPage_) renderer.drawText(SMALL_FONT_ID, x0, y, b.heading, true, EpdFontFamily::BOLD);
      y += headH;
    }
    const size_t n = std::min(kMaxLines, kannada::wrap(renderer, b.text, b.style, w, lines, kMaxLines));
    for (size_t i = 0; i < n; ++i) {
      if (y + lh > bottom) {
        ++page;
        y = content.y;
      }
      if (page == viewPage_) {
        kannada::draw(renderer, x0, y, b.text + lines[i].start, b.style, true, lines[i].length);
      }
      y += lh;
    }
  }
  viewPages_ = page + 1;
  if (viewPages_ > 1) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%s %d / %d", tr(STR_TOOLS_MANTRA_PAGE), viewPage_ + 1, viewPages_);
    renderer.drawCenteredText(SMALL_FONT_ID, content.y + content.height - renderer.getLineHeight(SMALL_FONT_ID), buf);
  }
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_MANTRA_JAPA_SHORT), tr(STR_TOOLS_PREVIOUS),
                   tr(STR_TOOLS_NEXT));
}

void MantrasActivity::renderJapa() {
  mantras::Category c;
  const int cat = lib_.categoryOf(japaIdx_);
  const bool haveCat = cat >= 0 && lib_.category(static_cast<uint16_t>(cat), c);
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_MANTRA_JAPA_SHORT), haveCat ? c.english : nullptr);
  const int x0 = content.x;
  const int w = content.width;
  int y = content.y;
  if (mantra_ && loadedMantra_ == japaIdx_) {
    if (useKannada(mantra_->kannada)) {
      kannada::Line lines[3];
      const size_t n = kannada::wrap(renderer, mantra_->kannada, kannada::Style::Value, w, lines, 3);
      for (size_t i = 0; i < n && i < 3; ++i) {
        const int lw =
            kannada::width(renderer, mantra_->kannada + lines[i].start, kannada::Style::Value, lines[i].length);
        kannada::draw(renderer, x0 + (w - lw) / 2, y, mantra_->kannada + lines[i].start, kannada::Style::Value, true,
                      lines[i].length);
        y += 30;
      }
    }
    const std::string en = renderer.truncatedText(UI_10_FONT_ID, mantra_->english, w, EpdFontFamily::REGULAR);
    renderer.drawCenteredText(UI_10_FONT_ID, y + 2, en.c_str());
    y += renderer.getLineHeight(UI_10_FONT_ID) + 12;
  }
  // Mala ring with the count inside.
  const int bottom = content.y + content.height - 2 * renderer.getLineHeight(UI_12_FONT_ID) - 16;
  const int radius = std::max(60, std::min(w / 2 - 10, (bottom - y) / 2));
  const int cx = renderer.getScreenWidth() / 2;
  const int cy = y + radius;
  const int inMala = japaCount_ % mala_;
  const float fraction = malaDone_ && japaCount_ == 0 ? 1.0f : static_cast<float>(inMala) / static_cast<float>(mala_);
  tools::drawProgressRing(renderer, cx, cy, radius, std::max(10, radius / 8), fraction);
  char digits[12];
  snprintf(digits, sizeof(digits), "%d", malaDone_ && japaCount_ == 0 ? mala_ : inMala);
  const int font = renderer.getTextWidth(TOOLS_DIGITS_44_FONT_ID, digits) < radius ? TOOLS_DIGITS_44_FONT_ID
                                                                                   : TOOLS_DIGITS_30_FONT_ID;
  const int asc = renderer.getFontAscenderSize(font);
  renderer.drawCenteredText(font, cy - asc * 3 / 4 + 4, digits);
  char buf[64];
  snprintf(buf, sizeof(buf), "%s %d", tr(STR_TOOLS_MANTRA_OF), mala_);
  renderer.drawCenteredText(UI_10_FONT_ID, cy + asc / 3 + 8, buf);
  y = cy + radius + 12;
  if (malaDone_) {
    renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_TOOLS_MANTRA_MALA_DONE), true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_12_FONT_ID) + 2;
  }
  // What is already completed and saved today (earlier sessions and each full
  // mala of this one); it does not move with every count, the ring does.
  snprintf(buf, sizeof(buf), "%s %d   %s %d", tr(STR_TOOLS_MANTRA_MALAS_TODAY), todayJapa_ / mala_,
           tr(STR_TOOLS_MANTRA_TOTAL_TODAY), todayJapa_);
  renderer.drawCenteredText(UI_12_FONT_ID, y, buf);
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_MANTRA_COUNT), tr(STR_TOOLS_MANTRA_UNDO),
                   tr(STR_TOOLS_MANTRA_RESET));
}

void MantrasActivity::renderAbout() {
  mantras::Kavacha k;
  const int kav = groupSel_ < groupCount_ ? groupRows_[groupSel_].kavacha : 0;
  lib_.kavacha(static_cast<uint8_t>(std::max(0, kav)), k);
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_MANTRA_ABOUT), k.english);
  if (!about_) return;
  const auto style = kannada::Style::Label;
  const int lh = kannada::lineHeight(renderer, style) + 2;
  const int perPage = std::max(1, (content.height - renderer.getLineHeight(SMALL_FONT_ID) - 4) / lh);
  static constexpr size_t kMaxLines = 160;
  auto lines = makeUniqueNoThrow<kannada::Line[]>(kMaxLines);
  if (!lines) return;
  const size_t n =
      std::min(kMaxLines, kannada::wrap(renderer, about_.get(), style, content.width, lines.get(), kMaxLines));
  aboutPages_ = std::max<int>(1, static_cast<int>((n + perPage - 1) / perPage));
  const size_t first = static_cast<size_t>(aboutPage_) * perPage;
  for (size_t i = first; i < n && i < first + perPage; ++i) {
    kannada::draw(renderer, content.x, content.y + static_cast<int>(i - first) * lh, about_.get() + lines[i].start,
                  style, true, lines[i].length);
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "%s %d / %d", tr(STR_TOOLS_MANTRA_PAGE), aboutPage_ + 1, aboutPages_);
  renderer.drawCenteredText(SMALL_FONT_ID, content.y + content.height - renderer.getLineHeight(SMALL_FONT_ID), buf);
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", tr(STR_TOOLS_PREVIOUS), tr(STR_TOOLS_NEXT));
}
