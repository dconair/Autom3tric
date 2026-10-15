#include "autodj/perf/PadKit.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace autodj::perf {

namespace {

constexpr double kTwoPi = 6.283185307179586;

struct Noise {
  std::uint32_t s = 0x2545F491u;
  float next() noexcept {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return static_cast<float>(s >> 8) * (1.f / 8388608.f) - 1.f;
  }
};

struct OnePole {
  float a = 0.f, z = 0.f;
  void setCutoff(double hz, double sr) noexcept { a = static_cast<float>(1.0 - std::exp(-kTwoPi * hz / sr)); }
  float lp(float x) noexcept { return z += a * (x - z); }
  float hp(float x) noexcept { return x - lp(x); }
};

/// State-variable band-pass (Chamberlin), good enough for noise colouring.
struct Band {
  float f = 0.f, q = 0.5f, low = 0.f, bp = 0.f;
  void set(double hz, double sr, float damping) noexcept {
    f = static_cast<float>(2.0 * std::sin(3.14159265358979 * std::min(hz, sr * 0.2) / sr));
    q = damping;
  }
  float process(float x) noexcept {
    low += f * bp;
    const float high = x - low - q * bp;
    bp += f * high;
    return bp;
  }
};

void normalize(std::vector<float>& v, float peak) {
  float m = 0.f;
  for (float x : v) m = std::max(m, std::fabs(x));
  if (m < 1e-9f) return;
  const float g = peak / m;
  for (float& x : v) x *= g;
}

/// Short fade at both ends: no click at the start of the buffer, none at its cut-off end.
void edges(std::vector<float>& v, double sr) {
  const std::size_t n = v.size();
  const std::size_t head = std::min<std::size_t>(n / 2, static_cast<std::size_t>(0.0004 * sr));
  const std::size_t tail = std::min<std::size_t>(n / 2, static_cast<std::size_t>(0.012 * sr));
  for (std::size_t i = 0; i < head; ++i) v[i] *= static_cast<float>(i) / static_cast<float>(head);
  for (std::size_t i = 0; i < tail; ++i) v[n - 1 - i] *= static_cast<float>(i) / static_cast<float>(tail);
}

/// A sine whose pitch falls exponentially from f0 to f1 with time constant `sweepTau`, under an exponential decay `tau`.
std::vector<float> sweptSine(double sr, double seconds, double f0, double f1, double sweepTau, double tau, float drive) {
  const std::size_t n = static_cast<std::size_t>(seconds * sr);
  std::vector<float> v(n);
  double phase = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / sr;
    const double f = f1 + (f0 - f1) * std::exp(-t / sweepTau);
    phase += kTwoPi * f / sr;
    float s = static_cast<float>(std::sin(phase) * std::exp(-t / tau));
    if (drive > 1.f) s = std::tanh(s * drive) / std::tanh(drive);
    v[i] = s;
  }
  return v;
}

std::vector<float> kick(double sr) {
  std::vector<float> v = sweptSine(sr, 0.5, 190.0, 46.0, 0.028, 0.16, 2.2f);
  Noise nz;
  const std::size_t click = static_cast<std::size_t>(0.0025 * sr);
  OnePole lp;
  lp.setCutoff(4000.0, sr);
  for (std::size_t i = 0; i < click && i < v.size(); ++i) {
    v[i] += 0.35f * lp.lp(nz.next()) * (1.f - static_cast<float>(i) / static_cast<float>(click));
  }
  normalize(v, 0.95f);
  return v;
}

std::vector<float> subKick(double sr) {
  std::vector<float> v = sweptSine(sr, 0.95, 110.0, 38.0, 0.05, 0.4, 1.8f);
  normalize(v, 0.9f);
  return v;
}

std::vector<float> snare(double sr) {
  const std::size_t n = static_cast<std::size_t>(0.38 * sr);
  std::vector<float> v(n);
  Noise nz;
  OnePole hp;
  hp.setCutoff(1600.0, sr);
  double p1 = 0.0, p2 = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / sr;
    p1 += kTwoPi * (180.0 + 60.0 * std::exp(-t / 0.01)) / sr;
    p2 += kTwoPi * 331.0 / sr;
    const float body = static_cast<float>((std::sin(p1) * 0.8 + std::sin(p2) * 0.35) * std::exp(-t / 0.055));
    const float noise = hp.hp(nz.next()) * static_cast<float>(std::exp(-t / 0.095));
    v[i] = body * 0.9f + noise * 0.75f;
  }
  normalize(v, 0.85f);
  return v;
}

