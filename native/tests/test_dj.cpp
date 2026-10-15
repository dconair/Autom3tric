#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <numeric>

#include "autodj/core/DemoSources.h"
#include "autodj/dj/DjDsp.h"
#include "autodj/dj/MixPlan.h"
#include "autodj/engine/Engine.h"
#include "autodj/engine/MixExport.h"
#include "test_util.h"

using namespace autodj;
using namespace autodj::dj;

namespace {

constexpr int kSr = 48000;

/// A track with a hand-written analysis (the exact grid of the synthetic song), so that these tests measure the decks and the
/// plan player, not the analyser (which has its own tests).
std::shared_ptr<Track> songTrack(double bpm, double startOffsetSec = 0.0, int id = 1, int rootMidi = 57, bool minor = true,
                                 const char* camelot = "8A", int intro = 8, int drop = 16, int brk = 8, int outro = 8) {
  demo::MusicSpec s;
  s.bpm = bpm;
  s.sampleRate = kSr;
  s.startOffsetSec = startOffsetSec;
  s.rootMidi = rootMidi;
  s.minor = minor;
  s.introBars = intro;
  s.dropBars = drop;
  s.breakBars = brk;
  s.outroBars = outro;
  auto t = std::make_shared<Track>();
  t->id = id;
  t->name = "song" + std::to_string(id);
  t->audio = demo::musicTrack(s);
  auto& a = t->analysis;
  a.durationSec = t->durationSec();
  a.bpm = bpm;
  a.beatSec = 60.0 / bpm;
  a.barSec = 4.0 * a.beatSec;
  a.gridOffsetSec = std::fmod(startOffsetSec, a.barSec);
  a.firstBarSec = a.gridOffsetSec;
  a.tempoConfidence = a.gridConfidence = 0.9;
  a.downbeatConfidence = 0.8;
  a.camelot = camelot;
  a.keyName = "x";
  a.mixInSec = a.firstBarSec;
  a.mixOutSec = startOffsetSec + (intro + drop + brk + drop) * a.barSec;
  a.trimDb = 0.0;
  return t;
}

/// A track that exists only for the planner: a cheap buffer at a tiny sample rate whose duration is what the planner reads.
std::shared_ptr<Track> planOnlyTrack(double bpm, double durationSec, int id, const char* camelot = "8A", double offset = 0.0) {
  auto t = std::make_shared<Track>();
  t->id = id;
  t->name = "t" + std::to_string(id);
  std::vector<float> z(static_cast<std::size_t>(durationSec * 100) * 2, 0.f);
  t->audio = SampleBuffer::fromInterleavedStereo(z.data(), z.size() / 2, 100, "plan-only");
  auto& a = t->analysis;
  a.durationSec = durationSec;
  a.bpm = bpm;
  a.beatSec = bpm > 0 ? 60.0 / bpm : 0.0;
  a.barSec = 4.0 * a.beatSec;
  a.gridOffsetSec = offset;
  a.firstBarSec = offset;
  a.gridConfidence = 0.9;
  a.camelot = camelot;
  a.mixInSec = offset;
  a.mixOutSec = std::max(0.0, std::floor((durationSec - 25.0) / std::max(a.barSec, 1.0)) * a.barSec + offset);
  return t;
}

std::vector<float> renderPlan(const std::shared_ptr<MixPlan>& plan, double seconds, int audibleMask = -1, int callback = 480,
                              DjStatus* last = nullptr, std::uint64_t* violations = nullptr) {
  EngineConfig cfg;
  cfg.sampleRate = kSr;
  cfg.realtime = false;
  Engine e(cfg);
  e.djNode().setDebugAudibleMask(audibleMask);
  testutil::ViolationCounter counter;
  e.planStart(plan);
  std::vector<float> out;
  const int total = static_cast<int>(seconds * kSr);
  out.reserve(static_cast<std::size_t>(total) * 2);
  std::vector<float> buf(static_cast<std::size_t>(callback) * 2);
  DjStatus st;
  for (int done = 0; done < total; done += callback) {
    e.process(buf.data(), callback);
    out.insert(out.end(), buf.begin(), buf.end());
    e.readDjStatus(st);
    if (st.plan.finished) break;
    if ((done / callback) % 20 == 0) e.collectGarbage();
  }
  if (last) *last = st;
  if (violations) *violations = counter.count();
  e.planStop();
  e.process(buf.data(), callback);
  e.collectGarbage();
  EXPECT_EQ(e.heldPlanCount(), 0u) << "the plan is handed back after PlanStop";
  return out;
}

/// Kick times: peaks of the envelope of the low-passed mono signal, refined by a parabola through the peak.
std::vector<double> kickTimes(const std::vector<float>& stereo, double fromSec, double toSec, double minGapSec) {
  const std::size_t n = stereo.size() / 2;
  std::vector<float> env(n);
  float lp = 0.f, e = 0.f;
  const float a = 1.f - std::exp(-2.f * 3.14159265f * 140.f / kSr);
  for (std::size_t i = 0; i < n; ++i) {
    const float m = 0.5f * (stereo[2 * i] + stereo[2 * i + 1]);
    lp += a * (m - lp);
    const float r = std::fabs(lp);
    e = r > e ? r : e + (r - e) * 0.002f;  // instant attack, ~10 ms release
    env[i] = e;
  }
  float mx = 0.f;
  const std::size_t i0 = static_cast<std::size_t>(fromSec * kSr), i1 = std::min(n - 1, static_cast<std::size_t>(toSec * kSr));
  for (std::size_t i = i0; i < i1; ++i) mx = std::max(mx, env[i]);
  std::vector<double> times;
  const std::size_t gap = static_cast<std::size_t>(minGapSec * kSr);
  for (std::size_t i = std::max<std::size_t>(i0, 2); i + 2 < i1; ++i) {
    if (env[i] > 0.5f * mx && env[i] >= env[i - 1] && env[i] > env[i + 1]) {
      if (!times.empty() && static_cast<double>(i) / kSr - times.back() < minGapSec) continue;
      (void)gap;
      const double y0 = env[i - 1], y1 = env[i], y2 = env[i + 1];
      const double d = (y0 - 2 * y1 + y2);
      const double shift = d != 0.0 ? 0.5 * (y0 - y2) / d : 0.0;
      times.push_back((static_cast<double>(i) + shift) / kSr);
    }
  }
  return times;
}

}  // namespace

