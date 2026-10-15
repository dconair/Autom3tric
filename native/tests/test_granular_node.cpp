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

EngineConfig offlineConfig() {
  EngineConfig c;
  c.realtime = false;
  return c;
}

struct Spawn {
  int voice;
  std::int64_t sample;
  double pos;
  int length;
  float rate;
  bool reversed;
};

// Test-only observer, called on the audio thread: the recording vector may grow, the engine may not.
void onSpawn(void* ctx, int voice, std::int64_t sample, double pos, int length, float rate, bool reversed) {
  rt::ScopedAllowAlloc allow;
  static_cast<std::vector<Spawn>*>(ctx)->push_back({voice, sample, pos, length, rate, reversed});
}

struct Rig {
  explicit Rig(EngineConfig cfg = offlineConfig()) : e(cfg) {
    e.granularNode().setSpawnObserver(&onSpawn, &spawns);
  }
  void set(const std::string& key, float v) { testutil::setByKey(e, key, v); }
  void v(int voice, const char* name, float val) { set("granular.v" + std::to_string(voice) + "." + name, val); }
  void source(int slot, std::shared_ptr<const SampleBuffer> b) { ASSERT_TRUE(e.setSource(slot, std::move(b))); }
  std::vector<float> run(double seconds, int cb = 192) { return testutil::render(e, static_cast<int>(seconds * 48000), cb); }

  Engine e;
  std::vector<Spawn> spawns;
};

}  // namespace

TEST(Granular, SilentWithoutGateAndFiniteBoundedWithGate) {
  Rig r;
  r.source(0, demo::chordPad(4.0, 48000));
  auto idle = r.run(0.5);
  EXPECT_EQ(testutil::peak(idle), 0.f);

  r.v(0, "gate", 1);
  r.v(0, "density", 80);
  auto out = r.run(2.0);
  EXPECT_TRUE(testutil::allFinite(out));
  EXPECT_GT(testutil::peak(out), 0.02f);
  EXPECT_LE(testutil::peak(out), 1.0f);
}

TEST(Granular, PitchParameterMapsToPlaybackRateExactly) {
  Rig r;
  r.source(0, demo::noise(3.0, 48000));
  r.v(0, "gate", 1);
  r.v(0, "density", 50);
  r.v(0, "pitch_st", 7);
  r.run(0.5);
  ASSERT_GT(r.spawns.size(), 5u);
  for (const Spawn& s : r.spawns) EXPECT_NEAR(s.rate, std::exp2(7.0f / 12.0f), 1e-5f);

  r.spawns.clear();
  r.v(0, "pitch_st", -24);
  r.v(0, "pitch_rand_st", 5);
  r.run(1.0);
  ASSERT_GT(r.spawns.size(), 20u);
  float lo = 99.f, hi = 0.f;
  for (const Spawn& s : r.spawns) {
    if (s.sample < 24000 + 48000 / 4) continue;  // this run starts at sample 24000; pitch smoothing settles within 100 ms
    lo = std::min(lo, s.rate);
    hi = std::max(hi, s.rate);
  }
  EXPECT_GE(lo, std::exp2((-24.f - 5.f) / 12.f) * 0.999f);
  EXPECT_LE(hi, std::exp2((-24.f + 5.f) / 12.f) * 1.001f);
  EXPECT_GT(hi / lo, 1.2f) << "random pitch must actually spread";
}

TEST(Granular, RenderedGrainHasTheRequestedPitch) {
  // One 500 ms grain per second, no overlap: analyse the middle of the first grain.
  const double f = 2000.0;
  for (double semis : {-12.0, 0.0, 7.0, 12.0}) {
    Rig r;
    r.source(0, demo::sine(f, 4.0, 48000, 0.8f));
    r.v(0, "gate", 1);
    r.v(0, "size_ms", 500);
    r.v(0, "density", 1);
    r.v(0, "position", 0.2f);
    r.v(0, "pitch_st", static_cast<float>(semis));
    r.v(0, "auto_gain", 0);
    r.v(0, "volume", 1.0f);
    r.set("master.gain", 1.0f);
    const auto out = r.run(0.6);
    const int start = 4800, n = 14400;  // 100 ms .. 400 ms, symmetric around the grain centre
    const double expected = f * std::pow(2.0, semis / 12.0);
    const double measured = testutil::peakFrequency(out.data() + 2 * start, n, 48000.0, expected * 0.95, expected * 1.05, 2);
    EXPECT_LT(std::fabs(testutil::centsBetween(measured, expected)), 0.6) << semis << " st -> " << measured << " Hz";
  }
}

