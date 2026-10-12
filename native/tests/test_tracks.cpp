#include <gtest/gtest.h>

#include <cmath>

#include "autodj/analysis/AnalysisJson.h"
#include "autodj/core/DemoSources.h"
#include "autodj/core/Resampler.h"
#include "autodj/engine/Engine.h"

using namespace autodj;

namespace {
std::vector<float> sineInterleaved(double f, std::size_t frames, int rate, int channels, float amp = 0.5f) {
  std::vector<float> v(frames * static_cast<std::size_t>(channels));
  for (std::size_t i = 0; i < frames; ++i) {
    const float s = amp * static_cast<float>(std::sin(2.0 * M_PI * f * static_cast<double>(i) / rate));
    for (int c = 0; c < channels; ++c) v[i * static_cast<std::size_t>(channels) + static_cast<std::size_t>(c)] = s;
  }
  return v;
}
}  // namespace

TEST(Resampler, LengthAndSineAccuracyFor44100To48000) {
  const std::size_t n = 44100 * 3;
  const auto in = sineInterleaved(1000.0, n, 44100, 2);
  const auto out = resampleInterleaved(in.data(), n, 2, 44100, 48000);
  EXPECT_EQ(out.size() / 2, n * 48000 / 44100);
  double maxErr = 0.0;
  for (std::size_t i = 64; i + 64 < out.size() / 2; ++i) {
    const double ref = 0.5 * std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
    maxErr = std::max(maxErr, std::fabs(out[2 * i] - ref));
    maxErr = std::max(maxErr, std::fabs(out[2 * i + 1] - ref));
  }
  EXPECT_LT(maxErr, 5e-4) << "worse than about -60 dB";
}

TEST(Resampler, DownsamplingRemovesFrequenciesAboveTheNewNyquist) {
  const std::size_t n = 96000 * 2;
  const auto in = sineInterleaved(30000.0, n, 96000, 1);  // above the 48 kHz-rate Nyquist of 24 kHz
  const auto out = resampleInterleaved(in.data(), n, 1, 96000, 48000);
  double peak = 0.0;
  for (std::size_t i = 200; i + 200 < out.size(); ++i) peak = std::max(peak, static_cast<double>(std::fabs(out[i])));
  EXPECT_LT(peak, 0.01) << "aliasing";
  EXPECT_EQ(out.size(), n / 2);
}

TEST(Resampler, EqualRatesCopyAndOddRatiosAreExactInLength) {
  const auto in = sineInterleaved(440.0, 1000, 48000, 2);
  EXPECT_EQ(resampleInterleaved(in.data(), 1000, 2, 48000, 48000), in);
  EXPECT_EQ(resampleInterleaved(in.data(), 1000, 2, 22050, 48000).size() / 2, 1000u * 48000 / 22050);
  EXPECT_TRUE(resampleInterleaved(in.data(), 0, 2, 44100, 48000).empty());
}

TEST(TrackIngest, MonoSurroundAndChunkedAppend) {
  TrackIngest mono(48000, 1);
  const auto m = sineInterleaved(440.0, 4800, 48000, 1);
  mono.append(m.data(), 1000);
  mono.append(m.data() + 1000, m.size() - 1000);
  auto buf = mono.finish(48000, "m");
  ASSERT_TRUE(buf);
  EXPECT_EQ(buf->frames(), 4800);
  EXPECT_FLOAT_EQ(buf->data()[2 * 100], buf->data()[2 * 100 + 1]);
  EXPECT_FLOAT_EQ(buf->data()[2 * 100], m[100]);

  TrackIngest six(48000, 6);
  std::vector<float> s(6 * 100, 0.f);
  for (int i = 0; i < 100; ++i) s[static_cast<std::size_t>(i * 6 + 2)] = 1.f;  // centre only
  six.append(s.data(), s.size() + 4);  // trailing partial frame is dropped
  auto b6 = six.finish(48000, "s");
  ASSERT_TRUE(b6);
  EXPECT_EQ(b6->frames(), 100);
  EXPECT_NEAR(b6->data()[0], 0.75f * 0.7071f, 1e-5);
  EXPECT_FLOAT_EQ(b6->data()[0], b6->data()[1]);

  TrackIngest empty(44100, 2);
  EXPECT_EQ(empty.finish(48000, "x"), nullptr);
}

