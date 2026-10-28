#pragma once
// JSON form of TrackAnalysis (library cache and the JNI boundary). Control thread only.

#include <nlohmann/json.hpp>

#include "autodj/analysis/Analysis.h"

namespace autodj::analysis {

nlohmann::json toJson(const TrackAnalysis& a);
/// Tolerant reader: missing keys keep their defaults; returns false if `j` is not an analysis object or its version is newer.
bool fromJson(const nlohmann::json& j, TrackAnalysis& out);

}  // namespace autodj::analysis
