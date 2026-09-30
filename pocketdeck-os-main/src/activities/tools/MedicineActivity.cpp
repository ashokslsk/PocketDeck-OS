#include "MedicineActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "ToolsLog.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "activities/util/OptionSelectionActivity.h"
#include "fontIds.h"

namespace {
constexpr uint16_t kDayChoices[] = {1, 2, 3, 4, 5, 6, 7, 10, 14, 21, 30, 45, 60, 90};

const char* statusLabel(const meds::Status s) {
  switch (s) {
    case meds::Status::Upcoming:
      return tr(STR_TOOLS_MED_UPCOMING);
    case meds::Status::Active:
      return tr(STR_TOOLS_MED_ACTIVE);
    case meds::Status::Completed:
      return tr(STR_TOOLS_MED_COMPLETED);
    default:
      return tr(STR_TOOLS_MED_STOPPED);
  }
}
}  // namespace

void MedicineActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  tlog::Stamp now;
  clockValid_ = tlog::now(now);
  today_ = now.day;
  nowMinute_ = now.minute;
  reload();
  transitionPending_ = true;
  requestUpdate();
}

void MedicineActivity::reload() {
  count_ = meds::load(courses_);
  selected_ = std::clamp(selected_, 0, std::max(0, count_ - 1));
  slot_ = 0;
}

bool MedicineActivity::todayIndex(const meds::Course& c, int& index) const {
  index = today_ - c.start;
  return meds::statusOf(c, today_) == meds::Status::Active && index >= 0 && index < c.days;
}

void MedicineActivity::openMenu() {
  enum Action : uint8_t { Add, Details, Stop, Delete };
  std::vector<std::string> options;
  std::vector<uint8_t> actions;
  options.reserve(4);
  actions.reserve(4);
  auto add = [&](const Action a, std::string label) {
    options.push_back(std::move(label));
    actions.push_back(a);
  };
  if (count_ > 0) {
    const meds::Course& c = courses_[selected_];
    add(Details, std::string(tr(STR_TOOLS_MED_DETAILS)) + ": " + c.name);
    if (meds::statusOf(c, today_) == meds::Status::Active) add(Stop, std::string(tr(STR_TOOLS_MED_STOP)) + ": " + c.name);
  }
  if (count_ < meds::kMaxCourses) add(Add, tr(STR_TOOLS_MED_ADD));
  if (count_ > 0) add(Delete, std::string(tr(STR_TOOLS_MED_DELETE)) + ": " + courses_[selected_].name);
  auto picker = makeUniqueNoThrow<OptionSelectionActivity>(renderer, mappedInput, "MedicineMenu",
                                                           StrId::STR_TOOLS_MEDICINE, std::move(options), 0);
  if (!picker) return;
  startActivityForResult(std::move(picker), [this, actions](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    const auto* sel = std::get_if<OptionSelectionResult>(&result.data);
    if (result.isCancelled || sel == nullptr || sel->index >= actions.size()) {
      requestUpdate();
      return;
    }
    switch (actions[sel->index]) {
      case Add:
        addCourseName();
        break;
      case Details:
        openDetails();
        break;
      case Stop:
        stopCourse();
        break;
      default:
        deleteCourse();
        break;
    }
  });
}

void MedicineActivity::addCourseName() {
  if (count_ >= meds::kMaxCourses) return;
  draft_ = meds::Course{};
  auto kb =
      makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_TOOLS_MED_NAME), "", meds::kNameCap - 1);
  if (!kb) return;
  startActivityForResult(std::move(kb), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    const auto* text = std::get_if<KeyboardResult>(&result.data);
    if (result.isCancelled || text == nullptr || text->text.empty() || text->text.find('|') != std::string::npos) {
      requestUpdate();
      return;
    }
    snprintf(draft_.name, sizeof(draft_.name), "%s", text->text.c_str());
    addCourseDoses();
  });
}

