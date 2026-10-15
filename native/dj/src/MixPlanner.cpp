#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>

#include "autodj/dj/DjDsp.h"
#include "autodj/dj/MixPlan.h"

namespace autodj::dj {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr float kEqKillDbForPlan = kEqKillDb;  // at the kill threshold a band is switched off completely

struct Info {
  const Track* t = nullptr;
  bool hasTempo = false;
  bool trusted = false;
  double nativeBpm = 120.0;  // the tempo the grid is built on (a neighbour's tempo is borrowed for songs without one)
  double beatSec = 0.5;
  double gridOff = 0.0;
  double duration = 0.0;
  double beatMul = 1.0;
  double effBpm = 120.0;
  double lastBeat = 0.0;  // beat index of the end of the audio
  double bIn = 0.0;       // beat index where this song enters (bar aligned)
};

double ceilBar(double k) { return 4.0 * std::ceil(k / 4.0 - 1e-6); }
double floorBar(double k) { return 4.0 * std::floor(k / 4.0 + 1e-6); }

struct LaneBuilder {
  std::vector<LaneKey> k[2][static_cast<int>(LaneTarget::kCount)];

  void add(int deck, LaneTarget t, double beat, float value) {
    auto& v = k[deck & 1][static_cast<int>(t)];
    if (!v.empty()) beat = std::max(beat, v.back().beat);  // lanes stay ascending even when two transitions touch
    v.push_back({beat, value});
  }
  void flatten(MixPlan& p) const {
    for (int d = 0; d < 2; ++d) {
      for (int t = 0; t < static_cast<int>(LaneTarget::kCount); ++t) {
        const auto& v = k[d][t];
        p.lanes[d][t] = {static_cast<int>(p.keys.size()), static_cast<int>(v.size())};
        p.keys.insert(p.keys.end(), v.begin(), v.end());
      }
    }
  }
};

double tempoGapRatio(double a, double b) { return std::fabs(std::log(a / b)); }

/// beatMul in {1, 2, 0.5} that brings the song's tempo closest to the previous one.
double bestBeatMul(double nativeBpm, double target) {
  double best = 1.0, bestGap = tempoGapRatio(nativeBpm, target);
  for (double m : {2.0, 0.5}) {
    const double g = tempoGapRatio(nativeBpm * m, target);
    if (g < bestGap - 1e-9) {
      bestGap = g;
      best = m;
    }
  }
  return best;
}

void equalPower(LaneBuilder& lb, int deckOut, int deckIn, double T, double L) {
  for (int s = 0; s <= 8; ++s) {
    const double f = s / 8.0;
    lb.add(deckOut, LaneTarget::Gain, T + f * L, static_cast<float>(std::cos(f * kPi / 2)));
    lb.add(deckIn, LaneTarget::Gain, T + f * L, static_cast<float>(std::sin(f * kPi / 2)));
  }
}

void bassSwap(LaneBuilder& lb, int deckOut, int deckIn, double T, double L) {
  const double mid = T + 0.5 * L;
  lb.add(deckIn, LaneTarget::EqLow, T, kEqKillDbForPlan);
  lb.add(deckIn, LaneTarget::EqLow, mid - 0.5, kEqKillDbForPlan);
  lb.add(deckIn, LaneTarget::EqLow, mid + 0.5, 0.f);
  lb.add(deckOut, LaneTarget::EqLow, T, 0.f);
  lb.add(deckOut, LaneTarget::EqLow, mid - 0.5, 0.f);
  lb.add(deckOut, LaneTarget::EqLow, mid + 0.5, kEqKillDbForPlan);
}

}  // namespace

