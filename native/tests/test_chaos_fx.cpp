// Unit tests of the Chaos effect (reverb -> flanger macro) in isolation.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "autodj/core/Dsp.h"
#include "autodj/core/RtGuard.h"
#include "autodj/fx/ChaosFx.h"
#include "test_util.h"

using autodj::fx::ChaosFx;

namespace {

constexpr int kBlock = 192;
constexpr double kSr = 48000.0;

// Runs `fx` over `in` (interleaved stereo) in blocks, with a constant amount.
std::vector<float> runConst(ChaosFx& fx, std::vector<float> in, float amount) {
  for (std::size_t i = 0; i < in.size(); i += 2 * kBlock) {
    const int n = static_cast<int>(std::min<std::size_t>(kBlock, (in.size() - i) / 2));
    fx.process(in.data() + i, n, amount, amount);
  }
  return in;
}

std::vector<float> sine(double hz, double seconds, float amp = 0.3f) {
  std::vector<float> v(static_cast<std::size_t>(seconds * kSr) * 2);
  for (std::size_t i = 0; i < v.size() / 2; ++i) {
    v[2 * i] = v[2 * i + 1] = amp * static_cast<float>(std::sin(2.0 * 3.14159265358979 * hz * static_cast<double>(i) / kSr));
  }
  return v;
}

std::vector<float> noise(double seconds, float amp = 0.3f) {
  autodj::XorShift32 rng(7);
  std::vector<float> v(static_cast<std::size_t>(seconds * kSr) * 2);
  for (float& x : v) x = amp * rng.bipolar();
  return v;
}

}  // namespace

TEST(ChaosMapping, IsMonotonicBoundedAndMorphsFromReverbToFlanger) {
  float prevLevel = -1.f, prevRev = 2.f, prevFlg = -1.f;
  const ChaosFx::Mapping zero = ChaosFx::map(0.f);
  EXPECT_EQ(zero.reverbGain, 0.f);
  EXPECT_EQ(zero.flangerGain, 0.f);
  for (int i = 0; i <= 100; ++i) {
    const float a = static_cast<float>(i) / 100.f;
    const ChaosFx::Mapping m = ChaosFx::map(a);
    const float level = std::sqrt(m.reverbGain * m.reverbGain + m.flangerGain * m.flangerGain);  // equal-power crossfade
    EXPECT_GE(level, prevLevel - 1e-6f);
    EXPECT_LE(level, 0.6f + 1e-5f);
    EXPECT_LT(m.feedback, 0.95f) << "the flanger comb must stay stable";
    if (i > 26) {  // above the fade-in region (level reaches its maximum at 25 %) the reverb share only falls, the flanger share only rises
      EXPECT_LE(m.reverbGain, prevRev + 1e-6f);
      EXPECT_GE(m.flangerGain, prevFlg - 1e-6f);
    }
    prevLevel = level;
    prevRev = m.reverbGain;
    prevFlg = m.flangerGain;
  }
  const ChaosFx::Mapping one = ChaosFx::map(1.f);
  EXPECT_NEAR(one.reverbGain, 0.f, 1e-6f);
  EXPECT_NEAR(one.flangerGain, 0.6f, 1e-5f);
}

TEST(ChaosMapping, OutOfRangeAndNanAreClamped) {
  EXPECT_EQ(ChaosFx::map(-3.f).reverbGain, 0.f);
  EXPECT_EQ(ChaosFx::map(std::nanf("")).reverbGain, 0.f);
  EXPECT_EQ(ChaosFx::map(std::nanf("")).flangerGain, 0.f);
  EXPECT_NEAR(ChaosFx::map(7.f).flangerGain, ChaosFx::map(1.f).flangerGain, 1e-7f);
}

TEST(Chaos, AmountZeroIsAnExactBypass) {
  ChaosFx fx;
  fx.prepare(kSr);
  const auto in = noise(0.5);
  const auto out = runConst(fx, in, 0.f);
  EXPECT_EQ(in, out);
}

TEST(Chaos, ReverbRingsOnAndDecays) {
  ChaosFx fx;
  fx.prepare(kSr);
  std::vector<float> in(static_cast<std::size_t>(2.5 * kSr) * 2, 0.f);
  in[0] = in[1] = 1.f;  // impulse
  const auto out = runConst(fx, in, 0.08f);
  ASSERT_TRUE(testutil::allFinite(out));
  auto windowRms = [&](double t0, double t1) {
    return testutil::rms(out, static_cast<std::size_t>(t0 * kSr) * 2, static_cast<std::size_t>(t1 * kSr) * 2);
  };
  const double early = windowRms(0.05, 0.25), mid = windowRms(0.6, 0.8), late = windowRms(2.0, 2.2);
  EXPECT_GT(early, 1e-4);
  EXPECT_GT(early, mid);
  EXPECT_GT(mid, late);
  EXPECT_LT(late, early * 0.2) << "the tail must die away (feedback < 1)";
}

