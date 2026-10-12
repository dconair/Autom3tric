#include "autodj/core/RtGuard.h"

#if AUTODJ_RT_GUARD

#include <unistd.h>

#include <atomic>
#include <cstdlib>
#include <cstring>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace autodj::rt {
namespace {

thread_local int tl_depth = 0;
thread_local int tl_allow = 0;
thread_local bool tl_reporting = false;

std::atomic<std::uint64_t> g_violations{0};

void defaultHandler(const char* what) noexcept {
  static const char prefix[] = "AUTODJ RT VIOLATION on audio thread: ";
  ssize_t r = write(2, prefix, sizeof(prefix) - 1);
  r = write(2, what, std::strlen(what));
  r = write(2, "\n", 1);
  (void)r;
#ifdef __ANDROID__
  __android_log_print(ANDROID_LOG_FATAL, "AutoDJ", "RT violation on audio thread: %s", what);
#endif
  std::abort();
}

std::atomic<ViolationHandler> g_handler{&defaultHandler};

}  // namespace

bool inAudioThread() noexcept { return tl_depth > 0 && tl_allow == 0; }
void enterAudioThread() noexcept { ++tl_depth; }
void leaveAudioThread() noexcept { --tl_depth; }

void reportViolation(const char* what) noexcept {
  if (tl_reporting) return;  // the handler itself may allocate
  tl_reporting = true;
  g_violations.fetch_add(1, std::memory_order_relaxed);
  g_handler.load(std::memory_order_relaxed)(what);
  tl_reporting = false;
}

ViolationHandler setViolationHandler(ViolationHandler h) noexcept {
  return g_handler.exchange(h ? h : &defaultHandler, std::memory_order_relaxed);
}

std::uint64_t violationCount() noexcept { return g_violations.load(std::memory_order_relaxed); }

ScopedAllowAlloc::ScopedAllowAlloc() noexcept { ++tl_allow; }
ScopedAllowAlloc::~ScopedAllowAlloc() noexcept { --tl_allow; }

}  // namespace autodj::rt

#endif  // AUTODJ_RT_GUARD
