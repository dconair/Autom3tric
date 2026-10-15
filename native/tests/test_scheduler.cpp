#include <gtest/gtest.h>

#include <cmath>
#include <numeric>
#include <vector>

#include "autodj/granular/GrainScheduler.h"

using autodj::XorShift32;
using autodj::granular::GrainScheduler;

namespace {

// Absolute sample index of every grain onset.
std::vector<long> onsets(double density, double sr, long totalFrames, int block, float jitter = 0.f,
                         std::uint32_t seed = 1) {
  GrainScheduler s;
  s.reset();
  XorShift32 rng(seed);
  std::vector<long> out;
  long done = 0;
  while (done < totalFrames) {
    const int n = static_cast<int>(std::min<long>(block, totalFrames - done));
    s.run(n, density, sr, jitter, rng, [&](int off) { out.push_back(done + off); });
    done += n;
  }
  return out;
}

}  // namespace

TEST(Scheduler, CountMatchesDensityOverTime) {
  for (double d : {1.0, 10.0, 37.3, 200.0, 1000.0, 2000.0}) {
    const auto o = onsets(d, 48000.0, 48000 * 10, 192);
    EXPECT_NEAR(static_cast<double>(o.size()), d * 10.0, 1.5) << "density " << d;
  }
}

TEST(Scheduler, FirstGrainFiresAtSampleZeroAndSpacingIsConstant) {
  const auto o = onsets(100.0, 48000.0, 48000, 128);
  ASSERT_GE(o.size(), 99u);
  EXPECT_EQ(o[0], 0);
  for (std::size_t i = 1; i < o.size(); ++i) {
    EXPECT_NEAR(static_cast<double>(o[i] - o[i - 1]), 480.0, 1.0) << i;
    EXPECT_NEAR(static_cast<double>(o[i]), 480.0 * static_cast<double>(i), 1.0) << "no drift at grain " << i;
  }
}

TEST(Scheduler, OnsetPositionsDoNotDependOnBlockSize) {
  for (float jitter : {0.f, 0.6f}) {
    const auto ref = onsets(73.1, 48000.0, 48000 * 5, 192, jitter, 7);
    for (int block : {1, 33, 64, 100, 128, 480, 1024, 7000}) {
      EXPECT_EQ(onsets(73.1, 48000.0, 48000 * 5, block, jitter, 7), ref) << "block " << block << " jitter " << jitter;
    }
  }
}

TEST(Scheduler, JitterKeepsMeanDensityButRandomisesTiming) {
  const auto o = onsets(50.0, 48000.0, 48000 * 120, 192, 1.0f, 3);
  EXPECT_NEAR(static_cast<double>(o.size()) / 120.0, 50.0, 0.7);
  std::vector<double> gaps;
  for (std::size_t i = 1; i < o.size(); ++i) gaps.push_back(static_cast<double>(o[i] - o[i - 1]));
  const double mean = std::accumulate(gaps.begin(), gaps.end(), 0.0) / static_cast<double>(gaps.size());
  double var = 0.0;
  for (double g : gaps) var += (g - mean) * (g - mean);
  var /= static_cast<double>(gaps.size());
  EXPECT_GT(std::sqrt(var), 0.3 * mean) << "gaps must actually vary";
  for (double g : gaps) EXPECT_GE(g, 0.05 * mean) << "threshold is bounded away from zero";
}

TEST(Scheduler, DensityChangeTakesEffectImmediately) {
  GrainScheduler s;
  s.reset();
  XorShift32 rng(1);
  std::vector<long> o;
  long t = 0;
  auto run = [&](double density, long frames) {
    for (long done = 0; done < frames; done += 128) {
      s.run(128, density, 48000.0, 0.f, rng, [&](int off) { o.push_back(t + off); });
      t += 128;
    }
  };
  run(0.5, 48000);          // one grain per 2 s: just fired at 0, next due in ~2 s
  const long switchAt = t;
  run(100.0, 4800);         // then 100 Hz: must not wait for the old 2 s interval
  long first = -1;
  for (long x : o) {
    if (x >= switchAt) { first = x; break; }
  }
  ASSERT_GE(first, 0);
  EXPECT_LT(first - switchAt, 48000 / 100 + 128) << "first grain after the change within ~one new period";
}

TEST(Scheduler, RestartFiresAtTheNextBlockStart) {
  GrainScheduler s;
  s.reset();
  XorShift32 rng(1);
  int n = 0;
  s.run(128, 2.0, 48000.0, 0.f, rng, [&](int) { ++n; });   // fires at 0
  s.run(128, 2.0, 48000.0, 0.f, rng, [&](int) { ++n; });   // nothing (next is 0.5 s away)
  EXPECT_EQ(n, 1);
  s.restart();
  int off = -1;
  s.run(128, 2.0, 48000.0, 0.f, rng, [&](int o) { off = o; });
  EXPECT_EQ(off, 0);
}

TEST(Scheduler, ZeroDensitySpawnsNothingAndSpawnCountIsBounded) {
  GrainScheduler s;
  s.reset();
  XorShift32 rng(1);
  int n = 0;
  EXPECT_EQ(s.run(192, 0.0, 48000.0, 0.f, rng, [&](int) { ++n; }), 0);
  EXPECT_EQ(n, 0);
  // absurd density: bounded by maxSpawns instead of looping forever
  EXPECT_LE(s.run(192, 1e9, 48000.0, 0.f, rng, [&](int) { ++n; }, 16), 16);
}
