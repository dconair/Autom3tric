#pragma once
// Parameter set of the granular engine. Everything the UI, automation, MIDI and (later) the
// modulation matrix can touch is registered here; the audio code only sees ParamIds.

#include "autodj/core/Param.h"

namespace autodj::granular {

inline constexpr int kMaxVoices = 4;

// New shapes are appended: presets store the enum index, so existing indices must never move.
enum class EnvShape : int { Hann = 0, Gauss, Tukey, Trapezoid, Square, Triangle, Count };
enum class SyncMode : int { Free = 0, Beat, Count };
enum class InterpQuality : int { Linear = 0, Hermite, Count };

// "Cells" turn a source into 8 or 16 equally wide slices. Hold ignores them (position spans the whole source),
// Step walks through them locked to the tempo grid, Keys plays the cell chosen by cell_index (pad / MIDI key).
enum class CellMode : int { Hold = 0, Step, Keys, Count };
inline constexpr int kMaxCells = 16;

/// Length of one sequencer step in beats: 2 bars, 1 bar, 1/2, 1/4, 1/8, 1/16 note (4/4).
inline constexpr int kNumStepDivisions = 6;
inline double stepDivisionBeats(int index) noexcept {
  static constexpr double kBeats[kNumStepDivisions] = {8.0, 4.0, 2.0, 1.0, 0.5, 0.25};
  return kBeats[index < 0 ? 0 : (index >= kNumStepDivisions ? kNumStepDivisions - 1 : index)];
}

struct VoiceParamIds {
  ParamId gate = kInvalidParam;         // Bool: grains are produced while true (XY-pad hold, automation)
  ParamId source = kInvalidParam;       // Int : source slot (switchable live, old grains finish on the old buffer)
  ParamId position = kInvalidParam;     // 0..1 position in the source (grain start)
  ParamId sizeMs = kInvalidParam;       // 5..500 ms
  ParamId density = kInvalidParam;      // grains per second
  ParamId sprayMs = kInvalidParam;      // random position offset +-spray (ms of source time)
  ParamId jitter = kInvalidParam;       // 0..1 random timing deviation of the grain trigger (free mode)
  ParamId pitchSt = kInvalidParam;      // -24..24 semitones
  ParamId pitchRandSt = kInvalidParam;  // 0..12 semitones of random pitch per grain
  ParamId envShape = kInvalidParam;     // EnvShape
  ParamId reverseProb = kInvalidParam;  // 0..1 chance that a grain plays backwards
  ParamId spread = kInvalidParam;       // 0..1 random stereo balance per grain
  ParamId ampRand = kInvalidParam;      // 0..1 random attenuation per grain
  ParamId scanOn = kInvalidParam;       // Bool
  ParamId scanSpeed = kInvalidParam;    // -4..4 (1 = real-time speed, negative = backwards)
  ParamId loopStart = kInvalidParam;    // 0..1 scan loop region
  ParamId loopEnd = kInvalidParam;
  ParamId freeze = kInvalidParam;       // Bool: scan head stops
  ParamId syncMode = kInvalidParam;     // SyncMode
  ParamId syncDiv = kInvalidParam;      // SyncDivision (transport grid)
  ParamId quality = kInvalidParam;      // InterpQuality
  ParamId volume = kInvalidParam;       // 0..2
  ParamId releaseMs = kInvalidParam;    // fade-out of the cloud after gate/note off
  ParamId autoGain = kInvalidParam;     // Bool: 1/sqrt(overlap) compensation
  ParamId cellMode = kInvalidParam;     // CellMode: Hold / Step / Keys
  ParamId cellCount = kInvalidParam;    // Enum: 8 or 16 cells
  ParamId cellIndex = kInvalidParam;    // Int 0..15: cell played in Keys mode
  ParamId stepDiv = kInvalidParam;      // Enum: step length (stepDivisionBeats)
  ParamId seqOn = kInvalidParam;        // Bool: in Step mode the 16-step pattern (GranularSteps) decides which cell plays, and when
};

struct GranularParamIds {
  int numVoices = 0;
  VoiceParamIds voice[kMaxVoices];
  ParamId adaptive = kInvalidParam;     // Bool: adaptive quality on/off
  ParamId stealFadeMs = kInvalidParam;  // 1..10 ms fade used when a voice/grain set is stolen
  ParamId maxVoices = kInvalidParam;    // 1..numVoices user limit
  ParamId grainCap = kInvalidParam;     // 16..512 global grain limit
  ParamId chaos = kInvalidParam;        // 0..1 reverb -> flanger macro behind the grains (0 = exact bypass)
  ParamId outputDb = kInvalidParam;     // -24..+6 dB level of the granular instrument
};

/// Registers all granular parameters ("granular.v0.size_ms", ...). Call before the registry is frozen.
GranularParamIds registerGranularParams(ParamRegistry& reg, int numVoices);

}  // namespace autodj::granular
