#include "autodj/fx/ChaosFx.h"

#include <algorithm>
#include <cmath>

namespace autodj::fx {

namespace {
// Freeverb tunings at 44.1 kHz (public domain), scaled to the actual rate. The right channel is detuned by 23 samples.
constexpr int kCombTuning[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
constexpr int kAllpassTuning[4] = {556, 441, 341, 225};
constexpr int kStereoSpread = 23;
constexpr float kReverbInputGain = 0.015f;
constexpr float kAllpassFeedback = 0.5f;
constexpr float kMaxWet = 0.6f;
constexpr double kTwoPi = 6.283185307179586;
constexpr float kLive = 1e-4f;  // a stage whose gain is below this is considered off
}  // namespace

ChaosFx::Mapping ChaosFx::map(float amount) noexcept {
  const float a = amount >= 0.f ? (amount <= 1.f ? amount : 1.f) : 0.f;  // NaN-safe clamp
  // Overall wet level rises quickly at the bottom of the knob so that 10 % is already audible.
  const float k = std::min(1.f, a * 4.f);
  const float level = kMaxWet * (1.f - (1.f - k) * (1.f - k));
  const float theta = a * 1.5707963f;  // 0 = pure reverb ... pi/2 = pure flanger (equal power)
  Mapping m;
  m.reverbGain = level * std::cos(theta);
  m.flangerGain = level * std::sin(theta);
  m.feedback = 0.45f + 0.35f * a;  // resonance of the flanger comb: 0.45 .. 0.80 (always < 1: stable)
  m.lfoHz = 0.08f + 0.5f * a;
  return m;
}

void ChaosFx::prepare(double sampleRate) {
  sr_ = sampleRate > 8000.0 ? sampleRate : 48000.0;
  const double scale = sr_ / 44100.0;
  for (int ch = 0; ch < 2; ++ch) {
    for (int i = 0; i < kCombs; ++i) {
      const std::size_t len = static_cast<std::size_t>(kCombTuning[i] * scale) + (ch ? static_cast<std::size_t>(kStereoSpread * scale) : 0);
      combs_[ch][i].buf.assign(std::max<std::size_t>(len, 8), 0.f);
    }
    for (int i = 0; i < kAllpasses; ++i) {
      const std::size_t len = static_cast<std::size_t>(kAllpassTuning[i] * scale) + (ch ? static_cast<std::size_t>(kStereoSpread * scale) : 0);
      allpass_[ch][i].buf.assign(std::max<std::size_t>(len, 8), 0.f);
    }
  }
  // Flanger delay line: next power of two above 12 ms (mask wrap-around, no modulo in the loop).
  std::size_t n = 64;
  while (n < static_cast<std::size_t>(0.012 * sr_) + 4) n <<= 1;
  for (auto& f : flange_) f.assign(n, 0.f);
  flangeMask_ = n - 1;
  reset();
}

void ChaosFx::clearReverb() noexcept {
  for (auto& ch : combs_) {
    for (auto& c : ch) {
      std::fill(c.buf.begin(), c.buf.end(), 0.f);
      c.pos = 0;
      c.lp = 0.f;
    }
  }
  for (auto& ch : allpass_) {
    for (auto& a : ch) {
      std::fill(a.buf.begin(), a.buf.end(), 0.f);
      a.pos = 0;
    }
  }
}

void ChaosFx::clearFlanger() noexcept {
  for (auto& f : flange_) std::fill(f.begin(), f.end(), 0.f);
  flangeFb_[0] = flangeFb_[1] = 0.f;
  flangeWrite_ = 0;
}

void ChaosFx::reset() noexcept {
  clearReverb();
  clearFlanger();
  lfoPhase_ = 0.0;
  reverbLive_ = false;
  flangerLive_ = false;
}

void ChaosFx::process(float* io, int frames, float a0, float a1) noexcept {
  const Mapping m0 = map(a0);
  const Mapping m1 = map(a1);
  const bool wantReverb = m0.reverbGain > kLive || m1.reverbGain > kLive;
  const bool wantFlanger = m0.flangerGain > kLive || m1.flangerGain > kLive;
  if (!wantReverb && !wantFlanger) {
    // Fully off: forget the state once, so that coming back never replays old audio.
    if (reverbLive_ || flangerLive_) reset();
    return;
  }
  if (wantReverb && !reverbLive_) clearReverb();
  if (wantFlanger && !flangerLive_) clearFlanger();
  reverbLive_ = wantReverb;
  flangerLive_ = wantFlanger;
  if (frames <= 0) return;

  const float inv = 1.f / static_cast<float>(frames);
  const float dRev = (m1.reverbGain - m0.reverbGain) * inv;
  const float dFlg = (m1.flangerGain - m0.flangerGain) * inv;
  const float fb = m1.feedback;
  const float lfoInc = static_cast<float>(kTwoPi * static_cast<double>(m1.lfoHz) / sr_);
  const float centerDelay = static_cast<float>(0.003 * sr_);  // 3 ms
  const float depth = static_cast<float>(0.0025 * sr_);       // +- 2.5 ms
  // Gain compensation: a comb with feedback fb boosts resonances by up to 1 / (1 - fb).
  const float flgNorm = 1.f - 0.5f * fb;
  const float damp1 = reverbDamp_, damp2 = 1.f - reverbDamp_;
  const float room = reverbFeedback_;

  float revG = m0.reverbGain, flgG = m0.flangerGain;
  float phase = static_cast<float>(lfoPhase_);
  std::size_t w = flangeWrite_;
  float* const dl = flange_[0].data();
  float* const dr = flange_[1].data();
  const std::size_t mask = flangeMask_;

  for (int i = 0; i < frames; ++i) {
    const float inL = io[2 * i];
    const float inR = io[2 * i + 1];
    float wetL = 0.f, wetR = 0.f;

    if (reverbLive_) {
      const float x = (inL + inR) * kReverbInputGain;
      float oL = 0.f, oR = 0.f;
      for (int c = 0; c < kCombs; ++c) {
        Comb& cl = combs_[0][c];
        Comb& cr = combs_[1][c];
        const float yl = cl.buf[cl.pos];
        cl.lp = yl * damp2 + cl.lp * damp1;
        cl.buf[cl.pos] = x + cl.lp * room;
        if (++cl.pos >= cl.buf.size()) cl.pos = 0;
        oL += yl;
        const float yr = cr.buf[cr.pos];
        cr.lp = yr * damp2 + cr.lp * damp1;
        cr.buf[cr.pos] = x + cr.lp * room;
        if (++cr.pos >= cr.buf.size()) cr.pos = 0;
        oR += yr;
      }
      for (int a = 0; a < kAllpasses; ++a) {
        Allpass& al = allpass_[0][a];
        Allpass& ar = allpass_[1][a];
        const float bl = al.buf[al.pos];
        const float yl = bl - oL;
        al.buf[al.pos] = oL + bl * kAllpassFeedback;
        oL = yl;
        if (++al.pos >= al.buf.size()) al.pos = 0;
        const float br = ar.buf[ar.pos];
        const float yr = br - oR;
        ar.buf[ar.pos] = oR + br * kAllpassFeedback;
        oR = yr;
        if (++ar.pos >= ar.buf.size()) ar.pos = 0;
      }
      wetL += oL * revG;  // Freeverb scaling: the 0.015 input trim above makes the summed network roughly unity gain
      wetR += oR * revG;
    }

    if (flangerLive_) {
      dl[w] = inL + flangeFb_[0] * fb;
      dr[w] = inR + flangeFb_[1] * fb;
      const float sL = std::sin(phase);
      const float sR = std::cos(phase);  // 90 degrees apart
      const float posL = static_cast<float>(w) + static_cast<float>(mask + 1) - (centerDelay + depth * sL);
      const float posR = static_cast<float>(w) + static_cast<float>(mask + 1) - (centerDelay + depth * sR);
      const std::size_t iL = static_cast<std::size_t>(posL);
      const std::size_t iR = static_cast<std::size_t>(posR);
      const float fL = posL - static_cast<float>(iL);
      const float fR = posR - static_cast<float>(iR);
      const float yL = dl[iL & mask] + (dl[(iL + 1) & mask] - dl[iL & mask]) * fL;
      const float yR = dr[iR & mask] + (dr[(iR + 1) & mask] - dr[iR & mask]) * fR;
      flangeFb_[0] = yL;
      flangeFb_[1] = yR;
      wetL += yL * flgG * flgNorm;
      wetR += yR * flgG * flgNorm;
      w = (w + 1) & mask;
      phase += lfoInc;
      if (phase > static_cast<float>(kTwoPi)) phase -= static_cast<float>(kTwoPi);
    }

    io[2 * i] = inL + wetL;
    io[2 * i + 1] = inR + wetR;
    revG += dRev;
    flgG += dFlg;
  }
  flangeWrite_ = w;
  lfoPhase_ = phase;
}

}  // namespace autodj::fx
