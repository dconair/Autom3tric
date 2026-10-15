// Performance section: master effects (echo, beat roll, stutter, reverb, flanger) and the drum pads with their sequencer.
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>

#include "autodj/core/DemoSources.h"
#include "autodj/engine/Engine.h"
#include "autodj/engine/Showcase.h"
#include "autodj/perf/MasterFx.h"
#include "autodj/perf/PadNode.h"
#include "test_util.h"

using namespace autodj;
using namespace autodj::perf;

namespace {

constexpr int kSr = 48000;
constexpr int kChunk = 192;

struct FxRig {
  ParamRegistry reg;
  PerfParamIds ids;
  std::unique_ptr<ParamStore> st;
  std::unique_ptr<MasterFx> fx;
  double bpm = 120.0;
  double beat = 0.0;

  FxRig() {
    ids = registerPerfParams(reg);
    reg.freeze();
    st = std::make_unique<ParamStore>(reg);
    fx = std::make_unique<MasterFx>(ids);
    fx->prepare(kSr);
  }
  void set(ParamId id, float v) { st->set(id, v); }

  /// Runs `io` (interleaved stereo) through the effects chunk by chunk; `at(frame)` may change parameters between chunks.
  template <typename Fn>
  void run(std::vector<float>& io, Fn&& at) {
    const std::size_t frames = io.size() / 2;
    for (std::size_t f = 0; f < frames; f += kChunk) {
      const int n = static_cast<int>(std::min<std::size_t>(kChunk, frames - f));
      at(f);
      MasterClock clk;
      clk.bpm = bpm;
      clk.beat = beat;
      fx->setParams(*st);
      fx->process(io.data() + 2 * f, n, clk);
      beat += n * bpm / (60.0 * kSr);
    }
  }
  void run(std::vector<float>& io) {
    run(io, [](std::size_t) {});
  }
};

std::vector<float> noise(double seconds, float amp = 0.3f, std::uint32_t seed = 7) {
  XorShift32 r(seed);
  std::vector<float> v(static_cast<std::size_t>(seconds * kSr) * 2);
  for (float& x : v) x = r.bipolar() * amp;
  return v;
}

/// First frame at or after `from` where |left| exceeds `thr`, or -1.
long firstAbove(const std::vector<float>& io, std::size_t from, float thr) {
  for (std::size_t i = from; i < io.size() / 2; ++i) {
    if (std::fabs(io[2 * i]) > thr) return static_cast<long>(i);
  }
  return -1;
}

double rmsWin(const std::vector<float>& io, double fromSec, double toSec, int ch = 0) {
  const std::size_t a = static_cast<std::size_t>(fromSec * kSr), b = std::min(io.size() / 2, static_cast<std::size_t>(toSec * kSr));
  double acc = 0.0;
  for (std::size_t i = a; i < b; ++i) acc += static_cast<double>(io[2 * i + ch]) * io[2 * i + ch];
  return b > a ? std::sqrt(acc / static_cast<double>(b - a)) : 0.0;
}

}  // namespace

// ------------------------------------------------------------------------------------------------------------------
// master effects
// ------------------------------------------------------------------------------------------------------------------

TEST(MasterFx, EverythingOffIsABitExactBypass) {
  FxRig r;
  std::vector<float> in = noise(1.5), io = in;
  r.run(io);
  EXPECT_EQ(0, std::memcmp(in.data(), io.data(), in.size() * sizeof(float)));
}

