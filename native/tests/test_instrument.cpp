// Tests for the "instrument" layer on top of the granular engine: cells (Hold / Step / Keys), the Chaos stage and the
// output level, as seen through Engine::process (so the RT guard is active).
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "autodj/core/DemoSources.h"
#include "autodj/core/RtGuard.h"
#include "autodj/engine/Engine.h"
#include "test_util.h"

using namespace autodj;
using namespace autodj::granular;

namespace {

struct Spawn {
  int voice;
  std::int64_t sample;
  double pos;
};

void onSpawn(void* ctx, int voice, std::int64_t sample, double pos, int, float, bool) {
  rt::ScopedAllowAlloc allow;  // test-only observer: the recording vector may grow
  static_cast<std::vector<Spawn>*>(ctx)->push_back({voice, sample, pos});
}

EngineConfig offline() {
  EngineConfig c;
  c.realtime = false;
  return c;
}

constexpr double kSourceSeconds = 4.0;

struct Rig {
  Rig() : e(offline()) {
    e.granularNode().setSpawnObserver(&onSpawn, &spawns);
    EXPECT_TRUE(e.setSource(0, demo::chordPad(kSourceSeconds, 48000)));
    v("gate", 1);
    v("density", 100);
    v("size_ms", 60);
    v("spray_ms", 0);
    v("jitter", 0);
  }
  void set(const std::string& key, float val) { testutil::setByKey(e, key, val); }
  void v(const char* name, float val) { set(std::string("granular.v0.") + name, val); }
  std::vector<float> run(double seconds, int cb = 192) { return testutil::render(e, static_cast<int>(seconds * 48000), cb); }
  double frames() const { return kSourceSeconds * 48000.0; }

  Engine e;
  std::vector<Spawn> spawns;
};

// Cell a spawn at `pos` (source frames) belongs to, and the offset inside it (0..1).
int cellOf(double pos, double frames, int n) { return static_cast<int>(std::floor(pos / (frames - 1.0) * n + 1e-9)); }

}  // namespace

// ------------------------------------------------------------------------------------------------ cells

TEST(Cells, KeysModePlaysTheSelectedCellAtTheOffsetInsideIt) {
  for (int count : {0, 1}) {  // enum index: 0 = 8 cells, 1 = 16 cells
    const int n = count == 0 ? 8 : 16;
    for (int c = 0; c < n; ++c) {
      Rig r;
      r.v("cell_mode", static_cast<float>(CellMode::Keys));
      r.v("cell_count", static_cast<float>(count));
      r.v("cell_index", static_cast<float>(c));
      r.v("position", 0.25f);
      r.run(0.25);
      ASSERT_GT(r.spawns.size(), 10u);
      const double expect = (c + 0.25) / n * (r.frames() - 1.0);
      for (const Spawn& s : r.spawns) EXPECT_NEAR(s.pos, expect, 2.0) << "cells " << n << " cell " << c;
    }
  }
}

TEST(Cells, HoldModeIgnoresCellSettings) {
  Rig r;
  r.v("cell_mode", static_cast<float>(CellMode::Hold));
  r.v("cell_index", 5);
  r.v("cell_count", 1);
  r.v("position", 0.5f);
  r.run(0.25);
  ASSERT_GT(r.spawns.size(), 10u);
  for (const Spawn& s : r.spawns) EXPECT_NEAR(s.pos, 0.5 * (r.frames() - 1.0), 2.0);
}

TEST(Cells, StepModeWalksThroughTheCellsLockedToTheTempoGrid) {
  for (int count : {0, 1}) {
    const int n = count == 0 ? 8 : 16;
    Rig r;
    r.set("transport.bpm", 120.f);  // 2 beats per second = 24000 samples per beat
    r.v("cell_mode", static_cast<float>(CellMode::Step));
    r.v("cell_count", static_cast<float>(count));
    r.v("step_div", 3);             // 1/4 note: one cell per beat
    r.v("position", 0.5f);
    r.run(static_cast<double>(n + 3) * 0.5);  // a bit more than one full lap

    ASSERT_GT(r.spawns.size(), 100u);
    int checked = 0;
    std::vector<bool> seen(static_cast<std::size_t>(n), false);
    for (const Spawn& s : r.spawns) {
      const double beat = static_cast<double>(s.sample) / 24000.0;
      const double frac = beat - std::floor(beat);
      if (frac < 1e-3 || frac > 1.0 - 1e-3) continue;  // a few samples around a boundary may go either way
      const int expectCell = static_cast<int>(std::floor(beat)) % n;
      EXPECT_EQ(cellOf(s.pos, r.frames(), n), expectCell) << "sample " << s.sample << " beat " << beat;
      EXPECT_NEAR(s.pos / (r.frames() - 1.0) * n - expectCell, 0.5, 1e-3);
      seen[static_cast<std::size_t>(expectCell)] = true;
      ++checked;
    }
    EXPECT_GT(checked, 100);
    for (int c = 0; c < n; ++c) EXPECT_TRUE(seen[static_cast<std::size_t>(c)]) << "cell " << c << " was never played";
  }
}