std::shared_ptr<MixPlan> planMix(const std::vector<std::shared_ptr<const Track>>& tracks, const PlanOptions& opt) {
  auto plan = std::make_shared<MixPlan>();
  const int n = static_cast<int>(tracks.size());
  if (n == 0) {
    plan->initAudio();
    return plan;
  }

  // ---- per-track facts and tempo relation to the previous track -------------------------------------
  std::vector<Info> info(n);
  for (int i = 0; i < n; ++i) {
    Info& f = info[i];
    f.t = tracks[i].get();
    const auto& a = f.t->analysis;
    f.duration = f.t->analysis.durationSec > 0.0 ? f.t->analysis.durationSec : f.t->durationSec();
    f.hasTempo = a.hasTempo() && a.beatSec > 0.0;
    f.trusted = f.hasTempo && a.gridConfidence >= 0.35;
  }
  std::vector<bool> matched(n, true);  // matched[i]: song i can follow song i-1 with a tempo ramp
  for (int i = 0; i < n; ++i) {
    Info& f = info[i];
    const auto& a = f.t->analysis;
    if (i == 0) {
      f.nativeBpm = f.hasTempo ? a.bpm : 120.0;
      f.beatMul = 1.0;
    } else if (!f.hasTempo) {
      f.nativeBpm = info[i - 1].effBpm;  // no tempo found: play at the neighbour's tempo, unstretched
      f.beatMul = 1.0;
    } else {
      f.nativeBpm = a.bpm;
      f.beatMul = bestBeatMul(f.nativeBpm, info[i - 1].effBpm);
      const double gap = std::fabs(f.nativeBpm * f.beatMul / info[i - 1].effBpm - 1.0) * 100.0;
      if (gap > opt.maxTempoGapPercent) {
        matched[i] = false;
        f.beatMul = 1.0;
      }
    }
    f.effBpm = f.nativeBpm * f.beatMul;
    f.beatSec = f.hasTempo ? a.beatSec : 60.0 / f.nativeBpm;
    f.gridOff = f.hasTempo ? a.gridOffsetSec : 0.0;
    f.lastBeat = (f.duration - f.gridOff) / f.beatSec;
    if (i == 0 && !opt.startAtMixIn) {
      f.bIn = -f.gridOff / f.beatSec;  // exactly the start of the file
    } else {
      f.bIn = ceilBar((a.mixInSec - f.gridOff) / f.beatSec);
    }
    if (f.bIn < -f.gridOff / f.beatSec) f.bIn = ceilBar(-f.gridOff / f.beatSec);
  }

  // ---- timeline ---------------------------------------------------------------------------------------
  plan->items.resize(n);
  for (int i = 0; i < n; ++i) {
    PlanItem& it = plan->items[i];
    it.track = tracks[i];
    it.deck = i & 1;
    it.beatMul = info[i].beatMul;
    it.beatSec = info[i].beatSec;
    it.gridOffsetSec = info[i].gridOff;
    it.gridTrusted = info[i].trusted;
    it.trackBeat0 = info[i].bIn;
  }
  plan->items[0].startBeat = 0.0;

  struct Pair {
    TransitionType type = TransitionType::EqBlend;
    double T = 0.0, L = 0.0;
    bool ramp = false;
    std::string note;
  };
  std::vector<Pair> pairs(std::max(0, n - 1));
  double minStartTrackBeat = info[0].bIn;  // earliest track beat at which item i may start its transition
  for (int i = 0; i + 1 < n; ++i) {
    const Info& o = info[i];
    const Info& in = info[i + 1];
    PlanItem& oi = plan->items[i];
    Pair& p = pairs[i];
    const bool isMatched = matched[i + 1];
    const bool bothTrusted = o.trusted && in.trusted;
    const bool harmonic = !o.t->analysis.camelot.empty() && !in.t->analysis.camelot.empty() &&
                          analysis::camelotCompatible(o.t->analysis.camelot, in.t->analysis.camelot);

    // type and preferred length
    TransitionType type = opt.type;
    int lenPref = opt.lengthBeats > 0 ? opt.lengthBeats : 16;
    std::string note;
    if (!isMatched) {
      type = TransitionType::Cut;
      note = "Tempo passt nicht zusammen: harter Schnitt auf dem Taktanfang";
    } else if (type == TransitionType::Auto) {
      if (!bothTrusted) {
        type = TransitionType::Crossfade;
        lenPref = opt.lengthBeats > 0 ? opt.lengthBeats : 8;
        note = "Beat-Raster unsicher: weiche Überblendung";
      } else if (harmonic) {
        type = (i % 3 == 2) ? TransitionType::FilterSweep : TransitionType::EqBlend;
        if (opt.lengthBeats == 0 && type == TransitionType::EqBlend) lenPref = 32;
        note = type == TransitionType::EqBlend ? "Tonarten passen: Bass-Tausch über 8 Takte" : "Filter-Übergang";
      } else {
        type = (i % 2 == 0) ? TransitionType::FilterSweep : TransitionType::Crossfade;
        note = type == TransitionType::FilterSweep ? "Tonarten passen nicht ideal: Filter-Übergang" : "Tonarten passen nicht ideal: weiche Überblendung";
      }
    } else if (note.empty()) {
      note = "";
    }
    if (type == TransitionType::Cut) lenPref = 0;

    // outgoing window: bar aligned, after the previous transition, ending before the audio does
    const double lenOutPerBeat = 1.0 / oi.beatMul;
    const double bPref = floorBar((o.t->analysis.mixOutSec - o.gridOff) / o.beatSec);
    const double minStart = ceilBar(minStartTrackBeat);
    double L = lenPref;
    double bX = 0.0;
    auto fits = [&](double len) {
      const double bxMax = floorBar(o.lastBeat - 2.0 - len * lenOutPerBeat);
      const double inAvail = in.lastBeat - in.bIn - 2.0;
      if (len / in.beatMul > inAvail * 0.6) return false;  // the incoming song must have a body left after the overlap
      bX = std::min(std::max(bPref, minStart), bxMax);
      return bX >= minStart;
    };
    if (type != TransitionType::Cut) {
      bool ok = fits(L);
      while (!ok && L > 4.0) {
        L = L / 2.0;
        ok = fits(L);
      }
      if (!ok) {
        type = TransitionType::Cut;
        L = 0.0;
        note = "Song zu kurz für eine Überblendung: harter Schnitt";
      }
    }
    if (type == TransitionType::Cut) {
      L = 0.0;
      bX = std::max(minStart, std::min(bPref, floorBar(o.lastBeat - 1.0)));
      if (bX < minStart) bX = minStart;
    } else if (L < lenPref && note.empty()) {
      note = "Überblendung verkürzt (Song zu kurz)";
    }
    p.type = type;
    p.L = L;
    p.ramp = type != TransitionType::Cut && isMatched;
    p.note = note;
    p.T = oi.startBeat + (bX - info[i].bIn) * oi.beatMul;  // master beat where the transition starts (an outgoing downbeat)
    oi.endBeat = p.T + L;
    PlanItem& ni = plan->items[i + 1];
    ni.startBeat = p.T;
    minStartTrackBeat = in.bIn + L / in.beatMul + 8.0;
  }
  {
    PlanItem& last = plan->items[n - 1];
    last.endBeat = last.startBeat + (info[n - 1].lastBeat - info[n - 1].bIn) * last.beatMul;
  }
  plan->totalBeats = plan->items[n - 1].endBeat;

  // ---- tempo curve ------------------------------------------------------------------------------------
  plan->tempo.push_back({0.0, info[0].effBpm});
  for (int i = 0; i + 1 < n; ++i) {
    const Pair& p = pairs[i];
    plan->tempo.push_back({p.T, info[i].effBpm});
    plan->tempo.push_back({p.ramp ? p.T + p.L : p.T, info[i + 1].effBpm});
  }
  plan->totalSec = plan->timeOfBeat(plan->totalBeats);

  // ---- automation lanes ---------------------------------------------------------------------------------
  LaneBuilder lb;
  lb.add(plan->items[0].deck, LaneTarget::Gain, 0.0, 1.f);
  for (int i = 0; i + 1 < n; ++i) {
    const Pair& p = pairs[i];
    const int dOut = plan->items[i].deck;
    const int dIn = plan->items[i + 1].deck;
    const double T = p.T, L = p.L;
    // a deck that is about to be used again starts from a neutral tone setting
    for (LaneTarget t : {LaneTarget::EqLow, LaneTarget::EqMid, LaneTarget::EqHigh, LaneTarget::Filter}) lb.add(dIn, t, T, 0.f);
    switch (p.type) {
      case TransitionType::Cut:
        lb.add(dIn, LaneTarget::Gain, T, 1.f);
        lb.add(dOut, LaneTarget::Gain, T, 0.f);
        break;
      case TransitionType::Crossfade:
        equalPower(lb, dOut, dIn, T, L);
        break;
      case TransitionType::FilterSweep:
        bassSwap(lb, dOut, dIn, T, L);
        lb.add(dIn, LaneTarget::Gain, T, 0.f);
        lb.add(dIn, LaneTarget::Gain, T + 0.15 * L, 1.f);
        lb.add(dOut, LaneTarget::Gain, T, 1.f);
        lb.add(dOut, LaneTarget::Gain, T + 0.6 * L, 1.f);
        lb.add(dOut, LaneTarget::Gain, T + L, 0.f);
        lb.add(dIn, LaneTarget::Filter, T, -0.85f);
        lb.add(dIn, LaneTarget::Filter, T + L, 0.f);
        lb.add(dOut, LaneTarget::Filter, T, 0.f);
        lb.add(dOut, LaneTarget::Filter, T + 0.8 * L, 0.8f);
        break;
      case TransitionType::EqBlend:
      default:
        bassSwap(lb, dOut, dIn, T, L);
        lb.add(dIn, LaneTarget::Gain, T, 0.f);
        lb.add(dIn, LaneTarget::Gain, T + 0.12 * L, 1.f);
        lb.add(dOut, LaneTarget::Gain, T, 1.f);
        lb.add(dOut, LaneTarget::Gain, T + 0.55 * L, 1.f);
        lb.add(dOut, LaneTarget::Gain, T + L, 0.f);
        break;
    }
  }
  lb.flatten(*plan);

  // ---- report ----------------------------------------------------------------------------------------------
  for (int i = 0; i + 1 < n; ++i) {
    const Pair& p = pairs[i];
    TransitionInfo t;
    t.from = i;
    t.to = i + 1;
    t.type = p.type;
    t.startBeat = p.T;
    t.lengthBeats = p.L;
    t.startSec = plan->timeOfBeat(p.T);
    t.lengthSec = plan->secondsBetween(p.T, p.T + p.L);
    t.bpmFrom = info[i].effBpm;
    t.bpmTo = info[i + 1].effBpm;
    t.tempoRamp = p.ramp && std::fabs(t.bpmFrom - t.bpmTo) > 0.05;
    t.note = p.note;
    plan->transitions.push_back(std::move(t));
  }
  plan->initAudio();
  return plan;
}