TEST(Chaos, FlangerSweepsTheCombAcrossAFixedTone) {
  // A 1 kHz sine through a swept comb: the level at 1 kHz moves between boost and cancellation as the delay changes.
  ChaosFx fx;
  fx.prepare(kSr);
  const auto out = runConst(fx, sine(1000.0, 6.0), 1.0f);
  ASSERT_TRUE(testutil::allFinite(out));
  const int win = 480;  // 10 ms
  double lo = 1e9, hi = 0.0;
  for (std::size_t start = static_cast<std::size_t>(1.0 * kSr); start + win < out.size() / 2; start += win) {
    const double r = testutil::rms(out, start * 2, (start + win) * 2);
    lo = std::min(lo, r);
    hi = std::max(hi, r);
  }
  EXPECT_GT(hi / lo, 2.0) << "flanger comb should sweep through the tone (min " << lo << ", max " << hi << ")";

  // The reverb-dominated end does not sweep.
  ChaosFx rev;
  rev.prepare(kSr);
  const auto rout = runConst(rev, sine(1000.0, 6.0), 0.04f);
  lo = 1e9;
  hi = 0.0;
  for (std::size_t start = static_cast<std::size_t>(2.0 * kSr); start + win < rout.size() / 2; start += win) {
    const double r = testutil::rms(rout, start * 2, (start + win) * 2);
    lo = std::min(lo, r);
    hi = std::max(hi, r);
  }
  EXPECT_LT(hi / lo, 1.25);
}

TEST(Chaos, StaysStableOnLoudNoiseAtEverySetting) {
  for (float a : {0.05f, 0.3f, 0.5f, 0.8f, 1.0f}) {
    ChaosFx fx;
    fx.prepare(kSr);
    auto in = noise(2.0, 0.9f);
    in.resize(in.size() + static_cast<std::size_t>(4.0 * kSr) * 2, 0.f);  // then 4 s of silence
    const auto out = runConst(fx, in, a);
    ASSERT_TRUE(testutil::allFinite(out)) << a;
    EXPECT_LT(testutil::peak(out), 12.f) << a;  // wet <= 0.6, comb resonance gain < 1/(1-0.8), reverb ~unity: bounded
    EXPECT_LT(testutil::rms(out, out.size() - static_cast<std::size_t>(0.5 * kSr) * 2), 1e-3) << "tail did not decay at amount " << a;
  }
}

TEST(Chaos, RampingToZeroLeavesNoJump) {
  ChaosFx fx;
  fx.prepare(kSr);
  auto in = sine(440.0, 3.0);
  const auto dry = in;
  float a = 0.9f;
  std::size_t blocks = 0;
  for (std::size_t i = 0; i < in.size(); i += 2 * kBlock, ++blocks) {
    const int n = static_cast<int>(std::min<std::size_t>(kBlock, (in.size() - i) / 2));
    const float next = blocks > 100 ? std::max(0.f, a - 0.01f) : a;  // linear fade-out of the knob after 100 blocks
    fx.process(in.data() + i, n, a, next);
    a = next;
  }
  ASSERT_EQ(a, 0.f);
  // The wet part (out - dry) must be continuous: no step when the stage finally switches itself off.
  float maxStep = 0.f, prev = 0.f;
  for (std::size_t i = 0; i < in.size(); i += 2) {
    const float wet = in[i] - dry[i];
    if (i > 0) maxStep = std::max(maxStep, std::fabs(wet - prev));
    prev = wet;
  }
  EXPECT_LT(maxStep, 0.05f);
  EXPECT_EQ(std::fabs(in[in.size() - 2] - dry[in.size() - 2]), 0.f) << "fully off at the end";
}

TEST(Chaos, EngagingAfterBeingOffNeverReplaysOldAudio) {
  ChaosFx fx;
  fx.prepare(kSr);
  runConst(fx, noise(0.5), 0.7f);
  std::vector<float> silence(static_cast<std::size_t>(0.5 * kSr) * 2, 0.f);
  runConst(fx, silence, 0.f);                 // off: state is dropped
  const auto out = runConst(fx, silence, 0.7f);  // on again with silent input
  EXPECT_EQ(testutil::peak(out), 0.f);
}

TEST(Chaos, SamplerateIndependentConstruction) {
  for (double sr : {22050.0, 44100.0, 48000.0, 96000.0, 192000.0}) {
    ChaosFx fx;
    fx.prepare(sr);
    std::vector<float> in(static_cast<std::size_t>(sr * 0.5) * 2, 0.f);
    in[0] = in[1] = 1.f;
    for (std::size_t i = 0; i < in.size(); i += 2 * kBlock) {
      const int n = static_cast<int>(std::min<std::size_t>(kBlock, (in.size() - i) / 2));
      fx.process(in.data() + i, n, 0.5f, 0.5f);
    }
    ASSERT_TRUE(testutil::allFinite(in)) << sr;
    EXPECT_GT(testutil::peak(in, 4), 0.f) << sr;
  }
}

TEST(Chaos, ProcessNeverAllocates) {
  ChaosFx fx;
  fx.prepare(kSr);
  auto in = noise(0.3);
  testutil::ViolationCounter violations;
  {
    autodj::rt::AudioThreadScope scope;
    for (std::size_t i = 0; i < in.size(); i += 2 * kBlock) {
      const int n = static_cast<int>(std::min<std::size_t>(kBlock, (in.size() - i) / 2));
      fx.process(in.data() + i, n, 0.0f, 0.5f);
      fx.process(in.data() + i, n, 0.5f, 1.0f);
      fx.process(in.data() + i, n, 1.0f, 0.0f);
      fx.process(in.data() + i, n, 0.0f, 0.0f);
    }
  }
  EXPECT_EQ(violations.count(), 0u);
}
