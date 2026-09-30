#include "PanchangaCalendarActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "KannadaText.h"
#include "PanchangaActivity.h"
#include "PanchangaEnglish.h"
#include "PanchangaStrings.h"
#include "ToolsDate.h"
#include "fontIds.h"

namespace {
constexpr int kFirstYear = 1976;
constexpr int kLastYear = 2075;
constexpr const char* kEnWeekdayShort[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

int daysInMonth(const int y, const int m) {
  return m == 12 ? 31
                 : static_cast<int>(tools::daysFromCivil(y, m + 1, 1) -
                                    tools::daysFromCivil(y, static_cast<unsigned>(m), 1));
}
}  // namespace

bool PanchangaCalendarActivity::kannadaMode() const { return !english_ && kannadaFont_; }

void PanchangaCalendarActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  kannadaFont_ = kannada::acquire();
  calendar_.refresh();
  setSelected(selected_);
  transitionPending_ = true;
  requestUpdate();
}

void PanchangaCalendarActivity::onExit() {
  kannada::release();
  Activity::onExit();
}

void PanchangaCalendarActivity::setSelected(const int32_t day) {
  selected_ = std::clamp(day, PanchangaActivity::firstDay(), PanchangaActivity::lastDay());
  uint16_t y = 0;
  uint8_t m = 0, d = 0;
  tools::civilFromDays(selected_, y, m, d);
  year_ = y;
  month_ = m;
  if (year_ != loadedYear_ || month_ != loadedMonth_) loadMonth();
}

void PanchangaCalendarActivity::loadMonth() {
  loadedYear_ = year_;
  loadedMonth_ = month_;
  festivalDays_ = holidayDays_ = 0;
  itemCount_ = 0;
  const int32_t first = tools::daysFromCivil(year_, static_cast<unsigned>(month_), 1);
  const int32_t last = first + daysInMonth(year_, month_) - 1;
  festivals::Ref refs[kMaxItems];
  const size_t n = calendar_.find(first, last, refs, kMaxItems);
  festivals::Entry e;
  for (size_t i = 0; i < n; ++i) {
    if (!calendar_.read(refs[i], e, false)) continue;
    const int d = static_cast<int>(refs[i].day - first) + 1;
    festivalDays_ |= 1u << (d - 1);
    if (e.holiday == 1) holidayDays_ |= 1u << (d - 1);
    Item& it = items_[itemCount_++];
    it.day = static_cast<uint8_t>(d);
    it.holiday = e.holiday == 1;
    const bool useKn = !english_ && e.kannada[0] != '\0';
    snprintf(it.name, sizeof(it.name), "%s", useKn || e.english[0] == '\0' ? e.kannada : e.english);
  }
}

