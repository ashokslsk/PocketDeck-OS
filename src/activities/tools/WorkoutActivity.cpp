#include "WorkoutActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "ToolStatsPages.h"
#include "ToolsLog.h"
#include "components/UITheme.h"
#include "fontIds.h"

using workout::Shape;
using workout::Type;

namespace {
constexpr char kPaceFile[] = "/tools/workout/pace.txt";
constexpr int kListRowH = 70;
constexpr int kFigure = 160;  // figure sprites are 160 x 160

void formatMmSs(char* buf, const size_t len, const uint32_t seconds) {
  snprintf(buf, len, "%02u:%02u", static_cast<unsigned>(seconds / 60), static_cast<unsigned>(seconds % 60));
}

// Appends printf output to buf (size cap), keeping it terminated.
void append(char* buf, const size_t cap, const char* fmt, const char* a = "", const char* b = "") {
  const size_t n = strlen(buf);
  if (n + 1 < cap) snprintf(buf + n, cap - n, fmt, a, b);
}
}  // namespace

namespace workout {
// Declared in WorkoutData.h; lives here so the data parser has no renderer dependency.
bool drawFigure(const GfxRenderer& r, const char* relPath, const int x, const int y, const int scale,
                const bool black) {
  if (relPath == nullptr || relPath[0] == '\0') return false;
  char path[112];
  snprintf(path, sizeof(path), "%s/%s", kDir, relPath);
  FsFile f;
  if (!Storage.exists(path) || !Storage.openFileForRead("WKT", path, f)) return false;
  // Binary PBM: "P4" <whitespace> width <whitespace> height <one whitespace> rows.
  int fields[2] = {0, 0};
  int field = 0;
  bool inNumber = false;
  bool ok = f.read() == 'P' && f.read() == '4';
  while (ok && field < 2) {
    const int c = f.read();
    if (c < 0) {
      ok = false;
    } else if (c >= '0' && c <= '9') {
      fields[field] = fields[field] * 10 + (c - '0');
      inNumber = true;
    } else if (inNumber) {
      ++field;  // the single whitespace after height ends the header
      inNumber = false;
    }
  }
  const int w = fields[0];
  const int h = fields[1];
  uint8_t row[64];
  const int stride = (w + 7) / 8;
  ok = ok && w > 0 && h > 0 && stride <= static_cast<int>(sizeof(row));
  for (int yy = 0; ok && yy < h; ++yy) {
    if (f.read(row, stride) != stride) break;
    // Draw runs of ink as rectangles.
    int run = -1;
    for (int xx = 0; xx <= w; ++xx) {
      const bool ink = xx < w && (row[xx >> 3] & (0x80 >> (xx & 7)));
      if (ink && run < 0) run = xx;
      if (!ink && run >= 0) {
        r.fillRect(x + run * scale, y + yy * scale, (xx - run) * scale, scale, black);
        run = -1;
      }
    }
  }
  f.close();
  return ok;
}

}  // namespace workout

// ---------------------------------------------------------------------------
// Lifecycle and navigation
// ---------------------------------------------------------------------------

void WorkoutActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  tools::ensureToolsDirs();
  kannada::acquire();
  cat_ = makeUniqueNoThrow<workout::Category>();
  item_ = makeUniqueNoThrow<workout::Item>();
  text_ = makeUniqueNoThrow<char[]>(kTextCap);
  catCount_ = (cat_ && item_ && text_) ? workout::listCategories(cats_, workout::kMaxCategories) : 0;
  {
    HalFile pace;
    char c[2] = {0, 0};
    if (Storage.exists(kPaceFile) && Storage.openFileForRead("WKT", kPaceFile, pace)) {
      if (pace.read(c, 2) < 1) c[0] = 0;
      pace.close();
    }
    slowPace_ = c[0] == '1';
    if (c[1] >= '0' && c[1] < '0' + kLevelCount) level_ = c[1] - '0';
  }
  if (!cat_ || !item_ || !text_) LOG_ERR("WKT", "Out of memory");
  show(catCount_ > 0 ? Screen::Categories : Screen::Missing);
  requestUpdate();
}

void WorkoutActivity::onExit() {
  if (screen_ == Screen::Session) finishSession(false);
  kannada::release();
  text_.reset();
  verse_.reset();
  item_.reset();
  cat_.reset();
  Activity::onExit();
}

void WorkoutActivity::show(const Screen s) {
  if (!kannada::ready()) kannada::retry();
  screen_ = s;
  transitionPending_ = true;
  requestUpdate();
}

void WorkoutActivity::openCategory(const int index) {
  if (!workout::loadCategory(cats_[index], *cat_)) {
    LOG_ERR("WKT", "Category %s has no practices", cats_[index].file);
    return;
  }
  openCat_ = index;
  itemSel_ = 0;
  checklistShown_ = false;
  show(Screen::List);
}

void WorkoutActivity::openItem(const int index) {
  if (!workout::loadItem(cat_->info, cat_->items[index].offset, *item_)) {
    LOG_ERR("WKT", "Could not read practice %d", index);
    return;
  }
  detailPage_ = 0;
  buildDetailText();
  verseCount_ = 0;
  openingOffset_ = 0;
  if (item_->pages[0]) {
    if (!verse_) verse_ = makeUniqueNoThrow<workout::Verse>();
    if (verse_) verseCount_ = workout::indexVerses(item_->pages, verseOffsets_, workout::kMaxVerses, openingOffset_);
  }
  show(Screen::Detail);
}

void WorkoutActivity::requestStart() {
  if (openingOffset_ != 0 && verse_ && workout::loadVerse(item_->pages, openingOffset_, *verse_)) {
    show(Screen::Opening);
    return;
  }
  startSession();
}

void WorkoutActivity::openVerse(const int index) {
  if (!verse_ || index < 0 || index >= verseCount_ ||
      !workout::loadVerse(item_->pages, verseOffsets_[index], *verse_)) {
    show(Screen::Summary);
    return;
  }
  verseIdx_ = index;
  show(Screen::Verses);
}

void WorkoutActivity::openStats() {
  auto stats = makeUniqueNoThrow<ToolStatsActivity>(renderer, mappedInput, tr(STR_TOOLS_WORKOUT_STATS_TITLE),
                                                    &toolstats::buildWorkout, nullptr, statsx::Feature::Workout, false);
  if (!stats) return;
  startActivityForResult(std::move(stats), [this](const ActivityResult&) {
    input_.reset(mappedInput);
    show(screen_);
  });
}

