#pragma once
// The drum pads as a graph node: sixteen one-shot voices of the PadKit, played by hand (PadTrigger command) or by a 16-step
// sequencer that runs on the MasterClock, so the pattern sits on the beat grid of the music that is playing (deck, mix plan)
// and follows its tempo changes. Step 0 is a bar line of that grid.
//
//   audio thread: the pattern (16 pads x 16 steps) is state of this node, edited with PadRow commands.
//   Swing delays every odd 16th by up to 30 % of a step. Hats are a choke group: a closed hat cuts the open one.

#include <atomic>
#include <cstdint>

#include "autodj/core/Dsp.h"
#include "autodj/core/MasterClock.h"
#include "autodj/core/Node.h"
#include "autodj/perf/PadKit.h"
#include "autodj/perf/PerfParams.h"

namespace autodj::perf {

class PadNode final : public Node {
 public:
  explicit PadNode(const PerfParamIds& ids) : ids_(ids) {}

  void prepare(const ProcessSpec& spec) override;
  void reset() noexcept override;
  void setParams(const ParamStore& st, const BlockTime& t) noexcept override;
  void process(ProcessIO& io) noexcept override;
  bool handleCommand(const Command& cmd, GarbageQueue& garbage) noexcept override;

  /// Audio thread, after process(): the step the sequencer is on (0..15), -1 while it is off.
  int currentStep() const noexcept { return stepShown_; }
  int activeVoices() const noexcept;
  unsigned row(int pad) const noexcept { return rows_[pad & 15]; }

  static constexpr int kVoices = 24;

 private:
  struct Voice {
    int pad = -1;           // -1 = free
    std::size_t pos = 0;
    int delay = 0;          // frames until the sound starts (sequencer hits inside the chunk)
    float vel = 1.f;
    float fade = 1.f;
    bool releasing = false;
    std::uint64_t age = 0;
  };

  void trigger(int pad, float vel, int delay) noexcept;
  void runSequencer(const MasterClock& clk, int frames) noexcept;

  PerfParamIds ids_;
  double sr_ = 48000.0;
  PadKit kit_;
  Voice voices_[kVoices];
  std::uint64_t ageCounter_ = 0;
  unsigned rows_[kPads] = {};

  float volume_ = 0.9f;
  BlockSmoother volSmooth_;
  float volPrev_ = 0.f;
  bool seqOn_ = false, seqWasOn_ = false;
  float swing_ = 0.f;
  long long nextK_ = 0;
  double expectBeat_ = 0.0;
  int stepShown_ = -1;
  float releaseStep_ = 0.01f;
};

}  // namespace autodj::perf
