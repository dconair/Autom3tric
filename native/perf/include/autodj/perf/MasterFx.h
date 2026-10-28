#pragma once
// Performance effects on the master bus, in series: stutter gate -> beat roll -> echo -> flanger -> reverb.
// All tempo-synced parts follow the MasterClock (beats of the music), so they sit on the groove. Nothing allocates after prepare().

#include <vector>

#include "autodj/core/MasterClock.h"
#include "autodj/core/Param.h"
#include "autodj/fx/ChaosFx.h"
#include "autodj/perf/PerfParams.h"

namespace autodj::perf {

class MasterFx {
 public:
  explicit MasterFx(const PerfParamIds& ids) : ids_(ids) {}

  void prepare(double sampleRate);
  void reset() noexcept;
  /// Audio thread, once per chunk before process().
  void setParams(const ParamStore& st) noexcept;
  /// In place on interleaved stereo.
  void process(float* io, int frames, const MasterClock& clock) noexcept;

 private:
  struct Snapshot {
    bool echoOn = false, reverbOn = false, flangerOn = false, rollOn = false, stutterOn = false;
    int echoTime = 1, rollSize = 1, stutterRate = 1;
    float echoFeedback = 0.5f, echoMix = 0.5f, reverbMix = 0.4f, flangerMix = 0.6f;
  };

  void startRoll(double samplesPerBeat, float beats) noexcept;

  PerfParamIds ids_;
  Snapshot p_;
  double sr_ = 48000.0;

  // echo
  std::vector<float> echo_[2];
  std::size_t echoSize_ = 0, echoWrite_ = 0;
  float echoDelay_ = 0.f;  // current delay in samples (smoothed)
  float echoFb_[2] = {0.f, 0.f};
  float echoSend_ = 0.f;
  float echoReturn_ = 0.f;
  bool echoLive_ = false;
  std::size_t echoQuiet_ = 0;  // samples since the repeats were last audible

  // beat roll: a ring that always records the input, and the slice that is looped while the roll is held
  std::vector<float> cap_[2], loop_[2];
  std::size_t capSize_ = 0, capWrite_ = 0;
  std::size_t loopLen_ = 0, loopPos_ = 0;
  bool rollWas_ = false;
  float rollGain_ = 0.f;

  // stutter
  float gate_ = 1.f;
  float stutterAmt_ = 0.f;

  fx::ChaosFx reverb_, flanger_;
  float reverbAmt_ = 0.f, flangerAmt_ = 0.f;
};

}  // namespace autodj::perf
