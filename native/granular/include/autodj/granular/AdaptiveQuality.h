#pragma once
// Load-driven quality governor for the granular engine.
//
// Input : callback load = processing time / buffer duration, once per audio callback.
// Output: a level 0..3 that the engine translates into limits (see limitsFor):
//
//   level 0  full quality: user interpolation (Hermite allowed), requested density, all voices
//   level 1  linear interpolation only
//   level 2  + density cap 75 %, grain cap 75 %, one voice less
//   level 3  + density cap 50 %, grain cap 50 %, two voices less
//
// Policy: step DOWN (level+1) when the smoothed load stays above 70 % for 100 ms, or at once if a
// single callback exceeds 90 %. Step UP only after the load has stayed below 50 % for 1.5 s, and
// every step-down shortly after a step-up doubles that hold time (up to 24 s) so a borderline
// workload settles instead of toggling quality every couple of seconds. At most one step per
// 300 ms. Pure state machine - unit-testable without any audio.

#include <algorithm>
#include <cmath>

namespace autodj::granular {

class AdaptiveQuality {
 public:
  struct Config {
    float downThreshold = 0.70f;
    float upThreshold = 0.50f;
    float emergencyLoad = 0.90f;
    float downHoldMs = 100.f;
    float upHoldMs = 1500.f;
    float maxUpHoldMs = 24000.f;
    float cooldownMs = 300.f;
    float emaTauMs = 60.f;
  };

  static constexpr int kMaxLevel = 3;

  struct Limits {
    bool forceLinear;
    float densityScale;
    float grainCapScale;
    int voiceReduction;
  };

  static Limits limitsFor(int level) noexcept {
    switch (level) {
      case 0: return {false, 1.0f, 1.0f, 0};
      case 1: return {true, 1.0f, 1.0f, 0};
      case 2: return {true, 0.75f, 0.75f, 1};
      default: return {true, 0.5f, 0.5f, 2};
    }
  }

  AdaptiveQuality() = default;
  explicit AdaptiveQuality(const Config& c) : cfg_(c), upHoldCur_(c.upHoldMs) {}

  void reset() noexcept {
    level_ = 0;
    ema_ = 0.f;
    primed_ = false;
    aboveMs_ = belowMs_ = 0.f;
    cooldownMs_ = 0.f;
    sinceUpMs_ = 1e9f;
    calmMs_ = 0.f;
    upHoldCur_ = cfg_.upHoldMs;
  }

  /// Call once per audio callback. `load` = busy time / available time, `dtSeconds` = buffer duration.
  void update(float load, float dtSeconds) noexcept {
    const float dtMs = dtSeconds * 1000.f;
    if (!primed_) {
      ema_ = load;
      primed_ = true;
    } else {
      const float a = 1.f - std::exp(-dtMs / cfg_.emaTauMs);
      ema_ += (load - ema_) * a;
    }
    cooldownMs_ = std::max(0.f, cooldownMs_ - dtMs);
    sinceUpMs_ += dtMs;

    if (load >= cfg_.emergencyLoad && level_ < kMaxLevel && cooldownMs_ <= 0.f) {
      stepDown();
      return;
    }
    if (ema_ > cfg_.downThreshold) {
      aboveMs_ += dtMs;
      belowMs_ = 0.f;
      calmMs_ = 0.f;
      if (aboveMs_ >= cfg_.downHoldMs && level_ < kMaxLevel && cooldownMs_ <= 0.f) stepDown();
    } else if (ema_ < cfg_.upThreshold) {
      belowMs_ += dtMs;
      aboveMs_ = 0.f;
      calmMs_ += dtMs;
      if (belowMs_ >= upHoldCur_ && level_ > 0 && cooldownMs_ <= 0.f) {
        --level_;
        belowMs_ = 0.f;
        cooldownMs_ = cfg_.cooldownMs;
        sinceUpMs_ = 0.f;
      }
      if (calmMs_ > 30000.f) upHoldCur_ = cfg_.upHoldMs;  // stable for a long time: forget the back-off
    } else {
      aboveMs_ = belowMs_ = 0.f;
    }
  }

  int level() const noexcept { return level_; }
  float smoothedLoad() const noexcept { return ema_; }

 private:
  void stepDown() noexcept {
    ++level_;
    aboveMs_ = 0.f;
    belowMs_ = 0.f;
    cooldownMs_ = cfg_.cooldownMs;
    if (sinceUpMs_ < upHoldCur_ * 2.f) upHoldCur_ = std::min(upHoldCur_ * 2.f, cfg_.maxUpHoldMs);
  }

  Config cfg_{};
  int level_ = 0;
  float ema_ = 0.f;
  bool primed_ = false;
  float aboveMs_ = 0.f;
  float belowMs_ = 0.f;
  float cooldownMs_ = 0.f;
  float sinceUpMs_ = 1e9f;
  float calmMs_ = 0.f;
  float upHoldCur_ = 1500.f;
};

}  // namespace autodj::granular