TEST(Granular, FreeRunningOnsetsAreSampleAccurateWhateverTheCallbackSize) {
  std::vector<std::vector<long>> all;
  for (int cb : {64, 192, 480, 1000}) {
    Rig r;
    r.source(0, demo::noise(2.0, 48000));
    r.v(0, "gate", 1);
    r.v(0, "density", 100);
    r.v(0, "size_ms", 20);
    r.run(1.0, cb);
    std::vector<long> o;
    for (const Spawn& s : r.spawns) o.push_back(static_cast<long>(s.sample));
    all.push_back(o);
  }
  ASSERT_GE(all[0].size(), 99u);
  for (std::size_t i = 1; i < all.size(); ++i) EXPECT_EQ(all[i], all[0]) << "callback size changes onset times";
  // gate attack starts at the first block: onset 0, then every 480 samples
  for (std::size_t i = 0; i < all[0].size(); ++i) EXPECT_NEAR(static_cast<double>(all[0][i]), 480.0 * static_cast<double>(i), 1.0);
}

TEST(Granular, BeatSyncGrainsLandExactlyOnTheTransportGrid) {
  struct Case { float bpm; int div; double beats; };
  // div index: 0=1/4 1=1/8 2=1/16 3=1/32 4=1/4T 5=1/8T 6=1/16T
  for (Case c : {Case{120.f, 2, 0.25}, Case{128.f, 5, 1.0 / 3.0}, Case{90.f, 0, 1.0}, Case{140.f, 6, 1.0 / 6.0}}) {
    std::vector<std::vector<long>> lists;
    for (int cb : {64, 192, 480}) {
      Rig r;
      r.source(0, demo::drumLoop(120.0, 2, 48000));
      r.set("transport.bpm", c.bpm);
      r.v(0, "gate", 1);
      r.v(0, "sync_mode", 1);
      r.v(0, "sync_div", static_cast<float>(c.div));
      r.v(0, "size_ms", 30);
      r.run(3.0, cb);
      std::vector<long> o;
      for (const Spawn& s : r.spawns) o.push_back(static_cast<long>(s.sample));
      lists.push_back(o);
    }
    for (std::size_t i = 1; i < lists.size(); ++i) EXPECT_EQ(lists[i], lists[0]) << "bpm " << c.bpm;
    const double samplesPerLine = 48000.0 * 60.0 / c.bpm * c.beats;
    ASSERT_GE(lists[0].size(), 4u);
    for (std::size_t k = 0; k < lists[0].size(); ++k) {
      EXPECT_LT(std::fabs(static_cast<double>(lists[0][k]) - samplesPerLine * static_cast<double>(k)), 1.0)
          << "bpm " << c.bpm << " div " << c.div << " grain " << k;
    }
  }
}

TEST(Granular, GateOffFadesTheCloudOutWithoutClick) {
  Rig r;
  r.source(0, demo::sine(220.0, 5.0, 48000, 0.8f));
  r.v(0, "gate", 1);
  r.v(0, "density", 40);
  r.v(0, "size_ms", 60);
  r.v(0, "release_ms", 80);
  std::vector<float> out = r.run(0.6);
  r.v(0, "gate", 0);
  const auto tail = r.run(1.0);
  out.insert(out.end(), tail.begin(), tail.end());

  EXPECT_LT(testutil::peak(tail, 2 * 48000 * 4 / 10), 1e-3f) << "silent 400 ms after release";
  EXPECT_LT(testutil::maxSecondDifference(out, 0), 0.02f) << "no click at gate on/off";
  EXPECT_LT(testutil::maxSecondDifference(out, 1), 0.02f);
}

