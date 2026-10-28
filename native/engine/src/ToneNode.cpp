#include "autodj/engine/ToneNode.h"

#include <cmath>

namespace autodj {

ToneParamIds registerToneParams(ParamRegistry& reg) {
  ToneParamIds ids;
  ParamSpec en;
  en.key = "tone.enabled";
  en.label = "Test tone";
  en.group = "Test tone";
  en.type = ParamType::Bool;
  en.def = 0.f;
  ids.enabled = reg.add(std::move(en));

  ParamSpec f;
  f.key = "tone.freq_hz";
  f.label = "Frequency";
  f.group = "Test tone";
  f.unit = "Hz";
  f.min = 20.f;
  f.max = 8000.f;
  f.def = 440.f;
  f.scale = ParamScale::Log;
  ids.freqHz = reg.add(std::move(f));

  ParamSpec g;
  g.key = "tone.gain";
  g.label = "Level";
  g.group = "Test tone";
  g.min = 0.f;
  g.max = 1.f;
  g.def = 0.2f;
  ids.gain = reg.add(std::move(g));
  return ids;
}

void ToneNode::setParams(const ParamStore& st, const BlockTime&) noexcept {
  freq_ = st.get(ids_.freqHz);
  gainTarget_ = st.getBool(ids_.enabled) ? st.get(ids_.gain) : 0.f;
}

void ToneNode::process(ProcessIO& io) noexcept {
  if (gainTarget_ <= 0.f && gainPrev_ <= 0.f) return;
  constexpr double kTwoPi = 6.283185307179586;
  const double inc = kTwoPi * freq_ / sr_;
  const float dg = (gainTarget_ - gainPrev_) / static_cast<float>(io.frames);
  for (int i = 0; i < io.frames; ++i) {
    const float s = static_cast<float>(std::sin(phase_)) * (gainPrev_ + dg * static_cast<float>(i));
    io.out[2 * i] += s;
    io.out[2 * i + 1] += s;
    phase_ += inc;
  }
  if (phase_ > kTwoPi) phase_ -= kTwoPi * std::floor(phase_ / kTwoPi);
  gainPrev_ = gainTarget_;
}

}  // namespace autodj