// ------------------------------------------------------------------------------------------------------------------
// time map of the master clock
// ------------------------------------------------------------------------------------------------------------------

TEST(MixPlanTime, ConstantRampAndJumpAreConsistent) {
  MixPlan p;
  p.tempo = {{0.0, 120.0}, {32.0, 120.0}, {48.0, 132.0}, {80.0, 132.0}, {80.0, 100.0}, {96.0, 100.0}};
  EXPECT_NEAR(p.secondsBetween(0.0, 32.0), 16.0, 1e-9);                       // 32 beats at 120
  EXPECT_NEAR(p.secondsBetween(32.0, 48.0), 60.0 * std::log(132.0 / 120.0) / (12.0 / 16.0), 1e-9);  // closed form of the ramp
  EXPECT_NEAR(p.secondsBetween(48.0, 80.0), 32.0 * 60.0 / 132.0, 1e-9);
  EXPECT_NEAR(p.secondsBetween(80.0, 96.0), 16.0 * 60.0 / 100.0, 1e-9);        // jump to 100 bpm at beat 80
  EXPECT_NEAR(p.secondsBetween(0.0, 96.0), p.secondsBetween(0.0, 40.0) + p.secondsBetween(40.0, 96.0), 1e-9);
  EXPECT_NEAR(p.bpmAt(40.0), 126.0 - 6.0 + 0.0, 6.5);
  EXPECT_NEAR(p.bpmAt(40.0), 120.0 + 12.0 * 8.0 / 16.0, 1e-9);
  // beatAfter is the inverse of secondsBetween everywhere, also across segment borders
  for (double from : {0.0, 10.0, 31.9, 36.0, 47.99, 70.0, 79.99, 85.0}) {
    for (double sec : {0.001, 0.2, 1.5, 7.0, 12.0}) {
      const double to = p.beatAfter(from, sec);
      EXPECT_NEAR(p.secondsBetween(from, to), sec, 1e-7) << from << " + " << sec;
    }
  }
}

// ------------------------------------------------------------------------------------------------------------------
// mixer DSP
// ------------------------------------------------------------------------------------------------------------------

namespace {
double toneGain(const std::function<void(float*, float*, int)>& process, double f) {
  const int n = kSr;  // 1 s
  std::vector<float> l(n), r(n);
  for (int i = 0; i < n; ++i) l[i] = r[i] = static_cast<float>(0.5 * std::sin(2 * 3.14159265358979 * f * i / kSr));
  process(l.data(), r.data(), n);
  double a = 0, b = 0;
  for (int i = n / 2; i < n; ++i) {
    a += 0.25 * std::sin(2 * 3.14159265358979 * f * i / kSr) * std::sin(2 * 3.14159265358979 * f * i / kSr);
    b += static_cast<double>(l[i]) * l[i];
  }
  return 10.0 * std::log10(b / a);  // dB
}
}  // namespace

TEST(Eq3, FlatWhenAllGainsAreOneAndKillsTheBandsItShould) {
  Eq3 eq;
  eq.prepare(kSr);
  const float one[3] = {1.f, 1.f, 1.f};
  for (double f : {60.0, 150.0, 500.0, 1000.0, 3000.0, 8000.0}) {
    eq.reset();
    EXPECT_NEAR(toneGain([&](float* l, float* r, int n) { eq.process(l, r, n, one, one); }, f), 0.0, 0.01) << f << " Hz";
  }
  const float killLow[3] = {0.f, 1.f, 1.f};
  const float killMid[3] = {1.f, 0.f, 1.f};
  const float killHigh[3] = {1.f, 1.f, 0.f};
  eq.reset();
  EXPECT_LT(toneGain([&](float* l, float* r, int n) { eq.process(l, r, n, killLow, killLow); }, 60.0), -25.0);
  eq.reset();
  EXPECT_GT(toneGain([&](float* l, float* r, int n) { eq.process(l, r, n, killLow, killLow); }, 1000.0), -1.0);
  eq.reset();
  EXPECT_LT(toneGain([&](float* l, float* r, int n) { eq.process(l, r, n, killMid, killMid); }, 800.0), -12.0);
  eq.reset();
  EXPECT_LT(toneGain([&](float* l, float* r, int n) { eq.process(l, r, n, killHigh, killHigh); }, 9000.0), -25.0);
  eq.reset();
  EXPECT_GT(toneGain([&](float* l, float* r, int n) { eq.process(l, r, n, killHigh, killHigh); }, 500.0), -1.0);
}

TEST(SweepFilter, BypassAtCentreLowPassLeftHighPassRight) {
  SweepFilter f;
  f.prepare(kSr);
  EXPECT_NEAR(toneGain([&](float* l, float* r, int n) { f.process(l, r, n, 0.f, 0.f); }, 1000.0), 0.0, 1e-6);
  f.reset();
  EXPECT_LT(toneGain([&](float* l, float* r, int n) { f.process(l, r, n, -1.f, -1.f); }, 5000.0), -30.0);
  f.reset();
  EXPECT_GT(toneGain([&](float* l, float* r, int n) { f.process(l, r, n, -1.f, -1.f); }, 40.0), -3.0);
  f.reset();
  EXPECT_LT(toneGain([&](float* l, float* r, int n) { f.process(l, r, n, 1.f, 1.f); }, 100.0), -25.0);
  f.reset();
  EXPECT_GT(toneGain([&](float* l, float* r, int n) { f.process(l, r, n, 1.f, 1.f); }, 14000.0), -3.0);
}

// ------------------------------------------------------------------------------------------------------------------
// a deck on its own
// ------------------------------------------------------------------------------------------------------------------

