#pragma once
// Flush-to-zero / denormals-are-zero for the calling thread.
//
// Denormal floats are 50-100x slower on many ARM and x86 cores. Granular feedback paths and
// envelope tails run into them constantly, so the audio thread switches them off for the
// whole duration of a callback.

#include "autodj/core/Platform.h"

#if defined(AUTODJ_X64)
#include <xmmintrin.h>
#endif

namespace autodj {

class ScopedFlushDenormals {
 public:
  ScopedFlushDenormals() noexcept {
#if defined(AUTODJ_X64)
    saved_ = _mm_getcsr();
    _mm_setcsr(saved_ | 0x8040u);  // FTZ (bit 15) | DAZ (bit 6)
#elif defined(AUTODJ_ARM64)
    std::uint64_t fpcr;
    asm volatile("mrs %0, fpcr" : "=r"(fpcr));
    saved_ = fpcr;
    fpcr |= (1ull << 24);  // FZ
    asm volatile("msr fpcr, %0" : : "r"(fpcr));
#endif
  }

  ~ScopedFlushDenormals() noexcept {
#if defined(AUTODJ_X64)
    _mm_setcsr(saved_);
#elif defined(AUTODJ_ARM64)
    asm volatile("msr fpcr, %0" : : "r"(saved_));
#endif
  }

  ScopedFlushDenormals(const ScopedFlushDenormals&) = delete;
  ScopedFlushDenormals& operator=(const ScopedFlushDenormals&) = delete;

  /// True if the current thread currently flushes denormals (test hook).
  static bool active() noexcept {
#if defined(AUTODJ_X64)
    return (_mm_getcsr() & 0x8040u) == 0x8040u;
#elif defined(AUTODJ_ARM64)
    std::uint64_t fpcr;
    asm volatile("mrs %0, fpcr" : "=r"(fpcr));
    return (fpcr & (1ull << 24)) != 0;
#else
    return false;
#endif
  }

 private:
#if defined(AUTODJ_X64)
  unsigned saved_ = 0;
#else
  std::uint64_t saved_ = 0;
#endif
};

}  // namespace autodj