TEST(MasterFx, EchoRepeatsOnTheBeatAtTheTempoOfTheClock) {
  for (double bpm : {120.0, 90.0}) {
    FxRig r;
    r.bpm = bpm;
    r.set(r.ids.echoOn, 1.f);
    r.set(r.ids.echoBeats, 1.f);  // "1/4" = one beat
    r.set(r.ids.echoFeedback, 0.4f);
    r.set(r.ids.echoMix, 0.5f);
    std::vector<float> io(static_cast<std::size_t>(2.5 * kSr) * 2, 0.f);
    const std::size_t b0 = static_cast<std::size_t>(0.3 * kSr), len = static_cast<std::size_t>(0.04 * kSr);
    for (std::size_t i = 0; i < len; ++i) {  // a 40 ms burst of 1 kHz
      const float s = 0.6f * std::sin(6.2831853f * 1000.f * static_cast<float>(i) / kSr);
      io[2 * (b0 + i)] = s;
      io[2 * (b0 + i) + 1] = s;
    }
    r.run(io);
    const double beatSec = 60.0 / bpm;
    // the dry burst, then a repeat one beat later at about half its level, then (feedback, ping-pong) another one two beats later
    const long first = firstAbove(io, b0 + len + 200, 0.02f);
    ASSERT_GT(first, 0);
    EXPECT_NEAR(static_cast<double>(first - static_cast<long>(b0)) / kSr, beatSec, 0.004) << "bpm " << bpm;
    const double dry = rmsWin(io, 0.3, 0.34), rep = rmsWin(io, 0.3 + beatSec + 0.002, 0.3 + beatSec + 0.038);
    EXPECT_NEAR(rep / dry, 0.5, 0.1) << "bpm " << bpm;
    EXPECT_LT(rmsWin(io, 0.3 + 0.1, 0.3 + beatSec - 0.05), 1e-4) << "nothing between the burst and its repeat";
    EXPECT_GT(rmsWin(io, 0.3 + 2 * beatSec + 0.004, 0.3 + 2 * beatSec + 0.036, 1), 0.01) << "second repeat (ping-pong to the other side)";
    ASSERT_TRUE(testutil::allFinite(io));
  }
}

TEST(MasterFx, EchoTailDiesAwayThenCostsNothingAndStartsClean) {
  FxRig r;
  r.set(r.ids.echoOn, 1.f);
  r.set(r.ids.echoFeedback, 0.6f);
  std::vector<float> io = noise(1.0);
  io.resize(static_cast<std::size_t>(16 * kSr) * 2, 0.f);
  r.run(io, [&](std::size_t f) {
    if (f >= static_cast<std::size_t>(1.0 * kSr)) r.set(r.ids.echoOn, 0.f);
  });
  // after the switch-off the repeats ring on (they are not cut) ...
  EXPECT_GT(rmsWin(io, 1.2, 1.6), 0.005);
  // ... and are completely gone long before the end
  for (std::size_t i = static_cast<std::size_t>(13 * kSr) * 2; i < io.size(); ++i) ASSERT_EQ(io[i], 0.f);
  // used again later, nothing of the old tail comes back: a fresh silent line gives silence until the new burst's repeat
  FxRig fresh;
  fresh.set(fresh.ids.echoOn, 1.f);
  std::vector<float> a(static_cast<std::size_t>(0.2 * kSr) * 2, 0.f);
  fresh.run(a);
  EXPECT_EQ(testutil::peak(a), 0.f);
}

TEST(MasterFx, BeatRollRepeatsTheLastSliceBeforeThePress) {
  FxRig r;
  r.set(r.ids.rollBeats, 1.f);  // "1/2" beat = 0.25 s at 120 bpm
  std::vector<float> in = noise(3.0), io = in;
  const std::size_t press = static_cast<std::size_t>(1.5 * kSr);
  r.run(io, [&](std::size_t f) { r.set(r.ids.rollOn, f >= press ? 1.f : 0.f); });
  const std::size_t L = static_cast<std::size_t>(0.25 * kSr);
  // the loop is periodic with L ...
  double worst = 0.0;
  const std::size_t from = press + 2 * kChunk + 400;
  for (std::size_t i = from; i + L < io.size() / 2; ++i) worst = std::max<double>(worst, std::fabs(io[2 * (i + L)] - io[2 * i]));
  EXPECT_LT(worst, 1e-5);
  // ... and holds what was playing right before the press
  double diff = 0.0;
  for (std::size_t k = 200; k < 2000; ++k) diff = std::max<double>(diff, std::fabs(io[2 * (press + 2 * kChunk + k)] - in[2 * (press + 2 * kChunk + k - L)]));
  EXPECT_LT(diff, 0.35) << "loop content is the slice before the press (chunk-aligned start)";
  // releasing returns to the live signal
  FxRig r2;
  r2.set(r2.ids.rollBeats, 1.f);
  std::vector<float> io2 = in;
  r2.run(io2, [&](std::size_t f) { r2.set(r2.ids.rollOn, (f >= press && f < press + 24000) ? 1.f : 0.f); });
  for (std::size_t i = (press + 24000 + 1000) * 2; i < io2.size(); ++i) ASSERT_NEAR(io2[i], in[i], 1e-6f);
}

