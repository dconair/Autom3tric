#pragma once
// Monotonic nanosecond clock. clock_gettime(CLOCK_MONOTONIC) is a vDSO call on Linux/Android:
// no syscall, no allocation, safe on the audio thread.

#include <cstdint>
#include <ctime>

namespace autodj {

inline std::uint64_t monotonicNowNs() noexcept {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ull + static_cast<std::uint64_t>(ts.tv_nsec);
}

/// Injectable for deterministic load tests.
using ClockFn = std::uint64_t (*)() noexcept;

}  // namespace autodj
