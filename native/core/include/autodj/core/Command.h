#pragma once
// Fixed-size, trivially copyable messages that cross thread boundaries through SPSC queues.
//
//   control thread  --Command-->  audio thread      (everything that is not a plain parameter value)
//   audio thread    --GarbageMsg-> control thread    (objects the audio thread is done with; freed off-thread)
//
// Continuous values (knobs, sliders, XY pads) do NOT go through commands, they are atomic
// parameters (see Param.h). Commands carry events: transport actions, note on/off,
// "here is a new immutable object, please start using it".

#include <cstdint>
#include <cstring>

#include "autodj/core/SpscQueue.h"

namespace autodj {

enum class CmdType : std::uint8_t {
  None = 0,
  Play,           // start the timeline playhead (aligns the beat clock to it)
  Stop,           // stop the timeline playhead
  Seek,           // payload.d = target position in beats
  PublishSource,  // slot = source slot, payload.ptr = const SampleBuffer* (kept alive by the control side)
  NoteOn,         // target = granular voice, f0 = pitch offset in semitones, f1 = velocity 0..1
  NoteOff,        // target = granular voice
  SwapGraph,      // payload.ptr = const GraphDesc*
  ResetEngine,    // clears voices, grains, transport phase
  StepReset,      // target = granular voice: restart its step sequence at cell 0 (aligned to the next grid step)
  DeckLoad,       // target = deck 0/1, payload.ptr = const Track* (null = unload), f0 = start position in seconds
  DeckPlay,       // target = deck, i0 = 1 play from the current position, 0 pause
  DeckSeek,       // target = deck, payload.d = seconds
  PlanStart,      // payload.ptr = const MixPlan* (kept alive by the control side until the audio thread returns it)
  PlanStop,       // stop the running mix plan (decks it started are cleared)
  PadTrigger,     // target = pad 0..15, f0 = velocity (0 = full)
  PadRow,         // target = pad 0..15, i0 = 16-bit step mask of its sequencer row
};

struct Command {
  CmdType type = CmdType::None;
  std::uint8_t target = 0;
  std::uint16_t slot = 0;
  std::int32_t i0 = 0;
  float f0 = 0.f;
  float f1 = 0.f;
  union Payload {
    double d;
    const void* ptr;
    std::uint64_t u;
  } payload{};

  static Command make(CmdType t) {
    Command c;
    c.type = t;
    return c;
  }
};
static_assert(sizeof(Command) <= 32, "keep commands small: they are copied through the queue");

struct GarbageMsg {
  enum class Kind : std::uint8_t { SampleBuffer, Graph, Track, Plan };
  Kind kind = Kind::SampleBuffer;
  const void* ptr = nullptr;
};

inline constexpr std::size_t kCommandQueueSize = 1024;
inline constexpr std::size_t kGarbageQueueSize = 256;

using CommandQueue = SpscQueue<Command, kCommandQueueSize>;
using GarbageQueue = SpscQueue<GarbageMsg, kGarbageQueueSize>;

}  // namespace autodj