// The detail text below the figure: what you need, how to, steps, caution,
// why. Uses drawWrappedText's Markdown subset so it wraps and pages.
void WorkoutActivity::buildDetailText() {
  char* t = text_.get();
  const workout::Item& it = *item_;
  t[0] = '\0';
  if (it.accessories[0]) append(t, kTextCap, "## %s\n%s\n", tr(STR_TOOLS_WORKOUT_ACCESSORIES), it.accessories);
  if (it.how[0]) {
    append(t, kTextCap, "## %s\n", tr(STR_TOOLS_WORKOUT_HOW));
    int n = 1;
    const char* line = it.how;
    while (*line) {
      const char* end = strchr(line, '\n');
      const size_t len = end ? static_cast<size_t>(end - line) : strlen(line);
      const size_t at = strlen(t);
      if (at + len + 8 < kTextCap) snprintf(t + at, kTextCap - at, "%d. %.*s\n", n++, static_cast<int>(len), line);
      line += len + (end ? 1 : 0);
    }
  }
  if (it.stepCount > 0) {
    append(t, kTextCap, "## %s\n", tr(STR_TOOLS_WORKOUT_STEPS_HEAD));
    for (int i = 0; i < it.stepCount; ++i) {
      const size_t at = strlen(t);
      // Step names in the packs carry their own numbers ("1. Pranamasana").
      const char* name = it.steps[i].name;
      if (at + 64 < kTextCap)
        snprintf(t + at, kTextCap - at, name[0] >= '0' && name[0] <= '9' ? "%s\n" : "- %s\n", name);
    }
  }
  if (it.caution[0]) append(t, kTextCap, "## %s\n%s\n", tr(STR_TOOLS_WORKOUT_CAUTION), it.caution);
  if (it.benefit[0]) append(t, kTextCap, "## %s\n%s\n", tr(STR_TOOLS_WORKOUT_BENEFIT), it.benefit);
}

void WorkoutActivity::loop() {
  input_.poll(mappedInput);
  if (input_.backLong) {
    if (screen_ == Screen::Session) finishSession(false);
    tools::exitToHome();
    return;
  }
  const bool next = input_.right || input_.down || input_.pageForward;
  const bool prev = input_.up || input_.pageBack;
  switch (screen_) {
    case Screen::Missing:
      if (input_.back) finish();
      break;
    case Screen::Categories:
      if (input_.back) {
        finish();
      } else if (input_.confirm) {
        openCategory(catSel_);
      } else if (input_.leftUp) {
        openStats();
      } else if (next || prev) {
        catSel_ = (catSel_ + (next ? 1 : catCount_ - 1)) % catCount_;
        requestUpdate();
      }
      break;
    case Screen::List:
      if (input_.back) {
        show(Screen::Categories);
      } else if (input_.confirm) {
        openItem(itemSel_);
      } else if (next || prev || input_.left) {
        const int n = cat_->itemCount;
        itemSel_ = (itemSel_ + (next ? 1 : n - 1)) % n;
        requestUpdate();
      }
      break;
    case Screen::Detail:
      if (input_.back) {
        show(Screen::List);
      } else if (input_.confirm) {
        if (cat_->checklistCount > 0 && !checklistShown_) {
          checklistShown_ = true;
          show(Screen::Checklist);
        } else {
          requestStart();
        }
      } else if (input_.confirmLong) {
        slowPace_ = !slowPace_;  // hold Start: Slow pace on/off
        savePace();
        requestUpdate();
      } else if (input_.leftUp) {
        level_ = (level_ + 1) % kLevelCount;  // Rookie .. Go Nuts
        savePace();
        requestUpdate();
      } else if ((next || prev) && detailPages_ > 1) {
        detailPage_ = (detailPage_ + (next ? 1 : detailPages_ - 1)) % detailPages_;
        requestUpdate();
      }
      break;
    case Screen::Checklist:
      if (input_.back) {
        show(Screen::Detail);
      } else if (input_.confirm) {
        requestStart();
      }
      break;
    case Screen::Opening:
      if (input_.back) {
        show(Screen::Detail);
      } else if (input_.confirm) {
        startSession();
      }
      break;
    case Screen::Verses:
      if (input_.back || (input_.confirm && verseIdx_ == verseCount_ - 1)) {
        show(Screen::Summary);
      } else if ((input_.confirm || input_.down || input_.pageForward) && verseIdx_ + 1 < verseCount_) {
        openVerse(verseIdx_ + 1);
      } else if ((prev || input_.left) && verseIdx_ > 0) {
        openVerse(verseIdx_ - 1);
      }
      break;
    case Screen::Session:
      loopSession();
      break;
    case Screen::Summary:
      if (input_.back) {
        show(Screen::Detail);
      } else if (input_.confirm) {
        requestStart();
      } else if (input_.leftUp && verseCount_ > 0) {
        openVerse(0);
      }
      break;
  }
}

void WorkoutActivity::loopSession() {
  const bool interval = item_->type == Type::Interval;
  if (input_.back) {
    finishSession(false);
    return;
  }
  if (interval && input_.confirm) {
    ++reps_;
    requestUpdate();
  } else if ((interval && input_.leftUp) || (!interval && input_.confirm)) {
    togglePause();
  } else if (input_.right) {
    advance(false);
    if (screen_ != Screen::Session) return;
  }
  if (redrawAfterFlash_) {
    redrawAfterFlash_ = false;
    requestUpdate();
  }
  if (!running_) return;
  // Finish every segment whose time is up (more than one after a long stall).
  for (int guard = 0; guard < 8 && screen_ == Screen::Session; ++guard) {
    const Seg s = seg(segIdx_);
    if (elapsedMs() - segStartMs_ < static_cast<uint32_t>(s.seconds) * 1000UL) break;
    advance(true);
  }
  if (screen_ != Screen::Session) return;
  if (item_->type == Type::Breath && elapsedMs() >= totalMs()) {
    finishSession(true);
    return;
  }
  if (elapsedMs() / 1000 != lastShown_) requestUpdate();
}

// ---------------------------------------------------------------------------
// Session engine
// ---------------------------------------------------------------------------

int WorkoutActivity::segCount() const {
  const workout::Item& it = *item_;
  switch (it.type) {
    case Type::Breath:
      return 0x7fff;  // runs until the practice time is up
    case Type::Hold:
      return it.sides * 2 - 1;
    case Type::Sequence:
      return std::max<int>(1, levelValue(LevelField::Rounds, it.rounds[0], it.rounds[1])) * std::max<int>(1, it.stepCount);
    case Type::Interval:
      return std::max<int>(1, levelValue(LevelField::Sets, it.sets[0], it.sets[1])) * 2 - 1;
  }
  return 1;
}

