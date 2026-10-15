#include <gtest/gtest.h>

#include <atomic>
#include <thread>

#include "autodj/core/DemoSources.h"
#include "autodj/engine/Engine.h"
#include "test_util.h"

using namespace autodj;

namespace {

void armEverything(Engine& e) {
  ASSERT_TRUE(e.setSource(0, demo::chordPad(5.0, 48000)));
  ASSERT_TRUE(e.setSource(1, demo::drumLoop(120.0, 2, 48000)));
  ASSERT_TRUE(e.setSource(2, demo::noise(1.0, 48000, 0.4f)));
  for (int v = 0; v < 4; ++v) {
    testutil::setByKey(e, "granular.v" + std::to_string(v) + ".gate", 1);
    testutil::setByKey(e, "granular.v" + std::to_string(v) + ".source", static_cast<float>(v % 3));
    testutil::setByKey(e, "granular.v" + std::to_string(v) + ".density", 150);
  }
  testutil::setByKey(e, "tone.enabled", 1);
}

std::uint64_t g_t = 0, g_busy = 0;
int g_n = 0;
std::uint64_t fakeClock() noexcept {
  if (g_n++ % 2 == 0) return g_t;
  const std::uint64_t r = g_t + g_busy;
  g_t += 4'000'000;
  return r;
}

}  // namespace

TEST(Engine, ProcessNeverAllocatesOrFreesOnTheAudioThread) {
  testutil::ViolationCounter violations;
  Engine e;
  armEverything(e);
  XorShift32 rng(5);
  std::vector<float> buf(2 * 1100);
  for (int i = 0; i < 600; ++i) {
    // arbitrary control-thread activity between callbacks (this thread allocates there, which is fine)
    if (i % 7 == 0) e.params().setNormalized(static_cast<ParamId>(rng.nextU32() % e.registry().size()), rng.uniform());
    if (i % 13 == 0) e.noteOn(rng.nextU32() % 4, rng.bipolar() * 12, rng.uniform());
    if (i % 17 == 0) e.noteOff(rng.nextU32() % 4);
    if (i % 29 == 0) e.play();
    if (i % 31 == 0) e.seekBeats(rng.uniform() * 64);
    if (i % 101 == 0) e.setSource(static_cast<int>(rng.nextU32() % 4), demo::sine(100 + 50.0 * (i % 5), 0.5, 48000));
    if (i % 53 == 0) e.collectGarbage();
    const int frames = 1 + static_cast<int>(rng.nextU32() % 1100);
    e.process(buf.data(), frames);
    ASSERT_TRUE(std::isfinite(buf[0]));
  }
  EXPECT_EQ(violations.count(), 0u) << "an allocation/free happened inside Engine::process";
}

TEST(Engine, ConcurrentControlAndAudioThreadsStressTest) {
  testutil::ViolationCounter violations;
  Engine e;
  armEverything(e);
  std::atomic<bool> done{false};
  std::atomic<long> callbacks{0};
  bool finite = true;

  std::thread audio([&] {
    std::vector<float> buf(2 * 480);
    for (int i = 0; i < 4000; ++i) {
      e.process(buf.data(), 96 + (i % 5) * 96);
      for (float x : buf) finite &= std::isfinite(x);
      callbacks.fetch_add(1);
    }
    done = true;
  });
  std::thread control([&] {
    XorShift32 rng(11);
    EngineStatus st;
    GrainViz viz;
    while (!done) {
      e.params().setNormalized(static_cast<ParamId>(rng.nextU32() % e.registry().size()), rng.uniform());
      e.noteOn(rng.nextU32() % 4);
      e.noteOff(rng.nextU32() % 4);
      if (rng.uniform() < 0.05f) e.setSource(static_cast<int>(rng.nextU32() % 6), demo::sine(80 + rng.uniform() * 400, 0.3, 48000));
      if (rng.uniform() < 0.05f) {
        auto g = std::make_unique<GraphDesc>();
        g->add(&e.granularNode(), 1.f);
        e.publishGraph(std::move(g));
      }
      e.readStatus(st);
      e.readGrainViz(viz);
      e.collectGarbage();
    }
  });
  audio.join();
  control.join();
  EXPECT_TRUE(finite);
  EXPECT_EQ(violations.count(), 0u);
  EXPECT_EQ(callbacks.load(), 4000);
}

TEST(Engine, GraphSwapTakesEffectAndOldGraphIsReturned) {
  Engine e;
  testutil::setByKey(e, "tone.enabled", 1);
  testutil::setByKey(e, "tone.gain", 0.5f);
  auto before = testutil::render(e, 4800);
  EXPECT_GT(testutil::peak(before, 2400), 0.1f);

  // New graph without the tone node.
  auto g = std::make_unique<GraphDesc>();
  g->add(&e.granularNode(), 1.f);
  ASSERT_TRUE(e.publishGraph(std::move(g)));
  testutil::render(e, 4800);                                  // gets applied, master smoothing etc. settle
  const auto after = testutil::render(e, 4800);
  EXPECT_EQ(testutil::peak(after, 0), 0.f) << "tone node is no longer part of the graph";
  EXPECT_GE(e.collectGarbage(), 1u) << "old graph handed back for freeing off-thread";
}

