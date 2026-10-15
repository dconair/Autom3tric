#pragma once
// Plain-data snapshots the audio thread publishes to the UI through TripleBuffer<T>.
// Layout is kept simple (floats/ints only) so the JNI layer can copy them straight into
// Java arrays without conversion.

#include <cstdint>

namespace autodj {

inline constexpr int kVizMaxGrains = 256;
inline constexpr int kVizMaxVoices = 4;

struct EngineStatus {
  float load = 0.f;          // smoothed callback time / available time (0..1+)
  float loadPeak = 0.f;      // peak since start, slow decay
  float lastBlockLoad = 0.f; // last callback only
  float peakL = 0.f;         // output peak since last snapshot
  float peakR = 0.f;
  std::uint32_t deadlineMisses = 0;  // callbacks that took longer than their buffer duration
  std::uint32_t xruns = 0;           // underruns reported by the audio driver (Oboe getXRunCount)
  std::uint32_t droppedGrains = 0;   // spawn requests rejected because the pool/cap was full
  std::uint64_t callbacks = 0;
  double clockBeat = 0.0;
  double playBeat = 0.0;
  double bpm = 120.0;
  std::int32_t playing = 0;
  std::int32_t activeGrains = 0;
  std::int32_t activeVoices = 0;
  std::int32_t qualityLevel = 0;     // 0 = full quality, 3 = most reduced (adaptive quality)
  std::int32_t callbackFrames = 0;
  std::int32_t sampleRate = 48000;
};

struct GrainVizItem {
  float pos = 0.f;   // read position in the source, 0..1
  float size = 0.f;  // grain duration relative to 500 ms
  float pan = 0.f;   // -1..1
  float age = 0.f;   // 0..1 progress through the envelope
  float rate = 1.f;  // playback ratio (negative = reversed)
  float voice = 0.f; // voice index
};

struct GrainViz {
  std::int32_t count = 0;
  std::int32_t voices = 0;
  float playhead[kVizMaxVoices] = {};  // effective grain-start position per voice, 0..1
  float voiceLevel[kVizMaxVoices] = {};
  std::int32_t cell[kVizMaxVoices] = {-1, -1, -1, -1};  // cell the voice currently reads from (Step/Keys), -1 in Hold or on a rest
  std::int32_t step[kVizMaxVoices] = {-1, -1, -1, -1};  // step 0..15 of the voice's pattern, -1 when no pattern runs
  GrainVizItem grains[kVizMaxGrains];
};

}  // namespace autodj