WorkoutActivity::Seg WorkoutActivity::seg(const int i) const {
  const workout::Item& it = *item_;
  switch (it.type) {
    case Type::Breath: {
      const int pc = std::max<int>(1, it.phaseCount);
      const int pattern = pc * it.repeat;
      const int len = pattern + (it.rest.seconds > 0 ? 1 : 0);
      const int j = i % len;
      if (j >= pattern) return {SegKind::BreathRest, 0, it.rest.seconds};
      const int p = j % pc;
      return {SegKind::Phase, static_cast<uint8_t>(p), it.phaseCount ? phaseSeconds(it.phases[p]) : uint16_t{4}};
    }
    case Type::Hold:
      if (i % 2 == 1) return {SegKind::Switch, static_cast<uint8_t>(i / 2), kSwitchSeconds};
      return {SegKind::Hold, static_cast<uint8_t>(i / 2), std::max<uint16_t>(1, levelValue(LevelField::HoldSeconds, it.seconds[0], it.seconds[1]))};
    case Type::Sequence: {
      const int sc = std::max<int>(1, it.stepCount);
      const workout::Step& st = it.steps[i % sc];
      const uint16_t secs = stepSeconds(st.seconds ? st.seconds : levelValue(LevelField::StepSeconds, it.stepSeconds[0], it.stepSeconds[1]));
      return {SegKind::Step, static_cast<uint8_t>(i % sc), std::max<uint16_t>(1, secs)};
    }
    case Type::Interval:
      if (i % 2 == 1)
        return {SegKind::Recover, static_cast<uint8_t>(i / 2), std::max<uint16_t>(1, levelValue(LevelField::Rest, it.restSeconds[0], it.restSeconds[1]))};
      return {SegKind::Work, static_cast<uint8_t>(i / 2), std::max<uint16_t>(1, levelValue(LevelField::Work, it.work[0], it.work[1]))};
  }
  return {SegKind::Hold, 0, 1};
}

uint32_t WorkoutActivity::elapsedMs() const {
  // Unsigned subtraction stays correct across the 49-day millis() wrap.
  return accMs_ + (running_ ? millis() - runStartMs_ : 0);
}

uint32_t WorkoutActivity::totalMs() const { return static_cast<uint32_t>(levelValue(LevelField::Minutes, item_->minutes[0], item_->minutes[1])) * 60000UL; }

void WorkoutActivity::startSession() {
  running_ = true;
  runStartMs_ = millis();
  accMs_ = 0;
  segStartMs_ = 0;
  segIdx_ = 0;
  count_ = 0;
  reps_ = 0;
  lastShown_ = UINT32_MAX;
  fastRefreshes_ = 0;
  flashPending_ = false;
  redrawAfterFlash_ = false;
  show(Screen::Session);
}

void WorkoutActivity::advance(const bool completed) {
  const Seg s = seg(segIdx_);
  const workout::Item& it = *item_;
  if (completed) {
    const bool counts = (s.kind == SegKind::Phase && s.index == it.phaseCount - 1) || s.kind == SegKind::Hold ||
                        s.kind == SegKind::Work ||
                        (s.kind == SegKind::Step && s.index == std::max<int>(1, it.stepCount) - 1);
    if (counts) ++count_;
    segStartMs_ += static_cast<uint32_t>(s.seconds) * 1000UL;
  } else {
    segStartMs_ = elapsedMs();
  }
  ++segIdx_;
  if (segIdx_ >= segCount()) {
    finishSession(completed);
    return;
  }
  // Pose changes in hold and interval sessions flash the screen as a cue;
  // breathing and sequences change too often for that.
  if (it.type == Type::Hold || it.type == Type::Interval) flashPending_ = true;
  requestUpdate();
}

void WorkoutActivity::togglePause() {
  if (running_) {
    accMs_ += millis() - runStartMs_;
    running_ = false;
  } else {
    runStartMs_ = millis();
    running_ = true;
  }
  requestUpdate();
}

