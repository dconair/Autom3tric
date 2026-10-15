#pragma once
// Decoded, analysed tracks. A Track is immutable once built; the audio thread only ever sees a raw pointer to one
// (kept alive by the engine until the audio thread returns it through the garbage queue, like every other shared object).

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "autodj/analysis/Analysis.h"
#include "autodj/analysis/PeakPyramid.h"
#include "autodj/core/SampleBuffer.h"

namespace autodj {

struct Track {
  int id = 0;
  std::string name;
  std::shared_ptr<const SampleBuffer> audio;  // stereo float at the engine rate
  analysis::TrackAnalysis analysis;
  analysis::PeakPyramid peaks;

  double durationSec() const { return audio ? static_cast<double>(audio->frames()) / audio->sampleRate() : 0.0; }
};

/// Collects decoded PCM chunks (any channel count, any rate) on a worker thread and turns them into one engine-rate
/// stereo SampleBuffer: mono is duplicated, surround is folded to the front pair plus centre, then the rate is converted.
class TrackIngest {
 public:
  TrackIngest(int srcRate, int channels);
  /// `sampleCount` interleaved samples (frames * channels); a trailing partial frame is dropped.
  void append(const float* interleaved, std::size_t sampleCount);
  std::size_t frames() const { return channels_ > 0 ? data_.size() / static_cast<std::size_t>(channels_) : 0; }
  std::shared_ptr<const SampleBuffer> finish(int engineRate, std::string name);

 private:
  int srcRate_;
  int channels_;
  std::vector<float> data_;
};

/// Quantised waveform overview for the library list: `columns` min/max pairs scaled to -127..127.
void trackOverview(const Track& track, int columns, std::vector<int>& mins, std::vector<int>& maxs);

}  // namespace autodj