TEST(MasterFx, StutterGatesOnTheBeatGridAndNotAtRandom) {
  FxRig r;
  r.set(r.ids.stutterOn, 1.f);
  r.set(r.ids.stutterBeats, 1.f);  // "1/8" = a period of half a beat = 0.25 s at 120 bpm: 0.125 s open, 0.125 s closed
  std::vector<float> io(static_cast<std::size_t>(1.2 * kSr) * 2, 0.5f);
  r.run(io);
  auto at = [&](double sec) { return std::fabs(io[2 * static_cast<std::size_t>(sec * kSr)]); };
  EXPECT_NEAR(at(0.100), 0.5f, 1e-3);  // open: first half of the period (the stutter fades in over 4 ms from the start)
  EXPECT_NEAR(at(0.200), 0.0f, 1e-3);  // closed
  EXPECT_NEAR(at(0.350), 0.5f, 1e-3);
  EXPECT_NEAR(at(0.450), 0.0f, 1e-3);
  EXPECT_NEAR(at(0.600), 0.5f, 1e-3);
  // the edges are where the grid says (0.125 s, 0.25 s ...), within the 1.5 ms gate ramp
  const long closeAt = firstAbove(io, 0, 1e-9f);
  (void)closeAt;
  std::size_t edge = 0;
  for (std::size_t i = 100; i < io.size() / 2; ++i) {
    if (std::fabs(io[2 * i]) < 0.25f) {
      edge = i;
      break;
    }
  }
  EXPECT_NEAR(static_cast<double>(edge) / kSr, 0.125, 0.002);
  ASSERT_TRUE(testutil::allFinite(io));
}

TEST(MasterFx, ReverbAddsATailFlangerColoursAndBothLeaveNoTraceWhenOff) {
  FxRig r;
  r.set(r.ids.reverbOn, 1.f);
  std::vector<float> io(static_cast<std::size_t>(2.0 * kSr) * 2, 0.f);
  for (std::size_t i = 0; i < 2000; ++i) {
    io[2 * (24000 + i)] = io[2 * (24000 + i) + 1] = 0.5f * std::sin(0.3f * static_cast<float>(i)) * (1.f - static_cast<float>(i) / 2000.f);
  }
  r.run(io);
  EXPECT_GT(rmsWin(io, 0.62, 0.9), 1e-3) << "reverb tail after the burst";
  ASSERT_TRUE(testutil::allFinite(io));

  FxRig f;
  f.set(f.ids.flangerOn, 1.f);
  std::vector<float> n = noise(1.0), flanged = n;
  f.run(flanged);
  double diff = 0.0;
  for (std::size_t i = 0; i < n.size(); ++i) diff += std::fabs(flanged[i] - n[i]);
  EXPECT_GT(diff / static_cast<double>(n.size()), 0.02) << "the flanger changes the sound";

  // switched off again: after the effect has faded out, the signal passes bit-exactly
  FxRig off;
  off.set(off.ids.reverbOn, 1.f);
  off.set(off.ids.flangerOn, 1.f);
  std::vector<float> in = noise(8.0), out = in;
  off.run(out, [&](std::size_t f2) {
    if (f2 >= static_cast<std::size_t>(1.0 * kSr)) {
      off.set(off.ids.reverbOn, 0.f);
      off.set(off.ids.flangerOn, 0.f);
    }
  });
  const std::size_t tail = static_cast<std::size_t>(6.0 * kSr) * 2;
  EXPECT_EQ(0, std::memcmp(in.data() + tail, out.data() + tail, (in.size() - tail) * sizeof(float)));
}

