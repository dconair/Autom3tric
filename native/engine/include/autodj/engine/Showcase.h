#pragma once
// The four sounds the granular instrument starts with: a pad, a drum loop, a plucked melody and a bass line. They are
// synthesised (no sample files, so they are identical on every platform and need no licence), band-limited (no aliasing),
// seamless loops of four bars at 120 bpm over the chords A minor - F - C - G, and levelled to the same loudness, so that
// the first thing a new user hears is clean and at a sensible volume.

#include <memory>

#include "autodj/core/SampleBuffer.h"

namespace autodj::showcase {

inline constexpr int kKinds = 4;
inline constexpr double kBpm = 120.0;
inline constexpr int kBars = 4;

/// Short names, in kind order: "pad", "drums", "pluck", "bass".
const char* name(int kind) noexcept;

/// kind 0..3; anything else wraps. `sampleRate` is the engine rate. Control thread (renders about 8 s of audio).
std::shared_ptr<const SampleBuffer> make(int kind, int sampleRate);

}  // namespace autodj::showcase