void WorkoutActivity::finishSession(const bool done) {
  durationSec_ = elapsedMs() / 1000;
  if (item_->type == Type::Breath) durationSec_ = std::min(durationSec_, totalMs() / 1000);
  running_ = false;
  done_ = done;
  saved_ = false;
  tlog::Stamp at;
  if (durationSec_ >= kMinLogSeconds && tlog::now(at)) {
    char fields[tlog::kLineCap - 20];
    snprintf(fields, sizeof(fields), "%s|%s|%lu|%u|%u|%c|%d", cat_->info.key, item_->id,
             static_cast<unsigned long>(durationSec_), count_, reps_, level_ >= 3 ? 'a' : 'b', done ? 1 : 0);
    saved_ = tlog::append("workout", at, fields);
    if (!saved_) LOG_ERR("WKT", "Could not save the session log");
  }
  // After the last round, the practice's closing verses (if any), then the summary.
  if (done && verseCount_ > 0) {
    openVerse(0);
  } else {
    show(Screen::Summary);
  }
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

bool WorkoutActivity::useKannada(const char* text) const {
  return kannada::ready() && text != nullptr && text[0] && kannada::hasKannada(text);
}

// Draws one line of Kannada on the baseline of UI text drawn at latinTop in
// `font`. Returns the width drawn.
int WorkoutActivity::drawKannadaLine(const char* text, const kannada::Style s, const int x, const int latinTop,
                                     const int font, const int maxW, const bool black) const {
  kannada::Line line[1];
  if (kannada::wrap(renderer, text, s, maxW, line, 1) == 0) return 0;
  const int top = latinTop + renderer.getFontAscenderSize(font) - kannada::ascent(renderer, s);
  return kannada::draw(renderer, x, top, text + line[0].start, s, black, line[0].length);
}

const char* WorkoutActivity::levelName() const {
  static const StrId kNames[kLevelCount] = {StrId::STR_TOOLS_WORKOUT_ROOKIE, StrId::STR_TOOLS_WORKOUT_BEGINNER,
                                            StrId::STR_TOOLS_WORKOUT_EXPERT, StrId::STR_TOOLS_WORKOUT_VETERAN,
                                            StrId::STR_TOOLS_WORKOUT_GO_NUTS};
  static char name[48];
  snprintf(name, sizeof(name), "%s%s%s", I18N.get(kNames[std::clamp(level_, 0, kLevelCount - 1)]),
           slowPace_ ? " · " : "", slowPace_ ? tr(STR_TOOLS_WORKOUT_SLOW_PACE) : "");
  return name;
}

uint16_t WorkoutActivity::levelValue(const LevelField field, const uint16_t beginner, const uint16_t advanced) const {
  const uint16_t b = std::max<uint16_t>(1, beginner);
  const uint16_t a = std::max<uint16_t>(1, advanced);
  const uint16_t mid = static_cast<uint16_t>((b + a + 1) / 2);
  switch (level_) {
    case 1:
      return b;
    case 3:
      return a;
    case 2:
      return mid;
    case 0:  // Rookie: a gentle start
      switch (field) {
        case LevelField::Rounds:
          return 1;
        case LevelField::StepSeconds:
          return std::max<uint16_t>(b, 10);  // slower, guided poses
        case LevelField::HoldSeconds:
          return std::max<uint16_t>(5, std::min<uint16_t>(b, 10));
        case LevelField::Sets:
          return std::max<uint16_t>(1, b - 1);
        case LevelField::Work:
          return std::max<uint16_t>(5, b * 3 / 4);
        case LevelField::Rest:
          return b * 5 / 4;
        case LevelField::Minutes:
        default:
          return std::max<uint16_t>(1, b / 2);
      }
    case 4:  // Go Nuts
    default:
      switch (field) {
        case LevelField::StepSeconds:
          return a;  // no faster than the file's quickest safe flow
        case LevelField::Sets:
          return a + 2;
        case LevelField::Work:
          return a * 5 / 4;
        case LevelField::Rest:
          return std::max<uint16_t>(5, a * 3 / 4);
        case LevelField::Rounds:
        case LevelField::HoldSeconds:
        case LevelField::Minutes:
        default:
          return a * 3 / 2;
      }
  }
}

uint16_t WorkoutActivity::phaseSeconds(const workout::Phase& phase) const {
  // Fast breathing (Kapalabhati, Bhastrika: 1-2 s phases) keeps its rhythm.
  if (!slowPace_ || phase.seconds < 3) return phase.seconds;
  if (phase.shape == workout::Shape::Grow) return 10;    // inhale
  if (phase.shape == workout::Shape::Shrink) return 15;  // exhale
  return phase.seconds;                                  // holds as written
}

uint16_t WorkoutActivity::stepSeconds(const uint16_t written) const {
  return slowPace_ ? std::max<uint16_t>(written, 30) : written;
}

void WorkoutActivity::savePace() const {
  HalFile pace;
  if (!Storage.openFileForWrite("WKT", kPaceFile, pace)) return;
  const uint8_t c[2] = {static_cast<uint8_t>(slowPace_ ? '1' : '0'), static_cast<uint8_t>('0' + level_)};
  pace.write(c, 2);
  pace.close();
}

void WorkoutActivity::timingLine(char* buf, const size_t len) const {
  const workout::Item& it = *item_;
  const char* s = tr(STR_TOOLS_WORKOUT_SEC);
  switch (it.type) {
    case Type::Breath: {
      char pattern[40] = "";
      for (int i = 0; i < it.phaseCount; ++i) {
        const size_t n = strlen(pattern);
        snprintf(pattern + n, sizeof(pattern) - n, i ? "-%u" : "%u", phaseSeconds(it.phases[i]));
      }
      snprintf(buf, len, "%u %s  |  %s %s", levelValue(LevelField::Minutes, it.minutes[0], it.minutes[1]), tr(STR_TOOLS_WORKOUT_MIN), pattern, s);
      break;
    }
    case Type::Hold:
      if (it.sides > 1) {
        snprintf(buf, len, "%u %s  |  %s %u", levelValue(LevelField::HoldSeconds, it.seconds[0], it.seconds[1]), s, tr(STR_TOOLS_WORKOUT_SIDES), it.sides);
      } else {
        snprintf(buf, len, "%u %s", levelValue(LevelField::HoldSeconds, it.seconds[0], it.seconds[1]), s);
      }
      break;
    case Type::Sequence: {
      const uint16_t step = stepSeconds(levelValue(LevelField::StepSeconds, it.stepSeconds[0], it.stepSeconds[1]));
      snprintf(buf, len, "%s %u  |  %s %u  |  %u %s", tr(STR_TOOLS_WORKOUT_ROUNDS), levelValue(LevelField::Rounds, it.rounds[0], it.rounds[1]),
               tr(STR_TOOLS_WORKOUT_STEPS), it.stepCount, step, s);
      break;
    }
    case Type::Interval:
      snprintf(buf, len, "%s %u  |  %s %u %s  |  %s %u %s", tr(STR_TOOLS_WORKOUT_SETS), levelValue(LevelField::Sets, it.sets[0], it.sets[1]),
               tr(STR_TOOLS_WORKOUT_WORK), levelValue(LevelField::Work, it.work[0], it.work[1]), s, tr(STR_TOOLS_WORKOUT_REST), levelValue(LevelField::Rest, it.restSeconds[0], it.restSeconds[1]), s);
      if (it.reps > 0) {
        const size_t n = strlen(buf);
        snprintf(buf + n, len - n, "  |  %s %u", tr(STR_TOOLS_WORKOUT_REPS), it.reps);
      }
      break;
  }
}

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------

void WorkoutActivity::render(RenderLock&&) {
  switch (screen_) {
    case Screen::Missing:
      renderMessage(tr(STR_TOOLS_WORKOUT), tr(STR_TOOLS_WORKOUT_MISSING));
      break;
    case Screen::Categories:
      renderCategories();
      break;
    case Screen::List:
      renderList();
      break;
    case Screen::Detail:
      renderDetail();
      break;
    case Screen::Checklist:
      renderChecklist();
      break;
    case Screen::Opening:
    case Screen::Verses:
      renderVerse();
      break;
    case Screen::Session:
      renderSession();
      break;
    case Screen::Summary:
      renderSummary();
      break;
  }
  if (screen_ == Screen::Session && flashPending_) {
    // Visual cue for a new pose: one inverted frame, then a clean redraw.
    flashPending_ = false;
    renderer.invertScreen();
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    transitionPending_ = true;
    redrawAfterFlash_ = true;
    return;
  }
  bool clean = transitionPending_;
  if (screen_ == Screen::Session) {
    clean = clean || fastRefreshes_ >= 60;
    fastRefreshes_ = clean ? 0 : fastRefreshes_ + 1;
  }
  transitionPending_ = false;
  renderer.displayBuffer(clean ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}

void WorkoutActivity::renderMessage(const char* title, const char* body) {
  const Rect content = tools::drawFrame(renderer, title);
  tools::WrapOptions opt;
  opt.fontId = UI_12_FONT_ID;
  opt.centered = true;
  tools::drawWrappedText(renderer, Rect{content.x, content.y + content.height / 3, content.width, content.height}, body,
                         opt);
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
}

void WorkoutActivity::renderCategories() {
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_WORKOUT));
  const int x0 = content.x;
  const int w = content.width;
  const int rowH = 92;
  for (int i = 0; i < catCount_; ++i) {
    const int ry = content.y + i * rowH;
    if (ry + rowH > content.y + content.height + 4) break;
    const bool on = i == catSel_;
    if (on) renderer.fillRoundedRect(x0 - 6, ry, w + 12, rowH - 8, 10, Color::Black);
    renderer.drawText(UI_12_FONT_ID, x0 + 8, ry + 10, cats_[i].titleEn, !on, EpdFontFamily::BOLD);
    if (useKannada(cats_[i].titleKn)) {
      drawKannadaLine(cats_[i].titleKn, kannada::Style::Label, x0 + 8, ry + 46, UI_12_FONT_ID, w - 16, !on);
    }
  }
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_OPEN), tr(STR_TOOLS_STATS_SHORT), tr(STR_TOOLS_NEXT));
}