void MedicineActivity::addCourseDoses() {
  std::vector<std::string> options;
  options.reserve(meds::kMaxDoses);
  for (int n = 1; n <= meds::kMaxDoses; ++n) {
    std::string label = std::to_string(n) + " " + tr(STR_TOOLS_MED_PER_DAY) + " (";
    for (int s = 0; s < n; ++s) {
      if (s > 0) label += ", ";
      label += meds::slotName(n, s);
    }
    options.emplace_back(label + ")");
  }
  auto picker = makeUniqueNoThrow<OptionSelectionActivity>(renderer, mappedInput, "MedicineDoses",
                                                           StrId::STR_TOOLS_MED_DOSES, std::move(options), 2);
  if (!picker) return;
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    const auto* sel = std::get_if<OptionSelectionResult>(&result.data);
    if (result.isCancelled || sel == nullptr) {
      requestUpdate();
      return;
    }
    draft_.doses = static_cast<uint8_t>(sel->index + 1);
    meds::defaultSlots(draft_.doses, draft_.slotMinute);
    addCourseFood();
  });
}

void MedicineActivity::addCourseFood() {
  std::vector<std::string> options;
  options.reserve(meds::FoodCount);
  options.emplace_back(tr(STR_TOOLS_MED_FOOD_AFTER));
  options.emplace_back(tr(STR_TOOLS_MED_FOOD_BEFORE));
  options.emplace_back(tr(STR_TOOLS_MED_FOOD_WITH));
  options.emplace_back(tr(STR_TOOLS_MED_FOOD_ANY));
  auto picker = makeUniqueNoThrow<OptionSelectionActivity>(renderer, mappedInput, "MedicineFood",
                                                           StrId::STR_TOOLS_MED_FOOD, std::move(options), 0);
  if (!picker) return;
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    const auto* sel = std::get_if<OptionSelectionResult>(&result.data);
    if (result.isCancelled || sel == nullptr) {
      requestUpdate();
      return;
    }
    static constexpr uint8_t kRowFood[] = {meds::FoodAfter, meds::FoodBefore, meds::FoodWith, meds::FoodAny};
    draft_.food = kRowFood[sel->index < 4 ? sel->index : 3];
    addCourseDays();
  });
}

void MedicineActivity::addCourseDays() {
  std::vector<std::string> options;
  options.reserve(sizeof(kDayChoices) / sizeof(kDayChoices[0]));
  for (const uint16_t d : kDayChoices) {
    options.emplace_back(std::to_string(d) + " " + (d == 1 ? tr(STR_STATS_DAY) : tr(STR_STATS_DAYS)));
  }
  auto picker = makeUniqueNoThrow<OptionSelectionActivity>(renderer, mappedInput, "MedicineDays",
                                                           StrId::STR_TOOLS_MED_LENGTH, std::move(options), 3);
  if (!picker) return;
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    const auto* sel = std::get_if<OptionSelectionResult>(&result.data);
    if (result.isCancelled || sel == nullptr || sel->index >= sizeof(kDayChoices) / sizeof(kDayChoices[0])) {
      requestUpdate();
      return;
    }
    draft_.days = kDayChoices[sel->index];
    addCourseStart();
  });
}

void MedicineActivity::addCourseStart() {
  std::vector<std::string> options;
  options.reserve(2);
  options.emplace_back(tr(STR_TOOLS_TODAY));
  options.emplace_back(tr(STR_TOOLS_TOMORROW));
  auto picker = makeUniqueNoThrow<OptionSelectionActivity>(renderer, mappedInput, "MedicineStart",
                                                           StrId::STR_TOOLS_MED_STARTS, std::move(options), 0);
  if (!picker) return;
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    const auto* sel = std::get_if<OptionSelectionResult>(&result.data);
    if (result.isCancelled || sel == nullptr) {
      transitionPending_ = true;
      requestUpdate();
      return;
    }
    draft_.start = today_ + (sel->index == 1 ? 1 : 0);
    confirmCourse();
  });
}

