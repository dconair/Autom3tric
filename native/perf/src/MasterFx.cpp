#include "autodj/perf/MasterFx.h"

#include <algorithm>
#include <cmath>

namespace autodj::perf {

namespace {
inline float ramp(float cur, float target, float step) noexcept {
  const float d = target - cur;
  if (d > step) return cur + step;
  if (d < -step) return cur - step;
  return target;
}
}  // namespace

void MasterFx::prepare(double sampleRate) {
  sr_ = sampleRate > 8000.0 ? sampleRate : 48000.0;
  echoSize_ = static_cast<std::size_t>(4.2 * sr_);
  capSize_ = static_cast<std::size_t>(2.0 * sr_);
  for (int c = 0; c < 2; ++c) {
    echo_[c].assign(echoSize_, 0.f);
    cap_[c].assign(capSize_, 0.f);
    loop_[c].assign(capSize_, 0.f);
  }
  reverb_.prepare(sr_);
  flanger_.prepare(sr_);
  reset();
}

void MasterFx::reset() noexcept {
  for (int c = 0; c < 2; ++c) {
    std::fill(echo_[c].begin(), echo_[c].end(), 0.f);
    std::fill(cap_[c].begin(), cap_[c].end(), 0.f);
  }
  echoWrite_ = capWrite_ = 0;
  echoFb_[0] = echoFb_[1] = 0.f;
  echoSend_ = echoReturn_ = 0.f;
  echoLive_ = false;
  echoQuiet_ = 0;
  echoDelay_ = 0.f;
  loopLen_ = loopPos_ = 0;
  rollWas_ = false;
  rollGain_ = 0.f;
  gate_ = 1.f;
  stutterAmt_ = 0.f;
  reverbAmt_ = flangerAmt_ = 0.f;
  reverb_.reset();
  flanger_.reset();
}

void MasterFx::setParams(const ParamStore& st) noexcept {
  p_.echoOn = st.getBool(ids_.echoOn);
  p_.reverbOn = st.getBool(ids_.reverbOn);
  p_.flangerOn = st.getBool(ids_.flangerOn);
  p_.rollOn = st.getBool(ids_.rollOn);
  p_.stutterOn = st.getBool(ids_.stutterOn);
  p_.echoTime = std::clamp(st.getInt(ids_.echoBeats), 0, 4);
  p_.rollSize = std::clamp(st.getInt(ids_.rollBeats), 0, 3);
  p_.stutterRate = std::clamp(st.getInt(ids_.stutterBeats), 0, 2);
  p_.echoFeedback = st.get(ids_.echoFeedback);
  p_.echoMix = st.get(ids_.echoMix);
  p_.reverbMix = st.get(ids_.reverbMix);
  p_.flangerMix = st.get(ids_.flangerMix);
}

void MasterFx::startRoll(double samplesPerBeat, float beats) noexcept {
  std::size_t len = static_cast<std::size_t>(samplesPerBeat * beats);
  len = std::clamp<std::size_t>(len, 64, capSize_ - 8);
  for (int c = 0; c < 2; ++c) {
    for (std::size_t i = 0; i < len; ++i) {
      // the last `len` frames before "now": capWrite_ points at the next frame to be written
      const std::size_t idx = (capWrite_ + capSize_ - len + i) % capSize_;
      loop_[c][i] = cap_[c][idx];
    }
  }
  loopLen_ = len;
  loopPos_ = 0;
}

void MasterFx::process(float* io, int frames, const MasterClock& clock) noexcept {
  const double bpm = std::clamp(clock.bpm, 40.0, 240.0);
  const double samplesPerBeat = 60.0 * sr_ / bpm;
  const double beatsPerSample = 1.0 / samplesPerBeat;

  // ---- per-chunk decisions ----------------------------------------------------------------------------------
  if (p_.rollOn && !rollWas_) startRoll(samplesPerBeat, kRollBeats[p_.rollSize]);
  rollWas_ = p_.rollOn;

  const float sendTarget = p_.echoOn ? 1.f : 0.f;
  const float echoDelayTarget = static_cast<float>(std::clamp(kEchoBeats[p_.echoTime] * samplesPerBeat, 0.03 * sr_, 4.0 * sr_));
  const float fb = std::clamp(p_.echoFeedback, 0.f, 0.92f);
  const float echoLevel = p_.echoMix;
  const bool echoRun = p_.echoOn || echoLive_;
  const double stutterDiv = kStutterBeats[p_.stutterRate];

  const float rampStep = static_cast<float>(1.0 / (0.004 * sr_));   // 4 ms
  const float gateStep = static_cast<float>(1.0 / (0.0015 * sr_));  // 1.5 ms
  const float delaySlew = static_cast<float>(1.0 - std::exp(-1.0 / (0.05 * sr_)));
  const float lpA = static_cast<float>(1.0 - std::exp(-2.0 * 3.14159265358979 * 3200.0 / sr_));
  const float stutterTarget = p_.stutterOn ? 1.f : 0.f;
  const float rollTarget = p_.rollOn ? 1.f : 0.f;
  float tailPeak = 0.f;

  for (int i = 0; i < frames; ++i) {
    float l = io[2 * i], r = io[2 * i + 1];

    // the roll records the input as it is, before anything below changes it
    cap_[0][capWrite_] = l;
    cap_[1][capWrite_] = r;
    capWrite_ = capWrite_ + 1 == capSize_ ? 0 : capWrite_ + 1;

    // ---- stutter: a gate on the beat grid ------------------------------------------------------------------
    stutterAmt_ = ramp(stutterAmt_, stutterTarget, rampStep);
    if (stutterAmt_ > 0.f || gate_ < 1.f) {
      const double x = (clock.beat + i * beatsPerSample) / stutterDiv;
      const double frac = x - std::floor(x);
      const float gateTarget = frac < 0.5 ? 1.f : 0.f;
      gate_ = ramp(gate_, gateTarget, gateStep);
      const float g = 1.f + (gate_ - 1.f) * stutterAmt_;
      l *= g;
      r *= g;
    }

    // ---- beat roll: the held slice replaces the input ------------------------------------------------------
    rollGain_ = ramp(rollGain_, rollTarget, rampStep);
    if (rollGain_ > 0.f && loopLen_ > 0) {
      float ll = loop_[0][loopPos_], lr = loop_[1][loopPos_];
      // short fades at the loop seam keep it free of clicks
      constexpr std::size_t kSeam = 48;
      float seam = 1.f;
      if (loopLen_ > 4 * kSeam) {
        if (loopPos_ < kSeam) seam = static_cast<float>(loopPos_) / kSeam;
        else if (loopPos_ + kSeam > loopLen_) seam = static_cast<float>(loopLen_ - loopPos_) / kSeam;
      }
      ll *= seam;
      lr *= seam;
      loopPos_ = loopPos_ + 1 >= loopLen_ ? 0 : loopPos_ + 1;
      l += (ll - l) * rollGain_;
      r += (lr - r) * rollGain_;
    }

    // ---- echo: feed gated by "on", return stays until the repeats have died away ---------------------------
    if (echoRun) {
      echoDelay_ += (echoDelayTarget - echoDelay_) * delaySlew;
      if (echoDelay_ < 1.f) echoDelay_ = echoDelayTarget;
      float rp = static_cast<float>(echoWrite_) - echoDelay_;
      if (rp < 0.f) rp += static_cast<float>(echoSize_);
      const std::size_t i0 = static_cast<std::size_t>(rp);
      const std::size_t i1 = i0 + 1 >= echoSize_ ? 0 : i0 + 1;
      const float fr = rp - static_cast<float>(i0);
      const float dl = echo_[0][i0] + (echo_[0][i1] - echo_[0][i0]) * fr;
      const float dr = echo_[1][i0] + (echo_[1][i1] - echo_[1][i0]) * fr;
      echoFb_[0] += lpA * (dl - echoFb_[0]);
      echoFb_[1] += lpA * (dr - echoFb_[1]);
      echoSend_ = ramp(echoSend_, sendTarget, rampStep);
      // ping-pong: each side feeds the other
      echo_[0][echoWrite_] = l * echoSend_ + echoFb_[1] * fb;
      echo_[1][echoWrite_] = r * echoSend_ + echoFb_[0] * fb;
      echoWrite_ = echoWrite_ + 1 == echoSize_ ? 0 : echoWrite_ + 1;
      echoReturn_ += (echoLevel - echoReturn_) * 0.002f;
      l += dl * echoReturn_;
      r += dr * echoReturn_;
      tailPeak = std::max(tailPeak, std::max(std::fabs(dl), std::fabs(dr)));
    }

    io[2 * i] = l;
    io[2 * i + 1] = r;
  }
  if (echoRun) {
    // the tail is over once nothing audible has come out of the delay line for longer than the line is long
    echoQuiet_ = tailPeak > 1e-4f ? 0 : echoQuiet_ + static_cast<std::size_t>(frames);
    echoLive_ = p_.echoOn || echoQuiet_ < static_cast<std::size_t>(echoDelay_) + static_cast<std::size_t>(0.1 * sr_);
    if (!echoLive_) {  // cleared once, so that the next use starts clean; afterwards the echo costs nothing
      echoSend_ = 0.f;
      for (int c = 0; c < 2; ++c) std::fill(echo_[c].begin(), echo_[c].end(), 0.f);
      echoFb_[0] = echoFb_[1] = 0.f;
      echoReturn_ = 0.f;
    }
  }

  // ---- flanger and reverb: the chaos macro at its two ends ---------------------------------------------------
  const float fa1 = p_.flangerOn ? 0.8f + 0.2f * p_.flangerMix : 0.f;
  flanger_.process(io, frames, flangerAmt_, fa1);
  flangerAmt_ = fa1;
  const float ra1 = p_.reverbOn ? 0.04f + 0.2f * p_.reverbMix : 0.f;
  reverb_.process(io, frames, reverbAmt_, ra1);
  reverbAmt_ = ra1;
}

}  // namespace autodj::perf