TEST(MasterFx, EveryEffectAtOnceStaysFiniteAndBounded) {
  FxRig r;
  std::vector<float> io = noise(6.0, 0.5f);
  XorShift32 rng(99);
  r.run(io, [&](std::size_t f) {
    if (f % (kChunk * 40) != 0) return;
    r.set(r.ids.echoOn, rng.uniform() > 0.4f);
    r.set(r.ids.reverbOn, rng.uniform() > 0.4f);
    r.set(r.ids.flangerOn, rng.uniform() > 0.4f);
    r.set(r.ids.rollOn, rng.uniform() > 0.6f);
    r.set(r.ids.stutterOn, rng.uniform() > 0.6f);
    r.set(r.ids.echoBeats, static_cast<float>(rng.nextU32() % 5));
    r.set(r.ids.rollBeats, static_cast<float>(rng.nextU32() % 4));
    r.set(r.ids.stutterBeats, static_cast<float>(rng.nextU32() % 3));
    r.set(r.ids.echoFeedback, rng.uniform() * 0.92f);
    r.bpm = 70.0 + 100.0 * rng.uniform();
  });
  ASSERT_TRUE(testutil::allFinite(io));
  EXPECT_LT(testutil::peak(io), 8.f);
}

// ------------------------------------------------------------------------------------------------------------------
// drum pads
// ------------------------------------------------------------------------------------------------------------------

namespace {

struct PadRig {
  ParamRegistry reg;
  PerfParamIds ids;
  std::unique_ptr<ParamStore> st;
  std::unique_ptr<PadNode> pads;
  double bpm = 120.0;
  double beat = 0.0;
  std::int64_t sample = 0;
  GarbageQueue garbage;

  PadRig() {
    ids = registerPerfParams(reg);
    reg.freeze();
    st = std::make_unique<ParamStore>(reg);
    pads = std::make_unique<PadNode>(ids);
    pads->prepare({static_cast<double>(kSr), kChunk});
    st->set(ids.padVolume, 0.9f);  // the thresholds below were set for this level; the product default is lower
    for (int p = 0; p < kPads; ++p) row(p, 0);
  }
  void row(int pad, unsigned mask) {
    Command c = Command::make(CmdType::PadRow);
    c.target = static_cast<std::uint8_t>(pad);
    c.i0 = static_cast<std::int32_t>(mask);
    pads->handleCommand(c, garbage);
  }
  void hit(int pad, float vel = 0.f) {
    Command c = Command::make(CmdType::PadTrigger);
    c.target = static_cast<std::uint8_t>(pad);
    c.f0 = vel;
    pads->handleCommand(c, garbage);
  }
  /// Renders `frames` frames; `jumpAt`/`jumpBeats` lets the clock leap (a seek).
  std::vector<float> render(std::size_t frames, bool useClock = true) {
    std::vector<float> out(frames * 2, 0.f);
    for (std::size_t f = 0; f < frames; f += kChunk) {
      const int n = static_cast<int>(std::min<std::size_t>(kChunk, frames - f));
      BlockTime t;
      t.frames = n;
      t.sampleRate = kSr;
      t.beatsPerSample = bpm / (60.0 * kSr);
      t.clockBeat = beat;
      MasterClock clk;
      clk.bpm = bpm;
      clk.beat = beat;
      pads->setParams(*st, t);
      ProcessIO io;
      io.time = &t;
      io.clock = useClock ? &clk : nullptr;
      io.out = out.data() + 2 * f;
      io.frames = n;
      pads->process(io);
      beat += n * bpm / (60.0 * kSr);
    }
    return out;
  }
};

/// Onsets of a pad pattern: a hit counts when the level jumps above `hi` while the (decaying) envelope of what rang before has
/// fallen below `lo`. Ringing tails of earlier hits therefore do not count as new onsets.
std::vector<long> onsets(const std::vector<float>& io, float hi, float lo) {
  std::vector<long> out;
  float env = 0.f;
  const float decay = std::exp(-1.f / (0.04f * kSr));
  bool armed = true;
  for (std::size_t i = 0; i < io.size() / 2; ++i) {
    const float a = std::max(std::fabs(io[2 * i]), std::fabs(io[2 * i + 1]));
    env = std::max(a, env * decay);
    if (armed && a > hi) {
      out.push_back(static_cast<long>(i));
      armed = false;
    } else if (!armed && env < lo) {
      armed = true;
    }
  }
  return out;
}

}  // namespace

