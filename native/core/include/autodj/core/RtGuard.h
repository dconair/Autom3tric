#pragma once
// Audio-thread rule enforcement.
//
// The audio callback is marked with an AudioThreadScope. In builds with AUTODJ_RT_GUARD=1
// (all Debug builds and every test binary) the replaced global operator new/delete
// (RtAllocGuard.cpp) and the AUTODJ_RT_FORBIDDEN() hooks report a violation as soon as
// something allocates, frees or does forbidden I/O inside such a scope. The default handler
// prints one line and aborts so the offending stack is right there in the crash report.
//
// In Release builds everything below compiles to nothing.

#include <cstdint>

#ifndef AUTODJ_RT_GUARD
#define AUTODJ_RT_GUARD 0
#endif

namespace autodj::rt {

using ViolationHandler = void (*)(const char* what) noexcept;

#if AUTODJ_RT_GUARD

bool inAudioThread() noexcept;
void enterAudioThread() noexcept;
void leaveAudioThread() noexcept;

/// Reports a violation through the installed handler (default: log + abort).
void reportViolation(const char* what) noexcept;

/// Replace the handler (tests install a counting handler). Returns the previous one.
ViolationHandler setViolationHandler(ViolationHandler h) noexcept;
std::uint64_t violationCount() noexcept;

/// Temporarily lifts the guard for the current thread (e.g. a test that allocates on purpose).
class ScopedAllowAlloc {
 public:
  ScopedAllowAlloc() noexcept;
  ~ScopedAllowAlloc() noexcept;
  ScopedAllowAlloc(const ScopedAllowAlloc&) = delete;
  ScopedAllowAlloc& operator=(const ScopedAllowAlloc&) = delete;
};

#define AUTODJ_RT_FORBIDDEN(what)                                  \
  do {                                                             \
    if (::autodj::rt::inAudioThread()) ::autodj::rt::reportViolation(what); \
  } while (0)

#else

inline bool inAudioThread() noexcept { return false; }
inline void enterAudioThread() noexcept {}
inline void leaveAudioThread() noexcept {}
inline void reportViolation(const char*) noexcept {}
inline ViolationHandler setViolationHandler(ViolationHandler) noexcept { return nullptr; }
inline std::uint64_t violationCount() noexcept { return 0; }
class ScopedAllowAlloc {};

#define AUTODJ_RT_FORBIDDEN(what) \
  do {                            \
  } while (0)

#endif

/// Marks the calling thread as "inside the audio callback" for the lifetime of the object.
class AudioThreadScope {
 public:
  AudioThreadScope() noexcept { enterAudioThread(); }
  ~AudioThreadScope() noexcept { leaveAudioThread(); }
  AudioThreadScope(const AudioThreadScope&) = delete;
  AudioThreadScope& operator=(const AudioThreadScope&) = delete;
};

}  // namespace autodj::rt
