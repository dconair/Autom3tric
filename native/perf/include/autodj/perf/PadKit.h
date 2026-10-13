#pragma once
// The drum kit behind the 4x4 pads: sixteen sounds synthesised once at prepare() time (no sample files, so the kit is identical
// on Android and macOS and needs no licensing). Each sound is a mono float buffer; the pad node plays them back as voices.

#include <vector>

namespace autodj::perf {

inline constexpr int kPads = 16;
inline constexpr int kSteps = 16;

/// Pad numbering (row by row, top left = 0):
///   0 Kick       1 Sub kick   2 Snare     3 Clap
///   4 Hat        5 Open hat   6 Rim       7 Shaker
///   8 Tom low    9 Tom high  10 Cowbell  11 Conga
///  12 Crash     13 Riser     14 Laser    15 Drop
struct PadSound {
  std::vector<float> data;
  float gainL = 0.7071f, gainR = 0.7071f;  // equal-power pan
  int chokeGroup = 0;                      // 0 = none; a new hit of the group cuts the others (closed / open hat)
  bool accentProfile = false;              // the step-position velocity profile applies (hats, shakers, percussion)
};

class PadKit {
 public:
  /// Control thread: renders all sounds for `sampleRate`.
  void render(double sampleRate);
  const PadSound& sound(int pad) const { return sounds_[static_cast<std::size_t>(pad)]; }

 private:
  PadSound sounds_[kPads];
};

/// Pattern preloaded into the sequencer: a plain house groove, so that "Sequencer an" is immediately musical.
/// Returns the 16-bit step mask of a pad.
unsigned defaultPatternRow(int pad) noexcept;

}  // namespace autodj::perf
