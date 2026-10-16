#include "PanchangaActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "KannadaText.h"
#include "PanchangaCalendarActivity.h"
#include "PanchangaEnglish.h"
#include "activities/util/OptionSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

constexpr char PanchangaActivity::kConfigPath[];

namespace {
constexpr int kTimeFont = UI_12_FONT_ID;
// Row line box: the Kannada value face (21 px bold) is 30 px tall; English
// rows use the same box so both languages share one layout.
constexpr int kLineH = 30;

// Every English table mirrors its Kannada twin entry for entry.
template <typename A, typename B>
constexpr bool sameSize(const A&, const B&) {
  return sizeof(A) / sizeof(const char*) == sizeof(B) / sizeof(const char*);
}
static_assert(sameSize(en::kWeekday, kn::kWeekday));
static_assert(sameSize(en::kGregorianMonth, kn::kGregorianMonth));
static_assert(sameSize(en::kMasa, kn::kMasa));
static_assert(sameSize(en::kSamvatsara, kn::kSamvatsara));
static_assert(sameSize(en::kTithi, kn::kTithi));
static_assert(sameSize(en::kNakshatra, kn::kNakshatra));
static_assert(sameSize(en::kYoga, kn::kYoga));
static_assert(sameSize(en::kKarana, kn::kKarana));
static_assert(sameSize(en::kSpecial, kn::kSpecial));
static_assert(sameSize(en::kLabel, kn::kLabel));

// Latin text (digits, punctuation) vertically centred on a row line box.
int drawLatin(const GfxRenderer& r, const int x, const int lineTop, const int lineH, const char* text,
              const bool bold = true) {
  const int fh = r.getLineHeight(kTimeFont);
  r.drawText(kTimeFont, x, lineTop + (lineH - fh) / 2 + 1, text, true,
             bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  return r.getTextWidth(kTimeFont, text, bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
}

// Drik Panchang convention: sunrise/sunset and the kaala windows derived from
// them are rounded to the nearest minute; element end times are truncated.
void formatHm(char* buf, const size_t len, const double jd, const double tz, const bool roundNearest = true) {
  const double h = panchanga::localHours(jd, tz) + (roundNearest ? 0.5 / 60.0 : 0.0);
  const int hh = static_cast<int>(h) % 24;
  const int mm = static_cast<int>((h - std::floor(h)) * 60.0);
  snprintf(buf, len, "%02d:%02d", hh, mm);
}

// Tithi name index into kn::kTithi: 0..13 pratipada..chaturdashi, 14 purnima, 15 amavasya.
int tithiName(const int tithi) {
  if (tithi == 14) return 14;
  if (tithi == 29) return 15;
  return tithi % 15;
}

// The festival-file festival an engine festival corresponds to, or -1.
int mappedOf(const panchanga::Special s) {
  using S = panchanga::Special;
  switch (s) {
    case S::Ugadi:
      return festivals::kUgadi;
    case S::Janmashtami:
      return festivals::kJanmashtami;
    case S::GaneshaChaturthi:
      return festivals::kGaneshaChaturthi;
    case S::NavaratriStart:
      return festivals::kNavaratriStart;
    case S::Vijayadashami:
      return festivals::kVijayadashami;
    case S::Deepavali:
      return festivals::kDeepavali;
    case S::MahaShivaratri:
      return festivals::kShivaratri;
    case S::MakaraSankranti:
      return festivals::kMakaraSankranti;
    default:
      return -1;
  }
}

// Moon disc with a terminator ellipse; the dark side is a dense dither so the
// whole disc reads on white e-ink.
void drawMoon(const GfxRenderer& r, const int cx, const int cy, const int radius, const double elongationDeg) {
  const double e = panchanga::norm360(elongationDeg);
  const bool waxing = e < 180.0;
  const double c = std::cos(panchanga::rad(e));
  for (int dy = -radius; dy <= radius; ++dy) {
    const int w = static_cast<int>(std::sqrt(static_cast<double>(radius * radius - dy * dy)));
    const double xt = w * c;
    for (int dx = -w; dx <= w; ++dx) {
      const bool lit = waxing ? dx > xt : dx < -xt;
      if (!lit && !((dx + radius) % 3 == 0 && (dy + radius) % 3 == 0)) r.drawPixel(cx + dx, cy + dy, true);
    }
  }
  // Outline so the lit side has an edge.
  for (int a = 0; a < 360; a += 2) {
    const double ar = panchanga::rad(a);
    r.drawPixel(cx + static_cast<int>(std::lround(radius * std::cos(ar))),
                cy + static_cast<int>(std::lround(radius * std::sin(ar))), true);
  }
}
}  // namespace

int32_t PanchangaActivity::firstDay() { return tools::daysFromCivil(1976, 1, 1); }
int32_t PanchangaActivity::lastDay() { return tools::daysFromCivil(2075, 12, 31); }

bool PanchangaActivity::writeConfig(FsFile& out, void* ctx) {
  const auto* self = static_cast<const PanchangaActivity*>(ctx);
  char body[360];
  snprintf(body, sizeof(body),
           "# PocketDeck-OS Panchanga settings\n"
           "# lat/lon in decimal degrees (north/east positive), tz in hours from UTC.\n"
           "lat=%.4f\n"
           "lon=%.4f\n"
           "tz=%g\n"
           "# animate=0 turns off the Moon phase sweep\n"
           "animate=%d\n"
           "# lang=kn (Kannada, needs /tools/fonts/kannada.knf) or lang=en (English)\n"
           "lang=%s\n",
           self->lat_, self->lon_, self->tz_, self->animate_ ? 1 : 0, self->english_ ? "en" : "kn");
  return tools::writeText(out, body);
}

bool PanchangaActivity::saveConfig() const {
  // writeConfig only reads through ctx; WriteFn's signature needs a mutable pointer.
  return tools::writeFileAtomic(kConfigPath, &PanchangaActivity::writeConfig, const_cast<PanchangaActivity*>(this));
}

void PanchangaActivity::loadConfig() {
  tools::recoverFromBackup(kConfigPath);
  if (!Storage.exists(kConfigPath)) {
    saveConfig();
    return;
  }
  FsFile f;
  if (!Storage.openFileForRead("PANCH", kConfigPath, f)) return;
  char line[64];
  while (tools::readLine(f, line, sizeof(line)) >= 0) {
    if (line[0] == '#') continue;
    char* eq = strchr(line, '=');
    if (eq == nullptr) continue;
    *eq = '\0';
    if (strcmp(line, "lang") == 0) {
      english_ = strncmp(eq + 1, "en", 2) == 0;
      continue;
    }
    const double v = atof(eq + 1);
    if (strcmp(line, "lat") == 0 && v >= -66 && v <= 66) lat_ = v;
    if (strcmp(line, "lon") == 0 && v >= -180 && v <= 180) lon_ = v;
    if (strcmp(line, "tz") == 0 && v >= -12 && v <= 14) tz_ = v;
    if (strcmp(line, "animate") == 0) animate_ = v != 0;
  }
  f.close();
  defaultPlace_ = std::fabs(lat_ - 12.9716) < 0.01 && std::fabs(lon_ - 77.5946) < 0.01;
}

bool PanchangaActivity::kannadaMode() const { return !english_ && kannadaFont_; }

void PanchangaActivity::openMenu() {
  enum Action : uint8_t { Today, Calendar, Jump, Details, Language, PrevMonth, NextMonth, Animation };
  std::vector<std::string> options;
  std::array<Action, 8> actions{};
  options.reserve(actions.size());
  auto add = [&](const char* label, const Action a) {
    actions[options.size()] = a;
    options.emplace_back(label);
  };
  add(tr(STR_TOOLS_PANCH_GO_TODAY), Today);
  add(tr(STR_TOOLS_PANCH_CALENDAR), Calendar);
  add(tr(STR_TOOLS_PANCH_JUMP), Jump);
  add(tr(STR_TOOLS_PANCH_ABOUT_DAY), Details);
  add(english_ ? tr(STR_TOOLS_PANCH_TO_KANNADA) : tr(STR_TOOLS_PANCH_TO_ENGLISH), Language);
  add(tr(STR_TOOLS_PANCH_PREV_MONTH), PrevMonth);
  add(tr(STR_TOOLS_PANCH_NEXT_MONTH), NextMonth);
  add(animate_ ? tr(STR_TOOLS_PANCH_ANIM_OFF) : tr(STR_TOOLS_PANCH_ANIM_ON), Animation);
  auto picker = makeUniqueNoThrow<OptionSelectionActivity>(renderer, mappedInput, "PanchangaMenu",
                                                           StrId::STR_TOOLS_PANCHANGA, std::move(options), 0);
  if (!picker) {
    LOG_ERR("PANCH", "OOM creating menu");
    return;
  }
  startActivityForResult(std::move(picker), [this, actions](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    const auto* sel = std::get_if<OptionSelectionResult>(&result.data);
    if (!result.isCancelled && sel != nullptr && sel->index < actions.size()) {
      switch (actions[sel->index]) {
        case Today: {
          RenderLock lock(*this);
          selectDay(today_);
          startAnimation();
          break;
        }
        case Calendar:
          openCalendar(false);
          return;
        case Jump:
          openCalendar(true);
          return;
        case Details:
          openDetails();
          break;
        case Language: {
          RenderLock lock(*this);
          english_ = !english_;
          if (!saveConfig()) LOG_ERR("PANCH", "Could not save %s", kConfigPath);
          loadDayNames();
          break;
        }
        case PrevMonth: {
          RenderLock lock(*this);
          selectDay(selected_ - 30);
          startAnimation();
          break;
        }
        case NextMonth: {
          RenderLock lock(*this);
          selectDay(selected_ + 30);
          startAnimation();
          break;
        }
        case Animation:
          animate_ = !animate_;
          if (!saveConfig()) LOG_ERR("PANCH", "Could not save %s", kConfigPath);
          break;
      }
    }
    requestUpdate();
  });
}

void PanchangaActivity::openCalendar(const bool jump) {
  auto cal = makeUniqueNoThrow<PanchangaCalendarActivity>(renderer, mappedInput, selected_, today_, english_, jump);
  if (!cal) {
    LOG_ERR("PANCH", "OOM creating calendar");
    return;
  }
  startActivityForResult(std::move(cal), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    const auto* picked = std::get_if<IntervalResult>(&result.data);
    if (!result.isCancelled && picked != nullptr) {
      RenderLock lock(*this);
      selectDay(static_cast<int32_t>(picked->value));
      startAnimation();
    }
    requestUpdate();
  });
}

void PanchangaActivity::openDetails() {
  screen_ = Screen::Details;
  detailsTop_ = 0;
  transitionPending_ = true;
}

int PanchangaActivity::text(const char* k, const char* e, const int x, const int y, const Sty sty,
                            const bool draw) const {
  char utf8[160];
  return textUtf8(kannadaMode() ? kn::toUtf8(k, utf8, sizeof(utf8)) : k, e, x, y, sty, draw);
}

int PanchangaActivity::textUtf8(const char* k, const char* e, const int x, const int y, const Sty sty,
                                const bool draw) const {
  if (kannadaMode()) {
    const auto style = sty == Sty::Label   ? kannada::Style::Label
                       : sty == Sty::Title ? kannada::Style::Title
                                           : kannada::Style::Value;
    // Labels use a smaller face than values; put them on the same baseline.
    const int nudge = sty == Sty::Label ? kannada::lineHeight(renderer, kannada::Style::Value) -
                                              kannada::lineHeight(renderer, kannada::Style::Label) - 1
                                        : 0;
    return draw ? kannada::draw(renderer, x, y + nudge, k, style) : kannada::width(renderer, k, style);
  }
  const int font = sty == Sty::Label ? UI_10_FONT_ID : UI_12_FONT_ID;
  const auto style = sty == Sty::Label ? EpdFontFamily::REGULAR : EpdFontFamily::BOLD;
  if (draw) renderer.drawText(font, x, y + (kLineH - renderer.getLineHeight(font)) / 2 + 1, e, true, style);
  return renderer.getTextWidth(font, e, style);
}

void PanchangaActivity::selectDay(const int32_t day) {
  selected_ = std::clamp(day, firstDay(), lastDay());
  uint16_t y = 0;
  uint8_t m = 0, d = 0;
  tools::civilFromDays(selected_, y, m, d);
  day_ = panchanga::compute(y, m, d, tz_, lat_, lon_);
  // Moon widget: current position for today, local noon otherwise.
  double when = panchanga::julianDay(y, m, d, 12.0 - tz_);
  tools::DateTime now;
  if (selected_ == today_ && tools::getUtcNow(now)) {
    when = panchanga::julianDay(now.year, now.month, now.day, now.hour + now.minute / 60.0);
  }
  moonElongation_ = panchanga::elongation(when);
  moonNakshatra_ = static_cast<int>(panchanga::moonSidereal(when) / (360.0 / 27.0)) % 27;
  loadDayNames();
}

// Festival-file entries first, then calculated festivals and observances that
// no file covers for this year.
void PanchangaActivity::loadDayNames() {
  nameCount_ = 0;
  festivals::Ref refs[kMaxDayNames];
  const size_t n = indexPending_ ? 0 : calendar_.find(selected_, selected_, refs, kMaxDayNames);
  festivals::Entry e;
  for (size_t i = 0; i < n && nameCount_ < kMaxDayNames; ++i) {
    if (!calendar_.read(refs[i], e, false)) continue;
    DayName& d = names_[nameCount_++];
    snprintf(d.kannada, sizeof(d.kannada), "%s", e.kannada[0] ? e.kannada : e.english);
    snprintf(d.english, sizeof(d.english), "%s", e.english[0] ? e.english : e.kannada);
    d.holiday = e.holiday == 1;
    d.layer = true;
    d.special = -1;
    d.ref = refs[i];
  }
  const uint16_t mask = calendar_.yearMask(day_.year);
  panchanga::Special sp[4];
  const int count = panchanga::specials(day_, sp, 4);
  for (int i = 0; i < count && nameCount_ < kMaxDayNames; ++i) {
    const int mapped = mappedOf(sp[i]);
    if (mapped >= 0 && (mask & (1u << mapped))) continue;  // a festival file decides this one
    DayName& d = names_[nameCount_++];
    const int idx = static_cast<int>(sp[i]);
    kn::toUtf8(kn::kSpecial[idx], d.kannada, sizeof(d.kannada));
    snprintf(d.english, sizeof(d.english), "%s", en::kSpecial[idx]);
    d.holiday = false;
    d.layer = false;
    d.special = static_cast<int8_t>(idx);
    d.ref = {};
  }
}

void PanchangaActivity::startAnimation() {
  if (!animate_) return;
  animFrame_ = 0;
  animLastMs_ = millis();
}

void PanchangaActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  tools::ensureToolsDirs();
  loadConfig();
  kannadaFont_ = kannada::acquire();
  tools::DateTime local;
  clockValid_ = tools::getTimeAtOffset(static_cast<int16_t>(tz_ * 60.0), local);
  today_ = clockValid_ ? tools::daysOf(local) : 0;
  indexPending_ = true;
  if (clockValid_) {
    selectDay(today_);
    startAnimation();
  }
  transitionPending_ = true;
  requestUpdate();
}