namespace {
struct DeckRig {
  EngineConfig cfg;
  std::unique_ptr<Engine> e;
  DeckRig() {
    cfg.sampleRate = kSr;
    cfg.realtime = false;
    e = std::make_unique<Engine>(cfg);
    testutil::setByKey(*e, "deck.a.auto_trim", 0.f);
    testutil::setByKey(*e, "deck.b.auto_trim", 0.f);
  }
  std::vector<float> run(double seconds, int callback = 192) {
    return testutil::render(*e, static_cast<int>(seconds * kSr), callback);
  }
  DjStatus status() {
    DjStatus s;
    e->readDjStatus(s);
    return s;
  }
};
std::shared_ptr<const Track> sineTrack(double f, double seconds, int id = 1) {
  auto t = std::make_shared<Track>();
  t->id = id;
  t->audio = demo::sine(f, seconds, kSr, 0.5f);
  t->analysis.durationSec = seconds;
  return t;
}
}  // namespace

TEST(Deck, PlaysImmediatelyAtFullLevelAndKeepsPitchAtDifferentTempo) {
  DeckRig rig;
  auto t = sineTrack(440.0, 30.0);
  testutil::ViolationCounter counter;
  ASSERT_TRUE(rig.e->deckLoad(0, t));
  ASSERT_TRUE(rig.e->deckPlay(0, true));
  const auto a = rig.run(2.0);
  // no start latency: the first 20 ms (after a 4 ms fade-in) already carry the tone at about 0.5 * master 0.8
  EXPECT_GT(testutil::rms(a, 2 * 960, 2 * 1920), 0.2) << "deck output starts at once";
  EXPECT_NEAR(testutil::peak(a, 2 * 4800), 0.4f, 0.03f);
  EXPECT_NEAR(testutil::peakFrequency(a.data() + 2 * 24000, 24000, kSr, 400, 480, 2), 440.0, 0.3);
  EXPECT_EQ(counter.count(), 0u) << "the decks must not allocate or lock on the audio thread";

  testutil::setByKey(*rig.e, "deck.a.tempo_pct", 8.f);
  rig.run(1.0);  // let the tempo settle
  const auto b = rig.run(2.0);
  EXPECT_NEAR(testutil::peakFrequency(b.data() + 2 * 24000, 24000, kSr, 400, 480, 2), 440.0, 1.0) << "key lock keeps the pitch";
  const double before = rig.status().deck[0].posSec;
  rig.run(4.0);
  const double after = rig.status().deck[0].posSec;
  EXPECT_NEAR(after - before, 4.0 * 1.08, 0.01) << "the track advances at the new tempo";

  testutil::setByKey(*rig.e, "deck.a.keylock", 0.f);
  rig.run(1.0);
  const auto c = rig.run(2.0);
  EXPECT_NEAR(testutil::peakFrequency(c.data() + 2 * 24000, 24000, kSr, 440, 520, 2), 440.0 * 1.08, 1.5) << "without key lock the pitch follows the tempo";
  EXPECT_EQ(counter.count(), 0u);
}

TEST(Deck, PauseSeekAndEndOfTrack) {
  DeckRig rig;
  auto t = sineTrack(330.0, 3.0);
  rig.e->deckLoad(0, t);
  rig.e->deckPlay(0, true);
  rig.run(1.0);
  EXPECT_TRUE(rig.status().deck[0].playing);
  rig.e->deckPlay(0, false);
  rig.run(0.2);
  DjStatus s = rig.status();
  EXPECT_FALSE(s.deck[0].playing);
  const double paused = s.deck[0].posSec;
  EXPECT_NEAR(paused, 1.0, 0.1);
  const auto silent = rig.run(0.5);
  EXPECT_LT(testutil::peak(silent), 1e-4f) << "paused deck is silent";
  EXPECT_NEAR(rig.status().deck[0].posSec, paused, 1e-3);

  rig.e->deckSeek(0, 2.0);
  rig.e->deckPlay(0, true);
  rig.run(0.3);
  EXPECT_NEAR(rig.status().deck[0].posSec, 2.3, 0.05);
  rig.run(1.5);  // beyond the end at 3 s
  s = rig.status();
  EXPECT_TRUE(s.deck[0].ended);
  EXPECT_FALSE(s.deck[0].playing);

  // a loaded track is handed back when replaced: nothing leaks
  rig.e->deckLoad(0, sineTrack(200.0, 1.0, 2));
  rig.run(0.1);
  rig.e->deckLoad(0, nullptr);
  rig.run(0.1);
  rig.e->collectGarbage();
  EXPECT_EQ(rig.e->heldTrackCount(), 0u);
}

TEST(Deck, LoopRepeatsTheRegionAndLeavesCleanly) {
  DeckRig rig;
  rig.e->deckLoad(0, sineTrack(330.0, 20.0));
  rig.e->deckPlay(0, true);
  rig.run(1.0);
  // loop 4.0 .. 5.0 s, then jump into it: the position has to stay inside while the sound goes on
  rig.e->deckSeek(0, 4.2);
  rig.run(0.3);
  rig.e->deckLoop(0, true, 4.0, 1.0);
  double lo = 1e9, hi = -1e9;
  float minPeak = 1e9f;
  bool stillPlaying = true;
  for (int i = 0; i < 40; ++i) {  // 4 s = four turns of the loop
    const auto out = rig.run(0.1);
    minPeak = std::min(minPeak, testutil::peak(out));
    const DjStatus st = rig.status();
    const double pos = st.deck[0].posSec;
    stillPlaying = stillPlaying && st.deck[0].playing;
    lo = std::min(lo, pos);
    hi = std::max(hi, pos);
  }
  EXPECT_GE(lo, 3.95) << "never behind the loop start";
  EXPECT_LE(hi, 5.05) << "never past the loop end";
  EXPECT_GT(hi - lo, 0.7) << "the position actually wraps";
  EXPECT_GT(minPeak, 0.05f) << "no gap in the sound";
  EXPECT_TRUE(stillPlaying);

  rig.e->deckLoop(0, false);
  rig.run(0.5);
  const double after = rig.status().deck[0].posSec;
  rig.run(1.0);
  EXPECT_NEAR(rig.status().deck[0].posSec - after, 1.0, 0.1) << "plays on linearly once the loop is off";

  // a seek out of the loop ends it
  rig.e->deckLoop(0, true, 8.0, 0.5);
  rig.run(0.2);
  rig.e->deckSeek(0, 12.0);
  rig.run(1.0);
  EXPECT_NEAR(rig.status().deck[0].posSec, 13.0, 0.15);
}