TEST(Granular, RetriggerStealsWithShortFadeAndStaysClickFree) {
  Rig r;
  r.source(0, demo::sine(220.0, 5.0, 48000, 0.8f));
  r.v(0, "density", 60);
  r.v(0, "size_ms", 120);
  r.v(0, "position", 0.3f);
  r.v(0, "env_shape", 3);  // trapezoid: grains are at full level when they are stolen, so a hard cut would be audible
  std::vector<float> out;
  r.e.noteOn(0);
  for (int i = 0; i < 40; ++i) {  // retrigger every 40 ms for 1.6 s: constant stealing of loud grains
    const auto blk = r.run(0.04, 112);
    out.insert(out.end(), blk.begin(), blk.end());
    r.e.noteOn(0);
  }
  ASSERT_GT(testutil::peak(out), 0.05f);
  const float d2 = std::max(testutil::maxSecondDifference(out, 0), testutil::maxSecondDifference(out, 1));
  EXPECT_LT(d2, 0.02f) << "stolen grains must fade, not cut";

  // Sensitivity check of the detector itself: cut the same signal abruptly where it is loud.
  std::vector<float> cut = out;
  const float loud = 0.6f * testutil::peak(out);
  std::size_t at = 0;
  for (std::size_t i = 20000; i < cut.size() / 2; ++i) {
    if (std::fabs(cut[2 * i]) > loud) { at = i; break; }
  }
  ASSERT_GT(at, 0u) << "peak " << testutil::peak(out);
  for (std::size_t i = at; i < at + 200; ++i) cut[2 * i] = cut[2 * i + 1] = 0.f;
  EXPECT_GT(testutil::maxSecondDifference(cut, 0), 0.5f * loud) << "the click detector must fire on a hard cut";
}

TEST(Granular, LoweringTheVoiceLimitFadesVoicesOutWithoutClick) {
  Rig r;
  r.source(0, demo::sine(180.0, 5.0, 48000, 0.8f));
  for (int v = 0; v < 4; ++v) {
    r.v(v, "gate", 1);
    r.v(v, "density", 30);
    r.v(v, "size_ms", 100);
    r.v(v, "position", 0.2f + 0.2f * static_cast<float>(v));
  }
  std::vector<float> out = r.run(0.8);
  r.set("granular.max_voices", 1);
  const auto tail = r.run(0.8);
  out.insert(out.end(), tail.begin(), tail.end());
  r.set("granular.max_voices", 4);
  const auto back = r.run(0.5);
  out.insert(out.end(), back.begin(), back.end());
  EXPECT_LT(testutil::maxSecondDifference(out, 0), 0.02f);
  EXPECT_LT(testutil::maxSecondDifference(out, 1), 0.02f);
  EXPECT_GT(testutil::peak(back), 0.02f) << "voices come back";
}

TEST(Granular, LiveSourceSwitchIsClickFreeAndOldBufferIsReleasedAfterItsGrains) {
  Rig r;
  auto a = demo::sine(200.0, 4.0, 48000, 0.7f);
  auto b = demo::sine(310.0, 4.0, 48000, 0.7f);
  r.source(0, a);
  r.source(1, b);
  r.v(0, "gate", 1);
  r.v(0, "density", 50);
  r.v(0, "size_ms", 80);
  std::vector<float> out = r.run(0.5);
  r.v(0, "source", 1);  // switch while grains of A are in flight
  auto more = r.run(0.5);
  out.insert(out.end(), more.begin(), more.end());
  EXPECT_LT(testutil::maxSecondDifference(out, 0), 0.02f);
  EXPECT_LT(testutil::maxSecondDifference(out, 1), 0.02f);

  // Replace slot 0 (no longer used by new grains): A must come back through the garbage queue once its grains ended.
  std::weak_ptr<const SampleBuffer> weakA = a;
  r.source(0, demo::sine(100.0, 1.0, 48000, 0.5f));
  a.reset();
  r.run(0.3);  // longer than any grain of A can still live (80 ms)
  r.e.collectGarbage();
  EXPECT_TRUE(weakA.expired()) << "buffer A should be freed on the control side after the audio thread released it";
}

