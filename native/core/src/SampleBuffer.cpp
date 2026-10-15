#include "autodj/core/SampleBuffer.h"

#include <cstring>

namespace autodj {

SampleBuffer::SampleBuffer(std::size_t frames, int sampleRate, std::string name)
    : frames_(static_cast<std::int64_t>(frames)), sampleRate_(sampleRate), name_(std::move(name)) {
  const std::size_t total = (frames + 2 * kGuardFrames) * 2;
  data_.reset(static_cast<float*>(::operator new[](total * sizeof(float), std::align_val_t{64})));
  std::memset(data_.get(), 0, total * sizeof(float));
}

std::shared_ptr<const SampleBuffer> SampleBuffer::fromInterleavedStereo(const float* lr, std::size_t frames,
                                                                         int sampleRate, std::string name) {
  std::shared_ptr<SampleBuffer> b(new SampleBuffer(frames, sampleRate, std::move(name)));
  if (frames > 0) std::memcpy(b->mutableData(), lr, frames * 2 * sizeof(float));
  return b;
}

std::shared_ptr<const SampleBuffer> SampleBuffer::fromPlanar(const float* l, const float* r, std::size_t frames,
                                                              int sampleRate, std::string name) {
  std::shared_ptr<SampleBuffer> b(new SampleBuffer(frames, sampleRate, std::move(name)));
  float* d = b->mutableData();
  for (std::size_t i = 0; i < frames; ++i) {
    d[2 * i] = l[i];
    d[2 * i + 1] = r ? r[i] : l[i];
  }
  return b;
}

void computePeaks(const SampleBuffer& buffer, int columns, float* out) noexcept {
  if (columns <= 0 || out == nullptr) return;
  const std::int64_t frames = buffer.frames();
  const float* d = buffer.data();
  for (int c = 0; c < columns; ++c) {
    std::int64_t a = frames * c / columns;
    std::int64_t b = frames * (c + 1) / columns;
    if (b <= a) b = a + 1;  // fewer frames than columns: every column still looks at one frame
    if (a >= frames) a = frames > 0 ? frames - 1 : 0;
    if (b > frames) b = frames;
    float lo = 0.f, hi = 0.f;
    if (frames > 0) {
      lo = hi = 0.5f * (d[2 * a] + d[2 * a + 1]);
      for (std::int64_t i = a + 1; i < b; ++i) {
        const float m = 0.5f * (d[2 * i] + d[2 * i + 1]);
        lo = m < lo ? m : lo;
        hi = m > hi ? m : hi;
      }
    }
    out[2 * c] = lo;
    out[2 * c + 1] = hi;
  }
}

}  // namespace autodj
