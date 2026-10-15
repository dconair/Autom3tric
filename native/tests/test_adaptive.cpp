#include <gtest/gtest.h>

#include "autodj/granular/AdaptiveQuality.h"

using autodj::granular::AdaptiveQuality;

namespace {
constexpr float kDt = 192.f / 48000.f;  // one callback = 4 ms

// Feeds a constant load for `seconds`; returns the callback index of each level change.
std::vector<int> feed(AdaptiveQuality& q, float load, double seconds) {
  std::vector<int> changes;
  int level = q.level();
  const int n = static_cast<int>(seconds / kDt);
  for (int i = 0; i < n; ++i) {
    q.update(load, kDt);
    if (q.level() != level) {
      changes.push_back(i);
      level = q.level();
    }
  }
  return changes;
}
}  // namespace

TEST(AdaptiveQuality, SustainedOverloadStepsDownToTheFloorQuickly) {
  AdaptiveQuality q;
  const auto changes = feed(q, 0.8f, 3.0);
  EXPECT_EQ(q.level(), 3);
  ASSERT_EQ(changes.size(), 3u);
  EXPECT_LT(changes[0] * kDt, 0.35) << "first reaction within ~100 ms hold + smoothing";
  for (std::size_t i = 1; i < changes.size(); ++i) {
    EXPECT_GE((changes[i] - changes[i - 1]) * kDt, 0.29) << "at most one step per 300 ms";
  }
}

TEST(AdaptiveQuality, SingleCallbackSpikeAbove90PercentReactsImmediately) {
  AdaptiveQuality q;
  feed(q, 0.3f, 1.0);
  q.update(0.95f, kDt);
  EXPECT_EQ(q.level(), 1);
}

TEST(AdaptiveQuality, RecoversOneStepPerHoldTimeAndOnlyWhenCalm) {
  AdaptiveQuality q;
  feed(q, 0.9f, 2.0);
  ASSERT_EQ(q.level(), 3);
  const auto changes = feed(q, 0.3f, 8.0);
  EXPECT_EQ(q.level(), 0);
  ASSERT_EQ(changes.size(), 3u);
  EXPECT_GE(changes[0] * kDt, 1.4) << "no recovery before ~1.5 s of calm";
  for (std::size_t i = 1; i < changes.size(); ++i) EXPECT_GE((changes[i] - changes[i - 1]) * kDt, 1.45);
}

TEST(AdaptiveQuality, LoadInsideTheHysteresisBandChangesNothing) {
  AdaptiveQuality q;
  feed(q, 0.9f, 1.0);
  const int level = q.level();
  ASSERT_GT(level, 0);
  EXPECT_TRUE(feed(q, 0.6f, 30.0).empty());
  EXPECT_EQ(q.level(), level);
}

TEST(AdaptiveQuality, BorderlineWorkloadSettlesInsteadOfToggling) {
  // Plant: the same musical demand costs 0.75 at level 0 but only 0.42 once Hermite is off. A naive
  // governor would flip 0 -> 1 -> 0 -> 1 ... every couple of seconds; the back-off must damp that.
  const float cost[4] = {0.75f, 0.42f, 0.30f, 0.22f};
  AdaptiveQuality q;
  int toggles = 0, level = 0;
  for (int i = 0; i < static_cast<int>(120.0 / kDt); ++i) {
    q.update(cost[q.level()], kDt);
    if (q.level() != level) {
      ++toggles;
      level = q.level();
    }
  }
  EXPECT_LE(toggles, 16) << "without the back-off this workload flips ~120 times in two minutes";
  EXPECT_GE(toggles, 1);
}

TEST(AdaptiveQuality, LimitsGetStricterWithEveryLevel) {
  float density = 2.f, cap = 2.f;
  int voices = -1;
  for (int l = 0; l <= AdaptiveQuality::kMaxLevel; ++l) {
    const auto lim = AdaptiveQuality::limitsFor(l);
    EXPECT_LE(lim.densityScale, density);
    EXPECT_LE(lim.grainCapScale, cap);
    EXPECT_GE(lim.voiceReduction, voices);
    EXPECT_EQ(lim.forceLinear, l >= 1);
    density = lim.densityScale;
    cap = lim.grainCapScale;
    voices = lim.voiceReduction;
  }
}
