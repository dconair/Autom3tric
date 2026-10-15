#pragma once
// Wait-free "latest value" channel for one writer and one reader.
//
// Used for engine -> UI snapshots (levels, play-head, grain cloud). The writer never waits for
// the reader and the reader never sees a half-written snapshot; if the reader is slower than
// the writer it simply skips intermediate snapshots.
//
//   writer:  T& s = tb.writeSlot();  fill s ...;  tb.publish();
//   reader:  T snapshot;  if (tb.read(snapshot)) { ...new data... }
//
// Exactly one writer thread and one reader thread.

#include <atomic>
#include <cstdint>

#include "autodj/core/Platform.h"

namespace autodj {

template <typename T>
class TripleBuffer {
 public:
  TripleBuffer() = default;
  TripleBuffer(const TripleBuffer&) = delete;
  TripleBuffer& operator=(const TripleBuffer&) = delete;

  /// Writer: slot to fill. Stale contents from an earlier snapshot, overwrite everything you need.
  T& writeSlot() noexcept { return slots_[back_]; }

  /// Writer: make the filled slot visible to the reader.
  void publish() noexcept {
    const unsigned old = shared_.exchange(back_ | kDirty, std::memory_order_acq_rel);
    back_ = old & kIndexMask;
  }

  /// Reader: copies the newest snapshot into `out`. Returns false if nothing new was published.
  bool read(T& out) noexcept {
    if ((shared_.load(std::memory_order_relaxed) & kDirty) == 0) return false;
    const unsigned old = shared_.exchange(front_, std::memory_order_acq_rel);
    front_ = old & kIndexMask;
    out = slots_[front_];
    return true;
  }

 private:
  static constexpr unsigned kDirty = 4u;
  static constexpr unsigned kIndexMask = 3u;

  T slots_[3]{};
  alignas(kCacheLine) std::atomic<unsigned> shared_{1};  // middle slot index (+dirty bit)
  alignas(kCacheLine) unsigned back_ = 0;                // owned by the writer
  alignas(kCacheLine) unsigned front_ = 2;               // owned by the reader
};

}  // namespace autodj