TEST(Deck, PauseFollowedByASeekRestsAtTheNewPlace) {
  DeckRig rig;
  rig.e->deckLoad(0, songTrack(124.0, 0.0, 1));
  rig.e->deckPlay(0, true);
  rig.run(1.0);
  ASSERT_TRUE(rig.status().deck[0].playing);
  rig.e->deckPlay(0, false);
  rig.e->deckSeek(0, 20.0);  // both arrive before the audio thread has faded the deck out
  rig.run(0.5);
  const auto st = rig.status();
  EXPECT_FALSE(st.deck[0].playing) << "the seek must not undo the pause";
  EXPECT_NEAR(st.deck[0].posSec, 20.0, 0.01);
  rig.e->deckPlay(0, true);
  rig.run(0.5);
  EXPECT_NEAR(rig.status().deck[0].posSec, 20.5, 0.1);
}

TEST(Deck, CrossfaderCurvesAndMixerControls) {
  DeckRig rig;
  rig.e->deckLoad(0, sineTrack(440.0, 20.0, 1));
  rig.e->deckLoad(1, sineTrack(660.0, 20.0, 2));
  rig.e->deckPlay(0, true);
  rig.e->deckPlay(1, true);
  rig.run(0.5);
  auto level = [&](double f) {
    const auto o = rig.run(0.5);
    return testutil::dftMagnitude(o.data() + 2 * 4800, 24000 - 4800, f, kSr, 2) / (24000 - 4800) * 4.0;  // ~ amplitude (Hann coherent gain 0.5)
  };
  testutil::setByKey(*rig.e, "mixer.crossfader", 0.f);
  const double aMid = level(440.0), bMid = level(660.0);
  EXPECT_NEAR(aMid, bMid, 0.02) << "blend curve: both decks at full level in the middle";
  testutil::setByKey(*rig.e, "mixer.crossfader", -1.f);
  EXPECT_GT(level(440.0), 0.9 * aMid);
  EXPECT_LT(level(660.0), 0.01);
  testutil::setByKey(*rig.e, "mixer.crossfader", 1.f);
  EXPECT_LT(level(440.0), 0.01);
  testutil::setByKey(*rig.e, "mixer.crossfader", 0.f);
  testutil::setByKey(*rig.e, "deck.b.eq_mid_db", -26.f);  // 660 Hz sits in the mid band
  rig.run(0.3);
  EXPECT_LT(level(660.0), 0.2 * bMid) << "mid kill on deck B";
  EXPECT_NEAR(level(440.0), aMid, 0.03) << "deck A unaffected";
}

// ------------------------------------------------------------------------------------------------------------------
// planner
// ------------------------------------------------------------------------------------------------------------------

namespace {
bool onBarGrid(const PlanItem& it, double masterBeat) {
  const double b = it.trackBeatAt(masterBeat);
  return std::fabs(b / 4.0 - std::round(b / 4.0)) < 1e-9;
}
bool lanesAscend(const MixPlan& p) {
  for (int d = 0; d < 2; ++d) {
    for (int t = 0; t < static_cast<int>(LaneTarget::kCount); ++t) {
      const Lane& l = p.lanes[d][t];
      for (int i = 1; i < l.count; ++i) {
        if (p.keys[l.first + i].beat < p.keys[l.first + i - 1].beat) return false;
      }
    }
  }
  return true;
}
}  // namespace

TEST(Planner, CompatibleTemposGetABassSwapOnABarlineWithATempoRamp) {
  std::vector<std::shared_ptr<const Track>> list = {planOnlyTrack(124.0, 200.0, 1, "8A"), planOnlyTrack(126.0, 210.0, 2, "9A"),
                                                    planOnlyTrack(124.0, 190.0, 3, "8A")};
  const auto plan = planMix(list, {});
  ASSERT_EQ(plan->items.size(), 3u);
  ASSERT_EQ(plan->transitions.size(), 2u);
  const TransitionInfo& t0 = plan->transitions[0];
  EXPECT_EQ(t0.type, TransitionType::EqBlend);
  EXPECT_GE(t0.lengthBeats, 16.0);
  EXPECT_TRUE(t0.tempoRamp);
  EXPECT_NEAR(t0.bpmFrom, 124.0, 1e-9);
  EXPECT_NEAR(t0.bpmTo, 126.0, 1e-9);
  // the overlap starts on a downbeat of the outgoing and of the incoming song
  EXPECT_TRUE(onBarGrid(plan->items[0], t0.startBeat));
  EXPECT_TRUE(onBarGrid(plan->items[1], t0.startBeat));
  EXPECT_NEAR(plan->items[1].startBeat, t0.startBeat, 1e-9);
  EXPECT_NEAR(plan->items[0].endBeat, t0.startBeat + t0.lengthBeats, 1e-9);
  EXPECT_NE(plan->items[0].deck, plan->items[1].deck);
  EXPECT_EQ(plan->items[0].deck, plan->items[2].deck);
  EXPECT_TRUE(lanesAscend(*plan));
  // second transition leaves the second song after the first overlap is over
  EXPECT_GT(plan->transitions[1].startBeat, t0.startBeat + t0.lengthBeats + 7.9);
  EXPECT_GT(plan->items[2].startBeat, plan->items[0].endBeat - 1e-9) << "deck 0 is free again when song 3 needs it";
  EXPECT_NEAR(plan->totalSec, plan->timeOfBeat(plan->totalBeats), 1e-9);
  EXPECT_GT(plan->totalSec, 380.0);
  EXPECT_LT(plan->totalSec, 600.0);
  EXPECT_NEAR(plan->laneValue(plan->items[0].deck, LaneTarget::Gain, 0.0), 1.f, 1e-6);
  EXPECT_NEAR(plan->laneValue(plan->items[1].deck, LaneTarget::Gain, t0.startBeat), 0.f, 1e-6) << "incoming song starts silent";
  EXPECT_NEAR(plan->laneValue(plan->items[1].deck, LaneTarget::Gain, t0.startBeat + t0.lengthBeats), 1.f, 1e-6);
  EXPECT_NEAR(plan->laneValue(plan->items[0].deck, LaneTarget::Gain, t0.startBeat + t0.lengthBeats), 0.f, 1e-6);
}