void PanchangaCalendarActivity::loop() {
  input_.poll(mappedInput);
  if (input_.backLong) {
    tools::exitToHome();
    return;
  }
  if (jump_) {
    if (input_.back) {
      if (jumpOnly_) {
        ActivityResult r;
        r.isCancelled = true;
        setResult(std::move(r));
        finish();
        return;
      }
      jump_ = false;
      requestUpdate();
      return;
    }
    if (input_.up || input_.pageBack) jumpRow_ = (jumpRow_ + 2) % 3;
    if (input_.down || input_.pageForward) jumpRow_ = (jumpRow_ + 1) % 3;
    int dir = input_.left ? -1 : input_.right ? 1 : 0;
    if (dir != 0) {
      uint16_t y = 0;
      uint8_t m = 0, d = 0;
      tools::civilFromDays(selected_, y, m, d);
      int year = y, month = m;
      if (jumpRow_ == 0) year += 10 * dir;
      if (jumpRow_ == 1) year += dir;
      if (jumpRow_ == 2) {
        month += dir;
        if (month < 1) {
          month = 12;
          --year;
        } else if (month > 12) {
          month = 1;
          ++year;
        }
      }
      year = std::clamp(year, kFirstYear, kLastYear);
      const int day = std::min<int>(d, daysInMonth(year, month));
      RenderLock lock(*this);
      setSelected(tools::daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)));
    }
    if (input_.confirm) {
      jump_ = false;
      transitionPending_ = true;
    }
    if (input_.any) requestUpdate();
    return;
  }

  if (input_.back) {
    ActivityResult r;
    r.isCancelled = true;
    setResult(std::move(r));
    finish();
    return;
  }
  if (input_.confirm) {
    setResult(IntervalResult{static_cast<uint32_t>(selected_)});
    finish();
    return;
  }
  int32_t target = selected_;
  if (input_.left) target -= 1;
  if (input_.right) target += 1;
  if (input_.up || input_.pageBack || input_.down || input_.pageForward) {
    uint16_t y = 0;
    uint8_t m = 0, d = 0;
    tools::civilFromDays(selected_, y, m, d);
    int year = y, month = m + ((input_.up || input_.pageBack) ? -1 : 1);
    if (month < 1) {
      month = 12;
      --year;
    } else if (month > 12) {
      month = 1;
      ++year;
    }
    if (year >= kFirstYear && year <= kLastYear) {
      target = tools::daysFromCivil(year, static_cast<unsigned>(month),
                                    static_cast<unsigned>(std::min<int>(d, daysInMonth(year, month))));
    }
  }
  if (target != selected_) {
    RenderLock lock(*this);
    setSelected(target);
    requestUpdate();
  }
}

int PanchangaCalendarActivity::drawName(const char* packed, const char* en, const int x, const int y, const bool bold,
                                        const bool draw) const {
  char kn[96];
  kn::toUtf8(packed, kn, sizeof(kn));
  if (kannadaMode() && kannada::hasKannada(kn)) {
    const auto style = bold ? kannada::Style::Value : kannada::Style::Label;
    return draw ? kannada::draw(renderer, x, y, kn, style) : kannada::width(renderer, kn, style);
  }
  const int font = UI_12_FONT_ID;
  const auto st = bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
  if (draw) renderer.drawText(font, x, y + 3, en, true, st);
  return renderer.getTextWidth(font, en, st);
}

void PanchangaCalendarActivity::render(RenderLock&&) {
  if (jump_) {
    renderJump();
  } else {
    renderGrid();
  }
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}

