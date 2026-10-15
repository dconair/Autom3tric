#include "autodj/core/DemoSources.h"

#include <algorithm>
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

namespace autodj::demo {

namespace {

double midiToHz(double m) { return 440.0 * std::pow(2.0, (m - 69.0) / 12.0); }

}  // namespace

std::shared_ptr<const SampleBuffer> musicTrack(const MusicSpec& spec) {
  const int sr = spec.sampleRate;
  const double beat = 60.0 / spec.bpm;
  const double bar = 4.0 * beat;
  const int totalBars = spec.introBars + spec.dropBars + spec.breakBars + spec.dropBars + spec.outroBars;
  const double lead = spec.startOffsetSec;
  const std::size_t n = static_cast<std::size_t>((lead + totalBars * bar) * sr) + static_cast<std::size_t>(sr / 2);
  std::vector<float> d(n * 2, 0.f);
  XorShift32 rng(spec.seed * 2654435761u + 17u);

  enum class Part { Intro, Drop, Break, Outro };
  auto partOf = [&](int b) {
    if (b < spec.introBars) return Part::Intro;
    b -= spec.introBars;
    if (b < spec.dropBars) return Part::Drop;
    b -= spec.dropBars;
    if (b < spec.breakBars) return Part::Break;
    b -= spec.breakBars;
    if (b < spec.dropBars) return Part::Drop;
    return Part::Outro;
  };
  auto put = [&](double t0, double len, auto gen) {
    const std::size_t a = static_cast<std::size_t>((lead + t0) * sr);
    const std::size_t l = static_cast<std::size_t>(len * sr);
    for (std::size_t i = 0; i < l && a + i < n; ++i) {
      const float v = gen(static_cast<double>(i) / sr, i);
      d[2 * (a + i)] += v;
      d[2 * (a + i) + 1] += v;
    }
  };

  const double root = spec.rootMidi;
  const double third = spec.minor ? 3.0 : 4.0;
  for (int b = 0; b < totalBars; ++b) {
    const Part part = partOf(b);
    const double t0 = b * bar;
    const bool outroFade = part == Part::Outro;
    const double outroGain = outroFade ? std::max(0.1, 0.6 - 0.5 * (b - (totalBars - spec.outroBars)) / std::max(1, spec.outroBars)) : 1.0;
    for (int k = 0; k < 4; ++k) {
      const double tb = t0 + k * beat;
      // Kick: pitch-dropping sine with a click; beat 1 of the bar is accented.
      if (part != Part::Break) {
        const double kickGain = (part == Part::Intro ? 0.55 : 0.9) * outroGain * (k == 0 ? 1.0 : 0.72);
        double phase = 0.0;
        put(tb, 0.30, [&](double t, std::size_t) {
          const double f = 48.0 + 160.0 * std::exp(-t / 0.028);
          phase += 2.0 * M_PI * f / sr;
          const double att = std::min(1.0, t / 0.001);
          return static_cast<float>(kickGain * att * std::exp(-t / 0.11) * std::sin(phase));
        });
      }
      // Clap on 2 and 4 in the drops.
      if (part == Part::Drop && (k == 1 || k == 3)) {
        put(tb, 0.12, [&](double t, std::size_t) { return static_cast<float>(0.35 * std::exp(-t / 0.035) * rng.bipolar()); });
      }
      // Off-beat hat (and a quiet one on the beat in the drop).
      if (part != Part::Break || k % 2 == 0) {
        const double hatGain = part == Part::Intro ? 0.12 : (part == Part::Break ? 0.06 : 0.2);
        float prevN = 0.f;
        put(tb + beat * 0.5, 0.05, [&](double t, std::size_t) {
          const float nz = rng.bipolar();
          const float hp = nz - prevN;  // crude high-pass
          prevN = nz;
          return static_cast<float>(hatGain * outroGain * std::exp(-t / 0.012) * hp);
        });
      }
      // Bass: eighth notes on the root (beat 1) and the fifth, drops only.
      if (part == Part::Drop) {
        for (int h = 0; h < 2; ++h) {
          const double tn = tb + h * beat * 0.5;
          const double note = root - 24.0 + ((k + h) % 4 == 3 ? 7.0 : 0.0);
          const double f = midiToHz(note);
          put(tn, beat * 0.45, [&](double t, std::size_t) {
            const double env = std::exp(-t / 0.18) * std::min(1.0, t / 0.004);
            const double s = std::sin(2.0 * M_PI * f * t) + 0.3 * std::sin(4.0 * M_PI * f * t);
            return static_cast<float>(0.28 * env * s);
          });
        }
      }
    }
    // Pad: one chord per bar through the break and under the drops, the full triad plus the root an octave up.
    if (part == Part::Break || part == Part::Drop || part == Part::Intro) {
      const double padGain = part == Part::Break ? 0.16 : (part == Part::Drop ? 0.07 : 0.04);
      const double notes[4] = {root, root + third, root + 7.0, root + 12.0};
      for (double nt : notes) {
        const double f = midiToHz(nt);
        put(t0, bar, [&](double t, std::size_t) {
          const double env = std::min(1.0, t / 0.05) * std::min(1.0, (bar - t) / 0.05);
          return static_cast<float>(padGain * env * (std::sin(2.0 * M_PI * f * t) + 0.3 * std::sin(4.0 * M_PI * f * t)) * 0.5);
        });
      }
    }
  }
  float peak = 0.f;
  for (float v : d) peak = std::max(peak, std::fabs(v));
  const float g = peak > 0.f ? spec.level / peak : 1.f;
  for (float& v : d) v *= g;
  return SampleBuffer::fromInterleavedStereo(d.data(), n, sr, spec.name);
}

}  // namespace autodj::demo
