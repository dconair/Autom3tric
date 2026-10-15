#include "autodj/granular/EnvelopeTables.h"

#include <algorithm>
#include <cmath>

namespace autodj::granular {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kGaussSigma = 0.17;
constexpr double kTukeyAlpha = 0.5;
constexpr double kTrapezoidRamp = 0.25;
// "Square" is a rectangular grain with very short linear edges: still exactly zero at both ends (never a click at the
// table level) but as hard as a grain can get. 3 % of a 50 ms grain is 1.5 ms.
constexpr double kSquareRamp = 0.03;

double gaussRaw(double x) {
  const double d = (x - 0.5) / kGaussSigma;
  return std::exp(-0.5 * d * d);
}
}  // namespace

double EnvelopeTables::evaluate(EnvShape shape, double x) noexcept {
  x = std::clamp(x, 0.0, 1.0);
  switch (shape) {
    case EnvShape::Hann:
      return 0.5 * (1.0 - std::cos(2.0 * kPi * x));
    case EnvShape::Gauss: {
      // Truncated Gaussian, re-normalised so both ends are exactly 0 and the peak is exactly 1.
      const double edge = gaussRaw(0.0);
      return (gaussRaw(x) - edge) / (1.0 - edge);
    }
    case EnvShape::Tukey: {
      const double a = kTukeyAlpha;
      if (x < a * 0.5) return 0.5 * (1.0 + std::cos(2.0 * kPi / a * (x - a * 0.5)));
      if (x > 1.0 - a * 0.5) return 0.5 * (1.0 + std::cos(2.0 * kPi / a * (x - 1.0 + a * 0.5)));
      return 1.0;
    }
    case EnvShape::Trapezoid:
      return std::min({1.0, x / kTrapezoidRamp, (1.0 - x) / kTrapezoidRamp});
    case EnvShape::Square:
      return std::min({1.0, x / kSquareRamp, (1.0 - x) / kSquareRamp});
    case EnvShape::Triangle:
      return 1.0 - std::fabs(2.0 * x - 1.0);
    default:
      return 0.0;
  }
}

EnvelopeTables::EnvelopeTables() {
  for (int s = 0; s < static_cast<int>(EnvShape::Count); ++s) {
    for (int i = 0; i <= kEnvTableSize; ++i) {
      tables_[s][i] = static_cast<float>(evaluate(static_cast<EnvShape>(s), static_cast<double>(i) / kEnvTableSize));
    }
    for (int i = kEnvTableSize + 1; i < kEnvTableStride; ++i) tables_[s][i] = 0.f;
    tables_[s][0] = 0.f;
    tables_[s][kEnvTableSize] = 0.f;
  }
}

}  // namespace autodj::granular