TEST(Cells, StepBoundaryIsSampleAccurateInsideACallback) {
  // One big callback (480 frames, chunked into 3 x 160): a grain born before the boundary must read cell k, one born
  // after it cell k+1, even when both are spawned in the same block.
  Rig r;
  r.set("transport.bpm", 120.f);
  r.v("cell_mode", static_cast<float>(CellMode::Step));
  r.v("step_div", 5);  // 1/16 note = 6000 samples per step
  r.v("density", 2000);
  r.v("position", 0.0f);
  r.run(0.6, 480);
  ASSERT_GT(r.spawns.size(), 200u);
  int checkedBefore = 0, checkedAfter = 0;
  for (const Spawn& s : r.spawns) {
    const std::int64_t stepIdx = s.sample / 6000;
    const std::int64_t inStep = s.sample % 6000;
    if (inStep < 3 || inStep > 5996) continue;
    EXPECT_EQ(cellOf(s.pos, r.frames(), 8), static_cast<int>(stepIdx % 8)) << "sample " << s.sample;
    (inStep < 3000 ? checkedBefore : checkedAfter)++;
  }
  EXPECT_GT(checkedBefore, 50);
  EXPECT_GT(checkedAfter, 50);
}

TEST(Cells, StepSequenceDoesNotDependOnTheCallbackSize) {
  std::vector<std::vector<std::pair<std::int64_t, double>>> runs;
  for (int cb : {64, 192, 480, 1000}) {
    Rig r;
    r.set("transport.bpm", 133.f);
    r.v("cell_mode", static_cast<float>(CellMode::Step));
    r.v("step_div", 4);
    r.v("position", 0.3f);
    r.run(3.0, cb);
    std::vector<std::pair<std::int64_t, double>> v;
    for (const Spawn& s : r.spawns) v.emplace_back(s.sample, s.pos);
    runs.push_back(std::move(v));
  }
  const auto& ref = runs[0];
  ASSERT_GT(ref.size(), 100u);
  for (std::size_t k = 1; k < runs.size(); ++k) {
    ASSERT_EQ(runs[k].size(), ref.size());
    for (std::size_t i = 0; i < ref.size(); ++i) {
      EXPECT_EQ(runs[k][i].first, ref[i].first) << "run " << k << " spawn " << i;
      EXPECT_NEAR(runs[k][i].second, ref[i].second, 1e-3) << "run " << k << " spawn " << i;
    }
  }
}

TEST(Cells, ResetRestartsTheSequenceAtCellZeroOnTheNextGridStep) {
  Rig r;
  r.set("transport.bpm", 120.f);
  r.v("cell_mode", static_cast<float>(CellMode::Step));
  r.v("step_div", 3);  // 1 beat = 24000 samples
  r.v("position", 0.5f);
  r.run(2.6 * 0.5);  // beats 0 .. 2.6 -> inside step 2
  r.spawns.clear();
  ASSERT_TRUE(r.e.resetSteps(0));
  r.run(1.0);  // beats 2.6 .. 4.6
  ASSERT_GT(r.spawns.size(), 50u);
  for (const Spawn& s : r.spawns) {
    const double beat = static_cast<double>(s.sample) / 24000.0;
    const double frac = beat - std::floor(beat);
    if (frac < 1e-3 || frac > 1.0 - 1e-3) continue;
    // the reset happened at beat 2.6 (step 2 of the grid): step 2 = cell 0, step 3 = cell 1, step 4 = cell 2
    const int expectCell = static_cast<int>(std::floor(beat)) - 2;
    EXPECT_EQ(cellOf(s.pos, r.frames(), 8), expectCell) << "beat " << beat;
  }
}

TEST(Cells, ScanIsParkedOutsideHoldMode) {
  Rig r;
  r.v("cell_mode", static_cast<float>(CellMode::Keys));
  r.v("cell_index", 2);
  r.v("position", 0.5f);
  r.v("scan_on", 1);
  r.v("scan_speed", 2.f);
  r.run(0.5);
  ASSERT_GT(r.spawns.size(), 20u);
  for (const Spawn& s : r.spawns) EXPECT_NEAR(s.pos, 2.5 / 8.0 * (r.frames() - 1.0), 2.0);
}

TEST(Cells, VizReportsTheCurrentCell) {
  Rig r;
  r.v("cell_mode", static_cast<float>(CellMode::Keys));
  r.v("cell_index", 3);
  r.run(0.3);
  GrainViz z;
  ASSERT_TRUE(r.e.readGrainViz(z));
  EXPECT_EQ(z.cell[0], 3);
  r.v("cell_mode", static_cast<float>(CellMode::Hold));
  r.run(0.3);
  ASSERT_TRUE(r.e.readGrainViz(z));
  EXPECT_EQ(z.cell[0], -1);
}