void PanchangaActivity::onExit() {
  kannada::release();
  Activity::onExit();
}

void PanchangaActivity::loop() {
  input_.poll(mappedInput);
  if (input_.backLong) {
    tools::exitToHome();
    return;
  }
  if (indexPending_) {
    // After the first frame: check the festival files (and rebuild their
    // index if they changed), then show the day's festivals.
    calendar_.refresh();
    RenderLock lock(*this);
    indexPending_ = false;
    if (clockValid_) loadDayNames();
    requestUpdate();
    return;
  }
  if (screen_ == Screen::Details) {
    if (input_.back) {
      screen_ = Screen::Main;
      transitionPending_ = true;
      requestUpdate();
    } else if ((input_.down || input_.pageForward || input_.right) && detailsTop_ + 1 < nameCount_) {
      ++detailsTop_;
      requestUpdate();
    } else if ((input_.up || input_.pageBack || input_.left) && detailsTop_ > 0) {
      --detailsTop_;
      requestUpdate();
    }
    return;
  }
  if (input_.back) {
    finish();
    return;
  }
  if (!clockValid_) return;

  int32_t target = selected_;
  if (input_.left) target -= 1;
  if (input_.right) target += 1;
  if (input_.up || input_.pageBack) target -= 30;
  if (input_.down || input_.pageForward) target += 30;
  if (input_.confirm) {
    openMenu();
    return;
  }
  if (target != selected_) {
    {
      RenderLock lock(*this);  // day_ is read by render()
      selectDay(target);
    }
    startAnimation();
    requestUpdate();
    return;
  }

  if (animFrame_ < kAnimFrames && millis() - animLastMs_ >= kAnimFrameMs) {
    animLastMs_ = millis();
    ++animFrame_;
    requestUpdate();
  }
}

