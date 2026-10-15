#pragma once
// Bounded single-producer / single-consumer ring buffer.
//
//  - wait-free for both sides, no allocation after construction
//  - index counters grow monotonically (size_t); capacity must be a power of two
//  - each side keeps a cached copy of the other side's index so the shared cache line is only
//    touched when the queue looks full/empty (classic Vyukov/Rigtorp layout)
//  - T must be trivially copyable: messages are plain data, pointers inside them are only
//    ever dereferenced by the side that owns the pointee
//
// Exactly ONE thread may call tryPush() and exactly ONE thread may call tryPop().
// Multi-producer use must go through a lock on the (non-realtime) producer side.

#include <atomic>
#include <cstddef>
#include <type_traits>

#include "autodj/core/Platform.h"

namespace autodj {

template <typename T, std::size_t Capacity>
class SpscQueue {
  static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "capacity must be a power of two");
  static_assert(std::is_trivially_copyable_v<T>, "queue payloads must be trivially copyable");

 public:
  SpscQueue() = default;
  SpscQueue(const SpscQueue&) = delete;
  SpscQueue& operator=(const SpscQueue&) = delete;

  static constexpr std::size_t capacity() noexcept { return Capacity; }

  /// Producer side.
  bool tryPush(const T& v) noexcept {
    const std::size_t t = tail_.load(std::memory_order_relaxed);
    if (t - cachedHead_ == Capacity) {
      cachedHead_ = head_.load(std::memory_order_acquire);
      if (t - cachedHead_ == Capacity) return false;
    }
    buf_[t & kMask] = v;
    tail_.store(t + 1, std::memory_order_release);
    return true;
  }

  /// Consumer side.
  bool tryPop(T& out) noexcept {
    const std::size_t h = head_.load(std::memory_order_relaxed);
    if (h == cachedTail_) {
      cachedTail_ = tail_.load(std::memory_order_acquire);
      if (h == cachedTail_) return false;
    }
    out = buf_[h & kMask];
    head_.store(h + 1, std::memory_order_release);
    return true;
  }

  /// Consumer side: pops up to maxItems and hands each to fn. Returns the number popped.
  template <typename Fn>
  std::size_t drain(Fn&& fn, std::size_t maxItems = Capacity) noexcept {
    std::size_t n = 0;
    T item{};
    while (n < maxItems && tryPop(item)) {
      fn(item);
      ++n;
    }
    return n;
  }

  /// Approximate fill level (exact only when called from one of the two sides).
  std::size_t sizeApprox() const noexcept {
    return tail_.load(std::memory_order_acquire) - head_.load(std::memory_order_acquire);
  }

 private:
  static constexpr std::size_t kMask = Capacity - 1;

  alignas(kCacheLine) std::atomic<std::size_t> head_{0};  // written by the consumer
  alignas(kCacheLine) std::atomic<std::size_t> tail_{0};  // written by the producer
  alignas(kCacheLine) std::size_t cachedHead_ = 0;        // producer's view of head_
  alignas(kCacheLine) std::size_t cachedTail_ = 0;        // consumer's view of tail_
  alignas(kCacheLine) T buf_[Capacity]{};  // zero-initialised once at construction (not on the RT path)
};

}  // namespace autodj