TEST(Granular, GrainCapIsHonouredAndPoolStaysConsistent) {
  Rig r;
  r.source(0, demo::noise(3.0, 48000, 0.3f));
  r.set("granular.grain_cap", 64);
  r.set("granular.adaptive", 0);
  for (int v = 0; v < 4; ++v) {
    r.v(v, "gate", 1);
    r.v(v, "density", 2000);
    r.v(v, "size_ms", 500);
  }
  int worst = 0;
  std::vector<float> out;
  for (int i = 0; i < 400; ++i) {
    const auto blk = r.run(0.004, 192);
    worst = std::max(worst, r.e.granularNode().stats().activeGrains);
    out.insert(out.end(), blk.begin(), blk.end());
  }
  EXPECT_LE(worst, 64);
  EXPECT_GT(worst, 40) << "the cap should actually be reached under this load";
  EXPECT_TRUE(r.e.granularNode().pool().checkInvariants());
  EXPECT_TRUE(testutil::allFinite(out));
  EXPECT_LE(testutil::peak(out), 1.0f);
}

TEST(Granular, ScanMovesTheHeadWrapsInsideTheLoopAndFreezes) {
  Rig r;
  r.source(0, demo::chordPad(10.0, 48000));
  r.v(0, "gate", 1);
  r.v(0, "scan_on", 1);
  r.v(0, "scan_speed", 1.0f);
  r.v(0, "position", 0.30f);
  r.v(0, "loop_start", 0.25f);
  r.v(0, "loop_end", 0.5f);
  auto head = [&] {
    GrainViz v;
    return r.e.readGrainViz(v) ? v.playhead[0] : -1.f;
  };
  float lo = 1.f, hi = 0.f;
  GrainViz v;
  for (int i = 0; i < 600; ++i) {  // 600 * 20 ms = 12 s, loop is 2.5 s wide
    r.run(0.02);
    if (r.e.readGrainViz(v)) {
      lo = std::min(lo, v.playhead[0]);
      hi = std::max(hi, v.playhead[0]);
    }
  }
  EXPECT_GE(lo, 0.25f - 1e-3f);
  EXPECT_LE(hi, 0.5f + 1e-3f);
  EXPECT_GT(hi - lo, 0.2f) << "head sweeps the whole loop region";

  r.v(0, "freeze", 1);
  r.run(0.1);
  head();
  r.run(0.05);
  const float a = head();
  r.run(0.5);
  const float b = head();
  EXPECT_FLOAT_EQ(a, b) << "freeze stops the scan head";

  r.v(0, "freeze", 0);
  r.v(0, "scan_speed", -1.0f);
  r.run(0.3);
  head();
  r.run(0.2);
  const float c = head();
  r.run(0.2);
  const float d = head();
  EXPECT_TRUE(d < c || (c < 0.3f && d > 0.4f)) << "backwards scan (or wrapped): " << c << " -> " << d;
}

TEST(Granular, ReverseProbabilityIsRespected) {
  for (float prob : {0.f, 0.5f, 1.f}) {
    Rig r;
    r.source(0, demo::noise(2.0, 48000));
    r.v(0, "gate", 1);
    r.v(0, "density", 400);
    r.v(0, "size_ms", 10);
    r.v(0, "reverse_prob", prob);
    r.run(5.0);
    ASSERT_GT(r.spawns.size(), 1500u);
    int rev = 0;
    for (const Spawn& s : r.spawns) {
      rev += s.reversed;
      EXPECT_EQ(s.reversed, s.rate < 0.f);
    }
    EXPECT_NEAR(static_cast<double>(rev) / static_cast<double>(r.spawns.size()), prob, 0.04) << prob;
  }
}

