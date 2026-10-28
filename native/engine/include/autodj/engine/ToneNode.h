#pragma once
// Plain sine source. Exists for the audio-path smoke test (Oboe stream, latency, glitch checks)
// before any real material is available.

#include "autodj/core/Dsp.h"
#include "autodj/core/Node.h"

namespace autodj {

struct ToneParamIds {
  ParamId enabled = kInvalidParam;
  ParamId freqHz = kInvalidParam;
  ParamId gain = kInvalidParam;
};

ToneParamIds registerToneParams(ParamRegistry& reg);

class ToneNode final : public Node {
 public:
  explicit ToneNode(const ToneParamIds& ids) : ids_(ids) {}

  void prepare(const ProcessSpec& spec) override { sr_ = spec.sampleRate; }
  void reset() noexcept override {
    phase_ = 0.0;
    gainPrev_ = 0.f;
  }
  void setParams(const ParamStore& st, const BlockTime& t) noexcept override;
  void process(ProcessIO& io) noexcept override;

 private:
  ToneParamIds ids_;
  double sr_ = 48000.0;
  double phase_ = 0.0;
  double freq_ = 440.0;
  float gainTarget_ = 0.f;
  float gainPrev_ = 0.f;
};

}  // namespace autodj