void MedicineActivity::confirmCourse() {
  // "Starts Mon 28 Sep, ends Fri 2 Oct. 3 a day at 08:00, 13:00, 19:00,
  //  after food. 12 doses in all."
  char start[16], end[16];
  tools::formatShortDate(start, sizeof(start), draft_.start);
  tools::formatShortDate(end, sizeof(end), draft_.lastDay());
  std::string body;
  body.reserve(200);
  body += tr(STR_TOOLS_MED_STARTS);
  body += " ";
  body += tools::weekdayShortName(tools::weekdayMon0(draft_.start));
  body += " ";
  body += start;
  body += ", ";
  body += tr(STR_TOOLS_MED_ENDS_LOWER);
  body += " ";
  body += tools::weekdayShortName(tools::weekdayMon0(draft_.lastDay()));
  body += " ";
  body += end;
  body += ". ";
  body += std::to_string(draft_.doses) + " " + tr(STR_TOOLS_MED_PER_DAY) + ":";
  for (int d = 0; d < draft_.doses; ++d) {
    char hm[8];
    charts::formatMinute(hm, sizeof(hm), draft_.slotMinute[d]);
    body += d == 0 ? " " : ", ";
    body += meds::slotName(draft_.doses, d);
    body += " ";
    body += hm;
  }
  if (draft_.food != meds::FoodAny) {
    body += ", ";
    body += meds::foodLabel(draft_.food);
  }
  body += ". " + std::to_string(draft_.doses * draft_.days) + " " + tr(STR_TOOLS_MED_DOSES_IN_ALL);
  const std::string heading = std::string(tr(STR_TOOLS_MED_ADD)) + ": " + draft_.name + ".";
  auto confirm = makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, heading, body);
  if (!confirm) return;
  startActivityForResult(std::move(confirm), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    if (!result.isCancelled && count_ < meds::kMaxCourses) {
      meds::makeId(draft_.id, draft_.name, today_, nowMinute_);
      RenderLock lock(*this);
      courses_[count_] = draft_;
      if (meds::save(courses_, count_ + 1)) {
        reload();
        selected_ = count_ - 1;
      }
    }
    requestUpdate();
  });
}

void MedicineActivity::stopCourse() {
  char heading[64];
  snprintf(heading, sizeof(heading), "%s: %s", tr(STR_TOOLS_MED_STOP), courses_[selected_].name);
  auto confirm = makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, heading, tr(STR_TOOLS_MED_STOP_BODY));
  if (!confirm) return;
  startActivityForResult(std::move(confirm), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    if (!result.isCancelled) {
      RenderLock lock(*this);
      courses_[selected_].stopped = today_;
      meds::save(courses_, count_);
    }
    requestUpdate();
  });
}

void MedicineActivity::deleteCourse() {
  char heading[64];
  snprintf(heading, sizeof(heading), "%s: %s", tr(STR_TOOLS_MED_DELETE), courses_[selected_].name);
  auto confirm = makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, heading, tr(STR_TOOLS_MED_DELETE_BODY));
  if (!confirm) return;
  startActivityForResult(std::move(confirm), [this](const ActivityResult& result) {
    input_.reset(mappedInput);
    transitionPending_ = true;
    if (!result.isCancelled) {
      RenderLock lock(*this);
      for (int i = selected_; i + 1 < count_; ++i) courses_[i] = courses_[i + 1];
      if (meds::save(courses_, count_ - 1)) reload();
    }
    requestUpdate();
  });
}

void MedicineActivity::openDetails() {
  if (count_ == 0) return;
  {
    RenderLock lock(*this);
    computeDetails();
    screen_ = Screen::Details;
  }
  transitionPending_ = true;
  requestUpdate();
}