std::vector<float> clap(double sr) {
  const std::size_t n = static_cast<std::size_t>(0.4 * sr);
  std::vector<float> v(n);
  Noise nz;
  Band bp;
  bp.set(1250.0, sr, 0.45f);
  OnePole hp;
  hp.setCutoff(500.0, sr);
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / sr;
    double env = 0.0;
    for (int k = 0; k < 3; ++k) {  // three quick hands, then the room
      const double tk = t - 0.011 * k;
      if (tk >= 0.0) env += std::exp(-tk / 0.0035);
    }
    env += 0.6 * std::exp(-t / 0.11);
    v[i] = hp.hp(bp.process(nz.next())) * static_cast<float>(env);
  }
  normalize(v, 0.8f);
  return v;
}

/// Closed / open hat: inharmonic square oscillators plus noise, high-passed.
std::vector<float> hat(double sr, double seconds, double tau) {
  static const double kFreq[6] = {1334.0, 1978.0, 2402.0, 3398.0, 3510.0, 5297.0};
  const std::size_t n = static_cast<std::size_t>(seconds * sr);
  std::vector<float> v(n);
  Noise nz;
  OnePole h1, h2;
  h1.setCutoff(6800.0, sr);
  h2.setCutoff(6800.0, sr);
  double ph[6] = {0, 0, 0, 0, 0, 0};
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / sr;
    float s = 0.f;
    for (int k = 0; k < 6; ++k) {
      ph[k] += kFreq[k] / sr;
      ph[k] -= std::floor(ph[k]);
      s += ph[k] < 0.5 ? 0.12f : -0.12f;
    }
    s += 0.5f * nz.next();
    s = h2.hp(h1.hp(s));
    v[i] = s * static_cast<float>(std::exp(-t / tau));
  }
  return v;
}

std::vector<float> rim(double sr) {
  const std::size_t n = static_cast<std::size_t>(0.1 * sr);
  std::vector<float> v(n);
  Noise nz;
  double p1 = 0.0, p2 = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / sr;
    p1 += kTwoPi * 1750.0 / sr;
    p2 += kTwoPi * 820.0 / sr;
    const double env = std::exp(-t / 0.011);
    v[i] = static_cast<float>((std::sin(p1) * 0.7 + std::sin(p2) * 0.5) * env) + (i < 40 ? 0.4f * nz.next() : 0.f);
  }
  normalize(v, 0.6f);
  return v;
}

std::vector<float> shaker(double sr) {
  const std::size_t n = static_cast<std::size_t>(0.17 * sr);
  std::vector<float> v(n);
  Noise nz;
  Band bp;
  bp.set(6200.0, sr, 0.9f);
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / sr;
    const double env = (1.0 - std::exp(-t / 0.006)) * std::exp(-t / 0.035);
    v[i] = bp.process(nz.next()) * static_cast<float>(env);
  }
  normalize(v, 0.45f);
  return v;
}

std::vector<float> cowbell(double sr) {
  const std::size_t n = static_cast<std::size_t>(0.45 * sr);
  std::vector<float> v(n);
  double a = 0.0, b = 0.0;
  Band bp;
  bp.set(1000.0, sr, 0.35f);
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / sr;
    a += 540.0 / sr;
    b += 810.0 / sr;
    a -= std::floor(a);
    b -= std::floor(b);
    const float sq = (a < 0.5 ? 0.5f : -0.5f) + (b < 0.5 ? 0.5f : -0.5f);
    v[i] = bp.process(sq) * static_cast<float>(0.4 * std::exp(-t / 0.018) + 0.6 * std::exp(-t / 0.13));
  }
  normalize(v, 0.5f);
  return v;
}

std::vector<float> conga(double sr) {
  std::vector<float> v = sweptSine(sr, 0.3, 380.0, 260.0, 0.02, 0.075, 1.f);
  normalize(v, 0.7f);
  return v;
}