TEST(Granular, ExtremeSettingsOnTinySourcesNeverReadOutOfBounds) {
  // Run under ASan for the real value of this test: random sources from 20 ms to 3 s, positions at both
  // edges, huge spray, +-24 st, reversed, Hermite, grains longer than the source.
  XorShift32 rng(77);
  for (int round = 0; round < 40; ++round) {
    Rig r;
    const double len = 0.02 + rng.uniform() * rng.uniform() * 3.0;
    r.source(0, demo::noise(len, 48000, 0.5f));
    for (int v = 0; v < 4; ++v) {
      r.v(v, "gate", 1);
      r.v(v, "source", 0);
      r.v(v, "position", rng.uniform() < 0.5f ? 0.f : (rng.uniform() < 0.5f ? 1.f : rng.uniform()));
      r.v(v, "size_ms", 5 + rng.uniform() * 495);
      r.v(v, "density", 5 + rng.uniform() * 1500);
      r.v(v, "spray_ms", rng.uniform() * 2000);
      r.v(v, "pitch_st", rng.bipolar() * 24);
      r.v(v, "pitch_rand_st", rng.uniform() * 12);
      r.v(v, "reverse_prob", rng.uniform());
      r.v(v, "quality", static_cast<float>(rng.nextU32() % 2));
      r.v(v, "spread", rng.uniform());
      r.v(v, "scan_on", static_cast<float>(rng.nextU32() % 2));
      r.v(v, "scan_speed", rng.bipolar() * 4);
    }
    const auto out = r.run(0.6, 100 + static_cast<int>(rng.nextU32() % 300));
    ASSERT_TRUE(testutil::allFinite(out)) << "round " << round << " source " << len << " s";
    ASSERT_LE(testutil::peak(out), 1.0f);
    ASSERT_TRUE(r.e.granularNode().pool().checkInvariants());
  }
}

TEST(Granular, OutputDoesNotDependOnTheCallbackSize) {
  auto renderWith = [](int cb) {
    Rig r;
    r.source(0, demo::chordPad(6.0, 48000));
    r.source(1, demo::drumLoop(120.0, 2, 48000));
    r.v(0, "gate", 1);
    r.v(0, "density", 120);
    r.v(0, "size_ms", 70);
    r.v(0, "spray_ms", 300);
    r.v(0, "jitter", 0.4f);
    r.v(0, "pitch_rand_st", 2);
    r.v(0, "spread", 0.8f);
    r.v(0, "reverse_prob", 0.2f);
    r.v(0, "quality", 1);
    r.v(1, "gate", 1);
    r.v(1, "source", 1);
    r.v(1, "sync_mode", 1);
    r.v(1, "sync_div", 2);
    r.v(1, "size_ms", 40);
    r.v(1, "env_shape", 2);
    return r.run(2.0, cb);
  };
  const auto ref = renderWith(192);
  ASSERT_GT(testutil::rms(ref), 0.01);
  for (int cb : {64, 96, 100, 128, 480, 1000}) {
    const auto out = renderWith(cb);
    ASSERT_EQ(out.size(), ref.size());
    // The first 20 ms differ: the gate attack ramp is stepped per block. Afterwards everything is identical
    // up to float summation order.
    double worst = 0.0;
    for (std::size_t i = 2 * 960; i < ref.size(); ++i) worst = std::max(worst, static_cast<double>(std::fabs(out[i] - ref[i])));
    EXPECT_LT(worst, 1e-4) << "callback size " << cb;
  }
}

TEST(Granular, AutoGainKeepsLoudnessRoughlyConstantAcrossDensities) {
  auto levelAt = [](float density) {
    Rig r;
    r.source(0, demo::chordPad(6.0, 48000));
    r.v(0, "gate", 1);
    r.v(0, "density", density);
    r.v(0, "size_ms", 100);
    r.v(0, "spray_ms", 1500);
    r.v(0, "volume", 1.0f);
    const auto out = r.run(2.0);
    return testutil::rms(out, 2 * 48000 / 2);
  };
  const double sparse = levelAt(10);
  const double dense = levelAt(600);
  ASSERT_GT(sparse, 1e-3);
  const double ratioDb = 20.0 * std::log10(dense / sparse);
  EXPECT_LT(std::fabs(ratioDb), 9.0) << "60x more grains must not be 60x louder (auto gain)";
}