TEST(Planner, TemposThatDoNotFitGetACutAndHalfTimeIsRecognised) {
  const auto cut = planMix({planOnlyTrack(100.0, 200.0, 1), planOnlyTrack(140.0, 200.0, 2)}, {});
  ASSERT_EQ(cut->transitions.size(), 1u);
  EXPECT_EQ(cut->transitions[0].type, TransitionType::Cut);
  EXPECT_EQ(cut->transitions[0].lengthBeats, 0.0);
  EXPECT_FALSE(cut->transitions[0].tempoRamp);
  EXPECT_TRUE(onBarGrid(cut->items[0], cut->transitions[0].startBeat));
  EXPECT_TRUE(onBarGrid(cut->items[1], cut->transitions[0].startBeat));
  EXPECT_NEAR(cut->items[0].endBeat, cut->items[1].startBeat, 1e-9) << "no overlap";

  const auto half = planMix({planOnlyTrack(140.0, 200.0, 1), planOnlyTrack(70.0, 200.0, 2)}, {});
  EXPECT_NEAR(half->items[1].beatMul, 2.0, 1e-9) << "a 70 bpm song counts as 140";
  EXPECT_NE(half->transitions[0].type, TransitionType::Cut);
  EXPECT_FALSE(half->transitions[0].tempoRamp) << "same effective tempo";

  const auto dbl = planMix({planOnlyTrack(80.0, 200.0, 1), planOnlyTrack(158.0, 200.0, 2)}, {});
  EXPECT_NEAR(dbl->items[1].beatMul, 0.5, 1e-9);
  EXPECT_NEAR(dbl->transitions[0].bpmTo, 79.0, 1e-9);
}

TEST(Planner, ShortSongsGetShorterOverlapsAndEverySongIsPlayedOnceInOrder) {
  std::vector<std::shared_ptr<const Track>> list;
  for (int i = 0; i < 6; ++i) list.push_back(planOnlyTrack(120.0 + i, 55.0 + 7.0 * i, i + 1, i % 2 ? "8A" : "9A"));
  PlanOptions o;
  o.lengthBeats = 32;
  const auto plan = planMix(list, o);
  ASSERT_EQ(plan->items.size(), 6u);
  double prevStart = -1.0;
  for (std::size_t i = 0; i < plan->items.size(); ++i) {
    const PlanItem& it = plan->items[i];
    EXPECT_EQ(it.track->id, static_cast<int>(i) + 1);
    EXPECT_GT(it.startBeat, prevStart) << "songs start in list order";
    EXPECT_GT(it.endBeat, it.startBeat);
    prevStart = it.startBeat;
  }
  for (const TransitionInfo& t : plan->transitions) {
    EXPECT_LE(t.lengthBeats, 32.0);
    // the overlap never runs past the end of the outgoing song's audio
    const PlanItem& out = plan->items[t.from];
    EXPECT_LE(out.trackBeatAt(t.startBeat + t.lengthBeats) * out.beatSec + out.gridOffsetSec, out.track->durationSec() + 1e-6);
  }
  EXPECT_TRUE(lanesAscend(*plan));
  for (std::size_t i = 0; i + 2 < plan->items.size(); ++i) {
    EXPECT_GE(plan->items[i + 2].startBeat, plan->items[i].endBeat - 1e-9) << "a deck is never needed by two songs at once";
  }
}

TEST(Planner, SmartOrderPutsCloseTemposAndKeysNextToEachOther) {
  std::vector<std::shared_ptr<const Track>> list = {planOnlyTrack(120.0, 200, 1, "8A"), planOnlyTrack(150.0, 200, 2, "3B"),
                                                    planOnlyTrack(122.0, 200, 3, "8A"), planOnlyTrack(148.0, 200, 4, "3B"),
                                                    planOnlyTrack(124.0, 200, 5, "9A")};
  const auto order = orderTracks(list, OrderMode::Smart);
  ASSERT_EQ(order.size(), 5u);
  EXPECT_EQ(order[0], 0);
  EXPECT_EQ(order[1], 2) << "122 bpm in 8A follows 120 bpm in 8A";
  EXPECT_EQ(order[2], 4);
  const auto up = orderTracks(list, OrderMode::TempoUp);
  EXPECT_EQ((std::vector<int>{0, 2, 4, 3, 1}), up);
  EXPECT_EQ((std::vector<int>{0, 1, 2, 3, 4}), orderTracks(list, OrderMode::AsListed));
}

// ------------------------------------------------------------------------------------------------------------------
// plan player: end to end through the engine
// ------------------------------------------------------------------------------------------------------------------

namespace {
struct KickStats {
  double median = 0.0, spread = 0.0;
  int count = 0;
};
/// Offsets of the observed kicks from the nearest master beat of the plan, within [fromSec, toSec].
KickStats kickOffsets(const std::vector<float>& stereo, const MixPlan& plan, double fromSec, double toSec) {
  const auto kicks = kickTimes(stereo, fromSec, toSec, 0.25);
  std::vector<double> err;
  const double beatSecApprox = 60.0 / plan.bpmAt(plan.beatAfter(0.0, fromSec));
  (void)beatSecApprox;
  for (double t : kicks) {
    // nearest integer master beat: search around the beat reached at t
    double m = plan.beatAfter(0.0, t);
    const double mr = std::round(m);
    double best = 1e9;
    for (double c : {mr - 1, mr, mr + 1}) {
      const double d = t - plan.timeOfBeat(c);
      if (std::fabs(d) < std::fabs(best)) best = d;
    }
    err.push_back(best);
  }
  KickStats s;
  s.count = static_cast<int>(err.size());
  if (err.empty()) return s;
  std::sort(err.begin(), err.end());
  s.median = err[err.size() / 2];
  for (double e : err) s.spread = std::max(s.spread, std::fabs(e - s.median));
  return s;
}
}  // namespace

