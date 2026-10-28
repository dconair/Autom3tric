#pragma once
// Pre-computed min/max pyramid of a track for the zoomable waveform (spec: "vorberechnete Peak-Pyramide").
// Level 0 holds one min/max pair per kBlock frames, each further level 4x coarser. A query picks the coarsest level
// that still has at least one pair per output column and falls back to the raw samples when zoomed in further.

#include <cstdint>
#include <vector>

#include "autodj/core/SampleBuffer.h"

namespace autodj::analysis {

class PeakPyramid {
 public:
  static constexpr int kBlock = 64;
  static constexpr int kFactor = 4;

  PeakPyramid() = default;
  explicit PeakPyramid(const SampleBuffer& buffer);

  /// Fills out[2*columns] = (min, max) of the mono mix for the frame range [startFrame, endFrame). Frames outside the
  /// track give (0, 0). `buffer` is the track the pyramid was built from (used when zoomed in below one block).
  void query(const SampleBuffer& buffer, std::int64_t startFrame, std::int64_t endFrame, int columns, float* out) const;

  std::size_t levels() const { return levels_.size(); }

 private:
  std::int64_t frames_ = 0;
  std::vector<std::vector<float>> levels_;  // each: (min, max) pairs
};

}  // namespace autodj::analysis
