#pragma once
// Offline track analysis (control thread / worker thread only, allocates freely).
//
// Input is the decoded track as interleaved stereo float at the engine rate. Output: tempo and a constant
// beat grid (with the downbeat), musical key (Camelot), loudness, per-bar energy, a coarse structure
// (intro / body / drop / break / outro) and the points where an automatic mix should enter and leave.
//
// Everything is heuristic and works best on rhythmic, constant-tempo music (dance, pop, rock with a click or
// a steady drummer). Live music with tempo drift gets a low gridConfidence, which the UI shows.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace autodj::analysis {

struct Section {
  double startSec = 0.0;
  double endSec = 0.0;
  std::string label;  // "intro", "body", "drop", "break", "outro"
};

struct TrackAnalysis {
  int version = 1;
  double durationSec = 0.0;

  // Tempo and grid: beat k is at gridOffsetSec + k * beatSec. k = 0 is a downbeat (first beat of a bar).
  double bpm = 0.0;  // 0 = no reliable tempo found
  double beatSec = 0.0;
  double gridOffsetSec = 0.0;
  double tempoConfidence = 0.0;     // 0..1, strength of the periodicity
  double gridConfidence = 0.0;      // 0..1, share of beats that sit on a detected onset
  double downbeatConfidence = 0.0;  // 0..1, how clearly one of the four beats stands out

  int keyTonic = -1;  // 0 = C ... 11 = B, -1 = unknown
  bool keyMinor = false;
  double keyConfidence = 0.0;
  std::string keyName;  // "A minor"
  std::string camelot;  // "8A"

  double rmsDb = -120.0;  // over the non-silent part
  double peakDb = -120.0;
  double trimDb = 0.0;  // suggested gain to bring tracks to a common level in a mix

  double barSec = 0.0;  // 4 beats (or 4 s when no tempo was found)
  double firstBarSec = 0.0;  // start of bar 0 (a downbeat, or 0)
  std::vector<float> barEnergy;  // 0..1 per bar, relative to the loud part of the track
  std::vector<Section> sections;
  double mixInSec = 0.0;   // where an incoming mix should start playing this track (a downbeat)
  double mixOutSec = 0.0;  // where the outgoing transition should begin at the latest (a phrase boundary)

  bool hasTempo() const { return bpm > 0.0; }
};

struct AnalysisOptions {
  double minBpm = 75.0;
  double maxBpm = 185.0;
};

using ProgressFn = std::function<void(float)>;

TrackAnalysis analyzeTrack(const float* interleavedStereo, std::int64_t frames, int sampleRate,
                           const AnalysisOptions& options = {}, const ProgressFn& progress = {});

/// "A minor", "C major"; empty if tonic < 0.
std::string keyName(int tonic, bool minor);
/// "8A" / "8B"; empty if tonic < 0.
std::string camelotName(int tonic, bool minor);
/// Camelot compatibility: same number, +-1 number with the same letter, or the same number with the other letter.
bool camelotCompatible(const std::string& a, const std::string& b);

}  // namespace autodj::analysis
