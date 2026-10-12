#include "autodj/core/DemoSources.h"

#include <cmath>
#include <vector>

#include "autodj/core/Dsp.h"

namespace autodj::demo {

namespace {
constexpr double kTwoPi = 6.283185307179586476925286766559;
}

std::shared_ptr<const SampleBuffer> sine(double freqHz, double seconds, int sampleRate, float amp, double rightFreqHz) {
  const std::size_t n = static_cast<std::size_t>(seconds * sampleRate);
  if (rightFreqHz <= 0.0) rightFreqHz = freqHz;
  std::vector<float> d(n * 2);
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / sampleRate;
    d[2 * i] = amp * static_cast<float>(std::sin(kTwoPi * freqHz * t));
    d[2 * i + 1] = amp * static_cast<float>(std::sin(kTwoPi * rightFreqHz * t));
  }
  return SampleBuffer::fromInterleavedStereo(d.data(), n, sampleRate, "sine");
}

std::shared_ptr<const SampleBuffer> chordPad(double seconds, int sampleRate) {
  const std::size_t n = static_cast<std::size_t>(seconds * sampleRate);
  static const double kNotes[] = {110.0, 130.81, 164.81, 196.0, 220.0, 261.63, 329.63};  // A2 C3 E3 G3 A3 C4 E4
  std::vector<float> d(n * 2, 0.f);
  XorShift32 rng(1234567u);
  for (std::size_t k = 0; k < sizeof(kNotes) / sizeof(kNotes[0]); ++k) {
    const double f = kNotes[k];
    const double detune = 1.0 + 0.0021 * (static_cast<double>(k % 3) - 1.0);
    const double lfoRate = 0.05 + 0.03 * static_cast<double>(k);
    const double ph = rng.uniform() * kTwoPi;
    const float panL = 0.5f + 0.4f * std::sin(static_cast<float>(k) * 1.7f);
    const float panR = 1.0f - panL;
    for (std::size_t i = 0; i < n; ++i) {
      const double t = static_cast<double>(i) / sampleRate;
      const double env = 0.6 + 0.4 * std::sin(kTwoPi * lfoRate * t + ph);
      // a little 2nd/3rd harmonic so the pad is not a pure sine
      const double s = std::sin(kTwoPi * f * detune * t + ph) + 0.35 * std::sin(kTwoPi * 2.0 * f * t) +
                       0.12 * std::sin(kTwoPi * 3.0 * f * detune * t);
      const float v = static_cast<float>(0.07 * env * s);
      d[2 * i] += v * panL * 1.4f;
      d[2 * i + 1] += v * panR * 1.4f;
    }
  }
  return SampleBuffer::fromInterleavedStereo(d.data(), n, sampleRate, "chord-pad");
}

std::shared_ptr<const SampleBuffer> drumLoop(double bpm, int bars, int sampleRate) {
  const double beat = 60.0 / bpm;
  const double step = beat / 4.0;  // 16th
  const int steps = bars * 16;
  const std::size_t n = static_cast<std::size_t>(steps * step * sampleRate);
  std::vector<float> d(n * 2, 0.f);
  XorShift32 rng(7777u);

  auto add = [&](std::size_t start, std::size_t len, auto gen) {
    for (std::size_t i = 0; i < len && start + i < n; ++i) {
      const float v = gen(static_cast<double>(i) / sampleRate, i);
      d[2 * (start + i)] += v;
      d[2 * (start + i) + 1] += v;
    }
  };

  for (int s = 0; s < steps; ++s) {
    const std::size_t start = static_cast<std::size_t>(s * step * sampleRate);
    const int inBeat = s % 4;
    const int beatIdx = (s / 4) % 4;
    if (inBeat == 0) {  // kick on every beat
      double kickPhase = 0.0;
      add(start, static_cast<std::size_t>(0.35 * sampleRate), [&](double t, std::size_t) {
        const double f = 45.0 + 110.0 * std::exp(-t * 28.0);
        kickPhase += kTwoPi * f / sampleRate;
        return static_cast<float>(0.9 * std::sin(kickPhase) * std::exp(-t * 9.0));
      });
    }
    if (inBeat == 0 && (beatIdx == 1 || beatIdx == 3)) {  // snare on 2 and 4
      add(start, static_cast<std::size_t>(0.25 * sampleRate), [&](double t, std::size_t) {
        return static_cast<float>((0.5 * rng.bipolar() + 0.3 * std::sin(kTwoPi * 190.0 * t)) * std::exp(-t * 18.0));
      });
    }
    {  // hats: noise differentiated (poor man's high-pass)
      const bool open = (inBeat == 2);
      float prev = 0.f;
      add(start, static_cast<std::size_t>((open ? 0.12 : 0.04) * sampleRate), [&](double t, std::size_t) {
        const float nz = rng.bipolar();
        const float hp = nz - prev;
        prev = nz;
        return static_cast<float>(0.18 * hp * std::exp(-t * (open ? 25.0 : 90.0)));
      });
    }
  }
  return SampleBuffer::fromInterleavedStereo(d.data(), n, sampleRate, "drum-loop");
}

std::shared_ptr<const SampleBuffer> noise(double seconds, int sampleRate, float amp) {
  const std::size_t n = static_cast<std::size_t>(seconds * sampleRate);
  std::vector<float> d(n * 2);
  XorShift32 rng(424242u);
  for (std::size_t i = 0; i < n * 2; ++i) d[i] = amp * rng.bipolar();
  return SampleBuffer::fromInterleavedStereo(d.data(), n, sampleRate, "noise");
}

}  // namespace autodj::demo
