#pragma once
// Immutable decoded audio, shared between the control side (owner) and the audio thread (reader).
//
//  - always interleaved stereo float at the engine sample rate (mono input is duplicated)
//  - kGuardFrames zeroed frames before frame 0 and after the last frame, so interpolation kernels
//    (linear: i..i+1, Hermite: i-1..i+2) may read slightly past the ends without bounds checks
//  - 64-byte aligned
//
// Ownership: std::shared_ptr on the control side only. The audio thread holds raw pointers that
// stay valid until it hands them back through the garbage queue (see SourceTable).

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>

namespace autodj {

class SampleBuffer {
 public:
  static constexpr int kGuardFrames = 8;

  static std::shared_ptr<const SampleBuffer> fromInterleavedStereo(const float* lr, std::size_t frames,
                                                                    int sampleRate, std::string name = {});
  /// `r` may be null (mono source, duplicated into both channels).
  static std::shared_ptr<const SampleBuffer> fromPlanar(const float* l, const float* r, std::size_t frames,
                                                         int sampleRate, std::string name = {});

  std::int64_t frames() const noexcept { return frames_; }
  int sampleRate() const noexcept { return sampleRate_; }
  const std::string& name() const noexcept { return name_; }

  /// Pointer to frame 0 (L,R interleaved). Valid for indices [-kGuardFrames, frames()+kGuardFrames).
  const float* data() const noexcept { return data_.get() + 2 * kGuardFrames; }

 private:
  struct Deleter {
    void operator()(float* p) const noexcept { ::operator delete[](p, std::align_val_t{64}); }
  };

  SampleBuffer(std::size_t frames, int sampleRate, std::string name);

  std::unique_ptr<float[], Deleter> data_;
  std::int64_t frames_;
  int sampleRate_;
  std::string name_;

  float* mutableData() noexcept { return data_.get() + 2 * kGuardFrames; }
};

/// Waveform overview for the UI: per column the min and max of the mono mix (L+R)/2 over that column's frames.
/// out holds 2 * columns floats (min0, max0, min1, max1, ...). Control-thread helper, reads the immutable buffer.
void computePeaks(const SampleBuffer& buffer, int columns, float* out) noexcept;

}  // namespace autodj