void PanchangaActivity::render(RenderLock&&) {
  if (screen_ == Screen::Details) {
    renderDetails();
  } else {
    renderMain();
  }
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}

void PanchangaActivity::renderMain() {
  const auto label = [&](const kn::Label l, const int x, const int y, const bool draw = true) {
    const int i = static_cast<int>(l);
    return text(kn::kLabel[i], en::kLabel[i], x, y, Sty::Label, draw);
  };

  const Rect content = tools::drawFrame(renderer, kannadaMode() ? "" : en::kTitle);
  if (kannadaMode()) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int th = kannada::lineHeight(renderer, kannada::Style::Title);
    char title[48];
    kannada::draw(renderer, content.x, metrics.topPadding + (metrics.headerHeight - th) / 2,
                  kn::toUtf8(kn::kTitle, title, sizeof(title)), kannada::Style::Title);
  }

  if (!clockValid_) {
    const int w = label(kn::Label::ClockNotSet, 0, 0, false);
    label(kn::Label::ClockNotSet, (renderer.getScreenWidth() - w) / 2, content.y + content.height / 2 - 34);
    renderer.drawCenteredText(UI_10_FONT_ID, content.y + content.height / 2 + 10, tr(STR_TOOLS_SYNC_FROM_WORLD_CLOCK));
    tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
    return;
  }
  if (indexPending_) {
    renderer.drawCenteredText(UI_12_FONT_ID, content.y + content.height / 2 - 12, tr(STR_TOOLS_PANCH_INDEXING));
    tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
    return;
  }

  const int x0 = content.x;
  const int right = content.x + content.width;
  const int lineH = kLineH;
  const int rowH = lineH + 4;
  int y = content.y;
  char buf[32];

  // --- Header: weekday, date, samvatsara, masa, place; Moon on the right.
  const int moonR = 34;
  const int moonCx = right - moonR - 4;
  const int moonCy = y + moonR + 4;
  const int headerRight = moonCx - moonR - 10;
  {
    int x = x0;
    x += text(kn::kWeekday[day_.vara], en::kWeekday[day_.vara], x, y, Sty::Value);
    snprintf(buf, sizeof(buf), ",  %d", day_.day);
    x += drawLatin(renderer, x, y, lineH, buf) + 8;
    x += text(kn::kGregorianMonth[day_.month - 1], en::kGregorianMonth[day_.month - 1], x, y, Sty::Value);
    snprintf(buf, sizeof(buf), " %d", day_.year);
    drawLatin(renderer, x, y, lineH, buf);
  }
  y += rowH;
  {
    int x = x0;
    x += label(kn::Label::Samvatsara, x, y) + 6;
    x += text(kn::kSamvatsara[day_.samvatsara], en::kSamvatsara[day_.samvatsara], x, y, Sty::Value) + 10;
    const int masaW = label(kn::Label::Masa, 0, 0, false) + 6 +
                      text(kn::kMasa[day_.masa], en::kMasa[day_.masa], 0, 0, Sty::Value, false) +
                      (day_.adhika ? label(kn::Label::Adhika, 0, 0, false) + 4 : 0);
    if (x + 12 + masaW > headerRight) {  // wrap masa onto its own line on narrow screens
      y += rowH;
      x = x0;
    } else {
      renderer.fillRect(x, y + 6, 2, lineH - 12);
      x += 12;
    }
    x += label(kn::Label::Masa, x, y) + 6;
    if (day_.adhika) x += label(kn::Label::Adhika, x, y) + 4;
    text(kn::kMasa[day_.masa], en::kMasa[day_.masa], x, y, Sty::Value);
  }
  y += rowH;
  {
    int x = x0;
    if (defaultPlace_) {
      x += label(kn::Label::Bengaluru, x, y) + 8;
    } else {
      snprintf(buf, sizeof(buf), "%.2f%c %.2f%c", std::fabs(lat_), lat_ >= 0 ? 'N' : 'S', std::fabs(lon_),
               lon_ >= 0 ? 'E' : 'W');
      x += drawLatin(renderer, x, y, lineH, buf, false) + 8;
    }
    if (selected_ == today_) {
      // Today badge: the word drawn white on a black pill (drawn, then inverted).
      const int tw = label(kn::Label::Today, 0, 0, false);
      renderer.fillRoundedRect(x - 2, y + 2, tw + 12, lineH - 2, 6, Color::Black);
      renderer.invertRect(x - 2, y + 2, tw + 12, lineH - 2);
      label(kn::Label::Today, x + 4, kannadaMode() ? y - 2 : y);
      renderer.invertRect(x - 2, y + 2, tw + 12, lineH - 2);
    }
  }
  // Moon widget (animated sweep into the real phase after a date change).
  double shownElongation = moonElongation_;
  if (animFrame_ < kAnimFrames) {
    const double start = moonElongation_ < 180.0 ? 0.0 : 180.0;
    shownElongation = start + (moonElongation_ - start) * (animFrame_ + 1) / (kAnimFrames + 1);
  }
  drawMoon(renderer, moonCx, moonCy, moonR, shownElongation);
  snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(std::lround(panchanga::illumination(moonElongation_) * 100)));
  {
    const int lw = label(kn::Label::Light, 0, 0, false);
    const int bw = renderer.getTextWidth(SMALL_FONT_ID, buf, EpdFontFamily::BOLD);
    const int lx = moonCx - (lw + 4 + bw) / 2;
    const int ly = moonCy + moonR + 4;
    label(kn::Label::Light, lx, ly - 6);
    renderer.drawText(SMALL_FONT_ID, lx + lw + 4, ly + 4, buf, true, EpdFontFamily::BOLD);
  }
  y += rowH + 8;
  renderer.fillRect(x0, y, content.width, 2);
  y += 8;

  // --- Panchanga elements with end times.
  // Values start after the widest of the four element labels.
  int labelW = 0;
  for (const kn::Label l : {kn::Label::Tithi, kn::Label::Nakshatra, kn::Label::Yoga, kn::Label::Karana}) {
    labelW = std::max(labelW, label(l, 0, 0, false));
  }
  const int valueX = x0 + std::max(kannadaMode() ? 104 : 0, labelW + 12);
  auto element = [&](const kn::Label l, const char* k1, const char* e1, const char* k2, const char* e2,
                     const double endJd) {
    label(l, x0, y);
    int x = valueX;
    if (k1 != nullptr) x += text(k1, e1, x, y, Sty::Value) + 6;
    if (k2 != nullptr) x += text(k2, e2, x, y, Sty::Value);
    // Width of the end-time block on the right; a long name pushes it down a row.
    int endW = 0;
    if (endJd >= day_.nextSunriseJd) {
      endW = label(kn::Label::FullNight, 0, 0, false);
    } else {
      char probe[12];
      formatHm(probe, sizeof(probe), endJd, tz_, false);
      const bool afterMidnight = endJd >= panchanga::julianDay(day_.year, day_.month, day_.day, 24.0 - tz_);
      endW = renderer.getTextWidth(kTimeFont, probe, EpdFontFamily::REGULAR) + label(kn::Label::Until, 0, 0, false) +
             (afterMidnight ? label(kn::Label::Tomorrow, 0, 0, false) + 6 : 0) + 8;
    }
    if (x + 8 > right - endW) y += rowH;
    // Right side: "HH:MM ವರೆಗೆ" / "upto HH:MM", marked as next day after
    // midnight, or "whole night" when it lasts past the next sunrise (as Drik shows it).
    if (endJd >= day_.nextSunriseJd) {
      const int w = label(kn::Label::FullNight, 0, 0, false);
      label(kn::Label::FullNight, right - w, y);
      y += rowH;
      return;
    }
    char t[12];
    formatHm(t, sizeof(t), endJd, tz_, false);
    const int tw = renderer.getTextWidth(kTimeFont, t, EpdFontFamily::REGULAR);
    const double midnightJd = panchanga::julianDay(day_.year, day_.month, day_.day, 24.0 - tz_);
    const bool nextDay = endJd >= midnightJd;
    if (!kannadaMode()) {
      // "upto 03:04 next day"
      int ux = right;
      if (nextDay) ux -= label(kn::Label::Tomorrow, 0, 0, false);
      if (nextDay) label(kn::Label::Tomorrow, ux, y);
      ux -= tw + (nextDay ? 6 : 0);
      drawLatin(renderer, ux, y, lineH, t, false);
      ux -= label(kn::Label::Until, 0, 0, false) + 6;
      label(kn::Label::Until, ux, y);
    } else {
      // "ನಾಳೆ 03:04 ವರೆಗೆ"
      int ux = right - label(kn::Label::Until, 0, 0, false);
      label(kn::Label::Until, ux, y);
      ux -= tw + 4;
      drawLatin(renderer, ux, y, lineH, t, false);
      if (nextDay) label(kn::Label::Tomorrow, ux - label(kn::Label::Tomorrow, 0, 0, false) - 4, y);
    }
    y += rowH;
  };
  const int paksha = day_.tithi < 15 ? 0 : 1;
  const int tithi = tithiName(day_.tithi);
  // English uses "Krishna Pratipada" (as Drik does) so the name fits beside its end time.
  element(kn::Label::Tithi, kn::kPaksha[paksha], kannadaMode() ? en::kPaksha[paksha] : en::kPakshaShort[paksha],
          kn::kTithi[tithi], en::kTithi[tithi], day_.tithiEndJd);
  element(kn::Label::Nakshatra, kn::kNakshatra[day_.nakshatra], en::kNakshatra[day_.nakshatra], nullptr, nullptr,
          day_.nakshatraEndJd);
  element(kn::Label::Yoga, kn::kYoga[day_.yoga], en::kYoga[day_.yoga], nullptr, nullptr, day_.yogaEndJd);
  element(kn::Label::Karana, kn::kKarana[day_.karana], en::kKarana[day_.karana], nullptr, nullptr, day_.karanaEndJd);
  y += 4;
  renderer.fillRect(x0, y, content.width, 2);
  y += 8;

  // --- Sun and kaala timings.
  char a[12], b[12];
  formatHm(a, sizeof(a), day_.sunriseJd, tz_);
  formatHm(b, sizeof(b), day_.sunsetJd, tz_);
  {
    int x = x0;
    x += label(kn::Label::Sunrise, x, y) + 8;
    x += drawLatin(renderer, x, y, lineH, a) + 24;
    x += label(kn::Label::Sunset, x, y) + 8;
    drawLatin(renderer, x, y, lineH, b);
    y += rowH;
  }
  auto window = [&](const kn::Label l, const double startJd, const double endJd) {
    label(l, x0, y);
    char s[12], e[12], w[32];
    formatHm(s, sizeof(s), startJd, tz_);
    formatHm(e, sizeof(e), endJd, tz_);
    snprintf(w, sizeof(w), "%s - %s", s, e);
    drawLatin(renderer, x0 + 180, y, lineH, w);
    y += rowH;
  };
  window(kn::Label::Rahu, day_.rahuStartJd, day_.rahuStartJd + day_.kaalaLengthDays);
  window(kn::Label::Yamaganda, day_.yamagandaStartJd, day_.yamagandaStartJd + day_.kaalaLengthDays);
  window(kn::Label::Gulika, day_.gulikaStartJd, day_.gulikaStartJd + day_.kaalaLengthDays);
  window(kn::Label::Abhijit, day_.abhijitStartJd, day_.abhijitEndJd);
  y += 4;
  renderer.fillRect(x0, y, content.width, 2);
  y += 8;

  // --- Special days: festival files first, then calculated ones (two rows at most).
  {
    label(kn::Label::Special, x0, y);
    const int startX = x0 + 150;
    int x = startX;
    int rows = 1;
    bool holiday = false;
    int shown = 0;
    if (nameCount_ == 0) label(kn::Label::NoSpecial, x, y);
    for (int i = 0; i < nameCount_; ++i) {
      const DayName& d = names_[i];
      holiday = holiday || d.holiday;
      const char* k = kannada::hasKannada(d.kannada) ? d.kannada : d.english;
      const int vw = textUtf8(k, d.english, 0, 0, Sty::Value, false);
      if (i > 0) {
        if (x + 10 + vw > right) {
          if (rows == 2) break;
          ++rows;
          y += rowH;
          x = startX;
        } else {
          x += drawLatin(renderer, x, y, lineH, ",") + 8;
        }
      }
      x += textUtf8(k, d.english, x, y, Sty::Value);
      ++shown;
    }
    if (shown < nameCount_) {
      snprintf(buf, sizeof(buf), " +%d", nameCount_ - shown);
      drawLatin(renderer, std::min(x, right - 40), y, lineH, buf);
    }
    if (holiday) {
      // Public holiday pill under the names.
      y += rowH;
      char holidayKn[24];
      const char* h =
          kannadaMode() ? kn::toUtf8(kn::kHoliday, holidayKn, sizeof(holidayKn)) : tr(STR_TOOLS_PANCH_HOLIDAY);
      const int hw =
          kannadaMode() ? kannada::width(renderer, h, kannada::Style::Label) : renderer.getTextWidth(UI_10_FONT_ID, h);
      renderer.fillRoundedRect(startX, y + 3, hw + 16, lineH - 6, 8, Color::Black);
      if (kannadaMode()) {
        kannada::draw(renderer, startX + 8, y, h, kannada::Style::Label, false);
      } else {
        renderer.drawText(UI_10_FONT_ID, startX + 8, y + 5, h, false);
      }
    }
    y += rowH;
  }
  {
    int x = x0;
    x += label(kn::Label::Moon, x, y) + 8;
    const int moonTithi = static_cast<int>(moonElongation_ / 12.0) % 30;
    const int moonPaksha = moonTithi < 15 ? 0 : 1;
    x += text(kn::kPakshaShort[moonPaksha], en::kPakshaShort[moonPaksha], x, y, Sty::Label) + 6;
    x += text(kn::kTithi[tithiName(moonTithi)], en::kTithi[tithiName(moonTithi)], x, y, Sty::Value) + 20;
    x += label(kn::Label::MoonNakshatra, x, y) + 6;
    const char* nk = kn::kNakshatra[moonNakshatra_];
    const char* ne = en::kNakshatra[moonNakshatra_];
    if (x + text(nk, ne, 0, 0, Sty::Label, false) > right) {
      y += rowH;
      x = x0 + 150;
    }
    text(nk, ne, x, y, Sty::Label);
  }
  if (!english_ && !kannadaFont_) {
    renderer.drawCenteredText(SMALL_FONT_ID, content.y + content.height - renderer.getLineHeight(SMALL_FONT_ID),
                              tr(STR_TOOLS_PANCH_KN_MISSING));
  }

  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_MENU), tr(STR_TOOLS_PREV_DAY),
                   tr(STR_TOOLS_NEXT_DAY));
}