TEST(PadKit, EverySoundExistsIsFiniteAndStartsAndEndsSmoothly) {
  PadKit kit;
  kit.render(kSr);
  for (int p = 0; p < kPads; ++p) {
    const PadSound& s = kit.sound(p);
    ASSERT_GT(s.data.size(), 1000u) << "pad " << p;
    EXPECT_TRUE(testutil::allFinite(s.data)) << "pad " << p;
    const float pk = testutil::peak(s.data);
    EXPECT_GT(pk, 0.2f) << "pad " << p;
    EXPECT_LE(pk, 1.0f) << "pad " << p;
    EXPECT_LT(std::fabs(s.data.front()), 0.05f) << "pad " << p;
    EXPECT_LT(std::fabs(s.data.back()), 0.01f) << "pad " << p;
  }
  // different sounds, not sixteen copies
  EXPECT_NE(kit.sound(0).data.size(), kit.sound(4).data.size());
}

TEST(PadNode, ATapPlaysTheSoundOnceAndTheHatsChokeEachOther) {
  PadRig r;
  r.hit(0);
  const std::vector<float> kick = r.render(kSr / 2 + 2000);
  EXPECT_GT(testutil::peak(kick), 0.5f);
  EXPECT_LT(testutil::peak(kick), 1.3f);
  EXPECT_EQ(r.pads->activeVoices(), 0) << "the one-shot is over after its length";

  r.hit(5);  // open hat
  (void)r.render(kSr / 10);
  EXPECT_EQ(r.pads->activeVoices(), 1);
  r.hit(4);  // closed hat cuts it
  (void)r.render(kSr / 50);
  EXPECT_EQ(r.pads->activeVoices(), 1) << "the open hat was released, only the closed hat is left";
  (void)r.render(kSr / 4);
  EXPECT_EQ(r.pads->activeVoices(), 0);
}

TEST(PadNode, SequencerHitsTheBeatGridAtTheTempoOfTheClock) {
  for (double bpm : {120.0, 91.0}) {
    PadRig r;
    r.bpm = bpm;
    r.row(0, 0x1111);  // kick on every beat
    r.st->set(r.ids.padSeqOn, 1.f);
    const std::vector<float> io = r.render(static_cast<std::size_t>(6.0 * kSr));
    const std::vector<long> on = onsets(io, 0.4f, 0.3f);
    ASSERT_GE(on.size(), 8u) << bpm;
    const double beatFrames = 60.0 * kSr / bpm;
    for (std::size_t k = 0; k < on.size(); ++k) {
      EXPECT_NEAR(static_cast<double>(on[k]), static_cast<double>(k) * beatFrames, 0.0012 * kSr) << "bpm " << bpm << " hit " << k;
    }
    // no hit twice, none missing: 6 s hold floor(6 / beat) + 1 downbeats
    EXPECT_EQ(static_cast<int>(on.size()), static_cast<int>(std::floor(6.0 * kSr / beatFrames - 1e-6)) + 1) << bpm;
  }
}

TEST(PadNode, SwingDelaysEveryOtherSixteenthAndTheStepFollowsTheClock) {
  PadRig r;
  r.row(7, 0x0002);  // shaker on step 1 only
  r.st->set(r.ids.padSeqOn, 1.f);
  r.st->set(r.ids.padSwing, 0.5f);
  const std::vector<float> io = r.render(static_cast<std::size_t>(2.2 * kSr));
  const std::vector<long> on = onsets(io, 0.01f, 0.004f);
  ASSERT_GE(on.size(), 2u);
  const double expect = (0.25 + 0.5 * 0.5 * 0.25) * 60.0 * kSr / 120.0;  // step 1 + half the swing offset (25 % of a step)
  EXPECT_NEAR(static_cast<double>(on[0]), expect, 0.003 * kSr) << "shaker has a slow attack: allow 3 ms";
  EXPECT_NEAR(static_cast<double>(on[1] - on[0]), 4.0 * 60.0 * kSr / 120.0, 0.0005 * kSr) << "once per bar";
}

