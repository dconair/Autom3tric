// Golden-file and "export == real-time" tests.
//
// The scenario below drives the whole engine (three voices, beat sync, scan, reverse, Hermite, note
// retrigger/stealing, transport) with deterministic synthetic sources and fixed seeds. Events happen on
// multiples of 960 frames so that every callback size from 64 to 960 applies them at the same sample.
//
//   AUTODJ_UPDATE_GOLDEN=1 ./test_engine --gtest_filter='Golden.*'   regenerates the reference file.

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

#include "autodj/core/DemoSources.h"
#include "autodj/engine/Engine.h"
#include "test_util.h"

using namespace autodj;

namespace {

constexpr int kSegment = 960;
constexpr int kSegments = 40;  // 0.8 s
const char* kGoldenFile = AUTODJ_GOLDEN_DIR "/granular_v1.f32";

void setup(Engine& e) {
  using testutil::setByKey;
  e.setSource(0, demo::chordPad(6.0, 48000));
  e.setSource(1, demo::drumLoop(120.0, 2, 48000));
  e.setSource(2, demo::noise(2.0, 48000, 0.5f));

  setByKey(e, "transport.bpm", 120);
  setByKey(e, "master.gain", 0.9f);
  // voice 0: dense tonal cloud, Hermite, stereo spread, random pitch
  setByKey(e, "granular.v0.gate", 1);
  setByKey(e, "granular.v0.source", 0);
  setByKey(e, "granular.v0.position", 0.35f);
  setByKey(e, "granular.v0.size_ms", 90);
  setByKey(e, "granular.v0.density", 140);
  setByKey(e, "granular.v0.spray_ms", 400);
  setByKey(e, "granular.v0.jitter", 0.3f);
  setByKey(e, "granular.v0.pitch_rand_st", 3);
  setByKey(e, "granular.v0.spread", 0.9f);
  setByKey(e, "granular.v0.quality", 1);
  setByKey(e, "granular.v0.env_shape", 1);
  // voice 1: drum slices on the 1/16 grid
  setByKey(e, "granular.v1.gate", 1);
  setByKey(e, "granular.v1.source", 1);
  setByKey(e, "granular.v1.sync_mode", 1);
  setByKey(e, "granular.v1.sync_div", 2);
  setByKey(e, "granular.v1.size_ms", 60);
  setByKey(e, "granular.v1.scan_on", 1);
  setByKey(e, "granular.v1.scan_speed", 0.5f);
  setByKey(e, "granular.v1.env_shape", 2);
  // voice 2: reversed noise texture, triggered by notes
  setByKey(e, "granular.v2.source", 2);
  setByKey(e, "granular.v2.size_ms", 30);
  setByKey(e, "granular.v2.density", 60);
  setByKey(e, "granular.v2.reverse_prob", 0.5f);
  setByKey(e, "granular.v2.env_shape", 3);
  setByKey(e, "granular.v2.pitch_st", -5);
}

void event(Engine& e, int seg) {
  using testutil::setByKey;
  if (seg == 0) e.play();
  if (seg == 5) e.noteOn(2, 0.f, 0.8f);
  if (seg == 12) e.noteOn(2, 7.f, 1.0f);   // retrigger -> steal fade
  if (seg == 18) e.noteOff(2);
  if (seg == 20) setByKey(e, "granular.v0.density", 40);
  if (seg == 24) setByKey(e, "granular.v1.sync_div", 5);
  if (seg == 30) e.noteOn(2, -7.f, 0.5f);
}

std::vector<float> renderScenario(const EngineConfig& cfg, int callback) {
  Engine e(cfg);
  setup(e);
  std::vector<float> out;
  out.reserve(static_cast<std::size_t>(kSegment) * kSegments * 2);
  for (int seg = 0; seg < kSegments; ++seg) {
    event(e, seg);
    const auto blk = testutil::render(e, kSegment, callback);
    out.insert(out.end(), blk.begin(), blk.end());
  }
  return out;
}

EngineConfig offlineCfg() {
  EngineConfig c;
  c.realtime = false;
  return c;
}

}  // namespace

