#pragma once
// Single source of truth for musical time.
//
// Two beat positions, both in double precision:
//   clockBeat : the master beat clock. Free-runs at the transport BPM whether or not the timeline
//               is playing, so pads, granular beat-sync and effects always lock to the same grid.
//   playBeat  : the timeline play-head. Advances only while playing, can be seeked.
//
// The audio thread calls beginBlock()/endBlock() once per internal block and hands the resulting
// BlockTime to every node. Nodes derive sample-accurate event offsets from it
// (forEachGridCrossing) - nobody keeps a private tempo or beat counter.

#include <cmath>
#include <cstdint>

#include "autodj/core/Platform.h"

namespace autodj {

struct BlockTime {
  int frames = 0;
  double sampleRate = 48000.0;
  std::int64_t engineSample = 0;  // free-running sample counter at the first frame of the block
  double clockBeat = 0.0;         // master clock at the first frame
  double playBeat = 0.0;          // timeline position at the first frame
  double beatsPerSample = 0.0;    // bpm / (60 * sr), constant within the block
  bool playing = false;

  double bpm() const noexcept { return beatsPerSample * 60.0 * sampleRate; }
  double samplesPerBeat() const noexcept { return beatsPerSample > 0.0 ? 1.0 / beatsPerSample : 0.0; }
  double clockBeatEnd() const noexcept { return clockBeat + frames * beatsPerSample; }

  /// Calls fn(frameOffset) for every grid line k*divBeats that falls in [clockBeat, clockBeatEnd).
  /// The half-open interval tiles perfectly across consecutive blocks (the next block starts at
  /// exactly this block's end), so a line is reported exactly once, never twice or never.
  ///
  /// Both interval ends are shifted down by kBeatTolerance (~1e-5 sample): a line that
  /// mathematically sits exactly on a block boundary (e.g. 120 BPM, 1/16 = 6000 samples) would
  /// otherwise land in the earlier block whenever the accumulated beat clock is a few ulp too
  /// high. The shift is the same constant at both ends, so the tiling stays exact.
  static constexpr double kBeatTolerance = 2e-9;

  template <typename Fn>
  void forEachGridCrossing(double divBeats, Fn&& fn) const noexcept {
    if (divBeats <= 0.0 || beatsPerSample <= 0.0 || frames <= 0) return;
    const double lo = clockBeat - kBeatTolerance;
    const double hi = clockBeatEnd() - kBeatTolerance;
    double k = std::ceil(lo / divBeats);
    // ceil() may land one grid line early/late because of the division's rounding: fix up.
    while (k > 0.0 && (k - 1.0) * divBeats >= lo) k -= 1.0;
    while (k * divBeats < lo) k += 1.0;
    for (int guard = 0; guard < 4096; ++guard, k += 1.0) {
      const double line = k * divBeats;
      if (line >= hi) break;
      int off = static_cast<int>((line - clockBeat) / beatsPerSample + 0.5);
      if (off >= frames) off = frames - 1;
      if (off < 0) off = 0;
      fn(off);
    }
  }
};

class Transport {
 public:
  void prepare(double sampleRate) noexcept {
    sampleRate_ = sampleRate;
    reset();
  }

  void reset() noexcept {
    engineSample_ = 0;
    clockBeat_ = 0.0;
    playBeat_ = 0.0;
    playing_ = false;
  }

  // ---- commands (audio thread) -------------------------------------------------------------
  void play() noexcept {
    playing_ = true;
    clockBeat_ = playBeat_;  // line the grid up with the play-head
  }
  void stop() noexcept { playing_ = false; }
  void seek(double beats) noexcept {
    playBeat_ = beats < 0.0 ? 0.0 : beats;
    if (playing_) clockBeat_ = playBeat_;
  }

  // ---- per block (audio thread) ------------------------------------------------------------
  BlockTime beginBlock(int frames, double bpm) const noexcept {
    BlockTime t;
    t.frames = frames;
    t.sampleRate = sampleRate_;
    t.engineSample = engineSample_;
    t.clockBeat = clockBeat_;
    t.playBeat = playBeat_;
    t.beatsPerSample = bpm / (60.0 * sampleRate_);
    t.playing = playing_;
    return t;
  }

  void endBlock(const BlockTime& t) noexcept {
    engineSample_ += t.frames;
    clockBeat_ = t.clockBeatEnd();
    if (playing_) playBeat_ = playBeat_ + t.frames * t.beatsPerSample;
  }

  bool playing() const noexcept { return playing_; }
  double clockBeat() const noexcept { return clockBeat_; }
  double playBeat() const noexcept { return playBeat_; }

 private:
  double sampleRate_ = 48000.0;
  std::int64_t engineSample_ = 0;
  double clockBeat_ = 0.0;
  double playBeat_ = 0.0;
  bool playing_ = false;
};

/// Beat length of the musical grid divisions the sync features offer.
enum class SyncDivision : int {
  Quarter = 0,    // 1/4 note  = 1 beat
  Eighth,         // 1/8
  Sixteenth,      // 1/16
  ThirtySecond,   // 1/32
  QuarterTriplet, // 1/4T
  EighthTriplet,  // 1/8T
  SixteenthTriplet,
  Count
};

inline double divisionBeats(SyncDivision d) noexcept {
  switch (d) {
    case SyncDivision::Quarter: return 1.0;
    case SyncDivision::Eighth: return 0.5;
    case SyncDivision::Sixteenth: return 0.25;
    case SyncDivision::ThirtySecond: return 0.125;
    case SyncDivision::QuarterTriplet: return 2.0 / 3.0;
    case SyncDivision::EighthTriplet: return 1.0 / 3.0;
    case SyncDivision::SixteenthTriplet: return 1.0 / 6.0;
    default: return 1.0;
  }
}

}  // namespace autodj
