#include "PanchangaActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "PanchangaKannada.h"
#include "components/UITheme.h"
#include "fontIds.h"

constexpr char PanchangaActivity::kConfigPath[];

namespace {
constexpr int kTimeFont = UI_12_FONT_ID;

// Blits a pre-shaped Kannada bitmap with its line box top at y; returns width.
int drawKn(const GfxRenderer& r, const kn::KnText& t, const int x, const int y) {
  const int stride = (t.w + 7) / 8;
  const uint8_t* bits = kn::kBits + t.offset;
  for (int row = 0; row < t.h; ++row) {
    const uint8_t* line = bits + row * stride;
    for (int col = 0; col < t.w; ++col) {
      if (line[col >> 3] & (0x80 >> (col & 7))) r.drawPixel(x + col, y + row, true);
    }
  }
  return t.w;
}

const kn::KnText& label(const kn::Label l) { return kn::kLabel[static_cast<int>(l)]; }

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

bool PanchangaActivity::writeDefaults(FsFile& out, void*) {
  return tools::writeText(out,
                          "# PocketDeck-OS Panchanga location\n"
                          "# lat/lon in decimal degrees (north/east positive), tz in hours from UTC.\n"
                          "lat=12.9716\n"
                          "lon=77.5946\n"
                          "tz=5.5\n"
                          "# animate=0 turns off the Moon phase sweep\n"
                          "animate=1\n");
}

void PanchangaActivity::loadConfig() {
  tools::recoverFromBackup(kConfigPath);
  if (!Storage.exists(kConfigPath)) tools::writeFileAtomic(kConfigPath, &PanchangaActivity::writeDefaults, nullptr);
  FsFile f;
  if (!Storage.openFileForRead("PANCH", kConfigPath, f)) return;
  char line[64];
  while (tools::readLine(f, line, sizeof(line)) >= 0) {
    if (line[0] == '#') continue;
    char* eq = strchr(line, '=');
    if (eq == nullptr) continue;
    *eq = '\0';
    const double v = atof(eq + 1);
    if (strcmp(line, "lat") == 0 && v >= -66 && v <= 66) lat_ = v;
    if (strcmp(line, "lon") == 0 && v >= -180 && v <= 180) lon_ = v;
    if (strcmp(line, "tz") == 0 && v >= -12 && v <= 14) tz_ = v;
    if (strcmp(line, "animate") == 0) animate_ = v != 0;
  }
  f.close();
  defaultPlace_ = std::fabs(lat_ - 12.9716) < 0.01 && std::fabs(lon_ - 77.5946) < 0.01;
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
  const Rect content = tools::drawFrame(renderer, "");
  const auto& metrics = UITheme::getInstance().getMetrics();
  const kn::KnText& title = kn::kTitle[0];
  drawKn(renderer, title, content.x, metrics.topPadding + (metrics.headerHeight - title.h) / 2);

  if (!clockValid_) {
    const kn::KnText& msg = label(kn::Label::ClockNotSet);
    drawKn(renderer, msg, (renderer.getScreenWidth() - msg.w) / 2, content.y + content.height / 2 - msg.h);
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
    x += drawKn(renderer, kn::kWeekday[day_.vara], x, y);
    snprintf(buf, sizeof(buf), ",  %d", day_.day);
    x += drawLatin(renderer, x, y, lineH, buf) + 8;
    x += drawKn(renderer, kn::kGregorianMonth[day_.month - 1], x, y);
    snprintf(buf, sizeof(buf), " %d", day_.year);
    drawLatin(renderer, x, y, lineH, buf);
  }
  y += rowH;
  {
    int x = x0;
    x += drawKn(renderer, label(kn::Label::Samvatsara), x, y + 2) + 6;
    x += drawKn(renderer, kn::kSamvatsara[day_.samvatsara], x, y) + 10;
    renderer.fillRect(x, y + 6, 2, lineH - 12);
    x += 12;
    if (x + 120 > headerRight) {  // wrap masa onto its own line on narrow screens
      y += rowH;
      x = x0;
    }
    x += drawKn(renderer, label(kn::Label::Masa), x, y + 2) + 6;
    if (day_.adhika) x += drawKn(renderer, label(kn::Label::Adhika), x, y + 2) + 4;
    drawKn(renderer, kn::kMasa[day_.masa], x, y);
  }
  y += rowH;
  {
    int x = x0;
    if (defaultPlace_) {
      x += drawKn(renderer, label(kn::Label::Bengaluru), x, y) + 8;
    } else {
      snprintf(buf, sizeof(buf), "%.2f%c %.2f%c", std::fabs(lat_), lat_ >= 0 ? 'N' : 'S', std::fabs(lon_),
               lon_ >= 0 ? 'E' : 'W');
      x += drawLatin(renderer, x, y, lineH, buf, false) + 8;
    }
    if (selected_ == today_) {
      const kn::KnText& t = label(kn::Label::Today);
      renderer.fillRoundedRect(x - 2, y + 2, t.w + 12, lineH - 2, 6, Color::Black);
      // Today badge: draw the word in white by inverting its bitmap.
      renderer.invertRect(x - 2, y + 2, t.w + 12, lineH - 2);
      drawKn(renderer, t, x + 4, y);
      renderer.invertRect(x - 2, y + 2, t.w + 12, lineH - 2);
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
    const kn::KnText& lightLabel = label(kn::Label::Light);
    const int bw = renderer.getTextWidth(SMALL_FONT_ID, buf, EpdFontFamily::BOLD);
    const int total = lightLabel.w + 4 + bw;
    const int lx = moonCx - total / 2;
    const int ly = moonCy + moonR + 4;
    drawKn(renderer, lightLabel, lx, ly - 4);
    renderer.drawText(SMALL_FONT_ID, lx + lightLabel.w + 4, ly + 4, buf, true, EpdFontFamily::BOLD);
  }
  y += rowH + 8;
  renderer.fillRect(x0, y, content.width, 2);
  y += 8;

  // --- Panchanga elements with end times.
  const int valueX = x0 + 104;
  auto element = [&](const kn::Label l, const kn::KnText* v1, const kn::KnText* v2, const double endJd) {
    drawKn(renderer, label(l), x0, y + 2);
    int x = valueX;
    if (v1 != nullptr) x += drawKn(renderer, *v1, x, y) + 6;
    if (v2 != nullptr) drawKn(renderer, *v2, x, y);
    // Right side: "HH:MM ವರೆಗೆ", "ನಾಳೆ HH:MM ವರೆಗೆ" after midnight, or
    // "ಪೂರ್ಣ ರಾತ್ರಿ" when it lasts past the next sunrise (as Drik shows it).
    if (endJd >= day_.nextSunriseJd) {
      const kn::KnText& full = label(kn::Label::FullNight);
      drawKn(renderer, full, right - full.w, y + 2);
    } else {
      const kn::KnText& until = label(kn::Label::Until);
      char t[12];
      formatHm(t, sizeof(t), endJd, tz_, false);
      const int tw = renderer.getTextWidth(kTimeFont, t, EpdFontFamily::REGULAR);
      int ux = right - until.w;
      drawKn(renderer, until, ux, y + 2);
      ux -= tw + 4;
      drawLatin(renderer, ux, y, lineH, t, false);
      const double midnightJd = panchanga::julianDay(day_.year, day_.month, day_.day, 24.0 - tz_);
      if (endJd >= midnightJd) {
        const kn::KnText& tomorrow = label(kn::Label::Tomorrow);
        drawKn(renderer, tomorrow, ux - tomorrow.w - 4, y + 2);
      }
    }
    y += rowH;
  };
  const int paksha = day_.tithi < 15 ? 0 : 1;
  element(kn::Label::Tithi, &kn::kPaksha[paksha], &kn::kTithi[tithiName(day_.tithi)], day_.tithiEndJd);
  element(kn::Label::Nakshatra, &kn::kNakshatra[day_.nakshatra], nullptr, day_.nakshatraEndJd);
  element(kn::Label::Yoga, &kn::kYoga[day_.yoga], nullptr, day_.yogaEndJd);
  element(kn::Label::Karana, &kn::kKarana[day_.karana], nullptr, day_.karanaEndJd);
  y += 4;
  renderer.fillRect(x0, y, content.width, 2);
  y += 8;

  // --- Sun and kaala timings.
  char a[12], b[12];
  formatHm(a, sizeof(a), day_.sunriseJd, tz_);
  formatHm(b, sizeof(b), day_.sunsetJd, tz_);
  {
    int x = x0;
    x += drawKn(renderer, label(kn::Label::Sunrise), x, y + 2) + 8;
    x += drawLatin(renderer, x, y, lineH, a) + 24;
    x += drawKn(renderer, label(kn::Label::Sunset), x, y + 2) + 8;
    drawLatin(renderer, x, y, lineH, b);
    y += rowH;
  }
  auto window = [&](const kn::Label l, const double startJd, const double endJd) {
    drawKn(renderer, label(l), x0, y + 2);
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
    drawKn(renderer, label(kn::Label::Special), x0, y + 2);
    panchanga::Special sp[2];
    const int count = panchanga::specials(day_, sp, 2);
    int x = x0 + 150;
    if (count == 0) {
      drawKn(renderer, label(kn::Label::NoSpecial), x, y + 2);
    }
    for (int i = 0; i < count; ++i) {
      const kn::KnText& v = kn::kSpecial[static_cast<int>(sp[i])];
      if (i > 0) {
        if (x + 10 + v.w > right) {
          y += rowH;
          x = x0 + 150;
        } else {
          x += drawLatin(renderer, x, y, lineH, ",") + 8;
        }
      }
      x += drawKn(renderer, v, x, y);
    }
    y += rowH;
  }
  {
    int x = x0;
    x += drawKn(renderer, label(kn::Label::Moon), x, y + 2) + 8;
    const int moonTithi = static_cast<int>(moonElongation_ / 12.0) % 30;
    x += drawKn(renderer, kn::kPakshaShort[moonTithi < 15 ? 0 : 1], x, y + 2) + 6;
    x += drawKn(renderer, kn::kTithi[tithiName(moonTithi)], x, y) + 20;
    x += drawKn(renderer, label(kn::Label::MoonNakshatra), x, y + 2) + 6;
    if (x + kn::kNakshatraLabel[moonNakshatra_].w <= right) {
      drawKn(renderer, kn::kNakshatraLabel[moonNakshatra_], x, y + 2);
    } else {
      y += rowH;
      drawKn(renderer, kn::kNakshatraLabel[moonNakshatra_], x0 + 150, y + 2);
    }
  }

  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_TODAY), tr(STR_TOOLS_PREV_DAY),
                   tr(STR_TOOLS_NEXT_DAY));
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
