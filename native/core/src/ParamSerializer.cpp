#include "autodj/core/ParamSerializer.h"

#include <algorithm>
#include <cmath>

namespace autodj {

using nlohmann::json;

const std::vector<PresetMigration>& defaultPresetMigrations() {
  static const std::vector<PresetMigration> kNone;
  return kNone;
}

std::string presetToJson(const ParamRegistry& reg, const ParamStore& store, std::string_view name,
                         std::string_view keyPrefix) {
  json doc;
  doc["format"] = "autodj-preset";
  doc["version"] = kPresetVersion;
  doc["name"] = std::string(name);
  json params = json::object();
  for (std::size_t i = 0; i < reg.size(); ++i) {
    const auto id = static_cast<ParamId>(i);
    const ParamSpec& s = reg.spec(id);
    if (!keyPrefix.empty() && s.key.compare(0, keyPrefix.size(), keyPrefix) != 0) continue;
    params[s.key] = store.get(id);
  }
  doc["params"] = std::move(params);
  return doc.dump(2);
}

LoadReport presetFromJson(const ParamRegistry& reg, ParamStore& store, std::string_view text,
                          const std::vector<PresetMigration>& migrations) {
  LoadReport rep;
  json doc = json::parse(text.begin(), text.end(), nullptr, /*allow_exceptions=*/false);
  if (doc.is_discarded() || !doc.is_object()) {
    rep.error = "not a JSON object";
    return rep;
  }
  if (doc.value("format", std::string()) != "autodj-preset") {
    rep.error = "not an autodj preset";
    return rep;
  }
  const json& ver = doc["version"];
  if (!ver.is_number_integer()) {
    rep.error = "missing version";
    return rep;
  }
  int version = ver.get<int>();
  rep.fromVersion = version;
  if (version > kPresetVersion) {
    rep.error = "preset was written by a newer app version";
    return rep;
  }
  while (version < kPresetVersion) {
    const auto it = std::find_if(migrations.begin(), migrations.end(),
                                 [version](const PresetMigration& m) { return m.fromVersion == version; });
    if (it == migrations.end()) {
      rep.error = "no migration path from version " + std::to_string(version);
      return rep;
    }
    it->apply(doc);
    ++version;
    doc["version"] = version;
  }
  if (!doc.contains("params") || !doc["params"].is_object()) {
    rep.error = "missing params";
    return rep;
  }
  for (const auto& [key, value] : doc["params"].items()) {
    const ParamId id = reg.find(key);
    if (id == kInvalidParam) {
      rep.unknownKeys.push_back(key);
      continue;
    }
    if (value.is_boolean()) {
      store.set(id, value.get<bool>() ? 1.f : 0.f);
      ++rep.applied;
      continue;
    }
    if (!value.is_number()) continue;
    const float v = value.get<float>();
    if (!std::isfinite(v)) continue;
    const ParamSpec& s = reg.spec(id);
    if (v < s.min || v > s.max) rep.clampedKeys.push_back(key);
    store.set(id, v);
    ++rep.applied;
  }
  rep.ok = true;
  return rep;
}

}  // namespace autodj
