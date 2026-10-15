#pragma once
// Preallocated grain storage, struct-of-arrays, stable slots.
//
//  - allocate() pops a slot from the freelist (O(1), no search) and appends it to the dense
//    "active" list; recycle() swap-removes it from that list (O(1)) and pushes the slot back.
//  - Per-grain fields live in parallel arrays indexed by slot, so one pass over the dense
//    active list touches hot data only; nothing is ever allocated after construction.
//  - Slots are stable for the lifetime of a grain (visualisation, voice stealing and tests can
//    refer to them), the active list is what the audio loop iterates.

#include <cstdint>

#include "autodj/core/Platform.h"

namespace autodj::granular {

class GrainPool {
 public:
  static constexpr int kCapacity = 512;

  GrainPool() noexcept { clear(); }

  void clear() noexcept {
    for (int i = 0; i < kCapacity; ++i) freeList_[i] = static_cast<std::uint16_t>(kCapacity - 1 - i);
    freeTop_ = kCapacity;
    activeCount_ = 0;
  }

  /// Returns a slot or -1 when the pool is exhausted.
  int allocate() noexcept {
    if (freeTop_ == 0) return -1;
    const int slot = freeList_[--freeTop_];
    activePos_[slot] = static_cast<std::uint16_t>(activeCount_);
    active_[activeCount_++] = static_cast<std::uint16_t>(slot);
    return slot;
  }

  void recycle(int slot) noexcept {
    const int idx = activePos_[slot];
    const int last = active_[--activeCount_];
    active_[idx] = static_cast<std::uint16_t>(last);
    activePos_[last] = static_cast<std::uint16_t>(idx);
    freeList_[freeTop_++] = static_cast<std::uint16_t>(slot);
  }

  int activeCount() const noexcept { return activeCount_; }
  int freeCount() const noexcept { return freeTop_; }
  int activeSlot(int i) const noexcept { return active_[i]; }

  /// Test hook: every slot is either free or active, exactly once.
  bool checkInvariants() const noexcept {
    if (activeCount_ + freeTop_ != kCapacity) return false;
    bool seen[kCapacity] = {};
    for (int i = 0; i < activeCount_; ++i) {
      const int s = active_[i];
      if (s < 0 || s >= kCapacity || seen[s] || activePos_[s] != i) return false;
      seen[s] = true;
    }
    for (int i = 0; i < freeTop_; ++i) {
      const int s = freeList_[i];
      if (s < 0 || s >= kCapacity || seen[s]) return false;
      seen[s] = true;
    }
    return true;
  }

  // ---- per-grain data, indexed by slot --------------------------------------------------------
  alignas(kCacheLine) double pos[kCapacity];  // source position (frames) of the next sample to render
  float rate[kCapacity];                      // source frames per output sample, negative = reversed
  float envPos[kCapacity];                    // envelope LUT position of the next sample
  float envStep[kCapacity];                   // LUT entries per output sample
  float gainL[kCapacity];
  float gainR[kCapacity];
  std::int32_t remaining[kCapacity];          // output samples still to render
  std::int32_t length[kCapacity];             // total length (for visualisation)
  std::int32_t startOffset[kCapacity];        // sample offset inside the block of the first render
  std::int32_t fadeLeft[kCapacity];           // > 0: grain is being stolen and fades out over this many samples
  float fadeGain[kCapacity];
  float fadeStep[kCapacity];
  std::uint8_t voice[kCapacity];
  std::uint8_t shape[kCapacity];
  std::uint8_t srcEntry[kCapacity];           // SourceTable entry the grain reads from

 private:
  std::uint16_t freeList_[kCapacity];
  std::uint16_t active_[kCapacity];
  std::uint16_t activePos_[kCapacity];
  int freeTop_ = 0;
  int activeCount_ = 0;
};

}  // namespace autodj::granular
