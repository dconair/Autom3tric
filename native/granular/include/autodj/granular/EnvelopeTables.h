#pragma once
// Precomputed grain envelopes: 2048 points + guard entries, read with linear interpolation.
// All shapes are exactly zero at both ends, so a grain can never start or stop with a click.
// Built once in prepare(); the inner loop never evaluates cos/exp.

#include "autodj/core/Platform.h"
#include "autodj/granular/GranularParams.h"

namespace autodj::granular {

inline constexpr int kEnvTableSize = 2048;
// Entries 2048..2051 are guards (zero) so "index + 1" after float rounding stays in bounds.
inline constexpr int kEnvTableStride = kEnvTableSize + 4;

class EnvelopeTables {
 public:
  EnvelopeTables();

  const float* table(int shape) const noexcept {
    return tables_[(shape >= 0 && shape < static_cast<int>(EnvShape::Count)) ? shape : 0];
  }

  /// Analytic envelope at x in [0,1] - the definition the tables are sampled from.
  static double evaluate(EnvShape shape, double x) noexcept;

 private:
  alignas(kCacheLine) float tables_[static_cast<int>(EnvShape::Count)][kEnvTableStride];
};

}  // namespace autodj::granular
