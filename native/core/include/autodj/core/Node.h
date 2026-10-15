#pragma once
// Uniform processing-node interface + the immutable graph description the audio thread runs.
//
// Lifecycle
//   prepare()    control thread, may allocate. Called before the node is part of a published graph.
//   setParams()  audio thread, once per block, before process(): read atomics, smooth, run the
//                control-rate modulation. Never per sample.
//   process()    audio thread. Real-time rules apply (see docs/AUDIO_THREAD_RULES.md).
//   reset()      audio thread or control thread while the audio thread is not running the node.
//
// A GraphDesc is built on the control thread, never modified afterwards, and handed over with a
// single pointer swap (SwapGraph command). The previous description comes back through the
// garbage queue. Node instances themselves are owned by the engine; a graph only references them.

#include <array>
#include <cstdint>

#include "autodj/core/Command.h"
#include "autodj/core/MasterClock.h"
#include "autodj/core/Param.h"
#include "autodj/core/Transport.h"

namespace autodj {

struct ProcessSpec {
  double sampleRate = 48000.0;
  int maxBlockFrames = 192;
};

struct ProcessIO {
  const BlockTime* time = nullptr;
  const MasterClock* clock = nullptr;  // where the beat grid of the music is in this block (see MasterClock.h)
  const float* in = nullptr;  // interleaved stereo, null for pure sources
  float* out = nullptr;       // interleaved stereo; nodes ADD into it (the graph clears it per block)
  int frames = 0;
  GarbageQueue* garbage = nullptr;
};

class Node {
 public:
  virtual ~Node() = default;

  virtual void prepare(const ProcessSpec& spec) = 0;
  virtual void reset() noexcept = 0;
  virtual void setParams(const ParamStore& params, const BlockTime& time) noexcept = 0;
  virtual void process(ProcessIO& io) noexcept = 0;

  /// Audio thread. Return true if the command was consumed.
  virtual bool handleCommand(const Command& cmd, GarbageQueue& garbage) noexcept {
    (void)cmd;
    (void)garbage;
    return false;
  }
};

struct GraphDesc {
  static constexpr int kMaxNodes = 16;
  struct Entry {
    Node* node = nullptr;
    float gain = 1.f;
  };
  std::array<Entry, kMaxNodes> entries{};
  int count = 0;
  std::uint32_t version = 0;

  bool add(Node* n, float gain = 1.f) {
    if (count >= kMaxNodes || n == nullptr) return false;
    entries[count++] = {n, gain};
    return true;
  }
};

}  // namespace autodj
