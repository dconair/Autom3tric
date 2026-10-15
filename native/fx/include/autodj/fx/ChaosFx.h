#pragma once
// "Chaos": one macro knob that morphs a reverb into a flanger and fades it in behind the dry signal.
//
//   amount 0      exact bypass (the output is bit-identical to the input, no CPU)
//   amount small  a quiet, short reverb tail
//   amount mid    reverb and flanger blended (equal power)
//   amount 1      mostly flanger: a sweeping, resonant comb on top of the dry signal
//
// Reverb  : Freeverb-style network (8 damped feedback combs + 4 all-passes per channel, right channel detuned for width)
// Flanger : stereo modulated delay (0.5 .. 5.5 ms), sine LFOs 90 degrees apart, feedback grows with the amount
//
// All delay lines are allocated in prepare(); process() never allocates, locks or branches on anything unbounded.
// Stage activity is tracked so that a stage that has been off for a while starts from silence instead of replaying
// stale audio, and a stage that is being faded out keeps running until its gain has reached zero (no tail cut).

#include <cstddef>
#include <vector>

namespace autodj::fx {

class ChaosFx {
 public:
  /// Control thread. Allocates the delay lines for `sampleRate` (up to 192 kHz).
  void prepare(double sampleRate);
  /// Clears all state (audio thread safe: no allocation).
  void reset() noexcept;

  /// In-place on interleaved stereo. `amount` ramps linearly from a0 (first frame) to a1 (end of the block); both 0..1.
  void process(float* io, int frames, float a0, float a1) noexcept;

  /// Gains the amount maps to, exposed for tests: reverb / flanger wet gain and flanger feedback.
  struct Mapping {
    float reverbGain, flangerGain, feedback, lfoHz;
  };
  static Mapping map(float amount) noexcept;

 private:
  static constexpr int kCombs = 8;
  static constexpr int kAllpasses = 4;

  struct Comb {
    std::vector<float> buf;
    std::size_t pos = 0;
    float lp = 0.f;
  };
  struct Allpass {
    std::vector<float> buf;
    std::size_t pos = 0;
  };

  void clearReverb() noexcept;
  void clearFlanger() noexcept;

  double sr_ = 48000.0;
  Comb combs_[2][kCombs];
  Allpass allpass_[2][kAllpasses];
  std::vector<float> flange_[2];
  std::size_t flangeMask_ = 0;
  std::size_t flangeWrite_ = 0;
  float flangeFb_[2] = {0.f, 0.f};
  double lfoPhase_ = 0.0;
  bool reverbLive_ = false;
  bool flangerLive_ = false;
  float reverbFeedback_ = 0.84f;
  float reverbDamp_ = 0.25f;
};

}  // namespace autodj::fx