void MedicineActivity::computeDetails() {
  const meds::Course& c = courses_[selected_];
  meds::CourseSummary sum;
  meds::summarize(c, today_, nowMinute_, doseMinute_, sum);
  const meds::Status status = sum.status;
  const int taken = sum.taken;
  const int due = sum.due;
  const int missed = sum.missed;
  const int timed = sum.timed;
  const int onTime = sum.onTime;
  const int fullDays = sum.fullDays;
  const int32_t lastTakenDay = sum.lastTakenDay;
  const int planned = sum.planned;
  int i = 0;
  auto put = [&](const char* label) { stats_[i++].label = label; };
  tools::formatShortDate(stats_[i].value, sizeof(stats_[i].value), c.start);
  put(tr(STR_TOOLS_MED_STARTED));
  tools::formatShortDate(stats_[i].value, sizeof(stats_[i].value), c.lastDay());
  put(status == meds::Status::Completed || status == meds::Status::Stopped ? tr(STR_TOOLS_MED_PLANNED_END)
                                                                           : tr(STR_TOOLS_MED_ENDS));
  snprintf(stats_[i].value, sizeof(stats_[i].value), "%s", statusLabel(status));
  put(tr(STR_TOOLS_MED_STATUS));
  if (status == meds::Status::Stopped) {
    tools::formatShortDate(stats_[i].value, sizeof(stats_[i].value), c.stopped);
    put(tr(STR_TOOLS_MED_STOPPED_ON));
  } else if (status == meds::Status::Completed) {
    tools::formatShortDate(stats_[i].value, sizeof(stats_[i].value), lastTakenDay != 0 ? lastTakenDay : c.lastDay());
    put(tr(STR_TOOLS_MED_COMPLETED_ON));
  } else {
    const int dayNo = std::clamp(static_cast<int>(today_ - c.start + 1), 0, static_cast<int>(c.days));
    snprintf(stats_[i].value, sizeof(stats_[i].value), "%d / %u", dayNo, static_cast<unsigned>(c.days));
    put(tr(STR_TOOLS_MED_DAY));
  }
  snprintf(stats_[i].value, sizeof(stats_[i].value), "%d / %d", taken, planned);
  put(tr(STR_TOOLS_MED_TAKEN));
  snprintf(stats_[i].value, sizeof(stats_[i].value), "%d%%", due > 0 ? std::min(100, taken * 100 / due) : 100);
  put(tr(STR_TOOLS_MED_ADHERENCE));
  snprintf(stats_[i].value, sizeof(stats_[i].value), "%d", missed);
  put(tr(STR_TOOLS_MED_MISSED));
  if (timed > 0) {
    snprintf(stats_[i].value, sizeof(stats_[i].value), "%d%%", onTime * 100 / timed);
  } else {
    snprintf(stats_[i].value, sizeof(stats_[i].value), "-");
  }
  put(tr(STR_TOOLS_MED_ON_TIME));
  if (timed > 0) {
    snprintf(stats_[i].value, sizeof(stats_[i].value), "%+d min", sum.avgDelay);
  } else {
    snprintf(stats_[i].value, sizeof(stats_[i].value), "-");
  }
  put(tr(STR_TOOLS_MED_AVG_DELAY));
  snprintf(stats_[i].value, sizeof(stats_[i].value), "%d", fullDays);
  put(tr(STR_TOOLS_MED_FULL_DAYS));
}

void MedicineActivity::loop() {
  input_.poll(mappedInput);
  if (input_.backLong) {
    tools::exitToHome();
    return;
  }
  if (screen_ == Screen::Details) {
    if (exportState_ == 1) {
      exportState_ = statsx::exportFeature(statsx::Feature::Medicine).ok ? 2 : 3;
      requestUpdate();
    } else if (input_.back) {
      screen_ = Screen::List;
      exportState_ = 0;
      transitionPending_ = true;
      requestUpdate();
    } else if (input_.confirm) {
      exportState_ = 1;
      requestUpdate();
    }
    return;
  }
  if (input_.back) {
    finish();
    return;
  }
  if (!clockValid_) return;
  if (input_.leftUp) {
    openMenu();
    return;
  }
  if (count_ == 0) {
    if (input_.confirm) addCourseName();
    return;
  }
  if (input_.up || input_.pageBack) {
    selected_ = (selected_ + count_ - 1) % count_;
    slot_ = 0;
    requestUpdate();
  } else if (input_.down || input_.pageForward) {
    selected_ = (selected_ + 1) % count_;
    slot_ = 0;
    requestUpdate();
  } else if (input_.right) {
    slot_ = (slot_ + 1) % courses_[selected_].doses;
    requestUpdate();
  } else if (input_.confirm) {
    meds::Course& c = courses_[selected_];
    int idx = 0;
    if (todayIndex(c, idx)) {
      RenderLock lock(*this);
      meds::logDose(c, idx, slot_, !c.isTaken(idx, slot_));
      requestUpdate();
    } else {
      openDetails();
    }
  }
}