// ---- adaptive governor integrated in the engine --------------------------------------------------

namespace {
std::uint64_t g_base = 0, g_busyNs = 0;
int g_calls = 0;
std::uint64_t fakeClock() noexcept {
  // Engine::process reads the clock twice per callback (start, end).
  if (g_calls++ % 2 == 0) return g_base;
  const std::uint64_t t = g_base + g_busyNs;
  g_base += 4'000'000;
  return t;
}
}  // namespace

TEST(Granular, AdaptiveGovernorShedsWorkUnderLoadAndRecovers) {
  EngineConfig cfg;
  cfg.realtime = true;
  cfg.clock = &fakeClock;
  Rig r(cfg);
  r.source(0, demo::chordPad(5.0, 48000));
  r.v(0, "gate", 1);
  r.v(0, "density", 400);
  r.v(0, "size_ms", 100);
  r.v(0, "quality", 1);  // Hermite requested

  auto avgGrains = [&](double seconds) {
    double acc = 0;
    int n = 0;
    for (int i = 0; i < static_cast<int>(seconds / 0.004); ++i) {
      r.run(0.004, 192);
      acc += r.e.granularNode().stats().activeGrains;
      ++n;
    }
    return acc / n;
  };

  g_calls = 0;
  g_base = 1000;
  g_busyNs = static_cast<std::uint64_t>(0.20 * 4e6);
  avgGrains(2.0);
  EXPECT_EQ(r.e.granularNode().stats().qualityLevel, 0);
  const double relaxed = avgGrains(1.0);
  EXPECT_GT(relaxed, 30.0);

  g_busyNs = static_cast<std::uint64_t>(0.85 * 4e6);  // pretend the device is struggling
  avgGrains(3.0);
  EXPECT_EQ(r.e.granularNode().stats().qualityLevel, 3);
  const double shed = avgGrains(1.0);
  EXPECT_LT(shed, relaxed * 0.62) << "density/grain cap reduced to 50 % at level 3";

  g_busyNs = static_cast<std::uint64_t>(0.15 * 4e6);
  avgGrains(9.0);
  EXPECT_EQ(r.e.granularNode().stats().qualityLevel, 0) << "recovered step by step";
  EXPECT_GT(avgGrains(1.0), relaxed * 0.9);

  // adaptive switched off: stays at full quality even when overloaded
  r.set("granular.adaptive", 0);
  g_busyNs = static_cast<std::uint64_t>(0.95 * 4e6);
  const double noAdapt = avgGrains(2.0);
  EXPECT_GT(noAdapt, relaxed * 0.9);
}

// Found by the stress test's fuzz phase: loop_start = loop_end = 1.0 made the loop window zero wide,
// the scan head became NaN and so did the output.
TEST(Granular, DegenerateLoopWindowsNeverProduceNaN) {
  for (float ls : {0.f, 0.5f, 1.f}) {
    for (float le : {0.f, 0.5f, 1.f}) {
      Rig r;
      r.source(0, demo::chordPad(3.0, 48000));
      r.v(0, "gate", 1);
      r.v(0, "scan_on", 1);
      r.v(0, "scan_speed", 3.f);
      r.v(0, "density", 200);
      r.v(0, "loop_start", ls);
      r.v(0, "loop_end", le);
      const auto out = r.run(1.0);
      EXPECT_TRUE(testutil::allFinite(out)) << "loop " << ls << " .. " << le;
      EXPECT_GT(testutil::peak(out), 0.f) << "loop " << ls << " .. " << le << " must still play";
      for (const Spawn& s : r.spawns) {
        ASSERT_TRUE(std::isfinite(s.pos));
        ASSERT_GE(s.pos, 0.0);
      }
    }
  }
}
