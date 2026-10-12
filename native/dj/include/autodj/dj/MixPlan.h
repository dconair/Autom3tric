#pragma once
// An automatic mix as immutable data: which track plays on which deck from which master beat to which, the tempo curve of
// the master clock, and automation lanes (volume, EQ, filter per deck) over master beats. Built on the control thread by
// planMix() from a list of analysed tracks, handed to the audio thread by pointer (see DjNode), kept alive by the engine.
//
// Master clock. Time is counted in master beats `m`. The tempo is piecewise linear in m (so a transition can ramp it), which
// gives closed forms for the time a stretch of beats takes. A track on the plan is "locked": its own beat index is
// `trackBeat0 + (m - startBeat) / beatMul`, so every track in a transition is on the same beat and bar grid by construction.
// This also makes the deck's source position a pure function of m, which is what Deck chases.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "autodj/engine/Tracks.h"

namespace autodj::dj {

enum class TransitionType : std::uint8_t {
  Auto = 0,
  EqBlend,      // bass swap: the incoming song's lows are cut until the middle of the overlap, then the two basses swap
  Crossfade,    // plain equal-power crossfade
  FilterSweep,  // outgoing song through a rising high-pass, incoming song through an opening low-pass
  Cut,          // no overlap: the incoming song starts on the downbeat where the outgoing one ends (also used when tempos do not fit)
  kCount
};

const char* transitionName(TransitionType t) noexcept;

enum class LaneTarget : std::uint8_t { Gain = 0, EqLow, EqMid, EqHigh, Filter, kCount };

struct LaneKey {
  double beat = 0.0;
  float value = 0.f;  // Gain: linear 0..1, Eq*: dB, Filter: knob -1..1
};

struct Lane {
  int first = 0;
  int count = 0;
};

struct PlanItem {
  std::shared_ptr<const Track> track;
  int deck = 0;
  double startBeat = 0.0;   // master beat where this track's deck starts
  double endBeat = 0.0;     // master beat where it stops
  double trackBeat0 = 0.0;  // the track's own beat index heard at startBeat
  double beatMul = 1.0;     // master beats per track beat: 2 = a 70 bpm track counted as 140, 0.5 = a 160 bpm track counted as 80
  // grid used for locking (a fallback of 120 bpm is substituted for tracks without a detected tempo)
  double beatSec = 0.5;
  double gridOffsetSec = 0.0;
  bool gridTrusted = false;

  double secOfTrackBeat(double b) const noexcept { return gridOffsetSec + b * beatSec; }
  double trackBeatAt(double masterBeat) const noexcept { return trackBeat0 + (masterBeat - startBeat) / beatMul; }
};

struct TempoPoint {
  double beat = 0.0;
  double bpm = 120.0;
};

struct TransitionInfo {
  int from = 0, to = 1;
  TransitionType type = TransitionType::EqBlend;
  double startBeat = 0.0, lengthBeats = 0.0;
  double startSec = 0.0, lengthSec = 0.0;
  double bpmFrom = 0.0, bpmTo = 0.0;  // master tempo at the start and at the end of the overlap
  bool tempoRamp = false;
  std::string note;  // why this type / length (shown in the UI)
};

struct MixPlan {
  std::vector<PlanItem> items;
  std::vector<TempoPoint> tempo;  // ascending in beat; two points at the same beat = a tempo jump
  std::vector<LaneKey> keys;
  Lane lanes[2][static_cast<int>(LaneTarget::kCount)];
  std::vector<TransitionInfo> transitions;
  double totalBeats = 0.0;
  double totalSec = 0.0;
  std::uint32_t id = 0;

  // ---- time <-> beat (pure functions, audio-thread safe) ----
  double bpmAt(double beat) const noexcept;
  /// Seconds the master clock needs to get from beat a to beat b (a <= b).
  double secondsBetween(double a, double b) const noexcept;
  /// The beat reached `seconds` after beat `from`.
  double beatAfter(double from, double seconds) const noexcept;
  double timeOfBeat(double beat) const noexcept { return secondsBetween(0.0, beat); }

  /// Lane value at a master beat; the neutral value when the plan has no lane for it.
  float laneValue(int deck, LaneTarget target, double beat) const noexcept;
  static float neutral(LaneTarget t) noexcept { return t == LaneTarget::Gain ? 1.f : 0.f; }
};

struct PlanOptions {
  TransitionType type = TransitionType::Auto;
  int lengthBeats = 0;          // 8 / 16 / 32, 0 = decide per pair
  int maxTempoGapPercent = 12;  // beyond this (after trying half/double time) the pair gets a Cut
  bool startAtMixIn = true;     // skip a silent or beat-less lead-in of the first song
  std::uint32_t seed = 1;       // variety between transition types in Auto mode
};

/// Plans a mix of the tracks in the given order. Never fails for a non-empty list; every pair gets some transition.
std::shared_ptr<MixPlan> planMix(const std::vector<std::shared_ptr<const Track>>& tracks, const PlanOptions& options);

enum class OrderMode : std::uint8_t { AsListed = 0, Smart, TempoUp, TempoDown, kCount };

/// Reorders `tracks` (indices into it) for a smoother set. Smart: start with the first track, then always the best next one
/// (close tempo, harmonic key, similar level), which is what a DJ's "harmonic mixing" does.
std::vector<int> orderTracks(const std::vector<std::shared_ptr<const Track>>& tracks, OrderMode mode);

std::string planToJson(const MixPlan& plan);

}  // namespace autodj::dj