TEST(Engine, StatusAndVisualisationSnapshotsArePublished) {
  Engine e;
  armEverything(e);
  e.play();
  testutil::render(e, 48000);
  EngineStatus st;
  ASSERT_TRUE(e.readStatus(st));
  EXPECT_EQ(st.sampleRate, 48000);
  EXPECT_NEAR(st.bpm, 120.0, 1e-9);
  EXPECT_EQ(st.playing, 1);
  EXPECT_NEAR(st.playBeat, 2.0, 0.02) << "1 s at 120 BPM = 2 beats";
  EXPECT_GT(st.activeGrains, 10);
  EXPECT_GT(st.callbacks, 100u);
  EXPECT_GT(st.peakL, 0.01f);
  EXPECT_FALSE(e.readStatus(st)) << "nothing new until the next callback";

  GrainViz viz;
  ASSERT_TRUE(e.readGrainViz(viz));
  EXPECT_GT(viz.count, 10);
  EXPECT_LE(viz.count, kVizMaxGrains);
  for (int i = 0; i < viz.count; ++i) {
    EXPECT_GE(viz.grains[i].pos, 0.f);
    EXPECT_LE(viz.grains[i].pos, 1.f);
    EXPECT_GE(viz.grains[i].age, 0.f);
    EXPECT_LE(viz.grains[i].age, 1.f);
    EXPECT_GE(viz.grains[i].pan, -1.f);
    EXPECT_LE(viz.grains[i].pan, 1.f);
  }
}

TEST(Engine, StatusCarriesThePositionInTheBarForTheUiPulse) {
  Engine e;
  testutil::render(e, 60000);  // 1.25 s at 120 BPM and 48 kHz: 2.5 beats on the free-running clock
  EngineStatus st;
  ASSERT_TRUE(e.readStatus(st));
  EXPECT_EQ(st.beatIndex, 2) << "beats 0,1,2 of the bar: the third beat is running";
  EXPECT_NEAR(st.beatPhase, 0.5f, 0.03f);
  EXPECT_EQ(st.beatFromMusic, 0) << "nothing plays: the clock is the free-running one";
  testutil::render(e, 48000);  // 1 s later: 4.5 beats, which is the second half of beat 0 of the next bar
  ASSERT_TRUE(e.readStatus(st));
  EXPECT_EQ(st.beatIndex, 0);
  EXPECT_NEAR(st.beatPhase, 0.5f, 0.03f);
}

TEST(Engine, TransportCommandsReachTheAudioThread) {
  Engine e;
  testutil::render(e, 24000);  // 0.5 s stopped: the clock free-runs, the play-head stays
  EngineStatus st;
  ASSERT_TRUE(e.readStatus(st));
  EXPECT_EQ(st.playing, 0);
  EXPECT_DOUBLE_EQ(st.playBeat, 0.0);
  EXPECT_NEAR(st.clockBeat, 1.0, 1e-6);

  e.seekBeats(8.0);
  e.play();
  testutil::render(e, 48000);
  ASSERT_TRUE(e.readStatus(st));
  EXPECT_NEAR(st.playBeat, 10.0, 0.02);
  e.stop();
  testutil::render(e, 4800);
  ASSERT_TRUE(e.readStatus(st));
  EXPECT_EQ(st.playing, 0);
  const double frozen = st.playBeat;
  testutil::render(e, 4800);
  ASSERT_TRUE(e.readStatus(st));
  EXPECT_DOUBLE_EQ(st.playBeat, frozen);

  testutil::setByKey(e, "transport.bpm", 150);
  e.play();
  testutil::render(e, 48000);
  ASSERT_TRUE(e.readStatus(st));
  EXPECT_NEAR(st.bpm, 150.0, 1e-9);
  EXPECT_NEAR(st.playBeat - frozen, 2.5, 0.02) << "1 s at 150 BPM";
}

TEST(Engine, CommandQueueAppliesBackPressureInsteadOfBlocking) {
  Engine e;
  int accepted = 0;
  while (e.noteOn(0)) {
    ++accepted;
    ASSERT_LE(accepted, 100000);
  }
  EXPECT_EQ(accepted, static_cast<int>(kCommandQueueSize));
  testutil::render(e, 192);  // drains up to 256 commands per callback
  EXPECT_TRUE(e.noteOn(0));
}