void WorkoutActivity::renderList() {
  char sub[16];
  snprintf(sub, sizeof(sub), "%d / %d", itemSel_ + 1, cat_->itemCount);
  const Rect content = tools::drawFrame(renderer, cat_->info.titleEn, sub);
  const int x0 = content.x;
  const int w = content.width;
  const int fit = std::max(1, content.height / kListRowH);
  const int top = itemSel_ / fit * fit;
  for (int i = top; i < cat_->itemCount && i < top + fit; ++i) {
    const workout::ItemInfo& it = cat_->items[i];
    const int ry = content.y + (i - top) * kListRowH;
    const bool on = i == itemSel_;
    if (on) renderer.fillRoundedRect(x0 - 6, ry, w + 12, kListRowH - 6, 10, Color::Black);
    const int gw = it.group[0] ? renderer.getTextWidth(SMALL_FONT_ID, it.group) + 12 : 0;
    const std::string en = renderer.truncatedText(UI_12_FONT_ID, it.nameEn, w - 12 - gw, EpdFontFamily::REGULAR);
    renderer.drawText(UI_12_FONT_ID, x0 + 6, ry + 6, en.c_str(), !on);
    if (gw) renderer.drawText(SMALL_FONT_ID, x0 + w - gw + 6, ry + 10, it.group, !on);
    if (useKannada(it.nameKn)) {
      drawKannadaLine(it.nameKn, kannada::Style::Label, x0 + 6, ry + 36, UI_10_FONT_ID, w - 12, !on);
    }
  }
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_OPEN), tr(STR_TOOLS_PREVIOUS), tr(STR_TOOLS_NEXT));
}

void WorkoutActivity::renderDetail() {
  const workout::Item& it = *item_;
  const Rect content = tools::drawFrame(renderer, cat_->info.titleEn, levelName());
  const int x0 = content.x;
  const int w = content.width;
  workout::drawFigure(renderer, it.image, x0, content.y, 1);

  // Names and timing beside the figure.
  const int tx = x0 + kFigure + 12;
  const int tw = w - kFigure - 12;
  int y = content.y;
  if (useKannada(it.nameKn)) {
    kannada::Line lines[2];
    const size_t n = std::min<size_t>(2, kannada::wrap(renderer, it.nameKn, kannada::Style::Value, tw, lines, 2));
    for (size_t i = 0; i < n; ++i) {
      kannada::draw(renderer, tx, y, it.nameKn + lines[i].start, kannada::Style::Value, true, lines[i].length);
      y += kannada::lineHeight(renderer, kannada::Style::Value);
    }
  }
  tools::WrapOptions bold;
  bold.fontId = UI_12_FONT_ID;
  bold.maxLines = 3;
  char name[72];
  snprintf(name, sizeof(name), "**%s**", it.nameEn);
  bold.markdown = true;
  const int nameLines = std::min(3, tools::drawWrappedText(renderer, Rect{tx, y, tw, 120}, name, bold));
  y += nameLines * renderer.getLineHeight(UI_12_FONT_ID) + 4;
  char timing[96];
  timingLine(timing, sizeof(timing));
  tools::WrapOptions small;
  small.fontId = UI_10_FONT_ID;
  small.maxLines = 3;
  y += tools::drawWrappedText(renderer, Rect{tx, y, tw, 90}, timing, small) * renderer.getLineHeight(UI_10_FONT_ID);
  if (it.group[0]) {
    renderer.drawText(SMALL_FONT_ID, tx, y + 2, it.group, true);
    y += renderer.getLineHeight(SMALL_FONT_ID) + 2;
  }
  renderer.drawText(SMALL_FONT_ID, tx, y + 4, tr(STR_TOOLS_WORKOUT_PACE_HINT), true);

  // Paged text under the figure.
  const int boxTop = std::max(content.y + kFigure + 10, y + 24);
  const Rect box{x0, boxTop, w, content.y + content.height - boxTop};
  tools::WrapOptions opt;
  opt.fontId = UI_10_FONT_ID;
  opt.markdown = true;
  opt.draw = false;
  const int total = tools::drawWrappedText(renderer, box, text_.get(), opt);
  const int perPage =
      std::max(1, box.height / std::max(1, tools::wrapLineHeight(renderer, UI_10_FONT_ID, text_.get())));
  detailPages_ = std::max(1, (total + perPage - 1) / perPage);
  detailPage_ = std::min(detailPage_, detailPages_ - 1);
  opt.draw = true;
  opt.skipLines = detailPage_ * perPage;
  opt.maxLines = opt.skipLines + perPage;
  tools::drawWrappedText(renderer, box, text_.get(), opt);
  if (detailPages_ > 1) {
    char pg[16];
    snprintf(pg, sizeof(pg), "%d / %d", detailPage_ + 1, detailPages_);
    renderer.drawText(SMALL_FONT_ID, x0 + w - renderer.getTextWidth(SMALL_FONT_ID, pg), content.y + content.height, pg,
                      true);
  }
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_START), tr(STR_TOOLS_WORKOUT_LEVEL),
                   detailPages_ > 1 ? tr(STR_TOOLS_WORKOUT_MORE) : "");
}

void WorkoutActivity::renderChecklist() {
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_WORKOUT_CHECKLIST), item_->nameEn);
  const int lineH = std::max(48, renderer.getLineHeight(UI_12_FONT_ID) + 18);
  for (int i = 0; i < cat_->checklistCount; ++i) {
    const int y = content.y + i * lineH;
    if (y + lineH > content.y + content.height) break;
    renderer.drawRect(content.x + 4, y + 4, 26, 26, 2, true);
    renderer.drawText(UI_12_FONT_ID, content.x + 44, y + 2, cat_->checklist[i], true);
  }
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_START), "", "");
}