TEST(Golden, OfflineRenderMatchesReferenceFile) {
  const auto out = renderScenario(offlineCfg(), 192);
  ASSERT_TRUE(testutil::allFinite(out));
  ASSERT_GT(testutil::rms(out), 0.02) << "scenario must produce real audio";

  if (std::getenv("AUTODJ_UPDATE_GOLDEN")) {
    std::ofstream f(kGoldenFile, std::ios::binary);
    f.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size() * sizeof(float)));
    GTEST_SKIP() << "golden file rewritten: " << kGoldenFile;
  }

  std::ifstream f(kGoldenFile, std::ios::binary);
  ASSERT_TRUE(f.good()) << "missing golden file " << kGoldenFile << " (run with AUTODJ_UPDATE_GOLDEN=1)";
  std::vector<float> ref(out.size());
  f.read(reinterpret_cast<char*>(ref.data()), static_cast<std::streamsize>(ref.size() * sizeof(float)));
  ASSERT_EQ(static_cast<std::size_t>(f.gcount()), ref.size() * sizeof(float)) << "golden file has the wrong size";

  double worst = 0.0, sq = 0.0;
  for (std::size_t i = 0; i < out.size(); ++i) {
    const double d = out[i] - ref[i];
    worst = std::max(worst, std::fabs(d));
    sq += d * d;
  }
  const double rmsErr = std::sqrt(sq / static_cast<double>(out.size()));
  RecordProperty("max_abs_diff", std::to_string(worst));
  RecordProperty("rms_diff", std::to_string(rmsErr));
  EXPECT_LT(worst, 1e-3) << "max |diff| against reference";
  EXPECT_LT(rmsErr, 5e-5) << "rms diff against reference";
}

namespace {

// A clock that advances 1 us per reading: the engine sees an (almost) idle CPU, so the adaptive
// quality governor stays at level 0 and nothing depends on the machine running the test.
std::uint64_t g_idleNs = 0;
std::uint64_t idleClock() noexcept { return g_idleNs += 1000; }

// RMS of every 100 ms window (both channels). A 20 ms window of a sparse stochastic cloud holds only
// a handful of grains and is too noisy a statistic to compare.
constexpr std::size_t kWindow = 4800;
std::vector<double> windowRms(const std::vector<float>& x) {
  std::vector<double> r;
  for (std::size_t s = 0; s + 2 * kWindow <= x.size(); s += 2 * kWindow) {
    double sq = 0.0;
    for (std::size_t i = 0; i < 2 * kWindow; ++i) sq += static_cast<double>(x[s + i]) * x[s + i];
    r.push_back(std::sqrt(sq / (2.0 * static_cast<double>(kWindow))));
  }
  return r;
}

}  // namespace

TEST(Golden, RealtimeModeIsBitIdenticalToOfflineExportForTheSameCallbackSize) {
  // "Export uses the same engine": realtime=true only adds load metering and the adaptive governor.
  // With an idle clock both must produce exactly the same samples.
  for (int cb : {64, 192, 480}) {
    const auto offline = renderScenario(offlineCfg(), cb);
    EngineConfig rt;
    rt.realtime = true;
    rt.clock = &idleClock;
    const auto live = renderScenario(rt, cb);
    ASSERT_EQ(live.size(), offline.size());
    EXPECT_EQ(0, std::memcmp(live.data(), offline.data(), live.size() * sizeof(float))) << "callback size " << cb;
  }
}

TEST(Golden, AnyCallbackSizeSoundsLikeTheExport) {
  // The engine cuts a callback into equal chunks of <= 192 frames, and control-rate smoothing runs once
  // per chunk. Different callback sizes therefore move grain read positions by fractions of a sample
  // (audible only as a different "random" realisation of the same cloud), but event timing, level and
  // density stay identical. Compare the loudness contour (100 ms windows), not the samples.
  const auto offline = renderScenario(offlineCfg(), 192);
  const auto refRms = windowRms(offline);
  EngineConfig rt;
  rt.clock = &idleClock;
  for (int cb : {64, 96, 240, 480, 960}) {
    const auto live = renderScenario(rt, cb);
    ASSERT_EQ(live.size(), offline.size());
    const auto liveRms = windowRms(live);
    double worstRel = 0.0;
    for (std::size_t s = 1; s < refRms.size(); ++s) {  // window 0 contains the gate attacks
      worstRel = std::max(worstRel, std::fabs(liveRms[s] - refRms[s]) / std::max(refRms[s], 1e-3));
    }
    RecordProperty("worst_rel_rms_cb" + std::to_string(cb), std::to_string(worstRel));
    std::printf("[ info ] callback %d: worst 100 ms RMS deviation %.4f, total RMS %.5f vs %.5f\n", cb, worstRel,
                testutil::rms(live), testutil::rms(offline));
    EXPECT_LT(worstRel, 0.10) << "callback size " << cb;
    EXPECT_NEAR(testutil::rms(live), testutil::rms(offline), 0.03 * testutil::rms(offline)) << "callback size " << cb;
    EXPECT_LT(testutil::peak(live), 1.0f);
  }
}
