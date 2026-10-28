#include <gtest/gtest.h>

#include <vector>

#include "autodj/core/Transport.h"

using namespace autodj;

namespace {

// Runs the transport for `totalFrames` in blocks of `block` and returns the absolute sample index
// of every grid line the BlockTime helpers report.
std::vector<double> crossings(double bpm, double div, int block, long totalFrames, double sr = 48000.0) {
  Transport t;
  t.prepare(sr);
  std::vector<double> out;
  long done = 0;
  while (done < totalFrames) {
    const int n = static_cast<int>(std::min<long>(block, totalFrames - done));
    const BlockTime bt = t.beginBlock(n, bpm);
    bt.forEachGridCrossing(div, [&](int off) { out.push_back(static_cast<double>(done + off)); });
    t.endBlock(bt);
    done += n;
  }
  return out;
}

}  // namespace

TEST(Transport, GridLinesAreSampleExactAtIntegerSamplesPerBeat) {
  // 120 BPM @ 48 kHz -> 24000 samples/beat -> 1/16 note = 6000 samples exactly.
  for (int block : {1, 7, 64, 100, 128, 192, 480, 1000, 4096}) {
    const auto c = crossings(120.0, 0.25, block, 48000 * 4);
    ASSERT_EQ(c.size(), 32u) << "block " << block;
    for (std::size_t k = 0; k < c.size(); ++k) {
      EXPECT_EQ(c[k], 6000.0 * static_cast<double>(k)) << "block " << block << " line " << k;
    }
  }
}

TEST(Transport, TripletsAndOddTemposStayWithinOneSample) {
  struct Case { double bpm, div; };
  for (Case cs : {Case{128.0, 1.0 / 3.0}, Case{123.7, 0.25}, Case{97.31, 1.0 / 6.0}, Case{174.0, 2.0 / 3.0}}) {
    const double samplesPerLine = 48000.0 * 60.0 / cs.bpm * cs.div;
    for (int block : {64, 192, 480}) {
      const long total = 48000 * 20;
      const auto c = crossings(cs.bpm, cs.div, block, total);
      const long expected = static_cast<long>(std::ceil(static_cast<double>(total) / samplesPerLine));
      EXPECT_NEAR(static_cast<double>(c.size()), static_cast<double>(expected), 1.0) << cs.bpm;
      for (std::size_t k = 0; k < c.size(); ++k) {
        // Lines are claimed by the block that contains them (exactly-once tiling); a line in the last
        // half sample of a block is reported at the block's last frame, so the bound is < 1 sample.
        EXPECT_LT(std::fabs(c[k] - samplesPerLine * static_cast<double>(k)), 1.0)
            << "bpm " << cs.bpm << " block " << block << " line " << k;
      }
    }
  }
}

TEST(Transport, NoLineIsReportedTwiceOrSkippedAcrossBlockBoundaries) {
  // Grid lines that land exactly on a block boundary are the classic failure case.
  const auto c = crossings(120.0, 0.5, 12000, 48000 * 10);  // block == one half-beat exactly
  ASSERT_EQ(c.size(), 40u);
  for (std::size_t k = 0; k < c.size(); ++k) EXPECT_EQ(c[k], 12000.0 * static_cast<double>(k));
}

TEST(Transport, TempoChangeKeepsBeatPositionContinuous) {
  Transport t;
  t.prepare(48000.0);
  t.play();
  double beat = 0.0;
  for (int i = 0; i < 300; ++i) {
    const double bpm = (i < 100) ? 120.0 : (i < 200 ? 90.0 : 150.0);
    const BlockTime bt = t.beginBlock(192, bpm);
    EXPECT_NEAR(bt.clockBeat, beat, 1e-9);
    beat += 192.0 * bpm / (60.0 * 48000.0);
    t.endBlock(bt);
  }
  EXPECT_NEAR(t.clockBeat(), beat, 1e-9);
  EXPECT_NEAR(t.playBeat(), beat, 1e-9);
}

TEST(Transport, StopFreezesPlayheadButClockKeepsRunning) {
  Transport t;
  t.prepare(48000.0);
  t.play();
  for (int i = 0; i < 50; ++i) t.endBlock(t.beginBlock(480, 120.0));
  const double stoppedAt = t.playBeat();
  t.stop();
  for (int i = 0; i < 50; ++i) t.endBlock(t.beginBlock(480, 120.0));
  EXPECT_DOUBLE_EQ(t.playBeat(), stoppedAt);
  EXPECT_GT(t.clockBeat(), stoppedAt + 1.0);
}

TEST(Transport, PlayAlignsClockToPlayheadAndSeekMovesBoth) {
  Transport t;
  t.prepare(48000.0);
  for (int i = 0; i < 10; ++i) t.endBlock(t.beginBlock(480, 120.0));  // free-running clock
  t.seek(16.0);
  EXPECT_DOUBLE_EQ(t.playBeat(), 16.0);
  t.play();
  EXPECT_DOUBLE_EQ(t.clockBeat(), 16.0) << "play() snaps the grid to the play-head";
  t.seek(32.0);
  EXPECT_DOUBLE_EQ(t.clockBeat(), 32.0);
  t.seek(-5.0);
  EXPECT_DOUBLE_EQ(t.playBeat(), 0.0);
}

TEST(Transport, ZeroTempoOrDivisionNeverLoops) {
  Transport t;
  t.prepare(48000.0);
  BlockTime bt = t.beginBlock(192, 0.0);
  int n = 0;
  bt.forEachGridCrossing(0.25, [&](int) { ++n; });
  bt = t.beginBlock(192, 120.0);
  bt.forEachGridCrossing(0.0, [&](int) { ++n; });
  EXPECT_EQ(n, 0);
}
