// Global operator new/delete replacement that trips the audio-thread guard.
//
// Compiled only when AUTODJ_RT_GUARD=ON and linked directly into the final binary
// (as an OBJECT library) so the linker cannot prefer libstdc++'s/libc++'s definitions.

#define AUTODJ_RT_GUARD 1
#include <cstdlib>
#include <new>

#include "autodj/core/RtGuard.h"

namespace {

inline void checkAlloc() noexcept {
  if (autodj::rt::inAudioThread()) autodj::rt::reportViolation("operator new (heap allocation)");
}

inline void checkFree(void* p) noexcept {
  if (p != nullptr && autodj::rt::inAudioThread()) autodj::rt::reportViolation("operator delete (heap free)");
}

inline void* allocate(std::size_t n) noexcept {
  checkAlloc();
  return std::malloc(n ? n : 1);
}

inline void* allocateAligned(std::size_t n, std::size_t align) noexcept {
  checkAlloc();
  void* p = nullptr;
  if (align < sizeof(void*)) align = sizeof(void*);
  if (posix_memalign(&p, align, n ? n : 1) != 0) return nullptr;
  return p;
}

[[noreturn]] void oom() noexcept { std::abort(); }

}  // namespace

void* operator new(std::size_t n) {
  void* p = allocate(n);
  if (!p) oom();
  return p;
}
void* operator new[](std::size_t n) {
  void* p = allocate(n);
  if (!p) oom();
  return p;
}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return allocate(n); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return allocate(n); }

void* operator new(std::size_t n, std::align_val_t a) {
  void* p = allocateAligned(n, static_cast<std::size_t>(a));
  if (!p) oom();
  return p;
}
void* operator new[](std::size_t n, std::align_val_t a) {
  void* p = allocateAligned(n, static_cast<std::size_t>(a));
  if (!p) oom();
  return p;
}
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
  return allocateAligned(n, static_cast<std::size_t>(a));
}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
  return allocateAligned(n, static_cast<std::size_t>(a));
}

void operator delete(void* p) noexcept { checkFree(p), std::free(p); }
void operator delete[](void* p) noexcept { checkFree(p), std::free(p); }
void operator delete(void* p, std::size_t) noexcept { checkFree(p), std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { checkFree(p), std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { checkFree(p), std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { checkFree(p), std::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { checkFree(p), std::free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { checkFree(p), std::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { checkFree(p), std::free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { checkFree(p), std::free(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { checkFree(p), std::free(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { checkFree(p), std::free(p); }
