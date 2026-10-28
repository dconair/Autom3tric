#pragma once
// Allocation-free filters of the DJ mixer: a 3-band EQ with kill switches and a bipolar low-pass / high-pass sweep filter.
// Everything here runs on the audio thread, sample by sample, with gains/cutoffs that ramp linearly across the block.

#include <algorithm>
#include <cmath>

#include "autodj/core/Dsp.h"

namespace autodj::dj {

inline constexpr float kEqKillDb = -26.f;  // at or below this value a band is switched off completely

inline float eqDbToGain(float db) noexcept { return db <= kEqKillDb + 0.05f ? 0.f : dbToLin(db); }

/// Direct-form-II-transposed biquad, one channel.
struct Biquad {
  float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
  float z1 = 0.f, z2 = 0.f;

  void setLowpass(double fs, double fc, double q) noexcept {
    const double w = 2.0 * 3.14159265358979323846 * fc / fs;
    const double c = std::cos(w), alpha = std::sin(w) / (2.0 * q), a0 = 1.0 + alpha;
    b0 = static_cast<float>((1.0 - c) * 0.5 / a0);
    b1 = static_cast<float>((1.0 - c) / a0);
    b2 = b0;
    a1 = static_cast<float>(-2.0 * c / a0);
    a2 = static_cast<float>((1.0 - alpha) / a0);
  }
  void setAllpass(double fs, double fc, double q) noexcept {
    const double w = 2.0 * 3.14159265358979323846 * fc / fs;
    const double c = std::cos(w), alpha = std::sin(w) / (2.0 * q), a0 = 1.0 + alpha;
    b0 = static_cast<float>((1.0 - alpha) / a0);
    b1 = static_cast<float>(-2.0 * c / a0);
    b2 = static_cast<float>((1.0 + alpha) / a0);
    a1 = b1;
    a2 = b0;
  }
  void setHighpass(double fs, double fc, double q) noexcept {
    const double w = 2.0 * 3.14159265358979323846 * fc / fs;
    const double c = std::cos(w), alpha = std::sin(w) / (2.0 * q), a0 = 1.0 + alpha;
    b0 = static_cast<float>((1.0 + c) * 0.5 / a0);
    b1 = static_cast<float>(-(1.0 + c) / a0);
    b2 = b0;
    a1 = static_cast<float>(-2.0 * c / a0);
    a2 = static_cast<float>((1.0 - alpha) / a0);
  }
  float process(float x) noexcept {
    const float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }
  void reset() noexcept { z1 = z2 = 0.f; }
};

/// Linkwitz-Riley 4th order = two Butterworth sections in series.
struct Lr4 {
  Biquad s1, s2;
  void setLowpass(double fs, double fc) noexcept {
    s1.setLowpass(fs, fc, 0.70710678);
    s2.setLowpass(fs, fc, 0.70710678);
  }
  void setHighpass(double fs, double fc) noexcept {
    s1.setHighpass(fs, fc, 0.70710678);
    s2.setHighpass(fs, fc, 0.70710678);
  }
  float process(float x) noexcept { return s2.process(s1.process(x)); }
  void reset() noexcept {
    s1.reset();
    s2.reset();
  }
};

/// Three-band split with Linkwitz-Riley 4th-order crossovers: low = LP1(x), mid = LP2(HP1(x)), high = HP2(HP1(x)). LP2+HP2 of the
/// upper split is a 2nd-order all-pass, so the low band runs through the same all-pass; with all gains at 1 the sum is flat in
/// magnitude. A killed band is really gone (24 dB/oct skirts), not just "x minus a phase-shifted copy".
class Eq3 {
 public:
  static constexpr double kLowCrossHz = 220.0;
  static constexpr double kHighCrossHz = 2400.0;

  void prepare(double sampleRate) noexcept {
    for (int c = 0; c < 2; ++c) {
      lowLp_[c].setLowpass(sampleRate, kLowCrossHz);
      lowHp_[c].setHighpass(sampleRate, kLowCrossHz);
      highLp_[c].setLowpass(sampleRate, kHighCrossHz);
      highHp_[c].setHighpass(sampleRate, kHighCrossHz);
      lowComp_[c].setAllpass(sampleRate, kHighCrossHz, 0.70710678);
    }
    reset();
  }
  void reset() noexcept {
    for (int c = 0; c < 2; ++c) {
      lowLp_[c].reset();
      lowHp_[c].reset();
      highLp_[c].reset();
      highHp_[c].reset();
      lowComp_[c].reset();
    }
  }