TEST(PlanPlayer, TwoSongsOverlapWithBothKickGridsLockedToTheMasterClock) {
  auto A = songTrack(124.0, 0.0, 1);
  auto B = songTrack(128.0, 0.37, 2, 52, true, "9A");
  PlanOptions o;
  o.type = TransitionType::EqBlend;
  o.lengthBeats = 16;
  auto plan = planMix({A, B}, o);
  ASSERT_EQ(plan->transitions.size(), 1u);
  const TransitionInfo tr = plan->transitions[0];
  ASSERT_TRUE(tr.tempoRamp);
  const double end = tr.startSec + tr.lengthSec + 3.0;

  std::uint64_t violations = 0;
  DjStatus st;
  const auto mixed = renderPlan(plan, end, -1, 480, &st, &violations);
  EXPECT_EQ(violations, 0u) << "decks and plan player must not allocate, lock or throw on the audio thread";
  EXPECT_TRUE(testutil::allFinite(mixed));
  EXPECT_LT(testutil::peak(mixed), 1.0f);
  // no gaps: every second of the mix carries sound
  for (double s = 0.0; s + 1.0 <= end; s += 1.0) {
    EXPECT_GT(testutil::rms(mixed, static_cast<std::size_t>(s * kSr) * 2, static_cast<std::size_t>((s + 1.0) * kSr) * 2), 0.01) << "second " << s;
  }

  // each song on its own, measured against the master beats of the plan
  const auto onlyA = renderPlan(plan, end, 1);
  const auto onlyB = renderPlan(plan, end, 2);
  // reference: the constant offset of the kick-envelope peak at native tempo (no stretching): song A before the overlap
  const KickStats refA = kickOffsets(onlyA, *plan, 8.0, tr.startSec - 1.0);
  ASSERT_GT(refA.count, 8);
  EXPECT_LT(refA.spread, 0.0008) << "unstretched playback sits on the grid";
  // the bass swap sits in the middle of the overlap: song A is measured before it, song B (whose lows are killed until then) after it
  const double midBeat = tr.startBeat + 0.5 * tr.lengthBeats;
  const KickStats inA = kickOffsets(onlyA, *plan, plan->timeOfBeat(tr.startBeat + 1.0), plan->timeOfBeat(midBeat - 1.0));
  const KickStats inB = kickOffsets(onlyB, *plan, plan->timeOfBeat(midBeat + 1.0), plan->timeOfBeat(tr.startBeat + tr.lengthBeats - 0.5));
  ASSERT_GT(inA.count, 5);
  ASSERT_GT(inB.count, 5);
  printf("kick offsets [ms]: refA median %.3f spread %.3f | A in overlap median %.3f spread %.3f | B in overlap median %.3f spread %.3f\n",
         refA.median * 1e3, refA.spread * 1e3, inA.median * 1e3, inA.spread * 1e3, inB.median * 1e3, inB.spread * 1e3);
  EXPECT_NEAR(inA.median, refA.median, 0.0035) << "song A stays on the master grid while it is stretched";
  EXPECT_NEAR(inB.median, refA.median, 0.0035) << "song B sits on the same grid as song A";
  EXPECT_LT(inA.spread, 0.004);
  EXPECT_LT(inB.spread, 0.004);
  // after the overlap song B is alone and runs at its own tempo
  EXPECT_NEAR(st.plan.bpm, 128.0, 0.01);
}

TEST(PlanPlayer, ThreeSongsPlayThroughAndTheEngineHandsBackWhatItHolds) {
  auto A = songTrack(122.0, 0.0, 1, 57, true, "8A", 4, 8, 4, 4);
  auto B = songTrack(126.0, 0.1, 2, 60, false, "8B", 4, 8, 4, 4);
  auto C = songTrack(124.0, 0.0, 3, 52, true, "9A", 4, 8, 4, 4);
  auto plan = planMix({A, B, C}, {});
  ASSERT_EQ(plan->items.size(), 3u);
  std::uint64_t violations = 0;
  DjStatus st;
  const auto mixed = renderPlan(plan, plan->totalSec + 5.0, -1, 192, &st, &violations);
  EXPECT_EQ(violations, 0u);
  EXPECT_TRUE(st.plan.finished) << "the plan reaches its end by itself";
  EXPECT_NEAR(static_cast<double>(mixed.size() / 2) / kSr, plan->totalSec, 0.05) << "the mix is as long as planned";
  EXPECT_LT(testutil::peak(mixed), 1.0f);
  for (double s = 0.0; s + 1.0 <= plan->totalSec - 3.0; s += 1.0) {
    EXPECT_GT(testutil::rms(mixed, static_cast<std::size_t>(s * kSr) * 2, static_cast<std::size_t>((s + 1.0) * kSr) * 2), 0.01) << "gap at " << s << " s";
  }
}

TEST(PlanPlayer, CutTransitionStartsTheNextSongExactlyOnTheBar) {
  auto A = songTrack(100.0, 0.0, 1, 57, true, "8A", 4, 8, 4, 4);
  auto B = songTrack(150.0, 0.2, 2, 52, true, "3B", 4, 8, 4, 4);
  auto plan = planMix({A, B}, {});
  ASSERT_EQ(plan->transitions[0].type, TransitionType::Cut);
  const double T = plan->transitions[0].startSec;
  const auto only = renderPlan(plan, T + 6.0, 2);
  // song B is silent before T and its first kick lands on the downbeat at T (plus the constant envelope offset)
  EXPECT_LT(testutil::rms(only, 0, static_cast<std::size_t>((T - 0.05) * kSr) * 2), 1e-4);
  const auto kicks = kickTimes(only, T - 0.1, T + 4.0, 0.25);
  ASSERT_GT(kicks.size(), 4u);
  const double beat = 60.0 / 150.0;
  EXPECT_LT(kicks.front() - T, 0.02) << "first kick right at the cut";
  for (std::size_t i = 1; i < kicks.size(); ++i) EXPECT_NEAR(kicks[i] - kicks[i - 1], beat, 0.002);
}