void MedicineActivity::renderList(const Rect& content) {
  if (count_ == 0) {
    const int midY = content.y + content.height / 2;
    renderer.drawCenteredText(UI_12_FONT_ID, midY - 30, tr(STR_TOOLS_MED_EMPTY), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, midY + 4, tr(STR_TOOLS_MED_EMPTY_HINT));
    return;
  }
  const int nameH = renderer.getLineHeight(UI_12_FONT_ID);
  const int smallH = renderer.getLineHeight(UI_10_FONT_ID);
  const int box = 26;
  const int rowH = std::min(content.height / std::max(count_, 1), nameH + smallH + box + 30);
  for (int i = 0; i < count_; ++i) {
    const meds::Course& c = courses_[i];
    const int y = content.y + i * rowH;
    const bool sel = i == selected_;
    if (sel) renderer.fillRect(content.x - 8, y + 4, 4, rowH - 8);
    const meds::Status status = meds::statusOf(c, today_);
    const char* statusText = statusLabel(status);
    const int statusW = renderer.getTextWidth(UI_10_FONT_ID, statusText, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, content.x + content.width - statusW, y + 4, statusText, true, EpdFontFamily::BOLD);
    const auto name = renderer.truncatedText(UI_12_FONT_ID, c.name, content.width - statusW - 12, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, content.x, y + 2, name.c_str(), true, EpdFontFamily::BOLD);
    char line[80];
    // Day count stops at the stop day (or the planned end) once a course is over.
    const int dayNo =
        std::clamp(static_cast<int>(std::min<int32_t>(today_, c.endDay()) - c.start + 1), 0, static_cast<int>(c.days));
    snprintf(line, sizeof(line), "%s %d / %u  -  %d / %d %s%s%s", tr(STR_TOOLS_MED_DAY), dayNo,
             static_cast<unsigned>(c.days), c.takenCount(), c.doses * c.days, tr(STR_TOOLS_MED_DOSES_TAKEN),
             c.food != meds::FoodAny ? "  -  " : "", meds::foodLabel(c.food));
    renderer.drawText(UI_10_FONT_ID, content.x, y + nameH + 2, line);
    int idx = 0;
    if (!todayIndex(c, idx)) {
      if (i + 1 < count_) renderer.drawLine(content.x, y + rowH - 1, content.x + content.width, y + rowH - 1);
      continue;
    }
    const int cellW = content.width / c.doses;
    const int by = y + nameH + smallH + 8;
    for (int s = 0; s < c.doses; ++s) {
      const int bx = content.x + s * cellW;
      if (c.isTaken(idx, s)) {
        renderer.fillRect(bx, by, box, box);
      } else {
        renderer.drawRect(bx, by, box, box, 2, true);
      }
      if (sel && s == slot_) renderer.drawRect(bx - 4, by - 4, box + 8, box + 8, 2, true);
      char hm[8];
      charts::formatMinute(hm, sizeof(hm), c.slotMinute[s]);
      char label[32];
      snprintf(label, sizeof(label), "%s", meds::slotName(c.doses, s));
      const auto shown = renderer.truncatedText(SMALL_FONT_ID, label, cellW - box - 12);
      renderer.drawText(SMALL_FONT_ID, bx + box + 8, by - 2, shown.c_str(), true, EpdFontFamily::BOLD);
      renderer.drawText(SMALL_FONT_ID, bx + box + 8, by + box / 2, hm);
    }
    if (i + 1 < count_) renderer.drawLine(content.x, y + rowH - 1, content.x + content.width, y + rowH - 1);
  }
}

