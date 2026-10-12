#pragma once
// Small iterative radix-2 FFT for the offline analysis code (never used on the audio thread).
// Double precision keeps the analysis results identical across compilers and architectures.

#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

namespace autodj::analysis {

class Fft {
 public:
  explicit Fft(std::size_t n) : n_(n), rev_(n), tw_(n / 2) {
    std::size_t bits = 0;
    while ((std::size_t{1} << bits) < n) ++bits;
    for (std::size_t i = 0; i < n; ++i) {
      std::size_t r = 0;
      for (std::size_t b = 0; b < bits; ++b) r |= ((i >> b) & 1u) << (bits - 1 - b);
      rev_[i] = r;
    }
    for (std::size_t k = 0; k < n / 2; ++k) {
      const double a = -2.0 * M_PI * static_cast<double>(k) / static_cast<double>(n);
      tw_[k] = {std::cos(a), std::sin(a)};
    }
  }

  std::size_t size() const { return n_; }

  /// In-place forward transform; `x` holds n complex values.
  void forward(std::complex<double>* x) const {
    for (std::size_t i = 0; i < n_; ++i) {
      if (i < rev_[i]) std::swap(x[i], x[rev_[i]]);
    }
    for (std::size_t len = 2; len <= n_; len <<= 1) {
      const std::size_t half = len / 2;
      const std::size_t step = n_ / len;
      for (std::size_t i = 0; i < n_; i += len) {
        for (std::size_t j = 0; j < half; ++j) {
          const std::complex<double> w = tw_[j * step];
          const std::complex<double> u = x[i + j];
          const std::complex<double> v = x[i + j + half] * w;
          x[i + j] = u + v;
          x[i + j + half] = u - v;
        }
      }
    }
  }

  /// Magnitude spectrum (bins 0..n/2) of a real frame that is already windowed.
  void magnitudes(const double* frame, double* mag, std::vector<std::complex<double>>& scratch) const {
    scratch.resize(n_);
    for (std::size_t i = 0; i < n_; ++i) scratch[i] = {frame[i], 0.0};
    forward(scratch.data());
    for (std::size_t k = 0; k <= n_ / 2; ++k) mag[k] = std::abs(scratch[k]);
  }

 private:
  std::size_t n_;
  std::vector<std::size_t> rev_;
  std::vector<std::complex<double>> tw_;
};

inline std::vector<double> hannWindow(std::size_t n) {
  std::vector<double> w(n);
  for (std::size_t i = 0; i < n; ++i) w[i] = 0.5 - 0.5 * std::cos(2.0 * M_PI * (static_cast<double>(i) + 0.5) / static_cast<double>(n));
  return w;
}

}  // namespace autodj::analysis
