#include "autodj/dj/MixPlan.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <nlohmann/json.hpp>

namespace autodj::dj {

const char* transitionName(TransitionType t) noexcept {
  switch (t) {
    case TransitionType::Auto: return "auto";
    case TransitionType::EqBlend: return "eq_blend";
    case TransitionType::Crossfade: return "crossfade";
    case TransitionType::FilterSweep: return "filter_sweep";
    case TransitionType::Cut: return "cut";
    default: return "auto";
  }
}

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

/// Seconds for the stretch [x0, x1] of one tempo segment whose bpm runs linearly from u0 at x0 to u1 at x1.
double segmentSeconds(double x0, double x1, double u0, double u1) noexcept {
  const double len = x1 - x0;
  if (len <= 0.0) return 0.0;
  if (std::fabs(u1 - u0) < 1e-9 * std::max(u0, 1.0)) return 60.0 * len / u0;
  const double s = (u1 - u0) / len;
  return 60.0 * std::log(u1 / u0) / s;
}

}  // namespace

double MixPlan::bpmAt(double beat) const noexcept {
  if (tempo.empty()) return 120.0;
  if (beat <= tempo.front().beat) return tempo.front().bpm;
  if (beat >= tempo.back().beat) return tempo.back().bpm;
  // last point with .beat <= beat; a jump (two points at one beat) resolves to the later one
  std::size_t lo = 0, hi = tempo.size() - 1;
  while (hi - lo > 1) {
    const std::size_t mid = (lo + hi) / 2;
    if (tempo[mid].beat <= beat) lo = mid; else hi = mid;
  }
  const TempoPoint& a = tempo[lo];
  const TempoPoint& b = tempo[hi];
  if (b.beat <= a.beat) return b.bpm;
  return a.bpm + (b.bpm - a.bpm) * (beat - a.beat) / (b.beat - a.beat);
}

double MixPlan::secondsBetween(double a, double b) const noexcept {
  if (b <= a) return 0.0;
  if (tempo.empty()) return 60.0 * (b - a) / 120.0;
  double t = 0.0;
  double x = a;
  // before the first point: constant
  if (x < tempo.front().beat) {
    const double e = std::min(b, tempo.front().beat);
    t += 60.0 * (e - x) / tempo.front().bpm;
    x = e;
  }
  for (std::size_t i = 0; i + 1 < tempo.size() && x < b; ++i) {
    const TempoPoint& p = tempo[i];
    const TempoPoint& q = tempo[i + 1];
    if (q.beat <= p.beat || x >= q.beat) continue;
    const double x1 = std::min(b, q.beat);
    const double s = (q.bpm - p.bpm) / (q.beat - p.beat);
    t += segmentSeconds(x, x1, p.bpm + s * (x - p.beat), p.bpm + s * (x1 - p.beat));
    x = x1;
  }
  if (x < b) t += 60.0 * (b - x) / tempo.back().bpm;
  return t;
}

double MixPlan::beatAfter(double from, double seconds) const noexcept {
  if (seconds <= 0.0) return from;
  if (tempo.empty()) return from + seconds * 120.0 / 60.0;
  double m = from;
  double remaining = seconds;
  // each pass consumes the rest of the segment that contains m (or finishes inside it)
  for (int guard = 0; guard < 4096; ++guard) {
    const double u = bpmAt(m);
    double end = kInf;
    double s = 0.0;
    if (m < tempo.front().beat) {
      end = tempo.front().beat;
    } else {
      for (std::size_t i = 0; i + 1 < tempo.size(); ++i) {
        const TempoPoint& p = tempo[i];
        const TempoPoint& q = tempo[i + 1];
        if (q.beat > p.beat && m >= p.beat && m < q.beat) {
          end = q.beat;
          s = (q.bpm - p.bpm) / (q.beat - p.beat);
          break;
        }
      }
    }
    if (end == kInf) return m + remaining * u / 60.0;  // constant tempo to infinity
    const double uEnd = u + s * (end - m);
    const double need = segmentSeconds(m, end, u, uEnd);
    if (need >= remaining) {
      if (std::fabs(s) < 1e-12) return m + remaining * u / 60.0;
      const double u1 = u * std::exp(s * remaining / 60.0);
      return m + (u1 - u) / s;
    }
    remaining -= need;
    m = end;
  }
  return m;
}

float MixPlan::laneValue(int deck, LaneTarget target, double beat) const noexcept {
  const Lane& l = lanes[deck & 1][static_cast<int>(target)];
  if (l.count <= 0) return neutral(target);
  const LaneKey* k = keys.data() + l.first;
  if (beat <= k[0].beat) return k[0].value;
  if (beat >= k[l.count - 1].beat) return k[l.count - 1].value;
  int lo = 0, hi = l.count - 1;
  while (hi - lo > 1) {
    const int mid = (lo + hi) / 2;
    if (k[mid].beat <= beat) lo = mid; else hi = mid;
  }
  const double span = k[hi].beat - k[lo].beat;
  if (span <= 0.0) return k[hi].value;
  const float f = static_cast<float>((beat - k[lo].beat) / span);
  return k[lo].value + (k[hi].value - k[lo].value) * f;
}

std::string planToJson(const MixPlan& p) {
  using nlohmann::json;
  json j;
  j["totalSec"] = p.totalSec;
  j["totalBeats"] = p.totalBeats;
  json items = json::array();
  for (std::size_t i = 0; i < p.items.size(); ++i) {
    const PlanItem& it = p.items[i];
    const double t0 = p.timeOfBeat(it.startBeat);
    const double t1 = p.timeOfBeat(it.endBeat);
    items.push_back({{"trackId", it.track ? it.track->id : 0},
                     {"name", it.track ? it.track->name : std::string()},
                     {"deck", it.deck},
                     {"startSec", t0},
                     {"endSec", t1},
                     {"startBeat", it.startBeat},
                     {"endBeat", it.endBeat},
                     {"trackStartSec", it.secOfTrackBeat(it.trackBeat0)},
                     {"beatMul", it.beatMul},
                     {"gridTrusted", it.gridTrusted}});
  }
  j["items"] = std::move(items);
  json trs = json::array();
  for (const TransitionInfo& t : p.transitions) {
    trs.push_back({{"from", t.from}, {"to", t.to}, {"type", transitionName(t.type)}, {"startSec", t.startSec},
                   {"lengthSec", t.lengthSec}, {"startBeat", t.startBeat}, {"lengthBeats", t.lengthBeats}, {"bpmFrom", t.bpmFrom},
                   {"bpmTo", t.bpmTo}, {"tempoRamp", t.tempoRamp}, {"note", t.note}});
  }
  j["transitions"] = std::move(trs);
  return j.dump();
}

}  // namespace autodj::dj
