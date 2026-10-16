#include "ToolStatsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdarg>
#include <cstdio>

#include "ToolsLog.h"
#include "fontIds.h"

namespace toolstats {

void Page::tile(const char* label, const char* fmt, ...) {
  if (tileCount >= kMaxTiles) return;
  charts::Stat& t = tiles[tileCount++];
  va_list args;
  va_start(args, fmt);
  vsnprintf(t.value, sizeof(t.value), fmt, args);
  va_end(args);
  t.label = label;
}

}  // namespace toolstats

void ToolStatsActivity::onEnter() {
  Activity::onEnter();
  input_.reset(mappedInput);
  tlog::Stamp now;
  page_.clockValid = tlog::now(now);
  if (page_.clockValid) build_(page_, now.day, ctx_);
  transitionPending_ = true;
  requestUpdate();
}

void ToolStatsActivity::loop() {
  if (export_ == ExportState::Working) {
    const bool ok = statsx::exportFeature(feature_).ok;
    export_ = ok ? ExportState::Done : ExportState::Failed;
    requestUpdate();
    return;
  }
  input_.poll(mappedInput);
  if (input_.backLong) {
    tools::exitToHome();
    return;
  }
  if (input_.back) {
    finish();
    return;
  }
  if (input_.confirm && page_.clockValid) {
    export_ = ExportState::Working;
    requestUpdate();
  }
}

void ToolStatsActivity::render(RenderLock&&) {
  const Rect content = tools::drawFrame(renderer, title_, page_.subtitle[0] != '\0' ? page_.subtitle : nullptr);
  if (!page_.clockValid) {
    renderer.drawCenteredText(UI_12_FONT_ID, content.y + content.height / 2 - 20, tr(STR_TOOLS_CLOCK_NOT_SET), true,
                              EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, content.y + content.height / 2 + 10,
                              tr(STR_TOOLS_SYNC_FROM_WORLD_CLOCK));
    tools::drawHints(renderer, mappedInput, tr(STR_BACK), "", "", "");
    renderer.displayBuffer(tools::transitionRefresh());
    transitionPending_ = false;
    return;
  }

  // Footer line: where the export goes, or what just happened.
  const int footH = renderer.getLineHeight(SMALL_FONT_ID) + 4;
  int y = content.y;
  y += charts::statGrid(renderer, Rect{content.x, y, content.width, 0}, page_.cols, page_.tiles, page_.tileCount) + 6;
  int chartCount = 0;
  for (const auto& c : page_.charts) chartCount += c.kind != toolstats::Chart::Kind::None ? 1 : 0;
  if (chartCount > 0) {
    const int avail = content.y + content.height - footH - y;
    const int chartH = std::min(220, (avail - 10 * (chartCount - 1)) / chartCount);
    for (const auto& c : page_.charts) {
      if (c.kind == toolstats::Chart::Kind::None) continue;
      const Rect plot = charts::drawPanel(renderer, Rect{content.x, y, content.width, chartH}, c.title, c.left, c.right);
      switch (c.kind) {
        case toolstats::Chart::Kind::Line:
          charts::lineChart(renderer, plot, c.values, c.n, c.minV, c.maxV, c.yTop[0] ? c.yTop : nullptr,
                            c.yBottom[0] ? c.yBottom : nullptr);
          break;
        case toolstats::Chart::Kind::Bar:
          charts::barChart(renderer, plot, c.values, c.n, c.maxV);
          if (c.yTop[0] != '\0') renderer.drawText(SMALL_FONT_ID, plot.x + 2, plot.y, c.yTop);
          break;
        case toolstats::Chart::Kind::Time:
          charts::timeScatter(renderer, plot, c.values, c.n);
          break;
        default:
          break;
      }
      y += chartH + 10;
    }
  }

  char foot[64];
  switch (export_) {
    case ExportState::Working:
      snprintf(foot, sizeof(foot), "%s", tr(STR_TOOLS_STATS_EXPORTING));
      break;
    case ExportState::Done:
      snprintf(foot, sizeof(foot), "%s /stats/%s/", tr(STR_TOOLS_STATS_SAVED_TO), statsx::featureFolder(feature_));
      break;
    case ExportState::Failed:
      snprintf(foot, sizeof(foot), "%s", tr(STR_TOOLS_STATS_EXPORT_FAILED));
      break;
    default:
      snprintf(foot, sizeof(foot), "%s /stats/%s/", tr(STR_TOOLS_STATS_EXPORT_HINT), statsx::featureFolder(feature_));
      break;
  }
  renderer.drawCenteredText(SMALL_FONT_ID, content.y + content.height - footH + 2, foot);
  tools::drawHints(renderer, mappedInput, tr(STR_BACK), tr(STR_TOOLS_STATS_EXPORT), "", "");
  const bool transition = transitionPending_;
  transitionPending_ = false;
  renderer.displayBuffer(transition ? tools::transitionRefresh() : HalDisplay::FAST_REFRESH);
}