void MedicineActivity::renderDetails(const Rect& content) {
  const meds::Course& c = courses_[selected_];
  int y = content.y;
  {
    // "3 a day: 08:00, 13:00, 19:00 - After food"
    std::string when = std::to_string(c.doses) + " " + tr(STR_TOOLS_MED_PER_DAY) + ":";
    for (int d = 0; d < c.doses; ++d) {
      char hm[8];
      charts::formatMinute(hm, sizeof(hm), c.slotMinute[d]);
      when += d == 0 ? " " : ", ";
      when += hm;
    }
    if (c.food != meds::FoodAny) {
      when += "  -  ";
      when += meds::foodLabel(c.food);
    }
    renderer.drawText(UI_10_FONT_ID, content.x, y, when.c_str(), true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_10_FONT_ID) + 8;
  }
  y += charts::statGrid(renderer, Rect{content.x, y, content.width, 0}, 2, stats_, 10) + 8;
  // Dose grid: one column per day, one row per dose; filled = taken,
  // outlined = missed, dotted = still to come.
  const Rect plot =
      charts::drawPanel(renderer, Rect{content.x, y, content.width, content.y + content.height - y},
                        tr(STR_TOOLS_MED_DOSE_GRID), tr(STR_TOOLS_MED_DAY_ONE), tr(STR_TOOLS_MED_LAST_DAY));
  const int cellW = std::max(3, plot.width / c.days);
  const int cellH = std::min(34, plot.height / c.doses);
  const int gap = cellW > 8 ? 2 : 1;
  for (int d = 0; d < c.days; ++d) {
    const int32_t day = c.start + d;
    for (int s = 0; s < c.doses; ++s) {
      const int x = plot.x + d * cellW;
      const int yy = plot.y + s * cellH;
      const int w = cellW - gap;
      const int h = cellH - 3;
      if (c.isTaken(d, s)) {
        renderer.fillRect(x, yy, w, h);
      } else if (day > today_ || (day == today_ && c.slotMinute[s] > nowMinute_) ||
                 (c.stopped != 0 && day >= c.stopped)) {
        renderer.fillRectDither(x, yy, w, h, Color::LightGray);
      } else {
        renderer.drawRect(x, yy, w, h, 1, true);
      }
    }
  }
}

void MedicineActivity::render(RenderLock&&) {
  const bool details = screen_ == Screen::Details && count_ > 0;
  const Rect content = tools::drawFrame(renderer, tr(STR_TOOLS_MEDICINE), details ? courses_[selected_].name : nullptr);
  if (!clockValid_) {
    renderer.drawCenteredText(UI_12_FONT_ID, content.y + content.height / 2 - 20, tr(STR_TOOLS_CLOCK_NOT_SET), true,
                              EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, content.y + content.height / 2 + 10, tr(STR_TOOLS_SYNC_FROM_WORLD_CLOCK));
    tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
  } else if (details) {
    const int footH = renderer.getLineHeight(SMALL_FONT_ID) + 4;
    renderDetails(Rect{content.x, content.y, content.width, content.height - footH});
    char foot[64];
    snprintf(foot, sizeof(foot), "%s",
             exportState_ == 1   ? tr(STR_TOOLS_STATS_EXPORTING)
             : exportState_ == 2 ? tr(STR_TOOLS_STATS_SAVED_MEDICINE)
             : exportState_ == 3 ? tr(STR_TOOLS_STATS_EXPORT_FAILED)
                                 : tr(STR_TOOLS_STATS_EXPORT_MEDICINE_HINT));
    renderer.drawCenteredText(SMALL_FONT_ID, content.y + content.height - footH + 2, foot);
    tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_STATS_EXPORT), "", "");
  } else {
    renderList(Rect{content.x + 8, content.y, content.width - 8, content.height});
    tools::drawHints(renderer, mappedInput, tr(STR_BACK), count_ > 0 ? tr(STR_TOOLS_MED_TICK) : tr(STR_TOOLS_MED_ADD),
                     tr(STR_TOOLS_MENU), count_ > 0 ? tr(STR_TOOLS_MED_NEXT_DOSE) : "");
  }
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
