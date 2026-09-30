#pragma once

#include <cstdint>

#include "StatsExport.h"
#include "ToolsCharts.h"
#include "ToolsCommon.h"
#include "activities/Activity.h"

// One shared stats screen for every tool: a grid of numbers, up to two
// charts and Confirm = Export (that tool's JSON under /stats/<tool>/). Each
// tool only supplies a builder that fills a StatsPage, which keeps the
// per-tool code (and the firmware) small.
namespace toolstats {

constexpr int kMaxTiles = 10;
constexpr int kMaxPoints = 30;

struct Chart {
  enum class Kind : uint8_t { None, Line, Bar, Time };
  Kind kind = Kind::None;
  const char* title = nullptr;
  const char* left = nullptr;   // x-axis caption, oldest end
  const char* right = nullptr;  // x-axis caption, newest end
  int16_t values[kMaxPoints] = {};
  int n = 0;
  int16_t minV = 0;
  int16_t maxV = 100;
  char yTop[12] = "";
  char yBottom[12] = "";
};

struct Page {
  char subtitle[40] = "";
  charts::Stat tiles[kMaxTiles] = {};
  int tileCount = 0;
  int cols = 2;
  Chart charts[2];
  bool clockValid = true;

  // Appends a tile; value is printf-formatted.
  void tile(const char* label, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
};

// Fills `page` for the tool; ctx is the builder's own argument (e.g. a habit name).
using BuildFn = void (*)(Page& page, int32_t today, const void* ctx);

}  // namespace toolstats

class ToolStatsActivity final : public Activity {
 public:
  ToolStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* title,
                    toolstats::BuildFn build, const void* ctx, statsx::Feature feature, bool keepAwake = false)
      : Activity("ToolStats", renderer, mappedInput),
        title_(title),
        build_(build),
        ctx_(ctx),
        feature_(feature),
        keepAwake_(keepAwake) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  // A running Pomodoro underneath still needs the device awake.
  bool preventAutoSleep() override { return keepAwake_; }

 private:
  enum class ExportState : uint8_t { None, Working, Done, Failed };

  const char* title_;
  toolstats::BuildFn build_;
  const void* ctx_;
  statsx::Feature feature_;
  bool keepAwake_;
  tools::ToolInput input_;
  toolstats::Page page_;
  ExportState export_ = ExportState::None;
  bool transitionPending_ = true;
};
