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

/// A short structured dance track: kick on every beat, clap on 2/4, off-beat hats, bass line in the key, pad chord.
/// Layout in bars: intro (kick + hats, quieter), drop, break (pad only), drop, outro (kick fading). `startOffsetSec`
/// shifts everything later (leading silence), which tests the beat-grid offset. Used by the analysis tests and as the
/// built-in demo tracks of the app.
struct MusicSpec {
  double bpm = 124.0;
  int rootMidi = 57;  // A3
  bool minor = true;
  int sampleRate = 48000;
  int introBars = 8;
  int dropBars = 16;
  int breakBars = 8;
  int outroBars = 8;
  double startOffsetSec = 0.0;
  float level = 0.6f;
  unsigned seed = 1u;
  const char* name = "demo-track";
};
std::shared_ptr<const SampleBuffer> musicTrack(const MusicSpec& spec);

/// White noise (fixed seed) at the given peak amplitude.
std::shared_ptr<const SampleBuffer> noise(double seconds, int sampleRate, float amp = 0.3f);

}  // namespace autodj::demo
