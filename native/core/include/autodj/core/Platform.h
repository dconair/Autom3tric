#pragma once
// Compiler / architecture macros shared by every native module.

#include <cstddef>
#include <cstdint>

#if defined(__aarch64__) || defined(_M_ARM64)
#define AUTODJ_ARM64 1
#endif

#if defined(AUTODJ_ARM64) || defined(__ARM_NEON)
#define AUTODJ_NEON 1
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define AUTODJ_X64 1
#define AUTODJ_SSE2 1
#endif

#if defined(__GNUC__) || defined(__clang__)
#define AUTODJ_INLINE inline __attribute__((always_inline))
#define AUTODJ_NOINLINE __attribute__((noinline))
#define AUTODJ_LIKELY(x) __builtin_expect(!!(x), 1)
#define AUTODJ_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define AUTODJ_RESTRICT __restrict__
#else
#define AUTODJ_INLINE inline
#define AUTODJ_NOINLINE
#define AUTODJ_LIKELY(x) (x)
#define AUTODJ_UNLIKELY(x) (x)
#define AUTODJ_RESTRICT
#endif

namespace autodj {

inline constexpr std::size_t kCacheLine = 64;

// Upper bound for one internal processing block. All per-block scratch buffers are sized with it.
inline constexpr int kMaxBlockFrames = 256;

}  // namespace autodj