// ------------------------------------------------------------------------------------------------------------------
// live sync of two decks
// ------------------------------------------------------------------------------------------------------------------

TEST(Deck, SyncMatchesTempoAndPullsTheBeatsTogether) {
  auto A = songTrack(124.0, 0.0, 1);
  auto B = songTrack(128.0, 0.21, 2, 52, true, "9A");
  auto run = [&](int mask, DjStatus* st) {
    EngineConfig cfg;
    cfg.sampleRate = kSr;
    cfg.realtime = false;
    Engine e(cfg);
    testutil::setByKey(e, "deck.a.auto_trim", 0.f);
    testutil::setByKey(e, "deck.b.auto_trim", 0.f);
    e.djNode().setDebugAudibleMask(mask);
    e.deckLoad(0, A);
    e.deckLoad(1, B);
    e.deckSeek(1, 3.1);  // somewhere that is clearly not aligned with deck A
    testutil::setByKey(e, "deck.b.sync", 1.f);
    e.deckPlay(0, true);
    testutil::render(e, kSr / 2, 192);
    e.deckPlay(1, true);
    auto out = testutil::render(e, 24 * kSr, 192);
    if (st) e.readDjStatus(*st);
    return out;
  };
  DjStatus st;
  const auto onlyA = run(1, &st);
  const auto onlyB = run(2, nullptr);
  EXPECT_NEAR(st.deck[1].bpm, 124.0, 0.05) << "deck B follows deck A's tempo";
  const double da = st.deck[0].beatPhase - st.deck[1].beatPhase;
  const double wrapped = da - std::round(da);
  EXPECT_LT(std::fabs(wrapped), 0.02) << "beat phases match";
  // audible check: kicks of A and B coincide in the last seconds
  const auto ka = kickTimes(onlyA, 18.0, 23.5, 0.25);
  const auto kb = kickTimes(onlyB, 18.0, 23.5, 0.25);
  ASSERT_GT(ka.size(), 6u);
  ASSERT_GT(kb.size(), 6u);
  double worst = 0.0;
  for (double t : kb) {
    double best = 1e9;
    for (double u : ka) best = std::min(best, std::fabs(t - u));
    worst = std::max(worst, best);
  }
  printf("worst kick distance deck A / deck B after sync: %.2f ms\n", worst * 1e3);
  EXPECT_LT(worst, 0.006) << "the stretcher moves a kick's envelope peak by a few ms at most; far below an audible flam";
}

TEST(Deck, CpuCostOfTwoStretchedDecks) {
  DeckRig rig;
  rig.e->deckLoad(0, songTrack(124.0, 0.0, 1));
  rig.e->deckLoad(1, songTrack(128.0, 0.0, 2));
  testutil::setByKey(*rig.e, "deck.a.tempo_pct", 5.f);
  testutil::setByKey(*rig.e, "deck.b.tempo_pct", -5.f);
  rig.e->deckPlay(0, true);
  rig.e->deckPlay(1, true);
  rig.run(1.0);
  const auto t0 = std::chrono::steady_clock::now();
  rig.run(20.0);
  const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  printf("two stretched decks: %.1f%% of one core on this host (realtime factor %.1fx)\n", 100.0 * wall / 20.0, 20.0 / wall);
  EXPECT_LT(wall / 20.0, 0.35);
}

// ------------------------------------------------------------------------------------------------------------------
// audio bound lazily to a plan, and the offline export
// ------------------------------------------------------------------------------------------------------------------

namespace {
std::shared_ptr<Track> stubOf(const Track& t) {
  auto s = std::make_shared<Track>();
  s->id = t.id;
  s->name = t.name;
  s->analysis = t.analysis;  // no audio
  return s;
}
}  // namespace

TEST(LazyAudio, PlanFromStubsSoundsLikeThePlanFromFullTracks) {
  std::vector<std::shared_ptr<Track>> songs = {songTrack(122.0, 0.0, 1, 57, true, "8A", 4, 8, 4, 4), songTrack(124.0, 0.1, 2, 52, true, "9A", 4, 8, 4, 4),
                                               songTrack(126.0, 0.0, 3, 57, true, "8A", 4, 8, 4, 4)};
  auto full = planMix({songs[0], songs[1], songs[2]}, {});
  std::vector<std::shared_ptr<const Track>> stubs;
  for (auto& s : songs) stubs.push_back(stubOf(*s));
  auto lazy = planMix(stubs, {});
  ASSERT_EQ(lazy->items.size(), 3u);
  EXPECT_NEAR(lazy->totalSec, full->totalSec, 1e-9) << "the plan needs the analysis only";
  for (int i = 0; i < 3; ++i) EXPECT_FALSE(lazy->audioBound(i));
  for (int i = 0; i < 3; ++i) EXPECT_TRUE(full->audioBound(i));
  for (int i = 0; i < 3; ++i) ASSERT_TRUE(lazy->bindAudio(i, songs[static_cast<std::size_t>(i)]));
  EXPECT_FALSE(lazy->bindAudio(1, stubs[1])) << "a stub has no audio to bind";
  const auto a = renderPlan(full, full->totalSec + 1.0, -1, 192);
  const auto b = renderPlan(lazy, full->totalSec + 1.0, -1, 192);
  ASSERT_EQ(a.size(), b.size());
  std::size_t diff = 0;
  for (std::size_t i = 0; i < a.size(); ++i) diff += a[i] != b[i];
  EXPECT_EQ(diff, 0u);
}