void PanchangaCalendarActivity::renderGrid() {
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_PANCH_CALENDAR));
  const int x0 = content.x;
  const int w = content.width;
  int y = content.y;
  char buf[48];

  // Month and year, centred, with arrows hinting Up/Down.
  {
    snprintf(buf, sizeof(buf), " %d", year_);
    const int mw = drawName(kn::kGregorianMonth[month_ - 1], en::kGregorianMonth[month_ - 1], 0, 0, true, false);
    const int yw = renderer.getTextWidth(UI_12_FONT_ID, buf, EpdFontFamily::BOLD);
    const int start = x0 + (w - mw - yw) / 2;
    drawName(kn::kGregorianMonth[month_ - 1], en::kGregorianMonth[month_ - 1], start, y, true);
    renderer.drawText(UI_12_FONT_ID, start + mw, y + 3, buf, true, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, x0 + 4, y + 3, "<", true, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, x0 + w - 16, y + 3, ">", true, EpdFontFamily::BOLD);
    y += 36;
  }

  // Weekday header, Sunday first as in Indian calendars.
  const int colW = w / 7;
  for (int c = 0; c < 7; ++c) {
    const int cx = x0 + c * colW;
    if (kannadaMode()) {
      char wd[32];
      kn::toUtf8(kn::kWeekdayShort[c], wd, sizeof(wd));
      const int tw = kannada::width(renderer, wd, kannada::Style::Label);
      kannada::draw(renderer, cx + (colW - tw) / 2, y - 2, wd, kannada::Style::Label);
    } else {
      const int tw = renderer.getTextWidth(UI_10_FONT_ID, kEnWeekdayShort[c]);
      renderer.drawText(UI_10_FONT_ID, cx + (colW - tw) / 2, y + 2, kEnWeekdayShort[c]);
    }
  }
  y += 28;
  renderer.drawLine(x0, y - 2, x0 + w, y - 2);

  // Day grid.
  const int32_t first = tools::daysFromCivil(year_, static_cast<unsigned>(month_), 1);
  const int lead = (tools::weekdayMon0(first) + 1) % 7;  // columns before day 1
  const int days = daysInMonth(year_, month_);
  const int rows = (lead + days + 6) / 7;
  const int cellH = 50;
  for (int d = 1; d <= days; ++d) {
    const int slot = lead + d - 1;
    const int cx = x0 + (slot % 7) * colW;
    const int cy = y + (slot / 7) * cellH;
    const int32_t day = first + d - 1;
    const bool sel = day == selected_;
    const bool fest = festivalDays_ & (1u << (d - 1));
    const bool hol = holidayDays_ & (1u << (d - 1));
    if (sel) renderer.fillRoundedRect(cx + 3, cy + 2, colW - 6, cellH - 4, 8, Color::Black);
    if (day == today_ && !sel) renderer.drawRoundedRect(cx + 3, cy + 2, colW - 6, cellH - 4, 2, 8, true);
    snprintf(buf, sizeof(buf), "%d", d);
    const auto st = fest ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    const int tw = renderer.getTextWidth(UI_12_FONT_ID, buf, st);
    renderer.drawText(UI_12_FONT_ID, cx + (colW - tw) / 2, cy + 6, buf, !sel, st);
    if (fest) {
      const int mx = cx + colW / 2;
      const int my = cy + cellH - 13;
      if (hol) {
        renderer.fillRect(mx - 4, my - 3, 8, 7, !sel);  // square: public holiday
      } else {
        renderer.fillRoundedRect(mx - 3, my - 3, 7, 7, 3, sel ? Color::White : Color::Black);  // dot: festival
      }
    }
  }
  y += rows * cellH + 4;
  renderer.drawLine(x0, y, x0 + w, y);
  y += 8;

  // This month's special days, keeping the selected day's rows in view.
  const int bottom = content.y + content.height - renderer.getLineHeight(SMALL_FONT_ID) - 6;
  const int rowH = 32;
  const int fit = std::max(1, (bottom - y) / rowH);
  uint16_t sy = 0;
  uint8_t sm = 0, sd = 0;
  tools::civilFromDays(selected_, sy, sm, sd);
  if (itemCount_ == 0) {
    renderer.drawText(UI_10_FONT_ID, x0, y + 4,
                      calendar_.hasData() ? tr(STR_TOOLS_PANCH_NO_SPECIAL_MONTH) : tr(STR_TOOLS_PANCH_NO_FEST_FILES));
  } else {
    int firstItem = 0;
    for (int i = 0; i < itemCount_; ++i) {
      if (items_[i].day >= sd) {
        firstItem = std::max(0, std::min(i, itemCount_ - fit));
        break;
      }
    }
    for (int i = firstItem; i < itemCount_ && i < firstItem + fit; ++i) {
      const Item& it = items_[i];
      const bool on = it.day == sd;
      const int ry = y + (i - firstItem) * rowH;
      if (on) renderer.fillRect(x0 - 4, ry, 6, rowH - 4);
      snprintf(buf, sizeof(buf), "%d", it.day);
      renderer.drawText(UI_12_FONT_ID, x0 + 10, ry + 3, buf, true, EpdFontFamily::BOLD);
      int nx = x0 + 52;
      if (kannadaMode() && kannada::hasKannada(it.name)) {
        nx += kannada::draw(renderer, nx, ry - 2, it.name, on ? kannada::Style::Value : kannada::Style::Label);
      } else {
        const auto st = on ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
        const std::string shown = renderer.truncatedText(UI_12_FONT_ID, it.name, x0 + w - nx - 70, st);
        renderer.drawText(UI_12_FONT_ID, nx, ry + 3, shown.c_str(), true, st);
        nx += renderer.getTextWidth(UI_12_FONT_ID, shown.c_str(), st);
      }
      if (it.holiday) {
        const char* h = tr(STR_TOOLS_PANCH_HOLIDAY);
        const int hw = renderer.getTextWidth(SMALL_FONT_ID, h);
        renderer.drawRoundedRect(x0 + w - hw - 12, ry + 3, hw + 10, rowH - 10, 1, 6, true);
        renderer.drawText(SMALL_FONT_ID, x0 + w - hw - 7, ry + 6, h);
      }
    }
  }
  renderer.drawCenteredText(SMALL_FONT_ID, content.y + content.height - renderer.getLineHeight(SMALL_FONT_ID),
                            tr(STR_TOOLS_PANCH_CAL_HINT));
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_OPEN), tr(STR_TOOLS_PREV_DAY), tr(STR_TOOLS_NEXT_DAY));
}

