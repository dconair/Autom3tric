#include "autodj/analysis/AnalysisJson.h"

namespace autodj::analysis {

using nlohmann::json;

json toJson(const TrackAnalysis& a) {
  json sections = json::array();
  for (const Section& s : a.sections) sections.push_back({{"start", s.startSec}, {"end", s.endSec}, {"label", s.label}});
  json energy = json::array();
  for (float e : a.barEnergy) energy.push_back(static_cast<int>(e * 100.f + 0.5f));  // percent keeps the file small
  return {{"version", a.version},
          {"durationSec", a.durationSec},
          {"bpm", a.bpm},
          {"beatSec", a.beatSec},
          {"gridOffsetSec", a.gridOffsetSec},
          {"tempoConfidence", a.tempoConfidence},
          {"gridConfidence", a.gridConfidence},
          {"downbeatConfidence", a.downbeatConfidence},
          {"keyTonic", a.keyTonic},
          {"keyMinor", a.keyMinor},
          {"keyConfidence", a.keyConfidence},
          {"keyName", a.keyName},
          {"camelot", a.camelot},
          {"rmsDb", a.rmsDb},
          {"peakDb", a.peakDb},
          {"trimDb", a.trimDb},
          {"barSec", a.barSec},
          {"firstBarSec", a.firstBarSec},
          {"barEnergy", energy},
          {"sections", sections},
          {"mixInSec", a.mixInSec},
          {"mixOutSec", a.mixOutSec}};
}

namespace {
template <class T>
void get(const json& j, const char* key, T& out) {
  auto it = j.find(key);
  if (it != j.end() && !it->is_null()) {
    try {
      out = it->get<T>();
    } catch (const json::exception&) {
    }
  }
}
}  // namespace

bool fromJson(const json& j, TrackAnalysis& out) {
  if (!j.is_object()) return false;
  TrackAnalysis a;
  get(j, "version", a.version);
  if (a.version > 1) return false;
  get(j, "durationSec", a.durationSec);
  get(j, "bpm", a.bpm);
  get(j, "beatSec", a.beatSec);
  get(j, "gridOffsetSec", a.gridOffsetSec);
  get(j, "tempoConfidence", a.tempoConfidence);
  get(j, "gridConfidence", a.gridConfidence);
  get(j, "downbeatConfidence", a.downbeatConfidence);
  get(j, "keyTonic", a.keyTonic);
  get(j, "keyMinor", a.keyMinor);
  get(j, "keyConfidence", a.keyConfidence);
  get(j, "keyName", a.keyName);
  get(j, "camelot", a.camelot);
  get(j, "rmsDb", a.rmsDb);
  get(j, "peakDb", a.peakDb);
  get(j, "trimDb", a.trimDb);
  get(j, "barSec", a.barSec);
  get(j, "firstBarSec", a.firstBarSec);
  get(j, "mixInSec", a.mixInSec);
  get(j, "mixOutSec", a.mixOutSec);
  if (auto it = j.find("barEnergy"); it != j.end() && it->is_array()) {
    for (const auto& e : *it) {
      if (e.is_number()) a.barEnergy.push_back(static_cast<float>(e.get<double>()) / 100.f);
    }
  }
  if (auto it = j.find("sections"); it != j.end() && it->is_array()) {
    for (const auto& s : *it) {
      if (!s.is_object()) continue;
      Section sec;
      get(s, "start", sec.startSec);
      get(s, "end", sec.endSec);
      get(s, "label", sec.label);
      a.sections.push_back(sec);
    }
  }
  out = std::move(a);
  return true;
}

}  // namespace autodj::analysis
