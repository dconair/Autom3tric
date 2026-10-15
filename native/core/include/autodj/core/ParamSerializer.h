#pragma once
// Preset (de)serialization of the parameter store. Keys are the stable registry keys, so presets
// survive reordering/insertion of parameters. A "version" field plus a chain of migration
// functions upgrades old presets on load (renamed keys, rescaled ranges, ...).

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "autodj/core/Param.h"

namespace autodj {

inline constexpr int kPresetVersion = 1;

/// Upgrades a parsed preset document from `fromVersion` to `fromVersion + 1` in place.
struct PresetMigration {
  int fromVersion;
  std::function<void(nlohmann::json& doc)> apply;
};

/// Migrations from older document versions to kPresetVersion (empty while version 1 is current).
const std::vector<PresetMigration>& defaultPresetMigrations();

struct LoadReport {
  bool ok = false;
  std::string error;
  int fromVersion = 0;
  int applied = 0;                         // parameters that were set
  std::vector<std::string> unknownKeys;    // present in the file, not in the registry (ignored)
  std::vector<std::string> clampedKeys;    // value was out of range and got clamped
};

/// Serializes all parameters whose key starts with `keyPrefix` (empty = everything).
std::string presetToJson(const ParamRegistry& reg, const ParamStore& store, std::string_view name,
                         std::string_view keyPrefix = {});

/// Applies a preset. Missing keys keep their current value; unknown keys are reported, not fatal.
LoadReport presetFromJson(const ParamRegistry& reg, ParamStore& store, std::string_view json,
                          const std::vector<PresetMigration>& migrations = defaultPresetMigrations());

}  // namespace autodj
