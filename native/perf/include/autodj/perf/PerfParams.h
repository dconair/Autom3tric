#pragma once
// Registry entries of the performance section.

#include "autodj/core/Param.h"

namespace autodj::perf {

struct PerfParamIds {
  ParamId echoOn = kInvalidParam, echoBeats = kInvalidParam, echoFeedback = kInvalidParam, echoMix = kInvalidParam;
  ParamId reverbOn = kInvalidParam, reverbMix = kInvalidParam;
  ParamId flangerOn = kInvalidParam, flangerMix = kInvalidParam;
  ParamId rollOn = kInvalidParam, rollBeats = kInvalidParam;
  ParamId stutterOn = kInvalidParam, stutterBeats = kInvalidParam;
  ParamId padVolume = kInvalidParam, padSeqOn = kInvalidParam, padSwing = kInvalidParam;
};

PerfParamIds registerPerfParams(ParamRegistry& reg);

/// Note lengths of the enum parameters, in beats.
inline constexpr float kEchoBeats[5] = {0.5f, 1.0f, 2.0f, 3.0f, 4.0f};      // "1/8", "1/4", "1/2", "3/4", "1" (note values, quarter = 1 beat)
inline constexpr float kRollBeats[4] = {1.0f, 0.5f, 0.25f, 0.125f};          // "1", "1/2", "1/4", "1/8" of a beat
inline constexpr float kStutterBeats[3] = {1.0f, 0.5f, 0.25f};              // "1/4", "1/8", "1/16" notes

}  // namespace autodj::perf