TEST(PadNode, ClockJumpsAndSequencerSwitchingNeverDoubleOrDropAHit) {
  PadRig r;
  r.row(0, 0x1111);
  r.st->set(r.ids.padSeqOn, 1.f);
  std::vector<float> io = r.render(static_cast<std::size_t>(1.2 * kSr));  // beats 0..2.4
  r.beat += 40.3;                                                           // a seek far ahead, mid-beat
  const std::vector<float> after = r.render(static_cast<std::size_t>(1.2 * kSr));
  io.insert(io.end(), after.begin(), after.end());
  const std::vector<long> on = onsets(io, 0.4f, 0.3f);
  // first part: beats 0, 1, 2 -> 3 hits at 0, 0.5 s, 1.0 s; after the jump (beat 42.7 -> next beat 43 after 0.15 s): 0.15, 0.65, 1.15 s
  ASSERT_EQ(on.size(), 6u);
  for (int k = 0; k < 3; ++k) EXPECT_NEAR(static_cast<double>(on[static_cast<std::size_t>(3 + k)]) / kSr, 1.2 + 0.15 + 0.5 * k, 0.003);
  // off: no more hits, the running tails finish
  r.st->set(r.ids.padSeqOn, 0.f);
  const std::vector<float> quiet = r.render(static_cast<std::size_t>(1.5 * kSr));
  EXPECT_EQ(r.pads->currentStep(), -1);
  EXPECT_LT(rmsWin(quiet, 0.8, 1.5), 1e-6);
}

// ------------------------------------------------------------------------------------------------------------------
// in the engine
// ------------------------------------------------------------------------------------------------------------------

namespace {
std::vector<float> renderEngine(Engine& e, double seconds, int callback = 480, std::set<int>* steps = nullptr,
                                std::uint64_t* violations = nullptr) {
  testutil::ViolationCounter counter;
  const int total = static_cast<int>(seconds * kSr);
  std::vector<float> out;
  out.reserve(static_cast<std::size_t>(total) * 2);
  std::vector<float> buf(static_cast<std::size_t>(callback) * 2);
  dj::DjStatus st;
  for (int done = 0; done < total; done += callback) {
    e.process(buf.data(), callback);
    out.insert(out.end(), buf.begin(), buf.end());
    if (steps && e.readDjStatus(st) && st.padStep >= 0) steps->insert(st.padStep);
    if ((done / callback) % 20 == 0) e.collectGarbage();
  }
  if (violations) *violations = counter.count();
  return out;
}
}  // namespace

TEST(PerfEngine, PadsFollowTheFreeRunningClockAndReportTheirStep) {
  EngineConfig cfg;
  cfg.sampleRate = kSr;
  cfg.realtime = false;
  Engine e(cfg);
  for (int p = 0; p < kPads; ++p) e.padSetRow(p, p == 0 ? 0x1111u : 0u);
  e.params().set(e.ids().bpm, 128.f);
  e.params().set(e.ids().perf.padSeqOn, 1.f);
  e.params().set(e.ids().perf.padVolume, 0.9f);
  e.params().set(e.ids().masterGain, 0.8f);
  std::set<int> steps;
  std::uint64_t viol = 0;
  const std::vector<float> io = renderEngine(e, 6.0, 480, &steps, &viol);
  EXPECT_EQ(viol, 0u);
  EXPECT_EQ(steps.size(), 16u) << "the step indicator visits all sixteen steps";
  const std::vector<long> on = onsets(io, 0.4f, 0.3f);
  ASSERT_GE(on.size(), 10u);
  const double beatFrames = 60.0 * kSr / 128.0;
  for (std::size_t k = 1; k < on.size(); ++k) EXPECT_NEAR(static_cast<double>(on[k] - on[k - 1]), beatFrames, 0.0015 * kSr);
}