// "About this day": every festival and holiday for the selected date with its
// type, holiday status, place, notes and significance.
void PanchangaActivity::renderDetails() {
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_PANCH_ABOUT_DAY));
  const int x0 = content.x;
  const int w = content.width;
  int y = content.y;
  char buf[64];
  // Date line.
  {
    snprintf(buf, sizeof(buf), "%s, %d %s %d", en::kWeekday[day_.vara], day_.day, en::kGregorianMonth[day_.month - 1],
             day_.year);
    renderer.drawText(UI_12_FONT_ID, x0, y, buf, true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_12_FONT_ID) + 10;
  }
  if (nameCount_ == 0) {
    renderer.drawText(UI_12_FONT_ID, x0, y, en::kLabel[static_cast<int>(kn::Label::NoSpecial)]);
  }
  const int bottom = content.y + content.height - 4;
  auto paragraph = [&](const char* label, const char* value) {
    if (value == nullptr || value[0] == '\0') return;
    char line[300];
    snprintf(line, sizeof(line), "%s: %s", label, value);
    kannada::Line lines[6];
    const size_t n = kannada::wrap(renderer, line, kannada::Style::Label, w, lines, 6);
    for (size_t i = 0; i < n && i < 6 && y + 26 < bottom; ++i) {
      kannada::draw(renderer, x0, y, line + lines[i].start, kannada::Style::Label, true, lines[i].length);
      y += 26;
    }
  };
  festivals::Entry e;
  for (int i = detailsTop_; i < nameCount_ && y + 60 < bottom; ++i) {
    const DayName& d = names_[i];
    // Name in both scripts.
    if (kannadaFont_ && kannada::hasKannada(d.kannada)) {
      kannada::draw(renderer, x0, y, d.kannada, kannada::Style::Title);
      y += kannada::lineHeight(renderer, kannada::Style::Title) + 2;
    }
    renderer.drawText(UI_12_FONT_ID, x0, y, d.english, true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_12_FONT_ID) + 4;
    if (d.layer && calendar_.read(d.ref, e, true)) {
      snprintf(buf, sizeof(buf), "%s%s%s", e.type, e.type[0] ? "  |  " : "",
               e.holiday == 1 ? tr(STR_TOOLS_PANCH_HOLIDAY) : (e.holiday == 0 ? tr(STR_TOOLS_PANCH_NOT_HOLIDAY) : ""));
      renderer.drawText(UI_10_FONT_ID, x0, y, buf);
      y += renderer.getLineHeight(UI_10_FONT_ID) + 4;
      paragraph(tr(STR_TOOLS_PANCH_PLACE), e.scope);
      paragraph(tr(STR_TOOLS_PANCH_NOTE), e.note);
      paragraph(tr(STR_TOOLS_PANCH_MEANING), e.significance);
      snprintf(buf, sizeof(buf), "%s: %s", tr(STR_TOOLS_PANCH_SOURCE), calendar_.layerName(d.ref.loc >> 28));
      renderer.drawText(SMALL_FONT_ID, x0, y, buf);
      y += renderer.getLineHeight(SMALL_FONT_ID) + 4;
    } else if (!d.layer) {
      renderer.drawText(UI_10_FONT_ID, x0, y, tr(STR_TOOLS_PANCH_CALCULATED));
      y += renderer.getLineHeight(UI_10_FONT_ID) + 4;
    }
    renderer.drawLine(x0, y + 4, x0 + w, y + 4);
    y += 14;
  }
  if (nameCount_ > 1) {
    snprintf(buf, sizeof(buf), "%d / %d", detailsTop_ + 1, nameCount_);
    renderer.drawText(SMALL_FONT_ID, x0 + w - renderer.getTextWidth(SMALL_FONT_ID, buf), content.y, buf);
  }
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", nameCount_ > 1 ? tr(STR_TOOLS_PREVIOUS) : "",
                   nameCount_ > 1 ? tr(STR_TOOLS_NEXT) : "");
}
