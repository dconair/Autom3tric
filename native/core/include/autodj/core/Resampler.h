#pragma once
// Offline sample-rate conversion for decoded tracks (control/worker thread, allocates).
// Kaiser-windowed sinc (16 zero crossings each side, beta 9, about -90 dB stop band), exact polyphase for the rational
// ratio: no accumulated phase error however long the track is.

#include <cstddef>
#include <vector>

namespace autodj {

/// `in` holds `frames` frames of `channels` interleaved channels at `srcRate`; returns interleaved audio at `dstRate`
/// (floor(frames * dstRate / srcRate) frames). Equal rates return a copy.
std::vector<float> resampleInterleaved(const float* in, std::size_t frames, int channels, int srcRate, int dstRate);

}  // namespace autodj
