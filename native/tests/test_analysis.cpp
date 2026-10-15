#include <gtest/gtest.h>

#include <chrono>
#include <cmath>

#include "autodj/analysis/Analysis.h"
#include "autodj/analysis/PeakPyramid.h"
#include "autodj/core/DemoSources.h"

using namespace autodj;
using autodj::analysis::analyzeTrack;
using autodj::analysis::TrackAnalysis;

namespace {

TrackAnalysis analyzeSpec(const demo::MusicSpec& spec) {
  auto buf = demo::musicTrack(spec);
  return analyzeTrack(buf->data(), buf->frames(), buf->sampleRate());
}

double wrapDiff(double a, double b, double period) {
  double d = std::fmod(a - b, period);
  if (d > 0.5 * period) d -= period;
  if (d < -0.5 * period) d += period;
  return d;
}

}  // namespace

TEST(Analysis, TempoIsFoundWithinATenthOfABpm) {
  for (double bpm : {100.0, 118.0, 124.0, 128.0, 135.0, 140.0}) {
    demo::MusicSpec s;
    s.bpm = bpm;
    s.introBars = 4;
    s.dropBars = 8;
    s.breakBars = 4;
    s.outroBars = 4;
    const TrackAnalysis a = analyzeSpec(s);
    EXPECT_NEAR(a.bpm, bpm, 0.15) << "target " << bpm << " got " << a.bpm << " grid conf " << a.gridConfidence;
    EXPECT_GT(a.gridConfidence, 0.65) << bpm;  // the 4-bar break has no kick, so not every beat carries an attack
    EXPECT_GT(a.tempoConfidence, 0.3) << bpm;
  }
}

TEST(Analysis, GridOffsetAndDownbeatFollowTheMusic) {
  for (double lead : {0.0, 0.13, 0.31, 0.77}) {
    demo::MusicSpec s;
    s.bpm = 126.0;
    s.startOffsetSec = lead;
    const TrackAnalysis a = analyzeSpec(s);
    const double bar = 4.0 * 60.0 / 126.0;
    ASSERT_GT(a.bpm, 0.0);
    const double err = wrapDiff(a.gridOffsetSec, lead, bar);
    EXPECT_LT(std::fabs(err), 0.015) << "lead " << lead << " grid offset " << a.gridOffsetSec << " (error " << err * 1000 << " ms)";
    EXPECT_GT(a.downbeatConfidence, 0.1);
  }
}

TEST(Analysis, KeyAndCamelot) {
  struct Case { int root; bool minor; const char* camelot; const char* name; };
  const Case cases[] = {
      {57, true, "8A", "A minor"},
      {61, false, "3B", "C# major"},
      {54, true, "11A", "F# minor"},
      {60, false, "8B", "C major"},
  };
  for (const Case& c : cases) {
    demo::MusicSpec s;
    s.rootMidi = c.root;
    s.minor = c.minor;
    s.introBars = 4;
    s.dropBars = 8;
    s.breakBars = 4;
    s.outroBars = 4;
    const TrackAnalysis a = analyzeSpec(s);
    EXPECT_EQ(a.keyName, c.name) << "expected " << c.name;
    EXPECT_EQ(a.camelot, c.camelot);
  }
  EXPECT_TRUE(analysis::camelotCompatible("8A", "8B"));
  EXPECT_TRUE(analysis::camelotCompatible("8A", "9A"));
  EXPECT_TRUE(analysis::camelotCompatible("12A", "1A"));
  EXPECT_FALSE(analysis::camelotCompatible("8A", "10A"));
  EXPECT_FALSE(analysis::camelotCompatible("8A", "9B"));
  EXPECT_EQ(analysis::keyName(9, true), "A minor");
  EXPECT_EQ(analysis::camelotName(9, true), "8A");
  EXPECT_EQ(analysis::camelotName(0, false), "8B");
}

TEST(Analysis, StructureFindsIntroDropBreakDropOutro) {
  demo::MusicSpec s;  // 8 intro, 16 drop, 8 break, 16 drop, 8 outro at 124 BPM
  const TrackAnalysis a = analyzeSpec(s);
  ASSERT_GT(a.bpm, 0.0);
  std::string labels;
  for (const auto& sec : a.sections) labels += sec.label + " ";
  ASSERT_EQ(a.sections.size(), 5u) << labels;
  EXPECT_EQ(a.sections[0].label, "intro");
  EXPECT_EQ(a.sections[1].label, "drop");
  EXPECT_EQ(a.sections[2].label, "break");
  EXPECT_EQ(a.sections[3].label, "drop");
  EXPECT_EQ(a.sections[4].label, "outro");
  const double bar = 4.0 * 60.0 / 124.0;
  EXPECT_NEAR(a.sections[1].startSec, 8 * bar, 1.2 * bar);
  EXPECT_NEAR(a.sections[2].startSec, 24 * bar, 1.2 * bar);
  EXPECT_NEAR(a.sections[3].startSec, 32 * bar, 1.2 * bar);
  EXPECT_NEAR(a.sections[4].startSec, 48 * bar, 1.2 * bar);
  // mix points
  EXPECT_NEAR(a.mixOutSec, 48 * bar, 1.2 * bar);
  EXPECT_LE(a.mixInSec, bar);
  EXPECT_GE(a.barEnergy.size(), 54u);
}

