#pragma once
// Deterministic synthetic audio used by tests, benchmarks and the demo UI (no asset files, no
// decoder needed before milestone 3). Control-thread only: allocates.

#include <memory>

#include "autodj/core/SampleBuffer.h"

namespace autodj::demo {

/// Stereo sine, optionally with a slightly different frequency on the right channel.
std::shared_ptr<const SampleBuffer> sine(double freqHz, double seconds, int sampleRate, float amp = 0.5f,
                                          double rightFreqHz = 0.0);

/// Slowly evolving detuned chord pad (A minor 7). Good for smooth, tonal granular clouds.
std::shared_ptr<const SampleBuffer> chordPad(double seconds, int sampleRate);

/// Four-on-the-floor kick, snare on 2/4, 16th hats. Good for rhythmic / beat-sync material.
std::shared_ptr<const SampleBuffer> drumLoop(double bpm, int bars, int sampleRate);

/// White noise (fixed seed) at the given peak amplitude.
std::shared_ptr<const SampleBuffer> noise(double seconds, int sampleRate, float amp = 0.3f);

}  // namespace autodj::demo
