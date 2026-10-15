#pragma once
// Audio-thread view of the sample buffers that are currently in use.
//
// The control side publishes a buffer into a numbered *slot* (PublishSource command). Grains
// that are already playing must keep reading the buffer they started with, even if the slot is
// re-assigned or switched live, so the table tracks:
//
//   entry : one live buffer pointer + a reference count (one per playing grain)
//   slot  : which entry new grains should use
//
// When an entry is no longer mapped by any slot and its reference count drops to zero, the
// pointer is handed back to the control thread through the garbage queue, which drops the
// shared_ptr there - the audio thread never frees anything.
//
// All methods are audio-thread only. No allocation, no locks.

#include <array>
#include <cstdint>

#include "autodj/core/Command.h"
#include "autodj/core/SampleBuffer.h"

namespace autodj {

class SourceTable {
 public:
  static constexpr int kSlots = 16;
  static constexpr int kEntries = 32;

  SourceTable() noexcept {
    slotToEntry_.fill(-1);
  }

  /// Maps `slot` to `buf` (nullptr unmaps). Returns false if the table is full; in that case the
  /// buffer is returned to the control side immediately so it is not leaked.
  bool publish(int slot, const SampleBuffer* buf, GarbageQueue& garbage) noexcept {
    if (slot < 0 || slot >= kSlots) {
      retire(buf, garbage);
      return false;
    }
    int fresh = -1;
    if (buf != nullptr) {
      for (int i = 0; i < kEntries; ++i) {
        if (entries_[i].buf == nullptr) {
          fresh = i;
          break;
        }
      }
      if (fresh < 0) {
        retire(buf, garbage);
        return false;
      }
      entries_[fresh] = {buf, 0, true, false};
    }
    const int old = slotToEntry_[slot];
    slotToEntry_[slot] = fresh;
    if (old >= 0) {
      entries_[old].mapped = false;
      reclaim(old, garbage);
    }
    return true;
  }

  int entryOfSlot(int slot) const noexcept {
    return (slot >= 0 && slot < kSlots) ? slotToEntry_[slot] : -1;
  }

  const SampleBuffer* buffer(int entry) const noexcept { return entries_[entry].buf; }

  void acquire(int entry) noexcept { ++entries_[entry].refs; }

  void release(int entry, GarbageQueue& garbage) noexcept {
    if (--entries_[entry].refs <= 0) {
      entries_[entry].refs = 0;
      reclaim(entry, garbage);
    }
  }

  /// Retries garbage pushes that failed because the queue was full. Call once per block.
  void flushRetries(GarbageQueue& garbage) noexcept {
    if (!retryPending_) return;
    retryPending_ = false;
    for (int i = 0; i < kEntries; ++i) {
      if (entries_[i].buf && entries_[i].retire) reclaim(i, garbage);
    }
  }

  int liveEntries() const noexcept {
    int n = 0;
    for (const auto& e : entries_) n += e.buf != nullptr;
    return n;
  }

 private:
  struct Entry {
    const SampleBuffer* buf = nullptr;
    std::int32_t refs = 0;
    bool mapped = false;
    bool retire = false;  // wanted to retire but the garbage queue was full
  };

  void reclaim(int i, GarbageQueue& garbage) noexcept {
    Entry& e = entries_[i];
    if (e.mapped || e.refs > 0 || e.buf == nullptr) return;
    GarbageMsg m;
    m.kind = GarbageMsg::Kind::SampleBuffer;
    m.ptr = e.buf;
    if (garbage.tryPush(m)) {
      e = Entry{};
    } else {
      e.retire = true;
      retryPending_ = true;
    }
  }

  void retire(const SampleBuffer* buf, GarbageQueue& garbage) noexcept {
    if (!buf) return;
    GarbageMsg m;
    m.kind = GarbageMsg::Kind::SampleBuffer;
    m.ptr = buf;
    (void)garbage.tryPush(m);  // queue full: buffer stays owned by the control side's map until shutdown
  }

  std::array<Entry, kEntries> entries_{};
  std::array<std::int32_t, kSlots> slotToEntry_{};
  bool retryPending_ = false;
};

}  // namespace autodj