TEST(Engine, MasterOutputIsBoundedEvenWithExtremeSettings) {
  Engine e;
  armEverything(e);
  testutil::setByKey(e, "master.gain", 1.5f);
  testutil::setByKey(e, "tone.gain", 1.0f);
  for (int v = 0; v < 4; ++v) {
    testutil::setByKey(e, "granular.v" + std::to_string(v) + ".volume", 2.0f);
    testutil::setByKey(e, "granular.v" + std::to_string(v) + ".auto_gain", 0);
    testutil::setByKey(e, "granular.v" + std::to_string(v) + ".density", 600);
  }
  const auto out = testutil::render(e, 48000);
  EXPECT_TRUE(testutil::allFinite(out));
  EXPECT_LE(testutil::peak(out), 1.0f);
  EXPECT_GT(testutil::peak(out), 0.9f) << "the test must actually drive the limiter";
}

TEST(Engine, LoadMeterCountsDeadlineMissesAndXrunsAreForwarded) {
  EngineConfig cfg;
  cfg.clock = &fakeClock;
  Engine e(cfg);
  g_n = 0;
  g_t = 100;
  g_busy = 2'000'000;  // 50 % of a 192-frame budget
  testutil::render(e, 192 * 50);
  EngineStatus st;
  ASSERT_TRUE(e.readStatus(st));
  EXPECT_NEAR(st.load, 0.5f, 0.02f);
  EXPECT_EQ(st.deadlineMisses, 0u);

  g_busy = 6'000'000;  // 150 %
  testutil::render(e, 192 * 10);
  e.reportXruns(3);
  testutil::render(e, 192);
  ASSERT_TRUE(e.readStatus(st));
  EXPECT_EQ(st.deadlineMisses, 11u);
  EXPECT_GT(st.loadPeak, 1.4f);
  EXPECT_EQ(st.xruns, 3u);
}

TEST(Engine, ResetEngineSilencesGrainsAndReleasesTheirBuffers) {
  Engine e;
  armEverything(e);
  testutil::render(e, 9600);
  ASSERT_GT(e.granularNode().stats().activeGrains, 0);
  e.noteOn(0);
  testutil::setByKey(e, "tone.enabled", 0);
  testutil::setByKey(e, "granular.v0.gate", 0);
  testutil::setByKey(e, "granular.v1.gate", 0);
  testutil::setByKey(e, "granular.v2.gate", 0);
  testutil::setByKey(e, "granular.v3.gate", 0);
  e.resetEngine();
  const auto out = testutil::render(e, 4800);
  EXPECT_EQ(e.granularNode().stats().activeGrains, 0);
  EXPECT_EQ(testutil::peak(out, 2 * 480), 0.f);
  EXPECT_TRUE(e.granularNode().pool().checkInvariants());
}

// ------------------------------------------------------------------------------------------------ waveform overview

TEST(SourcePeaks, MinMaxPerColumnOfAKnownRamp) {
  std::vector<float> lr(2 * 1000);
  for (int i = 0; i < 1000; ++i) lr[2 * i] = lr[2 * i + 1] = static_cast<float>(i) / 1000.f;  // 0 .. 0.999
  const auto buf = SampleBuffer::fromInterleavedStereo(lr.data(), 1000, 48000, "ramp");
  std::vector<float> out(2 * 10);
  computePeaks(*buf, 10, out.data());
  for (int c = 0; c < 10; ++c) {
    EXPECT_NEAR(out[2 * c], c * 100 / 1000.f, 1e-6f) << c;
    EXPECT_NEAR(out[2 * c + 1], (c * 100 + 99) / 1000.f, 1e-6f) << c;
  }
}

TEST(SourcePeaks, MoreColumnsThanFramesStillFillsEveryColumn) {
  std::vector<float> lr = {0.5f, 0.5f, -0.5f, -0.5f, 0.25f, 0.25f};
  const auto buf = SampleBuffer::fromInterleavedStereo(lr.data(), 3, 48000, "tiny");
  std::vector<float> out(2 * 12, 9.f);
  computePeaks(*buf, 12, out.data());
  for (float v : out) EXPECT_LE(std::fabs(v), 0.5f);
}

TEST(SourcePeaks, EngineReportsSlotContentAndRejectsEmptyOrInvalidSlots) {
  Engine e;
  std::vector<float> out(2 * 64, 0.f);
  EXPECT_FALSE(e.sourcePeaks(0, 64, out.data())) << "nothing loaded yet";
  ASSERT_TRUE(e.setSource(3, demo::sine(220.0, 2.0, 48000, 0.5f)));
  EXPECT_TRUE(e.sourcePeaks(3, 64, out.data()));
  for (int c = 0; c < 64; ++c) {
    EXPECT_NEAR(out[2 * c], -0.5f, 0.01f);
    EXPECT_NEAR(out[2 * c + 1], 0.5f, 0.01f);
  }
  EXPECT_FALSE(e.sourcePeaks(16, 64, out.data()));
  EXPECT_FALSE(e.sourcePeaks(-1, 64, out.data()));
  double seconds = 0.0;
  int sr = 0;
  std::string name;
  ASSERT_TRUE(e.sourceInfo(3, &seconds, &sr, &name));
  EXPECT_NEAR(seconds, 2.0, 1e-6);
  EXPECT_EQ(sr, 48000);
  EXPECT_EQ(name, "sine");
}