  /// In place on planar buffers. Gains are linear and ramp from g0 to g1 over the block.
  void process(float* l, float* r, int frames, const float (&g0)[3], const float (&g1)[3]) noexcept {
    float* ch[2] = {l, r};
    float g[3] = {g0[0], g0[1], g0[2]};
    const float inv = 1.f / static_cast<float>(frames);
    const float d[3] = {(g1[0] - g0[0]) * inv, (g1[1] - g0[1]) * inv, (g1[2] - g0[2]) * inv};
    for (int i = 0; i < frames; ++i) {
      for (int c = 0; c < 2; ++c) {
        const float x = ch[c][i];
        const float lo = lowComp_[c].process(lowLp_[c].process(x));
        const float rest = lowHp_[c].process(x);
        const float mid = highLp_[c].process(rest);
        const float hi = highHp_[c].process(rest);
        ch[c][i] = lo * g[0] + mid * g[1] + hi * g[2];
      }
      g[0] += d[0];
      g[1] += d[1];
      g[2] += d[2];
    }
  }

 private:
  Lr4 lowLp_[2], lowHp_[2], highLp_[2], highHp_[2];
  Biquad lowComp_[2];
};

/// Bipolar filter knob: left of centre a low-pass closing from 20 kHz to ~120 Hz, right of centre a high-pass opening from 20 Hz
/// to ~8 kHz, centre = off. Zavalishin's TPT state-variable filter (stable under fast cutoff changes), slightly resonant.
class SweepFilter {
 public:
  void prepare(double sampleRate) noexcept {
    fs_ = sampleRate;
    reset();
  }
  void reset() noexcept {
    for (int c = 0; c < 2; ++c) ic1_[c] = ic2_[c] = 0.f;
  }

  static float cutoffHz(float knob) noexcept {
    return knob < 0.f ? 20000.f * std::pow(0.006f, -knob) : 20.f * std::pow(400.f, knob);
  }

  /// In place on planar buffers; the knob ramps linearly from k0 to k1 across the block (coefficients follow per sample group of 16).
  void process(float* l, float* r, int frames, float k0, float k1) noexcept {
    if (std::fabs(k0) < kDeadZone && std::fabs(k1) < kDeadZone && !active_) {
      reset();  // stale state must not leak into the next sweep
      return;
    }
    float* ch[2] = {l, r};
    const float inv = 1.f / static_cast<float>(frames);
    float a1 = 0, a2 = 0, a3 = 0, wet = 0;
    bool hp = false;
    for (int i = 0; i < frames; ++i) {
      if ((i & 15) == 0) {
        const float k = k0 + (k1 - k0) * (static_cast<float>(i) + 8.f) * inv;
        hp = k > 0.f;
        const double g = std::tan(3.14159265358979323846 * std::min<double>(cutoffHz(k), 0.45 * fs_) / fs_);
        constexpr double kRes = 1.0 / 0.85;  // 1/Q
        a1 = static_cast<float>(1.0 / (1.0 + g * (g + kRes)));
        a2 = static_cast<float>(g) * a1;
        a3 = static_cast<float>(g) * a2;
        wet = std::min(1.f, std::fabs(k) / (4.f * kDeadZone));  // fade the filter in over the first few percent of the knob
        k_ = static_cast<float>(kRes);
      }
      for (int c = 0; c < 2; ++c) {
        const float v0 = ch[c][i];
        const float v3 = v0 - ic2_[c];
        const float v1 = a1 * ic1_[c] + a2 * v3;
        const float v2 = ic2_[c] + a2 * ic1_[c] + a3 * v3;
        ic1_[c] = 2.f * v1 - ic1_[c];
        ic2_[c] = 2.f * v2 - ic2_[c];
        const float y = hp ? v0 - k_ * v1 - v2 : v2;
        ch[c][i] = v0 + (y - v0) * wet;
      }
    }
    active_ = std::fabs(k1) >= kDeadZone;
  }

 private:
  static constexpr float kDeadZone = 0.01f;
  double fs_ = 48000.0;
  float ic1_[2] = {0.f, 0.f};
  float ic2_[2] = {0.f, 0.f};
  float k_ = 1.f;
  bool active_ = false;
};

}  // namespace autodj::dj
