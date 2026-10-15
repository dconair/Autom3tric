#pragma once
// Small allocation-free DSP helpers shared by all nodes.

#include <cmath>
#include <cstdint>

#include "autodj/core/Platform.h"

namespace autodj {

/// xorshift32: one instance per voice, no global state, deterministic from the seed.
class XorShift32 {
 public:
  XorShift32() noexcept { reseed(0x9E3779B9u); }
  explicit XorShift32(std::uint32_t seed) noexcept { reseed(seed); }

  void reseed(std::uint32_t seed) noexcept { s_ = seed ? seed : 0x9E3779B9u; }

  AUTODJ_INLINE std::uint32_t nextU32() noexcept {
    std::uint32_t x = s_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_ = x;
    return x;
  }
  /// [0, 1)
  AUTODJ_INLINE float uniform() noexcept { return static_cast<float>(nextU32() >> 8) * (1.0f / 16777216.0f); }
  /// [-1, 1)
  AUTODJ_INLINE float bipolar() noexcept { return uniform() * 2.0f - 1.0f; }

 private:
  std::uint32_t s_;
};

/// Smooth master safety: identity below the knee, asymptotically approaches +-1 above it
/// (C1-continuous at the knee). Only the rare peaks pay for the division.
AUTODJ_INLINE float softClip(float x) noexcept {
  constexpr float kKnee = 0.8f;
  constexpr float kRange = 1.0f - kKnee;
  const float a = std::fabs(x);
  if (AUTODJ_LIKELY(a <= kKnee)) return x;
  const float t = (a - kKnee) * (1.0f / kRange);
  const float y = kKnee + kRange * (t / (1.0f + t));
  return x < 0.f ? -y : y;
}

inline float dbToLin(float db) noexcept { return std::exp2(db * 0.16609640474436813f); }

/// Coefficient of a one-pole smoother stepped once per block of `blockSeconds`.
inline float onePoleBlockCoef(float tauMs, float blockSeconds) noexcept {
  if (tauMs <= 0.f) return 1.f;
  return 1.f - std::exp(-blockSeconds * 1000.f / tauMs);
}

/// Control-rate smoother: call next() once per block.
struct BlockSmoother {
  float value = 0.f;
  bool primed = false;

  void snap(float v) noexcept {
    value = v;
    primed = true;
  }
  float next(float target, float coef) noexcept {
    if (!primed) {
      snap(target);
      return value;
    }
    value += (target - value) * coef;
    return value;
  }
};

}  // namespace autodj
