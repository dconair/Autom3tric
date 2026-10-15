#pragma once
// Sample-accurate grain trigger generator for free-running density.
//
// Density is integrated as a phase accumulator (phase += density / sampleRate per sample, a
// grain fires when it crosses the threshold). The crossing is solved analytically, so the grain
// is started at the exact sample offset inside the block - the result does not depend on the
// block size, and a density change takes effect immediately instead of after the pending
// inter-onset interval.
//
// Timing jitter randomises the threshold (1 +- 0.9 * jitter), which keeps the long-term mean
// density equal to the requested one.

#include <cmath>

#include "autodj/core/Dsp.h"

namespace autodj::granular {

class GrainScheduler {
 public:
  /// Next call to run() fires a grain at the first sample.
  void restart() noexcept { phase_ = threshold_; }

  void reset() noexcept {
    threshold_ = 1.0;
    phase_ = threshold_;
  }

  /// Calls spawnAt(sampleOffset) for every grain that starts inside this block.
  /// `maxSpawns` bounds the work per block (protects against absurd densities).
  template <typename Rng, typename Fn>
  int run(int frames, double densityHz, double sampleRate, float jitter, Rng& rng, Fn&& spawnAt,
          int maxSpawns = 64) noexcept {
    const double inc = densityHz / sampleRate;
    if (!(inc > 0.0)) return 0;
    double p = phase_;
    int i = 0;
    int spawned = 0;
    while (spawned < maxSpawns) {
      const double at = static_cast<double>(i) + (threshold_ - p) / inc;
      if (at >= static_cast<double>(frames)) {
        p += static_cast<double>(frames - i) * inc;
        i = frames;
        break;
      }
      // 1e-6 sample of slack: an onset that mathematically sits on an integer sample (e.g. 100 Hz at
      // 48 kHz = every 480 samples) must not flip to the next sample because of rounding in `at`.
      int off = static_cast<int>(std::ceil(at - 1e-6));
      if (off < i) off = i;
      if (off >= frames) {  // crossing lies between the last sample and the next block: it fires at sample 0 there
        p += static_cast<double>(frames - i) * inc;
        i = frames;
        break;
      }
      spawnAt(off);
      ++spawned;
      p += static_cast<double>(off - i) * inc - threshold_;
      i = off;
      threshold_ = 1.0 + static_cast<double>(jitter) * 0.9 * static_cast<double>(rng.bipolar());
      if (p > threshold_) p = threshold_;  // never queue more than one immediate extra grain after a density jump
    }
    if (i < frames) p += static_cast<double>(frames - i) * inc;
    phase_ = p;
    return spawned;
  }

 private:
  double phase_ = 1.0;
  double threshold_ = 1.0;
};

}  // namespace autodj::granular