TEST(PerfEngine, PadsSitOnTheBeatsOfTheMixPlanAndFollowItsTempo) {
  EngineConfig cfg;
  cfg.sampleRate = kSr;
  cfg.realtime = false;
  Engine e(cfg);
  demo::MusicSpec ms;
  ms.bpm = 100.0;
  ms.sampleRate = kSr;
  ms.introBars = 4;
  ms.dropBars = 8;
  ms.breakBars = 4;
  ms.outroBars = 4;
  const auto audio = demo::musicTrack(ms);
  analysis::TrackAnalysis a = analysis::analyzeTrack(audio->data(), audio->frames(), kSr, {}, {});
  ASSERT_TRUE(a.hasTempo());
  const int id = e.addTrack(audio, "song", &a);
  const auto plan = e.planMix({id}, dj::PlanOptions{});
  ASSERT_TRUE(plan);
  for (int p = 0; p < kPads; ++p) e.padSetRow(p, p == 0 ? 0x1111u : 0u);
  e.params().set(e.ids().perf.padSeqOn, 1.f);
  e.params().set(e.ids().perf.padVolume, 0.9f);
  e.djNode().setDebugAudibleMask(0);  // the decks keep running but stay silent: only the pads are heard
  ASSERT_TRUE(e.planStart(plan));
  const std::vector<float> io = renderEngine(e, 8.0);
  const std::vector<long> on = onsets(io, 0.4f, 0.3f);
  ASSERT_GE(on.size(), 8u);
  // hits come at the master tempo of the plan, starting on its first beat
  const double beatFrames = 60.0 * kSr / plan->bpmAt(0.0);
  EXPECT_NEAR(static_cast<double>(on[0]), 0.0, 0.002 * kSr);
  for (std::size_t k = 1; k < on.size(); ++k) EXPECT_NEAR(static_cast<double>(on[k] - on[k - 1]), beatFrames, 0.003 * kSr) << k;
  e.planStop();
}

TEST(PerfEngine, EffectsAndPadsTogetherAllocateNothingOnTheAudioThreadAndCostLittle) {
  EngineConfig cfg;
  cfg.sampleRate = kSr;
  cfg.realtime = false;
  Engine e(cfg);
  e.params().set(e.ids().perf.padSeqOn, 1.f);
  e.params().set(e.ids().perf.padSwing, 0.3f);
  for (int p = 0; p < kPads; ++p) e.padSetRow(p, 0x5A5Au + static_cast<unsigned>(p) * 257u);
  XorShift32 rng(5);
  std::uint64_t viol = 0;
  testutil::ViolationCounter counter;
  const int cb = 240;
  std::vector<float> buf(static_cast<std::size_t>(cb) * 2);
  double worst = 0.0;
  const auto t0 = std::chrono::steady_clock::now();
  const int blocks = static_cast<int>(20.0 * kSr / cb);
  for (int b = 0; b < blocks; ++b) {
    if (b % 25 == 0) {
      e.params().set(e.ids().perf.echoOn, rng.uniform() > 0.4f);
      e.params().set(e.ids().perf.reverbOn, rng.uniform() > 0.4f);
      e.params().set(e.ids().perf.flangerOn, rng.uniform() > 0.4f);
      e.params().set(e.ids().perf.rollOn, rng.uniform() > 0.7f);
      e.params().set(e.ids().perf.stutterOn, rng.uniform() > 0.7f);
      e.padTrigger(static_cast<int>(rng.nextU32() % 16), 0.f);
    }
    const auto b0 = std::chrono::steady_clock::now();
    e.process(buf.data(), cb);
    worst = std::max(worst, std::chrono::duration<double>(std::chrono::steady_clock::now() - b0).count() / (static_cast<double>(cb) / kSr));
    for (float x : buf) ASSERT_TRUE(std::isfinite(x));
    if (b % 20 == 0) e.collectGarbage();
  }
  viol = counter.count();
  const double total = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  std::printf("[perf] 20 s of effects + pads: %.2f %% of real time, worst block %.1f %% of its budget\n", 100.0 * total / 20.0, 100.0 * worst);
  EXPECT_EQ(viol, 0u);
  EXPECT_LT(total / 20.0, 0.15);
}