TEST(Analysis, SilenceNoiseAndShortInputsDoNotCrashAndReportNoTempo) {
  std::vector<float> silence(48000 * 20 * 2, 0.f);
  TrackAnalysis a = analyzeTrack(silence.data(), 48000 * 20, 48000);
  EXPECT_EQ(a.bpm, 0.0);
  EXPECT_EQ(a.keyTonic, -1);

  auto noise = demo::noise(20.0, 48000, 0.3f);
  a = analyzeTrack(noise->data(), noise->frames(), 48000);
  EXPECT_LT(a.gridConfidence, 0.6);  // may or may not find a "tempo" in noise, but never claims a clean grid

  std::vector<float> tiny(100 * 2, 0.1f);
  a = analyzeTrack(tiny.data(), 100, 48000);
  EXPECT_EQ(a.bpm, 0.0);
  a = analyzeTrack(nullptr, 0, 48000);
  EXPECT_EQ(a.durationSec, 0.0);
}

TEST(Analysis, ReportsProgressMonotonically) {
  demo::MusicSpec s;
  s.introBars = 2;
  s.dropBars = 4;
  s.breakBars = 2;
  s.outroBars = 2;
  auto buf = demo::musicTrack(s);
  float last = -1.f;
  bool monotonic = true;
  int calls = 0;
  analyzeTrack(buf->data(), buf->frames(), 48000, {}, [&](float p) {
    if (p < last) monotonic = false;
    last = p;
    ++calls;
  });
  EXPECT_TRUE(monotonic);
  EXPECT_GT(calls, 5);
  EXPECT_FLOAT_EQ(last, 1.f);
}

TEST(Analysis, LoudnessAndTrim) {
  demo::MusicSpec s;
  s.level = 0.9f;
  s.introBars = 2;
  s.dropBars = 4;
  s.breakBars = 2;
  s.outroBars = 2;
  const TrackAnalysis loud = analyzeSpec(s);
  s.level = 0.1f;
  const TrackAnalysis quiet = analyzeSpec(s);
  EXPECT_NEAR(loud.peakDb, 20 * std::log10(0.9), 0.5);
  EXPECT_NEAR(loud.rmsDb - quiet.rmsDb, 20 * std::log10(9.0), 0.5);
  EXPECT_GT(quiet.trimDb, loud.trimDb);
  EXPECT_LE(loud.trimDb + loud.peakDb, 1.01);
}

TEST(Analysis, AnalysisSpeed) {
  demo::MusicSpec s;
  s.introBars = 16;
  s.dropBars = 32;
  s.breakBars = 16;
  s.outroBars = 16;  // 128 bars, ~4 min at 124 BPM
  auto buf = demo::musicTrack(s);
  const auto t0 = std::chrono::steady_clock::now();
  const TrackAnalysis a = analyzeTrack(buf->data(), buf->frames(), 48000);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  std::printf("analysis of %.0f s of audio took %.0f ms (bpm %.2f)\n", a.durationSec, ms, a.bpm);
  EXPECT_NEAR(a.bpm, 124.0, 0.15);
  EXPECT_LT(ms, 20000.0);
}

TEST(PeakPyramid, ContainsTheTruePeaksAndIsExactWhenZoomedIn) {
  auto buf = demo::drumLoop(120.0, 8, 48000);
  const analysis::PeakPyramid pyr(*buf);
  EXPECT_GE(pyr.levels(), 3u);

  const int cols = 200;
  std::vector<float> got(2 * cols), ref(2 * cols);
  pyr.query(*buf, 0, buf->frames(), cols, got.data());
  computePeaks(*buf, cols, ref.data());
  for (int c = 0; c < cols; ++c) {
    // blocks are not aligned to columns, so the pyramid may include a neighbouring block, never miss a sample
    EXPECT_LE(got[2 * c], ref[2 * c] + 1e-6f) << c;
    EXPECT_GE(got[2 * c + 1], ref[2 * c + 1] - 1e-6f) << c;
  }

  // zoomed in below one block (16 frames over 16 columns): exact samples
  float out[32];
  pyr.query(*buf, 1000, 1016, 16, out);
  for (int i = 0; i < 16; ++i) {
    const float m = 0.5f * (buf->data()[2 * (1000 + i)] + buf->data()[2 * (1000 + i) + 1]);
    EXPECT_FLOAT_EQ(out[2 * i], m);
    EXPECT_FLOAT_EQ(out[2 * i + 1], m);
  }
  // outside the track: zeros, no crash
  pyr.query(*buf, -5000, -4000, 10, got.data());
  EXPECT_EQ(got[0], 0.f);
  pyr.query(*buf, buf->frames() + 10, buf->frames() + 5000, 10, got.data());
  EXPECT_EQ(got[1], 0.f);
}