void WorkoutActivity::renderSession() {
  const workout::Item& it = *item_;
  const uint32_t elapsed = elapsedMs();
  lastShown_ = elapsed / 1000;
  const Rect c = tools::drawFrame(renderer, it.nameEn, running_ ? levelName() : tr(STR_TOOLS_WORKOUT_PAUSED));
  const Seg s = seg(segIdx_);
  const uint32_t segElapsed = elapsed - segStartMs_;
  if (it.type == Type::Breath) {
    renderBreath(c, s, segElapsed);
  } else {
    renderFigureSession(c, s, segElapsed);
  }
  const bool interval = it.type == Type::Interval;
  const char* pause = running_ ? tr(STR_TOOLS_PAUSE) : tr(STR_TOOLS_WORKOUT_RESUME);
  tools::drawHints(renderer, mappedInput, tr(STR_TOOLS_WORKOUT_END), interval ? tr(STR_TOOLS_WORKOUT_REP_PLUS) : pause,
                   interval ? pause : "", tr(STR_TOOLS_SKIP));
}

void WorkoutActivity::renderBreath(const Rect& c, const Seg& s, const uint32_t segElapsedMs) {
  const workout::Item& it = *item_;
  const workout::Phase& ph = s.kind == SegKind::BreathRest ? it.rest : it.phases[s.index];
  const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
  const int areaH = c.height - 3 * lineH - 70;
  const int R = std::max(50, std::min(c.width, areaH) / 2 - 4);
  const int cx = renderer.getScreenWidth() / 2;
  const int cy = c.y + 8 + R;
  const int rMin = R / 5;
  const int rMax = R - 14;
  const uint32_t segMs = std::max<uint32_t>(1000, static_cast<uint32_t>(s.seconds) * 1000UL);
  // Progress through the phase, stepped to the whole second just shown.
  const float f = std::min(1.0f, static_cast<float>(segElapsedMs / 1000 + 1) * 1000.0f / static_cast<float>(segMs));
  float size = 1.0f;
  switch (ph.shape) {
    case Shape::Grow:
      size = f;
      break;
    case Shape::Shrink:
      size = 1.0f - f;
      break;
    case Shape::Hold:
      size = 1.0f;
      break;
    case Shape::Empty:
      size = 0.0f;
      break;
    case Shape::Pulse:
      size = (segElapsedMs / 500) % 2 ? 0.15f : 0.55f;
      break;
  }
  tools::drawProgressRing(renderer, cx, cy, R, 10, 0.0f);  // guide: the full breath
  const int r = rMin + static_cast<int>(static_cast<float>(rMax - rMin) * size);
  tools::drawProgressRing(renderer, cx, cy, r, r, 1.0f);

  int y = cy + R + 14;
  renderer.drawCenteredText(UI_12_FONT_ID, y, ph.label[0] ? ph.label : tr(STR_TOOLS_WORKOUT_REST), true,
                            EpdFontFamily::BOLD);
  y += lineH + 4;
  const uint32_t segLeft = (segMs - std::min(segMs, segElapsedMs) + 999) / 1000;
  char buf[48];
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(segLeft));
  renderer.drawCenteredText(TOOLS_DIGITS_30_FONT_ID, y, buf);
  y += renderer.getLineHeight(TOOLS_DIGITS_30_FONT_ID) + 6;
  char left[12];
  const uint32_t total = totalMs();
  formatMmSs(left, sizeof(left), (total - std::min(total, elapsedMs()) + 999) / 1000);
  snprintf(buf, sizeof(buf), "%s %u  |  %s %s", tr(STR_TOOLS_WORKOUT_CYCLES), count_, left, tr(STR_TOOLS_WORKOUT_LEFT));
  renderer.drawCenteredText(UI_10_FONT_ID, y, buf);
}

void WorkoutActivity::renderFigureSession(const Rect& c, const Seg& s, const uint32_t segElapsedMs) {
  const workout::Item& it = *item_;
  const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
  const bool roomy = c.height >= 600 && c.width >= 2 * kFigure;
  const int scale = roomy ? 2 : 1;
  const int fig = kFigure * scale;
  const char* image = it.image;
  const workout::Step* step = nullptr;
  if (s.kind == SegKind::Step) {
    step = &it.steps[s.index];
    if (step->image[0]) image = step->image;
    if (step->mantraKn[0] || step->mantraEn[0]) {
      renderMantraStep(c, s, segElapsedMs);
      return;
    }
  }
  int y = c.y;
  workout::drawFigure(renderer, image, (renderer.getScreenWidth() - fig) / 2, y, scale);
  y += fig + 8;

  // What this segment is.
  char buf[96];
  switch (s.kind) {
    case SegKind::Step:
      if (useKannada(step->nameKn)) {
        const int kw = kannada::width(renderer, step->nameKn, kannada::Style::Label);
        drawKannadaLine(step->nameKn, kannada::Style::Label, std::max(c.x, (renderer.getScreenWidth() - kw) / 2), y,
                        UI_12_FONT_ID, c.width, true);
        y += kannada::lineHeight(renderer, kannada::Style::Label);
      }
      renderer.drawCenteredText(UI_12_FONT_ID, y, step->name, true, EpdFontFamily::BOLD);
      y += lineH;
      snprintf(buf, sizeof(buf), "%s %u / %u  |  %s %u / %u", tr(STR_TOOLS_WORKOUT_ROUNDS),
               static_cast<unsigned>(segIdx_ / std::max<int>(1, it.stepCount) + 1), levelValue(LevelField::Rounds, it.rounds[0], it.rounds[1]),
               tr(STR_TOOLS_WORKOUT_STEPS), static_cast<unsigned>(s.index + 1), it.stepCount);
      break;
    case SegKind::Hold:
    case SegKind::Switch:
      renderer.drawCenteredText(UI_12_FONT_ID, y,
                                s.kind == SegKind::Hold ? tr(STR_TOOLS_WORKOUT_HOLD) : tr(STR_TOOLS_WORKOUT_SWITCH),
                                true, EpdFontFamily::BOLD);
      y += lineH;
      if (it.sides > 1) {
        snprintf(buf, sizeof(buf), "%s %u / %u", tr(STR_TOOLS_WORKOUT_SIDES),
                 static_cast<unsigned>(s.index + (s.kind == SegKind::Switch ? 2 : 1)), it.sides);
      } else {
        buf[0] = '\0';
      }
      break;
    default:  // Work / Recover
      renderer.drawCenteredText(UI_12_FONT_ID, y,
                                s.kind == SegKind::Work ? tr(STR_TOOLS_WORKOUT_WORK) : tr(STR_TOOLS_WORKOUT_REST), true,
                                EpdFontFamily::BOLD);
      y += lineH;
      snprintf(buf, sizeof(buf), "%s %u / %u", tr(STR_TOOLS_WORKOUT_SETS),
               static_cast<unsigned>(s.index + (s.kind == SegKind::Recover ? 2 : 1)), levelValue(LevelField::Sets, it.sets[0], it.sets[1]));
      break;
  }

  // Seconds left in this segment, with a bar.
  const uint32_t segMs = static_cast<uint32_t>(s.seconds) * 1000UL;
  const uint32_t segLeft = (segMs - std::min(segMs, segElapsedMs) + 999) / 1000;
  char digits[12];
  formatMmSs(digits, sizeof(digits), segLeft);
  renderer.drawCenteredText(TOOLS_DIGITS_30_FONT_ID, y, digits);
  y += renderer.getLineHeight(TOOLS_DIGITS_30_FONT_ID) + 4;
  const int barW = c.width * 3 / 4;
  const int barX = (renderer.getScreenWidth() - barW) / 2;
  renderer.drawRect(barX, y, barW, 12, 2, true);
  const uint32_t done = std::min(segMs, segElapsedMs / 1000 * 1000);
  if (segMs > 0) renderer.fillRect(barX, y, static_cast<int>(static_cast<uint64_t>(barW) * done / segMs), 12, true);
  y += 22;
  if (buf[0]) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, buf);
    y += renderer.getLineHeight(UI_10_FONT_ID);
  }
  if (it.type == Type::Interval) {
    if (it.reps > 0) {
      snprintf(buf, sizeof(buf), "%s %u  |  %s %u", tr(STR_TOOLS_WORKOUT_REPS), reps_, tr(STR_TOOLS_WORKOUT_TARGET),
               it.reps);
    } else {
      snprintf(buf, sizeof(buf), "%s %u", tr(STR_TOOLS_WORKOUT_REPS), reps_);
    }
    renderer.drawCenteredText(UI_12_FONT_ID, y, buf, true, EpdFontFamily::BOLD);
    y += lineH;
  }
  // The step's cue under everything else (sequence practices).
  if (step != nullptr && step->text[0] && y < c.y + c.height - lineH) {
    tools::WrapOptions opt;
    opt.fontId = UI_10_FONT_ID;
    opt.centered = true;
    opt.maxLines = 4;
    tools::drawWrappedText(renderer, Rect{c.x, y + 4, c.width, c.y + c.height - y - 4}, step->text, opt);
  }
}