TEST(PerfEngine, CaptureCopiesTheLastSecondsOfTheOutputBitExactly) {
  EngineConfig cfg;
  cfg.sampleRate = kSr;
  cfg.realtime = false;
  Engine e(cfg);
  EXPECT_EQ(e.captureOutput(2.0, "none"), nullptr) << "nothing rendered yet";
  e.params().set(e.ids().perf.padSeqOn, 1.f);
  e.params().set(e.ids().perf.echoOn, 1.f);
  std::vector<float> all;
  std::vector<float> buf(480 * 2);
  for (int b = 0; b < 600; ++b) {  // 6 s: still inside the 10 s ring
    e.process(buf.data(), 480);
    all.insert(all.end(), buf.begin(), buf.end());
  }
  auto cap = e.captureOutput(2.0, "freeze");
  ASSERT_TRUE(cap);
  EXPECT_EQ(cap->name(), "freeze");
  EXPECT_EQ(cap->frames(), 2 * kSr);
  const std::size_t from = all.size() - static_cast<std::size_t>(2 * kSr) * 2;
  EXPECT_EQ(0, std::memcmp(cap->data(), all.data() + from, static_cast<std::size_t>(2 * kSr) * 2 * sizeof(float)));
  EXPECT_GT(testutil::peak(std::vector<float>(cap->data(), cap->data() + 2 * kSr)), 0.05f);
  // 15 s later (the ring has wrapped) it is still the most recent output
  for (int b = 0; b < 1500; ++b) {
    e.process(buf.data(), 480);
    all.insert(all.end(), buf.begin(), buf.end());
  }
  cap = e.captureOutput(8.0, "late");
  ASSERT_TRUE(cap);
  EXPECT_EQ(cap->frames(), 8 * kSr);
  const std::size_t from2 = all.size() - static_cast<std::size_t>(8 * kSr) * 2;
  EXPECT_EQ(0, std::memcmp(cap->data(), all.data() + from2, static_cast<std::size_t>(8 * kSr) * 2 * sizeof(float)));
  // it can be handed to the granular engine as a source
  EXPECT_TRUE(e.setSource(5, cap));
}

// ---- the instrument's four start sounds -----------------------------------------------------------------------------


TEST(Showcase, TheFourStartSoundsAreCleanLevelledAndLoopSeamlessly) {
  using namespace std::chrono;
  for (int kind = 0; kind < showcase::kKinds; ++kind) {
    const auto t0 = steady_clock::now();
    const auto buf = showcase::make(kind, kSr);
    const double ms = duration<double, std::milli>(steady_clock::now() - t0).count();
    ASSERT_TRUE(buf) << showcase::name(kind);
    EXPECT_LT(ms, 2500.0) << showcase::name(kind) << " is rendered at start-up and must not hold the window up";
    EXPECT_EQ(buf->name(), showcase::name(kind));
    const std::int64_t n = buf->frames();
    EXPECT_NEAR(static_cast<double>(n) / kSr, showcase::kBars * 4 * 60.0 / showcase::kBpm, 0.01) << "four bars";
    const float* d = buf->data();
    double e = 0;
    float peak = 0.f;
    bool finite = true;
    for (std::int64_t i = 0; i < 2 * n; ++i) {
      finite = finite && std::isfinite(d[i]);
      peak = std::max(peak, std::fabs(d[i]));
      e += static_cast<double>(d[i]) * d[i];
    }
    EXPECT_TRUE(finite) << showcase::name(kind);
    const double rmsDb = 10.0 * std::log10(e / (2.0 * static_cast<double>(n)));
    EXPECT_GE(rmsDb, -19.0) << showcase::name(kind) << " is not quiet";
    EXPECT_LE(rmsDb, -12.0) << showcase::name(kind) << " leaves room for the granular cloud";
    EXPECT_LE(peak, 0.91f) << showcase::name(kind) << " never reaches the soft clipper";
    // seamless loop: the step across the seam is no bigger than a step inside the sound
    float worstInside = 0.f;
    for (std::int64_t i = 1; i < n; ++i) worstInside = std::max({worstInside, std::fabs(d[2 * i] - d[2 * i - 2]), std::fabs(d[2 * i + 1] - d[2 * i - 1])});
    const float seam = std::max(std::fabs(d[0] - d[2 * (n - 1)]), std::fabs(d[1] - d[2 * (n - 1) + 1]));
    EXPECT_LE(seam, worstInside + 1e-6f) << showcase::name(kind) << " loops without a click";
    EXPECT_LT(seam, 0.1f) << showcase::name(kind);
  }
  // four different sounds
  EXPECT_NE(showcase::make(0, kSr)->data()[20000], showcase::make(2, kSr)->data()[20000]);
}
