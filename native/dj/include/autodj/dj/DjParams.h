#pragma once
// Registry entries of the DJ section: per deck volume / trim / 3-band EQ / filter / tempo / sync / key lock, plus the crossfader.

#include "autodj/core/Param.h"

namespace autodj::dj {

struct DeckParamIds {
  ParamId volume = kInvalidParam;
  ParamId trimDb = kInvalidParam;
  ParamId autoTrim = kInvalidParam;
  ParamId eqLow = kInvalidParam, eqMid = kInvalidParam, eqHigh = kInvalidParam;  // dB, -26 = kill
  ParamId filter = kInvalidParam;                                                // -1 low-pass ... +1 high-pass
  ParamId tempoPct = kInvalidParam;
  ParamId sync = kInvalidParam;
  ParamId keylock = kInvalidParam;
};

struct DjParamIds {
  DeckParamIds deck[2];
  ParamId crossfader = kInvalidParam;
  ParamId curve = kInvalidParam;  // 0 blend, 1 equal power, 2 cut
};

DjParamIds registerDjParams(ParamRegistry& reg);

}  // namespace autodj::dj