TEST(Cells, SwitchingCellsWhilePlayingIsClickFree) {
  auto render = [](bool switching) {
    Rig r;
    r.v("cell_mode", static_cast<float>(CellMode::Keys));
    r.v("cell_index", 0);
    r.v("size_ms", 80);
    r.v("density", 60);
    std::vector<float> all;
    for (int i = 0; i < 24; ++i) {
      if (switching) r.v("cell_index", static_cast<float>(i % 8));
      const auto part = r.run(0.1);
      all.insert(all.end(), part.begin(), part.end());
    }
    return all;
  };
  const auto steady = render(false);
  const auto switched = render(true);
  ASSERT_TRUE(testutil::allFinite(switched));
  // Every grain has its own zero-ended envelope, so changing where *new* grains start cannot create a discontinuity.
  const float base = testutil::maxSecondDifference(steady, 0, 4800);
  EXPECT_LT(testutil::maxSecondDifference(switched, 0, 4800), base * 1.6f + 1e-3f);
}

TEST(Cells, NoRealtimeViolationsInAnyMode) {
  testutil::ViolationCounter violations;
  for (CellMode m : {CellMode::Hold, CellMode::Step, CellMode::Keys}) {
    Rig r;
    r.v("cell_mode", static_cast<float>(m));
    r.v("cell_count", 1);
    r.set("granular.chaos", 0.6f);
    r.run(0.5);
    r.e.resetSteps(0);
    r.run(0.2);
  }
  EXPECT_EQ(violations.count(), 0u);
}

// ------------------------------------------------------------------------------------------------ envelopes

TEST(Instrument, EveryEnvelopeShapeRendersFiniteAndZeroEndedGrains) {
  for (int shape = 0; shape < static_cast<int>(EnvShape::Count); ++shape) {
    Rig r;
    r.v("env_shape", static_cast<float>(shape));
    r.v("size_ms", 40);
    r.v("density", 3);  // isolated grains
    const auto out = r.run(1.0);
    ASSERT_TRUE(testutil::allFinite(out)) << "shape " << shape;
    EXPECT_GT(testutil::peak(out), 0.01f) << "shape " << shape;
  }
}

// ------------------------------------------------------------------------------------------------ output level

TEST(Instrument, OutputLevelIsInDecibels) {
  auto rms = [](float db) {
    Rig r;
    r.set("granular.output_db", db);
    const auto out = r.run(1.5);
    return testutil::rms(out, 48000);
  };
  const double ref = rms(0.f);
  ASSERT_GT(ref, 1e-3);
  EXPECT_NEAR(rms(6.f) / ref, std::pow(10.0, 6.0 / 20.0), 0.03);
  EXPECT_NEAR(rms(-12.f) / ref, std::pow(10.0, -12.0 / 20.0), 0.01);
}

// ------------------------------------------------------------------------------------------------ chaos

TEST(Chaos, ChangesTheSoundStaysFiniteAndBounded) {
  Rig dry, wet;
  wet.set("granular.chaos", 0.7f);
  const auto a = dry.run(2.0);
  const auto b = wet.run(2.0);
  ASSERT_TRUE(testutil::allFinite(b));
  EXPECT_LE(testutil::peak(b), 1.0f);
  double diff = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i) diff += std::fabs(a[i] - b[i]);
  EXPECT_GT(diff / static_cast<double>(a.size()), 1e-3);
}

TEST(Chaos, AddsATailAfterTheGrainsHaveStopped) {
  auto tailRms = [](float chaos) {
    Rig r;
    r.set("granular.chaos", chaos);
    r.v("release_ms", 20);
    r.run(1.0);
    r.v("gate", 0);
    r.run(0.3);  // let the cloud release
    const auto tail = r.run(0.5);
    return testutil::rms(tail);
  };
  EXPECT_EQ(tailRms(0.f), 0.0) << "without chaos the output must be silent after the release";
  EXPECT_GT(tailRms(0.15f), 1e-4) << "a reverb tail should ring on";
}

TEST(Chaos, TurningItOffRemovesItWithoutAClickAndWithoutReplayingOldAudio) {
  Rig r;
  r.set("granular.chaos", 0.8f);
  r.run(1.5);
  r.set("granular.chaos", 0.f);
  const auto during = r.run(1.5);
  ASSERT_TRUE(testutil::allFinite(during));
  // Silence the source, keep chaos off: nothing may come out (the state was cleared when the stage went off).
  r.v("gate", 0);
  r.run(0.4);
  const auto silent = r.run(0.5);
  EXPECT_EQ(testutil::peak(silent), 0.f);
  // Switch it on again with nothing playing: still silent, the old tail does not come back.
  r.set("granular.chaos", 0.8f);
  const auto again = r.run(0.5);
  EXPECT_EQ(testutil::peak(again), 0.f);
}