std::vector<float> crash(double sr) {
  const std::size_t n = static_cast<std::size_t>(1.7 * sr);
  std::vector<float> v(n);
  Noise nz;
  OnePole h1, h2;
  h1.setCutoff(3200.0, sr);
  h2.setCutoff(3200.0, sr);
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / sr;
    v[i] = h2.hp(h1.hp(nz.next())) * static_cast<float>(0.7 * std::exp(-t / 0.55) + 0.3 * std::exp(-t / 0.09));
  }
  normalize(v, 0.55f);
  return v;
}

std::vector<float> riser(double sr) {
  const double len = 2.0;
  const std::size_t n = static_cast<std::size_t>(len * sr);
  std::vector<float> v(n);
  Noise nz;
  Band bp;
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / sr / len;
    if ((i & 63) == 0) bp.set(250.0 * std::pow(28.0, t), sr, 0.25f);
    v[i] = bp.process(nz.next()) * static_cast<float>(std::pow(t, 1.6));
  }
  normalize(v, 0.5f);
  return v;
}

std::vector<float> laser(double sr) {
  const std::size_t n = static_cast<std::size_t>(0.35 * sr);
  std::vector<float> v(n);
  double ph = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / sr;
    const double f = 90.0 + 2400.0 * std::exp(-t / 0.045);
    ph += f / sr;
    ph -= std::floor(ph);
    v[i] = static_cast<float>((2.0 * ph - 1.0) * std::exp(-t / 0.11));
  }
  normalize(v, 0.5f);
  return v;
}

std::vector<float> drop(double sr) {
  std::vector<float> v = sweptSine(sr, 1.3, 140.0, 26.0, 0.35, 0.6, 1.5f);
  normalize(v, 0.9f);
  return v;
}

void setPan(PadSound& s, float pan) {  // -1 left .. +1 right, equal power
  const float a = (pan + 1.f) * 0.25f * 3.14159265f;
  s.gainL = std::cos(a);
  s.gainR = std::sin(a);
}

}  // namespace

void PadKit::render(double sampleRate) {
  const double sr = sampleRate > 8000.0 ? sampleRate : 48000.0;
  sounds_[0].data = kick(sr);
  sounds_[1].data = subKick(sr);
  sounds_[2].data = snare(sr);
  sounds_[3].data = clap(sr);
  sounds_[4].data = hat(sr, 0.14, 0.022);
  sounds_[5].data = hat(sr, 0.7, 0.16);
  sounds_[6].data = rim(sr);
  sounds_[7].data = shaker(sr);
  sounds_[8].data = sweptSine(sr, 0.55, 150.0, 92.0, 0.03, 0.2, 1.3f);
  sounds_[9].data = sweptSine(sr, 0.4, 250.0, 160.0, 0.03, 0.14, 1.3f);
  sounds_[10].data = cowbell(sr);
  sounds_[11].data = conga(sr);
  sounds_[12].data = crash(sr);
  sounds_[13].data = riser(sr);
  sounds_[14].data = laser(sr);
  sounds_[15].data = drop(sr);
  normalize(sounds_[4].data, 0.5f);
  normalize(sounds_[5].data, 0.55f);
  normalize(sounds_[8].data, 0.8f);
  normalize(sounds_[9].data, 0.8f);
  for (PadSound& s : sounds_) edges(s.data, sr);

  sounds_[4].chokeGroup = sounds_[5].chokeGroup = 1;
  for (int p : {4, 5, 6, 7, 10, 11}) sounds_[p].accentProfile = true;
  setPan(sounds_[4], 0.25f);
  setPan(sounds_[5], 0.25f);
  setPan(sounds_[7], -0.3f);
  setPan(sounds_[8], -0.2f);
  setPan(sounds_[9], 0.2f);
  setPan(sounds_[10], 0.3f);
  setPan(sounds_[11], -0.3f);
}

unsigned defaultPatternRow(int pad) noexcept {
  // bit s = step s (0..15), a step is a 16th note; step 0 is the downbeat of the bar
  switch (pad) {
    case 0: return 0x1111u;  // kick on every beat (four on the floor)
    case 3: return 0x1010u;  // clap on 2 and 4
    case 4: return 0x8888u;  // closed hat on the last 16th of each beat (cuts the open hat)
    case 5: return 0x4444u;  // open hat on the off-beats
    case 7: return 0xAAAAu;  // shaker on the odd 16ths
    default: return 0u;
  }
}

}  // namespace autodj::perf
