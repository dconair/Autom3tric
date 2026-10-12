#include "autodj/dj/Deck.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <signalsmith-stretch/signalsmith-stretch.h>

namespace autodj::dj {

namespace {
constexpr int kMaxInput = 24576;  // outputSeek pre-roll (~ block + interval + latencies) and generous headroom for rate chasing
constexpr float kFadeFloor = 1e-4f;
}  // namespace

struct Deck::Impl {
  signalsmith::stretch::SignalsmithStretch<float> stretch{12345L};  // fixed seed: offline renders are reproducible
  alignas(64) float inL[kMaxInput];
  alignas(64) float inR[kMaxInput];
  float* in[2] = {inL, inR};
  float* out[2] = {nullptr, nullptr};
};

Deck::Deck() : impl_(std::make_unique<Impl>()) {
  std::memset(outL_, 0, sizeof(outL_));
  std::memset(outR_, 0, sizeof(outR_));
}

Deck::~Deck() = default;

void Deck::prepare(double sampleRate) {
  sr_ = sampleRate;
  impl_->stretch.presetDefault(2, static_cast<float>(sampleRate));
  impl_->stretch.reset();
  inLat_ = impl_->stretch.inputLatency();
  outLat_ = impl_->stretch.outputLatency();
  eq_.prepare(sampleRate);
  filter_.prepare(sampleRate);
  impl_->out[0] = outL_;
  impl_->out[1] = outR_;
}

void Deck::setTrack(const Track* t, double startSec) noexcept {
  track_ = t;
  streaming_ = false;
  stopRequested_ = false;
  wantStart_ = -1.0;
  restAt_ = -1.0;
  ended_ = false;
  pausedAt_ = std::max(0.0, startSec) * sr_;
  fadeGain_ = 1.f;
  out_ = {};
}

void Deck::play(double fromSec) noexcept {
  if (!track_) return;
  ended_ = false;
  stopRequested_ = false;
  restAt_ = -1.0;
  wantStart_ = std::clamp(fromSec * sr_, 0.0, static_cast<double>(track_->audio->frames()));
}

void Deck::pause() noexcept {
  wantStart_ = -1.0;
  restAt_ = -1.0;
  if (streaming_) stopRequested_ = true;
}

void Deck::seek(double sec) noexcept {
  if (!track_) return;
  const double f = std::clamp(sec * sr_, 0.0, static_cast<double>(track_->audio->frames()));
  ended_ = false;
  if (stopRequested_) {  // "pause, then jump": rest at the new place once the fade-out is done
    restAt_ = f;
    return;
  }
  if (streaming_ || wantStart_ >= 0.0) {
    wantStart_ = f;
    stopRequested_ = false;
  } else {
    pausedAt_ = f;
  }
}

double Deck::heardFrames() const noexcept {
  if (!streaming_) return wantStart_ >= 0.0 ? wantStart_ : pausedAt_;
  return cursorExact_ - lag(lastRate_);
}

void Deck::fillInput(std::int64_t from, int count) noexcept {
  const std::int64_t frames = track_->audio->frames();
  const float* src = track_->audio->data();
  float* l = impl_->inL;
  float* r = impl_->inR;
  if (from >= 0 && from + count <= frames) {
    for (int i = 0; i < count; ++i) {
      l[i] = src[2 * (from + i)];
      r[i] = src[2 * (from + i) + 1];
    }
    return;
  }
  for (int i = 0; i < count; ++i) {
    const std::int64_t idx = from + i;
    if (idx < 0 || idx >= frames) {
      l[i] = r[i] = 0.f;
    } else {
      l[i] = src[2 * idx];
      r[i] = src[2 * idx + 1];
    }
  }
}

void Deck::startStream(double startFrames, double rate) noexcept {
  const double r = std::clamp(rate, 0.25, 4.0);
  const int seekLen = std::min(impl_->stretch.outputSeekLength(static_cast<float>(r)), kMaxInput);
  const std::int64_t start = std::llround(startFrames);
  fillInput(start, seekLen);
  impl_->stretch.outputSeek(static_cast<float* const*>(impl_->in), seekLen);
  cursor_ = start + seekLen;
  cursorExact_ = static_cast<double>(cursor_);
  lastRate_ = r;
  streaming_ = true;
  fadeGain_ = 0.f;
  lastTranspose_ = -1.0;
}

void Deck::process(float* out, int frames, const DeckBlock& b) noexcept {
  out_.peakL = out_.peakR = 0.f;
  if (!track_ || frames <= 0 || frames > kMaxFrames) return;

  bool fadeOut = false;
  if (!streaming_) {
    if (wantStart_ < 0.0) return;
    startStream(wantStart_, b.rate);
    wantStart_ = -1.0;
  } else if (wantStart_ >= 0.0) {
    fadeOut = true;  // seek while playing: fade out now, restart at the new place in the next block
  } else if (stopRequested_) {
    fadeOut = true;
  }

  // --- how much source audio to feed ----------------------------------------------------------------
  const double rate = std::clamp(b.rate, 0.25, 4.0);
  double endCursor = cursorExact_ + frames * rate;
  if (b.hasTarget) {
    const double desired = b.targetEnd + lag(rate);
    const double dev = b.maxCorr * frames * rate;
    endCursor = std::clamp(desired, endCursor - dev, endCursor + dev);
  }
  std::int64_t newCursor = std::llround(endCursor);
  int nIn = static_cast<int>(std::clamp<std::int64_t>(newCursor - cursor_, 0, kMaxInput));
  newCursor = cursor_ + nIn;
  cursorExact_ = std::max(endCursor, static_cast<double>(cursor_));
  lastRate_ = rate;

  fillInput(cursor_, nIn);
  const double transpose = b.keylock ? 1.0 : rate;
  if (transpose != lastTranspose_) {
    impl_->stretch.setTransposeFactor(static_cast<float>(transpose));
    lastTranspose_ = transpose;
  }
  impl_->stretch.process(static_cast<float* const*>(impl_->in), nIn, static_cast<float* const*>(impl_->out), frames);
  cursor_ = newCursor;

  // --- tone: EQ, filter, gain ---------------------------------------------------------------------------
  eq_.process(outL_, outR_, frames, b.eq0, b.eq1);
  filter_.process(outL_, outR_, frames, b.filter0, b.filter1);

  const float f0 = fadeGain_;
  const float f1 = fadeOut ? 0.f : 1.f;
  const float inv = 1.f / static_cast<float>(frames);
  float pl = 0.f, pr = 0.f;
  for (int i = 0; i < frames; ++i) {
    const float t = (static_cast<float>(i) + 1.f) * inv;
    const float g = (b.gain0 + (b.gain1 - b.gain0) * t) * (f0 + (f1 - f0) * t);
    const float l = outL_[i] * g;
    const float r = outR_[i] * g;
    out[2 * i] += l;
    out[2 * i + 1] += r;
    pl = std::max(pl, std::fabs(l));
    pr = std::max(pr, std::fabs(r));
  }
  fadeGain_ = f1;
  out_.peakL = pl;
  out_.peakR = pr;

  if (fadeOut) {
    pausedAt_ = restAt_ >= 0.0 ? restAt_ : std::max(0.0, heardFrames());
    restAt_ = -1.0;
    streaming_ = false;
    stopRequested_ = false;
    (void)kFadeFloor;
    return;
  }
  if (heardFrames() >= static_cast<double>(track_->audio->frames())) {
    streaming_ = false;
    ended_ = true;
    pausedAt_ = static_cast<double>(track_->audio->frames());
  }
}

}  // namespace autodj::dj
