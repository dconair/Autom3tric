#include "autodj/core/Resampler.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>

namespace autodj {

namespace {

constexpr int kZeroCrossings = 16;
constexpr double kBeta = 9.0;

double besselI0(double x) {
  double sum = 1.0, term = 1.0;
  const double q = x * x * 0.25;
  for (int k = 1; k < 60; ++k) {
    term *= q / (static_cast<double>(k) * k);
    sum += term;
    if (term < 1e-14 * sum) break;
  }
  return sum;
}

double kernel(double x) {  // sinc(x) * Kaiser(x / Z), |x| <= Z
  const double ax = std::fabs(x);
  if (ax >= kZeroCrossings) return 0.0;
  const double s = ax < 1e-12 ? 1.0 : std::sin(M_PI * ax) / (M_PI * ax);
  const double r = ax / kZeroCrossings;
  return s * besselI0(kBeta * std::sqrt(1.0 - r * r)) / besselI0(kBeta);
}

}  // namespace

std::vector<float> resampleInterleaved(const float* in, std::size_t frames, int channels, int srcRate, int dstRate) {
  const std::size_t ch = static_cast<std::size_t>(std::max(1, channels));
  if (srcRate == dstRate || frames == 0 || srcRate <= 0 || dstRate <= 0) return std::vector<float>(in, in + frames * ch);

  const std::int64_t g = std::gcd<std::int64_t>(srcRate, dstRate);
  const std::int64_t srcG = srcRate / g;  // source frames per period
  const std::int64_t dstG = dstRate / g;  // output frames per period
  const double scale = std::min(1.0, static_cast<double>(dstRate) / srcRate);
  const int half = static_cast<int>(std::ceil(kZeroCrossings / scale));
  const int taps = 2 * half;

  // weights[phase][tap] for tap j = -half+1 .. half relative to floor(t)
  std::vector<float> weights(static_cast<std::size_t>(dstG) * static_cast<std::size_t>(taps));
  for (std::int64_t p = 0; p < dstG; ++p) {
    const double frac = static_cast<double>(p) / static_cast<double>(dstG);
    double sum = 0.0;
    std::vector<double> w(static_cast<std::size_t>(taps));
    for (int j = -half + 1; j <= half; ++j) {
      const double d = (static_cast<double>(j) - frac) * scale;
      w[static_cast<std::size_t>(j + half - 1)] = scale * kernel(d);
      sum += w[static_cast<std::size_t>(j + half - 1)];
    }
    for (int t = 0; t < taps; ++t) weights[static_cast<std::size_t>(p) * taps + t] = static_cast<float>(w[static_cast<std::size_t>(t)] / sum);
  }

  const std::size_t outFrames = static_cast<std::size_t>((static_cast<std::int64_t>(frames) * dstRate) / srcRate);
  std::vector<float> out(outFrames * ch);
  const std::int64_t n = static_cast<std::int64_t>(frames);
  for (std::size_t o = 0; o < outFrames; ++o) {
    const std::int64_t num = static_cast<std::int64_t>(o) * srcG;
    const std::int64_t c = num / dstG;
    const std::int64_t phase = num % dstG;
    const float* w = weights.data() + static_cast<std::size_t>(phase) * taps;
    const std::int64_t first = c - half + 1;
    if (first >= 0 && first + taps <= n) {
      for (std::size_t k = 0; k < ch; ++k) {
        float acc = 0.f;
        const float* src = in + static_cast<std::size_t>(first) * ch + k;
        for (int t = 0; t < taps; ++t) acc += w[t] * src[static_cast<std::size_t>(t) * ch];
        out[o * ch + k] = acc;
      }
    } else {
      for (std::size_t k = 0; k < ch; ++k) {
        float acc = 0.f;
        for (int t = 0; t < taps; ++t) {
          const std::int64_t i = first + t;
          if (i >= 0 && i < n) acc += w[t] * in[static_cast<std::size_t>(i) * ch + k];
        }
        out[o * ch + k] = acc;
      }
    }
  }
  return out;
}

}  // namespace autodj