void PanchangaCalendarActivity::renderJump() {
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_PANCH_JUMP));
  const int x0 = content.x;
  const int w = content.width;
  int y = content.y + 10;
  char buf[32];
  uint16_t sy = 0;
  uint8_t sm = 0, sd = 0;
  tools::civilFromDays(selected_, sy, sm, sd);

  const char* labels[3] = {tr(STR_TOOLS_PANCH_DECADE), tr(STR_TOOLS_PANCH_YEAR), tr(STR_TOOLS_PANCH_MONTH)};
  const int rowH = 92;
  for (int row = 0; row < 3; ++row) {
    const bool on = row == jumpRow_;
    const int ry = y + row * (rowH + 12);
    if (on) {
      renderer.fillRoundedRect(x0, ry, w, rowH, 12, Color::Black);
    } else {
      renderer.drawRoundedRect(x0, ry, w, rowH, 2, 12, true);
    }
    renderer.drawText(UI_10_FONT_ID, x0 + 16, ry + 8, labels[row], !on);
    int vw = 0;
    const int vy = ry + 38;
    if (row == 2) {
      if (kannadaMode()) {
        char month[48];
        kn::toUtf8(kn::kGregorianMonth[sm - 1], month, sizeof(month));
        vw = kannada::width(renderer, month, kannada::Style::Title);
        kannada::draw(renderer, x0 + (w - vw) / 2, vy - 8, month, kannada::Style::Title, !on);
      } else {
        vw = renderer.getTextWidth(UI_12_FONT_ID, en::kGregorianMonth[sm - 1], EpdFontFamily::BOLD);
        renderer.drawText(UI_12_FONT_ID, x0 + (w - vw) / 2, vy, en::kGregorianMonth[sm - 1], !on, EpdFontFamily::BOLD);
      }
    } else {
      if (row == 0) {
        snprintf(buf, sizeof(buf), "%ds", sy - sy % 10);
      } else {
        snprintf(buf, sizeof(buf), "%d", sy);
      }
      vw = renderer.getTextWidth(UI_12_FONT_ID, buf, EpdFontFamily::BOLD);
      renderer.drawText(UI_12_FONT_ID, x0 + (w - vw) / 2, vy, buf, !on, EpdFontFamily::BOLD);
    }
    // Arrows for Left/Right.
    renderer.drawText(UI_12_FONT_ID, x0 + 22, vy, "<", !on, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, x0 + w - 34, vy, ">", !on, EpdFontFamily::BOLD);
  }
  y += 3 * (rowH + 12) + 10;
  renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_TOOLS_PANCH_JUMP_HINT));
  y += renderer.getLineHeight(UI_10_FONT_ID) + 4;
  renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_TOOLS_PANCH_RANGE));
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_PANCH_SHOW), tr(STR_TOOLS_PREVIOUS),
                   tr(STR_TOOLS_NEXT));
}