void WorkoutActivity::renderSummary() {
  const workout::Item& it = *item_;
  const Rect c =
      tools::drawFrame(renderer, done_ ? tr(STR_TOOLS_WORKOUT_DONE_TITLE) : tr(STR_TOOLS_WORKOUT_ENDED_TITLE));
  int y = c.y;
  if (useKannada(it.nameKn)) {
    const int kw = kannada::width(renderer, it.nameKn, kannada::Style::Value);
    kannada::draw(renderer, std::max(c.x, (renderer.getScreenWidth() - kw) / 2), y, it.nameKn, kannada::Style::Value);
    y += kannada::lineHeight(renderer, kannada::Style::Value);
  }
  renderer.drawCenteredText(UI_12_FONT_ID, y, it.nameEn, true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 16;

  char mmss[12];
  formatMmSs(mmss, sizeof(mmss), durationSec_);
  renderer.drawCenteredText(TOOLS_DIGITS_44_FONT_ID, y, mmss);
  y += renderer.getLineHeight(TOOLS_DIGITS_44_FONT_ID) + 4;
  renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_TOOLS_WORKOUT_DURATION));
  y += renderer.getLineHeight(UI_10_FONT_ID) + 24;

  const char* countLabel = tr(STR_TOOLS_WORKOUT_CYCLES);
  if (it.type == Type::Hold) countLabel = tr(STR_TOOLS_WORKOUT_HOLDS);
  if (it.type == Type::Sequence) countLabel = tr(STR_TOOLS_WORKOUT_ROUNDS);
  if (it.type == Type::Interval) countLabel = tr(STR_TOOLS_WORKOUT_SETS);
  char line[64];
  const int lineH = renderer.getLineHeight(UI_12_FONT_ID) + 8;
  snprintf(line, sizeof(line), "%s: %u", countLabel, count_);
  renderer.drawCenteredText(UI_12_FONT_ID, y, line);
  y += lineH;
  if (it.type == Type::Interval) {
    snprintf(line, sizeof(line), "%s: %u", tr(STR_TOOLS_WORKOUT_REPS), reps_);
    renderer.drawCenteredText(UI_12_FONT_ID, y, line);
    y += lineH;
  }
  snprintf(line, sizeof(line), "%s: %s", tr(STR_TOOLS_WORKOUT_LEVEL), levelName());
  renderer.drawCenteredText(UI_12_FONT_ID, y, line);
  y += lineH + 16;
  const char* status =
      saved_ ? tr(STR_TOOLS_WORKOUT_SAVED)
             : (durationSec_ < kMinLogSeconds ? tr(STR_TOOLS_WORKOUT_TOO_SHORT) : tr(STR_TOOLS_WORKOUT_NOT_SAVED));
  renderer.drawCenteredText(UI_10_FONT_ID, y, status);
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_WORKOUT_AGAIN),
                   verseCount_ > 0 ? tr(STR_TOOLS_WORKOUT_MANTRAS) : "", "");
}

