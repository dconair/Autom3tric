#include "autodj/engine/Tracks.h"

#include <algorithm>
#include <cmath>

#include "autodj/core/Resampler.h"

namespace autodj {

TrackIngest::TrackIngest(int srcRate, int channels) : srcRate_(srcRate), channels_(std::max(1, channels)) {}

void TrackIngest::append(const float* interleaved, std::size_t sampleCount) {
  if (interleaved == nullptr || sampleCount == 0) return;
  const std::size_t whole = sampleCount - sampleCount % static_cast<std::size_t>(channels_);
  data_.insert(data_.end(), interleaved, interleaved + whole);
}

std::shared_ptr<const SampleBuffer> TrackIngest::finish(int engineRate, std::string name) {
  const std::size_t n = frames();
  if (n == 0 || srcRate_ <= 0 || engineRate <= 0) return nullptr;
  std::vector<float> stereo;
  const float* src = data_.data();
  if (channels_ != 2) {
    stereo.resize(n * 2);
    const std::size_t c = static_cast<std::size_t>(channels_);
    for (std::size_t i = 0; i < n; ++i) {
      const float* f = data_.data() + i * c;
      if (c == 1) {
        stereo[2 * i] = stereo[2 * i + 1] = f[0];
      } else {
        const float centre = c >= 3 ? 0.7071f * f[2] : 0.f;
        stereo[2 * i] = (f[0] + centre) * (c >= 3 ? 0.75f : 1.f);
        stereo[2 * i + 1] = (f[1] + centre) * (c >= 3 ? 0.75f : 1.f);
      }
    }
    src = stereo.data();
  }
  if (srcRate_ == engineRate) return SampleBuffer::fromInterleavedStereo(src, n, engineRate, std::move(name));
  const std::vector<float> rs = resampleInterleaved(src, n, 2, srcRate_, engineRate);
  return SampleBuffer::fromInterleavedStereo(rs.data(), rs.size() / 2, engineRate, std::move(name));
}

void trackOverview(const Track& track, int columns, std::vector<int>& mins, std::vector<int>& maxs) {
  mins.assign(static_cast<std::size_t>(std::max(0, columns)), 0);
  maxs.assign(mins.size(), 0);
  if (!track.audio || columns <= 0) return;
  std::vector<float> pk(static_cast<std::size_t>(columns) * 2);
  track.peaks.query(*track.audio, 0, track.audio->frames(), columns, pk.data());
  for (int c = 0; c < columns; ++c) {
    mins[static_cast<std::size_t>(c)] = static_cast<int>(std::lround(std::clamp(pk[2 * c], -1.f, 1.f) * 127.f));
    maxs[static_cast<std::size_t>(c)] = static_cast<int>(std::lround(std::clamp(pk[2 * c + 1], -1.f, 1.f) * 127.f));
  }
}

}  // namespace autodj
