#include "PanchangaActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "PanchangaEnglish.h"
#include "PanchangaKannada.h"
#include "activities/util/OptionSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

constexpr char PanchangaActivity::kConfigPath[];

namespace {
constexpr int kTimeFont = UI_12_FONT_ID;

// Blits a pre-shaped Kannada string with its line box top at y; returns width.
// The pixels are nibble run-length encoded (scripts/panchanga/kn_rle.py), so
// they are decoded straight from flash while drawing: no buffer is needed.
int drawKn(const GfxRenderer& r, const kn::KnText& t, const int x, const int y) {
  const uint8_t* p = kn::kBits + t.offset;
  const int total = t.w * t.h;
  int pos = 0;
  int run = 0;
  bool ink = false;
  bool high = true;
  while (pos < total) {
    const uint8_t nib = high ? (*p >> 4) : (*p++ & 0x0F);
    high = !high;
    run += nib;
    if (nib == 15) continue;
    if (ink) {
      for (int i = pos; i < pos + run && i < total; ++i) r.drawPixel(x + i % t.w, y + i / t.w, true);
    }
    pos += run;
    run = 0;
    ink = !ink;
  }
  return t.w;
}

// Every English table mirrors its Kannada twin entry for entry.
static_assert(sizeof(en::kWeekday) / sizeof(en::kWeekday[0]) == sizeof(kn::kWeekday) / sizeof(kn::kWeekday[0]));
static_assert(sizeof(en::kGregorianMonth) / sizeof(en::kGregorianMonth[0]) ==
              sizeof(kn::kGregorianMonth) / sizeof(kn::kGregorianMonth[0]));
static_assert(sizeof(en::kMasa) / sizeof(en::kMasa[0]) == sizeof(kn::kMasa) / sizeof(kn::kMasa[0]));
static_assert(sizeof(en::kSamvatsara) / sizeof(en::kSamvatsara[0]) ==
              sizeof(kn::kSamvatsara) / sizeof(kn::kSamvatsara[0]));
static_assert(sizeof(en::kTithi) / sizeof(en::kTithi[0]) == sizeof(kn::kTithi) / sizeof(kn::kTithi[0]));
static_assert(sizeof(en::kNakshatra) / sizeof(en::kNakshatra[0]) == sizeof(kn::kNakshatra) / sizeof(kn::kNakshatra[0]));
static_assert(sizeof(en::kYoga) / sizeof(en::kYoga[0]) == sizeof(kn::kYoga) / sizeof(kn::kYoga[0]));
static_assert(sizeof(en::kKarana) / sizeof(en::kKarana[0]) == sizeof(kn::kKarana) / sizeof(kn::kKarana[0]));
static_assert(sizeof(en::kSpecial) / sizeof(en::kSpecial[0]) == sizeof(kn::kSpecial) / sizeof(kn::kSpecial[0]));
static_assert(sizeof(en::kLabel) / sizeof(en::kLabel[0]) == sizeof(kn::kLabel) / sizeof(kn::kLabel[0]));

// Latin text (digits, punctuation) vertically centred on a Kannada line box.
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

bool PanchangaActivity::writeConfig(FsFile& out, void* ctx) {
  const auto* self = static_cast<const PanchangaActivity*>(ctx);
  char body[320];
  snprintf(body, sizeof(body),
           "# PocketDeck-OS Panchanga settings\n"
           "# lat/lon in decimal degrees (north/east positive), tz in hours from UTC.\n"
           "lat=%.4f\n"
           "lon=%.4f\n"
           "tz=%g\n"
           "# animate=0 turns off the Moon phase sweep\n"
           "animate=%d\n"
           "# lang=kn (Kannada) or lang=en (English); hold Confirm in the tool to switch\n"
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

void PanchangaActivity::chooseLanguage() {
  std::vector<std::string> options;
  options.reserve(2);
  options.emplace_back(tr(STR_TOOLS_LANG_KANNADA));
  options.emplace_back(tr(STR_TOOLS_LANG_ENGLISH));
  auto picker = makeUniqueNoThrow<OptionSelectionActivity>(renderer, mappedInput, "PanchangaLanguage",
                                                           StrId::STR_TOOLS_PANCHANGA_LANGUAGE, std::move(options),
                                                           english_ ? 1 : 0);
  if (!picker) {
    LOG_ERR("PANCH", "OOM creating language picker");
    return;
  }
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    if (!result.isCancelled) {
      const auto* selection = std::get_if<OptionSelectionResult>(&result.data);
      if (selection != nullptr && (selection->index == 1) != english_) {
        english_ = selection->index == 1;
        if (!saveConfig()) LOG_ERR("PANCH", "Could not save %s", kConfigPath);
      }
    }
    requestUpdate();
  });
}

int PanchangaActivity::text(const kn::KnText& k, const char* e, const int x, const int y, const Sty sty,
                            const bool draw) const {
  if (!english_) {
    // Kannada labels use a smaller face than values; nudge them onto the same baseline.
    if (draw) drawKn(renderer, k, x, y + (sty == Sty::Label ? 2 : 0));
    return k.w;
  }
  const int font = sty == Sty::Label ? UI_10_FONT_ID : UI_12_FONT_ID;
  const auto style = sty == Sty::Label ? EpdFontFamily::REGULAR : EpdFontFamily::BOLD;
  if (draw) {
    const int lineH = kn::kWeekday[0].h;
    renderer.drawText(font, x, y + (lineH - renderer.getLineHeight(font)) / 2 + 1, e, true, style);
  }
  return renderer.getTextWidth(font, e, style);
}

void PanchangaActivity::selectDay(const int32_t day) {
  selected_ = std::clamp(day, today_ - kRangeDays, today_ + kRangeDays);
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
  tools::DateTime local;
  clockValid_ = tools::getTimeAtOffset(static_cast<int16_t>(tz_ * 60.0), local);
  today_ = clockValid_ ? tools::daysOf(local) : 0;
  if (clockValid_) {
    selectDay(today_);
    startAnimation();
  }
  transitionPending_ = true;
  requestUpdate();
}

void PanchangaActivity::loop() {
  input_.poll(mappedInput);
  if (input_.backLong) {
    tools::exitToHome();
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
  if (input_.confirm) target = today_;
  if (target != selected_ || input_.confirm) {
    {
      RenderLock lock(*this);  // day_ is read by render()
      selectDay(target);
    }
    startAnimation();
    requestUpdate();
    return;
  }
  if (input_.confirmLong) {
    chooseLanguage();
    return;
  }
  if (animFrame_ < kAnimFrames && millis() - animLastMs_ >= kAnimFrameMs) {
    animLastMs_ = millis();
    ++animFrame_;
    requestUpdate();
  }
}

void PanchangaActivity::render(RenderLock&&) {
  // Both languages share one layout; L()/V() pick the Kannada bitmap and the
  // English string for the same table entry.
  const auto L = [](const kn::Label l) -> std::pair<const kn::KnText&, const char*> {
    return {kn::kLabel[static_cast<int>(l)], en::kLabel[static_cast<int>(l)]};
  };
  const auto label = [&](const kn::Label l, const int x, const int y, const bool draw = true) {
    const auto p = L(l);
    return text(p.first, p.second, x, y, Sty::Label, draw);
  };

  const Rect content = tools::drawFrame(renderer, english_ ? en::kTitle : "");
  if (!english_) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const kn::KnText& title = kn::kTitle[0];
    drawKn(renderer, title, content.x, metrics.topPadding + (metrics.headerHeight - title.h) / 2);
  }

  if (!clockValid_) {
    const int w = label(kn::Label::ClockNotSet, 0, 0, false);
    label(kn::Label::ClockNotSet, (renderer.getScreenWidth() - w) / 2, content.y + content.height / 2 - 34);
    renderer.drawCenteredText(UI_10_FONT_ID, content.y + content.height / 2 + 10, tr(STR_TOOLS_SYNC_FROM_WORLD_CLOCK));
    tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
    renderer.displayBuffer(tools::transitionRefresh());
    transitionPending_ = false;
    return;
  }

  const int x0 = content.x;
  const int right = content.x + content.width;
  const int lineH = kn::kWeekday[0].h;
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
      label(kn::Label::Today, x + 4, english_ ? y : y - 2);
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
    label(kn::Label::Light, lx, english_ ? ly - 6 : ly - 6);
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
  const int valueX = x0 + std::max(english_ ? 0 : 104, labelW + 12);
  auto element = [&](const kn::Label l, const kn::KnText* k1, const char* e1, const kn::KnText* k2, const char* e2,
                     const double endJd) {
    label(l, x0, y);
    int x = valueX;
    if (k1 != nullptr) x += text(*k1, e1, x, y, Sty::Value) + 6;
    if (k2 != nullptr) x += text(*k2, e2, x, y, Sty::Value);
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
    if (english_) {
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
  element(kn::Label::Tithi, &kn::kPaksha[paksha], english_ ? en::kPakshaShort[paksha] : en::kPaksha[paksha],
          &kn::kTithi[tithi], en::kTithi[tithi], day_.tithiEndJd);
  element(kn::Label::Nakshatra, &kn::kNakshatra[day_.nakshatra], en::kNakshatra[day_.nakshatra], nullptr, nullptr,
          day_.nakshatraEndJd);
  element(kn::Label::Yoga, &kn::kYoga[day_.yoga], en::kYoga[day_.yoga], nullptr, nullptr, day_.yogaEndJd);
  element(kn::Label::Karana, &kn::kKarana[day_.karana], en::kKarana[day_.karana], nullptr, nullptr, day_.karanaEndJd);
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

  // --- Special days (up to two, festivals first) and Moon details.
  {
    label(kn::Label::Special, x0, y);
    panchanga::Special sp[2];
    const int count = panchanga::specials(day_, sp, 2);
    int x = x0 + 150;
    if (count == 0) label(kn::Label::NoSpecial, x, y);
    for (int i = 0; i < count; ++i) {
      const int idx = static_cast<int>(sp[i]);
      const int vw = text(kn::kSpecial[idx], en::kSpecial[idx], 0, 0, Sty::Value, false);
      if (i > 0) {
        if (x + 10 + vw > right) {
          y += rowH;
          x = x0 + 150;
        } else {
          x += drawLatin(renderer, x, y, lineH, ",") + 8;
        }
      }
      x += text(kn::kSpecial[idx], en::kSpecial[idx], x, y, Sty::Value);
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
    const kn::KnText& nk = kn::kNakshatraLabel[moonNakshatra_];
    const char* ne = en::kNakshatra[moonNakshatra_];
    if (x + text(nk, ne, 0, 0, Sty::Label, false) > right) {
      y += rowH;
      x = x0 + 150;
    }
    text(nk, ne, x, y, Sty::Label);
  }
  y += rowH + 6;
  if (y + renderer.getLineHeight(SMALL_FONT_ID) < content.y + content.height) {
    renderer.drawCenteredText(SMALL_FONT_ID, y, tr(STR_TOOLS_PANCHANGA_HOLD_HINT));
  }

  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_TODAY), tr(STR_TOOLS_PREV_DAY),
                   tr(STR_TOOLS_NEXT_DAY));
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
