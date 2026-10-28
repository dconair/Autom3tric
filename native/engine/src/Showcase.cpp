#include "autodj/engine/Showcase.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "autodj/perf/PadKit.h"

namespace autodj::showcase {

namespace {

constexpr double kTwoPi = 6.283185307179586;

double midiToHz(double m) { return 440.0 * std::pow(2.0, (m - 69.0) / 12.0); }

/// Am - F - C - G, one chord per bar: root note (MIDI, octave 3) and whether the third is minor.
struct Chord {
  int root;
  int third;
};
constexpr Chord kChords[4] = {{57, 3}, {53, 4}, {48, 4}, {55, 4}};

struct Loop {
  int sr;
  std::size_t n;
  std::vector<float> l, r;
  explicit Loop(int sampleRate) : sr(sampleRate) {
    n = static_cast<std::size_t>(std::llround(kBars * 4 * 60.0 / kBpm * sampleRate));
    l.assign(n, 0.f);
    r.assign(n, 0.f);
  }
  /// Adds one sample at `pos`, wrapping around the end (tails run into the start of the loop, so the loop is seamless).
  void add(std::size_t pos, float vl, float vr) {
    const std::size_t i = pos % n;
    l[i] += vl;
    r[i] += vr;
  }
  double beatFrames() const { return 60.0 / kBpm * sr; }
};

/// Removes the click and the DC a loop can carry and brings it to `rmsDb` without letting the peak pass `peakCeil`.
void finish(Loop& lp, double rmsDb, float peakCeil) {
  double e = 0;
  float peak = 0.f, dcl = 0.f, dcr = 0.f;
  for (std::size_t i = 0; i < lp.n; ++i) {
    dcl += lp.l[i];
    dcr += lp.r[i];
  }
  dcl /= static_cast<float>(lp.n);
  dcr /= static_cast<float>(lp.n);
  for (std::size_t i = 0; i < lp.n; ++i) {
    lp.l[i] -= dcl;
    lp.r[i] -= dcr;
    e += static_cast<double>(lp.l[i]) * lp.l[i] + static_cast<double>(lp.r[i]) * lp.r[i];
    peak = std::max({peak, std::fabs(lp.l[i]), std::fabs(lp.r[i])});
  }
  const double rms = std::sqrt(e / (2.0 * static_cast<double>(lp.n)));
  if (rms < 1e-9 || peak < 1e-9f) return;
  const float g = std::min(static_cast<float>(std::pow(10.0, rmsDb / 20.0) / rms), peakCeil / peak);
  for (std::size_t i = 0; i < lp.n; ++i) {
    lp.l[i] *= g;
    lp.r[i] *= g;
  }
}

/// A one-pole low-pass run around the loop twice, so that the state at the seam is the state at the end of the loop.
void lowpassLoop(std::vector<float>& x, double sr, const std::vector<double>& cutoffHz) {
  float z = 0.f;
  for (int pass = 0; pass < 2; ++pass) {
    for (std::size_t i = 0; i < x.size(); ++i) {
      const float a = static_cast<float>(1.0 - std::exp(-kTwoPi * cutoffHz[i] / sr));
      z += a * (x[i] - z);
      if (pass == 1) x[i] = z;
    }
  }
}

/// One period of a band-limited saw-like wave (harmonics 1/h up to `maxHz`), read with linear interpolation.
struct Wave {
  static constexpr int kSize = 4096;
  std::vector<float> t;
  Wave(double f, double sr, double phase) : t(kSize + 1) {
    const int harmonics = std::max(1, std::min(24, static_cast<int>(std::min(9000.0, sr * 0.45) / f)));
    for (int i = 0; i < kSize; ++i) {
      double v = 0;
      for (int h = 1; h <= harmonics; ++h) v += std::sin(kTwoPi * h * i / kSize + phase * h) / h;
      t[static_cast<std::size_t>(i)] = static_cast<float>(v);
    }
    t[kSize] = t[0];
  }
  float at(double ph) const {
    const double x = (ph - std::floor(ph)) * kSize;
    const int i = static_cast<int>(x);
    const float fr = static_cast<float>(x - i);
    return t[static_cast<std::size_t>(i)] * (1.f - fr) + t[static_cast<std::size_t>(i) + 1] * fr;
  }
};

/// Pad: slow, detuned, band-limited saw-like voices, one chord per bar, a low-pass that opens and closes every bar.
void renderPad(Loop& lp) {
  const double sr = lp.sr;
  const double bar = 4 * lp.beatFrames();
  const double attack = 0.45 * sr, release = 1.1 * sr;
  for (int b = 0; b < kBars; ++b) {
    const Chord c = kChords[b];
    const int notes[4] = {c.root, c.root + c.third, c.root + 7, c.root + 12};
    const std::size_t start = static_cast<std::size_t>(b * bar);
    const std::size_t len = static_cast<std::size_t>(bar + release);
    for (int k = 0; k < 4; ++k) {
      for (int d = 0; d < 3; ++d) {  // three detuned copies per note, spread over the stereo field
        const double cents = (d - 1) * 7.0 + 1.3 * k;
        const double f = midiToHz(notes[k]) * std::pow(2.0, cents / 1200.0);
        const Wave wave(f, sr, 0.9 * k + 2.1 * d);
        const float pan = std::clamp(0.5f + 0.38f * static_cast<float>(d - 1) + 0.05f * (static_cast<float>(k) - 1.5f), 0.f, 1.f);
        const float gl = std::sqrt(1.f - pan), gr = std::sqrt(pan);
        double ph = 0.37 * d;
        for (std::size_t i = 0; i < len; ++i) {
          const double t = static_cast<double>(i);
          const double env = std::min(1.0, t / attack) * (t < bar ? 1.0 : std::max(0.0, 1.0 - (t - bar) / release));
          const double smooth = env * env * (3.0 - 2.0 * env);  // smoothstep: no corner at the ends
          const float v = static_cast<float>(0.05 * smooth) * wave.at(ph);
          ph += f / sr;
          lp.add(start + i, v * gl, v * gr);
        }
      }
    }
  }
  std::vector<double> cut(lp.n);
  for (std::size_t i = 0; i < lp.n; ++i) {
    const double phase = std::fmod(static_cast<double>(i), bar) / bar;
    cut[i] = 700.0 + 2600.0 * std::sin(3.14159265358979 * phase);
  }
  lowpassLoop(lp.l, sr, cut);
  lowpassLoop(lp.r, sr, cut);
  finish(lp, -15.0, 0.8f);
}

/// Drums: the pad kit of the Pads tab, four on the floor with a clap on 2 and 4, off-beat hats, a shaker and a small fill.
void renderDrums(Loop& lp) {
  perf::PadKit kit;
  kit.render(lp.sr);
  const double step = lp.beatFrames() / 4.0;
  auto hit = [&](int pad, int stepIndex, float vel) {
    const perf::PadSound& s = kit.sound(pad);
    const std::size_t at = static_cast<std::size_t>(std::llround(stepIndex * step));
    for (std::size_t i = 0; i < s.data.size(); ++i) {
      const float v = s.data[i] * vel;
      lp.add(at + i, v * s.gainL * 1.2f, v * s.gainR * 1.2f);
    }
  };
  for (int bar = 0; bar < kBars; ++bar) {
    const int o = bar * 16;
    for (int beat = 0; beat < 4; ++beat) hit(0, o + beat * 4, beat == 0 ? 1.f : 0.9f);
    hit(3, o + 4, 0.85f);
    hit(3, o + 12, 0.85f);
    hit(2, o + 12, 0.45f);
    for (int e = 0; e < 4; ++e) hit(4, o + e * 4 + 2, e % 2 == 0 ? 0.8f : 0.6f);
    hit(5, o + 14, 0.5f);
    for (int s = 0; s < 16; ++s) hit(7, o + s, s % 4 == 2 ? 0.5f : 0.3f);
    if (bar == 3) {  // fill: toms and a snare roll into the loop start
      hit(8, o + 10, 0.8f);
      hit(9, o + 11, 0.8f);
      hit(8, o + 13, 0.85f);
      hit(2, o + 14, 0.7f);
      hit(2, o + 15, 0.85f);
    }
  }
  finish(lp, -15.0, 0.9f);
}

/// Pluck: sixteenth-note arpeggio over the chords. Each note is a handful of harmonics, the upper ones dying faster (like a string).
void renderPluck(Loop& lp) {
  const double sr = lp.sr;
  const double step = lp.beatFrames() / 4.0;
  static const int kShape[16] = {0, 2, 1, 3, 2, 3, 1, 2, 0, 3, 2, 1, 3, 2, 1, 0};  // indices into the chord tones
  for (int b = 0; b < kBars; ++b) {
    const Chord c = kChords[b];
    const int tones[4] = {c.root + 12, c.root + 12 + c.third, c.root + 12 + 7, c.root + 24};
    for (int s = 0; s < 16; ++s) {
      const double f = midiToHz(tones[kShape[s]]);
      const std::size_t at = static_cast<std::size_t>(std::llround((b * 16 + s) * step));
      const float accent = (s % 4 == 0) ? 1.f : (s % 2 == 0 ? 0.75f : 0.55f);
      const float pan = 0.5f + 0.3f * std::sin(0.9f * static_cast<float>(s) + static_cast<float>(b));
      const float gl = std::sqrt(1.f - pan), gr = std::sqrt(pan);
      const std::size_t len = static_cast<std::size_t>(0.9 * sr);
      const int harmonics = std::max(1, std::min(10, static_cast<int>(sr * 0.45 / f)));
      // every harmonic is a rotating phasor with its own decay, so no sin()/exp() runs per sample
      struct Partial {
        double c, s, dc, ds, amp, decay;
      };
      std::vector<Partial> parts;
      for (int h = 1; h <= harmonics; ++h) {
        const double w = kTwoPi * f * h / sr;
        parts.push_back({1.0, 0.0, std::cos(w), std::sin(w), 1.0 / (h * h * 0.6 + 0.4), std::exp(-(3.2 + 2.6 * h) / sr)});
      }
      for (std::size_t i = 0; i < len; ++i) {
        double v = 0;
        for (Partial& p : parts) {
          v += p.s * p.amp;
          const double nc = p.c * p.dc - p.s * p.ds, ns = p.s * p.dc + p.c * p.ds;
          p.c = nc;
          p.s = ns;
          p.amp *= p.decay;
        }
        const double attack = std::min(1.0, static_cast<double>(i) / (0.002 * sr));
        const double tail = std::min(1.0, static_cast<double>(len - i) / (0.02 * sr));
        const float x = static_cast<float>(0.2 * accent * attack * tail * v);
        lp.add(at + i, x * gl, x * gr);
      }
    }
  }
  finish(lp, -16.0, 0.8f);
}

/// Bass: syncopated eighth notes on the chord root with a little saturation, so that the harmonics carry on small speakers.
void renderBass(Loop& lp) {
  const double sr = lp.sr;
  const double eighth = lp.beatFrames() / 2.0;
  static const int kPattern[8] = {1, 0, 1, 1, 0, 1, 0, 1};  // which eighths play
  for (int b = 0; b < kBars; ++b) {
    const Chord c = kChords[b];
    for (int e = 0; e < 8; ++e) {
      if (!kPattern[e]) continue;
      const bool fifth = (e == 7) && (b % 2 == 1);
      const double f = midiToHz(c.root - 12 + (fifth ? 7 : 0));
      const std::size_t at = static_cast<std::size_t>(std::llround((b * 8 + e) * eighth));
      const std::size_t len = static_cast<std::size_t>(0.95 * eighth);
      for (std::size_t i = 0; i < len; ++i) {
        const double t = static_cast<double>(i) / sr;
        const double attack = std::min(1.0, t / 0.004);
        const double release = std::min(1.0, (len - i) / (0.012 * sr));
        const double env = attack * release * (0.55 + 0.45 * std::exp(-t / 0.12));
        double s = std::sin(kTwoPi * f * t) + 0.35 * std::sin(kTwoPi * 2.0 * f * t) + 0.14 * std::sin(kTwoPi * 3.0 * f * t);
        s = std::tanh(1.6 * s) * 0.55;
        const float v = static_cast<float>(0.5 * env * s);
        lp.add(at + i, v, v);
      }
    }
  }
  finish(lp, -14.0, 0.8f);
}

}  // namespace

const char* name(int kind) noexcept {
  static const char* kNames[kKinds] = {"pad", "drums", "pluck", "bass"};
  return kNames[((kind % kKinds) + kKinds) % kKinds];
}

std::shared_ptr<const SampleBuffer> make(int kind, int sampleRate) {
  Loop lp(sampleRate);
  switch (((kind % kKinds) + kKinds) % kKinds) {
    case 0: renderPad(lp); break;
    case 1: renderDrums(lp); break;
    case 2: renderPluck(lp); break;
    default: renderBass(lp); break;
  }
  std::vector<float> lr(lp.n * 2);
  for (std::size_t i = 0; i < lp.n; ++i) {
    lr[2 * i] = lp.l[i];
    lr[2 * i + 1] = lp.r[i];
  }
  return SampleBuffer::fromInterleavedStereo(lr.data(), lp.n, sampleRate, name(kind));
}

}  // namespace autodj::showcase