TEST(AnalysisJson, RoundTripAndTolerantReader) {
  demo::MusicSpec spec;
  spec.introBars = 2;
  spec.dropBars = 4;
  spec.breakBars = 2;
  spec.outroBars = 2;
  auto buf = demo::musicTrack(spec);
  const auto a = analysis::analyzeTrack(buf->data(), buf->frames(), buf->sampleRate());
  const auto j = analysis::toJson(a);
  analysis::TrackAnalysis b;
  ASSERT_TRUE(analysis::fromJson(nlohmann::json::parse(j.dump()), b));
  EXPECT_DOUBLE_EQ(a.bpm, b.bpm);
  EXPECT_DOUBLE_EQ(a.gridOffsetSec, b.gridOffsetSec);
  EXPECT_EQ(a.camelot, b.camelot);
  ASSERT_EQ(a.sections.size(), b.sections.size());
  for (std::size_t i = 0; i < a.sections.size(); ++i) EXPECT_EQ(a.sections[i].label, b.sections[i].label);
  ASSERT_EQ(a.barEnergy.size(), b.barEnergy.size());
  for (std::size_t i = 0; i < a.barEnergy.size(); ++i) EXPECT_NEAR(a.barEnergy[i], b.barEnergy[i], 0.006f);

  analysis::TrackAnalysis c;
  EXPECT_TRUE(analysis::fromJson(nlohmann::json::parse(R"({"bpm": 128.5, "unknown": 1, "camelot": 5})"), c));  // wrong type is ignored
  EXPECT_DOUBLE_EQ(c.bpm, 128.5);
  EXPECT_EQ(c.camelot, "");
  EXPECT_FALSE(analysis::fromJson(nlohmann::json::parse("[1,2]"), c));
  EXPECT_FALSE(analysis::fromJson(nlohmann::json::parse(R"({"version": 99})"), c));
}

TEST(EngineTracks, AddAnalyseRemoveAndOverview) {
  EngineConfig cfg;
  cfg.realtime = false;
  Engine e(cfg);
  demo::MusicSpec spec;
  spec.introBars = 2;
  spec.dropBars = 4;
  spec.breakBars = 2;
  spec.outroBars = 2;
  const int id = e.addTrack(demo::musicTrack(spec), "song");
  ASSERT_GT(id, 0);
  auto t = e.track(id);
  ASSERT_TRUE(t);
  EXPECT_EQ(t->name, "song");
  EXPECT_NEAR(t->analysis.bpm, 124.0, 0.2);
  EXPECT_NEAR(t->durationSec(), t->analysis.durationSec, 0.01);

  std::vector<int> mn, mx;
  trackOverview(*t, 128, mn, mx);
  ASSERT_EQ(mn.size(), 128u);
  EXPECT_LE(mn[10], mx[10]);
  int peak = 0;
  for (int v : mx) peak = std::max(peak, v);
  EXPECT_GT(peak, 20);

  // a cached analysis is adopted without running the analyser
  analysis::TrackAnalysis cached;
  cached.bpm = 99.0;
  const int id2 = e.addTrack(demo::musicTrack(spec), "cached", &cached);
  EXPECT_DOUBLE_EQ(e.track(id2)->analysis.bpm, 99.0);
  EXPECT_NE(id, id2);

  EXPECT_EQ(e.trackCount(), 2u);
  e.removeTrack(id);
  EXPECT_EQ(e.track(id), nullptr);
  EXPECT_EQ(e.trackCount(), 1u);
  EXPECT_EQ(e.addTrack(nullptr, "none"), 0);
}
