#pragma once
// Helpers shared by the test suites.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "autodj/core/RtGuard.h"
#include "autodj/engine/Engine.h"

namespace testutil {

constexpr double kPi = 3.14159265358979323846;

/// Makes `p` (and what it points at) observable to the optimizer. Clang is allowed to drop a new/delete pair whose
/// storage never escapes ([expr.new]); a test that wants the allocation to really reach operator new must call this.
inline void escape(const void* p) { asm volatile("" : : "g"(p) : "memory"); }

/// Installs a handler that counts audio-thread violations instead of aborting.
struct ViolationCounter {
  ViolationCounter() {
    prev_ = autodj::rt::setViolationHandler(&ViolationCounter::onViolation);
    baseline_ = autodj::rt::violationCount();
  }
  ~ViolationCounter() { autodj::rt::setViolationHandler(prev_); }
  std::uint64_t count() const { return autodj::rt::violationCount() - baseline_; }

 private:
  static void onViolation(const char*) noexcept {}
  autodj::rt::ViolationHandler prev_ = nullptr;
  std::uint64_t baseline_ = 0;
};

inline bool allFinite(const std::vector<float>& v) {
  for (float x : v) {
    if (!std::isfinite(x)) return false;
  }
  return true;
}

inline float peak(const std::vector<float>& v, std::size_t from = 0) {
  float p = 0.f;
  for (std::size_t i = from; i < v.size(); ++i) p = std::max(p, std::fabs(v[i]));
  return p;
}

inline double rms(const std::vector<float>& v, std::size_t from = 0, std::size_t to = SIZE_MAX) {
  to = std::min(to, v.size());
  if (to <= from) return 0.0;
  double acc = 0.0;
  for (std::size_t i = from; i < to; ++i) acc += static_cast<double>(v[i]) * v[i];
  return std::sqrt(acc / static_cast<double>(to - from));
}

/// Largest |x[n+1] - 2 x[n] + x[n-1]| over one channel of an interleaved stereo buffer, starting at frame `fromFrame`.
/// A smooth signal has a small second difference; a click (discontinuity) shows up as a large one.
inline float maxSecondDifference(const std::vector<float>& stereo, int channel, std::size_t fromFrame = 1) {
  float m = 0.f;
  const std::size_t frames = stereo.size() / 2;
  for (std::size_t n = std::max<std::size_t>(fromFrame, 1); n + 1 < frames; ++n) {
    const float d2 = stereo[2 * (n + 1) + channel] - 2.f * stereo[2 * n + channel] + stereo[2 * (n - 1) + channel];
    m = std::max(m, std::fabs(d2));
  }
  return m;
}

/// |DFT| of x (Hann-windowed) at frequency f.
inline double dftMagnitude(const float* x, int n, double f, double sampleRate, int stride = 1) {
  double re = 0.0, im = 0.0;
  const double w = 2.0 * kPi * f / sampleRate;
  for (int i = 0; i < n; ++i) {
    const double win = 0.5 * (1.0 - std::cos(2.0 * kPi * (i + 0.5) / n));
    const double v = static_cast<double>(x[static_cast<std::size_t>(i) * stride]) * win;
    re += v * std::cos(w * i);
    im -= v * std::sin(w * i);
  }
  return std::sqrt(re * re + im * im);
}

/// Frequency of the strongest spectral line within [lo, hi] Hz: coarse scan then golden-section refinement.
inline double peakFrequency(const float* x, int n, double sampleRate, double lo, double hi, int stride = 1) {
  double best = lo, bestMag = -1.0;
  const double step = std::max(0.5, sampleRate / n * 0.25);
  for (double f = lo; f <= hi; f += step) {
    const double m = dftMagnitude(x, n, f, sampleRate, stride);
    if (m > bestMag) {
      bestMag = m;
      best = f;
    }
  }
  double a = best - step, b = best + step;
  const double gr = 0.6180339887498949;
  double c = b - gr * (b - a), d = a + gr * (b - a);
  double fc = dftMagnitude(x, n, c, sampleRate, stride), fd = dftMagnitude(x, n, d, sampleRate, stride);
  for (int it = 0; it < 60; ++it) {
    if (fc > fd) {
      b = d; d = c; fd = fc;
      c = b - gr * (b - a);
      fc = dftMagnitude(x, n, c, sampleRate, stride);
    } else {
      a = c; c = d; fc = fd;
      d = a + gr * (b - a);
      fd = dftMagnitude(x, n, d, sampleRate, stride);
    }
  }
  return 0.5 * (a + b);
}

inline double centsBetween(double f, double ref) { return 1200.0 * std::log2(f / ref); }

/// Renders `frames` stereo frames through engine.process() in callbacks of `callbackFrames`.
inline std::vector<float> render(autodj::Engine& e, int frames, int callbackFrames = 192) {
  std::vector<float> out(static_cast<std::size_t>(frames) * 2);
  int done = 0;
  while (done < frames) {
    const int n = std::min(callbackFrames, frames - done);
    e.process(out.data() + static_cast<std::size_t>(done) * 2, n);
    done += n;
  }
  return out;
}

inline void setByKey(autodj::Engine& e, const std::string& key, float v) {
  const autodj::ParamId id = e.registry().find(key);
  ASSERT_NE(id, autodj::kInvalidParam) << key;
  e.params().set(id, v);
}

}  // namespace testutil