// A step with a mantra (Surya Namaskara): a smaller figure, the pose, its
// mantra in Kannada and English, the meaning and the breath, then the timer.
void WorkoutActivity::renderMantraStep(const Rect& c, const Seg& s, const uint32_t segElapsedMs) {
  const workout::Item& it = *item_;
  const workout::Step& st = it.steps[s.index];
  const int sw = renderer.getScreenWidth();
  const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
  const int smallH = renderer.getLineHeight(UI_10_FONT_ID);
  int y = c.y;
  workout::drawFigure(renderer, st.image[0] ? st.image : it.image, (sw - kFigure) / 2, y, 1);
  y += kFigure + 4;

  // The pose.
  if (useKannada(st.nameKn)) {
    const int kw = std::min(c.width, kannada::width(renderer, st.nameKn, kannada::Style::Label));
    drawKannadaLine(st.nameKn, kannada::Style::Label, (sw - kw) / 2, y, UI_10_FONT_ID, c.width, true);
    y += kannada::lineHeight(renderer, kannada::Style::Label);
  }
  const std::string pose = renderer.truncatedText(UI_10_FONT_ID, st.name, c.width, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, y, pose.c_str(), true, EpdFontFamily::BOLD);
  y += smallH + 6;

  // The mantra, boxed.
  const int boxTop = y;
  y += 6;
  if (useKannada(st.mantraKn)) {
    const int kw = std::min(c.width - 16, kannada::width(renderer, st.mantraKn, kannada::Style::Value));
    kannada::Line line[1];
    if (kannada::wrap(renderer, st.mantraKn, kannada::Style::Value, c.width - 16, line, 1) > 0) {
      kannada::draw(renderer, (sw - kw) / 2, y, st.mantraKn + line[0].start, kannada::Style::Value, true,
                    line[0].length);
    }
    y += kannada::lineHeight(renderer, kannada::Style::Value);
  }
  if (st.mantraEn[0]) {
    renderer.drawCenteredText(UI_12_FONT_ID, y, st.mantraEn, true, EpdFontFamily::BOLD);
    y += lineH;
  }
  if (st.meaning[0]) {
    tools::WrapOptions opt;
    opt.fontId = UI_10_FONT_ID;
    opt.centered = true;
    opt.maxLines = 3;
    const int n = tools::drawWrappedText(renderer, Rect{c.x + 8, y, c.width - 16, 3 * smallH}, st.meaning, opt);
    y += std::min(3, n) * smallH;
  }
  y += 6;
  renderer.drawRect(c.x, boxTop, c.width, y - boxTop, 2, true);
  y += 8;

  // Breath and the seconds left in this pose.
  const uint32_t segMs = static_cast<uint32_t>(s.seconds) * 1000UL;
  const uint32_t segLeft = (segMs - std::min(segMs, segElapsedMs) + 999) / 1000;
  char buf[64];
  if (st.breath[0]) {
    renderer.drawCenteredText(UI_12_FONT_ID, y, st.breath, true, EpdFontFamily::BOLD);
    y += lineH;
  }
  char digits[12];
  formatMmSs(digits, sizeof(digits), segLeft);
  renderer.drawCenteredText(TOOLS_DIGITS_30_FONT_ID, y, digits);
  y += renderer.getLineHeight(TOOLS_DIGITS_30_FONT_ID) + 2;
  const int barW = c.width * 3 / 4;
  const int barX = (sw - barW) / 2;
  renderer.drawRect(barX, y, barW, 10, 2, true);
  const uint32_t done = std::min(segMs, segElapsedMs / 1000 * 1000);
  if (segMs > 0) renderer.fillRect(barX, y, static_cast<int>(static_cast<uint64_t>(barW) * done / segMs), 10, true);
  y += 18;
  snprintf(buf, sizeof(buf), "%s %u / %u  |  %s %u / %u", tr(STR_TOOLS_WORKOUT_ROUNDS),
           static_cast<unsigned>(segIdx_ / std::max<int>(1, it.stepCount) + 1), levelValue(LevelField::Rounds, it.rounds[0], it.rounds[1]),
           tr(STR_TOOLS_WORKOUT_STEPS), static_cast<unsigned>(s.index + 1), it.stepCount);
  renderer.drawCenteredText(UI_10_FONT_ID, y, buf);
  y += smallH;
  if (st.text[0] && y < c.y + c.height - smallH) {
    tools::WrapOptions opt;
    opt.fontId = UI_10_FONT_ID;
    opt.centered = true;
    opt.maxLines = 2;
    tools::drawWrappedText(renderer, Rect{c.x, y + 2, c.width, c.y + c.height - y - 2}, st.text, opt);
  }
}

// One verse page: section title, the verse in Kannada, the transliteration and
// the meaning. The opening verse uses the same page before the session.
void WorkoutActivity::renderVerse() {
  const workout::Verse& v = *verse_;
  const bool opening = screen_ == Screen::Opening;
  char sub[16] = "";
  if (!opening) snprintf(sub, sizeof(sub), "%d / %d", verseIdx_ + 1, verseCount_);
  const Rect c = tools::drawFrame(renderer, v.sectionEn, sub);
  const int sw = renderer.getScreenWidth();
  int y = c.y;
  if (useKannada(v.sectionKn)) {
    const int kw = std::min(c.width, kannada::width(renderer, v.sectionKn, kannada::Style::Label));
    drawKannadaLine(v.sectionKn, kannada::Style::Label, (sw - kw) / 2, y, UI_10_FONT_ID, c.width, true);
    y += kannada::lineHeight(renderer, kannada::Style::Label);
  }
  if (v.number > 0) {
    char num[24];
    snprintf(num, sizeof(num), "%s %u", tr(STR_TOOLS_WORKOUT_VERSE), v.number);
    renderer.drawCenteredText(UI_10_FONT_ID, y, num);
    y += renderer.getLineHeight(UI_10_FONT_ID);
  }
  y += 10;
  if (useKannada(v.kn)) {
    constexpr size_t kMaxLines = 9;
    kannada::Line lines[kMaxLines];
    const size_t n =
        std::min(kMaxLines, kannada::wrap(renderer, v.kn, kannada::Style::Body, c.width, lines, kMaxLines));
    const int lh = kannada::lineHeight(renderer, kannada::Style::Body) + 2;
    for (size_t i = 0; i < n; ++i) {
      const int lw = kannada::width(renderer, v.kn + lines[i].start, kannada::Style::Body, lines[i].length);
      kannada::draw(renderer, (sw - lw) / 2, y, v.kn + lines[i].start, kannada::Style::Body, true, lines[i].length);
      y += lh;
    }
    y += 12;
  }
  const int bottom = c.y + c.height;
  tools::WrapOptions opt;
  opt.fontId = UI_12_FONT_ID;
  opt.centered = true;
  if (v.en[0] && y < bottom) {
    opt.maxLines = 5;
    const int n = tools::drawWrappedText(renderer, Rect{c.x, y, c.width, bottom - y}, v.en, opt);
    y += std::min(5, n) * renderer.getLineHeight(UI_12_FONT_ID) + 16;
  }
  if (v.meaning[0] && y + 2 * renderer.getLineHeight(UI_10_FONT_ID) < bottom) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_TOOLS_MANTRA_MEANING), true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_10_FONT_ID) + 2;
    opt.fontId = UI_10_FONT_ID;
    opt.maxLines = (bottom - y) / renderer.getLineHeight(UI_10_FONT_ID);
    tools::drawWrappedText(renderer, Rect{c.x, y, c.width, bottom - y}, v.meaning, opt);
  }
  if (opening) {
    tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_START), "", "");
  } else {
    const bool last = verseIdx_ == verseCount_ - 1;
    tools::drawHints(renderer, mappedInput, tr(STR_TOOLS_WORKOUT_CLOSE), last ? tr(STR_DONE) : tr(STR_TOOLS_NEXT),
                     verseIdx_ > 0 ? tr(STR_TOOLS_PREVIOUS) : "", "");
  }
}