TEST(LazyAudio, ASongBoundLateJoinsInAtTheRightPositionAndNothingBreaks) {
  auto A = songTrack(124.0, 0.0, 1, 57, true, "8A", 4, 8, 4, 4);
  auto B = songTrack(124.0, 0.0, 2, 52, true, "9A", 4, 8, 4, 4);
  auto plan = planMix({stubOf(*A), stubOf(*B)}, {});
  ASSERT_EQ(plan->transitions.size(), 1u);
  plan->bindAudio(0, A);
  EngineConfig cfg;
  cfg.sampleRate = kSr;
  cfg.realtime = false;
  Engine e(cfg);
  testutil::ViolationCounter counter;
  e.planStart(plan);
  const double T = plan->transitions[0].startSec;
  const int cb = 192;
  std::vector<float> buf(cb * 2), all;
  DjStatus st;
  bool sawWaiting = false;
  auto run = [&](double untilSec) {
    while (static_cast<double>(all.size() / 2) / kSr < untilSec) {
      e.process(buf.data(), cb);
      all.insert(all.end(), buf.begin(), buf.end());
      e.readDjStatus(st);
      sawWaiting = sawWaiting || st.plan.waiting == 1;
      e.collectGarbage();
    }
  };
  run(T + plan->transitions[0].lengthSec * 0.5);  // B's slot has begun, its audio is missing
  EXPECT_TRUE(sawWaiting) << "the player reports the song it is waiting for";
  const std::size_t mid = all.size();
  plan->bindAudio(1, B);
  run(T + plan->transitions[0].lengthSec + 4.0);
  EXPECT_EQ(st.plan.waiting, -1);
  EXPECT_EQ(st.deck[plan->items[1].deck].trackId, 2) << "song B plays after it arrived";
  EXPECT_GT(testutil::rms(all, mid, all.size()), 0.02);
  EXPECT_TRUE(testutil::allFinite(all));
  EXPECT_EQ(counter.count(), 0u);
  // handing the audio back after the song ended must be safe
  plan->unbindAudio(0);
  e.planStop();
  e.process(buf.data(), cb);
  e.collectGarbage();
}

namespace {
struct WavData {
  int rate = 0, bits = 0, channels = 0;
  std::vector<float> samples;
};
bool readWav(const std::string& path, WavData& w) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) return false;
  std::vector<std::uint8_t> d;
  std::uint8_t tmp[4096];
  std::size_t n;
  while ((n = std::fread(tmp, 1, sizeof tmp, f)) > 0) d.insert(d.end(), tmp, tmp + n);
  std::fclose(f);
  if (d.size() < 44 || std::memcmp(d.data(), "RIFF", 4) != 0 || std::memcmp(d.data() + 8, "WAVE", 4) != 0) return false;
  auto u32 = [&](std::size_t o) { return static_cast<std::uint32_t>(d[o] | d[o + 1] << 8 | d[o + 2] << 16 | static_cast<std::uint32_t>(d[o + 3]) << 24); };
  w.channels = d[22] | d[23] << 8;
  w.rate = static_cast<int>(u32(24));
  w.bits = d[34] | d[35] << 8;
  const std::size_t bytes = u32(40);
  if (44 + bytes > d.size()) return false;
  const int bp = w.bits / 8;
  for (std::size_t o = 44; o + static_cast<std::size_t>(bp) <= 44 + bytes; o += static_cast<std::size_t>(bp)) {
    if (bp == 2) w.samples.push_back(static_cast<float>(static_cast<std::int16_t>(d[o] | d[o + 1] << 8)) / 32768.f);
    else {
      std::int32_t v = d[o] | d[o + 1] << 8 | d[o + 2] << 16;
      if (v & 0x800000) v -= 0x1000000;
      w.samples.push_back(static_cast<float>(v) / 8388608.f);
    }
  }
  return true;
}
}  // namespace

TEST(MixExport, WavMatchesWhatPlaybackWouldHaveSoundedLike) {
  auto A = songTrack(122.0, 0.0, 1, 57, true, "8A", 4, 8, 4, 4);
  auto B = songTrack(125.0, 0.1, 2, 52, true, "9A", 4, 8, 4, 4);
  auto plan = planMix({stubOf(*A), stubOf(*B)}, {});
  plan->bindAudio(0, A);
  plan->bindAudio(1, B);
  const std::string path = ::testing::TempDir() + "export_test.wav";
  MixExporter ex(plan, kSr, 16);
  ASSERT_TRUE(ex.open(path));
  while (ex.step(192 * 25)) {
  }
  EXPECT_TRUE(ex.error().empty()) << ex.error();
  EXPECT_TRUE(ex.close());
  WavData w;
  ASSERT_TRUE(readWav(path, w));
  EXPECT_EQ(w.rate, kSr);
  EXPECT_EQ(w.channels, 2);
  EXPECT_EQ(w.bits, 16);
  EXPECT_NEAR(static_cast<double>(w.samples.size() / 2) / kSr, plan->totalSec, 0.05);
  auto ref = [&] {
    auto full = planMix({A, B}, {});
    return renderPlan(full, full->totalSec, -1, 192);
  }();
  const std::size_t n = std::min(ref.size(), w.samples.size());
  double maxDiff = 0.0;
  for (std::size_t i = 0; i < n; ++i) maxDiff = std::max(maxDiff, static_cast<double>(std::fabs(ref[i] - w.samples[i])));
  EXPECT_LT(maxDiff, 3.0 / 32768.0 + 1e-6) << "only 16-bit rounding and dither differ from the live render";
  EXPECT_GT(ex.peak(), 0.1f);
  std::remove(path.c_str());
}

TEST(MixExport, TwentyFourBitAndCancel) {
  auto A = songTrack(124.0, 0.0, 1, 57, true, "8A", 4, 8, 4, 4);
  auto plan = planMix({A}, {});
  const std::string path = ::testing::TempDir() + "export_test24.wav";
  {
    MixExporter ex(plan, kSr, 24);
    ASSERT_TRUE(ex.open(path));
    while (ex.step(4800)) {
    }
    ASSERT_TRUE(ex.close());
    WavData w;
    ASSERT_TRUE(readWav(path, w));
    EXPECT_EQ(w.bits, 24);
    EXPECT_GT(w.samples.size(), static_cast<std::size_t>(kSr) * 2 * 10);
  }
  {
    MixExporter ex(plan, kSr, 16);
    ASSERT_TRUE(ex.open(path));
    ex.step(4800);
    EXPECT_FALSE(ex.close(true)) << "a cancelled export reports no success";
    std::FILE* f = std::fopen(path.c_str(), "rb");
    EXPECT_EQ(f, nullptr) << "and leaves no half-written file behind";
    if (f) std::fclose(f);
  }
}