std::vector<int> orderTracks(const std::vector<std::shared_ptr<const Track>>& tracks, OrderMode mode) {
  const int n = static_cast<int>(tracks.size());
  std::vector<int> order(n);
  std::iota(order.begin(), order.end(), 0);
  if (n < 3 || mode == OrderMode::AsListed) return order;
  auto bpmOf = [&](int i) { return tracks[i]->analysis.bpm; };
  if (mode == OrderMode::TempoUp || mode == OrderMode::TempoDown) {
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
      const double x = bpmOf(a), y = bpmOf(b);
      if (x <= 0.0 || y <= 0.0) return x > 0.0 && y <= 0.0;  // songs without tempo go last
      return mode == OrderMode::TempoUp ? x < y : x > y;
    });
    return order;
  }
  // Smart: greedy walk from the first song
  auto cost = [&](int a, int b) {
    const auto& A = tracks[a]->analysis;
    const auto& B = tracks[b]->analysis;
    double c = 0.0;
    if (A.hasTempo() && B.hasTempo()) {
      double best = 1e9;
      for (double m : {1.0, 2.0, 0.5}) best = std::min(best, std::fabs(B.bpm * m / A.bpm - 1.0) * 100.0);
      c += best * 1.0;  // a 4 % gap costs 4
      if (best > 12.0) c += 30.0;
    } else {
      c += 25.0;
    }
    if (!A.camelot.empty() && !B.camelot.empty()) {
      if (A.camelot == B.camelot) c += 0.0;
      else if (analysis::camelotCompatible(A.camelot, B.camelot)) c += 1.5;
      else c += 8.0;
    } else {
      c += 4.0;
    }
    c += std::fabs(A.rmsDb - B.rmsDb) * 0.3;
    return c;
  };
  std::vector<int> out;
  std::vector<bool> used(n, false);
  out.push_back(0);
  used[0] = true;
  while (static_cast<int>(out.size()) < n) {
    int best = -1;
    double bestCost = 1e18;
    for (int j = 0; j < n; ++j) {
      if (used[j]) continue;
      const double c = cost(out.back(), j);
      if (c < bestCost) {
        bestCost = c;
        best = j;
      }
    }
    used[best] = true;
    out.push_back(best);
  }
  return out;
}

}  // namespace autodj::dj
